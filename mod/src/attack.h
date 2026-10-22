// GoTVR 0.18.48 the attack intent around the blade contact (melee.h): the design doc's [COMBAT DESIGN] attack parameters.
// Pure, self-tested, no game access. Units: cm, ms, m/s. All positions BODY-relative (the hand / tip minus Jin's root), so walking,
// turning the stick or a lunge is never a swing (playbook INPUT-010).
//  - Swing (design doc "Minimum Speed", "Minimum Travel Distance"; "Rumble feedback light attack / heavy attack"): the blade tip's
//    speed over the last 150 ms past minSpeed with at least minTravelCm of tip travel -> one swing event (a light tick in the hand),
//    re-armed when the tip slows under rearmSpeed (playbook / FEAR: re-arm on low speed, not on a timer).
//  - Heavy charge (design doc: "Heavy attacks are triggered when the player holds right trigger down while 'charging' their attack
//    (with both hands on the katana)", "Minimum heavy attack charging time", "Maximum hand movement during heavy attack charge"):
//    two hands + right trigger held; the hands must stay within chargeMoveCm of where the charge began (else it starts over) for
//    chargeMs -> CHARGED; the charge is kept while the trigger stays held (the swing itself may move), lost graceMs after release,
//    spent by a blow.
//  - Lunge (design doc: "the player's body lunges forward a bit", "Player travel distance", "... Heavy", "Travel Distance Modifier",
//    "Player travel distance cooldown"): on a blow, toward the enemy struck, base * (1 + modifier * speed fraction), never within
//    the cooldown, never past the enemy (stops standoffCm short of it).
//  - Katana weight (design doc "Katana weight - how heavy the katana feels to swing (one hand)"; 0.18.58 after Blade & Sorcery and
//    the playbook ch02 "simulated weapon weight is a follow filter"): the blade's direction is pulled to the hand's by a rotational
//    spring-damper (B&S: a spring that pulls the weapon back to the hand and a damper that gives it inertia, the weapon builds speed
//    slowly and is hard to stop) - natural period weight * 45 ms, damping 0.85 (a hint of follow-through); gravity bends the held
//    direction down a little when the blade is level (sagDeg * weight, one-handed); a second hand on the hilt is a second joint:
//    twoHandFactor of the lag and a fifth of the sag; the lag angle is compressed asymptotically past lagSoftDeg toward lagMaxDeg
//    (playbook: a hard clamp in a tracked path reads as a wall). 0 = rigid. Resets are the caller's (an enumerated list, gotvr.cpp).
#pragma once
#include <cmath>
#include <cstdint>
namespace atk {
struct V3 { float x = 0, y = 0, z = 0; };
static inline V3 v3(float x, float y, float z) { V3 r; r.x = x; r.y = y; r.z = z; return r; }
static inline V3 sub(V3 a, V3 b) { return v3(a.x - b.x, a.y - b.y, a.z - b.z); }
static inline V3 add(V3 a, V3 b) { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
static inline V3 mul(V3 a, float s) { return v3(a.x * s, a.y * s, a.z * s); }
static inline float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline float len(V3 a) { return sqrtf(dot(a, a)); }
struct Cfg {
    float minSpeed = 2.2f, minTravelCm = 18.0f, rearmSpeed = 0.8f;        // swing (shared with melee::Cfg's minSpeed / minTravelCm)
    int chargeMs = 450; float chargeMoveCm = 25.0f; int graceMs = 250; float chargeTrigger = 0.6f;   // heavy charge
    bool heavyTwoHands = false; int heavySwingMs = 300;   // 0.18.62 (owner: "hold trigger to charge the attack, then just swing for a heavy attack (trigger can still be held)")
    float lungeCm = 35.0f, lungeHeavyCm = 60.0f, lungeMod = 0.5f, fullSpeed = 8.0f; int lungeCooldownMs = 600; float standoffCm = 70.0f;   // lunge
    float weight = 0.0f;                                                   // katana weight (0 = rigid)
    float sagDeg = 3.0f, twoHandFactor = 0.33f, lagSoftDeg = 16.0f, lagMaxDeg = 28.0f, damping = 0.85f;   // 0.18.58 weight model
};
enum : uint32_t { E_SWING = 1, E_CHARGE_START = 2, E_CHARGED = 4, E_CHARGE_LOST = 8 };
static const int HN = 12;
struct Intent {
    // tip history (body-relative cm) for the swing window
    V3 tp[HN]; uint64_t tms[HN]; int n = 0; bool armed = true; float lastSpeed = 0;
    // charge
    int charge = 0; uint64_t chargeAt = 0, releasedAt = 0; V3 chargeHand[2];   // 0 none, 1 charging, 2 charged
    uint64_t lastLunge = 0, heavyUntil = 0, lastNow = 0; uint32_t swings = 0, heavies = 0, restarts = 0;
    // one frame. tip: the blade tip, hands[0..1]: left / right grip, all body-relative cm; twoHands: both on the hilt; trigger 0..1
    uint32_t step(uint64_t now, V3 tip, const V3 *hands, bool twoHands, float trigger, const Cfg &c) {
        uint32_t ev = 0; lastNow = now;
        if (n > 0 && (now <= tms[n - 1] || now - tms[n - 1] > 100)) n = 0;   // a hitch: start again
        if (n == HN) { for (int i = 1; i < HN; i++) { tp[i - 1] = tp[i]; tms[i - 1] = tms[i]; } n--; }
        tp[n] = tip; tms[n] = now; n++;
        int o = n - 1; while (o > 0 && now - tms[o - 1] <= 150) o--;
        float speed = 0, travel = 0;
        if (o < n - 1) { for (int i = o + 1; i < n; i++) travel += len(sub(tp[i], tp[i - 1])); const float dt = (now - tms[o]) * 0.001f; speed = dt > 0 ? len(sub(tip, tp[o])) * 0.01f / dt : 0; }
        lastSpeed = speed;
        if (armed && speed >= c.minSpeed && travel >= c.minTravelCm) { armed = false; swings++; ev |= E_SWING;
            if (charge == 2) { charge = 0; releasedAt = 0; heavies++; heavyUntil = now + (uint64_t)c.heavySwingMs; } }   // 0.18.62 the charged swing IS the heavy attack (its blows within heavySwingMs)
        else if (!armed && speed < c.rearmSpeed) armed = true;
        // heavy charge
        const bool want = (twoHands || !c.heavyTwoHands) && trigger >= c.chargeTrigger && now >= heavyUntil;
        if (want) {
            releasedAt = 0;
            if (charge == 0) { charge = 1; chargeAt = now; chargeHand[0] = hands[0]; chargeHand[1] = hands[1]; ev |= E_CHARGE_START; }
            else if (charge == 1) {
                const float m = fmaxf(len(sub(hands[0], chargeHand[0])), len(sub(hands[1], chargeHand[1])));
                if (m > c.chargeMoveCm) { chargeAt = now; chargeHand[0] = hands[0]; chargeHand[1] = hands[1]; restarts++; }   // moved: begin again
                else if (now - chargeAt >= (uint64_t)c.chargeMs) { charge = 2; ev |= E_CHARGED; }
            }
        } else if (charge) {
            if (!releasedAt) releasedAt = now;
            if (charge == 1 || now - releasedAt >= (uint64_t)c.graceMs) { charge = 0; releasedAt = 0; ev |= E_CHARGE_LOST; }
        }
        return ev;
    }
    bool heavyReady() const { return charge == 2 || lastNow < heavyUntil; }
    void spend() { if (charge == 2) { charge = 0; releasedAt = 0; heavies++; } heavyUntil = 0; }
    // the lunge for a blow at closing speed `speed` toward an enemy `dist` cm away (horizontal): the distance to travel (0 = none)
    float lunge(uint64_t now, float speed, float dist, bool heavy, const Cfg &c) {
        if (lastLunge && now - lastLunge < (uint64_t)c.lungeCooldownMs) return 0.0f;
        const float u = fminf(1.0f, fmaxf(0.0f, (speed - c.minSpeed) / fmaxf(0.1f, c.fullSpeed - c.minSpeed)));
        float d = (heavy ? c.lungeHeavyCm : c.lungeCm) * (1.0f + c.lungeMod * u);
        d = fminf(d, fmaxf(0.0f, dist - c.standoffCm)); if (d < 2.0f) return 0.0f;
        lastLunge = now; return d;
    }
};
// katana weight: the blade direction pulled toward the hand's (unit vectors) by a rotational spring-damper; twoW = 0 one hand .. 1 two
static inline V3 nrm(V3 a, V3 fb) { const float l = len(a); return l > 1e-6f ? mul(a, 1.0f / l) : fb; }
struct Weight {
    V3 dir, vel; bool have = false; float lastLagDeg = 0, lastSagDeg = 0;
    V3 held(V3 target, float twoW, const Cfg &c) const {   // the hand's direction with gravity's sag (world z up)
        const float sag = c.sagDeg * c.weight * (1.0f - 0.8f * twoW) * 0.0174533f; if (sag <= 0.0f) return target;
        const float lvl = sqrtf(fmaxf(0.0f, 1.0f - target.z * target.z));   // 1 level, 0 pointing straight up / down
        return nrm(sub(target, v3(0, 0, tanf(sag) * lvl)), target); }
    V3 step(V3 target, float dt, float twoW, const Cfg &c) {
        twoW = twoW < 0 ? 0 : twoW > 1 ? 1 : twoW;
        const float period = c.weight * 0.045f * (1.0f + (c.twoHandFactor - 1.0f) * twoW);
        if (!std::isfinite(target.x + target.y + target.z)) return have ? dir : target;
        const V3 t = held(target, twoW, c);
        if (!have || period <= 1e-4f || dt <= 0 || dt > 0.1f) { dir = t; vel = v3(0, 0, 0); have = true; lastLagDeg = 0; return dir; }
        // 0.18.62 (owner: two hands on the hilt "causes the weapon to flicker left and right very fast"; log: tip speeds of 1600-1800 m/s):
        // the explicit step blew up - two-handed the period is 15 ms, a 67 fps frame is 15 ms (w dt = 1) and any hitch is far past stable.
        // Now the IMPLICIT step (stable for every dt): v' = (v - dt w^2 e) / (1 + 2 z w dt + w^2 dt^2), x' = x + dt v'
        const float w = 1.0f / period; const V3 e = sub(dir, t);
        vel = mul(sub(vel, mul(e, dt * w * w)), 1.0f / (1.0f + 2.0f * c.damping * w * dt + w * w * dt * dt)); dir = nrm(add(dir, mul(vel, dt)), t);
        vel = sub(vel, mul(dir, dot(vel, dir)));   // tangent only (a direction cannot grow)
        // soft limit on how far the blade may trail the hand: 1:1 to lagSoftDeg, then compressed toward lagMaxDeg, never a wall
        const float cs = fminf(1.0f, fmaxf(-1.0f, dot(dir, t))); const float ang = acosf(cs) * 57.29578f;
        if (ang > c.lagSoftDeg && c.lagMaxDeg > c.lagSoftDeg) {
            const float span = c.lagMaxDeg - c.lagSoftDeg, a2 = c.lagSoftDeg + span * (1.0f - expf(-(ang - c.lagSoftDeg) / span));
            const V3 perp = nrm(sub(dir, mul(t, cs)), dir); const float r = a2 * 0.0174533f; dir = nrm(add(mul(t, cosf(r)), mul(perp, sinf(r))), t);
            vel = mul(vel, a2 / ang); lastLagDeg = a2; } else lastLagDeg = ang;
        lastSagDeg = acosf(fminf(1.0f, fmaxf(-1.0f, dot(t, target)))) * 57.29578f;
        if (!std::isfinite(dir.x + dir.y + dir.z + vel.x + vel.y + vel.z)) { dir = t; vel = v3(0, 0, 0); }
        return dir;
    }
};
static inline bool selfTest() {
    bool ok = true; Cfg c; Intent I; V3 h[2] = {v3(0, 0, 100), v3(0, 10, 100)}; uint64_t t = 1000; int swings = 0;
    // a 4 m/s slash (7.2 cm per 18 ms) over 40 cm: one swing; held still: re-armed; another slash: a second swing
    for (int i = 0; i < 12; i++) { t += 18; swings += (I.step(t, v3(i * 7.2f, 50, 120), h, false, 0, c) & E_SWING) ? 1 : 0; }
    ok &= swings == 1;
    for (int i = 0; i < 12; i++) { t += 18; swings += (I.step(t, v3(86.4f, 50, 120), h, false, 0, c) & E_SWING) ? 1 : 0; } ok &= I.armed;
    for (int i = 0; i < 12; i++) { t += 18; swings += (I.step(t, v3(86.4f - i * 7.2f, 50, 120), h, false, 0, c) & E_SWING) ? 1 : 0; } ok &= swings == 2;
    // slow (1.5 m/s) or short (a 10 cm flick): no swing
    Intent S; int s2 = 0; for (int i = 0; i < 30; i++) { t += 18; s2 += (S.step(t, v3(i * 2.7f, 0, 0), h, false, 0, c) & E_SWING) ? 1 : 0; } ok &= s2 == 0;
    Intent F; int s3 = 0; for (int i = 0; i < 3; i++) { t += 18; s3 += (F.step(t, v3(i * 5.0f, 0, 0), h, false, 0, c) & E_SWING) ? 1 : 0; } for (int i = 0; i < 10; i++) { t += 18; s3 += (F.step(t, v3(10, 0, 0), h, false, 0, c) & E_SWING) ? 1 : 0; } ok &= s3 == 0;
    // heavy charge: two hands + trigger, held still 450 ms -> charged; one hand: never; moving 15 cm during the charge restarts it
    Intent H; uint32_t ev = 0; for (int i = 0; i < 30; i++) { t += 18; ev |= H.step(t, v3(0, 0, 0), h, true, 1.0f, c); } ok &= (ev & E_CHARGED) && H.heavyReady();
    { Cfg c2 = c; c2.heavyTwoHands = true; Intent O; for (int i = 0; i < 40; i++) { t += 18; O.step(t, v3(0, 0, 0), h, false, 1.0f, c2); } ok &= !O.heavyReady() && O.charge == 0; }
    { Intent O1; uint32_t e1 = 0; for (int i = 0; i < 30; i++) { t += 18; e1 |= O1.step(t, v3(0, 0, 0), h, false, 1.0f, c); } ok &= (e1 & E_CHARGED) && O1.heavyReady(); }   // 0.18.62 one hand charges too (default)
    Intent M; for (int i = 0; i < 20; i++) { t += 18; V3 hm[2] = {h[0], h[1]}; if (i >= 15) hm[1].x += 30; M.step(t, v3(0, 0, 0), hm, true, 1.0f, c); } ok &= M.charge == 1 && M.restarts == 1;
    // charged then swing (moving is fine once charged), trigger let go: kept for the grace, then lost; spent by a blow
    for (int i = 0; i < 5; i++) { t += 18; const V3 hm[2] = {v3(i * 7.f, 0, 100), v3(i * 7.f, 10, 100)}; H.step(t, v3(i * 7.2f, 0, 0), hm, true, 1.0f, c); } ok &= H.heavyReady();
    t += 18; H.step(t, v3(0, 0, 0), h, true, 0.0f, c); ok &= H.heavyReady(); t += 300; H.step(t, v3(0, 0, 0), h, true, 0.0f, c); ok &= !H.heavyReady();
    Intent P; for (int i = 0; i < 30; i++) { t += 18; P.step(t, v3(0, 0, 0), h, true, 1.0f, c); } P.spend(); ok &= !P.heavyReady() && P.heavies == 1;
    // lunge: light 35 cm at the minimum speed, more when faster, heavy 60, never inside the cooldown, never past the enemy
    Intent L; float d = L.lunge(10000, 2.2f, 500, false, c); ok &= fabsf(d - 35.0f) < 0.01f;
    ok &= L.lunge(10300, 8.0f, 500, false, c) == 0.0f;   // cooldown
    d = L.lunge(11000, 8.0f, 500, false, c); ok &= fabsf(d - 52.5f) < 0.01f;
    d = L.lunge(12000, 2.2f, 500, true, c); ok &= fabsf(d - 60.0f) < 0.01f;
    d = L.lunge(13000, 8.0f, 90, true, c); ok &= fabsf(d - 20.0f) < 0.01f;   // 90 cm away, stop 70 short
    ok &= L.lunge(14000, 8.0f, 60, false, c) == 0.0f;   // already close
    // weight: 0 = rigid; 1 = lags then settles; two hands settle faster
    Weight W0; const V3 a = v3(1, 0, 0), b = v3(0, 1, 0); W0.step(a, 0.011f, 0.0f, c); V3 r0 = W0.step(b, 0.011f, 0.0f, c); ok &= fabsf(r0.y - 1.0f) < 1e-5f;
    Cfg cw = c; cw.weight = 1.0f; cw.sagDeg = 0.0f; Weight W1, W2; W1.step(a, 0.011f, 0.0f, cw); W2.step(a, 0.011f, 1.0f, cw);
    V3 r1 = W1.step(b, 0.011f, 0.0f, cw), r2 = W2.step(b, 0.011f, 1.0f, cw); ok &= r1.y < 0.95f && r2.y > r1.y;   // a 90 deg snap trails (at most lagMaxDeg), two hands less
    for (int i = 0; i < 100; i++) { r1 = W1.step(b, 0.011f, 0.0f, cw); } ok &= r1.y > 0.999f;
    // the lag never exceeds lagMaxDeg (a 90 deg snap trails at most 28 deg), and sag: a level blade one-handed droops sagDeg * weight
    Weight W3; W3.step(a, 0.011f, 0.0f, cw); W3.step(b, 0.011f, 0.0f, cw); ok &= W3.lastLagDeg <= cw.lagMaxDeg + 0.01f && W3.lastLagDeg > cw.lagSoftDeg;
    Cfg cs = c; cs.weight = 1.0f; Weight W4; V3 r4 = a; for (int i = 0; i < 200; i++) r4 = W4.step(a, 0.011f, 0.0f, cs);
    ok &= fabsf(asinf(-r4.z) * 57.29578f - 3.0f) < 0.05f;
    Weight W5; V3 r5 = a; for (int i = 0; i < 200; i++) r5 = W5.step(a, 0.011f, 1.0f, cs); ok &= fabsf(asinf(-r5.z) * 57.29578f - 0.6f) < 0.05f;   // two hands: a fifth
    { Cfg cf = cw; cf.twoHandFactor = 0.33f; Weight Ws; V3 rs = a; bool fine = true; for (int i = 0; i < 400; i++) { const V3 tg = (i / 3) % 2 ? a : b; const float dts = (i % 7 == 0) ? 0.09f : 0.016f;   // hitches, two hands, a target flipping
          rs = Ws.step(tg, dts, 1.0f, cf); fine &= std::isfinite(rs.x + rs.y + rs.z) && len(Ws.vel) < 200.0f; } ok &= fine; }   // 0.18.62 stable at any frame time
    Weight W6; V3 up = v3(0, 0, 1), r6 = up; for (int i = 0; i < 50; i++) r6 = W6.step(up, 0.011f, 0.0f, cs); ok &= r6.z > 0.9999f;   // pointing up: no sag
    return ok;
}
} // namespace atk
