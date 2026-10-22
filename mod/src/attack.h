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
//  - Katana weight (design doc "Katana weight - how heavy the katana feels to swing (one hand)"): the blade's direction follows
//    the hand through a critically damped lag of weight * 60 ms one-handed, a third of that two-handed; 0 = rigid.
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
    int chargeMs = 450; float chargeMoveCm = 10.0f; int graceMs = 250; float chargeTrigger = 0.6f;   // heavy charge
    float lungeCm = 35.0f, lungeHeavyCm = 60.0f, lungeMod = 0.5f, fullSpeed = 8.0f; int lungeCooldownMs = 600; float standoffCm = 70.0f;   // lunge
    float weight = 0.0f;                                                   // katana weight (0 = rigid)
};
enum : uint32_t { E_SWING = 1, E_CHARGE_START = 2, E_CHARGED = 4, E_CHARGE_LOST = 8 };
static const int HN = 12;
struct Intent {
    // tip history (body-relative cm) for the swing window
    V3 tp[HN]; uint64_t tms[HN]; int n = 0; bool armed = true; float lastSpeed = 0;
    // charge
    int charge = 0; uint64_t chargeAt = 0, releasedAt = 0; V3 chargeHand[2];   // 0 none, 1 charging, 2 charged
    uint64_t lastLunge = 0; uint32_t swings = 0, heavies = 0, restarts = 0;
    // one frame. tip: the blade tip, hands[0..1]: left / right grip, all body-relative cm; twoHands: both on the hilt; trigger 0..1
    uint32_t step(uint64_t now, V3 tip, const V3 *hands, bool twoHands, float trigger, const Cfg &c) {
        uint32_t ev = 0;
        if (n > 0 && (now <= tms[n - 1] || now - tms[n - 1] > 100)) n = 0;   // a hitch: start again
        if (n == HN) { for (int i = 1; i < HN; i++) { tp[i - 1] = tp[i]; tms[i - 1] = tms[i]; } n--; }
        tp[n] = tip; tms[n] = now; n++;
        int o = n - 1; while (o > 0 && now - tms[o - 1] <= 150) o--;
        float speed = 0, travel = 0;
        if (o < n - 1) { for (int i = o + 1; i < n; i++) travel += len(sub(tp[i], tp[i - 1])); const float dt = (now - tms[o]) * 0.001f; speed = dt > 0 ? len(sub(tip, tp[o])) * 0.01f / dt : 0; }
        lastSpeed = speed;
        if (armed && speed >= c.minSpeed && travel >= c.minTravelCm) { armed = false; swings++; ev |= E_SWING; }
        else if (!armed && speed < c.rearmSpeed) armed = true;
        // heavy charge
        const bool want = twoHands && trigger >= c.chargeTrigger;
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
    bool heavyReady() const { return charge == 2; }
    void spend() { if (charge == 2) { charge = 0; releasedAt = 0; heavies++; } }
    // the lunge for a blow at closing speed `speed` toward an enemy `dist` cm away (horizontal): the distance to travel (0 = none)
    float lunge(uint64_t now, float speed, float dist, bool heavy, const Cfg &c) {
        if (lastLunge && now - lastLunge < (uint64_t)c.lungeCooldownMs) return 0.0f;
        const float u = fminf(1.0f, fmaxf(0.0f, (speed - c.minSpeed) / fmaxf(0.1f, c.fullSpeed - c.minSpeed)));
        float d = (heavy ? c.lungeHeavyCm : c.lungeCm) * (1.0f + c.lungeMod * u);
        d = fminf(d, fmaxf(0.0f, dist - c.standoffCm)); if (d < 2.0f) return 0.0f;
        lastLunge = now; return d;
    }
};
// katana weight: the blade direction eased toward the hand's (unit vectors), critically damped; returns the new direction
struct Weight {
    V3 dir, vel; bool have = false;
    V3 step(V3 target, float dt, bool twoHands, const Cfg &c) {
        const float tau = c.weight * 0.060f * (twoHands ? 0.33f : 1.0f);
        if (!have || tau <= 1e-4f || dt <= 0 || dt > 0.1f) { dir = target; vel = v3(0, 0, 0); have = true; return dir; }
        const float w = 1.0f / tau; const V3 e = sub(dir, target);   // x'' = -w^2 e - 2 w x'
        vel = add(vel, mul(add(mul(e, -w * w), mul(vel, -2.0f * w)), dt)); dir = add(dir, mul(vel, dt));
        const float l = len(dir); dir = l > 1e-6f ? mul(dir, 1.0f / l) : target; return dir;
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
    Intent O; for (int i = 0; i < 40; i++) { t += 18; O.step(t, v3(0, 0, 0), h, false, 1.0f, c); } ok &= !O.heavyReady() && O.charge == 0;
    Intent M; for (int i = 0; i < 20; i++) { t += 18; V3 hm[2] = {h[0], h[1]}; if (i >= 15) hm[1].x += 15; M.step(t, v3(0, 0, 0), hm, true, 1.0f, c); } ok &= M.charge == 1 && M.restarts == 1;
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
    Weight W0; const V3 a = v3(1, 0, 0), b = v3(0, 1, 0); W0.step(a, 0.011f, false, c); V3 r0 = W0.step(b, 0.011f, false, c); ok &= fabsf(r0.y - 1.0f) < 1e-5f;
    Cfg cw = c; cw.weight = 1.0f; Weight W1, W2; W1.step(a, 0.011f, false, cw); W2.step(a, 0.011f, true, cw);
    V3 r1 = W1.step(b, 0.011f, false, cw), r2 = W2.step(b, 0.011f, true, cw); ok &= r1.y < 0.3f && r2.y > r1.y;
    for (int i = 0; i < 100; i++) { r1 = W1.step(b, 0.011f, false, cw); } ok &= r1.y > 0.999f;
    return ok;
}
} // namespace atk
