// GoTVR 0.18.26 the physical katana draw, stage 1 (owner: "the left hand grips near the top of the sheath, the right hand grips the hilt;
// they unsheathe the blade along the hilt - the sheath acts as rails - until near the top, where the blade is fully in the player's
// control; the grip has controller feedback, and a slight rumble while the blade comes out").
// Pure state machine (self-tested), fed every frame with the two grip points and the sheath's rail (mouth + outward axis) in world
// units. References (playbook ch02 / pattern catalog):
//  - HAND-013 / VRIK slots: a holster is a pose, a hand and an accept-list; cross-body by default (the katana on the LEFT hip is drawn by
//    the RIGHT hand); discoverability by a haptic tick on hover.
//  - "Hysteresis on the grab threshold" (VRIK x1.75 distance, RE4VR grip 0.333 / 0.363): entry and exit always differ.
//  - HAND-017: one hand owns (the right hand on the hilt); the support hand (left on the sheath) never takes the blade.
//  - God of War VR (MIT): the Blades / axe are equipped by a reach-and-grab at the shoulder, with a pulse on the catch.
#pragma once
#include <cmath>
namespace sword {
enum State { S_SHEATHED = 0, S_DRAWING = 1, S_DRAWN = 2 };
static const char *kStateName[3] = {"in the sheath", "being drawn", "drawn (in your hand)"};
struct Cfg {
    float grabCm = 12.0f, leaveMul = 1.75f;    // grab radius around the sheath mouth / the hilt, leave at x1.75
    float gripOn = 0.5f, gripOff = 0.3f;        // grip hysteresis
    float hiltCm = 25.0f, bladeCm = 72.0f;     // the hilt sticks out of the mouth hiltCm; the blade is bladeCm long
    float drawnFrac = 0.92f;                    // past this much of the blade the katana is free in the hand
    int needSheathHand = 1;                     // 1 = the blade only slides while the left hand holds the sheath
    float rumbleFullCmS = 80.0f, rumbleMax = 0.35f;
};
struct In { float hand[2][3]; float grip[2]; bool valid[2]; float mouth[3], axis[3]; float dt, unitsPerCm; };
struct Out { int pulseHand[2]; float pulseAmp[2]; int pulseMs[2]; float rumble; bool event; const char *what; };
struct Sys {
    int state = S_SHEATHED; bool onSheath = false, onHilt = false, hoverSheath = false, hoverHilt = false;
    float s = 0.0f, grab0 = 0.0f, speed = 0.0f, dSheath = 0.0f, dHilt = 0.0f;   // s: how far the blade is out (cm)
    static float dist(const float *a, const float *b) { const float d[3] = {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; return sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]); }
    void reset() { state = S_SHEATHED; onSheath = onHilt = hoverSheath = hoverHilt = false; s = grab0 = speed = 0.0f; }
    void step(const In &in, const Cfg &c, Out &o) {
        o.pulseHand[0] = o.pulseHand[1] = 0; o.pulseAmp[0] = o.pulseAmp[1] = 0; o.pulseMs[0] = o.pulseMs[1] = 0; o.rumble = 0; o.event = false; o.what = "";
        const float u = in.unitsPerCm > 0 ? in.unitsPerCm : 1.0f;
        auto pulse = [&](int h, float a, int ms) { o.pulseHand[h] = 1; if (a > o.pulseAmp[h]) { o.pulseAmp[h] = a; o.pulseMs[h] = ms; } };
        auto ev = [&](const char *w) { o.event = true; o.what = w; };
        // where the hilt is now (it rides out with the blade)
        float hilt[3]; for (int k = 0; k < 3; k++) hilt[k] = in.mouth[k] + in.axis[k] * (c.hiltCm * 0.5f + s) * u;
        dSheath = in.valid[0] ? dist(in.hand[0], in.mouth) / u : 1e9f; dHilt = in.valid[1] ? dist(in.hand[1], hilt) / u : 1e9f;
        // ---- the left hand on the sheath ----
        const bool hs = dSheath < c.grabCm; if (hs && !hoverSheath && !onSheath) pulse(0, 0.12f, 10); hoverSheath = hs;
        if (!onSheath && in.valid[0] && in.grip[0] > c.gripOn && dSheath < c.grabCm) { onSheath = true; pulse(0, 0.45f, 25); ev("left hand took the sheath"); }
        else if (onSheath && (!in.valid[0] || in.grip[0] < c.gripOff || dSheath > c.grabCm * c.leaveMul)) { onSheath = false; ev("left hand let go of the sheath"); }
        // ---- the right hand on the hilt ----
        if (state != S_DRAWN) {
            const bool hh = dHilt < c.grabCm; if (hh && !hoverHilt && !onHilt) pulse(1, 0.12f, 10); hoverHilt = hh;
            if (!onHilt && in.valid[1] && in.grip[1] > c.gripOn && dHilt < c.grabCm) {
                onHilt = true; const float d[3] = {in.hand[1][0] - in.mouth[0], in.hand[1][1] - in.mouth[1], in.hand[1][2] - in.mouth[2]};
                grab0 = (d[0] * in.axis[0] + d[1] * in.axis[1] + d[2] * in.axis[2]) / u - c.hiltCm * 0.5f - s;   // no jump at the grab
                pulse(1, 0.6f, 30); ev("right hand took the hilt");
            } else if (onHilt && (!in.valid[1] || in.grip[1] < c.gripOff)) { onHilt = false; ev(s > 1.0f ? "right hand let go - the blade slides home" : "right hand let go of the hilt"); pulse(1, 0.3f, 20); }
        }
        // ---- the rail ----
        const float s0 = s;
        if (state != S_DRAWN) {
            const bool canSlide = onHilt && (onSheath || !c.needSheathHand);
            if (canSlide) {
                const float d[3] = {in.hand[1][0] - in.mouth[0], in.hand[1][1] - in.mouth[1], in.hand[1][2] - in.mouth[2]};
                float t = (d[0] * in.axis[0] + d[1] * in.axis[1] + d[2] * in.axis[2]) / u - c.hiltCm * 0.5f - grab0;
                s = t < 0.0f ? 0.0f : t > c.bladeCm ? c.bladeCm : t;
            } else if (!onHilt && s > 0.0f) { s = 0.0f; }   // let go: home
            if (s > 1.0f && state == S_SHEATHED) { state = S_DRAWING; ev("the blade is coming out"); }
            if (s <= 1.0f && state == S_DRAWING) { state = S_SHEATHED; ev("the blade is back in the sheath"); pulse(1, 0.5f, 25); pulse(0, 0.35f, 25); }
            if (s >= c.drawnFrac * c.bladeCm) { state = S_DRAWN; pulse(1, 0.9f, 60); pulse(0, 0.5f, 40); ev("DRAWN - the katana is free in your hand"); }
        } else if (!in.valid[1] || in.grip[1] < c.gripOff) {   // stage 1: letting go of a drawn katana puts it back (the real sheathe comes next)
            state = S_SHEATHED; onHilt = false; s = 0.0f; pulse(1, 0.4f, 25); ev("right hand let go of the drawn katana - back in the sheath (stage 1)");
        }
        speed = in.dt > 0.0f ? fabsf(s - s0) / in.dt : 0.0f;
        if (state == S_DRAWING && speed > 0.5f) { float r = speed / c.rumbleFullCmS; r = r > 1 ? 1 : r; o.rumble = 0.06f + (c.rumbleMax - 0.06f) * r; }
    }
};
} // namespace sword
