// GoTVR 0.18.26 the physical katana draw (owner: "the left hand grips near the top of the sheath, the right hand grips the hilt;
// they unsheathe the blade along the hilt - the sheath acts as rails - until near the top, where the blade is fully in the player's
// control; the grip has controller feedback, and a slight rumble while the blade comes out").
// 0.18.29 (owner: "the player shouldn't have to draw it so far - until the arm is mostly extended"; "sheathe the sword if the tip is
// inserted into the sheath, where you guide it along rails back in"):
//  - the draw has a GAIN: the blade comes out drawCm of hand travel for drawnFrac of its length (40 cm of pull for 66 of 72 cm), the
//    way a real draw also pulls the sheath back with the left hand (saya-biki) - the blade travels further than the right hand alone
//  - S_SHEATHING: a drawn blade whose tip is brought to the sheath mouth, lined up with the sheath, goes back onto the rail and slides
//    home as you push (the same gain); pulling back out frees it again; letting go mid-way slides it home
//  - letting go mid-draw slides the blade home (no longer instant)
// Pure state machine (self-tested), fed every frame with the two grip points, the sheath's rail (mouth + outward axis) and, while
// drawn, the blade's tip and direction in world units. References (playbook ch02 / pattern catalog):
//  - HAND-013 / VRIK slots: a holster is a pose, a hand and an accept-list; cross-body by default (the katana on the LEFT hip is drawn by
//    the RIGHT hand); discoverability by a haptic tick on hover.
//  - "Hysteresis on the grab threshold" (VRIK x1.75 distance, RE4VR grip 0.333 / 0.363): entry and exit always differ.
//  - HAND-017: one hand owns (the right hand on the hilt); the support hand (left on the sheath) never takes the blade.
//  - God of War VR (MIT): the Blades / axe are equipped by a reach-and-grab at the shoulder, with a pulse on the catch.
#pragma once
#include <cmath>
namespace sword {
enum State { S_SHEATHED = 0, S_DRAWING = 1, S_DRAWN = 2, S_SHEATHING = 3 };
static const char *kStateName[4] = {"in the sheath", "being drawn", "drawn (in your hand)", "going back into the sheath"};
struct Cfg {
    float grabCm = 12.0f, hiltGrabCm = 10.0f, leaveMul = 1.75f;   // grab radius around the sheath mouth / along the hilt, leave at x1.75
    float gripOn = 0.5f, gripOff = 0.3f;        // grip hysteresis
    float hiltCm = 25.0f, bladeCm = 72.0f;     // the hilt sticks out of the mouth hiltCm; the blade is bladeCm long
    float drawnFrac = 0.92f;                    // past this much of the blade the katana is free in the hand
    float drawCm = 40.0f;                       // 0.18.29: hand travel along the rail that frees the blade (the gain = drawnFrac * bladeCm / drawCm)
    float homeCmS = 220.0f;                     // 0.18.29: a let-go blade slides home at this speed
    float sheathTipCm = 8.0f, sheathDeg = 35.0f;   // 0.18.29: the tip within this of the mouth's line, the blade within this angle of the sheath
    float sheathOutCm = 8.0f, sheathInCm = 4.0f;   //          and the tip between sheathOutCm outside and sheathInCm inside the mouth
    float unsheathCm = 6.0f;                    // 0.18.29: pulling the hand this far back out of a full-length sheathing frees the blade again
    int needSheathHand = 0;                     // 1 = the blade only DRAWS while the left hand holds the sheath (0.18.30 default 0, owner: "draw with just the right hand on the grip")
    float freeMinFrac = 0.6f, freeExt = 0.93f;   // 0.18.31 (owner: "a tiny bit longer"): 0.5 / 0.9 -> 0.6 / 0.93   // 0.18.30 (owner: "until the arm is mostly extended"): past half the blade, an arm at 90 % of its reach frees it
    // 0.18.38 (owner: "the ini settings for grip placement don't seem to do anything - the right hand is still too far up the hilt no matter
    // what I change"): the rail's origin (the katana joint) is NOT the guard - the guard sits guardCm further out along the rail (from your
    // captured left grip: about 16 cm). The hilt you can grab is guardCm .. guardCm + hiltCm, the right fist sits rightCm below the guard
    // wherever you took the hilt, the left fist on the sheath at leftAx along the rail. (0.18.32's gripLowerCm, relative to where your
    // controller took the hilt and clamped to the wrong segment, is gone.)
    float guardCm = 0.0f, rightCm = 6.0f, leftAx = 0.0f;
    // 0.18.40 (owner: "I want sheathing to be player driven, not an animation - once the sword tip reaches the sheath mouth (around where
    // the left hand sits), the player must slide it back in"; playbook ch02 guided insertion: a line, a depth along it, the hand riding it):
    //  - mouthAx: the real sheath mouth along the rail (the guard's place less sword_mouth_below_guard_cm); the tip is taken there, with a
    //    window that reaches sheathInWinCm INTO the sheath (the blade's true length is not known exactly - the window absorbs it)
    //  - the blade may sit up to sheathOverCm further out than "fully out" while it is being guided in (no jump at the catch)
    //  - releaseHome = 0: letting go of a drawn katana keeps it in Jin's hand (it only goes away by sheathing it); 1 = 0.18.26's return
    //  - autoHome = 0: letting go mid-draw / mid-sheathe leaves the blade where it is in the sheath (take the hilt again to go on); 1 = it slides home
    // 0.18.41 tipLenCm: from the rail's origin as carried by the drawn katana to the REAL tip (sword_blade_out_cm - mouthAx): the 0.18.40
    // log had the tip 45 cm too deep with bladeCm (72) - the blade clears the mouth after ~45 cm of rail, which is where your draws free it
    float mouthAx = 0.0f, sheathInWinCm = 25.0f, sheathOverCm = 40.0f, tipLenCm = 72.0f; int releaseHome = 0, autoHome = 0;
    float handHalfCm = 4.5f;                    // 0.18.30: the fist's half width - its centre stays this far from the guard / the pommel
    float rumbleFullCmS = 80.0f, rumbleMax = 0.35f;
    float gain() const { return drawCm > 1.0f ? drawnFrac * bladeCm / drawCm : 1.0f; }
};
struct In { float hand[2][3]; float grip[2]; bool valid[2]; float mouth[3], axis[3]; float dt, unitsPerCm; float armExt = 0.0f;   // armExt: the right arm's reach used (0..1+), from the job's IK
            bool bladeOk; float tip[3], dir[3]; };   // dir: the blade's unit direction tip -> pommel (= the rail axis when it is in the sheath)
struct Out { int pulseHand[2]; float pulseAmp[2]; int pulseMs[2]; float rumble; bool event; const char *what; };
struct Sys {
    int state = S_SHEATHED; bool onSheath = false, onHilt = false, hoverSheath = false, hoverHilt = false, homing = false;
    float s = 0.0f, speed = 0.0f, dSheath = 0.0f, dHilt = 0.0f;   // s: how far the blade is out (cm)
    float a0 = 0.0f, s0 = 0.0f, hiltT = 0.0f;   // 0.18.29: hand axial position and s at the grab / sheathing start; where along the hilt the hand holds (cm from the guard)
    float tipAx = 0.0f, tipPerp = 0.0f, tipDeg = 0.0f; bool tipNear = false;   // the drawn blade's tip against the mouth (logged)
    static float dist(const float *a, const float *b) { const float d[3] = {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; return sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]); }
    static float rightT(const Cfg &c) { const float lo = c.guardCm + c.handHalfCm, hi = c.guardCm + c.hiltCm - c.handHalfCm, v = c.guardCm + c.rightCm; return v < lo ? lo : v > hi ? (hi > lo ? hi : lo) : v; }
    void reset() { state = S_SHEATHED; onSheath = onHilt = hoverSheath = hoverHilt = homing = tipNear = false; s = speed = a0 = s0 = hiltT = 0.0f; }
    void step(const In &in, const Cfg &c, Out &o) {
        o.pulseHand[0] = o.pulseHand[1] = 0; o.pulseAmp[0] = o.pulseAmp[1] = 0; o.pulseMs[0] = o.pulseMs[1] = 0; o.rumble = 0; o.event = false; o.what = "";
        const float u = in.unitsPerCm > 0 ? in.unitsPerCm : 1.0f, g = c.gain();
        auto pulse = [&](int h, float a, int ms) { o.pulseHand[h] = 1; if (a > o.pulseAmp[h]) { o.pulseAmp[h] = a; o.pulseMs[h] = ms; } };
        auto ev = [&](const char *w) { o.event = true; o.what = w; };
        auto axial = [&](const float *p) { const float d[3] = {p[0] - in.mouth[0], p[1] - in.mouth[1], p[2] - in.mouth[2]}; return (d[0] * in.axis[0] + d[1] * in.axis[1] + d[2] * in.axis[2]) / u; };
        { float q[3]; for (int k = 0; k < 3; k++) q[k] = in.mouth[k] + in.axis[k] * c.leftAx * u; dSheath = in.valid[0] ? dist(in.hand[0], q) / u : 1e9f; }
        float hAx = in.valid[1] ? axial(in.hand[1]) : 0.0f;
        {   // 0.18.28: the hilt is a segment (mouth .. pommel, riding out with the blade): any point along it can be grabbed
            dHilt = 1e9f; if (in.valid[1]) { float t = hAx - s; t = t < c.guardCm ? c.guardCm : t > c.guardCm + c.hiltCm ? c.guardCm + c.hiltCm : t;
                float q[3]; for (int k = 0; k < 3; k++) q[k] = in.mouth[k] + in.axis[k] * (t + s) * u; dHilt = dist(in.hand[1], q) / u; }
        }
        // ---- the left hand on the sheath ----
        const bool hs = dSheath < c.grabCm; if (hs && !hoverSheath && !onSheath) pulse(0, 0.12f, 10); hoverSheath = hs;
        if (!onSheath && in.valid[0] && in.grip[0] > c.gripOn && dSheath < c.grabCm) { onSheath = true; pulse(0, 0.45f, 25); ev("left hand took the sheath"); }
        else if (onSheath && (!in.valid[0] || in.grip[0] < c.gripOff || dSheath > c.grabCm * c.leaveMul)) { onSheath = false; ev("left hand let go of the sheath"); }
        const float s1 = s;
        // ---- the right hand on the hilt (sheathed / being drawn) ----
        if (state == S_SHEATHED || state == S_DRAWING) {
            const bool hh = dHilt < c.hiltGrabCm; if (hh && !hoverHilt && !onHilt) pulse(1, 0.12f, 10); hoverHilt = hh;
            if (!onHilt && in.valid[1] && in.grip[1] > c.gripOn && dHilt < c.hiltGrabCm) {
                onHilt = true; homing = false; a0 = hAx; s0 = s; hiltT = rightT(c);   // 0.18.38: always rightCm below the guard (the fist clear of the guard and the pommel)
                pulse(1, 0.6f, 30); ev("right hand took the hilt");
            } else if (onHilt && (!in.valid[1] || in.grip[1] < c.gripOff)) { onHilt = false; homing = c.autoHome && s > 0.0f; ev(s > 1.0f ? "right hand let go - the blade slides home" : "right hand let go of the hilt"); pulse(1, 0.3f, 20); }
            const bool canSlide = onHilt && (onSheath || !c.needSheathHand);
            if (canSlide) { const float t = s0 + g * (hAx - a0), top = s0 > c.bladeCm ? fminf(s0, c.bladeCm + c.sheathOverCm) : c.bladeCm; s = t < 0.0f ? 0.0f : t > top ? top : t; }
            else if (onHilt && s > 0.0f) { a0 = hAx; s0 = s; }   // the left hand let go mid-draw: the blade stays, the pull starts again from here
            if (!onHilt && homing) { s = fmaxf(0.0f, s - c.homeCmS * in.dt); if (s <= 0.0f) homing = false; }
            if (s > 1.5f && state == S_SHEATHED) { state = S_DRAWING; ev("the blade is coming out"); }
            if (s <= 0.5f && state == S_DRAWING) { state = S_SHEATHED;   /* 0.18.31: 1.5 / 0.5 cm hysteresis (the log flickered out / in at 1 cm) */ s = onHilt ? s : 0.0f; homing = false; ev("the blade is back in the sheath"); pulse(1, 0.5f, 25); pulse(0, 0.35f, 25); }
            if (state == S_DRAWING && onHilt && (s >= c.drawnFrac * c.bladeCm || (s >= c.freeMinFrac * c.bladeCm && in.armExt >= c.freeExt))) { state = S_DRAWN; tipNear = false; pulse(1, 0.9f, 60); pulse(0, 0.5f, 40); ev("DRAWN - the katana is free in your hand"); }
        } else if (state == S_DRAWN) {
            if (c.releaseHome && (!in.valid[1] || in.grip[1] < c.gripOff)) {   // sword_release_home = 1: letting go of a drawn katana puts it back
                state = S_SHEATHED; onHilt = false; homing = false; s = 0.0f; pulse(1, 0.4f, 25); ev("right hand let go of the drawn katana - back in the sheath");
            } else if (in.bladeOk) {   // the tip at the mouth, lined up -> back onto the rail (it stays in Jin's hand otherwise, grip or not)
                // 0.18.42 the catch at the LEFT HAND while it holds the sheath (where you feel the mouth), within 10 cm of the setting's place
                float mAx = c.mouthAx; if (onSheath && in.valid[0]) { const float la = axial(in.hand[0]); mAx = la < c.mouthAx - 10.0f ? c.mouthAx - 10.0f : la > c.mouthAx + 10.0f ? c.mouthAx + 10.0f : la; }
                const float ta0 = axial(in.tip), ta = ta0 - mAx; float pp[3]; for (int k = 0; k < 3; k++) pp[k] = in.tip[k] - (in.mouth[k] + in.axis[k] * ta0 * u);
                tipAx = ta; tipPerp = sqrtf(pp[0] * pp[0] + pp[1] * pp[1] + pp[2] * pp[2]) / u;
                float cd = in.dir[0] * in.axis[0] + in.dir[1] * in.axis[1] + in.dir[2] * in.axis[2]; cd = cd > 1 ? 1 : cd < -1 ? -1 : cd; tipDeg = acosf(cd) * 57.2957795f;
                const bool closeTip = tipPerp < c.sheathTipCm * 1.5f && ta < c.sheathOutCm * 2.0f && ta > -c.sheathInWinCm - 5.0f && tipDeg < c.sheathDeg * 1.5f;
                if (closeTip && !tipNear) pulse(1, 0.15f, 12); tipNear = closeTip;   // a tick: the tip is close to the mouth
                if (tipPerp < c.sheathTipCm && ta < c.sheathOutCm && ta > -c.sheathInWinCm && tipDeg < c.sheathDeg) {
                    state = S_SHEATHING; s = fminf(c.bladeCm + c.sheathOverCm, fmaxf(0.0f, c.tipLenCm + ta0)); s0 = s; a0 = hAx; homing = false; onHilt = true;   // where the blade IS (no jump)
                    pulse(1, 0.55f, 30); pulse(0, 0.3f, 20); ev("the tip is in the sheath mouth - guide it home");
                }
            }
        } else if (state == S_SHEATHING) {
            if (!in.valid[1] || (c.releaseHome && in.grip[1] < c.gripOff)) { onHilt = false; homing = c.autoHome != 0;   /* 0.18.40: the katana stays in the hand (sword_release_home 0) until it is home, grip or not */ ev(c.autoHome ? "right hand let go - the blade slides home" : "right hand let go - the blade stays where it is in the sheath"); pulse(1, 0.3f, 20); a0 = hAx; s0 = s; state = S_DRAWING; }
            else {
                const float t = s0 + g * (hAx - a0);
                if (hAx - a0 > c.unsheathCm) { state = S_DRAWN; tipNear = false; pulse(1, 0.5f, 25); ev("pulled back out - the katana is free again"); }
                else { const float top = fminf(s0, c.bladeCm + c.sheathOverCm); s = t < 0.0f ? 0.0f : t > top ? top : t;
                    if (s <= 1.0f) { state = S_SHEATHED; s = 0.0f; a0 = hAx; s0 = 0.0f; pulse(1, 0.8f, 40); pulse(0, 0.6f, 40); ev("SHEATHED - the blade is home"); } }
            }
        }
        speed = in.dt > 0.0f ? fabsf(s - s1) / in.dt : 0.0f;
        if ((state == S_DRAWING || state == S_SHEATHING) && speed > 0.5f) { float r = speed / c.rumbleFullCmS; r = r > 1 ? 1 : r; o.rumble = 0.06f + (c.rumbleMax - 0.06f) * r; }
    }
};
} // namespace sword
