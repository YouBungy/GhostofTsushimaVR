// GoTVR 0.18.0 VR controllers - the game side (included by gotvr.cpp).
// Playbook ch03 / INPUT-001 / XR-002 / HAND-004: GoTVRHost transports one CANONICAL controller per hand (any headset's
// controllers, bound in xrinput.h); this file alone gives those controls a meaning in Ghost of Tsushima, through the game's
// own funnel: the mod IS xinput1_4.dll, so a virtual Xbox pad (user 0) carries every action and the game's own controls,
// prompts and remapping stay valid. Rumble comes back through XInputSetState. Everything the player can bind lives in
// [controls] of gotvr.ini as "control = output", outputs being pad buttons (a b x y lb rb l3 r3 start back up down left
// right lt rt rs_up rs_down rs_left rs_right ls_up ...), several joined by '+', or a VR action (wheel, hud, none).
// Context first: theatre / menus get plain pad semantics; gameplay adds snap turn, the item wheel and head gestures.
#pragma once

namespace vrin {
// ---------- outputs ----------
enum : u32 {   // XInput button bits
    XB_UP = 0x1, XB_DOWN = 0x2, XB_LEFT = 0x4, XB_RIGHT = 0x8, XB_START = 0x10, XB_BACK = 0x20, XB_L3 = 0x40, XB_R3 = 0x80,
    XB_LB = 0x100, XB_RB = 0x200, XB_A = 0x1000, XB_B = 0x2000, XB_X = 0x4000, XB_Y = 0x8000 };
enum : u32 { OX_LT = 1u << 16, OX_RT = 1u << 17, OX_RSU = 1u << 18, OX_RSD = 1u << 19, OX_RSL = 1u << 20, OX_RSR = 1u << 21,
             OX_LSU = 1u << 22, OX_LSD = 1u << 23, OX_LSL = 1u << 24, OX_LSR = 1u << 25, OX_WHEEL = 1u << 26, OX_HUD = 1u << 27, OX_RECENTER = 1u << 28, OX_ANIM = 1u << 29, OX_ANIMS = 1u << 30 };
struct Out { u32 m = 0; };
static Out parseOut(const char *s) {
    static const struct { const char *n; u32 m; } k[] = {
        {"a", XB_A}, {"b", XB_B}, {"x", XB_X}, {"y", XB_Y}, {"lb", XB_LB}, {"rb", XB_RB}, {"l3", XB_L3}, {"r3", XB_R3}, {"start", XB_START}, {"menu", XB_START},
        {"back", XB_BACK}, {"view", XB_BACK}, {"up", XB_UP}, {"down", XB_DOWN}, {"left", XB_LEFT}, {"right", XB_RIGHT},
        {"dpad_up", XB_UP}, {"dpad_down", XB_DOWN}, {"dpad_left", XB_LEFT}, {"dpad_right", XB_RIGHT}, {"lt", OX_LT}, {"rt", OX_RT},
        {"rs_up", OX_RSU}, {"rs_down", OX_RSD}, {"rs_left", OX_RSL}, {"rs_right", OX_RSR}, {"ls_up", OX_LSU}, {"ls_down", OX_LSD}, {"ls_left", OX_LSL}, {"ls_right", OX_LSR},
        {"wheel", OX_WHEEL}, {"hud", OX_HUD}, {"recenter", OX_RECENTER}, {"recentre", OX_RECENTER}, {"anim", OX_ANIM}, {"anim_short", OX_ANIMS}, {"none", 0}};
    Out o; char b[128]; strncpy(b, s ? s : "", sizeof b - 1); b[sizeof b - 1] = 0;
    for (char *t = strtok(b, "+ ,"); t; t = strtok(nullptr, "+ ,")) { for (char *c = t; *c; c++) *c = (char)tolower(*c); bool f = false; for (auto &e : k) if (!strcmp(t, e.n)) { o.m |= e.m; f = true; } if (!f) logf("controls: unknown output '%s' (ignored)", t); }
    return o;
}
// ---------- canonical controls ----------
enum Ctl { C_LSTICKCLICK, C_RSTICKCLICK, C_LPRIMARY, C_LSECONDARY, C_LMENU, C_RPRIMARY, C_RSECONDARY, C_RMENU, C_LTRIGGER, C_RTRIGGER, C_LGRIP, C_RGRIP,
           C_GEST_RHEAD, C_GEST_LHEAD, C_N };
static const struct { const char *key, *def, *what; } kCtl[C_N] = {
    {"left_stick_click", "l3", "run"}, {"right_stick_click", "r3", "crouch"},
    {"left_primary", "wheel", "item wheel (Quest X, hold; 0.18.24 owner's scheme)"}, {"left_secondary", "x+anim_short", "quick attack / assassinate (Quest Y)"}, {"left_menu", "start", "menu (the three-line button)"},
    {"right_primary", "a", "jump (Quest A)"}, {"right_secondary", "b", "dodge (Quest B)"}, {"right_menu", "none", "-"},
    {"left_trigger", "back", "Focus while nothing is held (0.18.61 owner; the touchpad press, View on an Xbox pad - held off while the katana is out)"}, {"right_trigger", "rt+up", "the game's own right trigger (R2: interact / pick up supplies) + standoff when the game offers it (D-pad up) (0.18.61 owner)"},
    {"left_grip", "none", "grab (hands: later)"}, {"right_grip", "none", "grab (0.18.10: unbound - the physical sword comes next)"},
    {"right_hand_at_head_trigger", "dpad_right+rs_up", "guiding wind"}, {"left_hand_at_head_trigger", "hud+dpad_right+rs_up", "HUD in front + the game's objective (touchpad swipe up)"}};
static const int NWHEEL = 8;   // 0.18.7: up to 8 options; wheel_count (default 5) of them are used
struct Cfg {
    Out out[C_N]; Out wheel[NWHEEL]; char wheelName[NWHEEL][32]; int nWheel = 5; Out wheelTap; int wheelHoldMs = 300;
    float dead = 0.2f, snapDeg = 30.0f, smoothGain = 1.0f, headM = 0.22f, rumble = 1.0f, fistGrip = 0.3f; int turnSmooth = 0, enabled = 1, debugOverlay = 1, fistOff = 1;
    int moveHead = 1;   // 0.18.22 (owner: "movement totally headset oriented at all times"): the left stick's forward = where the headset looks NOW
};
static Cfg cfg;
static void loadCfg(const wchar_t *ini) {
    auto str = [&](const char *key, const char *def, char *o, int n) { wchar_t wk[64], wd[128], wb[128]; MultiByteToWideChar(CP_UTF8, 0, key, -1, wk, 64); MultiByteToWideChar(CP_UTF8, 0, def, -1, wd, 128);
        GetPrivateProfileStringW(L"controls", wk, wd, wb, 128, ini); WideCharToMultiByte(CP_UTF8, 0, wb, -1, o, n, nullptr, nullptr); };
    auto flt = [&](const char *key, float def, float lo, float hi) { char b[32], d[32]; snprintf(d, 32, "%g", def); str(key, d, b, 32); float v = (float)atof(b); return (v >= lo && v <= hi) ? v : def; };
    char b[128];
    {   // 0.18.61 (owner): the triggers changed meaning once - RT = the game's own RT (+ standoff), LT = Focus. An ini written before gets them once.
        wchar_t mv[8]; if (!GetPrivateProfileStringW(L"controls", L"controls_v", L"", mv, 8, ini)) {
            WritePrivateProfileStringW(L"controls", L"right_trigger", L"rt+up", ini); WritePrivateProfileStringW(L"controls", L"left_trigger", L"back", ini);
            WritePrivateProfileStringW(L"controls", L"controls_v", L"2", ini); logf("controls: triggers updated once - right trigger = the game's R2 + standoff, left trigger = Focus"); } }
    for (int c = 0; c < C_N; c++) { str(kCtl[c].key, kCtl[c].def, b, 128); cfg.out[c] = parseOut(b); }
    // 0.18.61 (owner): the wheel button - a quick press heals (wheel_tap), held past wheel_hold_ms it opens the wheel
    str("wheel_tap", "down", b, 128); cfg.wheelTap = parseOut(b); cfg.wheelHoldMs = (int)flt("wheel_hold_ms", 300.0f, 100.0f, 1500.0f);
    // 0.18.7 (owner's CONTROL_SCHEME.txt): Heal, Photo mode, Call horse, Play flute (touchpad swipe LEFT = hold D-pad right + flick
    // the right stick left on PC), Recenter view (the mod's own F11)
    static const char *wd[NWHEEL] = {"down", "right", "left", "dpad_right+rs_left+anim", "recenter", "none", "none", "none"},
                      *wn[NWHEEL] = {"Heal", "Photo mode", "Call horse", "Play flute", "Recenter view", "", "", ""};
    str("wheel_count", "5", b, 128); cfg.nWheel = atoi(b); if (cfg.nWheel < 2 || cfg.nWheel > NWHEEL) cfg.nWheel = 5;
    for (int i = 0; i < NWHEEL; i++) { char k[32]; snprintf(k, 32, "wheel_%d", i + 1); str(k, wd[i], b, 128); cfg.wheel[i] = parseOut(b); snprintf(k, 32, "wheel_%d_name", i + 1); str(k, wn[i], cfg.wheelName[i], 32); }
    str("turn", "snap", b, 128); cfg.turnSmooth = !_stricmp(b, "smooth");
    cfg.snapDeg = flt("snap_turn_deg", 30.0f, 5.0f, 90.0f); cfg.smoothGain = flt("smooth_turn_speed", 1.0f, 0.1f, 1.0f); cfg.dead = flt("stick_deadzone", 0.2f, 0.0f, 0.6f);
    cfg.headM = flt("hand_at_head_m", 0.22f, 0.08f, 0.5f); cfg.rumble = flt("rumble_strength", 1.0f, 0.0f, 1.0f);
    str("enabled", "1", b, 128); cfg.enabled = atoi(b) != 0;
    // 0.18.74 (owner: "make it so left trigger and right trigger only have their respective, no item held button inputs when grip is not
    // held - that way the player can make a full fist without unwanted button inputs"): a trigger pulled while that hand's grip is closed
    // (past trigger_grip_level 0.3) sends nothing - not its button, not a head gesture, not the wheel - until the trigger is let go
    str("trigger_off_while_gripping", "1", b, 128); cfg.fistOff = atoi(b) != 0; cfg.fistGrip = flt("trigger_grip_level", 0.3f, 0.05f, 0.95f);
    str("move_follows_head", "1", b, 128); cfg.moveHead = atoi(b) != 0;   /* 0.18.22: 1 = forward is always the headset's forward; 0 = taken at move start / snap / menu (0.18.21) */
    str("gesture_indicator", "0", b, 128);   /* 0.18.20 owner: "remove the test hud that appears when raising hands" */ cfg.debugOverlay = atoi(b) != 0;
}
// ---------- state ----------
// 0.18.1 (BUG-73: "the snap turn turns a little, waits a second, then turns more"): 0.18.0 drove the game's own camera turn with
// the right stick and held the view until it landed - but the game's camera accelerates, so 30 degrees took most of a second
// and the hold was a frame late. Now the TURN IS OURS (playbook ch03: VR owns snap/smooth turn): yawOff is the view's yaw
// relative to the game camera, a snap changes it at once (one seamless snap), the left stick is rotated by it so "forward" is
// where you look, and in the background the right stick turns the game camera after it while every degree the camera turns
// comes off yawOff - the view never moves during the catch-up, and the game camera (aim, lock-on, assassination targets)
// ends up facing your way again.
struct Snap { bool on = false; float start = 0, want = 0; u64 t0 = 0; };
static std::atomic<float> gameYaw{0}; static std::atomic<int> gameYawOk{0};   // the game camera's own yaw (hSlot43, game thread)
// 0.18.17 (owner: "the movement directions are screwed up with this animation change. Let's have forward be set to the headset's
// forward every time a snap turn is made, and once at the beginning of the game / exiting any menus"): the left stick's forward is a
// WORLD yaw (moveYaw) taken from your headset's forward at those moments (hSlot43 takes it when moveCapAt is due), not the view's
// yaw without your head - looking around no longer changes which way "forward" walks, and the body faces the same way (bodyStep).
static std::atomic<float> moveYaw{0}; static std::atomic<int> moveYawOk{0}; static std::atomic<u64> moveCapAt{0}, nMoveCaps{0};
static void moveCapture(u64 delayMs) { moveCapAt = GetTickCount64() + delayMs + 1; }
// 0.18.17 (owner: "when I exit a menu, I do not have the same direction I had going into it"): the view's world yaw is kept across
// menus / theatre: saved on the way in, and on the way out the view offset is set so the view faces the same way again (the game
// camera then catches up behind it as after a snap). 0.18.16 zeroed the offset in menus instead.
static float flatViewYaw = 0; static bool flatSaved = false, wasFlat = false;
// 0.18.24 room-scale (owner: "when I move in real life, I want Jin's actual game object to move too (colliders as well), so I could physically
// dodge attacks"): the game DLL publishes how far your head is from Jin's head (world, cm) while it is safe to move him; when it is
// more than rsDeadCm and you are not using the left stick, the stick walks Jin toward your head until he is within rsStopCm. The
// game moves Jin (animation, collision, physics - his real collider), and the mod takes the distance he covered out of your head
// offset so the view stays where your head is (gotvr.cpp: the tracking reference moves with him).
static std::atomic<float> rsOff[2]; static std::atomic<u64> rsOkUntil{0}, rsDriveUntil{0}, rsBlockUntil{0}, nRsDrive{0}; static std::atomic<float> rsDir[2], rsMag{0};
static int rsOn = 1; static float rsDeadCm = 12.0f, rsStopCm = 5.0f;
// 0.18.48 the katana's lunge (design doc: "when they do [make contact], the player's body lunges forward a bit"): the game DLL asks
// for a push of the stick toward a world yaw for a few ms (distance / sword_lunge_stick_speed); it overrides your own stick meanwhile
static std::atomic<float> lungeYaw{0}; static std::atomic<u64> lungeUntil{0}, nLunges{0};
static std::atomic<u64> lastAct{0};   // 0.18.13: last time the pad sent the game anything (idle hold)
static std::atomic<u64> turnSeq{0};   // 0.18.15: counts snap / smooth turns (the body follow squares the hips after one)
static std::atomic<u64> jumpAt{0};
// 0.18.20 (owner: "setting the movement direction to the headset's direction when stopped is not working properly"): 0.18.19 took it
// 0.3 s after Jin stopped (your head then), so a look-around while standing still did not count. Now it is taken the moment the left
// stick starts a move from rest (0.25 s idle): forward = where the headset looks as you start walking.
// 0.18.64 movement-direction guard (session 19: "the same bug with the movement forward direction until I did a snap turn" - BODY/s:
// Jin running 121-124 deg off where you pushed, at 4.3 m/s, fixed by a snap): the stick's intended WORLD direction is published here,
// the game thread compares it with where Jin actually runs and, when they stay far apart while running, turns the stick by the
// difference (moveFix) - cleared at every snap / smooth turn / menu exit (turnSeq), which is what fixed it by hand
static std::atomic<float> stickWorldYaw{0}, stickMag{0}, moveFix{0}; static std::atomic<u64> stickAt{0}, nMoveFixes{0};
static std::atomic<float> headYawNow{0}; static std::atomic<int> headYawOk{0}; static std::atomic<u64> nMoveStarts{0}, nHeadMoves{0};    // 0.18.19: last jump press (the arms stay VR through the jump animation)
static std::atomic<float> viewHold{0};                                       // yaw (deg, + = left) the composed view is turned past the game camera
static float lastGameYaw = 0; static bool lastYawOk = false; static u64 lastPoll = 0; static std::atomic<u64> bleedUntil{0}; static bool catching = false;
static Snap snap; static bool everActive = false; static u32 packet = 1; static XINPUT_GAMEPAD last{}; static bool turnArmed = true;
struct Wheel { bool open = false, used = false; int sel = -1; u64 tapUntil = 0, tapStart = 0; u32 tapMask = 0; };
// 0.18.28 a chord requested by the game DLL (the katana draw asks the game to draw / sheathe: touchpad swipe right = hold D-pad right +
// flick the right stick right on PC), played by build() in order like a wheel option
static u64 kickAt = 0, flatFrom = 0; static bool kickFirst = true; static std::atomic<u64> nKicks{0};   // 0.18.30 the start-up turn kick
// 0.18.54 cbt_assist=1 ONLY (off by default; owner: no attack animations in VR - kept as a setting to compare): the game's own attack
// button pressed for a moment so Jin's attack plays when the katana's blow lands (bypasses blockMask; also works in flat play)
static std::atomic<u32> forceMask{0}; static std::atomic<u64> forceUntil{0}, nForced{0};
static std::atomic<u64> forceFrom{0};
// 0.18.63 (review): one slot - a new press of the same button never cuts an active one short (the keep-drawn tap inside a block's
// guard hold), and forcePressFresh releases a held button for gapMs first so the game sees a NEW press (a parry right after a block)
static bool forceActive(u32 m, u64 now) { return (forceMask.load() & m) && now >= forceFrom.load() && now < forceUntil.load(); }
static void forcePress(u32 m, u32 ms) { const u64 now = GetTickCount64(); nForced++;
    if (forceActive(m, now) && forceMask.load() == m) { if (forceUntil.load() < now + ms) forceUntil = now + ms; return; }
    forceMask = m; forceFrom = now; forceUntil = now + ms; }
static u32 forcePressFresh(u32 m, u32 ms, u32 gapMs) { const u64 now = GetTickCount64(); nForced++;
    if (!forceActive(m, now)) { forceMask = m; forceFrom = now; forceUntil = now + ms; return 0; }
    forceMask = m; forceFrom = now + gapMs; forceUntil = now + gapMs + ms; return gapMs; }
static std::atomic<u32> blockMask{0}; static std::atomic<u64> nBlocked{0};   // 0.18.29: pad buttons held off (the game's sword attacks while the physical katana is in hand)
static u64 chordTurnAt = 0;   // 0.18.72
static std::atomic<u32> extReqMask{0}; static std::atomic<u64> extReqSeq{0}, nExt{0}; static u64 extSeen = 0, extStart = 0, extUntil = 0; static u32 extMask = 0;
static void requestChord(u32 m) { extReqMask = m; extReqSeq++; }
static Wheel wheel; static u64 gestUntil[2] = {0, 0}, gestStart[2] = {0, 0}; static u32 gestMask[2] = {0, 0}; static bool trigWas[2] = {false, false}, atHeadWas[2] = {false, false}, trigFist[2] = {false, false}; static u64 nFistHeld = 0, trigFrom[2] = {0, 0};
static u32 prevBtn[2] = {0, 0};
// 0.18.7: buttons + a stick flick are played the way a player does it - buttons first (D-pad right = the PC port's touchpad
// modifier), the stick 120 ms later and released 30 ms before the buttons (0.18.6 did this for gestures; now also wheel options)
static void applyStaged(u32 m, u64 start, u64 until, u64 now, XINPUT_GAMEPAD &g);
static std::atomic<u64> nPolls{0}, nActive{0}, nStale{0}, nSnaps{0}, nWheelOpen{0}, nWheelPick{0}, nGest{0}, nHud{0}; static std::atomic<int> rumbleMax{0};
static u64 (*nowMs)() = GetTickCount64;   // self-test clock
static void lunge(float worldYawDeg, u32 ms) { lungeYaw = worldYawDeg; lungeUntil = nowMs() + ms; nLunges++; }
static void hudToggle();                  // defined with the UI code
static void recenterView();               // 0.18.7: the wheel's "Recenter view" (= F11), defined with the hotkeys
static void animHold(int shortHold);       // 0.18.9: the game's animation owns Jin's arms (flute until you move / dodge; takedown / quick attack 2.5 s)
static void animMoved();                   // 0.18.9: you moved or dodged: a flute hold ends
static void pulse(int h, float amp, u32 ms) { if (!S) return; S->hapPulseAmp[h] = amp; S->hapPulseMs[h] = ms; InterlockedIncrement(&S->hapPulseSeq[h]); }
static float wrap180(float a) { while (a > 180) a -= 360; while (a < -180) a += 360; return a; }
// 0.18.12 (owner: "after snap turning, the camera fights back and forth a tiny bit"): the turn the game camera made while catching up
// came off the view offset only at the NEXT controller poll (input thread), but the view is composed from the game camera of THIS
// tick (game thread): for a frame or two the view had the camera's new yaw plus the old offset - a small overshoot, then back.
// The offset (viewHold) is now the one authority, changed atomically by both threads, and the game camera's turn comes off it in
// the same render-camera update that sees the turn: the composed view does not move at all while the game camera catches up.
static void holdAdd(float d) { float o = viewHold.load(); while (!viewHold.compare_exchange_weak(o, wrap180(o + d))) {} }
static void gameTick(float gy) {   // game thread, every render-camera update of CAM-A
    if (lastYawOk && nowMs() < bleedUntil.load()) { const float d = wrap180(gy - lastGameYaw); if (d != 0.0f) holdAdd(-d); }
    lastGameYaw = gy; lastYawOk = true; gameYaw = gy; gameYawOk = 1;
}
static void radial(const float in[2], float dz, float out[2]) {   // playbook ch03: one radial deadzone, rescale, keep the angle, clamp to 1
    const float r = sqrtf(in[0] * in[0] + in[1] * in[1]); if (r <= dz || r < 1e-6f) { out[0] = out[1] = 0; return; }
    float k = (r - dz) / (1 - dz); if (k > 1) k = 1; out[0] = in[0] / r * k; out[1] = in[1] / r * k;
}
static SHORT axis(float v) { v = v < -1 ? -1 : v > 1 ? 1 : v; return (SHORT)(v * 32767.0f); }
static float dist(const float *a, const float *b) { const float d[3] = {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; return sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]); }
// Apply an output mask onto a pad state (held while the control is held).
static void apply(u32 m, XINPUT_GAMEPAD &g, float analog);
static void applyStaged(u32 m, u64 start, u64 until, u64 now, XINPUT_GAMEPAD &g) {
    const u32 STK = OX_RSU | OX_RSD | OX_RSL | OX_RSR | OX_LSU | OX_LSD | OX_LSL | OX_LSR;
    if ((m & STK) && (m & ~STK) && (now < start + 120 || now + 30 >= until)) m &= ~STK;
    apply(m, g, 1.0f);
}
static void apply(u32 m, XINPUT_GAMEPAD &g, float analog = 1.0f) {
    g.wButtons |= (WORD)(m & 0xFFFF);
    if (m & OX_LT) g.bLeftTrigger = (BYTE)fmaxf((float)g.bLeftTrigger, analog * 255.0f);
    if (m & OX_RT) g.bRightTrigger = (BYTE)fmaxf((float)g.bRightTrigger, analog * 255.0f);
    if (m & OX_RSU) g.sThumbRY = 32767; if (m & OX_RSD) g.sThumbRY = -32767; if (m & OX_RSL) g.sThumbRX = -32767; if (m & OX_RSR) g.sThumbRX = 32767;
    if (m & OX_LSU) g.sThumbLY = 32767; if (m & OX_LSD) g.sThumbLY = -32767; if (m & OX_LSL) g.sThumbLX = -32767; if (m & OX_LSR) g.sThumbLX = 32767;
}
// 0.18.1 overlay for GoTVRHost (shared block v13): the item wheel at the hand and, while testing, the head-gesture indicator
static void ovGesture(int h, const char *name) { if (!S) return; gvWriteBegin(&S->ovSeq); strncpy(S->ovGestName[h], name, sizeof S->ovGestName[h] - 1); S->ovGestSeq[h]++; gvWriteEnd(&S->ovSeq); }
static void ovPublish(const bool atHead[2], bool gameplay, int wheelHand = 0) {
    if (!S) return;
    gvWriteBegin(&S->ovSeq);
    S->ovDebug = cfg.debugOverlay && gameplay ? 1u : 0u; S->ovAtHead = gameplay ? ((atHead[0] ? 1u : 0u) | (atHead[1] ? 2u : 0u)) : 0u;
    S->ovWheelOpen = gameplay && wheel.open ? 1u : 0u; S->ovWheelSel = wheel.sel; S->ovWheelN = (u32)cfg.nWheel; S->ovWheelHand = wheelHand < 0 ? 0 : wheelHand;
    for (int i = 0; i < cfg.nWheel && i < 8; i++) { strncpy(S->ovWheelName[i], cfg.wheelName[i], sizeof S->ovWheelName[i] - 1); S->ovWheelName[i][sizeof S->ovWheelName[i] - 1] = 0; }
    gvWriteEnd(&S->ovSeq);
}
// One evaluation of the VR pad. ctxFlat = theatre / menu (plain pad semantics). Returns false when VR input is not live.
static bool build(XINPUT_GAMEPAD &g, bool ctxFlat) {
    memset(&g, 0, sizeof g); nPolls++;
    if (!cfg.enabled || !S) return false;
    GvHand hd[2]; GvPose head{}; u32 act = 0, hv = 0; LONG64 smp = 0;
    if (!gvRead(&S->inSeq, [&] { act = S->inActive; hd[0] = S->hand[0]; hd[1] = S->hand[1]; smp = S->inSamples; })) return everActive;
    gvRead(&S->poseSeq, [&] { head = S->head; hv = S->poseValid; });
    static LONG64 lastSmp = -1; static u64 lastSmpT = 0; const u64 now = nowMs();
    if (smp != lastSmp) { lastSmp = smp; lastSmpT = now; }
    const bool live = act && now - lastSmpT < 250;   // XR-002: older than 250 ms / not focused -> neutral (but stay connected)
    if (!live) { if (everActive) nStale++; snap.on = false; wheel = Wheel{}; trigWas[0] = trigWas[1] = false; ovPublish(atHeadWas, false); return everActive; }
    everActive = true; nActive++;
    const float dt = lastPoll && now > lastPoll ? fminf(0.1f, (now - lastPoll) * 0.001f) : 0.0f; lastPoll = now;
    float ls[2], rs[2]; radial(hd[0].stick, cfg.dead, ls); radial(hd[1].stick, cfg.dead, rs);
    { static bool firstCap = false; if (!firstCap) { firstCap = true; moveCapture(0); } }   // the first live poll: forward = where you look
    if (ctxFlat && !wasFlat) flatFrom = now;
    if (!ctxFlat && (kickFirst || (wasFlat && now - flatFrom >= 8000))) { kickFirst = false; kickAt = now + 1200; }   // 0.18.30: after start / a load
    if (ctxFlat && !wasFlat) { if (gameYawOk.load()) { flatViewYaw = wrap180(gameYaw.load() + viewHold.load()); flatSaved = true; } }
    if (!ctxFlat && wasFlat) {
        if (flatSaved && gameYawOk.load()) { viewHold = wrap180(flatViewYaw - gameYaw.load()); catching = fabsf(viewHold.load()) > 0.5f; turnSeq++; }
        flatSaved = false; moveCapture(60);
    }
    wasFlat = ctxFlat;
    {   // a move from rest: forward = the headset's forward now (before the stick is turned below)
        static u64 idleSince = 0; static bool stickIdle = true; const bool idleNow = ls[0] * ls[0] + ls[1] * ls[1] < 1e-4f;
        if (idleNow) { if (!stickIdle) { stickIdle = true; idleSince = now; } }
        else if (stickIdle) { stickIdle = false; if (now - idleSince >= 250 && headYawOk.load() && !ctxFlat) { moveYaw = headYawNow.load(); moveYawOk = 1; nMoveCaps++; nMoveStarts++; } }
    }
    // 0.18.22: with move_follows_head (default) the stick is turned by the headset's yaw on every poll - forward is always where you look
    { const float m = sqrtf(ls[0] * ls[0] + ls[1] * ls[1]); stickMag = m; if (m > 0.05f && headYawOk.load()) { stickWorldYaw = wrap180(headYawNow.load() + atan2f(-ls[0], ls[1]) * 57.2957795f); stickAt = now; } }
    { static u64 seenTurn = 0; const u64 ts = turnSeq.load(); if (ts != seenTurn) { seenTurn = ts; if (moveFix.load() != 0.0f) { moveFix = 0.0f; logf("controls: movement direction correction cleared (a turn / menu exit resets the game's own frame)"); } } }
    const bool headMove = cfg.moveHead && headYawOk.load() && gameYawOk.load();
    if (headMove) nHeadMoves++;
    const float yoStick = headMove ? wrap180(headYawNow.load() - gameYaw.load() + moveFix.load())
                        : (moveYawOk.load() && gameYawOk.load()) ? wrap180(moveYaw.load() - gameYaw.load()) : viewHold.load();
    if (const float yo = yoStick; !ctxFlat && fabsf(yo) > 0.01f) {   // "forward" = the headset's forward at the last snap / start / menu exit (0.18.17)
        const float a = yo / 57.2957795f, c = cosf(a), sn = sinf(a), x = ls[0], y = ls[1]; ls[0] = x * c - y * sn; ls[1] = x * sn + y * c; }
    {   // 0.18.24 room-scale drive (see rsOff): only while your own stick rests, in gameplay, when the game DLL says it is safe
        static bool driving = false; const float ux = rsOff[0].load(), uy = rsOff[1].load(), d = sqrtf(ux * ux + uy * uy);
        const bool userIdle = ls[0] * ls[0] + ls[1] * ls[1] < 0.0025f;
        const bool allowed = rsOn && now < rsOkUntil.load() && !ctxFlat && !wheel.open && userIdle && gameYawOk.load() && now >= rsBlockUntil.load();
        if (!allowed) driving = false; else { if (!driving && d > rsDeadCm) driving = true; if (driving && d < rsStopCm) driving = false; }
        if (driving && d > 1e-3f) {
            const float rel = wrap180(atan2f(uy, ux) * 57.2957795f - gameYaw.load()) / 57.2957795f;
            float m = 0.5f + (d - rsStopCm) / 30.0f; m = m < 0.5f ? 0.5f : m > 1.0f ? 1.0f : m;   // 0.18.25: faster (0.35..0.85 -> 0.5..1)
            ls[0] = -sinf(rel) * m; ls[1] = cosf(rel) * m; rsDir[0] = ux / d; rsDir[1] = uy / d; rsMag = m; rsDriveUntil = now + 100; nRsDrive++;
        } else rsMag = 0.0f;
    }
    if (now < lungeUntil.load() && !ctxFlat && !wheel.open && gameYawOk.load()) {   // 0.18.48 the lunge
        const float rel = wrap180(lungeYaw.load() - gameYaw.load()) / 57.2957795f; ls[0] = -sinf(rel); ls[1] = cosf(rel); }
    g.sThumbLX = axis(ls[0]); g.sThumbLY = axis(ls[1]);
    auto btn = [&](int h, u32 bit) { return (hd[h].buttons & bit) != 0; };
    const bool lsc = btn(0, GV_BTN_STICK), rsc = btn(1, GV_BTN_STICK);
    if (lsc) apply(cfg.out[C_LSTICKCLICK].m, g); if (rsc) apply(cfg.out[C_RSTICKCLICK].m, g);
    if (btn(0, GV_BTN_PRIMARY)) apply(cfg.out[C_LPRIMARY].m, g); if (btn(0, GV_BTN_SECONDARY)) apply(cfg.out[C_LSECONDARY].m, g); if (btn(0, GV_BTN_MENU)) apply(cfg.out[C_LMENU].m, g);
    if (btn(1, GV_BTN_PRIMARY)) apply(cfg.out[C_RPRIMARY].m, g); if (btn(1, GV_BTN_SECONDARY)) apply(cfg.out[C_RSECONDARY].m, g); if (btn(1, GV_BTN_MENU)) apply(cfg.out[C_RMENU].m, g);
    {   // 0.18.9: press edges of outputs that hand Jin's arms to the game's animation (anim / anim_short)
        static const struct { int h; u32 bit; int c; } eb[] = {{0, GV_BTN_PRIMARY, C_LPRIMARY}, {0, GV_BTN_SECONDARY, C_LSECONDARY}, {1, GV_BTN_PRIMARY, C_RPRIMARY}, {1, GV_BTN_SECONDARY, C_RSECONDARY}};
        for (auto &x : eb) if ((hd[x.h].buttons & x.bit) && !(prevBtn[x.h] & x.bit)) { const u32 m = cfg.out[x.c].m; if (m & OX_ANIM) animHold(0); if (m & OX_ANIMS) animHold(1); if (x.c == C_RSECONDARY) animMoved(); }
        if (ls[0] * ls[0] + ls[1] * ls[1] > 0.25f) animMoved();
    }
    if (ctxFlat) {   // menus / theatre / cutscenes: right stick and triggers as on a pad (tabs, map zoom), no gestures, no wheel, no turn
        snap.on = false; catching = false; wheel = Wheel{}; ovPublish(atHeadWas, false);
        g.sThumbRX = axis(rs[0]); g.sThumbRY = axis(rs[1]);
        g.bLeftTrigger = (BYTE)(hd[0].trigger * 255.0f); g.bRightTrigger = (BYTE)(hd[1].trigger * 255.0f);
        if (hd[0].squeeze > 0.6f) g.wButtons |= XB_LB; if (hd[1].squeeze > 0.6f) g.wButtons |= XB_RB;
        return true;
    }
    // 0.18.74 a fist: the trigger is ignored from the moment the grip closes with it pulled until the trigger is let go
    for (int h = 0; h < 2; h++) { if (hd[h].trigger < 0.05f) { trigFist[h] = false; trigFrom[h] = 0; } else { if (!trigFrom[h]) trigFrom[h] = now; if (cfg.fistOff && hd[h].squeeze > cfg.fistGrip) { if (!trigFist[h]) nFistHeld++; trigFist[h] = true; } } }
    // a fist closes trigger and grip together: with the grip already moving (> 0.1) the trigger waits up to 80 ms to see if it is a fist
    auto fistWait = [&](int h) { return cfg.fistOff && hd[h].squeeze > 0.1f && trigFrom[h] && now - trigFrom[h] < 80; };
    // ---- head gestures (hand by the head + its trigger) ----
    bool atHead[2] = {false, false};
    for (int h = 0; h < 2; h++) {
        atHead[h] = hv && (hd[h].poseValid & 1) && dist(hd[h].grip.p, head.p) < cfg.headM;
        const bool tr = hd[h].trigger > 0.6f && !trigFist[h] && !fistWait(h);
        if (atHead[h] && tr && !trigWas[h]) {   // edge while at the head: one gesture, held for 0.4 s
            const u32 m = cfg.out[h ? C_GEST_RHEAD : C_GEST_LHEAD].m; nGest++;
            if (m & OX_HUD) { hudToggle(); nHud++; } if (m & OX_ANIM) animHold(0); if (m & OX_ANIMS) animHold(1);
            gestMask[h] = m & ~(u32)(OX_HUD | OX_WHEEL | OX_ANIM | OX_ANIMS); gestStart[h] = now; gestUntil[h] = now + 450; pulse(h, 0.4f, 30);
            ovGesture(h, (m & OX_HUD) ? "HUD" : h ? "GUIDING WIND" : "GESTURE");
        }
        if (atHead[h] != atHeadWas[h] && atHead[h]) pulse(h, 0.15f, 12);   // you can feel when the hand is in the gesture zone
        atHeadWas[h] = atHead[h];
        if (now < gestUntil[h]) applyStaged(gestMask[h], gestStart[h], gestUntil[h], now, g);   // 0.18.6: chord played in order
        else gestMask[h] = 0;
    }
    // ---- triggers and grips (not while that hand is at the head) ----
    auto trig = [&](int h, int c) {
        const u32 m = cfg.out[c].m; const float v = hd[h].trigger;
        if (atHead[h] || (trigWas[h] && now < gestUntil[h])) return;
        if (trigFist[h] || fistWait(h)) return;   // 0.18.74 a fist sends nothing
        if (m & OX_WHEEL) return;   // the wheel reads it below
        if (m & (OX_LT | OX_RT)) apply(m & (OX_LT | OX_RT), g, v > 0.05f ? v : 0.0f);
        if (v > 0.6f) apply(m & ~(u32)(OX_LT | OX_RT), g);
    };
    trig(0, C_LTRIGGER); trig(1, C_RTRIGGER);
    if (hd[0].squeeze > 0.6f) apply(cfg.out[C_LGRIP].m, g); if (hd[1].squeeze > 0.6f) apply(cfg.out[C_RGRIP].m, g);
    // ---- item wheel: the control bound to "wheel" (X by default) holds it open; the right stick picks; stick back to centre confirms ----
    // 0.18.24 (owner's CONTROL_SCHEME: "X - weapon wheel"): any control bound to "wheel" holds it open - a trigger, a grip or a button
    int wheelHand = -1; bool wheelHeld = false;
    {   static const struct { int c, h; u32 bit; } wb[] = {{C_LPRIMARY, 0, GV_BTN_PRIMARY}, {C_LSECONDARY, 0, GV_BTN_SECONDARY}, {C_LMENU, 0, GV_BTN_MENU}, {C_LSTICKCLICK, 0, GV_BTN_STICK},
                                                           {C_RPRIMARY, 1, GV_BTN_PRIMARY}, {C_RSECONDARY, 1, GV_BTN_SECONDARY}, {C_RMENU, 1, GV_BTN_MENU}, {C_RSTICKCLICK, 1, GV_BTN_STICK}};
        for (auto &x : wb) if (cfg.out[x.c].m & OX_WHEEL) { if (wheelHand < 0) wheelHand = x.h; if (hd[x.h].buttons & x.bit) { wheelHeld = true; wheelHand = x.h; } }
        for (int h = 0; h < 2; h++) {
            if (cfg.out[h ? C_RTRIGGER : C_LTRIGGER].m & OX_WHEEL) { if (wheelHand < 0) wheelHand = h; if (!atHead[h] && hd[h].trigger > 0.6f && !trigFist[h] && !fistWait(h)) { wheelHeld = true; wheelHand = h; } }
            if (cfg.out[h ? C_RGRIP : C_LGRIP].m & OX_WHEEL) { if (wheelHand < 0) wheelHand = h; if (hd[h].squeeze > 0.6f) { wheelHeld = true; wheelHand = h; } }
        }
    }
    {   // 0.18.61 a quick press = heal (cfg.wheelTap), the wheel only once the button has been held cfg.wheelHoldMs
        static bool wPrev = false; static u64 wAt = 0; if (wheelHeld && !wPrev) wAt = now;
        if (!wheelHeld && wPrev && !wheel.open && !wheel.used && now - wAt < (u64)cfg.wheelHoldMs && cfg.wheelTap.m) {
            wheel.tapMask = cfg.wheelTap.m & ~(u32)(OX_RECENTER | OX_HUD | OX_WHEEL | OX_ANIM | OX_ANIMS); wheel.tapStart = now; wheel.tapUntil = now + 150; pulse(wheelHand < 0 ? 0 : wheelHand, 0.35f, 25);
            logf("controls: quick press of the wheel button -> heal"); }
        wPrev = wheelHeld; if (wheelHeld && now - wAt < (u64)cfg.wheelHoldMs) wheelHeld = false; else if (!wheelHeld) wPrev = false; }
    if (wheelHeld && !wheel.open && !wheel.used) { wheel.open = true; wheel.sel = -1; nWheelOpen++; pulse(wheelHand, 0.25f, 15); }
    if (!wheelHeld) { wheel.open = false; wheel.used = false; wheel.sel = -1; }
    bool stickBusy = false;
    if (wheel.open) {
        stickBusy = true;
        const float r = sqrtf(rs[0] * rs[0] + rs[1] * rs[1]);
        if (r > 0.6f) {   // option i sits at 90 - i*360/n degrees (the first at the top, then clockwise)
            const float a = atan2f(rs[1], rs[0]) * 57.2957795f; int best = 0; float bd = 999; const float step = 360.0f / (float)cfg.nWheel;
            for (int i = 0; i < cfg.nWheel; i++) { const float d = fabsf(wrap180(a - (90.0f - i * step))); if (d < bd) { bd = d; best = i; } }
            if (best != wheel.sel) { wheel.sel = best; pulse(1, 0.3f, 15); }
        } else if (r < 0.3f && wheel.sel >= 0) {   // released onto an option: use it, close until the trigger is let go
            const u32 wm = cfg.wheel[wheel.sel].m; if (wm & OX_RECENTER) recenterView(); if (wm & OX_ANIM) animHold(0); if (wm & OX_ANIMS) animHold(1);
            wheel.tapMask = wm & ~(u32)(OX_RECENTER | OX_HUD | OX_WHEEL | OX_ANIM | OX_ANIMS); wheel.tapStart = now;
            { const u32 STK = OX_RSU | OX_RSD | OX_RSL | OX_RSR | OX_LSU | OX_LSD | OX_LSL | OX_LSR; wheel.tapUntil = now + (((wheel.tapMask & STK) && (wheel.tapMask & ~STK)) ? 450 : 150); }
            nWheelPick++; pulse(1, 0.6f, 40);
            logf("controls: wheel -> %s", cfg.wheelName[wheel.sel]); wheel.open = false; wheel.used = true; wheel.sel = -1;
        }
    }
    if (now < wheel.tapUntil) applyStaged(wheel.tapMask, wheel.tapStart, wheel.tapUntil, now, g);
    { const u64 sq = extReqSeq.load(); if (sq != extSeen) { extSeen = sq; extMask = extReqMask.load(); extStart = now; extUntil = now + 450; nExt++;
          // 0.18.72 (owner, session 26: "the turning bug happens sometimes after I unsheathe my sword"): the draw / sheathe chord pushes the
          // right stick, which turns the game camera - every degree it turns is taken into the view offset (as in a snap's catch-up) so the view
          // holds still, and the turn ends like a snap (movement forward re-taken)
          if (extMask & (OX_RSU | OX_RSD | OX_RSL | OX_RSR)) { bleedUntil = extUntil + 300; chordTurnAt = extUntil + 320; } }
      if (chordTurnAt && now >= chordTurnAt) { chordTurnAt = 0; turnSeq++; moveCapture(30); } }
    if (now < extUntil) applyStaged(extMask, extStart, extUntil, now, g);
    // ---- turning: the view turns at once (snap or smooth); the game camera follows in the background ----
    if (!stickBusy) {
        if (cfg.turnSmooth) { if (rs[0] != 0.0f) { holdAdd(-rs[0] * cfg.smoothGain * 150.0f * dt); turnSeq++; moveCapture(30); } }   // stick right = turn right = yaw down (Z up)
        else {
            if (fabsf(rs[0]) < 0.35f) turnArmed = true;
            if (turnArmed && fabsf(rs[0]) > 0.7f) { holdAdd(rs[0] > 0 ? -cfg.snapDeg : cfg.snapDeg); turnArmed = false; nSnaps++; turnSeq++; moveCapture(30); }
        }
    }
    // 0.18.30 (owner: "the headset forward isn't set properly / keeps moving until I do a snap turn; at the start of the game do two very
    // quick, unnoticeable snap turns, one left and one right"): 1.2 s into gameplay (game start, and after any menu / load of 8 s or
    // more) the right stick is pushed left for 0.2 s and right for 0.2 s - what a snap turn's catch-up does to the game camera - while
    // every degree the game camera turns is taken off the view offset (as in a snap's catch-up), so the view itself does not move;
    // the turn counters / forward capture of a snap run too. Not while you are using the right stick.
    bool kicking = false;
    if (kickAt && now >= kickAt && gameYawOk.load()) {
        const u64 t = now - kickAt;
        if (t == 0 || (t < 400 && fabsf(rs[0]) < 0.2f)) { kicking = true; g.sThumbRX = axis(t < 200 ? -0.6f : 0.6f); bleedUntil = now + 250;
            static u64 lastKick = 0; if (lastKick != kickAt) { lastKick = kickAt; turnSeq++; moveCapture(30); nKicks++; logf("controls: start-up turn kick (right stick left then right under a still view)"); } }
        else if (t >= 400) { kickAt = 0; turnSeq++; moveCapture(30); }
        else kickAt = 0;   // you took the right stick: no kick
    }
    const bool chordBusy = kicking || now < wheel.tapUntil || now < gestUntil[0] || now < gestUntil[1] || now < extUntil;   // 0.18.7: a played stick flick owns the stick
    const float yawOff = viewHold.load();
    // catch-up with hysteresis (0.18.12): starts above 2 deg, stops below 0.5 deg - the game camera's own inertia after the stick is
    // released only moves the offset (invisible), it no longer re-triggers small back-and-forth stick corrections
    if (fabsf(yawOff) > 2.0f) catching = true; else if (fabsf(yawOff) < 0.5f) catching = false;
    if (catching && gameYawOk.load() && !chordBusy) {   // catch-up: the stick turns the game camera toward the view (right stick + = right)
        const float k = fminf(0.8f, fmaxf(0.25f, fabsf(yawOff) / 25.0f)); g.sThumbRX = axis(yawOff < 0 ? k : -k); bleedUntil = now + 250; }
    { const u32 bm = blockMask.load(); if (bm && (g.wButtons & (WORD)(bm & 0xFFFF))) { g.wButtons &= (WORD)~(bm & 0xFFFF); nBlocked++; }
      if ((bm & OX_RT) && g.bRightTrigger) { g.bRightTrigger = 0; nBlocked++; } if ((bm & OX_LT) && g.bLeftTrigger) { g.bLeftTrigger = 0; nBlocked++; } }
    for (int h = 0; h < 2; h++) { trigWas[h] = hd[h].trigger > 0.6f && !trigFist[h] && !fistWait(h); prevBtn[h] = hd[h].buttons; }   // 0.18.74 a held-back pull is not a press yet
    { static bool aWas = false; const bool a = (g.wButtons & XB_A) != 0; if (a && !aWas) jumpAt = now; aWas = a; }
    if (g.wButtons || g.bLeftTrigger > 30 || g.bRightTrigger > 30 || abs(g.sThumbLX) > 3000 || abs(g.sThumbLY) > 3000 || abs(g.sThumbRX) > 3000 || abs(g.sThumbRY) > 3000) lastAct = now;   // 0.18.13: idle hold lets go
    ovPublish(atHead, true, wheelHand);
    return true;
}
// rumble from the game: low-frequency motor -> left hand, high -> right hand
static void rumble(const XINPUT_VIBRATION *v) {
    if (!S || !v) return; const float k = cfg.rumble;
    S->hapLevel[0] = k * v->wLeftMotorSpeed / 65535.0f; S->hapLevel[1] = k * v->wRightMotorSpeed / 65535.0f;
    const int m = (int)((v->wLeftMotorSpeed > v->wRightMotorSpeed ? v->wLeftMotorSpeed : v->wRightMotorSpeed) * 100 / 65535); if (m > rumbleMax.load()) rumbleMax = m;
}
} // namespace vrin
