// GoTVR 0.18.61 block / deflect (parry) with the physical katana - the design doc's [COMBAT DESIGN]: "Blocking is done by simply
// holding their sword still when in contact with an enemy weapon. Deflecting requires some velocity into the opponent's weapon", with
// its parry parameters: Blade-to-Blade Tolerance, Contact Validity Window, Minimum Parry Speed, Minimum Parry Travel Distance, Parry
// window multiplier, Perfect parry window multiplier, Show Contact and Result.
// Pure, self-tested, no game access (playbook HAND-004: a decision core with its own test). Units: cm, ms, m/s.
//  - The enemy's weapon is not tracked; its blow is: when the game is about to apply an enemy's hit to Jin, the attack's line runs from
//    the attacker's chest to where it lands on Jin. The katana DEFENDS if, at any moment within the validity window (windowMs *
//    windowMult), the blade (guard -> tip) came within tolCm of that line.
//  - mode 0 TIMING (default; owner 2026-10-22: "no velocity is required, simply interceding the enemy's blade counts as a block, so the
//    player must time when they raise their sword ... hold my sword up right at the beginning = block; wait for the right window and
//    intercept their sword with mine = parry"): when did the blade ENTER the blow's line? Entered within parryMs * perfectMult before the
//    blow lands -> PARRY; already there for longer (raised early and held) -> BLOCK.
//  - mode 1 VELOCITY (the first idea, kept as a setting): DEFLECT when the tip moved fast in the last perfectMs * perfectMult (top speed
//    >= minSpeed and path >= minTravelCm), BLOCK when held still.
//  The game then decides block / parry / perfect parry / guard break itself from the guard button timing the mod gives it.
#pragma once
#include <cmath>
#include <cstdint>
#include "melee.h"
namespace parry {
using melee::V3;
struct Cfg {
    float tolCm = 30.0f, windowMs = 300.0f, minSpeed = 1.5f, minTravelCm = 10.0f, windowMult = 1.0f, perfectMult = 1.0f, perfectMs = 150.0f;
    int mode = 0; float parryMs = 220.0f;   // timing mode: the blade entered the line within parryMs * perfectMult = parry
};
enum : int { NONE = 0, BLOCK = 1, DEFLECT = 2 };
static const int HN = 64;   // 0.18.62: 64 samples, at most one per 8 ms (the session-18 history held only 76 ms - several pushes per frame)
struct Hist {
    V3 g[HN], t[HN]; uint64_t ms[HN]; int n = 0, w = 0;
    void push(V3 guard, V3 tip, uint64_t now) { g[w] = guard; t[w] = tip; ms[w] = now; w = (w + 1) % HN; if (n < HN) n++; }
    int idx(int k) const { return ((w - 1 - k) % HN + HN) % HN; }   // k = 0 newest
};
struct Result { int kind = NONE; float distCm = 1e9f, speed = 0, travelCm = 0, enteredMs = -1; };
static inline Result judge(const Hist &h, uint64_t now, V3 attacker, V3 landing, const Cfg &c) {
    Result r; const float win = c.windowMs * c.windowMult, pwin = c.perfectMs * c.perfectMult;
    for (int k = 0; k < h.n; k++) { const int i = h.idx(k); if ((float)(now - h.ms[i]) > win) break;
        float s, t; const float d2 = melee::segSeg(h.g[i], h.t[i], attacker, landing, s, t); if (d2 < r.distCm * r.distCm) r.distCm = sqrtf(d2); }
    if (r.distCm > c.tolCm) return r;
    if (c.mode == 0) {   // timing: the run of contact the latest contact belongs to - when did it start?
        int k0 = -1; for (int k = 0; k < h.n; k++) { const int i = h.idx(k); if ((float)(now - h.ms[i]) > win) break; float s, t; if (melee::segSeg(h.g[i], h.t[i], attacker, landing, s, t) <= c.tolCm * c.tolCm) { k0 = k; break; } }
        if (k0 < 0) return r; uint64_t entry = h.ms[h.idx(k0)]; bool open = true;
        for (int k = k0 + 1; k < h.n; k++) { const int i = h.idx(k); float s, t; if (melee::segSeg(h.g[i], h.t[i], attacker, landing, s, t) > c.tolCm * c.tolCm) { open = false; break; } entry = h.ms[i]; }
        r.enteredMs = (float)(now - entry); const bool parryT = !open && r.enteredMs <= c.parryMs * c.perfectMult;   // a run reaching past the history = held up long ago
        r.kind = parryT ? DEFLECT : BLOCK; return r; }
    for (int k = 0; k + 1 < h.n; k++) { const int i = h.idx(k), j = h.idx(k + 1); if ((float)(now - h.ms[j]) > pwin) break;
        const float dms = (float)(h.ms[i] - h.ms[j]); if (dms <= 0.5f) continue; const float d = melee::len(melee::sub(h.t[i], h.t[j]));
        r.travelCm += d; const float sp = d / dms * 10.0f; if (sp > r.speed) r.speed = sp; }   // cm/ms -> m/s
    r.kind = (r.speed >= c.minSpeed && r.travelCm >= c.minTravelCm) ? DEFLECT : BLOCK;
    return r;
}
static inline bool selfTest() {
    bool ok = true; Cfg c; c.mode = 1; const V3 A = melee::v3(0, 150, 130), P = melee::v3(0, 0, 130);   // an attacker 1.5 m in front, the blow landing at chest height
    // held still across the line: BLOCK
    { Hist h; for (int i = 0; i < 10; i++) h.push(melee::v3(-20, 40, 130), melee::v3(40, 50, 140), 1000 + i * 16); const Result r = judge(h, 1160, A, P, c); ok &= r.kind == BLOCK && r.distCm < 30.0f; }
    // swung fast through the line (3 m/s, 16 ms frames = 4.8 cm per frame): DEFLECT
    { Hist h; for (int i = 0; i < 10; i++) h.push(melee::v3(-60.0f + i * 4.8f, 40, 130), melee::v3(-20.0f + i * 4.8f, 60, 140), 2000 + i * 16); const Result r = judge(h, 2160, A, P, c); ok &= r.kind == DEFLECT && r.speed > 2.5f; }
    // held far to the side: NONE
    { Hist h; for (int i = 0; i < 10; i++) h.push(melee::v3(-120, 0, 60), melee::v3(-160, 20, 40), 3000 + i * 16); ok &= judge(h, 3160, A, P, c).kind == NONE; }
    // across the line, but longer ago than the validity window: NONE; with the parry window doubled: BLOCK
    { Hist h; for (int i = 0; i < 5; i++) h.push(melee::v3(-20, 40, 130), melee::v3(40, 50, 140), 4000 + i * 16); for (int i = 0; i < 5; i++) h.push(melee::v3(-120, 0, 60), melee::v3(-160, 20, 40), 4400 + i * 16);
      ok &= judge(h, 4480, A, P, c).kind == NONE; Cfg c2 = c; c2.windowMult = 2.0f; ok &= judge(h, 4480, A, P, c2).kind == BLOCK; }
    // a tremor (fast but 4 cm in all): BLOCK, not a deflect
    { Hist h; for (int i = 0; i < 10; i++) h.push(melee::v3(-20, 40, 130), melee::v3(40.0f + ((i & 1) ? 0.4f : 0.0f), 50, 140), 5000 + i * 16); ok &= judge(h, 5160, A, P, c).kind == BLOCK; }
    // timing mode (default): raised into the line 100 ms before the blow, still or not -> PARRY; held there from 600 ms before -> BLOCK;
    // held up since before the history began -> BLOCK; never there -> NONE
    { Cfg t = c; t.mode = 0;
      Hist h1; for (int i = 0; i < 20; i++) h1.push(i < 14 ? melee::v3(-120, 0, 60) : melee::v3(-20, 40, 130), i < 14 ? melee::v3(-160, 20, 40) : melee::v3(40, 50, 140), 6000 + i * 16);
      const Result a = judge(h1, 6000 + 19 * 16 + 5, A, P, t); ok &= a.kind == DEFLECT && a.enteredMs < 120.0f;
      Hist h2; for (int i = 0; i < 32; i++) h2.push(i < 2 ? melee::v3(-120, 0, 60) : melee::v3(-20, 40, 130), i < 2 ? melee::v3(-160, 20, 40) : melee::v3(40, 50, 140), 7000 + i * 20);
      t.windowMs = 1000.0f; ok &= judge(h2, 7000 + 31 * 20 + 5, A, P, t).kind == BLOCK;
      Hist h3; for (int i = 0; i < 40; i++) h3.push(melee::v3(-20, 40, 130), melee::v3(40, 50, 140), 8000 + i * 16); ok &= judge(h3, 8000 + 39 * 16 + 5, A, P, t).kind == BLOCK;
      Hist h4; for (int i = 0; i < 20; i++) h4.push(melee::v3(-120, 0, 60), melee::v3(-160, 20, 40), 9000 + i * 16); ok &= judge(h4, 9000 + 19 * 16 + 5, A, P, t).kind == NONE; }
    return ok;
}
} // namespace parry
