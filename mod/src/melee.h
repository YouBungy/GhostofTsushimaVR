// GoTVR 0.18.47 physical katana hits: the decision core (pure, self-tested; no game access).
// Playbook rules it follows (docs ch02 "Throwing, knocking, and physical melee", ch03 "the melee rule was re-derived", HAND-004,
// INPUT-010 / INPUT-012):
//  - bill the CLOSING speed of blade and target at the point where they meet, along the line between them - not the controller's raw
//    speed (walking into an enemy with the blade out must not hit; an enemy running onto a held blade may)
//  - the blade point's own velocity (a wrist flick moves the tip, not the hand)
//  - a fast swing must not slip between frames: the tip's path since the last frame is tested too (a short swept segment)
//  - hysteresis: a target is hit once per pass - it re-arms only after the blade has left it for rearmMs; a global cooldown on top
//  - samples more than maxGapMs apart are discarded (a hitch or a load is a teleport, not a swing)
//  - the speed and the travel are both measured relative to the target, with the player's own locomotion taken off
//  - two counters that separate "never evaluated" from "evaluated but too slow" (peak closing speed, evaluated samples)
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <utility>
namespace melee {
struct V3 { float x = 0, y = 0, z = 0; };
static inline V3 v3(float x, float y, float z) { V3 r; r.x = x; r.y = y; r.z = z; return r; }
static inline V3 sub(V3 a, V3 b) { return v3(a.x - b.x, a.y - b.y, a.z - b.z); }
static inline V3 add(V3 a, V3 b) { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
static inline V3 mul(V3 a, float s) { return v3(a.x * s, a.y * s, a.z * s); }
static inline float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline float len(V3 a) { return sqrtf(dot(a, a)); }
// closest points of segments p0-p1 and q0-q1: returns the squared distance, s / t the parameters
static inline float segSeg(V3 p0, V3 p1, V3 q0, V3 q1, float &s, float &t) {
    const V3 d1 = sub(p1, p0), d2 = sub(q1, q0), r = sub(p0, q0); const float a = dot(d1, d1), e = dot(d2, d2), f = dot(d2, r);
    if (a <= 1e-8f && e <= 1e-8f) { s = t = 0; return dot(r, r); }
    if (a <= 1e-8f) { s = 0; t = f / e; t = t < 0 ? 0 : t > 1 ? 1 : t; }
    else { const float c = dot(d1, r);
        if (e <= 1e-8f) { t = 0; s = -c / a; s = s < 0 ? 0 : s > 1 ? 1 : s; }
        else { const float b = dot(d1, d2), den = a * e - b * b; s = den > 1e-8f ? (b * f - c * e) / den : 0.0f; s = s < 0 ? 0 : s > 1 ? 1 : s;
            t = (b * s + f) / e; if (t < 0) { t = 0; s = -c / a; s = s < 0 ? 0 : s > 1 ? 1 : s; } else if (t > 1) { t = 1; s = (b - c) / a; s = s < 0 ? 0 : s > 1 ? 1 : s; } } }
    const V3 c1 = add(p0, mul(d1, s)), c2 = add(q0, mul(d2, t)); const V3 d = sub(c1, c2); return dot(d, d); }
struct Cfg {
    float minSpeed = 2.2f;      // m/s closing speed at the contact (sword_hit_min_speed)
    float minTravelCm = 18.0f;  // cm the contact point travelled over the last 150 ms (sword_hit_min_travel_cm)
    float contactCm = 6.0f;     // extra margin around the target capsule (sword_hit_contact_cm)
    int cooldownMs = 220;       // between any two hits (sword_hit_cooldown_ms)
    int rearmMs = 120;          // the blade must be out of that target this long before it can hit it again (sword_hit_rearm_ms)
    int maxGapMs = 100;         // a sample gap above this is a hitch: the history restarts
    float bladeSkipCm = 4.0f;   // the first cm of the blade from the guard do not cut
};
struct Target { int id = -1; V3 a, b; float r = 30.0f; };   // a capsule in world cm (feet .. head), id: stable per enemy
static const int MAXT = 32, HIST = 8;
struct Hit { int target = -1; float speed = 0, travel = 0; V3 at; };
struct Core {
    // blade samples (world cm): guard g, tip t, with their time
    V3 hg[HIST], ht[HIST]; uint64_t hms[HIST]; int hn = 0;
    int tid[MAXT]; bool inside[MAXT]; uint64_t outSince[MAXT]; V3 tpos[MAXT]; uint64_t tms[MAXT]; int nt = 0;
    uint64_t lastHit = 0; float peakSpeed = 0; uint32_t evaluated = 0, contacts = 0, tooSlow = 0, tooShort = 0, cooling = 0, hits = 0;
    void reset() { hn = 0; nt = 0; }
    int slot(int id, const V3 &mid, uint64_t now) { for (int i = 0; i < nt; i++) if (tid[i] == id) return i;
        int i = nt; if (nt >= MAXT) { i = 0; for (int k = 1; k < nt; k++) if (tms[k] < tms[i]) i = k; } else nt++;   // full: the one unseen longest
        tid[i] = id; inside[i] = false; outSince[i] = 0; tpos[i] = mid; tms[i] = now; return i; }
    // one frame: the blade guard -> tip now (world cm), the targets; returns the number of hits written to out.
    // selfVel (cm/s): the player's own body velocity - taken off the blade's, so walking / turning into an enemy with the blade held
    // out is not a swing (playbook INPUT-010: bill the swing, not the locomotion); an enemy running onto the blade still is.
    int step(uint64_t now, V3 guard, V3 tip, const Target *tg, int ntg, const Cfg &c, Hit *out, int maxOut, V3 selfVel = V3()) {
        int no = 0;
        if (hn > 0 && (now <= hms[hn - 1] || now - hms[hn - 1] > (uint64_t)c.maxGapMs)) hn = 0;   // a hitch or a stall: start again
        if (hn == HIST) { for (int i = 1; i < HIST; i++) { hg[i - 1] = hg[i]; ht[i - 1] = ht[i]; hms[i - 1] = hms[i]; } hn--; }
        hg[hn] = guard; ht[hn] = tip; hms[hn] = now; hn++;
        const V3 bd = sub(tip, guard); const float bl = len(bd); if (bl < 1.0f) return 0;
        const V3 g0 = add(guard, mul(bd, c.bladeSkipCm / bl));
        for (int k = 0; k < ntg; k++) {
            const Target &T = tg[k]; const V3 mid = mul(add(T.a, T.b), 0.5f); const int si = slot(T.id, mid, now); if (si < 0) continue;
            const float dtT = tms[si] && now > tms[si] ? (now - tms[si]) * 0.001f : 0.0f; const V3 tv = dtT > 0.0f && dtT < 0.2f ? mul(sub(mid, tpos[si]), 1.0f / dtT) : v3(0, 0, 0);
            tpos[si] = mid; tms[si] = now;
            float s = 0, t = 0; float d2 = segSeg(g0, tip, T.a, T.b, s, t);
            if (hn >= 2) { float s2, t2; const float dp = segSeg(ht[hn - 2], tip, T.a, T.b, s2, t2); if (dp < d2) { d2 = dp; s = 1.0f; } }   // the tip's sweep since the last frame
            const float lim = T.r + c.contactCm, d = sqrtf(d2);
            if (d > lim) { if (inside[si]) { inside[si] = false; outSince[si] = now; } continue; }
            if (inside[si]) continue;   // still in it from this pass
            if (outSince[si] && now - outSince[si] < (uint64_t)c.rearmMs) continue;
            // the contact point on the blade (parameter s along g0..tip) - its velocity from the oldest usable sample within 150 ms
            const V3 cp = add(g0, mul(sub(tip, g0), s)); int o = hn - 2; while (o > 0 && now - hms[o - 1] <= 150) o--;
            if (o < 0) { inside[si] = true; continue; }
            const V3 og0 = add(hg[o], mul(sub(ht[o], hg[o]), c.bladeSkipCm / fmaxf(1.0f, len(sub(ht[o], hg[o]))))); const V3 ocp = add(og0, mul(sub(ht[o], og0), s));
            const float dts = (now - hms[o]) * 0.001f; if (dts <= 0.0f) { inside[si] = true; continue; }
            const V3 vb = mul(sub(cp, ocp), 1.0f / dts); const V3 vr = sub(sub(vb, selfVel), tv);   // cm/s: the swing alone, relative to the target
            // the line from the target's axis to the contact point: the closing speed is the relative velocity INTO the target
            const V3 ax = sub(T.b, T.a); const float al2 = dot(ax, ax); float u = al2 > 1e-6f ? dot(sub(cp, T.a), ax) / al2 : 0.0f; u = u < 0 ? 0 : u > 1 ? 1 : u;
            V3 n = sub(cp, add(T.a, mul(ax, u))); const float nl = len(n); n = nl > 1e-3f ? mul(n, 1.0f / nl) : mul(vr, -1.0f / fmaxf(1e-3f, len(vr)));
            const float closing = -dot(vr, n) * 0.01f, travel = len(vr) * dts;   // the travel relative to the target (no locomotion) too evaluated++; contacts++;
            if (closing > peakSpeed) peakSpeed = closing;
            inside[si] = true;
            if (closing < c.minSpeed) { tooSlow++; continue; }
            if (travel < c.minTravelCm) { tooShort++; continue; }
            if (lastHit && now - lastHit < (uint64_t)c.cooldownMs) { cooling++; continue; }
            lastHit = now; hits++;
            if (no < maxOut) { out[no].target = T.id; out[no].speed = closing; out[no].travel = travel; out[no].at = cp; no++; }
        }
        return no;
    }
};
// self-test: a fast slash through a standing target hits once; the same pass held inside does not hit again; a slow push does not hit;
// walking the blade into the target at 1.5 m/s does not hit; an out-and-back-in after the re-arm time hits again; a hitch restarts
static inline bool selfTest() {
    Cfg c; Core m; Target T; T.id = 7; T.a = v3(100, 0, 0); T.b = v3(100, 0, 170); T.r = 30; Hit h[4]; bool ok = true; uint64_t t = 1000;
    // slash: the blade (horizontal, along y) sweeps in x from 30 to 100 at 4 m/s (400 cm/s, 18 ms frames)
    auto blade = [&](float x) { return std::make_pair(v3(x, -60, 120), v3(x, 40, 120)); };
    int hits = 0; for (float x = 20; x <= 110; x += 7.2f) { t += 18; auto b = blade(x); hits += m.step(t, b.first, b.second, &T, 1, c, h, 4); }
    ok &= hits == 1 && h[0].target == 7 && h[0].speed > 3.0f;
    for (int i = 0; i < 10; i++) { t += 18; auto b = blade(100); hits += m.step(t, b.first, b.second, &T, 1, c, h, 4); }   // held inside: no second hit
    ok &= hits == 1;
    Core m2; int h2 = 0; for (float x = 20; x <= 110; x += 2.7f) { t += 18; auto b = blade(x); h2 += m2.step(t, b.first, b.second, &T, 1, c, h, 4); }   // 1.5 m/s: no hit
    ok &= h2 == 0 && m2.tooSlow >= 1;
    // out, wait past the re-arm, back in fast: hits again; a 200 ms hitch in the middle restarts the history without a false hit
    for (float x = 100; x >= 20; x -= 7.2f) { t += 18; auto b = blade(x); m.step(t, b.first, b.second, &T, 1, c, h, 4); }
    t += 300; for (float x = 20; x <= 110; x += 7.2f) { t += 18; auto b = blade(x); hits += m.step(t, b.first, b.second, &T, 1, c, h, 4); }
    ok &= hits == 2;
    Core m3; t += 1000; { auto b = blade(20); m3.step(t, b.first, b.second, &T, 1, c, h, 4); } t += 250; { auto b = blade(100); ok &= m3.step(t, b.first, b.second, &T, 1, c, h, 4) == 0; }   // a teleport is not a swing
    // a thrust along the blade into the target's side (tip leads) at 4 m/s hits
    Core m4; int h4 = 0; for (float y = -140; y <= -20; y += 7.2f) { t += 18; h4 += m4.step(t, v3(100, y - 100, 120), v3(100, y, 120), &T, 1, c, h, 4); }
    ok &= h4 == 1;
    // walking with the blade held out into the target at 4 m/s (the body's own velocity): no hit; the same with the target running
    // onto the held blade at 4 m/s: a hit
    Core m5; int h5 = 0; for (float x = 20; x <= 110; x += 7.2f) { t += 18; auto b = blade(x); h5 += m5.step(t, b.first, b.second, &T, 1, c, h, 4, v3(400, 0, 0)); }
    ok &= h5 == 0;
    Core m6; int h6 = 0; for (float x = 180; x >= 0; x -= 7.2f) { t += 18; Target R = T; R.a.x = x; R.b.x = x; auto b = blade(20); h6 += m6.step(t, b.first, b.second, &R, 1, c, h, 4); }
    ok &= h6 == 1;
    // many targets: the table never fills up (the longest unseen is reused)
    Core m7; for (int k = 0; k < 100; k++) { t += 18; Target R = T; R.id = 1000 + k; auto b = blade(400); m7.step(t, b.first, b.second, &R, 1, c, h, 4); } ok &= m7.nt == MAXT;
    return ok;
}
} // namespace melee
