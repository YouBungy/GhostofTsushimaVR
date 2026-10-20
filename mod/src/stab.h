// GoTVR 0.18.24 hand stabilisation (owner: "implement micro hand tremor stabilization, the hands jitter; FEAR VR does it well").
// FEAR VR's "fine-motion controller stability" is closed source (its repo is documentation only), so this is our own, built on the
// playbook's rules (ch01 "Latency, prediction, and smoothing": kill sub-mm / sub-degree jitter without adding lag, defaults low, all
// tunable; ch02 "A smoothing filter needs an enumerated list of things that must reset it").
// The filter is a One Euro filter (Casiez, Roussel, Vogel, CHI 2012) on the controller grip pose in the runtime's tracking space:
// the cut-off frequency rises with the hand's speed, so a hand held still is smoothed hard (tremor, tracking noise) and a moving hand
// is barely touched (no swim on a swing). Position per axis with a shared speed; rotation by slerp with the same rule on angular
// speed. Every discontinuity resets it to the raw pose (FEAR VR / playbook list): first pose, tracking lost or regained, recenter /
// reference-space change, a jump larger than a hand can move in a frame, a bad time step, non-finite values.
#pragma once
#include <cmath>
#include <cstring>
namespace stab {
enum Reset { R_NONE = 0, R_FIRST, R_LOST, R_RECENTER, R_JUMP, R_DT, R_NONFINITE, R_N };
static const char *kResetName[R_N] = {"none", "first pose", "tracking lost", "recenter", "jump", "time step", "non-finite"};
// 0.18.26 (owner: "improve hand smoothing"): rotation matters most (Batmaz / Stuerzlinger, SFU: pointing already suffers at +-0.25 deg of
// rotational jitter; a sword multiplies it with its length), so the rotation filter starts lower (0.9 Hz) and both get a SOFT deadband
// (playbook ch01: "a micro-deadzone on orientation / position kills visible jitter from sub-degree / sub-mm noise without adding lag"):
// the output only follows the part of a movement beyond deadMm / deadDeg from where it is, so tremor inside it is gone and any real
// move passes on at once, continuously (no step).
struct Cfg { float minHz = 1.2f, beta = 10.0f, rotMinHz = 0.9f, rotBeta = 3.0f, dHz = 1.0f, deadMm = 1.5f, deadDeg = 0.25f; };   // beta: Hz per m/s; rotBeta: Hz per rad/s
struct Filt {
    bool init = false; float p[3] = {0, 0, 0}, v = 0, q[4] = {0, 0, 0, 1}, w = 0, rawP[3] = {0, 0, 0}, rawQ[4] = {0, 0, 0, 1};
    float hp[3] = {0, 0, 0}, hq[4] = {0, 0, 0, 1};   // 0.18.26: the deadband's held output
};
static inline float alphaOf(float hz, float dt) { const float tau = 1.0f / (6.2831853f * hz); return 1.0f / (1.0f + tau / dt); }
static inline void qnorm(float *q) { const float n = sqrtf(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]); if (n > 1e-8f) for (int i = 0; i < 4; i++) q[i] /= n; }
static inline float qangle(const float *a, const float *b) {   // the rotation angle between two unit quaternions
    float d = fabsf(a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3]); d = d > 1.0f ? 1.0f : d; return 2.0f * acosf(d); }
static inline void qslerp(const float *a, const float *b0, float t, float *o) {
    float b[4] = {b0[0], b0[1], b0[2], b0[3]}; float d = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
    if (d < 0) { d = -d; for (int i = 0; i < 4; i++) b[i] = -b[i]; }
    if (d > 0.9995f) { for (int i = 0; i < 4; i++) o[i] = a[i] + (b[i] - a[i]) * t; qnorm(o); return; }
    const float th = acosf(d), s = sinf(th), ka = sinf((1 - t) * th) / s, kb = sinf(t * th) / s;
    for (int i = 0; i < 4; i++) o[i] = a[i] * ka + b[i] * kb; qnorm(o); }
// Filter one pose in place (q xyzw, p metres). valid = the runtime tracks it. resetReq = an outside reason (recenter). Returns the reset
// reason (R_NONE when it filtered).
static inline int step(Filt &F, float *q, float *p, bool valid, float dt, const Cfg &c, int resetReq = R_NONE) {
    if (!valid) { F.init = false; return R_LOST; }
    bool fin = true; for (int i = 0; i < 4; i++) fin &= std::isfinite(q[i]); for (int i = 0; i < 3; i++) fin &= std::isfinite(p[i]);
    int why = R_NONE;
    if (!fin) why = R_NONFINITE; else if (!F.init) why = R_FIRST; else if (resetReq) why = resetReq; else if (!(dt > 0.0005f) || dt > 0.1f) why = R_DT;
    else { const float d[3] = {p[0] - F.rawP[0], p[1] - F.rawP[1], p[2] - F.rawP[2]}; if (sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]) > 0.25f) why = R_JUMP; }   // > 25 cm in one frame
    if (why != R_NONE) {
        if (why == R_NONFINITE) { F.init = false; return why; }
        memcpy(F.p, p, 12); memcpy(F.rawP, p, 12); memcpy(F.q, q, 16); qnorm(F.q); memcpy(F.rawQ, F.q, 16); memcpy(F.hp, p, 12); memcpy(F.hq, F.q, 16); F.v = 0; F.w = 0; F.init = true; return why;
    }
    // position: speed of the raw signal (smoothed with dHz), cut-off = minHz + beta * speed
    const float d[3] = {p[0] - F.rawP[0], p[1] - F.rawP[1], p[2] - F.rawP[2]}; const float sp = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]) / dt;
    F.v += (sp - F.v) * alphaOf(c.dHz, dt);
    const float ap = alphaOf(c.minHz + c.beta * F.v, dt);
    memcpy(F.rawP, p, 12); for (int i = 0; i < 3; i++) F.p[i] += (p[i] - F.p[i]) * ap;
    {   // soft deadband on the filtered position
        const float e[3] = {F.p[0] - F.hp[0], F.p[1] - F.hp[1], F.p[2] - F.hp[2]}; const float l = sqrtf(e[0] * e[0] + e[1] * e[1] + e[2] * e[2]), r = c.deadMm * 0.001f;
        if (l > r && l > 1e-9f) { const float k = (l - r) / l; for (int i = 0; i < 3; i++) F.hp[i] += e[i] * k; }
        for (int i = 0; i < 3; i++) p[i] = F.hp[i];
    }
    // rotation: angular speed of the raw signal, the same rule, slerp toward the raw orientation
    float qr[4] = {q[0], q[1], q[2], q[3]}; qnorm(qr);
    const float om = qangle(F.rawQ, qr) / dt; F.w += (om - F.w) * alphaOf(c.dHz, dt);
    const float aq = alphaOf(c.rotMinHz + c.rotBeta * F.w, dt);
    memcpy(F.rawQ, qr, 16); float o[4]; qslerp(F.q, qr, aq, o); memcpy(F.q, o, 16);
    {   // soft deadband on the filtered orientation
        const float ang = qangle(F.hq, F.q), r = c.deadDeg / 57.2957795f;
        if (ang > r && ang > 1e-7f) { float h[4]; qslerp(F.hq, F.q, (ang - r) / ang, h); memcpy(F.hq, h, 16); }
        memcpy(q, F.hq, 16);
    }
    return R_NONE;
}
} // namespace stab
