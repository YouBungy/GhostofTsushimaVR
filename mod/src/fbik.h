// GoTVR full-body IK, stage 1 (0.18.7): VR arms on Jin's skeleton.
// Method (playbook ch12 "Torso calculations and ergonomics" + bldbrne-vr, whose public body code and tests pin these choices and
// numbers, and its credits): analytic two-bone IK per arm (shoulder -> elbow -> wrist) with the elbow pole in BODY space, out to
// the side, down and back (BetterVR: (+-1, -1, -0.5) turned with the body); the clavicle shrugs only toward farD targets (FRIK:
// reach fraction 0.15, at most 20 deg, downward x0.4); the hand takes the controller GRIP pose (OpenXR: -Z = little finger -> thumb
// through the fist, -Y = wrist -> knuckles), trimmed by ini angles; fingers, twist bones and anything parented below a moved joint
// move rigidly with it; the result is blended in/out (1 - cos) when tracking starts/stops.
// Everything here is pure math on a MODEL-SPACE pose (row-vector joint matrices: rows 0..2 = the joint's axes, row 3 = position,
// 16 floats per joint) plus a parent table, so it is unit-tested in the self-test with a synthetic skeleton.
#pragma once
#include <math.h>
#include <string.h>
#include <stdint.h>
namespace fbik {
enum { MAXJ = 4096, MAXSUB = 512 };
static inline float dot3(const float *a, const float *b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
static inline void cross3(const float *a, const float *b, float *o) { o[0] = a[1] * b[2] - a[2] * b[1]; o[1] = a[2] * b[0] - a[0] * b[2]; o[2] = a[0] * b[1] - a[1] * b[0]; }
static inline float len3f(const float *a) { return sqrtf(dot3(a, a)); }
static inline bool norm3(float *a) { const float l = len3f(a); if (!(l > 1e-6f)) return false; a[0] /= l; a[1] /= l; a[2] /= l; return true; }
static inline const float *P(const float *pose, int j) { return pose + (size_t)j * 16 + 12; }
// column-form rotation (v' = R v), stored row-major R[3][3]
struct Rot { float m[9]; };
static inline void rotv(const Rot &R, const float *v, float *o) { const float x = v[0], y = v[1], z = v[2]; o[0] = R.m[0] * x + R.m[1] * y + R.m[2] * z; o[1] = R.m[3] * x + R.m[4] * y + R.m[5] * z; o[2] = R.m[6] * x + R.m[7] * y + R.m[8] * z; }
static inline Rot axisAngle(const float *k, float a) {   // k unit
    const float c = cosf(a), s = sinf(a), t = 1 - c; Rot R;
    R.m[0] = c + k[0] * k[0] * t; R.m[1] = k[0] * k[1] * t - k[2] * s; R.m[2] = k[0] * k[2] * t + k[1] * s;
    R.m[3] = k[1] * k[0] * t + k[2] * s; R.m[4] = c + k[1] * k[1] * t; R.m[5] = k[1] * k[2] * t - k[0] * s;
    R.m[6] = k[2] * k[0] * t - k[1] * s; R.m[7] = k[2] * k[1] * t + k[0] * s; R.m[8] = c + k[2] * k[2] * t;
    return R;
}
static inline Rot arc(const float *from, const float *to) {   // the smallest rotation taking direction from -> to
    float a[3] = {from[0], from[1], from[2]}, b[3] = {to[0], to[1], to[2]}; Rot I{{1, 0, 0, 0, 1, 0, 0, 0, 1}};
    if (!norm3(a) || !norm3(b)) return I;
    float k[3]; cross3(a, b, k); const float s = len3f(k), c = dot3(a, b);
    if (s < 1e-6f) { if (c > 0) return I; float p[3] = {1, 0, 0}; if (fabsf(a[0]) > 0.9f) { p[0] = 0; p[1] = 1; } cross3(a, p, k); norm3(k); return axisAngle(k, 3.14159265f); }
    k[0] /= s; k[1] /= s; k[2] /= s; return axisAngle(k, atan2f(s, c));
}
static inline Rot frameTo(const float *f0, const float *t0, const float *f1, const float *t1) {   // rotation taking frame (f0, t0) onto (f1, t1)
    float a0[3] = {f0[0], f0[1], f0[2]}, b0[3], c0[3], a1[3] = {f1[0], f1[1], f1[2]}, b1[3], c1[3]; Rot I{{1, 0, 0, 0, 1, 0, 0, 0, 1}};
    if (!norm3(a0) || !norm3(a1)) return I;
    const float d0 = dot3(t0, a0), d1 = dot3(t1, a1);
    for (int k = 0; k < 3; k++) { b0[k] = t0[k] - d0 * a0[k]; b1[k] = t1[k] - d1 * a1[k]; }
    if (!norm3(b0) || !norm3(b1)) return arc(a0, a1);
    cross3(a0, b0, c0); cross3(a1, b1, c1);
    Rot R; for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) R.m[i * 3 + j] = a1[i] * a0[j] + b1[i] * b0[j] + c1[i] * c0[j];   // B * A^T
    return R;
}
static inline Rot scaled(const Rot &R, float w) {   // the same rotation with its angle scaled by w (0..1)
    const float tr = R.m[0] + R.m[4] + R.m[8]; float c = (tr - 1) * 0.5f; c = c > 1 ? 1 : c < -1 ? -1 : c; const float a = acosf(c);
    Rot I{{1, 0, 0, 0, 1, 0, 0, 0, 1}}; if (a < 1e-5f) return I;
    float k[3] = {R.m[7] - R.m[5], R.m[2] - R.m[6], R.m[3] - R.m[1]};
    if (!norm3(k)) {   // ~180 deg: axis from the diagonal
        k[0] = sqrtf(fmaxf(0.0f, (R.m[0] + 1) * 0.5f)); k[1] = sqrtf(fmaxf(0.0f, (R.m[4] + 1) * 0.5f)); k[2] = sqrtf(fmaxf(0.0f, (R.m[8] + 1) * 0.5f));
        if (R.m[1] < 0) k[1] = -k[1]; if (R.m[2] < 0) k[2] = -k[2]; if (!norm3(k)) return I; }
    return axisAngle(k, a * w);
}
// rigid motion of a set of joints: rotation R about the pivot p (rows and positions)
static inline void moveSet(float *pose, const uint16_t *set, int n, const Rot &R, const float *p) {
    for (int i = 0; i < n; i++) { float *m = pose + (size_t)set[i] * 16; float o[3];
        for (int r = 0; r < 3; r++) { rotv(R, m + 4 * r, o); m[4 * r] = o[0]; m[4 * r + 1] = o[1]; m[4 * r + 2] = o[2]; }
        const float d[3] = {m[12] - p[0], m[13] - p[1], m[14] - p[2]}; rotv(R, d, o); m[12] = p[0] + o[0]; m[13] = p[1] + o[1]; m[14] = p[2] + o[2]; }
}
// ---------- skeleton analysis ----------
struct Arm {
    int clav = -1, sh = -1, el = -1, hand = -1;
    uint16_t sub[4][MAXSUB]; int nSub[4] = {0, 0, 0, 0};   // subtrees of clav / shoulder / elbow / hand (each includes the joint)
    float fl[3] = {0, 0, 0}, tl[3] = {0, 0, 0}; bool frame = false;   // hand-LOCAL finger and thumb directions (from the pose at analysis)
    float upper = 0, lower = 0;
    int nTw = 0; uint16_t tw[8]; float twT[8];   // 0.18.23: forearm twist joints (on the elbow -> wrist line) and where along it (0..1)
};
static bool parentsOk(const int16_t *par, int n) {   // a parent table: -1 or an earlier index, for every joint; exactly one root among the first few
    if (n < 8) return false; int roots = 0;
    for (int i = 0; i < n; i++) { const int p = par[i]; if (p == -1) { roots++; continue; } if (p < 0 || p >= i) return false; }
    return roots >= 1 && roots <= 4;
}
static int subtree(const int16_t *par, int n, int j, uint16_t *out, int cap) {   // j and all its descendants (parents precede children)
    static uint8_t in[MAXJ]; if (n > MAXJ) return 0; memset(in, 0, (size_t)n); in[j] = 1; int k = 0; if (k < cap) out[k++] = (uint16_t)j;
    for (int i = j + 1; i < n; i++) { const int p = par[i]; if (p >= 0 && in[p]) { in[i] = 1; if (k < cap) out[k++] = (uint16_t)i; else return -1; } }
    return k;
}
static inline bool shownJ(const float *pose, int j) { const float *m = pose + (size_t)j * 16; return len3f(m) > 0.01f && len3f(m + 4) > 0.01f && len3f(m + 8) > 0.01f; }
// find both arm chains in a model-space pose (x forward, y LEFT, z up; cm). headZ: height of the head top. skip[j] = 1: not a candidate (head set)
// 0.18.8: chain-only search (0.18.7 required a compact hand subtree and found nothing on Jin - items parented under the hand reach far).
// Every shown joint j gets its chain: e = the first ancestor >= 15 cm away (the elbow, past forearm twist joints), s = the first
// ancestor of e >= 15 cm away (the shoulder); kept when the forearm is 15..45 cm, the upper arm 15..50 cm, the shoulder high
// (> 0.6 head height) and off the body axis (|y| > 8 cm). The fingers, the hand and the twist joints of one arm all share the same
// (e, s); per side the pair with the most members wins, and the hand is the member nearest the elbow that has other members below
// it (the wrist above the fingers).
static inline int upFrom(const int16_t *par, const float *pose, int j, float minD) {
    int e = par[j]; while (e >= 0) { const float d[3] = {P(pose, e)[0] - P(pose, j)[0], P(pose, e)[1] - P(pose, j)[1], P(pose, e)[2] - P(pose, j)[2]}; if (len3f(d) >= minD) break; e = par[e]; }
    return e;
}
static inline bool isAncestor(const int16_t *par, int a, int j) { for (int q = par[j]; q >= 0; q = par[q]) if (q == a) return true; return false; }
static int findArms(const int16_t *par, int n, const float *pose, float headZ, const uint8_t *skip, Arm arms[2], char *why, int whyN) {
    static int ce[MAXJ], cs[MAXJ]; int nCand = 0;
    for (int j = 1; j < n && j < MAXJ; j++) {
        ce[j] = cs[j] = -1;
        if (skip && skip[j]) continue; if (!shownJ(pose, j)) continue;
        const int e = upFrom(par, pose, j, 15.0f); if (e < 0) continue; const int s = upFrom(par, pose, e, 15.0f); if (s < 0 || par[s] < 0) continue;
        const float de[3] = {P(pose, e)[0] - P(pose, j)[0], P(pose, e)[1] - P(pose, j)[1], P(pose, e)[2] - P(pose, j)[2]}, ds[3] = {P(pose, s)[0] - P(pose, e)[0], P(pose, s)[1] - P(pose, e)[1], P(pose, s)[2] - P(pose, e)[2]};
        const float lo = len3f(de), up = len3f(ds);
        if (lo > 45.0f || up < 15.0f || up > 50.0f) continue;
        if (P(pose, s)[2] < 0.6f * headZ || fabsf(P(pose, s)[1]) < 8.0f) continue;
        ce[j] = e; cs[j] = s; nCand++;
    }
    int bestE[2] = {-1, -1}, bestS[2] = {-1, -1}, bestN[2] = {0, 0};
    for (int j = 1; j < n && j < MAXJ; j++) {
        if (ce[j] < 0) continue; int cnt = 0; for (int q = 1; q < n && q < MAXJ; q++) if (ce[q] == ce[j] && cs[q] == cs[j]) cnt++;
        const int side = P(pose, cs[j])[1] > 0 ? 0 : 1;
        if (cnt > bestN[side]) { bestN[side] = cnt; bestE[side] = ce[j]; bestS[side] = cs[j]; }
    }
    static uint16_t tmp[MAXSUB]; int found = 0;
    for (int side = 0; side < 2; side++) {
        Arm &A = arms[side]; A = Arm{}; if (bestE[side] < 0) continue;
        const int e = bestE[side], s = bestS[side]; int hand = -1; float hd = 1e9f, hdAny = 1e9f; int handAny = -1;
        for (int j = 1; j < n && j < MAXJ; j++) {
            if (ce[j] != e || cs[j] != s) continue;
            const float d[3] = {P(pose, j)[0] - P(pose, e)[0], P(pose, j)[1] - P(pose, e)[1], P(pose, j)[2] - P(pose, e)[2]}; const float dl = len3f(d);
            if (dl < hdAny) { hdAny = dl; handAny = j; }
            int below = 0; for (int q = 1; q < n && q < MAXJ && below < 3; q++) if (ce[q] == e && cs[q] == s && q != j && isAncestor(par, j, q)) below++;
            if (below >= 3 && dl < hd) { hd = dl; hand = j; }
        }
        if (hand < 0) hand = handAny; if (hand < 0) continue;
        A.hand = hand; A.el = e; A.sh = s; A.clav = par[s];
        const int ids[4] = {A.clav, A.sh, A.el, A.hand}; bool ok = true;
        for (int q = 0; q < 4; q++) { A.nSub[q] = subtree(par, n, ids[q], A.sub[q], MAXSUB); if (A.nSub[q] <= 0) ok = false; }
        if (!ok) { A.hand = -1; continue; }
        { const float a[3] = {P(pose, e)[0] - P(pose, s)[0], P(pose, e)[1] - P(pose, s)[1], P(pose, e)[2] - P(pose, s)[2]}, b[3] = {P(pose, hand)[0] - P(pose, e)[0], P(pose, hand)[1] - P(pose, e)[1], P(pose, hand)[2] - P(pose, e)[2]}; A.upper = len3f(a); A.lower = len3f(b); }
        // hand frame: finger direction = the mean of the hand's shown children within 15 cm (finger roots), thumb = the child farthest from that mean
        float cs3[3] = {0, 0, 0}; int nc = 0, kids[24];
        for (int i = hand + 1; i < n && nc < 24; i++) { if (par[i] != hand || !shownJ(pose, i)) continue; const float d[3] = {P(pose, i)[0] - P(pose, hand)[0], P(pose, i)[1] - P(pose, hand)[1], P(pose, i)[2] - P(pose, hand)[2]};
            const float dl = len3f(d); if (dl < 1.0f || dl > 15.0f) continue; kids[nc++] = i; cs3[0] += P(pose, i)[0]; cs3[1] += P(pose, i)[1]; cs3[2] += P(pose, i)[2]; }
        if (nc >= 3) {
            float mean[3] = {cs3[0] / nc, cs3[1] / nc, cs3[2] / nc}; int th = -1; float farD = -1;
            for (int q = 0; q < nc; q++) { const float d[3] = {P(pose, kids[q])[0] - mean[0], P(pose, kids[q])[1] - mean[1], P(pose, kids[q])[2] - mean[2]}; if (len3f(d) > farD) { farD = len3f(d); th = kids[q]; } }
            float m2[3] = {0, 0, 0}; int n2 = 0; for (int q = 0; q < nc; q++) if (kids[q] != th) { m2[0] += P(pose, kids[q])[0]; m2[1] += P(pose, kids[q])[1]; m2[2] += P(pose, kids[q])[2]; n2++; }
            float f[3] = {m2[0] / n2 - P(pose, hand)[0], m2[1] / n2 - P(pose, hand)[1], m2[2] / n2 - P(pose, hand)[2]}, t[3] = {P(pose, th)[0] - P(pose, hand)[0], P(pose, th)[1] - P(pose, hand)[1], P(pose, th)[2] - P(pose, hand)[2]};
            const float *M = pose + (size_t)hand * 16; float r0[3] = {M[0], M[1], M[2]}, r1[3] = {M[4], M[5], M[6]}, r2[3] = {M[8], M[9], M[10]};
            if (norm3(r0) && norm3(r1) && norm3(r2) && norm3(f) && norm3(t)) {
                A.fl[0] = dot3(f, r0); A.fl[1] = dot3(f, r1); A.fl[2] = dot3(f, r2); A.tl[0] = dot3(t, r0); A.tl[1] = dot3(t, r1); A.tl[2] = dot3(t, r2);
                A.frame = fabsf(dot3(f, t)) < 0.95f;
            }
        }
        {   // 0.18.23 forearm twist joints: in the elbow's subtree, not the hand's, within 3 cm of the elbow -> wrist line, 15..105 % along
            // it (Jin: 508 / 509 / 510 at 25 / 50 / 75 %, 511 / 512 at the wrist)
            A.nTw = 0; const float *Ep = P(pose, A.el), *Wp = P(pose, A.hand); const float ew[3] = {Wp[0] - Ep[0], Wp[1] - Ep[1], Wp[2] - Ep[2]}; const float L2 = dot3(ew, ew);
            for (int q = 0; q < A.nSub[2] && A.nTw < 8 && L2 > 1.0f; q++) {
                const int j = A.sub[2][q]; if (j == A.el || j == A.hand) continue; bool inHand = false; for (int r = 0; r < A.nSub[3]; r++) if (A.sub[3][r] == j) { inHand = true; break; } if (inHand) continue;
                if (!shownJ(pose, j)) continue; const float d[3] = {P(pose, j)[0] - Ep[0], P(pose, j)[1] - Ep[1], P(pose, j)[2] - Ep[2]}; const float t = dot3(d, ew) / L2;
                const float off[3] = {d[0] - ew[0] * t, d[1] - ew[1] * t, d[2] - ew[2] * t};
                if (t >= 0.15f && t <= 1.05f && len3f(off) < 3.0f) { A.tw[A.nTw] = (uint16_t)j; A.twT[A.nTw] = t > 1.0f ? 1.0f : t; A.nTw++; }
            }
        }
        (void)tmp; found++;
    }
    if (why) snprintf(why, (size_t)whyN, "%d chain candidates | left hand %d (elbow %d shoulder %d clavicle %d, group %d, %d joints below the hand, upper %.0f / lower %.0f cm, hand frame %d) | right hand %d (elbow %d shoulder %d clavicle %d, group %d, %d below, upper %.0f / lower %.0f cm, frame %d)",
                      nCand, arms[0].hand, arms[0].el, arms[0].sh, arms[0].clav, bestN[0], arms[0].nSub[3], arms[0].upper, arms[0].lower, (int)arms[0].frame,
                      arms[1].hand, arms[1].el, arms[1].sh, arms[1].clav, bestN[1], arms[1].nSub[3], arms[1].upper, arms[1].lower, (int)arms[1].frame);
    return found;
}
// ---------- the solve ----------
struct Target { float pos[3]; float fing[3], thumb[3]; bool orient; };   // model space: the WRIST position and the hand's finger / thumb directions
struct Params { float reachFrac = 0.15f, clavMaxDeg = 20.0f, clavDown = 0.4f, poleOut = 1.0f, poleDown = 1.0f, poleBack = 0.5f, bodyYaw = 0.0f, twist = 1.0f, stretch = 0.0f; };
static float lastTwistDeg[2] = {0, 0}, lastStretchCm[2] = {0, 0};   // 0.18.23 diagnostics: the forearm twist of the last solve per side
// signed angle from r to p about the unit axis a, both projected onto the plane across a; false when either lies along a
static inline bool twistAngle(const float *r, const float *p, const float *a, float &out) {
    float r2[3], p2[3]; const float dr = dot3(r, a), dp = dot3(p, a); for (int k = 0; k < 3; k++) { r2[k] = r[k] - dr * a[k]; p2[k] = p[k] - dp * a[k]; }
    if (!norm3(r2) || !norm3(p2)) return false; float c[3]; cross3(r2, p2, c); out = atan2f(dot3(c, a), dot3(r2, p2)); return true; }
static void solveArm(float *pose, const Arm &A, int side, const Target &T, float w, const Params &prm) {
    if (A.hand < 0 || w <= 0.0f) return; if (w > 1) w = 1;
    float S[3], E[3], W[3];
    auto get = [&]() { memcpy(S, P(pose, A.sh), 12); memcpy(E, P(pose, A.el), 12); memcpy(W, P(pose, A.hand), 12); };
    get();
    // 0.18.23 forearm twist, before: the animated hand's roll about the forearm, measured from the forearm's own frame
    float tw0 = 0; bool tw0ok = false; int hk = -1, ek = -1;
    if (A.nTw > 0 && prm.twist > 0.0f && A.el >= 0) {
        float a0[3] = {W[0] - E[0], W[1] - E[1], W[2] - E[2]};
        if (norm3(a0)) { const float *Mh = pose + (size_t)A.hand * 16, *Me = pose + (size_t)A.el * 16; float bh = 2, be = 2;
            for (int k = 0; k < 3; k++) { float v[3] = {Mh[4 * k], Mh[4 * k + 1], Mh[4 * k + 2]}, u[3] = {Me[4 * k], Me[4 * k + 1], Me[4 * k + 2]};
                if (norm3(v) && fabsf(dot3(v, a0)) < bh) { bh = fabsf(dot3(v, a0)); hk = k; } if (norm3(u) && fabsf(dot3(u, a0)) < be) { be = fabsf(dot3(u, a0)); ek = k; } }
            if (hk >= 0 && ek >= 0) tw0ok = twistAngle(Me + 4 * ek, Mh + 4 * hk, a0, tw0); }
    }
    float tgt[3]; for (int k = 0; k < 3; k++) tgt[k] = W[k] + (T.pos[k] - W[k]) * w;   // blend in/out (w eased by the caller)
    const float a = A.upper, b = A.lower, reach = a + b;
    {   // clavicle: shrug toward farD targets only (FRIK reach fraction 0.15, <= 20 deg, downward x0.4)
        const float *C = P(pose, A.clav); const float d0[3] = {tgt[0] - S[0], tgt[1] - S[1], tgt[2] - S[2]}; const float dd = len3f(d0);
        const float start = reach * (1.0f - prm.reachFrac);
        if (dd > start && A.nSub[0] > 0) {
            float ex = (dd - start) / fmaxf(1e-3f, reach * prm.reachFrac); ex = ex > 1 ? 1 : ex; float ang = ex * prm.clavMaxDeg / 57.2957795f; if (tgt[2] < S[2]) ang *= prm.clavDown;
            const float cs[3] = {S[0] - C[0], S[1] - C[1], S[2] - C[2]}, ct[3] = {tgt[0] - C[0], tgt[1] - C[1], tgt[2] - C[2]}; float k[3]; cross3(cs, ct, k);
            if (norm3(k)) { const float full = acosf(fmaxf(-1.0f, fminf(1.0f, dot3(cs, ct) / fmaxf(1e-6f, len3f(cs) * len3f(ct))))); if (ang > full) ang = full;
                const float Cp[3] = {C[0], C[1], C[2]}; moveSet(pose, A.sub[0], A.nSub[0], axisAngle(k, ang), Cp); get(); }
        }
    }
    float D[3] = {tgt[0] - S[0], tgt[1] - S[1], tgt[2] - S[2]}; float d = len3f(D); if (!(d > 1e-3f)) return; for (auto &x : D) x /= d;
    // 0.18.26 (owner: "let Jin's arms stretch a bit beyond how far they normally go, for players with longer arms; around the elbow /
    // bicep; the amount editable"): a target beyond reach lengthens the UPPER arm by up to prm.stretch x the arm's length (the elbow
    // moves out along the upper arm, the bicep's own helper joints spread with it), so the hand still arrives
    float extra = 0.0f; if (prm.stretch > 0.0f && d > reach * 0.999f) { extra = d - reach * 0.999f; const float mx = prm.stretch * reach; extra = extra > mx ? mx : extra; }
    const float a2 = a + extra, reach2 = a2 + b;
    lastStretchCm[side & 1] = extra;
    const float dmin = fabsf(a2 - b) * 1.01f + 0.5f, dmax = reach2 * 0.999f; d = d < dmin ? dmin : d > dmax ? dmax : d;   // out of reach: the hand stops short
    // elbow pole in BODY space (out, down, back) turned with the drawn body (BetterVR); model y = Jin's left
    const float c = cosf(prm.bodyYaw), s = sinf(prm.bodyYaw), ox = -prm.poleBack, oy = (side == 0 ? 1.0f : -1.0f) * prm.poleOut;
    float pole[3] = {c * ox - s * oy, s * ox + c * oy, -prm.poleDown};
    float pd = dot3(pole, D); for (int k = 0; k < 3; k++) pole[k] -= pd * D[k];
    if (!norm3(pole)) { pole[0] = 0; pole[1] = 0; pole[2] = -1; pd = dot3(pole, D); for (int k = 0; k < 3; k++) pole[k] -= pd * D[k]; if (!norm3(pole)) return; }
    const float x = (a2 * a2 - b * b + d * d) / (2 * d), h = sqrtf(fmaxf(0.0f, a2 * a2 - x * x));
    const float E2[3] = {S[0] + D[0] * x + pole[0] * h, S[1] + D[1] * x + pole[1] * h, S[2] + D[2] * x + pole[2] * h};
    {   // upper arm: aim the elbow (moves everything below the shoulder)
        const float from[3] = {E[0] - S[0], E[1] - S[1], E[2] - S[2]}, to[3] = {E2[0] - S[0], E2[1] - S[1], E2[2] - S[2]};
        moveSet(pose, A.sub[1], A.nSub[1], arc(from, to), S); get();
        if (extra > 0.0f) {   // the stretch: the elbow (and everything below it) out along the upper arm; the bicep's helpers in between by their place
            float u[3] = {E[0] - S[0], E[1] - S[1], E[2] - S[2]}; const float lu = len3f(u);
            if (lu > 1e-3f) { for (auto &q : u) q /= lu;
                static uint8_t inEl[MAXJ]; for (int i = 0; i < A.nSub[1]; i++) if (A.sub[1][i] < MAXJ) inEl[A.sub[1][i]] = 0; for (int i = 0; i < A.nSub[2]; i++) if (A.sub[2][i] < MAXJ) inEl[A.sub[2][i]] = 1;
                for (int i = 0; i < A.nSub[1]; i++) { const int j = A.sub[1][i]; float *m = pose + (size_t)j * 16; float f;
                    if (j < MAXJ && inEl[j]) f = 1.0f;
                    else { const float dj[3] = {m[12] - S[0], m[13] - S[1], m[14] - S[2]}; const float t = dot3(dj, u) / lu; const float off[3] = {dj[0] - u[0] * t * lu, dj[1] - u[1] * t * lu, dj[2] - u[2] * t * lu};
                           f = (t > 0.0f && t < 1.0f && len3f(off) < 6.0f) ? t : 0.0f; }
                    if (f > 0.0f) { m[12] += u[0] * extra * f; m[13] += u[1] * extra * f; m[14] += u[2] * extra * f; } }
                get(); }
        }
    }
    {   // forearm: aim the wrist
        const float W2[3] = {S[0] + D[0] * d, S[1] + D[1] * d, S[2] + D[2] * d};
        const float from[3] = {W[0] - E[0], W[1] - E[1], W[2] - E[2]}, to[3] = {W2[0] - E[0], W2[1] - E[1], W2[2] - E[2]};
        moveSet(pose, A.sub[2], A.nSub[2], arc(from, to), E); get();
    }
    if (T.orient && A.frame) {   // hand: the controller's grip frame (the fingers keep their animated curl)
        const float *M = pose + (size_t)A.hand * 16; float fm[3], tm[3];
        for (int k = 0; k < 3; k++) { fm[k] = A.fl[0] * M[k] + A.fl[1] * M[4 + k] + A.fl[2] * M[8 + k]; tm[k] = A.tl[0] * M[k] + A.tl[1] * M[4 + k] + A.tl[2] * M[8 + k]; }
        const Rot R = scaled(frameTo(fm, tm, T.fing, T.thumb), w);
        moveSet(pose, A.sub[3], A.nSub[3], R, W);
    }
    // 0.18.23 (owner: "when I rotate my wrist, have the forearm rotate a bit too, to a natural degree - otherwise the wrist joint becomes
    // very small"): a real wrist cannot twist - turning the hand (pronation / supination) turns the forearm, the radius rolling round
    // the ulna, most at the wrist and nothing at the elbow. The IK forearm is aimed rigidly, so all the roll sat in the wrist and the
    // skin there pinched. Now the hand's extra roll about the forearm (after the solve, against the animated pose) is shared along the
    // forearm's twist joints by where they sit (25 % along -> 25 % of the roll, the wrist joints all of it), times prm.twist.
    if (tw0ok) {
        const float *Mh = pose + (size_t)A.hand * 16, *Me = pose + (size_t)A.el * 16; float a1[3] = {P(pose, A.hand)[0] - P(pose, A.el)[0], P(pose, A.hand)[1] - P(pose, A.el)[1], P(pose, A.hand)[2] - P(pose, A.el)[2]}; float tw1 = 0;
        if (norm3(a1) && twistAngle(Me + 4 * ek, Mh + 4 * hk, a1, tw1)) {
            float d = tw1 - tw0; while (d > 3.14159265f) d -= 6.2831853f; while (d < -3.14159265f) d += 6.2831853f;
            const float lim = 150.0f / 57.2957795f; d = d > lim ? lim : d < -lim ? -lim : d;
            lastTwistDeg[side & 1] = d * 57.2957795f;
            for (int i = 0; i < A.nTw; i++) { const int j = A.tw[i]; const float pv[3] = {P(pose, j)[0], P(pose, j)[1], P(pose, j)[2]}; const uint16_t one = (uint16_t)j;
                moveSet(pose, &one, 1, axisAngle(a1, d * A.twT[i] * prm.twist), pv); }
        }
    }
}

// ---------- 0.18.9 legs: stand tall while the game crouches into its sword stance (owner: "I don't want him to bend his knees") ----------
struct Leg { int hip = -1, knee = -1, foot = -1; uint16_t sub[3][MAXSUB]; int nSub[3] = {0, 0, 0}; float thigh = 0, shin = 0; };
// legs: the same chain search, low instead of high (the "shoulder" of a leg chain is the hip: 0.25..0.75 of the head height)
static int findLegs(const int16_t *par, int n, const float *pose, float headZ, const uint8_t *skip, Leg legs[2], char *why, int whyN) {
    static int ce[MAXJ], cs[MAXJ];
    for (int j = 1; j < n && j < MAXJ; j++) {
        ce[j] = cs[j] = -1;
        if (skip && skip[j]) continue; if (!shownJ(pose, j)) continue;
        const int e = upFrom(par, pose, j, 20.0f); if (e < 0) continue; const int s = upFrom(par, pose, e, 20.0f); if (s < 0 || par[s] < 0) continue;
        const float de[3] = {P(pose, e)[0] - P(pose, j)[0], P(pose, e)[1] - P(pose, j)[1], P(pose, e)[2] - P(pose, j)[2]}, ds[3] = {P(pose, s)[0] - P(pose, e)[0], P(pose, s)[1] - P(pose, e)[1], P(pose, s)[2] - P(pose, e)[2]};
        const float lo = len3f(de), up = len3f(ds);
        if (lo > 65.0f || up < 25.0f || up > 65.0f) continue;
        if (P(pose, s)[2] < 0.25f * headZ || P(pose, s)[2] > 0.75f * headZ || fabsf(P(pose, s)[1]) < 3.0f || P(pose, j)[2] > 0.3f * headZ) continue;   // hips mid-height, feet low
        ce[j] = e; cs[j] = s;
    }
    int bestE[2] = {-1, -1}, bestS[2] = {-1, -1}, bestN[2] = {0, 0};
    for (int j = 1; j < n && j < MAXJ; j++) {
        if (ce[j] < 0) continue; int cnt = 0; for (int q = 1; q < n && q < MAXJ; q++) if (ce[q] == ce[j] && cs[q] == cs[j]) cnt++;
        const int side = P(pose, cs[j])[1] > 0 ? 0 : 1;
        if (cnt > bestN[side]) { bestN[side] = cnt; bestE[side] = ce[j]; bestS[side] = cs[j]; }
    }
    int found = 0;
    for (int side = 0; side < 2; side++) {
        Leg &L = legs[side]; L = Leg{}; if (bestE[side] < 0) continue;
        const int e = bestE[side], s = bestS[side]; int foot = -1, footAny = -1; float fd = 1e9f, fdAny = 1e9f;   // the ankle: nearest the knee with toes below it
        for (int j = 1; j < n && j < MAXJ; j++) { if (ce[j] != e || cs[j] != s) continue; const float d[3] = {P(pose, j)[0] - P(pose, e)[0], P(pose, j)[1] - P(pose, e)[1], P(pose, j)[2] - P(pose, e)[2]}; const float dl = len3f(d);
            if (dl < fdAny) { fdAny = dl; footAny = j; }
            bool below = false; for (int q = j + 1; q < n && q < MAXJ && !below; q++) if (ce[q] == e && cs[q] == s && isAncestor(par, j, q)) below = true;
            if (below && dl < fd) { fd = dl; foot = j; } }
        if (foot < 0) foot = footAny;
        if (foot < 0) continue;
        L.hip = s; L.knee = e; L.foot = foot; const int ids[3] = {s, e, foot}; bool ok = true;
        for (int q = 0; q < 3; q++) { L.nSub[q] = subtree(par, n, ids[q], L.sub[q], MAXSUB); if (L.nSub[q] <= 0) ok = false; }
        if (!ok) { L.hip = -1; continue; }
        const float a[3] = {P(pose, e)[0] - P(pose, s)[0], P(pose, e)[1] - P(pose, s)[1], P(pose, e)[2] - P(pose, s)[2]}, b[3] = {P(pose, foot)[0] - P(pose, e)[0], P(pose, foot)[1] - P(pose, e)[1], P(pose, foot)[2] - P(pose, e)[2]};
        L.thigh = len3f(a); L.shin = len3f(b); found++;
    }
    if (why) snprintf(why, (size_t)whyN, "left leg hip %d knee %d foot %d (thigh %.0f / shin %.0f cm), right leg hip %d knee %d foot %d (thigh %.0f / shin %.0f cm)",
                      legs[0].hip, legs[0].knee, legs[0].foot, legs[0].thigh, legs[0].shin, legs[1].hip, legs[1].knee, legs[1].foot, legs[1].thigh, legs[1].shin);
    return found;
}
// raise everything by dz (model up = +z), then put each foot back where the animation planted it with a two-bone leg IK (knee pole
// forward, turned with the drawn body), keeping the foot's own orientation. Returns the raise actually applied (limited by leg reach).
static float standTall(float *pose, int n, const Leg legs[2], float dz, float bodyYaw) {
    if (dz <= 0.0f || legs[0].hip < 0 || legs[1].hip < 0) return 0.0f;
    for (int side = 0; side < 2; side++) {   // never past 98% of a straight leg
        const Leg &L = legs[side]; const float *H = P(pose, L.hip), *F = P(pose, L.foot);
        const float reach = (L.thigh + L.shin) * 0.98f, dx = H[0] - F[0], dy = H[1] - F[1], h0 = H[2] - F[2];
        const float hmax = sqrtf(fmaxf(0.0f, reach * reach - dx * dx - dy * dy)) - h0; if (dz > hmax) dz = fmaxf(0.0f, hmax);
    }
    if (dz <= 0.05f) return 0.0f;
    float feet[2][16]; for (int side = 0; side < 2; side++) memcpy(feet[side], pose + (size_t)legs[side].foot * 16, 64);
    for (int j = 0; j < n; j++) pose[(size_t)j * 16 + 14] += dz;
    const float c = cosf(bodyYaw), s = sinf(bodyYaw);
    for (int side = 0; side < 2; side++) {
        const Leg &L = legs[side]; float S[3], E[3], W[3]; memcpy(S, P(pose, L.hip), 12); memcpy(E, P(pose, L.knee), 12); memcpy(W, P(pose, L.foot), 12);
        const float T[3] = {feet[side][12], feet[side][13], feet[side][14]}; const float a = L.thigh, b = L.shin;
        float D[3] = {T[0] - S[0], T[1] - S[1], T[2] - S[2]}; float d = len3f(D); if (!(d > 1e-3f)) continue; for (auto &x : D) x /= d;
        d = fminf(fmaxf(d, fabsf(a - b) * 1.01f + 0.5f), (a + b) * 0.999f);
        float pole[3] = {c, s, 0.0f}; const float pd = dot3(pole, D); for (int k = 0; k < 3; k++) pole[k] -= pd * D[k]; if (!norm3(pole)) continue;   // knees forward
        const float x = (a * a - b * b + d * d) / (2 * d), h = sqrtf(fmaxf(0.0f, a * a - x * x));
        const float E2[3] = {S[0] + D[0] * x + pole[0] * h, S[1] + D[1] * x + pole[1] * h, S[2] + D[2] * x + pole[2] * h};
        { const float from[3] = {E[0] - S[0], E[1] - S[1], E[2] - S[2]}, to[3] = {E2[0] - S[0], E2[1] - S[1], E2[2] - S[2]}; moveSet(pose, L.sub[0], L.nSub[0], arc(from, to), S); }
        memcpy(E, P(pose, L.knee), 12); memcpy(W, P(pose, L.foot), 12);
        { const float W2[3] = {S[0] + D[0] * d, S[1] + D[1] * d, S[2] + D[2] * d}; const float from[3] = {W[0] - E[0], W[1] - E[1], W[2] - E[2]}, to[3] = {W2[0] - E[0], W2[1] - E[1], W2[2] - E[2]}; moveSet(pose, L.sub[1], L.nSub[1], arc(from, to), E); }
        {   // the foot keeps the animation's orientation (flat on the ground)
            const float *M = pose + (size_t)L.foot * 16; const float r0[3] = {M[0], M[1], M[2]}, r1[3] = {M[4], M[5], M[6]}, o0[3] = {feet[side][0], feet[side][1], feet[side][2]}, o1[3] = {feet[side][4], feet[side][5], feet[side][6]};
            const float Wp[3] = {M[12], M[13], M[14]}; moveSet(pose, L.sub[2], L.nSub[2], frameTo(r0, r1, o0, o1), Wp);
        }
    }
    return dz;
}
} // namespace fbik
