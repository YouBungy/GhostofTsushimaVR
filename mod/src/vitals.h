// GoTVR wrist vitals (docs/WRIST_HUD.md): the decision core for health + resolve on Jin's left forearm. Pure, self-tested, no game
// access. Behaviour modelled on bldbrne-vr's wrist Blood Gauge (its test list: fills with guards, change edges, a low-health
// heartbeat, glance to show) and the playbook's ch04 (look-activate with a low-health override, body-locked panels), re-implemented
// here for Ghost of Tsushima's two values: health (hero+0x1974 / +0x1970) and resolve (location still to be found - WRIST_HUD.md W0).
#pragma once
#include <cmath>
#include <cstdint>
namespace vit {
struct Status { float hp = 0, hpMax = 0, res = 0, resMax = 0; bool ok = false; };   // res / resMax in the game's own units
struct Fill { float hp = 0, res = 0; int resFull = 0, resUnits = 0; float resPart = 0; };   // fractions 0..1; resolve as whole units + the part filling
static inline float clamp01(float v) { return std::isfinite(v) ? (v < 0 ? 0 : v > 1 ? 1 : v) : 0; }
// resUnit: how much of the game's resolve value one segment is (the game shows resolve as segments; 1 until W0 finds the scale)
static inline Fill fillOf(const Status &s, float resUnit = 1.0f) {
    Fill f; if (!s.ok) return f;
    f.hp = s.hpMax > 0 ? clamp01(s.hp / s.hpMax) : 0; f.res = s.resMax > 0 ? clamp01(s.res / s.resMax) : 0;
    if (resUnit > 0 && std::isfinite(s.resMax) && s.resMax > 0 && s.resMax / resUnit < 64) {
        f.resUnits = (int)(s.resMax / resUnit + 0.5f); const float r = fminf(fmaxf(std::isfinite(s.res) ? s.res : 0, 0), s.resMax) / resUnit;
        f.resFull = (int)floorf(r + 1e-4f); f.resPart = clamp01(r - (float)f.resFull); if (f.resFull >= f.resUnits) { f.resFull = f.resUnits; f.resPart = 0; } }
    return f;
}
// what changed since the last good reading (bits): the panel flashes / shows on these, and the heartbeat's mute timer restarts on a hit
enum : uint32_t { CHit = 1, CHeal = 2, CResGain = 4, CResSpend = 8, CMaxHp = 16, CMaxRes = 32, CDied = 64, CRevived = 128 };
struct ChangeTracker {
    Status last; bool have = false;
    uint32_t step(const Status &s) {   // a failed read changes nothing and forgets nothing
        if (!s.ok || !std::isfinite(s.hp) || !std::isfinite(s.hpMax) || s.hpMax <= 0) return 0;
        if (!have) { last = s; have = true; return 0; }
        uint32_t b = 0; const float eh = 0.05f, er = 0.01f;
        if (fabsf(s.hpMax - last.hpMax) > eh) b |= CMaxHp;   // an upgrade: not a hit or a heal
        else if (s.hp < last.hp - eh) b |= s.hp <= 0 ? (CHit | CDied) : CHit;
        else if (s.hp > last.hp + eh) b |= last.hp <= 0 ? CRevived : CHeal;
        if (fabsf(s.resMax - last.resMax) > er) b |= CMaxRes;
        else if (s.res > last.res + er) b |= CResGain; else if (s.res < last.res - er) b |= CResSpend;
        last = s; return b;
    }
};
// the low-health heartbeat (design doc "Rumble feedback low health"): below `low` of max health, 60 bpm at the line rising to 130 at
// 5 %, none when dead; a lub-dub felt in the left controller (and the panel pulses with it), quiet muteS after the last health lost
static inline float bpm(float hpFrac, float low = 0.30f) {
    if (!(hpFrac > 0.0f) || hpFrac >= low) return 0.0f; const float t = clamp01((low - hpFrac) / fmaxf(1e-3f, low - 0.05f)); return 60.0f + 70.0f * t; }
static inline float beatEnvelope(float t) {   // seconds since the beat: the lub (peak 1 at 25 ms), the dub (0.7 at 175 ms), quiet after
    if (t < 0) return 0; auto bump = [](float x, float c, float w) { const float d = (x - c) / w; return expf(-d * d); };
    return fmaxf(bump(t, 0.025f, 0.03f), 0.7f * bump(t, 0.175f, 0.035f)); }
struct Heartbeat {
    bool beating = false; float since = 0;   // seconds since the last beat
    bool step(float dt, float hpFrac, bool active, float low = 0.30f) {   // true on a beat (the first one at once)
        const float r = active ? bpm(hpFrac, low) : 0.0f; if (r <= 0) { beating = false; since = 0; return false; }
        if (!beating) { beating = true; since = 0; return true; }
        since += dt; const float period = 60.0f / r; if (since >= period - 1e-5f) { since -= period; if (since > period) since = 0; return true; } return false; }
    float pulse() const { return beating ? beatEnvelope(since) : 0.0f; }
};
static inline bool heartbeatFelt(uint64_t nowMs, uint64_t lastHitMs, float muteS = 20.0f) { return lastHitMs && nowMs >= lastHitMs && (float)(nowMs - lastHitMs) <= muteS * 1000.0f; }
// glance: the panel shows when it faces your eyes, near enough, and near the middle of your view - after showS, hidden after hideS
// of not looking; forced on below the low-health line and for forceS after a change (playbook ch04: the escape hatch for the case
// that matters)
struct V3 { float x = 0, y = 0, z = 0; };
static inline V3 v3(float x, float y, float z) { V3 r; r.x = x; r.y = y; r.z = z; return r; }
static inline V3 sub(V3 a, V3 b) { return v3(a.x - b.x, a.y - b.y, a.z - b.z); }
static inline float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline float len(V3 a) { return sqrtf(dot(a, a)); }
struct GlanceCfg { float faceDeg = 50, viewDeg = 35, maxM = 0.65f, showS = 0.12f, hideS = 0.25f, forceS = 2.0f, low = 0.30f; };
struct Glance {
    bool shown = false; float in = 0, out = 0, force = 0;
    // p, n: the panel's centre and its face normal; eye, fwd: the head (metres, any one space); hpFrac; changed: this frame's bits
    bool step(V3 p, V3 n, V3 eye, V3 fwd, float dt, float hpFrac, uint32_t changed, const GlanceCfg &c = GlanceCfg()) {
        if (changed & (CHit | CHeal | CResGain | CResSpend | CDied)) force = c.forceS; else force = fmaxf(0.0f, force - dt);
        const V3 to = sub(eye, p); const float d = len(to); const float nl = len(n), fl = len(fwd);
        bool look = false;
        if (d > 1e-4f && d <= c.maxM && nl > 1e-6f && fl > 1e-6f) {
            const float face = dot(to, n) / (d * nl), view = -dot(to, fwd) / (d * fl);
            look = face >= cosf(c.faceDeg * 0.0174533f) && view >= cosf(c.viewDeg * 0.0174533f); }
        if (look) { in += dt; out = 0; } else { out += dt; in = 0; }
        if (!shown && look && in >= c.showS - 1e-5f) shown = true; else if (shown && !look && out >= c.hideS - 1e-5f) shown = false;
        return shown || force > 0 || (hpFrac > 0 && hpFrac < c.low);
    }
};
// the design doc's "left hand up to the head shows the full HUD": the left grip within nearM of the eye and in front of it, held holdS,
// released past farM (hysteresis)
struct HandAtHead { bool on = false; float held = 0;
    bool step(V3 hand, V3 eye, V3 fwd, float dt, float nearM = 0.22f, float farM = 0.30f, float holdS = 0.30f) {
        const V3 to = sub(hand, eye); const float d = len(to); const bool front = dot(to, fwd) > 0.0f;
        if (!on) { if (d < nearM && front) { held += dt; if (held >= holdS - 1e-5f) on = true; } else held = 0; }
        else if (d > farM || !front) { on = false; held = 0; }
        return on; } };
// self-test (gotvr's GoTVR_SelfTest): the cases WRIST_HUD.md lists
static inline bool selfTest() {
    bool ok = true; Status s; s.ok = true; s.hp = 175; s.hpMax = 350; s.res = 2.5f; s.resMax = 4;
    Fill f = fillOf(s); ok &= fabsf(f.hp - 0.5f) < 1e-6f && f.resUnits == 4 && f.resFull == 2 && fabsf(f.resPart - 0.5f) < 1e-4f;
    Status z; z.ok = true; z.hp = 10; z.hpMax = 0; z.res = -3; z.resMax = 4; f = fillOf(z); ok &= f.hp == 0 && f.resFull == 0 && f.resPart == 0;   // guards
    Status o = s; o.hp = 900; o.res = 9; f = fillOf(o); ok &= f.hp == 1 && f.resFull == 4 && f.resPart == 0;
    Status bad; f = fillOf(bad); ok &= f.hp == 0 && f.resUnits == 0;
    ChangeTracker t; ok &= t.step(s) == 0; ok &= t.step(Status()) == 0;   // first: none; a failed read: none, nothing forgotten
    Status a = s; a.hp = 100; ok &= t.step(a) == CHit; a.hp = 222.5f; ok &= t.step(a) == CHeal;   // a resolve heal (+122.5)
    a.res = 1.5f; ok &= t.step(a) == CResSpend; a.res = 2.0f; ok &= t.step(a) == CResGain;
    a.hpMax = 400; a.hp = 400; ok &= t.step(a) == CMaxHp;   // an upgrade is not a heal
    a.hp = 0; ok &= t.step(a) == (CHit | CDied); a.hp = 120; ok &= t.step(a) == CRevived;
    ok &= bpm(0.31f) == 0 && bpm(1.0f) == 0 && bpm(0.0f) == 0 && fabsf(bpm(0.2999f) - 60.0f) < 0.1f && fabsf(bpm(0.05f) - 130.0f) < 0.01f && fabsf(bpm(0.01f) - 130.0f) < 0.01f;
    { float last = 0; for (float h = 0.299f; h > 0.0f; h -= 0.005f) { const float b = bpm(h); ok &= b >= last - 1e-3f; last = b; } }   // faster as health falls
    ok &= bpm(0.35f, 0.40f) > 0 && bpm(0.25f, 0.20f) == 0;
    { Heartbeat h; const float dt = 1.0f / 90.0f; int n = 0; float first = -1, prev = -1, maxGap = 0, minGap = 9;
      for (int i = 0; i < 900; i++) if (h.step(dt, 0.2999f, true)) { const float tt = i * dt; if (first < 0) first = tt; if (prev >= 0) { maxGap = fmaxf(maxGap, tt - prev); minGap = fminf(minGap, tt - prev); } prev = tt; n++; }
      ok &= n == 10 && first == 0 && fabsf(maxGap - 1.0f) < dt + 1e-4f && fabsf(minGap - 1.0f) < dt + 1e-4f;
      ok &= !h.step(dt, 0.5f, true) && !h.beating && h.pulse() == 0; ok &= h.step(dt, 0.2f, true) && !h.step(dt, 0.2f, false); }
    ok &= fabsf(beatEnvelope(0.025f) - 1.0f) < 1e-3f && fabsf(beatEnvelope(0.175f) - 0.7f) < 0.02f && beatEnvelope(0.6f) < 0.02f && beatEnvelope(-0.1f) == 0;
    ok &= heartbeatFelt(100000, 90000) && !heartbeatFelt(111000, 90000) && !heartbeatFelt(100000, 0) && heartbeatFelt(100000, 70000, 60.0f);
    { Glance g; const V3 eye = v3(0, 0, 0), fwd = v3(0, 0, 1), p = v3(0.05f, -0.05f, 0.4f), n = v3(0, 0, -1);
      ok &= !g.step(p, n, eye, fwd, 0.10f, 1.0f, 0); ok &= g.step(p, n, eye, fwd, 0.05f, 1.0f, 0);   // facing you: shown after 0.12 s
      const V3 away = v3(0, -0.34f, 0.4f); ok &= g.step(away, n, eye, fwd, 0.2f, 1.0f, 0); ok &= !g.step(away, n, eye, fwd, 0.1f, 1.0f, 0);   // looked away: hidden after 0.25 s
      Glance h; bool any = false; for (int i = 0; i < 10; i++) { any |= h.step(p, v3(0, 0, 1), eye, fwd, 0.1f, 1.0f, 0); any |= h.step(v3(0, 0, 0.9f), n, eye, fwd, 0.1f, 1.0f, 0); } ok &= !any;   // facing away / too far: never
      Glance k; ok &= k.step(away, v3(0, 0, 1), eye, fwd, 0.01f, 0.2f, 0);   // low health: always
      Glance m; ok &= m.step(away, v3(0, 0, 1), eye, fwd, 0.01f, 1.0f, CHit); bool still = true; for (int i = 0; i < 19; i++) still &= m.step(away, v3(0, 0, 1), eye, fwd, 0.1f, 1.0f, 0); ok &= still;
      ok &= !m.step(away, v3(0, 0, 1), eye, fwd, 0.2f, 1.0f, 0); }   // a hit shows it for 2 s, then it goes
    { HandAtHead hh; const V3 eye = v3(0, 0, 0), fwd = v3(0, 0, 1); ok &= !hh.step(v3(0.05f, 0, 0.15f), eye, fwd, 0.2f); ok &= hh.step(v3(0.05f, 0, 0.15f), eye, fwd, 0.15f);
      ok &= hh.step(v3(0.05f, 0, 0.27f), eye, fwd, 0.1f); ok &= !hh.step(v3(0.05f, 0, 0.35f), eye, fwd, 0.1f); ok &= !hh.step(v3(0.05f, 0, -0.15f), eye, fwd, 1.0f); }   // behind the head: never
    return ok;
}
} // namespace vit
