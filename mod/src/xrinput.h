// GoTVRHost VR controllers (v12). Included by host.cpp after the OpenXR function table.
// Playbook ch03 "OpenXR actions: keep physical transport and game semantics separate" / INPUT-001 / XR-002:
//   one action set, per-hand subaction paths, suggested bindings for every supported profile, attach once, sync while focused,
//   read only active states, log the profile the runtime actually chose, neutral on focus loss.
// The host publishes ONE CANONICAL CONTROLLER per hand (stick, stick click, trigger, squeeze, primary, secondary, menu,
// thumbrest, grip and aim poses). Adding a controller = adding one entry to kProfiles below; the game DLL never changes.
#pragma once

namespace xin {
enum Act { A_STICK, A_STICKCLICK, A_TRIGGER, A_SQUEEZE, A_PRIMARY, A_SECONDARY, A_MENU, A_THUMBREST, A_GRIP, A_AIM, A_HAPTIC, A_THUMBTOUCH, A_TRIGTOUCH, A_N };
struct ActDef { const char *name, *loc; XrActionType type; };
static const ActDef kActs[A_N] = {
    {"stick", "Thumbstick / trackpad", XR_ACTION_TYPE_VECTOR2F_INPUT}, {"stick_click", "Thumbstick click", XR_ACTION_TYPE_BOOLEAN_INPUT},
    {"trigger", "Trigger", XR_ACTION_TYPE_FLOAT_INPUT}, {"squeeze", "Grip", XR_ACTION_TYPE_FLOAT_INPUT},
    {"primary", "Lower face button (A / X)", XR_ACTION_TYPE_BOOLEAN_INPUT}, {"secondary", "Upper face button (B / Y)", XR_ACTION_TYPE_BOOLEAN_INPUT},
    {"menu", "Menu button", XR_ACTION_TYPE_BOOLEAN_INPUT}, {"thumbrest", "Thumb rest touch", XR_ACTION_TYPE_BOOLEAN_INPUT},
    {"grip_pose", "Hand (grip) pose", XR_ACTION_TYPE_POSE_INPUT}, {"aim_pose", "Pointer (aim) pose", XR_ACTION_TYPE_POSE_INPUT},
    {"haptic", "Vibration", XR_ACTION_TYPE_VIBRATION_OUTPUT},
    {"thumb_touch", "Thumb on the stick / buttons", XR_ACTION_TYPE_BOOLEAN_INPUT}, {"trigger_touch", "Finger on the trigger", XR_ACTION_TYPE_BOOLEAN_INPUT},   // 0.14.3: VR fingers
};
// Binding: hand 0 = left, 1 = right, 2 = both; path below /user/hand/<h>
struct Bind { int act; int hand; const char *path; };
struct Profile { const char *path; const char *ext; const Bind *b; int n; };
#define B(a, h, p) {a, h, p}
static const Bind kTouch[] = {   // Quest 1/2/3 Touch, Rift S, Touch Plus/Pro through their own profiles below
    B(A_STICK, 2, "/input/thumbstick"), B(A_STICKCLICK, 2, "/input/thumbstick/click"), B(A_TRIGGER, 2, "/input/trigger/value"), B(A_SQUEEZE, 2, "/input/squeeze/value"),
    B(A_PRIMARY, 0, "/input/x/click"), B(A_SECONDARY, 0, "/input/y/click"), B(A_MENU, 0, "/input/menu/click"), B(A_PRIMARY, 1, "/input/a/click"), B(A_SECONDARY, 1, "/input/b/click"),
    B(A_THUMBREST, 2, "/input/thumbrest/touch"), B(A_GRIP, 2, "/input/grip/pose"), B(A_AIM, 2, "/input/aim/pose"), B(A_HAPTIC, 2, "/output/haptic"),
    B(A_THUMBTOUCH, 2, "/input/thumbstick/touch"), B(A_THUMBTOUCH, 2, "/input/thumbrest/touch"), B(A_THUMBTOUCH, 0, "/input/x/touch"), B(A_THUMBTOUCH, 0, "/input/y/touch"),
    B(A_THUMBTOUCH, 1, "/input/a/touch"), B(A_THUMBTOUCH, 1, "/input/b/touch"), B(A_TRIGTOUCH, 2, "/input/trigger/touch")};
static const Bind kIndex[] = {
    B(A_STICK, 2, "/input/thumbstick"), B(A_STICKCLICK, 2, "/input/thumbstick/click"), B(A_TRIGGER, 2, "/input/trigger/value"), B(A_SQUEEZE, 2, "/input/squeeze/value"),
    B(A_PRIMARY, 2, "/input/a/click"), B(A_SECONDARY, 2, "/input/b/click"), B(A_GRIP, 2, "/input/grip/pose"), B(A_AIM, 2, "/input/aim/pose"), B(A_HAPTIC, 2, "/output/haptic"),
    B(A_THUMBTOUCH, 2, "/input/thumbstick/touch"), B(A_THUMBTOUCH, 2, "/input/a/touch"), B(A_THUMBTOUCH, 2, "/input/b/touch"), B(A_TRIGTOUCH, 2, "/input/trigger/touch")};
static const Bind kVive[] = {    // no stick: the trackpad is the stick, its click the stick click; menu = the upper button
    B(A_STICK, 2, "/input/trackpad"), B(A_STICKCLICK, 2, "/input/trackpad/click"), B(A_TRIGGER, 2, "/input/trigger/value"), B(A_SQUEEZE, 2, "/input/squeeze/click"),
    B(A_SECONDARY, 2, "/input/menu/click"), B(A_GRIP, 2, "/input/grip/pose"), B(A_AIM, 2, "/input/aim/pose"), B(A_HAPTIC, 2, "/output/haptic")};
static const Bind kWmr[] = {     // trackpad click = lower button, menu = upper button
    B(A_STICK, 2, "/input/thumbstick"), B(A_STICKCLICK, 2, "/input/thumbstick/click"), B(A_TRIGGER, 2, "/input/trigger/value"), B(A_SQUEEZE, 2, "/input/squeeze/click"),
    B(A_PRIMARY, 2, "/input/trackpad/click"), B(A_SECONDARY, 2, "/input/menu/click"), B(A_GRIP, 2, "/input/grip/pose"), B(A_AIM, 2, "/input/aim/pose"), B(A_HAPTIC, 2, "/output/haptic")};
static const Bind kHp[] = {
    B(A_STICK, 2, "/input/thumbstick"), B(A_STICKCLICK, 2, "/input/thumbstick/click"), B(A_TRIGGER, 2, "/input/trigger/value"), B(A_SQUEEZE, 2, "/input/squeeze/value"),
    B(A_PRIMARY, 0, "/input/x/click"), B(A_SECONDARY, 0, "/input/y/click"), B(A_PRIMARY, 1, "/input/a/click"), B(A_SECONDARY, 1, "/input/b/click"), B(A_MENU, 2, "/input/menu/click"),
    B(A_GRIP, 2, "/input/grip/pose"), B(A_AIM, 2, "/input/aim/pose"), B(A_HAPTIC, 2, "/output/haptic")};
static const Bind kPico[] = {
    B(A_STICK, 2, "/input/thumbstick"), B(A_STICKCLICK, 2, "/input/thumbstick/click"), B(A_TRIGGER, 2, "/input/trigger/value"), B(A_SQUEEZE, 2, "/input/squeeze/value"),
    B(A_PRIMARY, 0, "/input/x/click"), B(A_SECONDARY, 0, "/input/y/click"), B(A_MENU, 0, "/input/menu/click"), B(A_PRIMARY, 1, "/input/a/click"), B(A_SECONDARY, 1, "/input/b/click"),
    B(A_GRIP, 2, "/input/grip/pose"), B(A_AIM, 2, "/input/aim/pose"), B(A_HAPTIC, 2, "/output/haptic")};
static const Bind kSimple[] = {
    B(A_TRIGGER, 2, "/input/select/click"), B(A_MENU, 2, "/input/menu/click"), B(A_GRIP, 2, "/input/grip/pose"), B(A_AIM, 2, "/input/aim/pose"), B(A_HAPTIC, 2, "/output/haptic")};
#undef B
#define P(path, ext, arr) {path, ext, arr, (int)(sizeof arr / sizeof arr[0])}
static const Profile kProfiles[] = {
    P("/interaction_profiles/oculus/touch_controller", nullptr, kTouch),
    P("/interaction_profiles/meta/touch_controller_plus", "XR_META_touch_controller_plus", kTouch),
    P("/interaction_profiles/facebook/touch_controller_pro", "XR_FB_touch_controller_pro", kTouch),
    P("/interaction_profiles/valve/index_controller", nullptr, kIndex),
    P("/interaction_profiles/htc/vive_controller", nullptr, kVive),
    P("/interaction_profiles/microsoft/motion_controller", nullptr, kWmr),
    P("/interaction_profiles/hp/mixed_reality_controller", "XR_EXT_hp_mixed_reality_controller", kHp),
    P("/interaction_profiles/bytedance/pico4_controller", "XR_BD_controller_interaction", kPico),
    P("/interaction_profiles/khr/simple_controller", nullptr, kSimple),
};
#undef P
static const int NPROF = (int)(sizeof kProfiles / sizeof kProfiles[0]);
static const char *kWantExt[] = {"XR_META_touch_controller_plus", "XR_FB_touch_controller_pro", "XR_EXT_hp_mixed_reality_controller", "XR_BD_controller_interaction"};

static PFN_xrStringToPath sToPath; static PFN_xrPathToString sPathToStr; static PFN_xrCreateActionSet sCreateSet; static PFN_xrCreateAction sCreateAct;
static PFN_xrSuggestInteractionProfileBindings sSuggest; static PFN_xrAttachSessionActionSets sAttach; static PFN_xrSyncActions sSync;
static PFN_xrGetActionStateBoolean sBool; static PFN_xrGetActionStateFloat sFloat; static PFN_xrGetActionStateVector2f sVec2; static PFN_xrGetActionStatePose sPose;
static PFN_xrCreateActionSpace sActSpace; static PFN_xrApplyHapticFeedback sHapOn; static PFN_xrStopHapticFeedback sHapOff; static PFN_xrGetCurrentInteractionProfile sCurProf;
static XrActionSet set = XR_NULL_HANDLE; static XrAction act[A_N]; static XrPath handPath[2]; static XrSpace gripSp[2], aimSp[2];
static bool ready = false, profDirty = true, hapOn[2] = {false, false}; static LONG hapSeen[2] = {0, 0};
static std::vector<const char *> enabledExt;
static u64 nSync = 0, nSyncFail = 0;

// before xrCreateInstance: which optional controller extensions the runtime offers (enabled when present)
static void pickExtensions(PFN_xrGetInstanceProcAddr gpa) {
    PFN_xrEnumerateInstanceExtensionProperties en = nullptr; gpa(XR_NULL_HANDLE, "xrEnumerateInstanceExtensionProperties", (PFN_xrVoidFunction *)&en);
    if (!en) return; u32 n = 0; if (XR_FAILED(en(nullptr, 0, &n, nullptr)) || !n) return;
    std::vector<XrExtensionProperties> ep(n, {XR_TYPE_EXTENSION_PROPERTIES}); en(nullptr, n, &n, ep.data());
    for (const char *w : kWantExt) for (auto &e : ep) if (!strcmp(e.extensionName, w)) { enabledExt.push_back(w); break; }
    std::string l; for (auto *e : enabledExt) { l += " "; l += e; } logf("controllers: optional profile extensions offered and enabled:%s", l.empty() ? " none" : l.c_str());
    // WRIST_HUD.md W1/W3: every extension the runtime offers, logged once (does it have XR_FB_composition_layer_depth_test /
    // XR_KHR_composition_layer_depth - can a quad be hidden behind the game's world?)
    std::string all; for (auto &e : ep) { all += " "; all += e.extensionName; } logf("runtime extensions (%u):%s", n, all.c_str());
}
static bool extOn(const char *e) { if (!e) return true; for (auto *x : enabledExt) if (!strcmp(x, e)) return true; return false; }

static bool init() {
#define L(fn, var) gipa(inst, #fn, (PFN_xrVoidFunction *)&var)
    L(xrStringToPath, sToPath); L(xrPathToString, sPathToStr); L(xrCreateActionSet, sCreateSet); L(xrCreateAction, sCreateAct);
    L(xrSuggestInteractionProfileBindings, sSuggest); L(xrAttachSessionActionSets, sAttach); L(xrSyncActions, sSync);
    L(xrGetActionStateBoolean, sBool); L(xrGetActionStateFloat, sFloat); L(xrGetActionStateVector2f, sVec2); L(xrGetActionStatePose, sPose);
    L(xrCreateActionSpace, sActSpace); L(xrApplyHapticFeedback, sHapOn); L(xrStopHapticFeedback, sHapOff); L(xrGetCurrentInteractionProfile, sCurProf);
#undef L
    if (!sToPath || !sCreateSet || !sCreateAct || !sSuggest || !sAttach || !sSync || !sBool || !sFloat || !sVec2 || !sPose || !sActSpace || !sCurProf) { logf("controllers: action functions missing -> no VR controllers"); return false; }
    sToPath(inst, "/user/hand/left", &handPath[0]); sToPath(inst, "/user/hand/right", &handPath[1]);
    XrActionSetCreateInfo si{XR_TYPE_ACTION_SET_CREATE_INFO}; strcpy(si.actionSetName, "gameplay"); strcpy(si.localizedActionSetName, "Ghost of Tsushima VR"); si.priority = 0;
    if (!ok(sCreateSet(inst, &si, &set), "xrCreateActionSet")) return false;
    for (int a = 0; a < A_N; a++) {
        XrActionCreateInfo ai{XR_TYPE_ACTION_CREATE_INFO}; strcpy(ai.actionName, kActs[a].name); strcpy(ai.localizedActionName, kActs[a].loc);
        ai.actionType = kActs[a].type; ai.countSubactionPaths = 2; ai.subactionPaths = handPath;
        if (!ok(sCreateAct(set, &ai, &act[a]), "xrCreateAction")) return false;
    }
    int nOk = 0;
    for (int p = 0; p < NPROF; p++) {
        const Profile &pr = kProfiles[p]; if (!extOn(pr.ext)) continue;
        std::vector<XrActionSuggestedBinding> sb;
        for (int i = 0; i < pr.n; i++) for (int h = 0; h < 2; h++) {
            if (pr.b[i].hand != 2 && pr.b[i].hand != h) continue;
            char path[160]; snprintf(path, sizeof path, "/user/hand/%s%s", h ? "right" : "left", pr.b[i].path);
            XrPath xp; if (XR_FAILED(sToPath(inst, path, &xp))) continue; sb.push_back({act[pr.b[i].act], xp});
        }
        XrPath pp; sToPath(inst, pr.path, &pp);
        XrInteractionProfileSuggestedBinding s{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING}; s.interactionProfile = pp; s.countSuggestedBindings = (u32)sb.size(); s.suggestedBindings = sb.data();
        const XrResult r = sSuggest(inst, &s); if (XR_SUCCEEDED(r)) nOk++;
        logf("controllers: bindings for %s: %s (%u)", pr.path, XR_SUCCEEDED(r) ? "suggested" : "REFUSED", (u32)sb.size());
    }
    for (int h = 0; h < 2; h++) {
        XrActionSpaceCreateInfo ci{XR_TYPE_ACTION_SPACE_CREATE_INFO}; ci.poseInActionSpace.orientation.w = 1; ci.subactionPath = handPath[h];
        ci.action = act[A_GRIP]; sActSpace(sess, &ci, &gripSp[h]); ci.action = act[A_AIM]; sActSpace(sess, &ci, &aimSp[h]);
    }
    XrSessionActionSetsAttachInfo at{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO}; at.countActionSets = 1; at.actionSets = &set;
    if (!ok(sAttach(sess, &at), "xrAttachSessionActionSets")) return false;
    ready = nOk > 0; logf("controllers: %d profile(s) suggested, action set attached", nOk);
    return ready;
}
static void neutral() { gvWriteBegin(&S->inSeq); S->inActive = 0; for (auto &h : S->hand) h = GvHand{}; gvWriteEnd(&S->inSeq); }
static void logProfiles() {
    for (int h = 0; h < 2; h++) {
        XrInteractionProfileState ps{XR_TYPE_INTERACTION_PROFILE_STATE}; char nm[XR_MAX_PATH_LENGTH] = "";
        if (XR_SUCCEEDED(sCurProf(sess, handPath[h], &ps)) && ps.interactionProfile != XR_NULL_PATH && sPathToStr) { u32 n = 0; sPathToStr(inst, ps.interactionProfile, sizeof nm, &n, nm); }
        if (strcmp(S->inProfile[h], nm)) { strncpy(S->inProfile[h], nm, sizeof S->inProfile[h] - 1); logf("controllers: %s hand -> %s", h ? "right" : "left", nm[0] ? nm : "(none)"); }
    }
}
// once per host frame, after xrWaitFrame; t = the display time the head pose was predicted for
static void frame(XrTime t, bool focused) {
    if (!ready) return;
    if (!focused) { if (S->inActive) neutral(); return; }
    XrActiveActionSet as{set, XR_NULL_PATH}; XrActionsSyncInfo sy{XR_TYPE_ACTIONS_SYNC_INFO}; sy.countActiveActionSets = 1; sy.activeActionSets = &as;
    const XrResult r = sSync(sess, &sy); if (XR_FAILED(r) || r == XR_SESSION_NOT_FOCUSED) { nSyncFail++; if (S->inActive) neutral(); return; }
    nSync++; if (profDirty) { profDirty = false; logProfiles(); }
    GvHand hd[2] = {};
    for (int h = 0; h < 2; h++) {
        GvHand &o = hd[h]; XrActionStateGetInfo gi{XR_TYPE_ACTION_STATE_GET_INFO}; gi.subactionPath = handPath[h];
        auto bl = [&](int a, u32 bit) { XrActionStateBoolean s{XR_TYPE_ACTION_STATE_BOOLEAN}; gi.action = act[a]; if (XR_SUCCEEDED(sBool(sess, &gi, &s)) && s.isActive && s.currentState) o.buttons |= bit; };
        auto fl = [&](int a) { XrActionStateFloat s{XR_TYPE_ACTION_STATE_FLOAT}; gi.action = act[a]; return (XR_SUCCEEDED(sFloat(sess, &gi, &s)) && s.isActive) ? s.currentState : 0.0f; };
        { XrActionStateVector2f s{XR_TYPE_ACTION_STATE_VECTOR2F}; gi.action = act[A_STICK]; if (XR_SUCCEEDED(sVec2(sess, &gi, &s)) && s.isActive) { o.stick[0] = s.currentState.x; o.stick[1] = s.currentState.y; } }
        o.trigger = fl(A_TRIGGER); o.squeeze = fl(A_SQUEEZE);
        bl(A_STICKCLICK, GV_BTN_STICK); bl(A_PRIMARY, GV_BTN_PRIMARY); bl(A_SECONDARY, GV_BTN_SECONDARY); bl(A_MENU, GV_BTN_MENU); bl(A_THUMBREST, GV_BTN_THUMBREST); bl(A_THUMBTOUCH, GV_BTN_THUMBTOUCH); bl(A_TRIGTOUCH, GV_BTN_TRIGTOUCH);
        if (o.trigger > 0.6f) o.buttons |= GV_BTN_TRIGGER; if (o.squeeze > 0.6f) o.buttons |= GV_BTN_GRIP;
        auto pose = [&](XrSpace sp, GvPose &d, u32 bit) {
            XrSpaceLocation l{XR_TYPE_SPACE_LOCATION};
            if (sp && XR_SUCCEEDED(xrLocateSpace_(sp, local, t, &l)) && (l.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) && (l.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT)) {
                d.q[0] = l.pose.orientation.x; d.q[1] = l.pose.orientation.y; d.q[2] = l.pose.orientation.z; d.q[3] = l.pose.orientation.w; d.p[0] = l.pose.position.x; d.p[1] = l.pose.position.y; d.p[2] = l.pose.position.z; o.poseValid |= bit; }
        };
        pose(gripSp[h], o.grip, 1); pose(aimSp[h], o.aim, 2);
    }
    gvWriteBegin(&S->inSeq); S->inActive = 1; S->inTime = t; S->hand[0] = hd[0]; S->hand[1] = hd[1]; S->inSamples++; gvWriteEnd(&S->inSeq);
    // haptics: the game's rumble level (continuous, re-applied with a short duration every frame) and one-shot pulses
    if (sHapOn && sHapOff) for (int h = 0; h < 2; h++) {
        XrHapticActionInfo hi{XR_TYPE_HAPTIC_ACTION_INFO}; hi.action = act[A_HAPTIC]; hi.subactionPath = handPath[h];
        const LONG ps = S->hapPulseSeq[h];
        if (ps != hapSeen[h]) { hapSeen[h] = ps; XrHapticVibration v{XR_TYPE_HAPTIC_VIBRATION}; v.amplitude = S->hapPulseAmp[h]; v.duration = (XrDuration)S->hapPulseMs[h] * 1000000; v.frequency = XR_FREQUENCY_UNSPECIFIED; sHapOn(sess, &hi, (XrHapticBaseHeader *)&v); continue; }
        const float lv = S->hapLevel[h];
        if (lv > 0.02f) { XrHapticVibration v{XR_TYPE_HAPTIC_VIBRATION}; v.amplitude = lv > 1 ? 1 : lv; v.duration = 40000000; v.frequency = XR_FREQUENCY_UNSPECIFIED; sHapOn(sess, &hi, (XrHapticBaseHeader *)&v); hapOn[h] = true; }
        else if (hapOn[h]) { sHapOff(sess, &hi); hapOn[h] = false; }
    }
}
} // namespace xin
