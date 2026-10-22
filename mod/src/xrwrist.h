// GoTVRHost v16: the wrist HUD, stage W1 (docs/WRIST_HUD.md). The game's own health / resolve art, cropped from the UI layer the game
// already sends us, shown on a small quad on the OUTSIDE of your left forearm (the kunai goes on the inside) - placed from the left
// controller's grip pose every frame. The same rectangle is cut out of the room HUD panel so it is never shown twice.
//  - shown when you look at it (vit::Glance: facing you, within 0.65 m, near the middle of the view; 0.12 s to show, 0.25 s to hide),
//    forced on at low health and for 2 s after a hit / heal / resolve change (the game sends those, v16)
//  - left hand held up at the head + its trigger: the whole HUD panel (objectives too), uncut (design doc [UI], control scheme)
//  - no tracked left hand: no wrist panel, nothing cut (the room panel shows the full HUD as before)
// A compositor quad is drawn over the world (no occlusion); the world-space version (drawn into the game's frame against its depth,
// WRIST_HUD.md W1b) replaces the quad later. Settings in gotvr.ini [gotvr] wrist_hud_*.
#pragma once
#include "vitals.h"
namespace wrist {
static Chain sc; static bool on = false, cut = true, glanceOn = true; static int panelAtHead = 0, mode = 1, lockTop = 1, fromAim = 1;   // mode: 1 the game's own HUD art, 2 our gauge (always there), 3 both
static float rect[4] = {0.015f, 0.865f, 0.235f, 0.125f};   // x, y, w, h as fractions of the UI image (measured on the 16:9 flat HUD)
static float widthM = 0.10f, alongM = 0.11f, outM = 0.045f, rollDeg = 0.0f, upperM = 0.30f, lowerM = 0.29f, wristM = 0.10f; static u64 nBone = 0;
static vit::GlanceCfg gcfg; static vit::Glance glance; static vit::HandAtHead atHead;
static bool shownNow = false, cutNow = false, headNow = false; static XrPosef pose{}; static int px[4] = {0, 0, 0, 0};
static u32 lastChg = 0; static u64 nShown = 0, nFrames = 0; static LARGE_INTEGER qf{}, ql{};
static void load(const wchar_t *dir) {
    wchar_t ini[MAX_PATH]; swprintf(ini, MAX_PATH, L"%ls\\gotvr.ini", dir);
    auto f = [&](const wchar_t *k, float d) { wchar_t v[64]; GetPrivateProfileStringW(L"gotvr", k, L"", v, 64, ini); return v[0] ? (float)_wtof(v) : d; };
    auto def = [&](const wchar_t *k, const wchar_t *v) { wchar_t t[8]; if (!GetPrivateProfileStringW(L"gotvr", k, L"", t, 8, ini)) WritePrivateProfileStringW(L"gotvr", k, v, ini); };
    def(L"wrist_hud", L"1"); def(L"wrist_hud_rect", L"0.015,0.865,0.235,0.125"); def(L"wrist_hud_width_m", L"0.10"); def(L"wrist_hud_from_hand_m", L"0.15");
    def(L"wrist_hud_out_m", L"0.05"); def(L"wrist_hud_forearm", L"body"); def(L"wrist_hud_roll_deg", L"0"); def(L"wrist_hud_glance", L"1"); def(L"wrist_hud_cut", L"1"); def(L"wrist_hud_panel_at_head_only", L"0"); def(L"wrist_hud_low_hp", L"0.30"); def(L"wrist_hud_mode", L"1"); def(L"wrist_hud_lock", L"top");
    {   // 0.18.61 (owner: "get rid of your version and just have the game's"): an ini written before keeps mode 3 - moved to 1 once
        wchar_t t[8]; if (!GetPrivateProfileStringW(L"gotvr", L"wrist_hud_v", L"", t, 8, ini)) { WritePrivateProfileStringW(L"gotvr", L"wrist_hud_mode", L"1", ini); WritePrivateProfileStringW(L"gotvr", L"wrist_hud_v", L"2", ini); } }
    {   // 0.15.4 (owner, session 19: "the HUD was not on the forearm but to the left of the wrist - I want it overlaid on top of the forearm"):
        // the forearm is now the controller's POINTING line (aim pose), further up the arm and a little higher - moved once
        if (GetPrivateProfileIntW(L"gotvr", L"wrist_hud_v", 0, ini) < 3) { WritePrivateProfileStringW(L"gotvr", L"wrist_hud_from_hand_m", L"0.15", ini); WritePrivateProfileStringW(L"gotvr", L"wrist_hud_out_m", L"0.05", ini); WritePrivateProfileStringW(L"gotvr", L"wrist_hud_v", L"3", ini); } }
    on = f(L"wrist_hud", 1) != 0; cut = f(L"wrist_hud_cut", 1) != 0; glanceOn = f(L"wrist_hud_glance", 1) != 0; panelAtHead = f(L"wrist_hud_panel_at_head_only", 0) != 0 ? 1 : 0; mode = (int)f(L"wrist_hud_mode", 1); if (mode < 1 || mode > 3) mode = 1;
    { wchar_t lk[16]; GetPrivateProfileStringW(L"gotvr", L"wrist_hud_lock", L"top", lk, 16, ini); lockTop = _wcsicmp(lk, L"hand") != 0; }
    gcfg.faceDeg = f(L"wrist_hud_face_deg", 70.0f); gcfg.viewDeg = f(L"wrist_hud_view_deg", 40.0f); gcfg.maxM = f(L"wrist_hud_max_m", 0.80f);   // 0.18.62 (owner: "the forearm HUD never appeared") wider than 50 / 35 / 0.65
    widthM = f(L"wrist_hud_width_m", 0.10f); alongM = f(L"wrist_hud_from_hand_m", 0.15f); outM = f(L"wrist_hud_out_m", 0.05f);
    {   // 0.15.5 (owner, session 20: "the wrist HUD kept rotating with my wrist - I want the rotation locked going along the forearm"):
        // "body" (default) = the forearm from an estimated ELBOW (shoulder from the headset, two-bone arm) to the hand - positions only, so
        // bending or turning the wrist never moves it; "aim" = the controller's pointing line (0.15.4); "grip" = the grip's axis (0.15.3)
        if (GetPrivateProfileIntW(L"gotvr", L"wrist_hud_v", 0, ini) < 4) { WritePrivateProfileStringW(L"gotvr", L"wrist_hud_forearm", L"body", ini); WritePrivateProfileStringW(L"gotvr", L"wrist_hud_v", L"4", ini); }
        wchar_t fa[16]; GetPrivateProfileStringW(L"gotvr", L"wrist_hud_forearm", L"body", fa, 16, ini); fromAim = _wcsicmp(fa, L"grip") == 0 ? 0 : _wcsicmp(fa, L"aim") == 0 ? 1 : 2;
        upperM = f(L"wrist_hud_upper_arm_m", 0.30f); lowerM = f(L"wrist_hud_forearm_m", 0.29f);
        def(L"wrist_hud_from_wrist_m", L"0.10"); wristM = f(L"wrist_hud_from_wrist_m", 0.10f);
        if (GetPrivateProfileIntW(L"gotvr", L"wrist_hud_v", 0, ini) < 5) { WritePrivateProfileStringW(L"gotvr", L"wrist_hud_out_m", L"0.045", ini); WritePrivateProfileStringW(L"gotvr", L"wrist_hud_v", L"5", ini); } outM = f(L"wrist_hud_out_m", 0.045f); } rollDeg = f(L"wrist_hud_roll_deg", 0.0f); gcfg.low = f(L"wrist_hud_low_hp", 0.30f);
    wchar_t v[96]; GetPrivateProfileStringW(L"gotvr", L"wrist_hud_rect", L"", v, 96, ini); float r[4]; if (swscanf(v, L"%f,%f,%f,%f", &r[0], &r[1], &r[2], &r[3]) == 4 && r[2] > 0.01f && r[3] > 0.01f) for (int i = 0; i < 4; i++) rect[i] = r[i];
    logf("wrist HUD: forearm line from %s", fromAim == 2 ? "an estimated elbow (headset + two-bone arm) - the wrist's own turns never move it" : fromAim == 1 ? "the controller's pointing direction" : "the grip axis");
    logf("wrist HUD: placed %s", lockTop ? "on TOP of the forearm, never rolling with the wrist (wrist_hud_lock = top)" : "on the back of the hand, turning with it (wrist_hud_lock = hand)");
    logf("wrist HUD: mode %d (%s)", mode, mode == 1 ? "the game's own health / resolve art" : mode == 2 ? "our gauge from the game's values, always there" : "our gauge + the game's own art above it");
    logf("wrist HUD: %s (rect %.3f,%.3f %.3fx%.3f of the UI image, %.2f m wide, %.2f m up the forearm from the grip, %.3f m out, glance %d, cut from the room panel %d, room panel only with the hand at the head %d)",
         on ? "ON" : "off (wrist_hud = 0)", rect[0], rect[1], rect[2], rect[3], widthM, alongM, outM, (int)glanceOn, (int)cut, panelAtHead);
}
static XrQuaternionf qmul(XrQuaternionf a, XrQuaternionf b) { return {a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y, a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x, a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w, a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z}; }
static void rot(XrQuaternionf q, const float *v, float *o) {   // q * v
    const float tx = 2 * (q.y * v[2] - q.z * v[1]), ty = 2 * (q.z * v[0] - q.x * v[2]), tz = 2 * (q.x * v[1] - q.y * v[0]);
    o[0] = v[0] + q.w * tx + (q.y * tz - q.z * ty); o[1] = v[1] + q.w * ty + (q.z * tx - q.x * tz); o[2] = v[2] + q.w * tz + (q.x * ty - q.y * tx); }
// once per frame, after the controllers are located: where the panel is, whether it shows, whether the room panel is cut
static void decide(const XrPosef &head, bool headOk, u32 uiW, u32 uiH) {
    shownNow = cutNow = false; nFrames++;
    if (!qf.QuadPart) QueryPerformanceFrequency(&qf); LARGE_INTEGER qn; QueryPerformanceCounter(&qn); const float dt = ql.QuadPart ? fminf(0.1f, (float)(qn.QuadPart - ql.QuadPart) / (float)qf.QuadPart) : 0.0f; ql = qn;
    if (!on || !headOk || !uiW || !uiH) return;
    GvHand hd{}; u32 act = 0; gvRead(&S->inSeq, [&] { hd = S->hand[0]; act = S->inActive; });
    if (!act || !(hd.poseValid & 1)) return;   // no tracked left hand: the room panel keeps the whole HUD
    float hp = 1.0f; u32 chg = 0, cseq = 0, vok = 0; gvRead(&S->vitSeq, [&] { hp = S->vitHpMax > 0 ? S->vitHp / S->vitHpMax : 1.0f; cseq = S->vitChangedSeq; chg = S->vitChanged; vok = S->vitOk; });
    const u32 bits = cseq != lastChg ? chg : 0; lastChg = cseq; if (!vok) hp = 1.0f;
    // the panel: grip frame axes - +X out of the back of the left hand, -Z forward through the fist, +Y the thumb side. On the back of
    // the forearm (+X), alongM behind the grip toward the elbow (+Z); its picture's right = toward the hand (-Z), up = +Y, face = +X
    const XrQuaternionf qg = {hd.grip.q[0], hd.grip.q[1], hd.grip.q[2], hd.grip.q[3]};
    const float rr = rollDeg / 57.29578f; const XrQuaternionf qy = {0, 0.70710678f, 0, 0.70710678f}, qz = {0, 0, sinf(rr * 0.5f), cosf(rr * 0.5f)};
    XrQuaternionf q = qmul(qmul(qg, qy), qz);
    const float off[3] = {outM, 0.0f, alongM}; float ow[3]; rot(qg, off, ow);
    if (lockTop) {   // 0.18.61 (owner: "I want the HUD on the outside, like in this photo ... it should stay locked on top of the forearm, not
        // rotating with my wrist"): the forearm's line from the grip (+Z toward the elbow) keeps the panel on the arm, but its face is the
        // world's UP taken square to that line - the top of the forearm whatever the wrist does. A forearm held near vertical faces you.
        // 0.15.4: the forearm's line is the controller's POINTING direction (aim pose, toward the elbow = its +Z) - the grip's Z runs down
        // the handle through the fist, across the forearm (session 19: the HUD sat beside the wrist)
        const float zA[3] = {0, 0, 1}; float A[3]; rot(qg, zA, A);
        // 0.15.6 (owner, session 21: "not flush along the forearm ... nor glued onto the forearm bone"): Jin's own drawn forearm from the game
        // DLL (shared v18, relative to the grip) - the panel sits ON that bone, wristM up from his wrist joint, outM above its centre line
        float boneW[3] = {0, 0, 0}; bool bone = false;
        { float ed[3], wd[3]; u32 aok = 0; gvRead(&S->armSeq, [&] { memcpy(ed, S->armElbowD, 12); memcpy(wd, S->armWristD, 12); aok = S->armOk; });
          if (aok && fromAim == 2) { float E2[3]; for (int k = 0; k < 3; k++) { E2[k] = ed[k] - wd[k]; boneW[k] = hd.grip.p[k] + wd[k]; } const float el = sqrtf(E2[0] * E2[0] + E2[1] * E2[1] + E2[2] * E2[2]);
              if (el > 0.1f) { for (int k = 0; k < 3; k++) A[k] = E2[k] / el; bone = true; nBone++; } } }
        if (bone) {}
        else if (fromAim == 1 && (hd.poseValid & 2)) { const XrQuaternionf qa = {hd.aim.q[0], hd.aim.q[1], hd.aim.q[2], hd.aim.q[3]}; rot(qa, zA, A); }
        else if (fromAim == 2) {   // the left arm from the headset: shoulder 24 cm below / 18 cm left / 6 cm behind the eyes (head yaw only)
            const float fz[3] = {0, 0, -1}; float hf[3]; rot(head.orientation, fz, hf); float fl = sqrtf(hf[0] * hf[0] + hf[2] * hf[2]);
            if (fl > 1e-3f) { const float F3[3] = {hf[0] / fl, 0, hf[2] / fl}, R3[3] = {-F3[2], 0, F3[0]};   // right = forward x up (y up, -z forward)
                const float Sh[3] = {head.position.x - R3[0] * 0.18f - F3[0] * 0.06f, head.position.y - 0.24f, head.position.z - R3[2] * 0.18f - F3[2] * 0.06f};
                const float G[3] = {hd.grip.p[0], hd.grip.p[1], hd.grip.p[2]}; float D[3] = {G[0] - Sh[0], G[1] - Sh[1], G[2] - Sh[2]}; const float d = sqrtf(D[0] * D[0] + D[1] * D[1] + D[2] * D[2]);
                if (d > 0.05f) { for (auto &x : D) x /= d; float El[3];
                    if (d >= upperM + lowerM - 1e-3f) for (int k = 0; k < 3; k++) El[k] = Sh[k] + D[k] * upperM * d / (upperM + lowerM);   // straight arm
                    else { const float a = (upperM * upperM - lowerM * lowerM + d * d) / (2.0f * d), h = sqrtf(fmaxf(0.0f, upperM * upperM - a * a));
                        float P[3] = {-R3[0] * 0.5f - F3[0] * 0.3f, -1.0f, -R3[2] * 0.5f - F3[2] * 0.3f};   // the elbow hangs down, out to the left, a little back
                        const float pd = P[0] * D[0] + P[1] * D[1] + P[2] * D[2]; for (int k = 0; k < 3; k++) P[k] -= pd * D[k]; const float pl = sqrtf(P[0] * P[0] + P[1] * P[1] + P[2] * P[2]);
                        for (int k = 0; k < 3; k++) El[k] = Sh[k] + D[k] * a + (pl > 1e-4f ? P[k] / pl * h : 0.0f); }
                    float E2[3] = {El[0] - G[0], El[1] - G[1], El[2] - G[2]}; const float el = sqrtf(E2[0] * E2[0] + E2[1] * E2[1] + E2[2] * E2[2]);
                    if (el > 1e-3f) { static float Af[3] = {0, 0, 0}; const float bk = 0.35f;   // a light filter on the (already wrist-free) line
                        for (int k = 0; k < 3; k++) Af[k] = Af[k] * (1.0f - bk) + E2[k] / el * bk; const float al = sqrtf(Af[0] * Af[0] + Af[1] * Af[1] + Af[2] * Af[2]);
                        if (al > 1e-3f) for (int k = 0; k < 3; k++) A[k] = Af[k] / al; } } } }
        float up[3] = {0, 1, 0}; float d = up[0] * A[0] + up[1] * A[1] + up[2] * A[2];
        if (fabsf(d) > 0.92f) { const float hp3[3] = {head.position.x - hd.grip.p[0], head.position.y - hd.grip.p[1], head.position.z - hd.grip.p[2]}; for (int k = 0; k < 3; k++) up[k] = hp3[k]; d = up[0] * A[0] + up[1] * A[1] + up[2] * A[2]; }
        float N[3] = {up[0] - A[0] * d, up[1] - A[1] * d, up[2] - A[2] * d}; const float nl = sqrtf(N[0] * N[0] + N[1] * N[1] + N[2] * N[2]);
        if (nl > 1e-4f) { for (auto &x : N) x /= nl;
            const float X[3] = {-A[0], -A[1], -A[2]};   // the picture's right = toward the hand
            const float Y[3] = {N[1] * X[2] - N[2] * X[1], N[2] * X[0] - N[0] * X[2], N[0] * X[1] - N[1] * X[0]};   // up = face x right
            // rotation matrix columns X, Y, N -> quaternion
            const float m00 = X[0], m11 = Y[1], m22 = N[2], tr = m00 + m11 + m22; XrQuaternionf r;
            if (tr > 0) { const float s4 = sqrtf(tr + 1.0f) * 2; r.w = 0.25f * s4; r.x = (Y[2] - N[1]) / s4; r.y = (N[0] - X[2]) / s4; r.z = (X[1] - Y[0]) / s4; }
            else if (m00 > m11 && m00 > m22) { const float s4 = sqrtf(1.0f + m00 - m11 - m22) * 2; r.w = (Y[2] - N[1]) / s4; r.x = 0.25f * s4; r.y = (Y[0] + X[1]) / s4; r.z = (N[0] + X[2]) / s4; }
            else if (m11 > m22) { const float s4 = sqrtf(1.0f + m11 - m00 - m22) * 2; r.w = (N[0] - X[2]) / s4; r.x = (Y[0] + X[1]) / s4; r.y = 0.25f * s4; r.z = (N[1] + Y[2]) / s4; }
            else { const float s4 = sqrtf(1.0f + m22 - m00 - m11) * 2; r.w = (X[1] - Y[0]) / s4; r.x = (N[0] + X[2]) / s4; r.y = (N[1] + Y[2]) / s4; r.z = 0.25f * s4; }
            q = qmul(r, qz); for (int k = 0; k < 3; k++) ow[k] = bone ? boneW[k] - hd.grip.p[k] + A[k] * wristM + N[k] * outM : N[k] * outM + A[k] * alongM; } }
    pose.orientation = q; pose.position = {hd.grip.p[0] + ow[0], hd.grip.p[1] + ow[1], hd.grip.p[2] + ow[2]};
    const float nz[3] = {0, 0, 1}; float n[3]; rot(q, nz, n);
    const float fz[3] = {0, 0, -1}; float fw[3]; const XrQuaternionf qh = head.orientation; rot(qh, fz, fw);
    const vit::V3 P = vit::v3(pose.position.x, pose.position.y, pose.position.z), N = vit::v3(n[0], n[1], n[2]), E = vit::v3(head.position.x, head.position.y, head.position.z), F = vit::v3(fw[0], fw[1], fw[2]);
    // control scheme (owner, 2026-10-21): "Hold left hand up to head and press trigger - HUD/Quest objectives" - the whole HUD, uncut,
    // from the trigger press at the head until the hand leaves the head
    static bool latched = false; const bool atH = atHead.step(vit::v3(hd.grip.p[0], hd.grip.p[1], hd.grip.p[2]), E, F, dt, 0.22f, 0.30f, 0.0f);
    if (!atH) latched = false; else if (hd.trigger > 0.6f) latched = true;
    headNow = atH && latched;
    const bool look = glance.step(P, N, E, F, dt, hp, bits, gcfg);
    shownNow = !headNow && (!glanceOn || look);
    cutNow = cut && !headNow;
    px[0] = (int)(rect[0] * uiW); px[1] = (int)(rect[1] * uiH); px[2] = (int)(rect[2] * uiW); px[3] = (int)(rect[3] * uiH);
    if (px[0] < 0) px[0] = 0; if (px[1] < 0) px[1] = 0; if (px[0] + px[2] > (int)uiW) px[2] = (int)uiW - px[0]; if (px[1] + px[3] > (int)uiH) px[3] = (int)uiH - px[1];
    if (px[2] < 4 || px[3] < 4) { shownNow = cutNow = false; return; }
    if (shownNow) nShown++;
    { static u64 lastLog = 0, n0 = 0, s0 = 0; const u64 tl = GetTickCount64(); if (tl - lastLog > 10000) { if (lastLog) logf("wrist HUD: on Jin's forearm bone %llu frames; shown %llu of %llu frames in the last 10 s (glance: face %.0f deg, view %.0f deg, %.2f m) - note the game hides its own health art out of combat; wrist_hud_mode 3 adds our gauge",
          (unsigned long long)nBone, (unsigned long long)(nShown - s0), (unsigned long long)(nFrames - n0), gcfg.faceDeg, gcfg.viewDeg, gcfg.maxM); lastLog = tl; n0 = nFrames; s0 = nShown; } }
    S->wristHostOn = shownNow ? 1 : 0;
}
} // namespace wrist
