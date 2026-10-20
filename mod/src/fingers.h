// GoTVR 0.18.18: VR fingers - Jin's fingers detached from the game's animation, posed by the mod.
// Method (owner: "hands fully detached from animations, so we can have custom finger poses; grip makes a fist when held for now; these
// poses will be edited to wrap / grab different weapons and handles"), after the two references the owner named:
//  - bldbrne-vr (core/vr/bloodborne_body.cpp, PrepareFingers / ReadFingerTuning): every finger joint starts from the skeleton's
//    REFERENCE pose (the "neutral" locals, not the animation) and a grip is per-finger, per-joint bend angles + spread + thumb
//    opposition, tunable per grip kind, with a bend sign tunable.
//  - God of War VR (source/src/fp/arms.cpp, MIT; BuildFingers + the finger loop): finger chains are the wrist's children with a
//    chain below them; the thumb is told apart from the long fingers; each bending joint turns by
//    relaxed + (max - relaxed) * curl about the joint's side axis, maxima ~80 / 95 / 65 deg for the long fingers and 30 / 45 for the
//    thumb; the index follows the trigger, the others the grip; on Quest (no grip touch sensor) a light squeeze is already a fist
//    (grip_fist_full 0.35).
// Here: the reference pose is the skin's BIND pose, read from the inverse-bind matrices the skinning palette job uses (0x59EB30:
// node+0x2F8 -> +0x660 -> +0xA8, 3x4 per joint, 0x30 apart, in joint order). Every finger joint is placed at its bind pose relative to
// the (IK-placed) hand, every other joint under the hand (weapon sockets, helpers) keeps its animated offset from its parent, then
// each finger bends toward the palm (the controller's palm normal) by its curl. Pure math on a model-space pose: self-tested.
#pragma once
#include "fbik.h"
namespace fing {
using fbik::dot3; using fbik::cross3; using fbik::norm3; using fbik::len3f;
// column-vector affine: p_model = R p_local + t
struct Aff { float R[9]; float t[3]; };
static inline Aff fromPose(const float *m) {   // a pose joint (row-vector: rows 0..2 = the joint's axes, row 3 = position)
    Aff a; for (int r = 0; r < 3; r++) for (int c = 0; c < 3; c++) a.R[c * 3 + r] = m[4 * r + c]; a.t[0] = m[12]; a.t[1] = m[13]; a.t[2] = m[14]; return a; }
static inline void toPose(const Aff &a, float *m) {
    for (int r = 0; r < 3; r++) { for (int c = 0; c < 3; c++) m[4 * r + c] = a.R[c * 3 + r]; m[4 * r + 3] = 0.0f; }
    m[12] = a.t[0]; m[13] = a.t[1]; m[14] = a.t[2]; m[15] = 1.0f; }
static inline Aff fromRows34(const float *ib) {   // the palette's 3x4 [R | t] rows
    Aff a; for (int r = 0; r < 3; r++) { for (int c = 0; c < 3; c++) a.R[r * 3 + c] = ib[r * 4 + c]; a.t[r] = ib[r * 4 + 3]; } return a; }
static inline Aff mul(const Aff &a, const Aff &b) {   // a o b (apply b, then a)
    Aff o; for (int i = 0; i < 3; i++) { for (int j = 0; j < 3; j++) o.R[i * 3 + j] = a.R[i * 3] * b.R[j] + a.R[i * 3 + 1] * b.R[3 + j] + a.R[i * 3 + 2] * b.R[6 + j];
        o.t[i] = a.R[i * 3] * b.t[0] + a.R[i * 3 + 1] * b.t[1] + a.R[i * 3 + 2] * b.t[2] + a.t[i]; } return o; }
static inline bool inv(const Aff &a, Aff &o) {
    const float *m = a.R; const float d = m[0] * (m[4] * m[8] - m[5] * m[7]) - m[1] * (m[3] * m[8] - m[5] * m[6]) + m[2] * (m[3] * m[7] - m[4] * m[6]);
    if (!(fabsf(d) > 1e-8f) || !std::isfinite(d)) return false; const float k = 1.0f / d;
    o.R[0] = (m[4] * m[8] - m[5] * m[7]) * k; o.R[1] = (m[2] * m[7] - m[1] * m[8]) * k; o.R[2] = (m[1] * m[5] - m[2] * m[4]) * k;
    o.R[3] = (m[5] * m[6] - m[3] * m[8]) * k; o.R[4] = (m[0] * m[8] - m[2] * m[6]) * k; o.R[5] = (m[2] * m[3] - m[0] * m[5]) * k;
    o.R[6] = (m[3] * m[7] - m[4] * m[6]) * k; o.R[7] = (m[1] * m[6] - m[0] * m[7]) * k; o.R[8] = (m[0] * m[4] - m[1] * m[3]) * k;
    for (int i = 0; i < 3; i++) o.t[i] = -(o.R[i * 3] * a.t[0] + o.R[i * 3 + 1] * a.t[1] + o.R[i * 3 + 2] * a.t[2]);
    return true; }
static inline float dist(const float *a, const float *b) { const float d[3] = {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; return len3f(d); }

enum { MAXF = 6, MAXC = 5, MAXH = 96 };
struct Chain { int n = 0; int j[MAXC]; uint16_t sub[MAXC][MAXH]; int nSub[MAXC]; bool thumb = false, index = false;
    int first = 0; float hingeL[3] = {0, 0, 0}; };   // 0.18.21: the first joint that bends, and the finger's ONE hinge axis (hand-local, from the bind)
struct Hand {
    bool ok = false; int hand = -1; int nf = 0; Chain f[MAXF];
    int nq = 0; uint16_t q[MAXH]; int16_t par[MAXH]; bool isF[MAXH];   // the hand's subtree in order (q[0] = the hand), local parent slots
    Aff rel[MAXH];                                                      // finger joints: the bind pose relative to the hand
    float bendSign = 1.0f; int links = 0, bad = 0; float palmL[3] = {0, 0, 0};   // 0.18.20: the palm normal in the hand's own axes
    float sideL[3] = {0, 0, 0};   // 0.18.21: across the palm, index -> little finger (hand-local)
};
struct Curl { float grip = 0, index = 0, thumb = 0; };
// 0.18.21 a critically damped spring step toward tgt (exact for a constant target: no overshoot, eases in and out); om = stiffness (1/s)
static inline void spring(float &x, float &v, float tgt, float om, float dt) {
    if (!(dt > 0.0f)) return; const float e = x - tgt, tmp = (v + om * e) * dt, ex = expf(-om * dt);
    x = tgt + (e + tmp) * ex; v = (v - om * tmp) * ex;
    if (!std::isfinite(x) || !std::isfinite(v)) { x = tgt; v = 0; } }
// 0.18.21 fist angles per joint (knuckle / middle / end joint): God of War VR's 80 / 95 / 65; checked on Jin's own bind fingers (sim + the
// self-test): 80 / 100 / 60 puts the four finger tips on the palm. Thumb (base / middle / end): 15 / 40 / 60 about a hinge that turns it
// across the palm (thumbAcrossDeg = how far the hinge leans from "across" toward "into the palm") lays it over the curled index and middle,
// like a thumb in a fist.
struct Tune { float relaxed = 5.0f, longDeg[3] = {80.0f, 100.0f, 60.0f}, thumbDeg[3] = {15.0f, 40.0f, 60.0f}, thumbAcrossDeg = 15.0f; float sign = 1.0f; };

// Analysis (once, from a fresh pose + the bind matrices). bindOf(j, Aff&) returns joint j's bind pose (model space). Fails closed.
template <class BindOf>
static bool analyse(Hand &H, const int16_t *par, int n, const float *pose, const fbik::Arm &A, BindOf bindOf, char *why, int whyN, const Tune &tune = Tune{}) {
    H = Hand{}; if (A.hand < 0 || A.nSub[3] <= 1 || !A.frame) { snprintf(why, (size_t)whyN, "no hand frame"); return false; }
    H.hand = A.hand;
    for (int i = 0; i < A.nSub[3] && H.nq < MAXH; i++) { const int j = A.sub[3][i]; H.q[H.nq] = (uint16_t)j; H.par[H.nq] = -1; H.isF[H.nq] = false;
        for (int k = 0; k < H.nq; k++) if (H.q[k] == par[j]) { H.par[H.nq] = (int16_t)k; break; } H.nq++; }
    if (H.nq != A.nSub[3]) { snprintf(why, (size_t)whyN, "hand subtree too large (%d)", A.nSub[3]); return false; }
    Aff Bh; if (!bindOf(A.hand, Bh)) { snprintf(why, (size_t)whyN, "no bind pose for the hand"); return false; }
    Aff BhI; if (!inv(Bh, BhI)) { snprintf(why, (size_t)whyN, "hand bind not invertible"); return false; }
    // 0.18.19 chains (0.18.18 followed single-child runs from the hand and found only 3 per hand on Jin: helper branches and shared
    // palm joints break such runs). Now every LEAF of the hand subtree gives a path up to the hand; paths of >= 3 joints are kept,
    // and of two paths that end as siblings (a helper leaf next to a finger tip) the longer wins. Joints on exactly one kept path
    // are that finger's own (they bend); joints shared by several paths are the palm (placed at the bind pose, not bent).
    static int path[64][MAXC + 8]; static int plen[64]; int np = 0;
    static int nKids[MAXH]; memset(nKids, 0, sizeof nKids); for (int s2 = 1; s2 < H.nq; s2++) if (H.par[s2] >= 0) nKids[H.par[s2]]++;
    for (int s2 = 1; s2 < H.nq && np < 64; s2++) {
        if (nKids[s2] != 0) continue;   // a leaf
        { bool side = false; const int pp = H.par[s2]; for (int s3 = 1; s3 < H.nq && !side; s3++) if (s3 != s2 && H.par[s3] == pp && nKids[s3] > 0) side = true;
          if (side) continue; }   // a helper branching off a joint that goes on (a finger continues past it)
        int tmp[MAXH], nt = 0; for (int c = s2; c > 0 && nt < MAXH; c = H.par[c]) tmp[nt++] = c;
        if (nt < 3 || nt > MAXC + 8) continue;
        plen[np] = nt; for (int k = 0; k < nt; k++) path[np][k] = tmp[nt - 1 - k]; np++;
    }
    auto blen = [&](int a, int b) { Aff Ba, Bb; if (!bindOf(H.q[a], Ba) || !bindOf(H.q[b], Bb)) return -1.0f; return dist(Ba.t, Bb.t); };
    auto plength = [&](int p) { float L = 0; for (int k = 0; k + 1 < plen[p]; k++) { const float l = blen(path[p][k], path[p][k + 1]); if (l < 0) return -1.0f; L += l; } return L; };
    static bool keep[64]; for (int p = 0; p < np; p++) keep[p] = plength(p) > 2.0f;
    for (int a = 0; a < np; a++) for (int b = 0; b < np; b++) if (a != b && keep[a] && keep[b] && plen[a] >= 2 && plen[b] >= 2 && H.par[path[a][plen[a] - 1]] == H.par[path[b][plen[b] - 1]]) {
        if (plength(a) < plength(b) || (plength(a) == plength(b) && a > b)) keep[a] = false; }   // sibling leaves: a helper next to a tip
    static int use[MAXH]; memset(use, 0, sizeof use); for (int p = 0; p < np; p++) if (keep[p]) for (int k = 0; k < plen[p]; k++) use[path[p][k]]++;
    int bad = 0, links = 0;
    for (int p = 0; p < np && H.nf < MAXF; p++) {
        if (!keep[p]) continue; Chain C; int first = -1;
        for (int k = 0; k < plen[p]; k++) if (use[path[p][k]] == 1) { first = k; break; }
        if (first < 0) continue;
        for (int k = first; k < plen[p] && C.n < MAXC; k++) C.j[C.n++] = H.q[path[p][k]];
        if (C.n < 2) continue;
        for (int k = 0; k + 1 < C.n; k++) { Aff Ba, Bb; if (!bindOf(C.j[k], Ba) || !bindOf(C.j[k + 1], Bb)) continue; links++;
            const float lb = dist(Ba.t, Bb.t), la = dist(pose + (size_t)C.j[k] * 16 + 12, pose + (size_t)C.j[k + 1] * 16 + 12);
            if (fabsf(lb - la) > 0.15f * lb + 0.2f) bad++; }
        H.f[H.nf++] = C;
        for (int k = 0; k < plen[p]; k++) H.isF[path[p][k]] = true;   // palm joints of this path too: bind pose
    }
    H.links = links; H.bad = bad;
    if (H.nf < 4 || (links > 0 && bad * 3 > links)) { snprintf(why, (size_t)whyN, "%d finger chains from %d leaf paths, %d of %d finger links do not match the animated lengths", H.nf, np, bad, links); return false; }
    // 0.18.20 (log 0.18.19: the thumb was taken for the ring finger - the arm analysis' "thumb direction" is the hand child farthest from
    // the others, on Jin a socket): the thumb is the SHORTEST chain (Jin: 3 joints / 7 cm, the long fingers 4 joints / 12.6 cm with
    // the metacarpal; God of War VR: the thumb is the chain with fewer joints)
    int th = -1; float best = 1e9f;
    for (int c = 0; c < H.nf; c++) { float L = 0; for (int k = 0; k + 1 < H.f[c].n; k++) { Aff a1, b1; if (bindOf(H.f[c].j[k], a1) && bindOf(H.f[c].j[k + 1], b1)) L += dist(a1.t, b1.t); }
        const float sc = L + 100.0f * (float)H.f[c].n; if (sc < best) { best = sc; th = c; } }
    if (th >= 0) H.f[th].thumb = true;
    if (th >= 0) {   // the index: the long finger whose base is nearest the thumb's
        Aff Bt; bindOf(H.f[th].j[0], Bt); int ix = -1; float bd = 1e9f;
        for (int c = 0; c < H.nf; c++) { if (c == th) continue; Aff Bb; bindOf(H.f[c].j[0], Bb); const float d = dist(Bb.t, Bt.t); if (d < bd) { bd = d; ix = c; } }
        if (ix >= 0) H.f[ix].index = true;
    }
    for (int c = 0; c < H.nf; c++) {
        Chain &C = H.f[c];
        for (int k = 0; k < C.n; k++) {   // the joint's own subtree inside the hand (for the bends)
            int slot = -1; for (int s = 0; s < H.nq; s++) if (H.q[s] == C.j[k]) { slot = s; break; }
            C.nSub[k] = 0; if (slot < 0) continue;
            static bool in[MAXH]; memset(in, 0, sizeof in); in[slot] = true; C.sub[k][C.nSub[k]++] = (uint16_t)C.j[k];
            for (int s = slot + 1; s < H.nq; s++) if (H.par[s] >= 0 && in[H.par[s]]) { in[s] = true; if (C.nSub[k] < MAXH) C.sub[k][C.nSub[k]++] = H.q[s]; }
        }
    }
    for (int s2 = 1; s2 < H.nq; s2++) if (H.isF[s2]) { Aff Bj; if (bindOf(H.q[s2], Bj)) H.rel[s2] = mul(BhI, Bj); else H.isF[s2] = false; }
    {   // 0.18.20 the palm normal of the hand itself (0.18.19 bent toward the CONTROLLER's palm - sideways when the two disagree): finger
        // direction = hand -> mean long-finger tip, side = index base -> farthest long-finger base, normal = finger x side, signed so the
        // thumb tip is on the palm side (Jin, both hands: hand-local +-y, the side the animation curls the fingers to)
        float fd[3] = {0, 0, 0}, sd[3] = {0, 0, 0}, tt[3] = {0, 0, 0}; int nfd = 0; Aff Bix; int ixc = -1; for (int c = 0; c < H.nf; c++) if (H.f[c].index) ixc = c;
        for (int c = 0; c < H.nf; c++) { if (H.f[c].thumb) { Aff Bt; if (bindOf(H.f[c].j[H.f[c].n - 1], Bt)) { const Aff L = mul(BhI, Bt); tt[0] = L.t[0]; tt[1] = L.t[1]; tt[2] = L.t[2]; } continue; }
            Aff Bt; if (bindOf(H.f[c].j[H.f[c].n - 1], Bt)) { const Aff L = mul(BhI, Bt); fd[0] += L.t[0]; fd[1] += L.t[1]; fd[2] += L.t[2]; nfd++; } }
        if (ixc >= 0 && bindOf(H.f[ixc].j[0], Bix)) { const Aff Li = mul(BhI, Bix); float farD = -1;
            for (int c = 0; c < H.nf; c++) { if (H.f[c].thumb || c == ixc) continue; Aff Bb; if (!bindOf(H.f[c].j[0], Bb)) continue; const Aff Lb = mul(BhI, Bb); const float d = dist(Lb.t, Li.t);
                if (d > farD) { farD = d; sd[0] = Lb.t[0] - Li.t[0]; sd[1] = Lb.t[1] - Li.t[1]; sd[2] = Lb.t[2] - Li.t[2]; } } }
        float nrm[3]; cross3(fd, sd, nrm);
        if (nfd && norm3(nrm)) { if (dot3(nrm, tt) < 0) { nrm[0] = -nrm[0]; nrm[1] = -nrm[1]; nrm[2] = -nrm[2]; } memcpy(H.palmL, nrm, 12); }
        if (!norm3(sd)) { sd[0] = 0; sd[1] = 0; sd[2] = 0; }
        memcpy(H.sideL, sd, 12);
    }
    if (len3f(H.palmL) < 0.5f || len3f(H.sideL) < 0.5f) { snprintf(why, (size_t)whyN, "no palm / side direction from the bind fingers"); return false; }
    // 0.18.21 (owner, 0.18.20 photo: "grip is totally wrong" - fingers bent sideways): 0.18.20 took each joint's bend axis from the bone
    // AFTER the joints before it had bent; once the knuckle has turned ~80 deg the next bone points along the palm normal, bone x palm is
    // ~0 and its direction is noise, so the middle joints bent sideways. A finger is a hinge: ONE axis per finger, from the straight bind
    // finger (knuckle -> tip) x the palm normal, used for every joint of that finger (a turn about it leaves it where it is).
    for (int c = 0; c < H.nf; c++) {
        Chain &C = H.f[c]; Aff B0, B1, Bn; float dl[3];
        if (!bindOf(C.j[0], B0) || !bindOf(C.j[1], B1) || !bindOf(C.j[C.n - 1], Bn)) { snprintf(why, (size_t)whyN, "no bind pose for a finger joint"); return false; }
        C.first = (!C.thumb && C.n >= 3 && dist(B0.t, B1.t) > 5.5f) ? 1 : 0;   // a first link longer than 5.5 cm is a metacarpal: it does not bend
        Aff Bf; bindOf(C.j[C.first], Bf); const Aff Lf = mul(BhI, Bf), Ln = mul(BhI, Bn);
        for (int k = 0; k < 3; k++) dl[k] = Ln.t[k] - Lf.t[k];
        if (!norm3(dl)) { snprintf(why, (size_t)whyN, "finger %d has no length", c); return false; }
        float cdir[3];
        if (C.thumb) { const float a = tune.thumbAcrossDeg / 57.2957795f; for (int k = 0; k < 3; k++) cdir[k] = cosf(a) * H.sideL[k] + sinf(a) * H.palmL[k]; }
        else memcpy(cdir, H.palmL, 12);
        cross3(dl, cdir, C.hingeL); if (!norm3(C.hingeL)) { snprintf(why, (size_t)whyN, "finger %d lies along its bend direction", c); return false; }
    }
    H.ok = true;
    snprintf(why, (size_t)whyN, "%d finger chains (thumb = chain %d, joints %d..%d), %d hand joints, %d of %d links off the animated length", H.nf, th, H.f[th < 0 ? 0 : th].j[0], H.f[th < 0 ? 0 : th].j[H.f[th < 0 ? 0 : th].n - 1], H.nq, H.bad, H.links);
    return true;
}
// Pose the fingers of an IK-placed hand. palm: model-space palm normal (out of the palm). w: 0..1 blend with the animated fingers.
static void apply(float *pose, const Hand &H, const Curl &cu, const float *palmIn, float w, const Tune &tn) {
    if (!H.ok || w <= 0.0f) return; if (w > 1) w = 1;
    static float old[MAXH][16]; for (int s = 0; s < H.nq; s++) memcpy(old[s], pose + (size_t)H.q[s] * 16, 64);
    const Aff Mh = fromPose(old[0]);
    // 1. the reference (bind) fingers on the hand; everything else keeps its animated offset from its (new) parent
    static Aff nw[MAXH]; nw[0] = Mh;
    for (int s = 1; s < H.nq; s++) {
        if (H.isF[s]) nw[s] = mul(Mh, H.rel[s]);
        else { const int p = H.par[s] < 0 ? 0 : H.par[s]; Aff Po, PoI; Po = fromPose(old[p]); if (!inv(Po, PoI)) { nw[s] = fromPose(old[s]); continue; } nw[s] = mul(nw[p], mul(PoI, fromPose(old[s]))); }
        toPose(nw[s], pose + (size_t)H.q[s] * 16);
    }
    // 2. the bends (0.18.21): each finger turns about its own hinge (hand-local, from the bind; see analyse), joint by joint from the first
    // bending joint to the chain's last joint INCLUSIVE (the last joint is the end knuckle: turning it curls the finger's end), each
    // joint's subtree moving with it. palmIn is no longer used (the hand's own palm decides).
    (void)palmIn;
    for (int c = 0; c < H.nf; c++) {
        const fing::Chain &C = H.f[c];
        float ax[3]; for (int k = 0; k < 3; k++) ax[k] = Mh.R[k * 3] * C.hingeL[0] + Mh.R[k * 3 + 1] * C.hingeL[1] + Mh.R[k * 3 + 2] * C.hingeL[2];
        if (!norm3(ax)) continue;
        float amt = C.thumb ? cu.thumb : (C.index ? cu.index : cu.grip); amt = amt < 0 ? 0 : amt > 1 ? 1 : amt;
        const float *tab = C.thumb ? tn.thumbDeg : tn.longDeg;
        for (int k = C.first, bi = 0; k < C.n && bi < 3; k++, bi++) {
            const float deg = (tn.relaxed + (tab[bi] - tn.relaxed) * amt) * tn.sign * H.bendSign;
            const float *a = pose + (size_t)C.j[k] * 16 + 12; const float pv[3] = {a[0], a[1], a[2]};
            fbik::moveSet(pose, C.sub[k], C.nSub[k], fbik::axisAngle(ax, deg / 57.2957795f), pv);
        }
    }
    // 3. blend with the animated fingers (in / out with the arm)
    if (w < 0.999f) for (int s = 1; s < H.nq; s++) { float *m = pose + (size_t)H.q[s] * 16; for (int i = 0; i < 16; i++) m[i] = old[s][i] + (m[i] - old[s][i]) * w; }
}
} // namespace fing
