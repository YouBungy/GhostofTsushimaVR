// GoTVR 0.6 — camera/projection seams + alternate-eye 6DoF stereo; OpenXR runs in GoTVRHost.exe (out of process) for Ghost of Tsushima DIRECTOR'S CUT (Steam)
// exe SHA-256 ED0931B6F6D3CA3874BAF39EBC16D95FF0E93296891182F96C3A790DA7109F63, PE timestamp 1782230973.
//
// Loaded as xinput1_4.dll from the game folder (the pad loader 0x497F30 asks for that name first).
// Everything fails closed: unknown build -> pure XInput passthrough, no hooks.
//
// The game process never loads the OpenXR runtime (BUG-5/BUG-6: both in-process crashes were inside the
// runtime's own code in this hook-heavy process). This DLL: census + camera hooks + per-frame eye packets
// keyed by the engine's Reflex frame id + copy of each finished frame into shared D3D12 textures.
#define WIN32_LEAN_AND_MEAN
#define WIDL_EXPLICIT_AGGREGATE_RETURNS
#include <windows.h>
#include <bcrypt.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <intrin.h>
#include <tlhelp32.h>
#include <atomic>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <vector>
#include <cfloat>
#include <cstddef>
#include "MinHook.h"
#include "gotvr_shared.h"

typedef uint64_t u64; typedef uint32_t u32; typedef uint8_t u8;
#define GOTVR_VERSION "GoTVR 0.18.65 (session-20 fixes: the parry hitbox is the whole sword - 30 cm below the guard to 12 cm past the tip, 40 cm around it, recorded whenever the katana is in your hand; block / parry feedback: a synthesized steel clang, a rim flash and rumble; the damage vignette in GoTVRHost (shared block v17); VR arms kept for grapples, ropes, logs and riding - the game's arms only for climbing (a raised hand while slow), takedowns and cutscenes; the game's own sheathe / hidden draws silenced, and its sword put away when yours is home and your hand leaves the hilt (no drawn stance afterwards); hand pitch put back to 20 deg (F1 is the hand trim); Ctrl+Shift+U saves the UI image (the black square); 0.18.64: session-19 fixes: parry_engine=mod - a blow that meets the blade is not applied (block), a parry also staggers the attacker, one swing = one defence (repeat contacts swallowed - they hit Jin twice before); parry_engine=game kept (guard replay at 130 ms); the stance held only while Jin stands (sword_mask_while_held 2); the game's pace adapts to the headset (xr_game_fps_adaptive); movement-direction guard (Jin running 60+ deg off the stick for 0.5 s -> the stick is turned, cleared at the next turn); a load / respawn ends a death-camera cutscene (first person again); 0.18.63: review fixes: heap memory checks trusted 50 ms (crash guard), the game's draw toggle never sent twice within 2.5 s, a parry after a held block is a fresh guard press, the keep-drawn tap never cuts a guard short, a free left hand never spends a one-handed heavy charge, PARRY/total log line; 0.18.62: session-18 fixes: no black screen when hit (the full-view hit flash off by default), parry history long enough to judge (one sample per 8 ms) and the game's sword drawn for a guard / kept out while enemies are near, parry_mode=blade (the enemy's own weapon as the hitbox) as a test option, heavy attack = hold the trigger to charge, then swing (25 cm of drift allowed, a 300 ms swing window), two-handed katana steadied (implicit spring step - no flicker), VR arms kept on logs / ropes (only hands above the head hand the arms to the game, held 1.2 s for grapples), Jin's walking animations no longer frozen while the katana is out (sword_mask_while_held 0), the game's frame pace capped in xr=4 (xr_game_fps_cap 58 / xr_menu_fps_cap 60) so the headset compositor is never starved; 0.18.61: block / parry with the katana by timing (the blade in the blow's path early = block, just in time = parry; the game's own guard and its blow replayed); frame-rate fix with the katana out (memory checks cached); the left hand carried on the hilt by the solved right fist; Jin's upper body held while the katana is out (no game draw / stance / sheathe); triggers: RT = the game's R2 + standoff, LT = Focus, both held off with the katana out; wheel button: quick press heals, hold opens the wheel; big rumble on a heavy swing; the HUD over the whole view for a moment when Jin is hit; no cutscene for the death fade; 0.18.60: records for dogs as well as people (their own slots, people's as the fallback), the plainest blow of each kind kept (no 150 / 225-damage 'light' records); 0.18.59: the flinching record saved for VR: it carries its own copy of the attack definition (session 15: not reachable from Jin), loaded at start so a plainer blow never replaces it; 0.18.58: katana weight after Blade & Sorcery: a rotational spring-damper (inertia, a hint of follow-through), a little gravity sag one-handed, a second hand as a second joint, a soft lag limit, an enumerated reset list - sword_weight 1.0 by default; the attack definition followed up to three steps from the moveset entry so the flinching record can be saved for VR; the katana's blows carry the blade's real contact point and swing direction into the game's hit (the reaction faces your cut); 0.18.57: Ctrl+Shift+M crash fixed: the message path never carries the hit-reaction bit (a template with it is dropped from gotvr.ini); the attack definition searched through the moveset entry and the component's per-attack array so a record can be saved for VR; 0.18.56: records found again from Jin one step deeper and with their own attack-definition cell (session 12: not saved - the component is not in Jin directly), the message path's templates prefer the hit-reaction bit, Ctrl+Shift+M = one blow by the message path to compare; 0.18.55: the katana's blows through the game's whole hit path from a RECORD LIBRARY: only ordinary damaging blows kept (no finishers / kills - the insta-kill fix), light and heavy, a record whose blow carries the game's hit reaction always preferred, damage scaled on the record's own base damage, saved to gotvr.ini with how to find its pointers from Jin again so VR needs no real blow; 0.18.54: flinch hunt without any attack animation: the whole delivered message real vs ours (MSGDIFF), flat swing test Ctrl+Shift+G (the katana's blow during YOUR own swing); cbt_assist=1 keeps the old method (the game's attack pressed, its animation plays) for comparison, off by default; resolve maximum from the game's own progression value (hero+0x4644 + 3), VITALS lines in flat play; resolve scan off; 0.18.53: the katana's blows run the game's whole hit path - its composer and attack callbacks on a copy of Jin's own hit record - for the enemy's flinch (cbt_mode 2); resolve (hero+0x43A4) on the wrist gauge (wrist_resolve_max 10) with VITALS log lines to find its maximum; the strike's own script events logged; 0.18.52: flat diagnostics: the game's own hit processing and the script events a blow fires, real vs ours; 0.18.51: parries and staggers marked in the resolve scan automatically; 0.18.50: blows carry Jin as the attacker (+0x168) and a combo step like his own, delivery diff + Ctrl+Shift+W data watchpoints to find the enemy flinch; 0.18.49: the katana's blows delivered the game's own way (its delivery 0x85A770: the hit events that make enemies flinch, bleed and sound), not straight into HandleHit; 0.18.48: combat stage 1b: every enemy the game itself counts as hostile can be cut (its own attitude query, not only those already in a hit), heavy blows (two hands + right trigger held still to charge), a lunge toward the enemy struck, swing / charge / heavy rumble, katana weight, all from the design doc's attack parameters; hit-reaction diagnostics; 0.18.47: combat stage 1: the katana cuts - a blow when the blade meets a hostile enemy fast enough (closing speed at the contact, your own walking taken off, swept tip, one blow per pass), sent as the game's own hit message copied from Jin's own blows (damage x0.75..x1.10 by speed, killing blows the game's way), rumble on the blow, a light tap on a slow touch, Ctrl+Shift+H / K / J flat test keys; 0.18.46: the enemy watcher follows real hits only (spawns no longer grab it); 0.18.45: flat capture: the enemy last in a hit with Jin watched like the hero (its attack wind-up for a VR parry), 32 KB of the hero watched; 0.18.44: flat-play capture for the design doc: 23 action tags (numpad, Ctrl+numpad, numpad / * -) and keyboard / mouse presses logged with their time; 0.18.43: combat observer: the game's SetHealth (0x69F3D0) and HandleHit (0x694080) logged for every character, hits with their hit records, COMBAT/s with the attacks incoming on Jin; the right trigger = Focus (touchpad press / View); flat log with decimal values; 0.18.42: the sheath mouth caught at your left hand, a wider catch (12 cm, 55 deg - your tries were 37-46 deg off), the game's auto-sheathe timing remembered from the start and both its draw and put-away animations masked to the end, play_mode written into gotvr.ini; 0.18.41: sheathing catches the REAL tip (sword_blade_out_cm 45 - the tip was 45 cm too deep), the game's own auto-sheathe masked (learned timing) and redrawn in the background, the sheath never flies to the hand while the game's sword is briefly away, the walking legs kept under the mask, the thumb follows your thumb again, numpad action tags for the flat-play log; 0.18.40: sheathing is yours alone - the game is never asked to sheathe (no animation, no repeats), the tip taken at the real sheath mouth and slid home along the rail, the katana stays in Jin's hand until you sheathe it, a blade let go of stays where it is in the sheath; 0.18.39: the sword alone out of the sheath again - the game's own sword drawn in the background with its draw / sheathe animation masked on Jin (his pose from before, the camera unmoved), placed in your hand by the mod, the sheath stays on the hip; two-handed steering halved (sword_two_hand_steer 0.5); 0.18.38: the game never asked to draw / sheathe (no draw / sheathe animation), the 0.18.37 per-eye TAA removed, the katana and the hands on it no longer a frame behind the body while you move, the guard found from your captured left grip so the right fist goes where sword_right_hand_cm says (Ctrl+PageDown / PageUp, Ctrl+End / Home move the hands live), the body follows the headset unless a hand grips the sheathed katana; 0.18.37: the game's own drawn sword (joint 637) held in Jin's fist with the game's grip, the game drawn / sheathed with your draw, per-eye TAA on top of the culling preset (no arm / katana ghost), no body teleport when running back / sideways, faster stand-up camera, two hands hold until let go; 0.18.36: katana carried by Jin's actual fist (never beyond the hand), captured left grip turned thumb-up the sheath, two hands hold until let go, triggers held off while the katana is held (idle hold), blade probe logs whether the game has its sword drawn; 0.18.35: katana: drawn-grip capture (Ctrl+F5 with the sword out), captured grips shown as captured, swing filter (no katana lag), the original 0.18.26 joints back in the blade probe; 0.18.34: katana probe: the game-draw joints 637 / 644 / 651 in the Ctrl+F7 list, Ctrl+F9 census sampled every 50 ms with the game's own pose and 637 in the hand / katana frames; 0.18.33: katana: grip capture hotkeys (Ctrl+F5 right on the hilt, Ctrl+F6 left on the sheath), the drawn katana steered from the right controller with the blade turned along its forward, fists snapped onto it, two-handed gate on the unsteered katana (no flicker), katana settings written into gotvr.ini; 0.18.32: katana: blade along the controller's forward (knuckles to the side), the right fist lower on the hilt, the left hand up at the sheath mouth, two-handed grip (left hand on the hilt steers), drawn katana published for sheathing, Ctrl+F9 game-draw joint census; 0.18.31: katana rail from the katana joints (the drawn hilt), right wrist bent so the blade points along the controller, blade probe with the sheath joint held still, body held still while you reach for the katana, faster index finger, a slightly longer draw; 0.18.30: Jin's own fist snapped onto the hilt / sheath, left thumb along the sheath to the guard, katana held through the fist, right-hand-only draw freed at arm reach, start-up turn kick for the movement forward; 0.18.29 physical katana: hands locked on the hilt / sheath, only the blade moves (blade joint pick), draw frees at 40 cm of pull, sheathing by the tip along the rail, the game's draw off and its attacks held off while the katana is out; 0.18.28 katana: hold finger poses on the hilt and the sheath, hands pinned to the sword while drawing, the game draws / sheathes with you, the hilt grabbed anywhere along it; body squares to the headset when you stop; left-trigger calibration removed; no memory queries in the audio emitter hook, katana draw stage 1: sheath + hilt grab, rail, rumble, logs, joint probe and calibration; Jin's own sounds centred; physical dodging off; crouched start; hand deadband; look-down threshold 30; arm stretch, 3D audio: the Wwise listener is your head, Jin's own pose back in frozen frames (head shown in pause / cutscene starts), view rigid on the drawn body (no orbit when Jin turns to walk), faster room-scale, body turn eases in, AO fix through the game's own settings apply (AO off + screen-space shadows on), hand stabilisation, body threshold 20 deg, X = item wheel, play_mode=flat with state / flag logging, room-scale: Jin walks under your head, ambient occlusion held off in first person (see-through arms), VR arms kept through the landing, eyes 3 cm right / 2 cm up, body turns faster, headset recenter button recentres the game, forearm twist with the wrist, movement always follows the headset, body turns to the headset past 7.5 deg standing and moving, movement forward only when you start moving, fingers bend on one hinge each (no sideways fingers), thumb curls across like a fist, fingers ease into their poses, Shift+F6 arm shading test (screen-space shadows / AO), near clip back to the game, fingers: thumb by length + the hand's own palm, thumb / trigger touch, forward taken as you start moving, the body turns past the threshold while moving too, gesture indicator off, fingers from leaf paths + structure log, riding needs two markers (jumps keep the VR arms), movement forward taken at stops, Shift+F4 HUD-layer test, VR fingers: bind-pose fingers on the IK hands, grip curls a fist, trigger the index, movement forward = headset forward at snap / start / menu exit, view direction kept across menus, body dead zone 45 deg, body under the headset smoothed, body heading kept in world yaw with an even dead zone, movement faces the headset, body under the headset, smoother eye anchor, Shift+F7 near-plane A/B, standing hips squared to the body follow, hips turned square after a snap turn, Shift+F7 TAA A/B, 0.18.12-14 transparency changes removed, standing stance squared to face forward, idle animations held still in first person, near-focus stipple off in first person, snap turn without the catch-up wobble, wrist point from the grip (back / up / thumb, Ctrl+F1-F3), near clip in the environment too (TAA reprojection agrees), eyes 3 cm further back, each eye culled by its own view - no cross-eye occlusion (preset 7), eyes 5 cm further back, near plane 2 cm in first person, F1/F2/F3 hand trims saved to gotvr.ini, near stipple fade off in first person, hands pitched, grip unbound, per-eye occlusion by tracked slot ownership, hands flipped, near plane 4 cm, stand tall in the sword stance, arms handed to the game for flute / climbing / takedowns, arm chains by bone chain, F11 resets the per-eye histories, five-option wheel with flute and recenter, eyes centred on the head, AA diagnostics, motion blur off in VR, carried items turn with the body, body drawn facing forward when running back, eye-order swap diagnostic, head found by its joint cluster - lone gear above it is skipped, left eye composed before the tick, VR controllers for any OpenXR controller: virtual pad, snap turn, item wheel, head gestures, rumble; first person with the head hidden in the render pose, xr=4 SYNCHRONIZED-SEQUENTIAL stereo: both eyes from one simulation tick, pair-atomic presentation, per-eye occlusion + TAA history, left-eye monitor mirror, HUD on its own sharp layer that stays put in the room (lazy follow), cinematic letterbox bars and fades over the whole view, theatre screen for videos/pause/menus/loading (flat-rendered); xr=3 experimental)"
static const u32 EXPECTED_TIMESTAMP = 1782230973u;
static const char EXPECTED_SHA[] = "ed0931b6f6d3ca3874baf39ebc16d95ff0e93296891182f96c3a790da7109f63";

// ---- RVAs (ADDRESS_REGISTRY.md) ----
enum : u32 {
    RVA_CAMA = 0x1EDBCC0, RVA_SERVICES = 0x1998300, RVA_VT_NXD3DIMPL = 0x10FAC50, RVA_VT_NXD3D12DRV = 0x10FAF58,
    RVA_VT_CAM11C = 0x1143850, RVA_VT_CAM11F = 0x1183C60,
    RVA_GAMETICK = 0x4C49E0, RVA_PRESENT = 0x434CC0, RVA_SETLOCAL = 0x806E50, RVA_SLOT43 = 0x807D10,
    RVA_SETPROJRECT = 0x8068E0, RVA_FRAMEVIEW = 0x5A07F0,
    RVA_SECVIEW_COUNT = 0x339E5B0, RVA_LIST2_COUNT = 0x339BE98,
    RVA_FRAMECTR = 0x19F1A50,
    RVA_RVSUBMIT = 0x9B6FE0, RVA_SECREG = 0x339E498, RVA_LIST2 = 0x339BE38,
    // R1 render-graph seams (STEREO_ROUTE session 6/7)
    RVA_RV_CTOR = 0x9B2770, RV_SIZE = 0x1600, RVA_RV_PREP = 0x9B6FB0, RVA_RV_POST = 0x9B5180, RVA_FV_LIST = 0x5A0540,
    RVA_RNDOBJ = 0x34A2BC0, RVA_RV_ACTIVATE = 0x9E5DE0,
    RVA_SUBMITGRAPH = 0x4D7EB0, RVA_SCHED = 0x1A0AB50, RVA_FRAMEALLOC = 0x1EBAE48, RVA_FN_ALLOC = 0x576E80, RVA_FN_RETIRE = 0x4DA510,
    RVA_FN_QUEUEGROUP = 0x4DAC80, RVA_FN_LISTPUSH = 0x5764B0, RVA_FN_CTXJOB = 0x4DADF0,
    RVA_VC_END = 0x77F340, RVA_SLPASS = 0x60F9E0, RVA_FN_BUFINIT = 0x7E1E80, RVA_FN_BUFFINISH = 0x7E2D70, RVA_FN_RESREC = 0x501A10,
    RVA_SLOT_UPS_OUT = 0x1EBA130, RVA_SLOT_UPS_IN = 0x1EBA148,
    RVA_RCTX_BUILD = 0x9B2DD0, RVA_RCTX_FLUSH = 0x9B49E0,
    RVA_CAM_SETVPF = 0x806BA0, RVA_RES_TRANSITION_IMPL = 0x4FDED0, RVA_RES_TABLE = 0x1487D38, RVA_RES_MULTI = 0x1487EA8,
    RVA_RES_TRANSITION = 0x4FDD50, RVA_CTX_FLUSHBARRIERS = 0x4FE780, RVA_SWAPCHAIN_GET = 0x4093C0, RVA_RV_SNAPSHOT = 0x9B6AE0, RVA_PASS0_PRE = 0x60EDC0,
    RVA_CLOCK_UPDATE = 0x56ED80, RVA_CLOCK = 0x1489DF0, RVA_CAM_TEMPORAL = 0x808EF0, RVA_CAMS_STOREPREV = 0x8085D0, RVA_RV_OCC = 0x9E5350, RVA_OCC_VIEW = 0x1EDBD70, RVA_OCC_VIEWFN = 0x9E5960, RVA_AUDIO_LISTENER = 0x5BAC80, RVA_CAMB = 0x1EDBCC8, RVA_AUDIO_EMITTER = 0x5BC780, RVA_AUDIO_W2A = 0x20A1CD0, RVA_OCC_SLOTS = 0x3389240, RVA_OCC_DIMS = 0x33891A4, RVA_PARITY = 0x148A3F8, RVA_OCC_ENABLE = 0x1804010, RVA_AA_RESOLVE = 0x614EB0, RVA_AA_VELOCITY = 0x614720, RVA_TAA_DRAW = 0x7E7D10, RVA_FRAMEID = 0x19F1A50, RVA_AA_OBJ = 0x3376280, RVA_SL_CONSTS = 0x807140, RVA_NGX_CREATE = 0x4B91B0, RVA_NGX_EVAL = 0x4B9300, RVA_NGX_RELEASE = 0x4B96B0,
    RVA_PASSJOB = 0x9B55B0, RVA_PASSBODY = 0x9B55D0, RVA_INLINE_RET = 0x9B5431, RVA_PASSTAB = 0x1210930, RVA_FN_NOP = 0x3FCBD0, NPASS = 69,
    RVA_RT_BIND = 0x7E34E0, RVA_SC_COMPUTESIZES = 0x46C840, RVA_POSTSCCMD = 0x4277D0, RVA_WND_PUMP = 0x446530, RVA_APPLY_DISPLAY = 0x421ED0, RVA_SC_RECREATE = 0x46E3D0, RVA_HERO = 0x34C6F20, RVA_ANIM_JOB = 0xB2DBC0, RVA_SERVICES_ROOT = 0x1998300, RVA_CTX_FLUSHRT = 0x4FEA80, RVA_FLUSHRT_DSV = 0x4FED6D, RVA_DSV_INC = 0x1CDFBD8, RVA_DSV_BASE = 0x1CDFB58, RVA_CTX_VIEWPORT = 0x50ECA0, RVA_CTX_SCISSOR = 0x50EC60, RVA_MOVIE_DRAW = 0x7EAF80, RVA_UI_TARGET = 0x3377510, RVA_WORLD_ON = 0x5A0450, RVA_UI60_PRE = 0x6E3790, RVA_PASS65_PRE = 0x610460, RVA_BATCH_DRAW = 0x5681A0, RVA_FADE = 0x149AF80, RVA_UI_WORLDDRAW = 0x6E37D0, RVA_STR_LETTERBOX = 0x1114870, RVA_SVC_DISPLAY = 0x409450,   // Reflex frame id: incremented first thing in GameTick; Present tags desc[0] with it
};

// ---------------- XInput forwarding ----------------
static HMODULE g_real;
static FARPROC xreal(const char *name) {
    if (!g_real) {
        wchar_t p[MAX_PATH]; UINT n = GetSystemDirectoryW(p, MAX_PATH - 20);
        wcscpy(p + n, L"\\xinput1_4.dll"); g_real = LoadLibraryW(p);
    }
    return g_real ? GetProcAddress(g_real, name) : nullptr;
}
typedef u64(WINAPI *FG)(u64, u64, u64, u64);
#define FWD(name, err) extern "C" u64 WINAPI name(u64 a, u64 b, u64 c, u64 d) { FG f = (FG)xreal(#name); return f ? f(a, b, c, d) : (err); }
#define FWDORD(fn, ord, err) extern "C" u64 WINAPI fn(u64 a, u64 b, u64 c, u64 d) { FG f = (FG)xreal((const char *)(uintptr_t)(ord)); return f ? f(a, b, c, d) : (err); }
// xr=4: during the frozen (right-eye) tick the game must not consume a pad change it cannot act on -> it is shown
// the previous state again and sees the change one tick later, on the next simulated (left-eye) tick.
static std::atomic<int> g_freezeNow{0};
static u8 g_padLast[2][4][16]; static bool g_padHave[2][4];
static bool g_playFlat = false;   // 0.18.24 play_mode=flat
static int g_audioHead = 1;        // 0.18.25 audio_head
// 0.18.0: VR controllers merge into user 0 (vrin::padHook, set once the mod is up; null = pure passthrough)
static bool (*g_padHook)(DWORD user, u8 *state, bool realOk) = nullptr;
static void (*g_rumbleHook)(DWORD user, const void *vib) = nullptr;
static bool (*g_capsHook)(DWORD user, u8 *caps) = nullptr;
static u64 padGet(int which, FARPROC f, u64 a, u64 b, u64 c, u64 d) {
    DWORD i = (DWORD)a; u8 *st = (u8 *)b;
    if (i < 4 && st && g_freezeNow.load() && g_padHave[which][i]) { memcpy(st, g_padLast[which][i], 16); return 0; }
    u64 r = f ? ((FG)f)(a, b, c, d) : 1167;
    if (g_padHook && i == 0 && st && g_padHook(i, st, r == 0)) r = 0;
    if (r == 0 && i < 4 && st) { memcpy(g_padLast[which][i], st, 16); g_padHave[which][i] = true; }
    return r;
}
extern "C" u64 WINAPI XInputGetState(u64 a, u64 b, u64 c, u64 d) { return padGet(0, xreal("XInputGetState"), a, b, c, d); }
extern "C" u64 WINAPI Ord100(u64 a, u64 b, u64 c, u64 d) { return padGet(1, xreal((const char *)(uintptr_t)100), a, b, c, d); }
extern "C" u64 WINAPI XInputSetState(u64 a, u64 b, u64 c, u64 d) {
    if (g_rumbleHook && (DWORD)a == 0 && b) g_rumbleHook((DWORD)a, (const void *)b);
    FG f = (FG)xreal("XInputSetState"); const u64 r = f ? f(a, b, c, d) : 1167;
    return (r != 0 && g_capsHook && (DWORD)a == 0 && g_capsHook(0, nullptr)) ? 0 : r;   // the virtual pad takes rumble too
}
extern "C" u64 WINAPI XInputGetCapabilities(u64 a, u64 b, u64 c, u64 d) {
    FG f = (FG)xreal("XInputGetCapabilities"); u64 r = f ? f(a, b, c, d) : 1167;
    if (r != 0 && g_capsHook && (DWORD)a == 0 && c && g_capsHook(0, (u8 *)c)) r = 0;
    return r;
}
FWD(XInputEnable, 0)
FWD(XInputGetAudioDeviceIds, 1167) FWD(XInputGetBatteryInformation, 1167) FWD(XInputGetKeystroke, 1167)
FWDORD(Ord101, 101, 1167) FWDORD(Ord102, 102, 1167) FWDORD(Ord103, 103, 1167) FWDORD(Ord104, 104, 1167) FWDORD(Ord108, 108, 1167)

// ---------------- logging ----------------
static HANDLE g_log = INVALID_HANDLE_VALUE; static CRITICAL_SECTION g_logcs; static std::atomic<int> g_lines{0};
static wchar_t g_dir[MAX_PATH];
static void logf(const char *fmt, ...) {
    if (g_log == INVALID_HANDLE_VALUE || g_lines.load() > 200000) return;
    char b[2048]; int n = snprintf(b, sizeof b, "[%llu] ", (unsigned long long)GetTickCount64());
    va_list ap; va_start(ap, fmt); n += vsnprintf(b + n, sizeof b - n - 3, fmt, ap); va_end(ap);
    if (n > (int)sizeof b - 3) n = sizeof b - 3;
    b[n++] = '\r'; b[n++] = '\n';
    EnterCriticalSection(&g_logcs); DWORD w; WriteFile(g_log, b, n, &w, nullptr); LeaveCriticalSection(&g_logcs);
    g_lines++;
}
static void logflush() { if (g_log != INVALID_HANDLE_VALUE) FlushFileBuffers(g_log); }

// ---------------- memory helpers ----------------
static u8 *g_base;
// 0.18.61 (owner: "the frame rate dropped often when I grabbed my sword"; log: 55-60 ticks/s sheathed, 21-37 while the katana was in
// hand - lower the more characters were around): every per-frame check of an enemy (alive, hostile, position) went through VirtualQuery,
// and VirtualQuery is a kernel call whose cost grows with the size of the memory region asked about (it measures the run of pages with the
// same attributes) - the game's object heaps are huge regions. Each thread now keeps the last 32 regions it asked about for 200 ms.
static std::atomic<u64> g_jinHurtAt{0}, g_jinDownAt{0};
static std::atomic<u64> g_drawChordAt{0}, g_muteJinUntil{0}, g_nMuted{0};   // 0.18.65 g_muteJinUntil: Jin's own sounds silenced (the game's sheathe / our hidden draws)   // 0.18.63 the last draw / sheathe toggle sent to the game (it is a toggle: never twice within 2.5 s)
static int g_pairCap = 58, g_menuCap = 60, g_pairCapMax = 58; static bool g_paceAdaptive = true; static std::atomic<u64> g_pairTicks{0}, g_capDowns{0}, g_capUps{0}; static std::atomic<float> g_hostFps{0}, g_displayHz{0};   // 0.18.62 the game's pace in VR (capPace)   // 0.18.61 Jin's last health loss / when he was at 1 health or less (the UI's hit flash and death fade)
struct RgnC { uintptr_t b = 0, e = 0; u64 t = 0; u32 ttl = 0; bool ok = false; };
static __thread RgnC t_rgn[32]; static __thread int t_rgnN = 0;
static std::atomic<u64> nVQ{0}, nVQHit{0};
static bool readable(const void *p, size_t n) {
    if (!p) return false; const uintptr_t a = (uintptr_t)p, z = a + n; if (z < a) return false; const u64 now = GetTickCount64();
    for (int i = 0; i < 32; i++) { const RgnC &r = t_rgn[i]; if (r.e && a >= r.b && a < r.e && now - r.t < r.ttl) { nVQHit++; return r.ok && z <= r.e; } }
    MEMORY_BASIC_INFORMATION m; nVQ++; if (!VirtualQuery(p, &m, sizeof m)) return false;
    const bool ok = !(m.State != MEM_COMMIT || (m.Protect & PAGE_GUARD) || (m.Protect & PAGE_NOACCESS));
    RgnC &w = t_rgn[t_rgnN++ & 31]; w.b = (uintptr_t)m.BaseAddress; w.e = (uintptr_t)m.BaseAddress + m.RegionSize; w.t = now; w.ok = ok;
    // 0.18.63 (review): heap pages can be given back to Windows at any time - a cached "readable" heap region is trusted for 50 ms only
    // (a few frames: the hot per-frame reads still hit the cache), the game's own image (never unmapped) for 1 s
    w.ttl = (ok && m.Type == MEM_IMAGE) ? 1000u : 50u;
    return ok && (const u8 *)p + n <= (const u8 *)m.BaseAddress + m.RegionSize;
}
template <class T> static T rd(const void *p) { T v{}; if (readable(p, sizeof(T))) memcpy(&v, p, sizeof(T)); return v; }
static u64 camA() { return *(volatile u64 *)(g_base + RVA_CAMA); } // module .data, always mapped
static bool isCam(u64 o) {
    if (!o || !readable((void *)o, 0x8D0)) return false; u64 vt = *(u64 *)o;
    return vt == (u64)(g_base + RVA_VT_CAM11C) || vt == (u64)(g_base + RVA_VT_CAM11F);
}

// ---------------- guards ----------------
struct Sig { const char *name; u32 rva; const char *sig; };
static const Sig kSigs[] = {
    {"GameTick", RVA_GAMETICK, "48 89 5C 24 08 57 48 81 EC C0 00 00 00 FF 05 ?? ?? ?? ??"},
    {"NxDXGI_Present", RVA_PRESENT, "40 55 53 56 57 41 54 41 56 41 57 48 8D AC 24 80 FD FF FF"},
    {"Cam_SetLocalMatrix", RVA_SETLOCAL, "0F 10 02 0F 11 81 00 01 00 00 0F 10 4A 10 0F 11 89 10 01 00 00"},
    {"Cam_vUpdateFrustum43", RVA_SLOT43, "48 8B C4 48 81 EC 08 01 00 00 0F 29 70 E8 0F 29 78 D8 44 0F 29 40 C8"},
    {"Cam_SetProjRect", RVA_SETPROJRECT, "40 53 48 83 EC 20 0F 10 02 48 8B 01 48 8B D9 0F 11 81 94 06 00 00"},
    {"FrameView_5A07F0", RVA_FRAMEVIEW, "40 53 55 56 57 41 54 48 83 EC 40 48 8B 05 ?? ?? ?? ??"},
    {"RenderView_global_load", 0x60FA11, "48 8B 35 ?? ?? ?? ?? 4C 8D 45 50 0F 57 C0 66 C7 45 70 00 00"},
    {"D3D12_CreateQueue_site", 0x43846F, "41 8B C7 44 89 65 A0 D3 E0 4C 8D 05 ?? ?? ?? ??"},
    {"RV_Submit", RVA_RVSUBMIT, "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 8B FA 48 8B F1 F6 C2 01 74"},
    {"RV_Ctor", RVA_RV_CTOR, "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 48 89 7C 24 20 41 56 48 83 EC 20 33 ED C7 41 08 05"},
    {"RV_Prep", RVA_RV_PREP, "40 53 48 83 EC 20 48 8B 05 53 50 52 01 48 8B D9 48 89 81 B0 13 00 00 E8"},
    {"RV_Post", RVA_RV_POST, "48 89 5C 24 18 55 56 57 41 54 41 55 41 56 41 57 48 81 EC E0 00 00 00 48 8B 05 F2 2F FD 00 48 33"},
    {"VC_FrameEnd", RVA_VC_END, "40 55 53 41 54 48 8D 6C 24 D0 48 81 EC 30 01 00 00 48 8B 05 38 8E 20 01 48 33 C4 48 89 45 10 4C"},
    {"StreamlinePass", RVA_SLPASS, "40 55 56 57 41 55 41 56 48 8D AC 24 00 FF FF FF 48 81 EC 00 02 00 00 44 0F 29 84 24 D0 01 00 00"},
    {"Pass_Job", RVA_PASSJOB, "83 B9 50 2A 00 00 0F 48 8D 05 82 05 00 00 48 8D 15 5B 06 00 00 48 0F 45 D0 E9 02 00 00 00 CC CC"},
    {"Pass_NopCallback", RVA_FN_NOP, "33 C0 C3"},
    {"Cam_SetViewProjFrustum", RVA_CAM_SETVPF, "48 8B C4 48 89 58 10 55 56 57 48 81 EC D0 00 00 00 0F 29 70 D8 0F 29 78"},
    {"Res_TransitionImpl", RVA_RES_TRANSITION_IMPL, "48 89 5C 24 10 57 48 83 EC 40 48 8B D9 41 8B F8 48 8B 09 4C 8B DA 48 83"},
    {"Res_Transition", RVA_RES_TRANSITION, "48 89 6C 24 10 48 89 74 24 18 57 48 83 EC 20 48 8B E9 41 8B F0 48 8D 0D"},
    {"Ctx_FlushBarriers", RVA_CTX_FLUSHBARRIERS, "40 53 48 83 EC 20 8B 91 78 0C 00 00 48 8B D9 85 D2 75 0A 39"},
    {"Swapchain_Get", RVA_SWAPCHAIN_GET, "48 8B 05 39 EF 58 01 48 8B 00 C3"},
    {"RV_Snapshot", RVA_RV_SNAPSHOT, "40 57 48 83 EC 40 48 8B F9 48 89 5C 24 58 48 81 C1 80 05 00 00 0F 29 74"},
    {"Pass0_pre", RVA_PASS0_PRE, "48 89 5C 24 10 57 48 83 EC 30 48 8B F9 0F 29 74 24 20 48 8D 0D 37 0D 19"},
    {"RCtx_Build", RVA_RCTX_BUILD, "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 48 89 7C 24 20 41 54 41 56 41 57 48 83 EC 20 48 8B"},
    {"RCtx_FlushDraws", RVA_RCTX_FLUSH, "4C 8B DC 55 56 57 48 81 EC D0 00 00 00 48 8B 05 9C 37 FD 00 48 33 C4 48 89 84 24 B0 00 00 00 8B"},
    {"Clock_Update", RVA_CLOCK_UPDATE, "48 89 5C 24 18 57 48 83 EC 50 48 89 6C 24 60 48 8B D9 48 89 74 24 68 0F"},
    {"Cam_TemporalUpdate", RVA_CAM_TEMPORAL, "48 8B C4 48 89 58 10 57 48 81 EC B0 01 00 00 0F 29 70 E8 0F 29 78 D8 44"},
    {"Cam_StorePrev", RVA_CAMS_STOREPREV, "0F 10 81 40 01 00 00 8B 81 70 06 00 00 0F 11 81 C0 01 00 00 0F 10 89 50"},
    {"NGX_D3D12_CreateFeature", RVA_NGX_CREATE, "48 89 6C 24 10 48 89 74 24 18 48 89 7C 24 20 41 56 48 83 EC 20 48 83 3D"},
    {"NGX_D3D12_EvaluateFeature_C", RVA_NGX_EVAL, "48 83 EC 38 48 8B C1 48 8B 0D 02 6F 52 01 48 85 C9 75 0A B8 07 00 D0 BA"},
    {"NGX_D3D12_ReleaseFeature", RVA_NGX_RELEASE, "48 89 5C 24 08 57 48 83 EC 20 48 8B 1D 57 6B 52 01 48 8B F9 48 85 DB 75"},
    {"Cam_BuildSLConstants", RVA_SL_CONSTS, "48 89 5C 24 10 55 48 8D AC 24 00 FD FF FF 48 81 EC 00 04 00 00 48 8B 05"},
    {"AA_Resolve", RVA_AA_RESOLVE, "40 56 57 48 81 EC C8 00 00 00 48 8B 05 CF 32 37 01 48 33 C4 48 89 84 24"},
    {"AA_Velocity", RVA_AA_VELOCITY, "48 89 5C 24 20 55 56 57 41 56 41 57 48 81 EC 80 00 00 00 48 8B 05 56 3A"},
    {"TAA_Draw", RVA_TAA_DRAW, "40 55 53 56 57 41 54 41 56 41 57 48 8D 6C 24 F9 48 81 EC 90 00 00 00 0F"},
    {"RV_OcclusionSetup", RVA_RV_OCC, "48 8B C4 53 41 54 48 81 EC 88 00 00 00 4C 8B 25 5C 69 4F 01 48 8B D9 44"},
    {"Pass_Body", RVA_PASSBODY, "4C 8B DC 49 89 53 10 49 89 4B 08 55 53 56 41 56 49 8D 6B A1 48 81 EC C8 00 00 00 48 63 81 50 2A"},
    {"RV_Post_inline_pass_site", RVA_INLINE_RET - 5, "E8 9F 01 00 00 4C 89 A3 58 2A 00 00 40 88 BB 6D 2A 00 00"},
    {"Jobs_SubmitGraph", RVA_SUBMITGRAPH, "4C 8B DC 56 41 57 48 83 EC 58 8B 81 60 5C 2B 00 4C 8B FA 48 8B F1 85 C0 0F 84 1D 01 00 00 8B 81"},
    {"RV_Activate", RVA_RV_ACTIVATE, "8B 42 08 45 33 C0 A9 51 20 00 00 75 4D A8 A2 74 0B 41 B8 01 00 00 00 E9 44 00 00 00 A8 08 74 0B"},
    {"RT_Bind", RVA_RT_BIND, "4C 8B DC 56 41 55 41 57 48 81 EC A0 00 00 00 48 8B 05 9A 4C 1A 01 48 33 C4 48 89 44 24 68 49 89"},
    {"Ctx_SetViewport", RVA_CTX_VIEWPORT, "0F 57 C0 8B C2 4C 8D 91 10 03 00 00 C7 81 20 03 00 00 00 00 00 00 44 2B CA C7 81 24 03 00 00 00"},
    {"Ctx_SetScissor", RVA_CTX_SCISSOR, "8B 44 24 28 89 81 34 03 00 00 48 8B 01 44 89 81 2C 03 00 00 4C 8D 81 28 03 00 00 41 89 10 BA 01"},
    {"Movie_Draw", RVA_MOVIE_DRAW, "48 8B C4 55 53 56 57 41 56 41 57 48 8D 68 98 48 81 EC 38 01 00 00 0F 29 70 B8 0F 29 78 A8 44 0F"},
    {"World_IsRendered (+0x80F09)", RVA_WORLD_ON, "0F B6 81 09 0F 08 00 C3"},
    {"UI_Pass60_pre (binds UI target 0x143377510)", RVA_UI60_PRE, "40 53 48 83 EC 20 41 B9 FF FF FF FF 48 8D 15 6D 3D C9 02 45 33 C0 48 8B D9 E8 32 FD 0F 00"},
    {"SC_ComputeSizes (window/output/render size records of the swapchain object)", RVA_SC_COMPUTESIZES, "40 55 53 56 57 41 54 41 55 41 56 41 57 48 8D 6C 24 E1 48 81 EC D8 00 00 00"},
    {"NxApp_PostSCCmd (synchronous swapchain-thread command; 2 = apply display)", RVA_POSTSCCMD, "48 89 5C 24 10 57 48 83 EC 20 48 83 B9 F0 06 00 00 00"},
    {"SC_Recreate (SCImpl vt+0x20: destroys and creates the swapchain)", RVA_SC_RECREATE, "48 89 5C 24 08 57 48 83 EC 20 48 8B DA 48 8B F9 E8 ?? ?? ?? ?? 4C 8D 05 ?? ?? ?? ??"},
    {"Occ_View (occlusion view getter: obj+0x40 when obj+0x34)", RVA_OCC_VIEWFN, "33 C0 48 8D 51 40 38 41 34 48 0F 45 C2 C3"},
    {"Audio_ListenerFromCamera (Wwise listener 0-2 from CAM-B +0x890)", RVA_AUDIO_LISTENER, "4C 8B DC 55 41 56 49 8D 6B 98 48 81 EC 58 01 00 00 48 8B 05"},
    {"Audio_EmitterUpdate (one Wwise emitter: id +0x18, source +0x28)", RVA_AUDIO_EMITTER, "48 89 5C 24 20 55 56 57 48 8D 6C 24 90 48 81 EC 70 01 00 00"},
    {"RV_OcclusionSetup caller: frame 0x141EBD2C0 + 0x1EAB0 = the occlusion view", 0x5A08A6, "48 8D AE B0 EA 01 00 48 8B CD E8"},
    {"Anim_Job (character animation job: job+0xE8 node, job+0xA8 render pose)", RVA_ANIM_JOB, "40 55 48 8D AC 24 50 FF FF FF 48 81 EC B0 01 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 85 A0 00 00 00 48 8B 51 48 48 8B 41 40"},
    {"Anim_Setup stores the node at job+0xE8", 0x9BB7B4, "48 89 B7 E8 00 00 00"},
    {"Anim_Setup stores the current render pose node+0x508[cur] at job+0xA8", 0x9BB8D6, "48 8B 84 C6 08 05 00 00 48 89 87 A8 00 00 00"},
    {"Skin palette job reads node+0x2DA / node+0x508[cur] / node+0x280", 0x59EC15, "48 0F BE 82 DA 02 00 00"},
    {"Skin palette job: current render pose", 0x59EC38, "4C 8B 9C CA 08 05 00 00"},
    {"Skin palette job: joint count node+0x280 - 1", 0x59EC42, "8B 92 80 02 00 00 FF CA"},
    {"GameWindow_Pump (main thread: queued window messages -> display changes)", RVA_WND_PUMP, "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 41 54 41 55 41 56 41 57 48 83 EC 50 0F 29 74 24 40"},
    {"NxApp_ApplyDisplay (the game's own display apply: flags, PostSCCmd 2, window + Reflex follow-ups)", RVA_APPLY_DISPLAY, "48 89 5C 24 08 48 89 74 24 10 48 89 7C 24 18 4C 89 74 24 20 55 48 8D AC 24 40 FE FF FF"},
    {"Ctx_FlushRT (binds the colour/depth targets: OMSetRenderTargets)", RVA_CTX_FLUSHRT, "4C 8B DC 41 57 48 83 EC 70 F6 81 40 03 00 00 01 4C 8B F9 0F 84 3C 03 00 00"},
    {"Ctx_FlushRT depth handle (ctx+0x33C * [0x141CDFBD8] + [0x141CDFB58])", RVA_FLUSHRT_DSV, "41 8B 8F 3C 03 00 00 0F AF 0D 5D 0E 7E 01 48 03 0D D6 0D 7E 01 48 85 D2"},
    {"Pass65_pre (full-screen fade)", RVA_PASS65_PRE, "48 83 EC 28 48 8B D1 48 8D 0D 12 AB E8 00 E8 8D 7F 11 00 33 C0 48 83 C4"},
    {"Display service getter (services+0xC8)", RVA_SVC_DISPLAY, "48 8B 05 A9 EE 58 01 48 8B 80 C8 00 00 00 C3"},
    {"\"Letterbox\" display mode name", RVA_STR_LETTERBOX, "4C 65 74 74 65 72 62 6F 78 00"},
    {"Batch_Draw (renderer->draw per record batch)", RVA_BATCH_DRAW, "40 53 55 57 41 56 48 83 EC 58 48 8B 05 DF FF 41 01 48 33 C4 48 89 44 24"},
    {"UI_WorldDraw (world-projected UI elements)", RVA_UI_WORLDDRAW, "48 8B C4 48 89 58 18 55 56 57 41 54 41 55 41 56 41 57 48 8D A8 68 F2 FF"},
    {"FrameView_ViewList", RVA_FV_LIST, "40 53 57 41 55 48 83 EC 40 0F B6 05 79 DC 99 01 48 89 6C 24 70 3C 02 48 8B A9 08 EA 01 00 0F 93"},
};
static int hexv(char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'A' && c <= 'F' ? c - 'A' + 10 : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1; }
static bool checkSig(const Sig &s) {
    const u8 *p = g_base + s.rva; if (!readable(p, 64)) return false;
    int i = 0; for (const char *q = s.sig; *q;) {
        while (*q == ' ') q++; if (!*q) break;
        if (*q == '?') { q += 2; i++; continue; }
        if (p[i] != (u8)(hexv(q[0]) * 16 + hexv(q[1]))) return false; q += 2; i++;
    }
    return true;
}
static void sha256File(const wchar_t *path, char *out) {
    out[0] = 0; BCRYPT_ALG_HANDLE alg = nullptr; BCRYPT_HASH_HANDLE h = nullptr; static u8 data[1 << 16]; u8 dg[32];
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) == 0 && BCryptCreateHash(alg, &h, nullptr, 0, nullptr, 0, 0) == 0) {
        DWORD r; while (ReadFile(f, data, sizeof data, &r, nullptr) && r) BCryptHashData(h, data, r, 0);
        if (BCryptFinishHash(h, dg, 32, 0) == 0) for (int i = 0; i < 32; i++) sprintf(out + 2 * i, "%02x", dg[i]);
        BCryptDestroyHash(h);
    }
    if (alg) BCryptCloseAlgorithmProvider(alg, 0);
    CloseHandle(f);
}

// ---------------- config ----------------
struct Config { int xr = 0; int census = 1; int hotkeys = 1; float unitsPerM = 100.0f; int headPos = 1; int level = 1; int queueFallback = 1; int widen = 1; int histReset = 0; int histMode = 0; int mirrorLeft = 1; int dlssMode = 0; };
static Config g_cfg;
static std::atomic<int> m_cullHead{1}; static std::atomic<u64> c_cullHead{0};   // F7 (xr=4): world copy +0x850 follows the eye camera
static float g_poseLeadMs = -1.0f; static int g_gameGpuPrio = 1, g_renderSync = 0, g_foliageOcc = 2;
static float g_fastTurnDeg = 2.0f, g_fastTurnDegLeft = 2.0f; static int g_fastTurnMode = 1; static float g_fovMarginDeg = 3.0f;
struct UiCfg { int hud = 1; float hudDeg = 55.0f, hudDist = 1.6f, hudDown = 3.0f; int theater = 1; float thDist = 3.0f, thWidth = 3.2f, thHeight = 0.0f; int thBackdrop = 0; int thCrop = 0; int flatRender = 1; u32 pauseMask = 0x80; int layer = 1; int premul = 1; int titleReason = 27; int follow = 1; float followDeg = 35.0f, followSpeed = 90.0f; int loadTheater = 1, menuTheater = 1, crop = 0; };
static UiCfg g_ui; static int g_hostStart = 1, g_renderSize = 2304; static float g_uiAspect = 16.0f / 9.0f;
// 0.17.0 first person (playbook ch01/ch02/ch12; references: bldbrne-vr, BetterVR, FRIK - anchor on the body root, not the head bone)
struct FpCfg { float armsAboveHeadCm = 0.0f; int armsHoldMs = 1200; int armsRaised = 0, armsRiding = 1; int on = 1; float nearCm = 0.0f, stanceMaxCm = 25.0f; int standTall = 0; float eyeUpCm = -1.0f, eyeFwdCm = 1.0f, eyeLeftCm = -3.0f, vertTau = 0.35f, horizTau = 1.0f, lean = 1.0f, maxCamDistM = 8.0f, hideRadiusCm = 22.0f; int hide = 1;
                int idleHold = 1, stanceSquare = 1, fingers = 1, indexTrigger = 1; float gripFull = 0.35f, fingerSign = 1.0f, fingerRelaxed = 5.0f, fingerTween = 0.15f, indexTween = 0.07f, forearmTwist = 1.0f, armStretch = 0.10f; int stab = 1, roomscale = 0, headRigid = 1; float bodyTurnS = 0.35f; int bodySquareOnStop = 1; float rsDeadCm = 12.0f, rsStopCm = 5.0f, stabMinHz = 1.2f, stabBeta = 10.0f, stabRotMinHz = 0.9f, stabRotBeta = 3.0f, stabDeadMm = 1.5f, stabDeadDeg = 0.25f; int bodyFollow = 1, bodyAttach = 1, bodyHead = 1, arms = 1; float headDeadDownDeg = 30.0f, headDeadDeg = 20.0f, headTau = 0.08f, moveTau = 0.15f, underMaxCm = 60.0f, underTau = 0.1f; int bodyUnder = 1; float idleSettle = 0.5f, handBackCm = 7.0f, handUpCm = 3.0f, handSideCm = 0.0f, handPitch = 0.0f, handYaw = 0.0f, handRoll = 0.0f; float bodyStartDeg = 80.0f, bodyFullDeg = 130.0f, bodyTurnSpeed = 1440.0f, bodyMaxSpeed = 8.0f; };
static FpCfg g_fp;   // 0.12.0 UI/HUD/video settings (playbook ch04)
static std::atomic<bool> g_flatRender{false};   // 0.13.0 xr=4: this tick is a flat context rendered as a normal flat frame (game thread decides at tick start)
static std::atomic<int> g_eyeSwap{0}; static int g_eyeSwapLatched = 0;
static std::atomic<int> g_mblurOff{1};   // 0.18.6: hold the game's motion blur at 0 while in VR
namespace mblur { static void tick(bool force = false); static void restore(); }
namespace obs { static void tick(); static void mark(); }
namespace cbt { static void tick(); }   // 0.18.47 the katana's blows, sent after the game's tick
namespace fp { static void snapRestore(); }
namespace shade { static void tick(bool force = false); static void restore(); static std::atomic<int> mode{1}; }
namespace stipple { static void tick(bool fpOn); }
static std::atomic<int> g_flatCut{0};           // camera cuts still to force after a flat <-> stereo switch
static float g_ipd = 0.064f;   // metres between the eyes (from the runtime's eye poses; game thread)
static void loadConfig() {
    wchar_t ini[MAX_PATH]; swprintf(ini, MAX_PATH, L"%ls\\gotvr.ini", g_dir);
    if (GetFileAttributesW(ini) == INVALID_FILE_ATTRIBUTES) {
        const char *def =
            "; GoTVR 0.6 settings. Restart the game after editing.\r\n"
            "[gotvr]\r\n"
            "; audio_self_centre (1): Jin's own sounds within audio_self_radius_cm (35) of your head (his voice, gear at his head) are heard centred, audio_self_ahead_cm (25) in front of you; 0 = where the game puts them\r\n"
            "; audio_head (xr=4): 1 = 3D audio - the game's sound listener is your head in first person, so sounds stay put in the world when you turn (default); 0 = the game's camera\r\n"
            "; play_mode: vr (default) = the VR mod; flat = play on the monitor with the game's own camera and pad while the log records Jin's state and flags (F8 = MARK)\r\n"
            "; xr: 0 = off (flat game, census only)\r\n"
            ";     1 = mono 6DoF: camera follows your head, same image in both eyes (comfort fallback)\r\n"
            ";     2 = stereo 6DoF, alternate-eye (left eye on even frames, right eye on odd frames)\r\n"
            ";     3 = NATIVE stereo 6DoF, second render per frame (experimental, known artifacts)\r\n"
            ";     4 = SYNCHRONIZED stereo 6DoF: left and right eye from the SAME simulation tick (game time frozen for the right eye)\r\n"
            ";     9 = headset test pattern only (red left eye / blue right eye), game untouched\r\n"
            "xr=0\r\n"
            "census=1\r\n"
            "hotkeys=1\r\n"
            "; game units per metre for head/eye position; 100 = centimetres\r\n"
            "units_per_m=100\r\n"
            "; 1 = apply head position (6DoF), 0 = rotation only\r\n"
            "head_position=1\r\n"
            "; 1 = remove the game camera's pitch/roll (your head owns pitch and roll), 0 = keep it\r\n"
            "level_camera=1\r\n"
            "; history_mode (xr=4): 1..7 (see README), default 7 (each eye culled by its own view); 4 = cross-eye (0.18.10); F6 cycles them, the number shows as squares in the headset\r\n"
            "; monitor_left_eye (xr=4): 1 = the game window shows the left eye only (default), 0 = every rendered frame\r\n"
            "; cull_follows_head (xr=4): 1 = distance/shadow culling uses your eye camera (default; F7 toggles), 0 = the game camera\r\n"
            "; fov_margin_deg (xr=4): render each eye this many degrees wider than the lens on every side (3.0) so fast turns never show an edge\r\n"
            "; dlss_per_eye (xr=4): 0 = off (default; use the game's TAA, DLSS is not supported in VR), 1/2 = experimental per-eye DLSS\r\n"
            "; hud (xr=4): 1 = HUD and menus drawn on a flat panel in front of you at the right aspect ratio (default), 0 = stretched over the lens\r\n"
            "; hud_width_deg / hud_distance / hud_down_deg: panel width in degrees (55), distance it converges at in metres (1.6), centre below eye level (3); F9 cycles sizes\r\n"
            "; theater (xr=4): 1 = videos and world-less menus on a world-locked 16:9 screen (default), 0 = never; F10 toggles auto/always (0.18.2: never only from the ini)\r\n"
            "; theater_distance / theater_width / theater_height: screen distance (3.0 m), width (3.2 m), centre height vs your eyes (0.0 m)\r\n"
            "; theater_backdrop: 0 = black room around the screen (default), 1 = the game world behind it\r\n"
            "; theater_flat_render: 1 = while the screen is up the game renders a normal flat frame (default), 0 = the stereo image\r\n"
            "; pause_channels: game time-scale reasons that put the game on the screen (bit mask, 0x80 = pausing menus)\r\n"
            "; hud_layer: 1 = the HUD on its own sharp layer (default; Ctrl+Alt+F1 toggles), 0 = drawn into the 3D image (0.14 panel)\r\n"
            "; hud_follow: 1 = the HUD/menu panel stays put in the room and only follows when you look away (default), 0 = fixed to your view\r\n"
            "; hud_follow_deg / hud_follow_speed: how far you can look away before it follows (35) and how fast it follows (90 deg/s)\r\n"
            "; loading_theater / menu_theater: loading screens / the start screen on the theatre screen (1 = default)\r\n"
            "; render_size (xr=4): square render size per eye in pixels (2304 default, 0.17.1; the game must be in WINDOWED mode), 0 = the game's own size; Ctrl+Alt+F2 cycles\r\n"
            "; ui_aspect (with render_size): the aspect the game lays out its HUD, menus and flat frames for (1.7778 = 16:9 default;\r\n"
            ";   the square image is shown at this aspect in the headset), 0 = square like the image\r\n"
            "; first_person (xr=4): 1 = first person (default; F5 toggles, Shift+F5 logs the hero skeleton), 0 = the game's third-person camera\r\n"
            "; fp_eye_up_cm / fp_eye_forward_cm / fp_eye_left_cm: your eyes relative to the top of the hero's head (-1 / 1 / -3 = 3 cm to his right; sideways they sit on the head's centre); fp_hide_head: 1 = hide the head (default), 0 = off\r\n"
            "; fp_body_follow: 1 = when Jin faces away from your forward direction (running backwards) his body is drawn facing it (default), 0 = off;\r\n"
            ";   fp_body_follow_start_deg / _full_deg (80 / 130), fp_body_turn_speed (1440 deg/s, 0 = instant), fp_body_follow_max_speed (8 m/s; off above it and while riding)\r\n"
            ";   fp_body_follow_items: 1 = the sword and other carried items turn with the body (default; the joint world matrices are turned), 0 = only the body\r\n"
            "; fp_body_follows_head: 1 = Jin's body turns to where you look once you look more than fp_body_head_deadzone_deg (20; fp_body_head_deadzone_down_deg 30 while you look down at yourself) away, standing and moving (default), easing round in fp_body_turn_s (0.35 s)\r\n"
            "; [controls] move_follows_head: 1 = the left stick's forward is always where the headset looks (default); 0 = taken when you start moving / snap turn / leave a menu\r\n"
            "; fp_arm_stretch (0.10): how far Jin's arm may stretch past its length to reach your hand (a fraction of the arm: 0.10 = 5 cm), in the upper arm / elbow; 0 = never\r\n"
            "; fp_forearm_twist (1): how much of the wrist's roll the forearm takes, shared along its twist joints like a real forearm (0 = off, the roll stays in the wrist)\r\n"
            "; fp_roomscale (0 = off, default since 0.18.26; 1 = on): when you step away from Jin (more than fp_roomscale_start_cm 12) and the left stick rests, Jin walks under your head (his real collider moves: dodge for real) until within fp_roomscale_stop_cm 5; 0 = off\r\n"
            "; fp_hand_stab (1): steadies the hands when held still (One Euro filter: fp_hand_stab_min_hz 1.2 = smoothing at rest, lower = steadier; fp_hand_stab_beta 10 = how fast it lets go when the hand moves; fp_hand_stab_rot_min_hz 0.9 / fp_hand_stab_rot_beta 3 for turning; fp_hand_stab_dead_mm 1.5 / fp_hand_stab_dead_deg 0.25 = a soft deadband: tremor smaller than that is held still)\r\n"
            "; fp_ao_off (xr=4): 1 = in first person the mod sets ambient occlusion OFF and screen-space shadows ON through the game's own settings apply (the see-through arms fix; default; Shift+F6 toggles), 0 = left as set in the menu\r\n"
            "; fp_near_cm (xr=4): the camera's near clip plane in first person (0 = the game's own 10 cm, default since 0.18.20; 2 = arms closer than 10 cm not cut open, but grass then draws over them; Shift+F7 toggles)\r\n"
            "; fp_stand_tall: 1 = when the game crouches him a little (the sword stance, up to fp_stand_tall_max_cm 25) he is drawn standing tall, feet planted (0 = off, default)\r\n"
            "; fp_arms: 1 = Jin's arms follow your controllers (full-body IK stage 1, default; Shift+F10 toggles), 0 = the game's animation\r\n"
            ";   fp_fingers (1): Jin's fingers detached from the animation (bind pose on the VR hands); fp_grip_fist_full (0.35): grip at which the fist is closed; fp_finger_index_trigger (1): the index also follows the trigger; fp_finger_bend_sign (1; -1 if the fingers bend backwards); fp_finger_relaxed_deg (5); fp_finger_tween_s (0.15): how long the fingers take to move to a new pose\r\n"
            ";   fp_idle_hold (1): standing still, Jin's idle animations are held still (fp_idle_settle_s 0.5: how long he stands before the pose is taken); fp_stance_square (1): that pose turned to face straight forward\r\n"

            ";   fp_hand_back_cm (7) / fp_hand_up_cm (3) / fp_hand_side_cm (0): where your wrist is from the controller grip - behind it, toward the back of the hand, toward the thumb (Ctrl+F1 / F2 / F3 change them 1 cm, Ctrl+Shift back); fp_hand_pitch_deg / _yaw_deg / _roll_deg (0): hand angle trims - F1 / F2 / F3 change them in 5-degree steps (Shift = back) and save them here\r\n"
            "; motion_blur_off (xr=4): 1 = the game's motion blur is set to 0 while in VR and restored on exit (default; playbook ch14: required for this stereo), 0 = leave it\r\n"
            "; host_start: 1 = start the headset session at the game's first frame (default; the launcher shows in Virtual Desktop / SteamVR), 0 = at once\r\n"
            "; letterbox_crop: 0 = the whole window image goes to the headset (default), 1 = only a 16:9 picture inside it (Ctrl+Alt+F3 toggles)\r\n"
            "; fast_turn_deg / fast_turn_mode (xr=4): head turn per eye frame pair above which culling memory is not trusted (2.0);\r\n"
            "; fast_turn_deg_left: the same for the left eye (2.0; 0.17.3 tried 0.6 - no effect on the pop-in, reverted)\r\n"
            "; pose_lead_ms (xr=4): predict the head pose this much past the display time: auto (default, the measured pipeline lag), 0 = off, 1..60 = fixed\r\n"
            "; game_gpu_priority_below_normal: 1 = the game yields the GPU to the headset streamer/compositor and GoTVRHost (default), 0 = leave it\r\n"
            "; foliage_occlusion (xr=4): 2 = grass/foliage culling without the previous frame's occlusion (default: no left-eye pop-in), 1 = the engine's own view, 0 = the per-eye one (0.17.4); Shift+F8 cycles\r\n"
                        "; eye_order_swap (xr=4): 0 = the simulated tick renders the left eye (default), 1 = the right eye (pop-in diagnostic; Shift+F9 toggles)\r\n"
            "; render_sync (xr=4): 0 = off (default; 0.17.4 measured no effect on the pop-in), 1 = the right-eye tick waits for the left-eye frame, 2 = every eye\r\n"
            ";   mode 1 = off during the turn (default), 2 = game's own memory during the turn, 0 = no special handling (F8 cycles)\r\n";
        HANDLE f = CreateFileW(ini, GENERIC_WRITE, 0, nullptr, CREATE_NEW, 0, nullptr);
        if (f != INVALID_HANDLE_VALUE) { DWORD w; WriteFile(f, def, (DWORD)strlen(def), &w, nullptr); CloseHandle(f); }
    }
    g_cfg.xr = GetPrivateProfileIntW(L"gotvr", L"xr", 0, ini);
    g_audioHead = GetPrivateProfileIntW(L"gotvr", L"audio_head", 1, ini) ? 1 : 0;   // 0.18.25 3D audio
    {   // 0.18.24 play_mode: vr (default) or flat (no headset, the game's own camera and pad, the observer logs Jin's state and flags)
        wchar_t pm[32]; GetPrivateProfileStringW(L"gotvr", L"play_mode", L"", pm, 32, ini);
        if (!pm[0]) { WritePrivateProfileStringW(L"gotvr", L"play_mode", L"vr", ini); wcscpy(pm, L"vr"); }   // 0.18.42 (owner: "I can't find play_mode = flat in the ini"): written when missing; set it to flat for flat play
        if (!_wcsicmp(pm, L"flat")) { g_playFlat = true; g_cfg.xr = 0; }
    }
    g_cfg.census = GetPrivateProfileIntW(L"gotvr", L"census", 1, ini);
    {   // 0.18.62 the arms handed to the game only for a hand above the head (climb, ledge, grapple), held 1.2 s
        wchar_t t[16]; if (!GetPrivateProfileStringW(L"gotvr", L"arms_game_above_head_cm", L"", t, 16, ini)) WritePrivateProfileStringW(L"gotvr", L"arms_game_above_head_cm", L"0", ini);
        if (!GetPrivateProfileStringW(L"gotvr", L"arms_game_hold_ms", L"", t, 16, ini)) WritePrivateProfileStringW(L"gotvr", L"arms_game_hold_ms", L"1200", ini);
        GetPrivateProfileStringW(L"gotvr", L"arms_game_above_head_cm", L"0", t, 16, ini); g_fp.armsAboveHeadCm = (float)_wtof(t); g_fp.armsHoldMs = GetPrivateProfileIntW(L"gotvr", L"arms_game_hold_ms", 1200, ini);
        if (g_fp.armsHoldMs < 100 || g_fp.armsHoldMs > 5000) g_fp.armsHoldMs = 1200;
        if (!GetPrivateProfileStringW(L"gotvr", L"arms_game_when_raised", L"", t, 16, ini)) WritePrivateProfileStringW(L"gotvr", L"arms_game_when_raised", L"2", ini);
        if (!GetPrivateProfileStringW(L"gotvr", L"arms_game_when_riding", L"", t, 16, ini)) WritePrivateProfileStringW(L"gotvr", L"arms_game_when_riding", L"0", ini);
        g_fp.armsRaised = GetPrivateProfileIntW(L"gotvr", L"arms_game_when_raised", 2, ini); if (g_fp.armsRaised < 0 || g_fp.armsRaised > 2) g_fp.armsRaised = 2; g_fp.armsRiding = GetPrivateProfileIntW(L"gotvr", L"arms_game_when_riding", 0, ini) == 0;
        // 0.18.65: F1 pressed for the HUD in session 20 turned the hands 20 -> 45 deg (F1 is the hand pitch trim in VR) - put back once
        if (GetPrivateProfileIntW(L"gotvr", L"fp_hand_v", 0, ini) < 2) { WritePrivateProfileStringW(L"gotvr", L"fp_hand_pitch_deg", L"20.0", ini); WritePrivateProfileStringW(L"gotvr", L"fp_hand_v", L"2", ini); } }
    {   // 0.18.62 the game's pace in VR (see capPace)
        wchar_t t[16]; if (!GetPrivateProfileStringW(L"gotvr", L"xr_game_fps_cap", L"", t, 16, ini)) WritePrivateProfileStringW(L"gotvr", L"xr_game_fps_cap", L"58", ini);
        if (!GetPrivateProfileStringW(L"gotvr", L"xr_menu_fps_cap", L"", t, 16, ini)) WritePrivateProfileStringW(L"gotvr", L"xr_menu_fps_cap", L"60", ini);
        g_pairCap = GetPrivateProfileIntW(L"gotvr", L"xr_game_fps_cap", 58, ini); g_menuCap = GetPrivateProfileIntW(L"gotvr", L"xr_menu_fps_cap", 60, ini);
        if (g_pairCap < 0 || g_pairCap > 500) g_pairCap = 58; if (g_menuCap < 0 || g_menuCap > 500) g_menuCap = 60;
        g_pairCapMax = g_pairCap; if (!GetPrivateProfileStringW(L"gotvr", L"xr_game_fps_adaptive", L"", t, 16, ini)) WritePrivateProfileStringW(L"gotvr", L"xr_game_fps_adaptive", L"1", ini);
        g_paceAdaptive = GetPrivateProfileIntW(L"gotvr", L"xr_game_fps_adaptive", 1, ini) != 0; }
    g_cfg.hotkeys = GetPrivateProfileIntW(L"gotvr", L"hotkeys", 1, ini);
    g_cfg.unitsPerM = (float)GetPrivateProfileIntW(L"gotvr", L"units_per_m", 100, ini);
    g_cfg.headPos = GetPrivateProfileIntW(L"gotvr", L"head_position", 1, ini);
    g_cfg.level = GetPrivateProfileIntW(L"gotvr", L"level_camera", 1, ini);
    g_cfg.queueFallback = GetPrivateProfileIntW(L"gotvr", L"queue_fallback", 1, ini);
    g_cfg.widen = GetPrivateProfileIntW(L"gotvr", L"widen_tangents", 1, ini);
    if (g_cfg.xr != 0 && g_cfg.xr != 1 && g_cfg.xr != 2 && g_cfg.xr != 3 && g_cfg.xr != 4 && g_cfg.xr != 9) { g_cfg.xr = 0; }
    // xr=3: two renders in one frame share one TAA history -> reset it every frame.
    // xr=4 (BUG-22, 0.9.1): NOT reset. The engine's own prev-frame chain is consistent under synchronized-sequential
    // stereo: Cams_StorePrev 0x8085D0 (GameTick, every tick) copies the camera of the LAST RENDERED frame (= the other
    // eye) into the prev slots, Cam_TemporalUpdate 0x808EF0 builds the reprojection from it, and the history textures
    // hold that same frame. Prev camera + prev texture describe the same image, so TAA/SSR/denoisers reproject across
    // the 6 cm correctly (playbook ch14 #velocity-not-temporal [HEADSET]: TAA is fine, velocity effects are the
    // blocker -> motion blur off). Forcing the cut every frame left every temporal pass permanently in its
    // first-frame state: unaccumulated jitter/dither (object flicker) and unconverged temporal effects (black streaks).
    g_cfg.histReset = GetPrivateProfileIntW(L"gotvr", L"history_reset", g_cfg.xr == 3 ? 1 : 0, ini);
    g_cfg.mirrorLeft = GetPrivateProfileIntW(L"gotvr", L"monitor_left_eye", 1, ini);
    g_hostStart = GetPrivateProfileIntW(L"gotvr", L"host_start", 1, ini) ? 1 : 0;
    g_renderSize = GetPrivateProfileIntW(L"gotvr", L"render_size", 2304, ini); if (g_renderSize != 0 && (g_renderSize < 512 || g_renderSize > 4096)) g_renderSize = 2304;
    g_cfg.dlssMode = GetPrivateProfileIntW(L"gotvr", L"dlss_per_eye", 0, ini); if (g_cfg.dlssMode < 0 || g_cfg.dlssMode > 2) g_cfg.dlssMode = 0;
    {   auto fl = [&](const wchar_t *k, float def, float lo, float hi) { wchar_t b[32], d[32]; swprintf(d, 32, L"%g", def); GetPrivateProfileStringW(L"gotvr", k, d, b, 32, ini); float v = (float)_wtof(b); return (v >= lo && v <= hi) ? v : def; };
        g_ui.hud = GetPrivateProfileIntW(L"gotvr", L"hud", 1, ini) ? 1 : 0;
        g_ui.hudDeg = fl(L"hud_width_deg", 55.0f, 20.0f, 120.0f); g_ui.hudDist = fl(L"hud_distance", 1.6f, 0.5f, 20.0f); g_ui.hudDown = fl(L"hud_down_deg", 3.0f, -30.0f, 30.0f);
        g_ui.theater = GetPrivateProfileIntW(L"gotvr", L"theater", 1, ini) ? 1 : 0;
        g_ui.thDist = fl(L"theater_distance", 3.0f, 0.5f, 30.0f); g_ui.thWidth = fl(L"theater_width", 3.2f, 0.3f, 40.0f); g_ui.thHeight = fl(L"theater_height", 0.0f, -5.0f, 5.0f);
        g_ui.thBackdrop = GetPrivateProfileIntW(L"gotvr", L"theater_backdrop", 0, ini) ? 1 : 0; g_ui.thCrop = GetPrivateProfileIntW(L"gotvr", L"theater_crop", 0, ini) ? 1 : 0;
        g_ui.flatRender = GetPrivateProfileIntW(L"gotvr", L"theater_flat_render", 1, ini) ? 1 : 0;
        // BUG-51 (0.14.3): the "UI target" is the game's final 2560x1440 picture with the UI drawn ON TOP of the 3D scene, so
        // it cannot be separated by copying. 0.15.0 (BUG-58): the UI passes' colour target is redirected into our own shared
        // image at the engine's target flush instead (HUD-004) -> hud_layer is back (1 = default, 0 = the 0.14 panel).
        g_ui.layer = GetPrivateProfileIntW(L"gotvr", L"hud_layer", 1, ini) ? 1 : 0; g_ui.premul = GetPrivateProfileIntW(L"gotvr", L"hud_layer_premultiplied", 1, ini) ? 1 : 0;
        g_ui.titleReason = GetPrivateProfileIntW(L"gotvr", L"title_reason", -1, ini);   // 0.14.3: reason 27 marks "leaving the world", not the start screen -> off
        g_ui.follow = GetPrivateProfileIntW(L"gotvr", L"hud_follow", 1, ini) ? 1 : 0;
        g_ui.loadTheater = GetPrivateProfileIntW(L"gotvr", L"loading_theater", 1, ini) ? 1 : 0; g_ui.menuTheater = GetPrivateProfileIntW(L"gotvr", L"menu_theater", 1, ini) ? 1 : 0;
        g_ui.crop = GetPrivateProfileIntW(L"gotvr", L"letterbox_crop", 0, ini) ? 1 : 0;   // 0.15.0: off by default (BUG-56: F3 off = no stretch)
        g_fp.on = GetPrivateProfileIntW(L"gotvr", L"first_person", 1, ini) ? 1 : 0; g_fp.hide = GetPrivateProfileIntW(L"gotvr", L"fp_hide_head", 1, ini) ? 1 : 0;
        g_fp.eyeUpCm = fl(L"fp_eye_up_cm", -1.0f, -60.0f, 40.0f); /* 0.18.23 owner: "make the view 2 cm higher" (-3 -> -1) */ g_fp.nearCm = fl(L"fp_near_cm", 0.0f, 0.0f, 50.0f);   /* 0.18.20: back to the game's 10 cm - grass was drawn over the arms with 2 cm (photo) */ g_fp.standTall = GetPrivateProfileIntW(L"gotvr", L"fp_stand_tall", 0, ini) ? 1 : 0; g_fp.stanceMaxCm = fl(L"fp_stand_tall_max_cm", 25.0f, 0.0f, 80.0f); g_fp.eyeLeftCm = fl(L"fp_eye_left_cm", -3.0f, -30.0f, 30.0f); /* 0.18.23 owner: "still a bit to the left of Jin's head (maybe 3 cm)" -> 3 cm to his right */   // 0.18.7: -11 -> -8 ("a tiny bit higher")
        g_fp.eyeFwdCm = fl(L"fp_eye_forward_cm", 1.0f, -40.0f, 60.0f);   // 0.18.12 owner: "back a few cm, still too far out" (4 -> 1); 0.18.11: 9 -> 4
        g_fp.bodyHead = GetPrivateProfileIntW(L"gotvr", L"fp_body_follows_head", 1, ini) ? 1 : 0; g_fp.arms = GetPrivateProfileIntW(L"gotvr", L"fp_arms", 1, ini) ? 1 : 0;
        g_fp.headDeadDeg = fl(L"fp_body_head_deadzone_deg", 20.0f, 1.0f, 120.0f); g_fp.headDeadDownDeg = fl(L"fp_body_head_deadzone_down_deg", 30.0f, 1.0f, 120.0f); /* 0.18.24 owner: 7.5 -> 20 */ /* 0.18.17 owner: 35 -> 45; 0.18.22 owner: "always rotate to match the headset after a small threshold (5-10 deg)" -> 7.5 */ g_fp.headTau = fl(L"fp_body_head_follow_s", 0.08f, 0.0f, 3.0f); /* 0.18.23 owner: "Jin's body must face the headset faster - in large turns I can see his body" (0.25 -> 0.08) */ g_fp.moveTau = fl(L"fp_body_move_follow_s", 0.15f, 0.0f, 3.0f);
        g_fp.bodyUnder = GetPrivateProfileIntW(L"gotvr", L"fp_body_under_hmd", 1, ini) ? 1 : 0; g_fp.underMaxCm = fl(L"fp_body_under_max_cm", 60.0f, 0.0f, 200.0f); g_fp.underTau = fl(L"fp_body_under_follow_s", 0.1f, 0.0f, 5.0f); /* 0.18.25: 0.5 -> 0.1 (owner: "the headset should follow the body's displacement faster") */
        g_fp.fingers = GetPrivateProfileIntW(L"gotvr", L"fp_fingers", 1, ini) ? 1 : 0; g_fp.indexTrigger = GetPrivateProfileIntW(L"gotvr", L"fp_finger_index_trigger", 1, ini) ? 1 : 0;
        g_fp.gripFull = fl(L"fp_grip_fist_full", 0.35f, 0.1f, 1.0f); g_fp.fingerSign = fl(L"fp_finger_bend_sign", 1.0f, -1.0f, 1.0f) < 0.0f ? -1.0f : 1.0f; g_fp.fingerRelaxed = fl(L"fp_finger_relaxed_deg", 5.0f, -20.0f, 40.0f); g_fp.fingerTween = fl(L"fp_finger_tween_s", 0.15f, 0.02f, 1.0f); g_fp.indexTween = fl(L"fp_index_tween_s", 0.07f, 0.02f, 1.0f); g_fp.forearmTwist = fl(L"fp_forearm_twist", 1.0f, 0.0f, 1.5f); g_fp.armStretch = fl(L"fp_arm_stretch", 0.10f, 0.0f, 0.4f); g_fp.roomscale = GetPrivateProfileIntW(L"gotvr", L"fp_roomscale", 0, ini) ? 1 : 0; /* 0.18.26 owner: "remove the physical dodging, it causes too many problems; keep the code" -> off by default */ g_fp.rsDeadCm = fl(L"fp_roomscale_start_cm", 12.0f, 5.0f, 100.0f); g_fp.rsStopCm = fl(L"fp_roomscale_stop_cm", 5.0f, 1.0f, 50.0f); g_fp.headRigid = GetPrivateProfileIntW(L"gotvr", L"fp_view_on_drawn_body", 1, ini) ? 1 : 0; g_fp.bodyTurnS = fl(L"fp_body_turn_s", 0.35f, 0.0f, 3.0f); g_fp.bodySquareOnStop = GetPrivateProfileIntW(L"gotvr", L"fp_body_square_on_stop", 1, ini) ? 1 : 0; g_fp.stab = GetPrivateProfileIntW(L"gotvr", L"fp_hand_stab", 1, ini) ? 1 : 0; g_fp.stabMinHz = fl(L"fp_hand_stab_min_hz", 1.2f, 0.1f, 30.0f); g_fp.stabBeta = fl(L"fp_hand_stab_beta", 10.0f, 0.0f, 200.0f); g_fp.stabRotMinHz = fl(L"fp_hand_stab_rot_min_hz", 0.9f, 0.1f, 30.0f); g_fp.stabDeadMm = fl(L"fp_hand_stab_dead_mm", 1.5f, 0.0f, 20.0f); g_fp.stabDeadDeg = fl(L"fp_hand_stab_dead_deg", 0.25f, 0.0f, 5.0f); g_fp.stabRotBeta = fl(L"fp_hand_stab_rot_beta", 3.0f, 0.0f, 100.0f); shade::mode = GetPrivateProfileIntW(L"gotvr", L"fp_ao_off", 1, ini) ? 1 : 0;
        g_fp.idleHold = GetPrivateProfileIntW(L"gotvr", L"fp_idle_hold", 1, ini) ? 1 : 0; g_fp.idleSettle = fl(L"fp_idle_settle_s", 0.5f, 0.1f, 5.0f); g_fp.stanceSquare = GetPrivateProfileIntW(L"gotvr", L"fp_stance_square", 1, ini) ? 1 : 0;
        g_fp.handBackCm = fl(L"fp_hand_back_cm", 7.0f, -10.0f, 20.0f); g_fp.handUpCm = fl(L"fp_hand_up_cm", 3.0f, -10.0f, 10.0f); g_fp.handSideCm = fl(L"fp_hand_side_cm", 0.0f, -10.0f, 10.0f); g_fp.handPitch = fl(L"fp_hand_pitch_deg", 0.0f, -180.0f, 180.0f); g_fp.handYaw = fl(L"fp_hand_yaw_deg", 0.0f, -180.0f, 180.0f); g_fp.handRoll = fl(L"fp_hand_roll_deg", 0.0f, -180.0f, 180.0f);
        g_fp.bodyFollow = GetPrivateProfileIntW(L"gotvr", L"fp_body_follow", 1, ini) ? 1 : 0; g_fp.bodyAttach = GetPrivateProfileIntW(L"gotvr", L"fp_body_follow_items", 1, ini) ? 1 : 0;
        g_fp.bodyStartDeg = fl(L"fp_body_follow_start_deg", 80.0f, 10.0f, 170.0f); g_fp.bodyFullDeg = fl(L"fp_body_follow_full_deg", 130.0f, 20.0f, 180.0f);
        if (g_fp.bodyFullDeg < g_fp.bodyStartDeg + 5.0f) g_fp.bodyFullDeg = fminf(180.0f, g_fp.bodyStartDeg + 5.0f);
        g_fp.bodyTurnSpeed = fl(L"fp_body_turn_speed", 1440.0f, 0.0f, 5000.0f); /* 0.18.23: 540 -> 1440 deg/s */ g_fp.bodyMaxSpeed = fl(L"fp_body_follow_max_speed", 8.0f, 0.5f, 100.0f);
        g_fp.hideRadiusCm = fl(L"fp_hide_radius_cm", 22.0f, 5.0f, 60.0f); g_fp.vertTau = fl(L"fp_vertical_smoothing_s", 0.35f, 0.0f, 3.0f);
        g_fp.horizTau = fl(L"fp_lean_smoothing_s", 1.0f, 0.0f, 3.0f); /* 0.18.16: 0.3 -> 1.0 (owner: "stuttering and jittering when I look down") */ g_fp.lean = fl(L"fp_follow_lean", 1.0f, 0.0f, 1.0f); g_fp.maxCamDistM = fl(L"fp_max_camera_distance_m", 8.0f, 2.0f, 100.0f);
        g_uiAspect = fl(L"ui_aspect", 16.0f / 9.0f, 0.0f, 4.0f); if (g_uiAspect > 0 && g_uiAspect < 0.5f) g_uiAspect = 16.0f / 9.0f;
        g_ui.followDeg = fl(L"hud_follow_deg", 35.0f, 5.0f, 120.0f); g_ui.followSpeed = fl(L"hud_follow_speed", 90.0f, 10.0f, 720.0f);
        { wchar_t b[32]; GetPrivateProfileStringW(L"gotvr", L"pause_channels", L"0x80", b, 32, ini); g_ui.pauseMask = (u32)wcstoul(b, nullptr, 0) & 0x0FFFFFFFu; } }
    m_cullHead = GetPrivateProfileIntW(L"gotvr", L"cull_follows_head", 1, ini) ? 1 : 0;
    { wchar_t b[32]; GetPrivateProfileStringW(L"gotvr", L"fast_turn_deg", L"2.0", b, 32, ini); g_fastTurnDeg = (float)_wtof(b); if (!(g_fastTurnDeg > 0.1f && g_fastTurnDeg < 90.0f)) g_fastTurnDeg = 2.0f; }
    { wchar_t b[32]; GetPrivateProfileStringW(L"gotvr", L"pose_lead_ms", L"auto", b, 32, ini); g_poseLeadMs = (_wcsicmp(b, L"auto") == 0 || !b[0]) ? -1.0f : (float)_wtof(b); if (!(g_poseLeadMs >= -1.0f && g_poseLeadMs <= 60.0f)) g_poseLeadMs = -1.0f; }
    g_gameGpuPrio = GetPrivateProfileIntW(L"gotvr", L"game_gpu_priority_below_normal", 1, ini) ? 1 : 0;
    g_eyeSwap = GetPrivateProfileIntW(L"gotvr", L"eye_order_swap", 0, ini) ? 1 : 0;
    g_mblurOff = GetPrivateProfileIntW(L"gotvr", L"motion_blur_off", 1, ini) ? 1 : 0;
    g_foliageOcc = GetPrivateProfileIntW(L"gotvr", L"foliage_occlusion", 2, ini); if (g_foliageOcc < 0 || g_foliageOcc > 2) g_foliageOcc = 2;
    g_renderSync = GetPrivateProfileIntW(L"gotvr", L"render_sync", 0, ini); if (g_renderSync < 0 || g_renderSync > 2) g_renderSync = 0;
    { wchar_t b[32]; GetPrivateProfileStringW(L"gotvr", L"fast_turn_deg_left", L"2.0", b, 32, ini); g_fastTurnDegLeft = (float)_wtof(b); if (!(g_fastTurnDegLeft > 0.05f && g_fastTurnDegLeft < 90.0f)) g_fastTurnDegLeft = 2.0f; }
    { wchar_t b[32]; GetPrivateProfileStringW(L"gotvr", L"fov_margin_deg", L"3.0", b, 32, ini); g_fovMarginDeg = (float)_wtof(b); if (!(g_fovMarginDeg >= 0.0f && g_fovMarginDeg <= 15.0f)) g_fovMarginDeg = 3.0f; }
    g_fastTurnMode = GetPrivateProfileIntW(L"gotvr", L"fast_turn_mode", 1, ini); if (g_fastTurnMode < 0 || g_fastTurnMode > 2) g_fastTurnMode = 1;
    // xr=4 (0.9.2, BUG-23): history preset 1..5 (see kPresets / README); default 1 = HYBRID
    g_cfg.histMode = GetPrivateProfileIntW(L"gotvr", L"history_mode", 7, ini) - 1; if (g_cfg.histMode < 0 || g_cfg.histMode > 6) g_cfg.histMode = 6;   // 0.18.11: 7 (per-eye frustum culling, no pop-in, nothing culled in one eye only); 0.18.10 owner: 4 removed the pop-in
}

// ---------------- census state ----------------
static std::atomic<u64> c_tick{0}, c_present{0}, c_frameview{0}, c_slot43A{0}, c_slot43Other{0}, c_setlocalA{0}, c_setlocalOther{0}, c_projrectA{0};
static std::atomic<u32> t_tick{0}, t_present{0}, t_frameview{0}, t_slot43{0}, t_projrect{0};
struct CallerStat { std::atomic<u64> rva{0}; std::atomic<u64> n{0}; std::atomic<u64> nA{0}; };
static CallerStat g_callers[32];
static void noteCaller(u64 ret, bool isA) {
    u64 rva = ret - (u64)g_base;
    for (auto &c : g_callers) {
        u64 cur = c.rva.load();
        if (cur == rva) { c.n++; if (isA) c.nA++; return; }
        if (cur == 0) { u64 z = 0; if (c.rva.compare_exchange_strong(z, rva) || c.rva.load() == rva) { c.n++; if (isA) c.nA++; return; } }
    }
}
static float g_lastRect[4]; static std::atomic<u32> g_rectSeq{0};
struct ViewSample { u32 seq; int views; int distinctCams; u64 mainCam; u64 frameCam; u8 mode80f09; int secCount; int list2Count; u64 frame; };
static ViewSample g_view; static CRITICAL_SECTION g_viewcs;
struct ResSample { u64 ptr[4]; u32 state[4]; D3D12_RESOURCE_DESC desc[4]; bool valid[4]; u32 seq; };
static ResSample g_res; static CRITICAL_SECTION g_rescs;

// ---------------- mutations (EXP-02 / EXP-03b), all off by default ----------------
static std::atomic<int> m_armed{0};      // F7: armed zero-delta (B control): hooks rewrite the same values
static std::atomic<int> m_leftHalf{0};   // F8: asymmetric projection (left half) injected at +0x200 in slot43
static std::atomic<int> m_yaw{0};        // F9: +5 deg yaw on the render camera world matrix (EXP-02 mutation)
static std::atomic<int> m_burst{0};      // F10: log a detailed snapshot
static std::atomic<int> m_recenter{0};   // F11: recentre head tracking
static std::atomic<u64> nMetaRecenters{0};   // 0.18.23: recentres from the headset's own button
static u64 g_autoRecentreAt = 0;          // 0.15.5: recentre once the headset session runs
static std::atomic<int> m_histReset{-1}; // F6: force the camera-cut flag every VR frame (TAA/DLSS history reset A/B); -1 = use ini
static std::atomic<u64> c_engineCuts{0};  // camera cuts the ENGINE raised itself (seen at Cam_TemporalUpdate entry)

// ---------------- shared block with GoTVRHost.exe ----------------
static GvShared *S; static HANDLE g_map, g_hostProc;
static bool hostFresh() { return S && S->hostState == GV_HOST_RUNNING && (u64)GetTickCount64() - (u64)S->hostBeat < 1000; }
#ifndef _XINPUT_H_
struct XINPUT_GAMEPAD { WORD wButtons; BYTE bLeftTrigger, bRightTrigger; SHORT sThumbLX, sThumbLY, sThumbRX, sThumbRY; };
struct XINPUT_VIBRATION { WORD wLeftMotorSpeed, wRightMotorSpeed; };
#endif
#include "vrinput.h"
#include "fbik.h"
#include "fingers.h"
#include "stab.h"
#include "sword.h"
#include "melee.h"
#include "parry.h"    // 0.18.61 block / deflect with the katana (the design doc's parry parameters)
#include "attack.h"   // 0.18.48 swing / heavy charge / lunge / katana weight (the design doc's attack parameters)
#include "vitals.h"   // wrist health / resolve decision core (docs/WRIST_HUD.md; not wired in yet)

// One packet per game frame, built on the game thread the first time the frame touches the render
// camera, and found again at Present by the frame id the engine tags its Present with (desc[0]).
struct FramePkt { volatile LONG seq; u32 frame; int eye; int phase; bool valid; GvPose pose; float fov[4]; bool stereo; GvPose pose2; float fov2[4]; bool flat; };
static int g_pairPhase = 0;   // xr=4: 0 = left eye, simulated tick; 1 = right eye, frozen tick (game thread)
// 0.18.5 diagnostic (BUG-74): which eye the SIMULATED tick renders. 0 = left (default), 1 = right (Shift+F9 / eye_order_swap).
// The pop-in has been left-eye-only through every fix; if it follows the swap to the right eye, it belongs to "the eye drawn
// right after the simulation step" (its previous frame is the other eye of the previous pair - playbook ch14
// #cpu-visibility-history); if it stays in the left eye, it belongs to the left eye itself. Latched at a pair start.
struct PairSample { bool ok; GvPose e[2]; float f[2][4]; }; static PairSample g_pairSample{};
static std::atomic<u64> g_frozenTicks{0}, g_simTicks{0}; static bool g_lockGame = false;
static int g_runEye = 0; static std::atomic<u64> g_rfixCalls{0}; static float g_lastDelta[3];
static int g_fixCulprit = -1;   // pass the second render takes from the first (F2 result / right_eye_reuse)   // native stereo: which eye CAM-A is composed for right now (0 = left/first render, 1 = right/second render). Game thread.
static std::atomic<int> m_offTest{0};   // F1: second render's camera 50 cm to the right (flat proof of a real second camera)
static FramePkt g_ring[32];
static FramePkt g_cur; static u32 g_curFrame = 0xFFFFFFFFu;   // game thread only
#ifdef GOTVR_SELFTEST
static u32 g_stFrame = 0; static bool g_stFrameOn = false;
static u32 frameCtr() { return g_stFrameOn ? g_stFrame : *(volatile u32 *)(g_base + RVA_FRAMECTR); }
#else
static u32 frameCtr() { return *(volatile u32 *)(g_base + RVA_FRAMECTR); }
#endif
static std::atomic<u64> c_pktValid{0}, c_slot43BeforeFV{0}, c_slot43AfterFV{0};

// ---------------- hooks ----------------
typedef void (*FnV0)(); typedef u64 (*FnP2)(void *, void *); typedef void (*FnV1)(void *); typedef void (*FnV2)(void *, const float *);
static FnV0 oGameTick; static FnP2 oPresent; static FnV2 oSetLocal; static FnV1 oSlot43; static FnV2 oSetProjRect; static FnV1 oFrameView;

// ---------------- xr=4 synchronized-sequential stereo (playbook ch17 "Synchronized Sequential") ----------------
// The engine carries one main view per frame and globally slotted per-frame state (R1 findings), so instead of a
// second render inside a frame, every PAIR of ticks renders both eyes from ONE simulation state: tick A (left eye)
// simulates normally; tick B (right eye) is frozen: Clock_Update_56ED80 is skipped (its timestamps stay put, so tick
// A+2's step covers both ticks: normal game speed, one simulation step per pair), the game steps (+0x08 = the
// GameTick gate 0x141489DF8, +0x18, +0x38, +0x48) are 0 and the real step tiny (no division by zero; GameTick takes
// its paused branch, like photo mode), the camera composes from tick A's game camera (cache lock), the right eye
// uses the SAME head-pose sample as the left, and the pad shows tick A's state (no lost presses). Every tick is a
// complete normal engine frame: no second-render state hazards.
typedef void (*FnClock)(float *); static FnClock oClock;
// Cam_TemporalUpdate 0x808EF0 (GameTick -> 0x59CB80, every camera, every tick): if the camera-cut flag +0x7D8 is set it
// seeds prev = current (zero camera motion), then builds the reprojection matrices (+0x2C0..+0x370) from the prev slots
// that Cam_StorePrev 0x8085D0 (GameTick -> 0x59C710) filled with the last rendered frame's camera, and calls slots
// 42/43 (unjittered, then jittered). Hooked only to count the cuts the engine raises itself (BUG-22 telemetry).
typedef void (*FnCamT)(u8 *); static FnCamT oCamT;
static std::atomic<int> g_camEye{-1};   // xr=4: eye whose camera CAM-A currently holds (set at Cam_TemporalUpdate)
namespace taa { static void sameEyeRun(u8 *cam); static void afterNormalRun(u8 *cam); }
namespace occ { static void fastCheck(u8 *cam); }
namespace dlss { static bool velWanted(); }
static void hCamT(u8 *cam) {
    const bool a = cam && cam == *(u8 **)(g_base + RVA_CAMA);
    if (a && *(int *)(cam + 0x7D8)) c_engineCuts++;
    if (a) { g_camEye = g_pairPhase; occ::fastCheck(cam); taa::sameEyeRun(cam); }
    oCamT(cam);
    if (a) taa::afterNormalRun(cam);
}
// ---------------- BUG-26 (0.9.4): per-eye GPU occlusion history ----------------
// RV_OcclusionSetup 0x9E5350 (GameTick -> 0x5A07F0 -> 0x5A08B0, game thread) prepares the HiZ occlusion test of the
// main view (decompiled: re/decomp/cut/9e5350): steady state = source HiZ record slot p (0x143389240 + p*0x20), the
// slot (1-p) is this frame's destination, and the PREVIOUS camera (+0x380 VPrel rows, +0x560 pos) reprojects it
// (outputs view+0x60..+0x9C viewport-scaled rows, +0xA0 pos, +0xB0..+0xEC rows; +0x120 = 0 in this branch).
// Under xr=4 the previous frame is the other eye: KEPT = pop-in per eye (0.9.3 setting 5); forcing a cut here
// (setting 3) marks the history invalid (view+0x34) -> no pop-in but the light flicker. Per-eye bank instead
// (playbook ch14 #per-eye-history-bank / STR-015, both halves): the slot (1-p) still holds THIS eye's HiZ from two
// frames ago when the test runs (it is rebuilt later in the frame), and Cam_StorePrev 0x8085D0 gives this eye's
// camera of two ticks ago. Source = slot (1-p), matrices = that camera; history stays valid.
namespace occ {
struct Bank { bool ok; float vp[16]; float pos[4]; u8 w180[0x80]; u8 w380[0x80]; u8 w560[0x10]; u32 t674; };
static Bank bank[2]; static std::atomic<int> on{0};
struct SlotOwner { bool ok; int eye; u32 frame; float vp[16]; float pos[4]; };
static SlotOwner owner[2]; static std::atomic<u64> trkOwn{0}, trkEngine{0}, parEye[2][2];

static std::atomic<u64> calls{0}, fixed{0}, skipBranch{0}, skipDims{0}, skipBank{0};
typedef void (*FnSP)(u8 *); static FnSP oSP;
static void hStorePrev(u8 *cam) {
    oSP(cam);   // prev slots := current = the camera of the LAST tick, i.e. eye g_camEye
    if (g_cfg.xr != 4 || cam != *(u8 **)(g_base + RVA_CAMA)) return;
    int e = g_camEye.load(); if (e < 0 || e > 1) return;
    Bank &b = bank[e]; memcpy(b.vp, cam + 0x380, 64); memcpy(b.pos, cam + 0x560, 16);
    memcpy(b.w180, cam + 0x180, 0x80); memcpy(b.w380, cam + 0x380, 0x80); memcpy(b.w560, cam + 0x560, 0x10); b.t674 = *(u32 *)(cam + 0x674); b.ok = true;
}
static void reseed() { bank[0].ok = bank[1].ok = false; owner[0].ok = owner[1].ok = false; }
// BUG-36 (0.10.3, playbook ch14 "treat a pose gap as a camera cut" / STR-015 "detect your own pose gaps"): the per-eye
// occlusion history is two ticks old. A fast head turn makes that gap large: the HiZ test reprojects across a big
// rotation, objects entering from outside the old view test against clamped edge depth and are culled until the turn
// slows ("only when I move my head fast, even if I look at the object"). When this eye's rotation since its previous
// frame exceeds fast_turn_deg, this tick's occlusion history is declared invalid (the engine's own cut path for the
// occlusion setup, as on a camera cut): everything is drawn for that frame.
static float fastDeg = 2.0f, fastDegLeft = 2.0f; static std::atomic<int> fastMode{1};   // 1 = occlusion history invalid, 2 = engine's cross-eye (1 frame) history
static std::atomic<bool> fast{false}; static std::atomic<u64> fastTicks{0}; static float lastAngle = 0;
static void setVoFast(bool f);   // defined after the cut overrides
// BUG-67 (0.17.1: "objects pop in and out in the LEFT eye when I move my head fast"): the angle compared this eye's camera at
// Cam_TemporalUpdate - the start of the tick, before this tick's head pose is composed - with the banked camera. On the
// simulated (left) tick the game has just rewritten its camera, so a head-only turn was invisible there; on the frozen
// (right) tick the camera still carries the composed head. The headset's own rotation since this eye's previous frame is
// now measured too (from the pose packet), and the larger of the two decides.
static float headQ[2][4]; static bool headQok[2] = {false, false}; static std::atomic<float> lastHeadAngle{0};
static float headTurnSince(int e) {
    if (!S) return 0.0f; GvPose h{}; u32 v = 0;
    if (!gvRead(&S->poseSeq, [&] { h = S->head; v = S->poseValid; }) || !v) return 0.0f;
    float a = 0.0f;
    if (headQok[e]) { float d = fabsf(h.q[0] * headQ[e][0] + h.q[1] * headQ[e][1] + h.q[2] * headQ[e][2] + h.q[3] * headQ[e][3]); a = 2.0f * acosf(fminf(1.0f, d)) * 57.2957795f; }
    memcpy(headQ[e], h.q, 16); headQok[e] = true; lastHeadAngle = a; return a;
}
// BUG-67b (0.17.3: "still only in the LEFT eye, worse facing grass/flowers when looking side to side somewhat fast"). RE
// (re/decomp_anim 62a530 / 79ada0, the grass and foliage culling passes): besides the RV occlusion test (per-eye banked
// above), six GPU culling passes test against the PREVIOUS frame's HiZ pyramid (0x1433891A4 + (1-parity)*0x34) through
// Occ_View 0x9E5960(0x141EDBD70). Under synchronized-sequential stereo the previous frame of the RIGHT eye is the left eye
// of the same pair (same head pose, 6 cm apart); the previous frame of the LEFT eye is the right eye of the PREVIOUS pair,
// one whole pair of head motion earlier (playbook ch14 #cpu-visibility-history: "eye zero reads the previous pair's
// eye-one set"). Foliage near the leading edge is culled against depth from a view that did not see it. The left eye's
// pose gap is real, the right eye's is not, so the left eye declares the history invalid (the engine's cut path: the
// passes then test nothing and draw everything) at a much smaller turn (fast_turn_deg_left, 0.6 deg per pair = 36 deg/s).
static std::atomic<u64> fastL{0}, fastR{0};
static void fastCheck(u8 *cam) {
    bool f = false;
    const int e = g_pairPhase;
    const float lim = e == 0 ? fastDegLeft : fastDeg;
    const float ha = (g_cfg.xr == 4 && e >= 0 && e <= 1) ? headTurnSince(e) : 0.0f;
    const int om = on.load(); const bool hist = om == 1 || om == 2;   // 0.18.11: mode 3 (per-eye frustum culling) has no history to protect
    if (hist && g_cfg.xr == 4 && ha > lim && fastMode.load() != 0) { lastAngle = ha; f = true; }
    else if (hist && g_cfg.xr == 4 && bank[e].ok) {
        const float *cw = (const float *)(cam + 0x140), *pw = (const float *)(bank[e].w180 + 0x40);   // +0x1C0 = that tick's world
        float worst = 1.0f;
        for (int r = 0; r < 3; r++) {
            float d = 0, la = 0, lb = 0; for (int k = 0; k < 3; k++) { d += cw[r * 4 + k] * pw[r * 4 + k]; la += cw[r * 4 + k] * cw[r * 4 + k]; lb += pw[r * 4 + k] * pw[r * 4 + k]; }
            if (la > 1e-6f && lb > 1e-6f) worst = fminf(worst, d / sqrtf(la * lb));
        }
        lastAngle = acosf(fmaxf(-1.0f, fminf(1.0f, worst))) * 57.2957795f;
        f = fastMode.load() != 0 && lastAngle > lim;
    }
    if (f) { fastTicks++; if (e == 0) fastL++; else fastR++; }
    if (f != fast.load()) { fast = f; setVoFast(f); }
}
// BUG-71 (0.17.5: the left-eye pop-in survived the 0.17.3 history cut and the 0.17.4 render sync - both reverted). RE
// (ADDR-144/146): RV_OcclusionSetup's object IS the occlusion view 0x141EDBD70 (frame 0x141EBD2C0 + 0x1EAB0) that six GPU
// culling passes - grass, foliage, terrain tiles - read through Occ_View 0x9E5960: they take the HiZ record +0xF0 (table B,
// slot 1-parity = the PREVIOUS frame, i.e. the other eye) and reproject it with +0xA0 (position) and +0xB0..+0xEC (rows).
// Since 0.9.4 hOcc overwrites those fields with THIS eye's camera of two ticks ago (right for RV's own table-A source,
// which it also swaps) - so the foliage passes reprojected the other eye's depth with this eye's old camera. Near
// geometry (first person: grass and flowers at arm's length) turns that 6 cm-plus-a-pair mismatch into large angles.
// Now the foliage passes get the ENGINE's own view of that tick (record + matrices of the same previous frame, the
// engine's design), saved before hOcc rewrites it; RV's per-eye fix stays as it was. Zero cost. Shift+F8 cycles
// foliage_occlusion: 1 = engine view (default), 0 = 0.17.4 behaviour, 2 = no occlusion view (diagnostic, costs GPU).
static u8 engView[2][0x140]; static std::atomic<int> engViewIdx{-1}; static std::atomic<int> viewMode{2};
static std::atomic<u64> viewCalls{0}, viewEngine{0}, viewNull{0};
typedef u8 *(*FnOccView)(u8 *); static FnOccView oOccView;
static u8 *hOccView(u8 *obj) {
    if (obj == g_base + RVA_OCC_VIEW && g_cfg.xr == 4 && on.load()) {
        viewCalls++; const int m = on.load() == 3 ? 2 : viewMode.load();   // 0.18.11: mode 3 = no cross-eye occlusion anywhere
        if (m == 2) { viewNull++; return nullptr; }
        const int k = engViewIdx.load(std::memory_order_acquire);
        if (m == 1 && k >= 0) { viewEngine++; return engView[k][0x34] ? engView[k] + 0x40 : nullptr; }
    }
    return oOccView(obj);
}
typedef void (*FnOcc)(u8 *); static FnOcc oOcc;
// 0.18.9 (BUG-74, owner's F6 test: preset 4 - cross-eye occlusion - removes the left-eye pop-in but leaves objects missing in one eye;
// preset 1 - the 0.9.4 per-eye bank - pops in the left eye): the 0.9.4 bank ASSUMES that slot (1-p) holds this eye's HiZ from two
// frames ago and pairs it with this eye's camera from two ticks ago. Under first person that pairing is wrong for the left eye. Now
// (occ mode 2, default) nothing is assumed: every frame records which eye wrote which HiZ slot (its destination is slot 1-p) and with
// which camera (+0x430 view-projection rows, +0x550 position - what Cam_StorePrev copies). An eye tests against a slot only if its OWN
// last frame wrote it (at most 4 frames old), reprojected with the camera that drew it; otherwise the engine's own pairing is kept.
static void hOcc(u8 *v) {
    oOcc(v); calls++;
    { static int told = 0; if (!told++) logf("occlusion: RV_OcclusionSetup object %p %s the foliage culling view %p", (void *)v, v == g_base + RVA_OCC_VIEW ? "IS" : "is NOT", (void *)(g_base + RVA_OCC_VIEW)); }
    if (v == g_base + RVA_OCC_VIEW) { const int k = engViewIdx.load() == 0 ? 1 : 0; memcpy(engView[k], v, sizeof engView[k]); engViewIdx.store(k, std::memory_order_release); }
    if (on.load() == 3) return;   // 0.18.11 per-eye frustum culling: the setup is left exactly as the engine made it (nobody reads it)
    if (on.load() == 2 && g_cfg.xr == 4 && v) {   // tracked: works with or without the fast-turn rule, records every frame
        const int p = *(int *)(g_base + RVA_PARITY); const int e = g_camEye.load(); u8 *cam = *(u8 **)(g_base + RVA_CAMA);
        if ((p == 0 || p == 1) && (e == 0 || e == 1) && cam) {
            parEye[e][p]++;
            const u32 N = frameCtr(); int src = -1; u32 best = 0;
            for (int q = 0; q < 2; q++) if (owner[q].ok && owner[q].eye == e && N - owner[q].frame <= 4 && N != owner[q].frame && owner[q].frame + 1 > best) { best = owner[q].frame + 1; src = q; }
            SlotOwner mine{true, e, N, {}, {}}; memcpy(mine.vp, cam + 0x430, 64); memcpy(mine.pos, cam + 0x550, 16);
            const bool steady = *(u8 *)(v + 0x120) == 0 && *(u8 *)(g_base + RVA_OCC_ENABLE) != 0 && *(u8 **)(v + 0x110) == g_base + RVA_OCC_SLOTS + p * 0x20;
            const u32 dP = *(u32 *)(g_base + RVA_OCC_DIMS + p * 0x34), dW = *(u32 *)(g_base + RVA_OCC_DIMS + (1 - p) * 0x34);
            if (steady && !fast.load() && src >= 0 && dP == dW && dW) {
                const SlotOwner &o = owner[src];
                *(u8 **)(v + 0x110) = g_base + RVA_OCC_SLOTS + src * 0x20;
                const float f27 = (float)(dW & 0xFFFF) * 0.5f, f32 = (float)(dW >> 16) * 0.5f, f33 = -f32;
                for (int k = 0; k < 4; k++) { const float *r = o.vp + 4 * k; float *ov = (float *)(v + 0x60 + 0x10 * k); ov[0] = r[0] * f27 + r[3] * f27; ov[1] = r[1] * f33 + r[3] * f32; ov[2] = r[2]; ov[3] = r[3]; }
                memcpy(v + 0xA0, o.pos, 16); memcpy(v + 0xB0, o.vp, 48); memcpy(v + 0xE0, o.vp + 12, 16);
                fixed++; trkOwn++;
            } else if (steady) { trkEngine++; if (src < 0) skipBank++; }
            else skipBranch++;
            owner[1 - p] = mine;   // this frame draws into slot 1-p
        }
        return;
    }
    if (!on.load() || g_cfg.xr != 4 || !v || fast.load()) return;
    const int p = *(int *)(g_base + RVA_PARITY); if (p != 0 && p != 1) return;
    u8 *slotP = g_base + RVA_OCC_SLOTS + p * 0x20, *slotW = g_base + RVA_OCC_SLOTS + (1 - p) * 0x20;
    if (*(u8 *)(v + 0x120) != 0 || *(u8 **)(v + 0x110) != slotP || *(u8 *)(g_base + RVA_OCC_ENABLE) == 0) { skipBranch++; return; }
    const u32 dP = *(u32 *)(g_base + RVA_OCC_DIMS + p * 0x34), dW = *(u32 *)(g_base + RVA_OCC_DIMS + (1 - p) * 0x34);
    if (dP != dW || !dW) { skipDims++; return; }
    int e = g_camEye.load(); if (e < 0 || e > 1 || !bank[e].ok) { skipBank++; return; }
    const Bank &b = bank[e];
    *(u8 **)(v + 0x110) = slotW;
    const float f27 = (float)(dW & 0xFFFF) * 0.5f, f32 = (float)(dW >> 16) * 0.5f, f33 = -f32;
    for (int k = 0; k < 4; k++) {
        const float *r = b.vp + 4 * k; float *o = (float *)(v + 0x60 + 0x10 * k);
        o[0] = r[1] * 0.0f + r[0] * f27 + r[2] * 0.0f + r[3] * f27;
        o[1] = r[1] * f33 + r[0] * 0.0f + r[2] * 0.0f + r[3] * f32;
        o[2] = r[2]; o[3] = r[3];
    }
    memcpy(v + 0xA0, b.pos, 16); memcpy(v + 0xB0, b.vp, 48); memcpy(v + 0xE0, b.vp + 12, 16);
    fixed++;
}
} // namespace occ
// ---------------- BUG-27 (0.9.5): per-eye TAA history (playbook STR-015, both halves) ----------------
// RE (re/decomp/taa): AA_Resolve 0x614EB0(AA = 0x143376280, ctx) is the TAA: unless reset (AA+0x1D40E) it draws
// TAA_Draw 0x7E7D10(shader, ctx, CAM-A, scene [frame+0x18]+0x88, history AA+0x193A8 = view of record AA+0x19320,
// velocity AA+0xEA90, exposure) into AA+0x8788, copies that to AA+0x640 (the output post uses), then copies the
// CURRENT scene into record AA+0x19320: the only carried TAA history is the previous frame's scene colour. The
// reprojection comes from the camera: TAA_Draw binds it (RV_9A2610 reads +0x340..+0x37C, the prev-relative VP that
// Cam_TemporalUpdate builds), and the velocity pass AA_Velocity 0x614720(AA, ctx, cam) reads +0x400..+0x42C.
// Under xr=4 history + reprojection are the other eye (faint double edge on near objects: parallax disocclusion).
// Per eye: (1) matrices: run Cam_TemporalUpdate once more on CAM-A with THIS eye's banked prev slots, keep its
// reprojection outputs, restore the camera, then the normal run (other consumers keep the engine's chain);
// (2) TAA_Draw / AA_Velocity get a copy of CAM-A carrying those outputs; (3) texture: record AA+0x19320 is restored
// from this eye's bank before the resolve and saved into it after (GPU copies in the engine's own command list,
// engine transitions). Honour the engine's reset: no restore on a reset frame, banks reseeded.
namespace taa {
static std::atomic<int> on{0};
static const u32 CAMSZ = 0x8D0;
struct Rep { bool ok; int eye; float pos[4]; u8 r2c0[0xC0]; u8 r400[0x30]; };
static Rep rep{}; static CRITICAL_SECTION repcs; static bool repcsInit = false;
static double dSameSum = 0, dCrossSum = 0, dRepSum = 0; static u64 dN = 0, dRepN = 0;
static std::atomic<u64> runs{0}, drawFix{0}, velFix{0}, restores{0}, saves{0}, resets{0}, stale{0}, texFail{0}, velSwaps{0};
static void sameEyeRun(u8 *cam) {
    if (!(on.load() & 6) || g_cfg.xr != 4) { rep.ok = false; return; }
    const int e = g_pairPhase; occ::Bank &b = occ::bank[e];
    if (*(int *)(cam + 0x7D8) || !b.ok) { rep.ok = false; return; }
    static u8 save[CAMSZ]; memcpy(save, cam, CAMSZ);
    memcpy(cam + 0x180, b.w180, 0x80); memcpy(cam + 0x380, b.w380, 0x80); memcpy(cam + 0x560, b.w560, 0x10); *(u32 *)(cam + 0x674) = b.t674;
    oCamT(cam);
    Rep r{}; r.ok = true; r.eye = e; memcpy(r.r2c0, cam + 0x2C0, 0xC0); memcpy(r.r400, cam + 0x400, 0x30);
    memcpy(r.pos, cam + 0x550, 16);   // identity of this tick's camera (eye position; checked again at render time)
    {   // diagnostics (0.9.9): how far the same-eye and the cross-eye previous cameras are from the current one
        const float *c = (const float *)(cam + 0x550), *ps = (const float *)b.w560, *pc = (const float *)(save + 0x560);
        float ds = 0, dc = 0; for (int k = 0; k < 3; k++) { ds += (c[k] - ps[k]) * (c[k] - ps[k]); dc += (c[k] - pc[k]) * (c[k] - pc[k]); }
        dSameSum += sqrtf(ds); dCrossSum += sqrtf(dc); dN++;
    }
    memcpy(cam, save, CAMSZ);   // as if the extra run never happened; the normal run follows
    EnterCriticalSection(&repcs); rep = r; LeaveCriticalSection(&repcs);
    runs++;
}
static void afterNormalRun(u8 *cam) {   // how different the same-eye reprojection is from the engine's (diagnostic)
    if (!rep.ok) return; float m = 0; const float *a = (const float *)(cam + 0x340), *b = (const float *)(rep.r2c0 + 0x80);
    for (int i = 0; i < 16; i++) m = fmaxf(m, fabsf(a[i] - b[i])); dRepSum += m; dRepN++;
}
static bool repFor(const u8 *cam, Rep &out) {
    EnterCriticalSection(&repcs); out = rep; LeaveCriticalSection(&repcs);
    if (!out.ok) return false;
    if (memcmp(cam + 0x550, out.pos, 16)) { stale++; return false; }
    return true;
}
static thread_local u8 fake[CAMSZ]; static bool selfTestDraw = false;
static u8 *fakeCam(u8 *cam, const Rep &r) { memcpy(fake, cam, CAMSZ); memcpy(fake + 0x2C0, r.r2c0, 0xC0); memcpy(fake + 0x400, r.r400, 0x30); return fake; }
typedef void (*FnVel)(u8 *, u8 *, u8 *); static FnVel oVel;
typedef void (*FnDraw)(u64, u8 *, u8 *, void *, void *, u64, u32, float); static FnDraw oDraw;
static thread_local bool tl_sameEye = false;   // set by AA_Resolve while this eye's history is in the record
static void hDraw(u64 sh, u8 *ctx, u8 *cam, void *scene, void *hist, u64 vel, u32 p7, float p8) {
    Rep r; if ((on.load() & 2) && g_cfg.xr == 4 && (tl_sameEye || selfTestDraw) && cam == *(u8 **)(g_base + RVA_CAMA) && repFor(cam, r)) { drawFix++; oDraw(sh, ctx, fakeCam(cam, r), scene, hist, vel, p7, p8); return; }
    oDraw(sh, ctx, cam, scene, hist, vel, p7, p8);
}
// BUG-30 (0.9.6): the AA members (+0x19320, +0xEA90) are RENDER-TARGET handles; the resource record Res_Transition
// 0x4FDD50 takes is *(handle + 0x10) (RT_ResOf 0x7E10F0, as RT_ToX 0x7E3E50 does). 0.9.5 resolved the handle itself
// -> no native resource -> the history bank never ran (log: restored=0 saved=0) while the TAA already used this
// eye's reprojection = cross-eye history reprojected with same-eye motion (the moving shadows).
static u8 *resOf(u8 *rt) { return (rt && readable(rt + 0x10, 8)) ? *(u8 **)(rt + 0x10) : nullptr; }
// resource record -> per-buffer object -> native resource (same resolution as Res_Transition 0x4FDD50)
static ID3D12Resource *nativeRec(u8 *rec) {   // engine resource record -> native resource
    if (!rec || !readable(rec, 0x80)) return nullptr;
    int idx = *(int *)(rec + 0x38); u8 *obj;
    if (idx < 0) obj = *(u8 **)(rec + 0x20);
    else { u8 *tab = *(u8 **)(g_base + RVA_RES_TABLE); if (!tab || !readable(tab + (u64)idx * 0x18, 8)) return nullptr; u8 **arr = *(u8 ***)(tab + (u64)idx * 0x18); int sub = *(int *)(g_base + RVA_RES_MULTI) ? *(int *)(rec + 0x3C) : 0; if (!arr || sub < 0 || sub > 7 || !readable(arr + sub, 8)) return nullptr; obj = arr[sub]; }
    if (!obj || !readable(obj + 8, 8)) return nullptr;
    return *(ID3D12Resource **)(obj + 8);
}
static ID3D12Resource *nativeOf(u8 *rt) { return nativeRec(resOf(rt)); }
// two private textures shaped like an engine resource (simultaneous access: implicit promotion + decay, so no state
// has to be tracked across command lists); recreated whenever the source's desc or identity changes
struct TexSet { ID3D12Resource *t[2]; bool ok[2]; D3D12_RESOURCE_DESC desc; ID3D12Resource *src; const char *name; };
static TexSet hist{{}, {}, {}, nullptr, "history"}, velo{{}, {}, {}, nullptr, "velocity"};
static CRITICAL_SECTION bcs;
// BUG-32 (0.9.8): the history handle is multi-buffered (its native resource changes between frames), and 0.9.7
// recreated the banks on every identity change and RELEASED the old ones while GPU work still referenced them ->
// device removed at the main menu (host: xrBeginFrame RUNTIME_FAILURE). Now: identity changes with the same desc
// keep the banks (content is copied by value, the source identity does not matter); a desc change retires the old
// textures to a graveyard released only seconds later, when no frame in flight can still use them.
struct Grave { ID3D12Resource *r; u64 at; }; static Grave grave[32]; static int ngrave = 0;
static void bury(ID3D12Resource *r) {
    if (!r) return; u64 now = GetTickCount64();
    for (int i = 0; i < ngrave;) { if (now - grave[i].at > 3000) { grave[i].r->Release(); grave[i] = grave[--ngrave]; } else i++; }
    if (ngrave < 32) grave[ngrave++] = {r, now};
    else { static int told = 0; if (!told++) logf("taa: graveyard full - texture kept alive (leak) rather than freed in flight"); }
}
static std::atomic<u64> srcChanges{0};
// BUG-34 (0.10.1): the two resources the history handle alternates between have descs that differ only in fields a
// copy does not care about (flags/alignment/layout), so the whole-struct memcmp recreated the banks on every switch:
// 529 recreations in one session, 1000+ full-resolution textures, the graveyard overflowed and leaked -> VRAM
// exhausted -> 20 fps. Compare only what CopyResource needs.
static bool sameShape(const D3D12_RESOURCE_DESC &a, const D3D12_RESOURCE_DESC &b) {
    return a.Dimension == b.Dimension && a.Width == b.Width && a.Height == b.Height && a.DepthOrArraySize == b.DepthOrArraySize &&
           a.MipLevels == b.MipLevels && a.Format == b.Format && a.SampleDesc.Count == b.SampleDesc.Count && a.SampleDesc.Quality == b.SampleDesc.Quality;
}
static std::atomic<u64> recreations{0};
static bool ensureSet(TexSet &ts, ID3D12Resource *nat) {
    D3D12_RESOURCE_DESC d = nat->GetDesc();
    if (ts.t[0] && sameShape(d, ts.desc)) { if (ts.src != nat) { ts.src = nat; srcChanges++; } return true; }
    if (++recreations > 64) { texFail++; return false; }   // hard stop: never churn GPU memory (falls back to the engine TAA)
    for (auto &b : ts.t) { bury(b); b = nullptr; }
    ts.ok[0] = ts.ok[1] = false; ts.src = nullptr;
    if (d.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D || d.SampleDesc.Count != 1 || (d.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)) { texFail++; return false; }
    ID3D12Device *dv = nullptr; if (FAILED(nat->GetDevice(IID_PPV_ARGS(&dv))) || !dv) { texFail++; return false; }
    D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC bd = d; bd.Flags = (d.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) | D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS;   // implicit promotion + decay: no state to track across lists
    bool ok = true; for (auto &b : ts.t) ok &= SUCCEEDED(dv->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&b)));
    dv->Release();
    if (!ok) { for (auto &b : ts.t) { bury(b); b = nullptr; } texFail++; logf("taa: %s textures create failed", ts.name); return false; }
    ts.desc = d; ts.src = nat;
    logf("taa: %s textures %llux%u format %d mips %u flags 0x%x align %llu layout %d (creation #%llu)", ts.name, (unsigned long long)d.Width, d.Height, (int)d.Format, d.MipLevels, (unsigned)d.Flags, (unsigned long long)d.Alignment, (int)d.Layout, (unsigned long long)recreations.load());
    return true;
}
static void barrier(ID3D12GraphicsCommandList *gl, ID3D12Resource *r, D3D12_RESOURCE_STATES a, D3D12_RESOURCE_STATES b) {
    D3D12_RESOURCE_BARRIER br{}; br.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; br.Transition.pResource = r; br.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES; br.Transition.StateBefore = a; br.Transition.StateAfter = b; gl->ResourceBarrier(1, &br);
}
static ID3D12GraphicsCommandList *listOf(u8 *ctx) { u64 w = ctx ? *(u64 *)ctx : 0; return (w && readable((void *)w, 8)) ? *(ID3D12GraphicsCommandList **)w : nullptr; }
static void engTrans(u8 *ctx, u8 *rt, int st) { u8 *rec = resOf(rt); if (!rec) return; ((void (*)(u8 *, u8 *, int))(g_base + RVA_RES_TRANSITION))(ctx, rec, st); ((void (*)(u8 *))(g_base + RVA_CTX_FLUSHBARRIERS))(ctx); }
static std::atomic<u64> velSplit{0};
static ID3D12Resource *nativeOf(u8 *rt);
static bool ensureSet(TexSet &ts, ID3D12Resource *nat);
static ID3D12GraphicsCommandList *listOf(u8 *ctx); static void engTrans(u8 *ctx, u8 *rec, int st);
// BUG-29 (0.9.6): the velocity buffer AA+0xEA90 is not TAA-private: other temporal passes (shadows moved with the
// head in 0.9.5) read it together with THEIR cross-eye history. on == 1: the pass runs twice - this eye's camera
// first, saved into velo.t[0] for the TAA only - then the normal camera, which is what everyone else sees.
static void hVel(u8 *aa, u8 *ctx, u8 *cam) {
    Rep r; const int m = on.load();
    if (((m & 4) || dlss::velWanted()) && g_cfg.xr == 4 && cam == *(u8 **)(g_base + RVA_CAMA) && repFor(cam, r)) {
        ID3D12GraphicsCommandList *gl = listOf(ctx); ID3D12Resource *nat = gl ? nativeOf(aa + 0xEA90) : nullptr;
        bool ok = false;
        if (nat) { EnterCriticalSection(&bcs); ok = ensureSet(velo, nat); LeaveCriticalSection(&bcs); }
        if (ok) {
            oVel(aa, ctx, fakeCam(cam, r));
            engTrans(ctx, aa + 0xEA90, D3D12_RESOURCE_STATE_COPY_SOURCE);
            gl->CopyResource(velo.t[0], nat); velo.ok[0] = true; velFix++; velSplit++;
        }
    }
    oVel(aa, ctx, cam);
}
typedef void (*FnRes)(u8 *, u8 *); static FnRes oRes;
static void hRes(u8 *aa, u8 *ctx) {
    if (!on.load() || g_cfg.xr != 4 || aa != g_base + RVA_AA_OBJ || !ctx) { oRes(aa, ctx); return; }
    // BUG-31 (0.9.7): AA+0x19320 is the 0xA8-byte target RECORD (bound with 0x4DBE20); its texture handle - the one
    // TAA_Draw gets as history - is record+0x88 = AA+0x193A8 (0.9.6 log: +0x19320 -> "resource" 0xbf80000000000001)
    u8 *rec = aa + 0x193A8, *vrec = aa + 0xEA90; u8 *cam = *(u8 **)(g_base + RVA_CAMA); Rep r{};
    const bool have = cam && repFor(cam, r);
    ID3D12GraphicsCommandList *gl = listOf(ctx);
    ID3D12Resource *nat = have && gl ? nativeOf(rec) : nullptr, *vnat = have && gl && (on.load() & 4) ? nativeOf(vrec) : nullptr;
    { static int once = 0; if (have && gl && !once) { once = 1; u8 *hr = resOf(rec), *vr = resOf(vrec);
        logf("taa: history RT %p -> resource record %p -> native %p; velocity RT %p -> %p -> native %p; list %p", (void *)rec, (void *)hr, (void *)nat, (void *)vrec, (void *)vr, (void *)nativeOf(vrec), (void *)gl); } }
    EnterCriticalSection(&bcs);
    const bool okTex = nat && ensureSet(hist, nat);
    const bool okVel = vnat && velo.src == vnat && velo.ok[0] && velo.t[1];
    const bool reset = *(u8 *)(aa + 0x1D40E) != 0;
    bool restored = false; const int e = r.eye;
    if (okTex && reset) { hist.ok[0] = hist.ok[1] = false; resets++; }
    if (okTex && !reset && hist.ok[e]) {   // this eye's previous scene colour back into the history record
        engTrans(ctx, rec, D3D12_RESOURCE_STATE_COPY_DEST);
        gl->CopyResource(nat, hist.t[e]); restored = true; restores++;
    }
    // all-or-nothing (playbook STR-015: matrices and texture are one bank): this eye's reprojection and velocity are
    // used ONLY when this eye's history texture was actually restored this frame; otherwise the engine's own
    // cross-eye chain runs unchanged (0.9.6 ghosts = same-eye motion on cross-eye history)
    const bool useVel = okVel && restored;
    if (useVel) {   // TAA reads this eye's velocity; the shared one is parked in velo.t[1] and put back after
        engTrans(ctx, vrec, D3D12_RESOURCE_STATE_COPY_SOURCE); gl->CopyResource(velo.t[1], vnat);
        engTrans(ctx, vrec, D3D12_RESOURCE_STATE_COPY_DEST); gl->CopyResource(vnat, velo.t[0]);
    }
    LeaveCriticalSection(&bcs);
    tl_sameEye = restored;
    oRes(aa, ctx);
    tl_sameEye = false;
    EnterCriticalSection(&bcs);
    if (okTex && hist.src == nat) {      // the record now holds THIS eye's scene colour: keep it for this eye's next frame
        engTrans(ctx, rec, D3D12_RESOURCE_STATE_COPY_SOURCE);
        if (restored) barrier(gl, hist.t[e], D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
        gl->CopyResource(hist.t[e], nat); hist.ok[e] = true; saves++;
    }
    if (useVel) {
        engTrans(ctx, vrec, D3D12_RESOURCE_STATE_COPY_DEST);
        barrier(gl, velo.t[1], D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE);
        gl->CopyResource(vnat, velo.t[1]); velSwaps++;
    }
    LeaveCriticalSection(&bcs);
}
} // namespace taa
// ---------------- BUG-37 (0.11.0): per-eye DLSS (playbook ch19 #dlss-in-vr, ch14 "temporal upscalers need per-eye
// history") ----------------
// RE: StreamlinePass_cb 0x60F9E0 is the upscaler pass. Cam_BuildSLConstants 0x807140(CAM-A) builds sl::Constants on
// the stack from the camera (+0x200 projection, +0x2C0..+0x2FC the prev-clip reprojection that Cam_TemporalUpdate
// builds, +0x550, +0x694 jitter, +0x7D8 cut; depthInverted = cameraMotionIncluded = 1) and calls slSetConstants with
// a ViewportHandle {0x171b6435-9b3c-4fc8-9994-fbe52569aaa4} value 0. The DLSS motion-vector input is AA+0xEA90 (the
// AA_Velocity output), depth = [frame+0x18]+0x88+(+0x620)*0xA8. One viewport = one DLSS history, so under xr=4 the
// two eyes shared it (the TAA trap in its most expensive form).
// Per eye, the same three halves that fixed the TAA: (1) history - the right eye's frames go to viewport value+OFF:
// every slSetTag / slSetConstants / slEvaluateFeature / slDLSSSetOptions of a right-eye frame is remapped, and the
// game's DLSS options are applied to both viewports; nothing is remapped until the second viewport has options
// (fail closed); (2) constants - Cam_BuildSLConstants gets the camera copy with this eye's reprojection;
// (3) motion vectors - AA_Velocity's this-eye run is swapped in around the upscaler pass only.
namespace dlss {
struct VP { void *next; u8 type[16]; size_t ver; u32 value; u32 pad; };
static const u8 kVpGuid[16] = {0x35, 0x64, 0x1b, 0x17, 0x3c, 0x9b, 0xc8, 0x4f, 0x99, 0x94, 0xfb, 0xe5, 0x25, 0x69, 0xaa, 0xa4};
static const u32 OFF = 0x40;
static std::atomic<int> mode{1};   // 0 off (shared history), 1 history + constants + motion vectors, 2 history + constants
static std::atomic<bool> optsSecond{false}, hooked{false}; static std::atomic<u64> lastEval{0};
static std::atomic<u64> nTag{0}, nConst{0}, nEval{0}, nOpt{0}, nRemap{0}, nConstFake{0}, nVelSwap{0}, nEvalDlss{0};
static bool active() { return g_cfg.xr == 4 && mode.load() != 0; }
static bool velWanted() { return active() && mode.load() == 1 && GetTickCount64() - lastEval.load() < 2000; }
static bool rightEye() { return g_camEye.load() == 1; }
// BUG-39 (0.11.1): this game runs DLSS through NGX directly, not through Streamline (0.11.0 log: zero SL constants /
// evaluates while DLSS Quality was on; the exe statically links the NGX shim and exports NVSDK_NGX_D3D12_*; the
// engine wrapper calls CreateFeature at 0x469EA8, EvaluateFeature_C at 0x46AE54, ReleaseFeature at 0x46AED8). An NGX
// feature handle IS the DLSS history, so the right eye gets its own handle.
static std::atomic<void *> ngxH0{nullptr}, ngxH1{nullptr}; static std::atomic<bool> ngxFailed{false};
static std::atomic<u64> nNgxEval{0}, nNgxRight{0}, nNgxCreate{0};
static bool perEye() { return active() && (optsSecond.load() || ngxH1.load() != nullptr); }   // all-or-nothing: per-eye halves only with per-eye history
static bool remapNow() { return active() && optsSecond.load() && rightEye(); }
static void copyVp(VP &o, const void *in) { memcpy(&o, in, sizeof(VP)); o.value += OFF; }

// NGX: NVSDK_NGX_D3D12_CreateFeature(cmd, feature, params, &handle) / EvaluateFeature_C(cmd, handle, params, cb) /
// ReleaseFeature(handle). Result success = (r & 0xFFF00000) != 0xBAD00000. Feature 1 = SuperSampling (DLSS).
typedef u32 (*FnNgxCreate)(void *cmd, u32 feature, void *params, void **out); static FnNgxCreate oNgxCreate;
typedef u32 (*FnNgxEval)(void *cmd, void *h, void *params, void *cb); static FnNgxEval oNgxEval;
typedef u32 (*FnNgxRelease)(void *h); static FnNgxRelease oNgxRelease;
static bool ngxOk(u32 r) { return (r & 0xFFF00000u) != 0xBAD00000u; }
static u32 hNgxCreate(void *cmd, u32 feature, void *params, void **out) {
    u32 r = oNgxCreate(cmd, feature, params, out);
    if (feature == 1 && ngxOk(r) && out && *out) {
        void *old = ngxH0.exchange(*out); nNgxCreate++;
        if (old && old != *out) { void *h1 = ngxH1.exchange(nullptr); if (h1) oNgxRelease(h1); ngxFailed = false; }
        logf("dlss: game created DLSS feature %p (result 0x%08x)", *out, r);
    }
    return r;
}
static u32 hNgxEval(void *cmd, void *h, void *params, void *cb) {
    nNgxEval++; lastEval = GetTickCount64();
    if (active() && h && h == ngxH0.load() && rightEye()) {
        void *h1 = ngxH1.load();
        if (!h1 && !ngxFailed.load()) {   // first right-eye frame: a second DLSS instance from the same parameters
            u32 r = oNgxCreate(cmd, 1, params, &h1);
            if (ngxOk(r) && h1) { ngxH1 = h1; logf("dlss: right-eye DLSS feature %p created (result 0x%08x) -> each eye has its own DLSS history", h1, r); }
            else { ngxFailed = true; h1 = nullptr; logf("dlss: creating the right-eye DLSS feature failed 0x%08x -> both eyes keep sharing", r); }
        }
        if (h1) { nNgxRight++; return oNgxEval(cmd, h1, params, cb); }
    }
    return oNgxEval(cmd, h, params, cb);
}
static u32 hNgxRelease(void *h) {
    if (h && h == ngxH0.load()) { ngxH0 = nullptr; void *h1 = ngxH1.exchange(nullptr); if (h1) oNgxRelease(h1); ngxFailed = false; logf("dlss: game released its DLSS feature -> right-eye feature released too"); }
    return oNgxRelease(h);
}
typedef int (*FnSetTag)(const void *vp, const void *tags, u32 n, void *cmd); static FnSetTag oSetTag;
static int hSetTag(const void *vp, const void *tags, u32 n, void *cmd) {
    nTag++; if (vp && remapNow()) { VP c; copyVp(c, vp); nRemap++; return oSetTag(&c, tags, n, cmd); }
    return oSetTag(vp, tags, n, cmd);
}
typedef int (*FnSetConst)(const void *consts, const void *frame, const void *vp); static FnSetConst oSetConst;
static int hSetConst(const void *consts, const void *frame, const void *vp) {
    nConst++; if (vp && remapNow()) { VP c; copyVp(c, vp); nRemap++; return oSetConst(consts, frame, &c); }
    return oSetConst(consts, frame, vp);
}
typedef int (*FnEval)(u32 feature, const void *frame, const void **inputs, u32 n, void *cmd); static FnEval oEval;
static int hEval(u32 feature, const void *frame, const void **inputs, u32 n, void *cmd) {
    nEval++; if (feature == 0) { nEvalDlss++; lastEval = GetTickCount64(); }
    if (inputs && n && n < 32 && remapNow()) {
        const void *arr[32]; VP c{}; bool sw = false;
        for (u32 i = 0; i < n; i++) {
            arr[i] = inputs[i];
            const u8 *b = (const u8 *)inputs[i];
            if (!sw && b && readable(b, sizeof(VP)) && !memcmp(b + 8, kVpGuid, 16)) { copyVp(c, b); arr[i] = &c; sw = true; }
        }
        if (sw) { nRemap++; return oEval(feature, frame, arr, n, cmd); }
    }
    return oEval(feature, frame, inputs, n, cmd);
}
typedef int (*FnSetOpt)(const void *vp, const void *opts); static FnSetOpt oSetOpt;
static int hSetOpt(const void *vp, const void *opts) {
    nOpt++; int r = oSetOpt(vp, opts);
    if (vp && g_cfg.xr == 4) {   // the same options for the right eye's viewport (always - the game may set them once)
        VP c; copyVp(c, vp); int r2 = oSetOpt(&c, opts);
        if (r2 == 0 && !optsSecond.exchange(true)) logf("dlss: options applied to the right-eye viewport %u", c.value);
        if (r2 != 0) { static int told = 0; if (!told++) logf("dlss: slDLSSSetOptions(viewport %u) failed %d -> right eye keeps the shared viewport", c.value, r2); }
    }
    return r;
}
typedef int (*FnGetFn)(u32 feature, const char *name, void *&fn); static FnGetFn oGetFn;
static void hookSetOptions(void *fn) {
    static std::atomic<bool> done{false}; if (!fn || done.exchange(true)) return;
    MH_STATUS a = MH_CreateHook(fn, (void *)hSetOpt, (void **)&oSetOpt), b = a == MH_OK ? MH_EnableHook(fn) : a;
    logf("dlss: slDLSSSetOptions %p hook %s/%s", fn, MH_StatusToString(a), MH_StatusToString(b));
}
static int hGetFn(u32 feature, const char *name, void *&fn) {
    int r = oGetFn(feature, name, fn);
    if (r == 0 && feature == 0 && name && !strcmp(name, "slDLSSSetOptions")) hookSetOptions(fn);
    return r;
}
static void install() {
    HMODULE sl = GetModuleHandleW(L"sl.interposer.dll"); if (!sl) { logf("dlss: sl.interposer.dll not loaded -> per-eye DLSS unavailable"); return; }
    struct { const char *n; void *det; void **orig; } h[] = {{"slSetTag", (void *)hSetTag, (void **)&oSetTag}, {"slSetConstants", (void *)hSetConst, (void **)&oSetConst},
                                                           {"slEvaluateFeature", (void *)hEval, (void **)&oEval}, {"slGetFeatureFunction", (void *)hGetFn, (void **)&oGetFn}};
    int ok = 0;
    for (auto &x : h) { void *f = (void *)GetProcAddress(sl, x.n); MH_STATUS st = f ? MH_CreateHook(f, x.det, x.orig) : MH_ERROR_FUNCTION_NOT_FOUND; if (st == MH_OK) st = MH_EnableHook(f); ok += st == MH_OK; logf("dlss: %s %p -> %s", x.n, f, MH_StatusToString(st)); }
    hooked = ok == 4;
    if (hooked) { void *fn = nullptr; if (oGetFn(0, "slDLSSSetOptions", fn) == 0) hookSetOptions(fn); else logf("dlss: DLSS not loaded yet (slDLSSSetOptions hooked when the game asks for it)"); }
}
// Cam_BuildSLConstants 0x807140: this eye's reprojection for the DLSS constants
typedef void (*FnSLC)(u8 *); static FnSLC oSLC;
static void hSLC(u8 *cam) {
    taa::Rep r;
    if (perEye() && cam == *(u8 **)(g_base + RVA_CAMA) && taa::repFor(cam, r)) { nConstFake++; oSLC(taa::fakeCam(cam, r)); return; }
    oSLC(cam);
}
// around the upscaler pass: this eye's motion vectors in AA+0xEA90, the shared ones parked and put back after
static thread_local bool tl_swapped = false;
static void prePass(u8 *ctx) {
    tl_swapped = false;
    if (!velWanted() || !perEye() || !ctx) return;
    u8 *aa = g_base + RVA_AA_OBJ, *vrec = aa + 0xEA90; u8 *cam = *(u8 **)(g_base + RVA_CAMA); taa::Rep r;
    if (!cam || !taa::repFor(cam, r)) return;
    ID3D12GraphicsCommandList *gl = taa::listOf(ctx); ID3D12Resource *vnat = gl ? taa::nativeOf(vrec) : nullptr; if (!vnat) return;
    EnterCriticalSection(&taa::bcs);
    const bool ok = taa::velo.src == vnat && taa::velo.ok[0] && taa::velo.t[1];
    if (ok) {
        taa::engTrans(ctx, vrec, D3D12_RESOURCE_STATE_COPY_SOURCE); gl->CopyResource(taa::velo.t[1], vnat);
        taa::engTrans(ctx, vrec, D3D12_RESOURCE_STATE_COPY_DEST); gl->CopyResource(vnat, taa::velo.t[0]);
        tl_swapped = true; nVelSwap++;
    }
    LeaveCriticalSection(&taa::bcs);
}
static void postPass(u8 *ctx) {
    if (!tl_swapped) return; tl_swapped = false;
    u8 *vrec = g_base + RVA_AA_OBJ + 0xEA90; ID3D12GraphicsCommandList *gl = taa::listOf(ctx); ID3D12Resource *vnat = gl ? taa::nativeOf(vrec) : nullptr; if (!vnat) return;
    EnterCriticalSection(&taa::bcs);
    taa::engTrans(ctx, vrec, D3D12_RESOURCE_STATE_COPY_DEST);
    taa::barrier(gl, taa::velo.t[1], D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE);
    gl->CopyResource(vnat, taa::velo.t[1]);
    LeaveCriticalSection(&taa::bcs);
}
} // namespace dlss



// ---------------- BUG-23 (0.9.2): per-consumer camera-cut overrides ----------------
// 0.9.1 headset A/B: history KEPT -> faint double edge on near objects + objects/shadows in ONE eye only (flicker);
// history RESET (global cut every frame) -> both eyes correct, but bright flashes of lights/objects (monitor too).
// The cut flag cam+0x7D8 is read by ~20 consumers (decompiled: out/re/decomp/cut). Under synchronized-sequential the
// "previous frame" is the other eye, and the consumers fall into classes (playbook ch14):
//   V  visibility/culling that reuses the previous frame's view (HiZ/occlusion, light lists): from the other eye's
//      viewpoint it culls what this eye can see -> objects/shadows missing in one eye (#cpu-visibility-history,
//      "results deferred a frame"). Must see a cut every frame.
//   T1 TAA / AA / upscaler / velocity history: cross-eye reprojection works but leaves a faint IPD ghost.
//   T2 lighting / volumetric / exposure temporal accumulation: reset every frame = unconverged noise (the flashes).
// Each reader instruction `cmp [reg+0x7D8], 0|r32` is redirected (5-byte jmp + nops) to a stub near the image:
//   push rax; lock inc [hits]; mov eax,[reg+0x7D8]; or eax,[force_group]; cmp eax, 0|r32; pop rax; jmp back
// so a group can be forced to "cut" without touching the camera (the engine's own cuts still pass through).
namespace cutp {
enum { G_VV = 0, G_VO = 1, G_L = 2, G_T1 = 3, G_T2 = 4, NG = 5 };
struct Site { u32 rva; u8 len; u8 base; int cmpReg; int group; const char *name; const char *bytes; };
static const Site kSites[] = {
    {0x9BB95A, 7, 1, 15, G_VV, "visibility view flag (Cut 0x9BB4F0, +0xE1 occlusion reset)", "44 39 B9 D8 07 00 00"},
    {0x9E5393, 9, 12, -1, G_VO, "occlusion setup RV_9E5350 (previous-slot data)", "41 83 BC 24 D8 07 00 00 00"},
    {0x631B92, 7, 0, -1, G_L, "light list rebuild List2_Build 0x6319C0", "83 B8 D8 07 00 00 00"},
    {0x618D9A, 7, 6, -1, G_T1, "TAA history valid / jitter 0x618D30", "83 BE D8 07 00 00 00"},
    {0x6149FE, 9, 12, -1, G_T1, "AA history pyramid reset 0x614970", "41 83 BC 24 D8 07 00 00 00"},
    {0x610CD0, 7, 0, -1, G_T1, "reprojection/velocity pass 0x610CC0", "83 B8 D8 07 00 00 00"},
    {0x609BDD, 7, 0, -1, G_T1, "post pass 0x609B90", "83 B8 D8 07 00 00 00"},
    {0x60FD07, 7, 6, 13, G_T1, "Streamline/DLSS reset 0x60F9E0", "44 39 AE D8 07 00 00"},
    {0xAF98C0, 7, 3, -1, G_T1, "AA mode history 0xAF9850", "83 BB D8 07 00 00 00"},
    {0x63D246, 7, 7, -1, G_T2, "temporal blend 0.95 (0x63C130)", "83 BF D8 07 00 00 00"},
    {0x63D8BF, 7, 14, 3, G_T2, "temporal params (0x63D4C0, exposure)", "41 39 9E D8 07 00 00"},
    {0x63E082, 7, 13, 6, G_T2, "temporal params (0x63D960)", "41 39 B5 D8 07 00 00"},
    {0x60C922, 8, 15, -1, G_T2, "temporal pass 0x60C7F0", "41 83 BF D8 07 00 00 00"},
};
static const int NS = (int)(sizeof kSites / sizeof kSites[0]);
struct Page { volatile LONG force[NG]; volatile LONG hits[32]; u8 code[NS][64]; };
static Page *pg = nullptr; static bool applied = false; static u8 orig[NS][16];
static const char *gname[NG] = {"VV view occlusion", "VO occlusion setup", "L light lists", "T1 TAA", "T2 lighting"};
static bool sigOk(u8 *base, const Site &st) {
    const char *q = st.bytes; int i = 0;
    while (*q) { while (*q == ' ') q++; if (!*q) break; int v = 0; for (int k = 0; k < 2; k++) { char c = q[k]; v = v * 16 + (c >= 'A' ? c - 'A' + 10 : c - '0'); } if (base[st.rva + i] != (u8)v) return false; q += 2; i++; }
    return i == st.len;
}
static Page *allocNear(u8 *img) {   // within +-2 GB of the image so rel32 jumps/rip-relative operands reach
    SYSTEM_INFO si; GetSystemInfo(&si); u64 gran = si.dwAllocationGranularity;
    for (u64 d = 0x10000000; d < 0x70000000; d += gran * 16) {
        for (int dir = 0; dir < 2; dir++) {
            u32 e = *(u32 *)(img + 0x3C), soi = *(u32 *)(img + e + 0x50);   // SizeOfImage
            u64 a = dir ? (u64)img + soi + d : (u64)img - d; a = (a + gran - 1) & ~(gran - 1);
            void *p = VirtualAlloc((void *)a, sizeof(Page), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
            if (p) { if ((u64)p + sizeof(Page) > (u64)img + 0x7F000000 || (u64)img + soi > (u64)p + 0x7F000000) { VirtualFree(p, 0, MEM_RELEASE); continue; } return (Page *)p; }
        }
    }
    return nullptr;
}
static int emit(u8 *c, const Site &st, int idx, u64 ret) {
    int n = 0; auto put32 = [&](u32 v) { memcpy(c + n, &v, 4); n += 4; }; auto rel = [&](const void *t) { return (u32)((u64)t - ((u64)c + n + 4)); };
    c[n++] = 0x50;                                                                           // push rax
    c[n++] = 0xF0; c[n++] = 0xFF; c[n++] = 0x05; put32(rel((const void *)&pg->hits[idx]));     // lock inc dword [rip+hits]
    if (st.base >= 8) c[n++] = 0x41; c[n++] = 0x8B; c[n++] = (u8)(0x80 | (st.base & 7)); if ((st.base & 7) == 4) c[n++] = 0x24; put32(0x7D8);   // mov eax,[base+0x7D8]
    c[n++] = 0x0B; c[n++] = 0x05; put32(rel((const void *)&pg->force[st.group]));            // or eax,[rip+force]
    if (st.cmpReg < 0) { c[n++] = 0x83; c[n++] = 0xF8; c[n++] = 0x00; }                      // cmp eax,0
    else { if (st.cmpReg >= 8) c[n++] = 0x44; c[n++] = 0x39; c[n++] = (u8)(0xC0 | ((st.cmpReg & 7) << 3)); }   // cmp eax,r32
    c[n++] = 0x58;                                                                           // pop rax (flags kept)
    c[n++] = 0xFF; c[n++] = 0x25; put32(0); memcpy(c + n, &ret, 8); n += 8;                  // jmp [rip+0] -> ret
    return n;
}
// Suspend every other thread of the process while code bytes change (none may sit inside a patched instruction).
static void withThreadsFrozen(void (*fn)(void *), void *arg) {
    HANDLE th[512]; int nt = 0; DWORD me = GetCurrentThreadId(), pid = GetCurrentProcessId();
    HANDLE sn = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (sn != INVALID_HANDLE_VALUE) {
        THREADENTRY32 te{}; te.dwSize = sizeof te;
        for (BOOL ok = Thread32First(sn, &te); ok && nt < 512; ok = Thread32Next(sn, &te))
            if (te.th32OwnerProcessID == pid && te.th32ThreadID != me) { HANDLE h = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT, FALSE, te.th32ThreadID); if (h) { if (SuspendThread(h) != (DWORD)-1) th[nt++] = h; else CloseHandle(h); } }
        CloseHandle(sn);
    }
    for (int i = 0; i < nt; i++) {   // a thread stopped inside a site's bytes would resume on garbage: step it out first
        for (int tries = 0; tries < 50; tries++) {
            CONTEXT c{}; c.ContextFlags = CONTEXT_CONTROL; if (!GetThreadContext(th[i], &c)) break;
            bool inside = false; for (auto &st : kSites) inside |= c.Rip > (u64)(g_base + st.rva) && c.Rip < (u64)(g_base + st.rva + st.len);
            if (!inside) break; ResumeThread(th[i]); Sleep(0); SuspendThread(th[i]);
        }
    }
    fn(arg);
    for (int i = 0; i < nt; i++) { ResumeThread(th[i]); CloseHandle(th[i]); }
}
static void writeAll(void *on) {
    for (int i = 0; i < NS; i++) {
        u8 *a = g_base + kSites[i].rva; DWORD op; VirtualProtect(a, 16, PAGE_EXECUTE_READWRITE, &op);
        if (on) { u8 j[16]; j[0] = 0xE9; u32 r = (u32)((u64)pg->code[i] - ((u64)a + 5)); memcpy(j + 1, &r, 4); for (int k = 5; k < kSites[i].len; k++) j[k] = 0x90; memcpy(a, j, kSites[i].len); }
        else memcpy(a, orig[i], kSites[i].len);
        VirtualProtect(a, 16, op, &op); FlushInstructionCache(GetCurrentProcess(), a, 16);
    }
}
static bool apply() {
    if (applied) return true;
    for (auto &st : kSites) if (!sigOk(g_base, st)) { logf("cut overrides: site 0x%X (%s) bytes differ -> none applied (fail closed)", st.rva, st.name); return false; }
    if (!pg) pg = allocNear(g_base);
    if (!pg) { logf("cut overrides: no stub page within 2 GB of the image -> none applied"); return false; }
    for (int i = 0; i < NS; i++) { memcpy(orig[i], g_base + kSites[i].rva, kSites[i].len); emit(pg->code[i], kSites[i], i, (u64)(g_base + kSites[i].rva + kSites[i].len)); }
    FlushInstructionCache(GetCurrentProcess(), pg, sizeof(Page));
    withThreadsFrozen(writeAll, (void *)1); applied = true;
    logf("cut overrides: %d camera-cut readers redirected (stubs at %p)", NS, (void *)pg);
    return true;
}
static void remove() { if (applied) { withThreadsFrozen(writeAll, nullptr); applied = false; } }
static void setForce(int g, int v) { if (pg) InterlockedExchange(&pg->force[g], v); }
static u64 groupHits(int g) { u64 h = 0; if (pg) for (int i = 0; i < NS; i++) if (kSites[i].group == g) h += (u32)pg->hits[i]; return h; }
} // namespace cutp

// History presets (F6 cycles; ini history_mode; the number is shown as white squares in the headset for 3 s).
// global = force the camera's own cut flag every VR frame (0.9.0). 0.9.2 headset A/B (BUG-24): forcing V (view
// occlusion + occlusion setup + light lists together) = light flicker; forcing T1 = shadows/lights/objects missing per
// eye (T1 holds the depth pyramid the GPU occlusion test reads); KEPT = correct lighting but per-eye culling pop-in.
// -> V split three ways to find the one that fixes culling without the light flicker.
// 0.9.3 headset (BUG-26): forcing the occlusion setup (VO) = no pop-in + light flicker; view flag (VV) irrelevant;
// KEPT = pop-in. -> 0.9.4 setting 1 = per-eye occlusion history (occ::), nothing forced.
// 0.9.4 headset: setting 1 = no light flicker, no pop-in; faint double edge on very close objects -> 0.9.5 per-eye TAA.
// 0.9.5 headset: monitor fixed; setting 1 "shadows moving as I move my head" -> BUG-29 (shared velocity buffer).
struct HistPreset { const char *name; int global, vv, vo, l, t1, t2, occ, taa; };
static const HistPreset kPresets[] = {
    // 0.9.8 headset: per-eye TAA (history + reprojection + velocity) = object outlines left in place when the head
    // moves; engine TAA = great -> engine TAA is the default again, the per-eye TAA is split into its parts to find
    // which one is wrong (playbook STR-015: toggle per piece until the artifact goes, then freeze as data).
    // taa bits: 1 history bank, 2 camera constants (reprojection), 4 velocity
    // 0.9.9 headset bisection (BUG-33): 3 (history + reprojection) and 5 (history) clean, 4 (+ velocity) ghosts ->
    // the private velocity is the broken piece: AA_Velocity reprojects the PREVIOUS frame's depth from the ring
    // [frame+0x18]+0x88 + (+0x620)*0xA8, i.e. the other eye's depth; with this eye's matrices that is garbage.
    // Final per-eye TAA = this eye's history + this eye's reprojection constants; engine velocity untouched.
    // 0.18.9: occ 2 = per-eye occlusion by TRACKED slot ownership (default); occ 1 = the 0.9.4 parity-assumed bank (preset 6, for A/B)
    {"1 PER-EYE OCCLUSION (tracked) + PER-EYE TAA", 0, 0, 0, 0, 0, 0, 2, 3},
    {"2 PER-EYE OCCLUSION (tracked), engine TAA", 0, 0, 0, 0, 0, 0, 2, 0},
    {"3 PER-EYE OCCLUSION (tracked), TAA reset every frame", 0, 0, 0, 0, 1, 0, 2, 0},
    {"4 KEPT: everything cross-eye", 0, 0, 0, 0, 0, 0, 0, 0},
    {"5 RESET: global cut every frame (0.9.0)", 1, 0, 0, 0, 0, 0, 0, 0},
    {"6 PER-EYE OCCLUSION (0.9.4 bank, parity assumed) + PER-EYE TAA", 0, 0, 0, 0, 0, 0, 1, 3},
    // 0.18.11 (BUG-76, owner: "if grass or other objects is in the centre of one eye but not the other, it gets culled in the other
    // eye"): preset 4 tests every object against the OTHER eye's depth pyramid (its previous frame). Whatever that eye could not see -
    // behind Jin's arm, a grass clump, a post at arm's length (6.4 cm of parallax is large that close) - is culled in this eye. The
    // presets with per-eye occlusion already returned no occlusion view to all ~60 Occ_View callers per frame (OCCVIEW/s "none"), so
    // their left-eye pop-in was never the depth test: it came with the fast-head-turn rule (an occlusion cut forced on the first eye of
    // a pair during fast turns, whose cut path tests with the CURRENT camera against an old pyramid). Preset 7: no previous-frame
    // occlusion test for any pass and no fast-turn cut: every object is culled only by THIS eye's own frustum (playbook ch14:
    // "reporting visible can never wrongly delete geometry, only fail to save work"), engine TAA as in preset 4.
    {"7 PER-EYE CULLING: own frustum per eye, no cross-eye occlusion, engine TAA", 0, 0, 0, 0, 0, 0, 3, 0},
};
static const int NPRESET = (int)(sizeof kPresets / sizeof kPresets[0]);
static std::atomic<int> g_preset{0}; static std::atomic<u64> g_presetShownAt{0};
static int g_presetVo = 0;
void occ::setVoFast(bool f) { cutp::setForce(cutp::G_VO, g_presetVo | ((f && occ::fastMode.load() == 1) ? 1 : 0)); }
static void applyPreset(int k) {
    k = ((k % NPRESET) + NPRESET) % NPRESET; g_preset = k; const HistPreset &p = kPresets[k];
    m_histReset = p.global;
    occ::on = p.occ; taa::on = p.taa;
    g_presetVo = p.vo; cutp::setForce(cutp::G_VV, p.vv); cutp::setForce(cutp::G_VO, p.vo | ((occ::fast.load() && occ::fastMode.load() == 1) ? 1 : 0)); cutp::setForce(cutp::G_L, p.l); cutp::setForce(cutp::G_T1, p.t1); cutp::setForce(cutp::G_T2, p.t2);
    g_presetShownAt = GetTickCount64();
}
static void hClock(float *c) {
    if (!g_freezeNow.load() || (u8 *)c != g_base + RVA_CLOCK) { oClock(c); return; }
    c[0] = 1e-4f; c[10] = c[0xc] * 1e-4f; c[2] = 0; c[6] = 0; c[0xe] = 0; c[0x12] = 0;
}
static void uiDecide();
// BUG-70 (0.17.4: left-eye-only pop-in and stutter that a left-eye history cut did not fix). The engine presents on its own
// render thread (census: tick tid != present tid), so frame N's render passes are recorded WHILE the game thread already runs
// tick N+1. Those passes read game-thread globals at record time: the occlusion view 0x141EDBD70 (RV_OcclusionSetup writes it
// every tick; the grass / foliage / tile culling passes read it through Occ_View 0x9E5960), CAM-A itself, the eye id. Under
// xr=4 tick N+1 of the left frame N is the FROZEN right tick, which is short: it composes the right eye into CAM-A and rewrites
// the occlusion view while the left frame is still being recorded, so the left eye is culled and lit with right-eye data (the
// right frame is recorded during the long simulated tick, before its globals change). Fix (playbook STR-012: carry the eye with
// the frame, never let the next eye's state leak into it): the right tick starts only once the left frame has been presented
// (render_sync = 1, default; 2 = before every stereo tick; 0 = off). Timeout 40 ms; SYNC/s logs waits and their cost.
static std::atomic<u32> g_presFrame{0}; static HANDLE g_presEv = nullptr;
static std::atomic<u64> g_syncNeeded{0}, g_syncAlready{0}, g_syncTimeouts{0}, g_syncWaitUs{0};
static int g_syncMiss = 0, g_syncPause = 0;
static void waitPresented(u32 f) {
    if (g_syncPause > 0) { g_syncPause--; return; }   // presents stalled (loading, minimised): do not slow every tick down
    if ((int)(g_presFrame.load() - f) >= 0) { g_syncAlready++; g_syncMiss = 0; return; }
    g_syncNeeded++; LARGE_INTEGER q0, q1, fq; QueryPerformanceCounter(&q0); QueryPerformanceFrequency(&fq);
    for (;;) {
        if (g_presEv) WaitForSingleObject(g_presEv, 2); else Sleep(1);
        if ((int)(g_presFrame.load() - f) >= 0) break;
        QueryPerformanceCounter(&q1); if ((q1.QuadPart - q0.QuadPart) * 1000 > fq.QuadPart * 40) { g_syncTimeouts++; if (++g_syncMiss >= 3) { g_syncMiss = 0; g_syncPause = 300; } break; }
    }
    if ((int)(g_presFrame.load() - f) >= 0) g_syncMiss = 0;
    QueryPerformanceCounter(&q1); g_syncWaitUs += (u64)((q1.QuadPart - q0.QuadPart) * 1000000 / fq.QuadPart);
}
// BUG-74 (0.18.2): pre_compose (the left-eye camera composed before the simulated tick) ran ~60 times/s in the 0.18.3/0.18.4
// sessions and the pop-in stayed `[HEADSET]` -> removed in 0.18.5 (owner: no cost without a fix).
// 0.18.62 (owner: "when I looked or moved in certain directions the game got extremely laggy and jittery despite no frame drops - it also
// affected the game's menus"; host log: its own loop fell from a steady 72 fps to 43-50 while the game ran 60-70 stereo pairs/s, and the
// menus ran ~280 frames/s): the game and the host share one GPU, and a game rendering as fast as it can starves the host's composition
// (judder in the headset although the game's own rate looks fine). The game is now paced: at most xr_game_fps_cap stereo pairs per second
// in VR (each pair = two renders) and xr_menu_fps_cap frames per second on the flat theatre screen (menus, map, videos). 0 = uncapped.
static std::atomic<u64> g_capWaits{0}, g_capWaitUs{0};
static void capPace(int fps) {
    if (fps <= 0) return; static LARGE_INTEGER fq{}, next{}; static bool tp = false; if (!fq.QuadPart) QueryPerformanceFrequency(&fq); if (!tp) { tp = true; typedef UINT (WINAPI *TBP)(UINT); HMODULE wm = LoadLibraryW(L"winmm.dll"); TBP f = wm ? (TBP)(void *)GetProcAddress(wm, "timeBeginPeriod") : nullptr; if (f) f(1); }
    const LONGLONG step = fq.QuadPart / fps; LARGE_INTEGER now; QueryPerformanceCounter(&now);
    if (!next.QuadPart || now.QuadPart - next.QuadPart > step * 4) next.QuadPart = now.QuadPart;   // first, or far behind (a load): no catch-up burst
    if (next.QuadPart > now.QuadPart) { const LONGLONG t0 = now.QuadPart; g_capWaits++;
        for (;;) { QueryPerformanceCounter(&now); const LONGLONG left = next.QuadPart - now.QuadPart; if (left <= 0) break; if (left * 1000 / fq.QuadPart >= 2) Sleep(1); else YieldProcessor(); }
        g_capWaitUs += (u64)((now.QuadPart - t0) * 1000000 / fq.QuadPart); }
    next.QuadPart += step; }
// 0.18.64 (owner, session 19: "performance is better, but it lagged a bit when grappling"; host log: 72 fps mostly, but 55-67 in heavy
// stretches while the game held its 58 pairs/s): the cap follows the headset - once a second, if the host's compositor loop ran more
// than 4 fps under the display rate while the game was at its cap, the cap drops 3 pairs/s (not under 40); after 3 s at the full rate
// it climbs back 1 pair/s up to xr_game_fps_cap. xr_game_fps_adaptive = 0 keeps it fixed.
static void paceAdapt() {
    static u64 last = 0, lastHF = 0, lastPT = 0; static int good = 0; const u64 now = GetTickCount64(); if (now - last < 1000) return;
    const u64 hf = S ? (u64)S->hostFrames : 0, pt = g_pairTicks.load(); const double sec = last ? (now - last) / 1000.0 : 0.0;
    const double hfps = sec > 0 && hf >= lastHF ? (hf - lastHF) / sec : 0.0, pps = sec > 0 ? (pt - lastPT) / sec : 0.0; last = now; lastHF = hf; lastPT = pt;
    if (sec <= 0 || sec > 3.0 || !S) return; g_hostFps = (float)hfps;
    float dh = g_displayHz.load(); if (hfps > dh) dh = (float)hfps; else dh = dh * 0.999f; g_displayHz = dh;   // the best rate seen = the display's
    if (!g_paceAdaptive || g_cfg.xr != 4 || g_flatRender.load() || g_pairCapMax <= 0 || dh < 50.0f) return;
    if (hfps < dh - 4.0 && pps >= g_pairCap - 3) { const int nc = g_pairCap - 3 < 40 ? 40 : g_pairCap - 3; if (nc != g_pairCap) { g_pairCap = nc; g_capDowns++; } good = 0; }
    else if (hfps >= dh - 1.5) { if (++good >= 3 && g_pairCap < g_pairCapMax) { g_pairCap++; g_capUps++; good = 0; } }
    else good = 0;
}
static void hGameTick() {
    c_tick++; t_tick = GetCurrentThreadId(); paceAdapt();
    if (g_cfg.xr == 4) { if (g_flatRender.load()) capPace(g_menuCap); else if (g_pairPhase == 0) { g_pairTicks++; capPace(g_pairCap); } }
    if (g_cfg.xr != 4) { oGameTick(); obs::tick(); cbt::tick(); return; }
    uiDecide();   // 0.13.0: flat context for this tick (pause / video / no world / F10)
    if (g_flatRender.load()) { fp::snapRestore(); g_simTicks++; oGameTick(); cbt::tick(); g_pairPhase = 0; return; }   // 0.18.25: Jin's own pose back in frozen frames   // every tick simulated and shown; the next stereo pair starts with the left eye
    const bool freeze = g_pairPhase == 1 && S && hostFresh() && S->imagesOk && !S->gameRefuse;
    float *clk = (float *)(g_base + RVA_CLOCK); float saved[24];
    if (g_renderSync && t_present && t_present != t_tick && (freeze || g_renderSync >= 2)) waitPresented(frameCtr());   // the previous frame left the render thread
    if (freeze) { memcpy(saved, clk, sizeof saved); clk[2] = 0; clk[6] = 0; clk[0xe] = 0; clk[0x12] = 0; g_lockGame = true; g_freezeNow = 1; g_frozenTicks++; }
    else g_simTicks++;
    oGameTick();
    if (freeze) { g_freezeNow = 0; g_lockGame = false; memcpy(clk, saved, sizeof saved); }   // the next tick's early code sees tick A's steps again
    else cbt::tick();   // 0.18.47 the katana's blows (queued during this simulated tick) - never inside a frozen one
    g_pairPhase ^= 1;
}

static void hSetLocal(void *cam, const float *m) {
    bool isA = (u64)cam == camA();
    if (isA) c_setlocalA++; else c_setlocalOther++;
    noteCaller((u64)__builtin_return_address(0), isA);
    oSetLocal(cam, m);
}

static void rotateZ(float *v, float c, float s) { float x = v[0], y = v[1]; v[0] = c * x - s * y; v[1] = s * x + c * y; }

// OpenXR quaternion -> 3x3 (column-vector convention, xr axes)
static void quatMat(const float *q, float m[3][3]) {
    float x = q[0], y = q[1], z = q[2], w = q[3];
    m[0][0] = 1 - 2 * (y * y + z * z); m[0][1] = 2 * (x * y - z * w); m[0][2] = 2 * (x * z + y * w);
    m[1][0] = 2 * (x * y + z * w); m[1][1] = 1 - 2 * (x * x + z * z); m[1][2] = 2 * (y * z - x * w);
    m[2][0] = 2 * (x * z - y * w); m[2][1] = 2 * (y * z + x * w); m[2][2] = 1 - 2 * (x * x + y * y);
}
// Game camera local basis (forward, left, up) <-> OpenXR view axes: f = -z, l = -x, u = +y.
// C maps game-local components to xr components: xr = C * g.
static const float kC[3][3] = {{0, -1, 0}, {0, 0, 1}, {-1, 0, 0}};

// Recentre reference: yaw-only head pose (horizon stays level). Game thread only.
static GvPose g_ref; static bool g_refValid = false;
static void recentre(const GvPose &h) {
    float yaw = atan2f(2 * (h.q[3] * h.q[1] + h.q[0] * h.q[2]), 1 - 2 * (h.q[1] * h.q[1] + h.q[0] * h.q[0]));
    g_ref = h; g_ref.q[0] = 0; g_ref.q[1] = sinf(yaw / 2); g_ref.q[2] = 0; g_ref.q[3] = cosf(yaw / 2); g_refValid = true;
    logf("recentred: yaw %.1f deg, head at %.3f %.3f %.3f m", yaw * 57.2958f, h.p[0], h.p[1], h.p[2]);
}
// Pose relative to the reference, in game camera-local (f,l,u) coordinates: rotation Rg, translation dg (metres).
static void poseLocal(const GvPose &h, float Rg[3][3], float dg[3]) {
    float R[3][3], Rr[3][3]; quatMat(h.q, R); quatMat(g_ref.q, Rr);
    float Rel[3][3], d[3], dp[3] = {h.p[0] - g_ref.p[0], h.p[1] - g_ref.p[1], h.p[2] - g_ref.p[2]};
    for (int i = 0; i < 3; i++) { d[i] = 0; for (int j = 0; j < 3; j++) { Rel[i][j] = 0; for (int k = 0; k < 3; k++) Rel[i][j] += Rr[k][i] * R[k][j]; d[i] += Rr[j][i] * dp[j]; } }
    float T[3][3];
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) { T[i][j] = 0; for (int k = 0; k < 3; k++) T[i][j] += Rel[i][k] * kC[k][j]; }
    for (int i = 0; i < 3; i++) { dg[i] = 0; for (int k = 0; k < 3; k++) dg[i] += kC[k][i] * d[k]; for (int j = 0; j < 3; j++) { Rg[i][j] = 0; for (int k = 0; k < 3; k++) Rg[i][j] += kC[k][i] * T[k][j]; } }
}

// 0.18.24 room-scale: move the tracking reference so the head offset loses the world displacement dw (game units, horizontal) - the view
// (anchor + offset) then stays put while the anchor (Jin) moved by dw. gm = the game camera (its level forward), viewHoldDeg = the
// VR-owned yaw; the same frame compose() maps the head offset with (dg . rows).
static bool rsShiftRef(const float *gm, float viewHoldDeg, const float dw[2]) {
    float fx = gm[0], fy = gm[1]; const float n = sqrtf(fx * fx + fy * fy); if (!(n > 1e-4f)) return false; fx /= n; fy /= n;
    const float hdg = viewHoldDeg / 57.2957795f, c = cosf(hdg), sn = sinf(hdg);
    const float r0[2] = {fx * c - fy * sn, fx * sn + fy * c}, r1[2] = {-r0[1], r0[0]};   // level forward / left, turned by the VR-owned view yaw
    const float g[3] = {(r0[0] * dw[0] + r0[1] * dw[1]) / g_cfg.unitsPerM, (r1[0] * dw[0] + r1[1] * dw[1]) / g_cfg.unitsPerM, 0.0f};
    float kg[3]; for (int i = 0; i < 3; i++) { kg[i] = 0; for (int k = 0; k < 3; k++) kg[i] += kC[i][k] * g[k]; }
    float Rr[3][3]; quatMat(g_ref.q, Rr); for (int i = 0; i < 3; i++) { float v = 0; for (int j = 0; j < 3; j++) v += Rr[i][j] * kg[j]; g_ref.p[i] += v; }
    return true;
}
// Build this frame's packet from the host's latest located views (game thread).
static void buildPacket(u32 f, FramePkt &p) {
    p.frame = f; p.valid = false; p.eye = -1; p.phase = -1; p.stereo = false; p.flat = false;
    if (g_cfg.xr == 4 && g_flatRender.load()) { p.flat = true; return; }   // flat context: the game renders its own flat frame (HUD-003)
    if (!S || (g_cfg.xr != 1 && g_cfg.xr != 2 && g_cfg.xr != 3 && g_cfg.xr != 4) || !hostFresh() || !S->imagesOk || S->gameRefuse) return;
    GvPose head{}, eye[2]{}; float fov[2][4]; u32 valid = 0;
    if (!gvRead(&S->poseSeq, [&] { head = S->head; eye[0] = S->eye[0]; eye[1] = S->eye[1]; memcpy(fov, S->fov, sizeof fov); valid = S->poseValid; }) || !valid) return;
    if (!g_refValid || m_recenter.exchange(0)) recentre(head);
    if (g_cfg.xr == 2) { int e = (int)(f & 1); p.eye = e; p.pose = eye[e]; memcpy(p.fov, fov[e], 16); }
    else if (g_cfg.xr == 4) {   // synchronized pair: both eyes from the head-pose sample of the pair's first (left) tick
        if (g_pairPhase == 0 || !g_pairSample.ok) {
            g_pairSample.ok = true; g_pairSample.e[0] = eye[0]; g_pairSample.e[1] = eye[1]; memcpy(g_pairSample.f, fov, sizeof fov);
            { float dx = eye[1].p[0] - eye[0].p[0], dy = eye[1].p[1] - eye[0].p[1], dz = eye[1].p[2] - eye[0].p[2], d = sqrtf(dx * dx + dy * dy + dz * dz); if (d > 0.03f && d < 0.09f) g_ipd = d; }
            // BUG-38 (0.11.0, playbook ch01 BuildCullFrustum: "add margin - it covers reprojection and the pose moving
            // between cull time and render time"): each eye is rendered (and therefore culled) a little wider than the
            // lens; the host declares the rendered frustum, so the compositor crops it and a very fast head turn shows
            // rendered world instead of an edge. Angles: left/down negative, right/up positive.
            const float m = g_fovMarginDeg / 57.2957795f;
            if (m > 0) for (int k = 0; k < 2; k++) { float *f = g_pairSample.f[k]; f[0] = fmaxf(f[0] - m, -1.45f); f[1] = fminf(f[1] + m, 1.45f); f[2] = fminf(f[2] + m, 1.45f); f[3] = fmaxf(f[3] - m, -1.45f); }
        }
        if (g_pairPhase == 0 && g_eyeSwapLatched != g_eyeSwap.load()) { g_eyeSwapLatched = g_eyeSwap.load(); g_flatCut = 2; }   // no history across the swap
        int e = g_eyeSwapLatched ? 1 - g_pairPhase : g_pairPhase; p.eye = e; p.phase = g_pairPhase; p.pose = g_pairSample.e[e]; memcpy(p.fov, g_pairSample.f[e], 16);
    }
    else if (g_cfg.xr == 3) { p.eye = 0; p.pose = eye[0]; memcpy(p.fov, fov[0], 16); p.stereo = true; p.pose2 = eye[1]; memcpy(p.fov2, fov[1], 16); }
    else {   // mono: head pose, frustum = union of both eyes
        p.eye = -1; p.pose = head;
        p.fov[0] = fminf(fov[0][0], fov[1][0]); p.fov[1] = fmaxf(fov[0][1], fov[1][1]);
        p.fov[2] = fmaxf(fov[0][2], fov[1][2]); p.fov[3] = fminf(fov[0][3], fov[1][3]);
    }
    p.valid = true;
}
static void currentPacket() {   // game thread: (re)build when the engine's frame id moves
    u32 f = frameCtr(); if (f == g_curFrame) return;
    g_curFrame = f; buildPacket(f, g_cur); if (g_cur.valid) c_pktValid++;
    FramePkt &r = g_ring[f & 31];
    gvWriteBegin(&r.seq); r.frame = g_cur.frame; r.eye = g_cur.eye; r.phase = g_cur.phase; r.valid = g_cur.valid; r.flat = g_cur.flat; r.pose = g_cur.pose; memcpy(r.fov, g_cur.fov, 16);
    r.stereo = g_cur.stereo; r.pose2 = g_cur.pose2; memcpy(r.fov2, g_cur.fov2, 16); gvWriteEnd(&r.seq);
}

// Idempotent cache: slot 43 runs ~5-6x per frame on the same matrix, often without the game rewriting it.
// We keep the game's own values and recompose from them every call (BUG-3: 0.4 accumulated the yaw).
// Keyed by camera object: if the render camera switches (cutscene, photo mode) the old camera gets its
// own values back, so a later switch back can never mistake GoTVR's matrix for the game's (review #7).
struct Cache { u64 cam; float *live; int n; float game[16]; float last[16]; bool valid; };
static Cache c_world{0, nullptr, 16}, c_proj{0, nullptr, 16}, c_tan{0, nullptr, 2};
static std::atomic<int> g_projBad{0};
static void cacheOff(Cache &c) {   // restore the game's values if what is there is still ours
    if (c.valid && isCam(c.cam) && memcmp(c.live, c.last, 4 * c.n) == 0) memcpy(c.live, c.game, 4 * c.n);
    c.valid = false;
}
static float *cacheIn(Cache &c, u64 cam, float *live) {   // the game's values (re-captured when the game wrote new ones)
    if (c.valid && c.cam != cam) cacheOff(c);
    if (!c.valid || (!g_lockGame && memcmp(live, c.last, 4 * c.n) != 0)) { memcpy(c.game, live, 4 * c.n); c.valid = true; }
    c.cam = cam; c.live = live;
    return c.game;
}
static void cacheOut(Cache &c, const float *v) { memcpy(c.live, v, 4 * c.n); memcpy(c.last, v, 4 * c.n); }
static void cacheOffFor(Cache &c, u64) { cacheOff(c); }


// ---------------- 0.17.0 first person ----------------
// RE (STEREO_ROUTE session 19): the hero is *(0x1434C6F20) (627 readers; cleared/set by 0x8F6120..); a scene-node object
// (class id = vt[4] "mov eax, imm; ret" in [10, 220)): world matrix at +0x150 (rows fwd, left, up, pos; camera driver 0xA33720
// reads node+0x150 for those classes), joint WORLD matrices at *(+0x278) + (joint+1)*0x40 (entry 0 = the node itself;
// Node_JointWorld 0x93AE40), and two per-character joint indices +0x7F8 / +0x7FC that the UI uses as the bottom / TOP of the
// character's screen bounds (0x6E9433 / 0x6EFB90 -> the head end). Method (playbook ch01 + bldbrne-vr / BetterVR): the eye is
// anchored on the hero's ROOT, not the animated head bone - horizontal = root + a forward offset along the hero's facing,
// vertical = root + the head joint's height above it, low-passed (stairs, crouch) so walk/attack animations do not bob the
// view; the HMD pose is added on top as before. The game's camera keeps owning yaw (stick/mouse) and everything else.
// Head hiding v1 (FRIK's bHidePlayerHead family, as bone collapse): the joints within fp_hide_radius of the head joint in the
// first observed pose are collapsed onto the head joint (rotation rows x0.001) right before each render-camera update and the
// frame snapshot; a joint that later strays > 35 cm from the head is not part of the head (a hand) and is dropped for good.
// Cost: no head in the shadow (bldbrne-vr's colour-pass draw filter is the follow-up that keeps it).
namespace ui { static int fpBlockReason(); }
static float len3_st(const float *v) { return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); }   // 0 = allowed; else why the game camera keeps the view (flat / cutscene / title)
namespace fp {
static std::atomic<int> on{1}, dumpReq{0};
static std::atomic<u64> nearHits[2], nearMiss{0};   // 0.18.11: near clip plane applied per eye (left/simulated, right/frozen) / projection not recognised
static u8 *hero = nullptr, *node = nullptr; static float *joints = nullptr; static int nJ = 0, headIdx = -1, botIdx = -1; static bool valid = false;
static int nodeOff = -1, nodeOff2 = -1, nodeClass = -1; static u64 retryAt = 0;
// 0.17.3 (BUG-68: "the head is still there"): the joint WORLD matrices at +0x278 are not what the renderer skins with. RE
// (re/decomp_anim): Anim_Setup 0x9BB4F0 queues the character's animation job FUN_140B2DBC0(job) with job+0xE8 = the node,
// job+0xA8 = the node's CURRENT render-pose buffer (node+0x508 + cur*0x18, cur = node+0x2DA toggled per update; joint i at
// +i*0x40, MODEL space), job+0xD8 = the previous one (velocity), job+0x98 = the world array (+0x278 + 0x40). The job writes
// the model-space poses and world = pose * node world; the skinning palette job (0x59EB30, waits for the job's ready flag)
// builds the GPU palette from node+0x508[cur] * inverse bind - the world array never reaches the GPU. 0.17.2 also capped the
// head set at 96 joints (487 of the 500 counted joints are face/head joints) and the joint count at 500 (node+0x280 - 1 is
// the real count). Now: the head set (every shown joint within fp_hide_radius of the top-of-head joint in the first pose) is
// collapsed in the CURRENT render-pose buffer right after the hero's animation job, before the palette job can run: rows
// zeroed (the engine's own way of hiding a gear joint) and moved onto the neck joint below the head. The world array (game
// logic: aim, AI, attachments, our eye anchor) is left alone. No head in shadows (bldbrne-vr's colour-pass filter keeps it).
enum { HIDE_CAP = 2048, JOINT_CAP = 4096 };
struct HidePub { u8 *node; int target, head, n; uint16_t idx[HIDE_CAP]; };
static HidePub hpub[2]; static std::atomic<int> hpubIdx{-1}; static std::atomic<u32> hideFrame{0}; static std::atomic<u8> strayFlag[JOINT_CAP];
static int hideSet[HIDE_CAP]; static int nHide = 0, neckIdx = -1; static bool calibrated = false; static u8 strayed[JOINT_CAP];
static u32 lastFrame = 0xFFFFFFFFu; static bool activeNow = false; static float anchor[3] = {0, 0, 0}, relUpF = 0, relHF[3] = {0, 0, 0}; static bool relInit = false; static u64 lastT = 0;
static std::atomic<int> lastReason{-1};
static std::atomic<u64> nActive{0}, nBlockedUi{0}, nNoHero{0}, nFar{0}, nOff{0}, nCollapse{0}, nSeenCollapsed{0}, nSeenRestored{0}, nDropped{0}, nRevalidate{0};
static std::atomic<u64> nHeroJobs{0}, nJobsHidden{0}, nJobsNoBuf{0}, nJointsHidden{0};
static std::atomic<float> camDist{0}, headUp{0}; static std::atomic<u64> nJointSwaps{0};
static int countSrc = 0;   // 1 = node+0x280, 2 = scan
static const float *J(int i) { return joints + (size_t)(i + 1) * 16; }
static u8 *heroNow() { return *(u8 **)(g_base + RVA_HERO); }
static u8 *(*heroPtr)() = heroNow;   // self-test: a stand-in hero
static int classId(u8 *o) {   // vt[4] = "mov eax, imm32; ret" (decoded, never called)
    u8 *vt = *(u8 **)o; u8 *f = vt && readable(vt + 0x20, 8) ? *(u8 **)(vt + 0x20) : nullptr;
#ifndef GOTVR_SELFTEST
    if (vt < g_base || vt > g_base + 0x4000000 || f < g_base || f > g_base + 0x1400000) return -1;
#endif
    if (!f || !readable(f, 6)) return -1;
    return (f[0] == 0xB8 && f[5] == 0xC3) ? *(int *)(f + 1) : -1;
}
static float len3(const float *v) { return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); }
// BUG-65b (0.17.1): hidden pieces (gear slots it does not show) are ZERO-scale joints: finite, rows shorter than 50 (zero
// allowed), within 10 m of the root are joints.
static bool saneLoose(const float *m, const float *root) {
    for (int i = 0; i < 16; i++) if (!std::isfinite(m[i])) return false;
    for (int r = 0; r < 3; r++) if (!(len3(m + 4 * r) < 50.0f)) return false;
    const float d[3] = {m[12] - root[12], m[13] - root[13], m[14] - root[14]}; return len3(d) < 1000.0f;
}
static bool shown(const float *m) { return len3(m) > 0.01f && len3(m + 4) > 0.01f && len3(m + 8) > 0.01f; }   // not a hidden (zero-scale) joint
static int countJoints(u8 *n, float *j) {
    // the node's own count (palette job 0x59EC42: count = node+0x280 - 1) when it is plausible and readable
    const int c280 = *(int *)(n + 0x280) - 1;
    if (c280 >= 8 && c280 <= JOINT_CAP && readable(j, (size_t)(c280 + 1) * 64)) { countSrc = 1; return c280; }
    const float *root = (const float *)(n + 0x150); int last = -1, gap = 0;
    for (int c = 0; c < JOINT_CAP; c++) {
        const float *m = j + (size_t)(c + 1) * 16; if (!readable(m, 0x40)) break;
        if (saneLoose(m, root)) { last = c; gap = 0; } else if (++gap > 8) break;
    }
    countSrc = 2; return last + 1;
}
static bool exePtr(const void *p) { return (const u8 *)p >= g_base && (const u8 *)p < g_base + 0x4000000; }
// BUG-75 (0.18.4: started on the horse -> "a bit above Jin's head", the head visible again, the view on the floor after getting
// off): the head was "the highest shown joint near the body axis". On the horse two gear joints (644 / 651: something carried
// only while riding) sat 40 cm above the head, alone - no other joint within 22 cm - and won; on foot joint 644 drops to the
// root, so the eyes went to the floor, and the head set was that 1 joint instead of the ~480 face / head joints. The head is now
// the highest near-axis joint with a DENSE neighbourhood: at least half the densest near-axis joint's count (capped at 40)
// shown joints within fp_hide_radius. Measured on both receipts (horse + standing): picks the head (22 / 123), never 644.
static int pickHead(const float *jarr, int c, const float *r, float &topUp, int *denseOut = nullptr) {
    static float P[JOINT_CAP][3]; static int cand[JOINT_CAP], cnt[JOINT_CAP]; static float cu[JOINT_CAP];
    topUp = -1e9f; if (denseOut) *denseOut = 0;
    float U[3] = {r[8], r[9], r[10]}; { const float l = len3(U); if (!(l > 1e-4f)) return -1; for (auto &x : U) x /= l; }
    const float s = g_cfg.unitsPerM * 0.01f, rad = g_fp.hideRadiusCm * s, rad2 = rad * rad;
    int nS = 0, nC = 0; static int sIdx[JOINT_CAP];
    for (int i = 0; i < c && i < JOINT_CAP; i++) { const float *m = jarr + (size_t)(i + 1) * 16; if (!saneLoose(m, r) || !shown(m)) continue;
        P[nS][0] = m[12]; P[nS][1] = m[13]; P[nS][2] = m[14]; sIdx[nS] = i;
        const float e[3] = {m[12] - r[12], m[13] - r[13], m[14] - r[14]}; const float u = e[0] * U[0] + e[1] * U[1] + e[2] * U[2];
        const float hz[3] = {e[0] - u * U[0], e[1] - u * U[1], e[2] - u * U[2]};
        if (len3(hz) < 40.0f * s) { cand[nC] = nS; cu[nC] = u; nC++; }
        nS++; }
    int maxCnt = 0;
    for (int k = 0; k < nC; k++) { const float *a = P[cand[k]]; int n = 0;
        for (int t = 0; t < nS; t++) { const float d0 = P[t][0] - a[0], d1 = P[t][1] - a[1], d2 = P[t][2] - a[2]; if (d0 * d0 + d1 * d1 + d2 * d2 <= rad2) n++; }
        cnt[k] = n; if (n > maxCnt) maxCnt = n; }
    int need = maxCnt / 2; if (need > 40) need = 40; if (need < 3) need = 3;
    int best = -1, dense = 0;
    for (int k = 0; k < nC; k++) if (cnt[k] >= need && cu[k] > topUp) { topUp = cu[k]; best = sIdx[cand[k]]; dense = cnt[k]; }
    if (denseOut) *denseOut = dense;
    return best;
}
// BUG-65 (0.17.0): the skinned model is searched on the hero and one level down (in practice: the hero itself, class 15).
static u8 *findModel(u8 *h, int &off, int &off2, int &cnt) {
    const float *hr = (const float *)(h + 0x150); u8 *best = nullptr; cnt = 0; off = off2 = -1; int probes = 0;
    auto consider = [&](u8 *p, int o1, int o2) {
        if (((u64)p & 7) || (u64)p < 0x10000 || (u64)p > 0x7FFFFFFFFFFFull || exePtr(p) || !readable(p, 0x800)) return false;
        probes++;
#ifndef GOTVR_SELFTEST
        if (!exePtr(*(u8 **)p)) return false;   // a game object: its vtable is in the exe
#endif
        const int cid = classId(p); if (cid < 10 || cid >= 220) return true;   // an object, not a scene node (searched one level down)
        float *j = *(float **)(p + 0x278); if (!j || ((u64)j & 3) || !readable(j, 0x80)) return true;
        const float *r = (const float *)(p + 0x150); const float d[3] = {r[12] - hr[12], r[13] - hr[13], r[14] - hr[14]};
        if (len3(d) > 60.0f) return true;   // the hero's own model shares its root (a horse or a held weapon does not)
        const int c = countJoints(p, j); if (c <= cnt) return true;
        {   // a human: the highest joint near the body axis 0.7..2.3 m above the root (standing or crouched)
            float top = -1e9f; pickHead(j, c, r, top);   // BUG-75: the head, not lone gear above it
            if (top < 70.0f || top > 230.0f) {   /* 0.18.26: 100 -> 70 (log: loading in crouched, 99 cm, kept third person for 6 s) */ static int told = 0; if (told++ < 8) logf("first person: node %p (%d joints) at hero+0x%X skipped: highest joint %.0f cm", (void *)p, c, o1 < 0 ? 0 : o1, top); return true; }
        }
        cnt = c; best = p; off = o1; off2 = o2;
        return true;
    };
    consider(h, -1, -1);
    if (cnt < 20) {   // not the hero itself: its objects, and one level down
        u8 *lvl1[64]; int o1s[64], n1 = 0;
        for (int o = 8; o < 0x4100; o += 8) { u8 *p = *(u8 **)(h + o); if (p == h) continue; if (consider(p, o, -1) && n1 < 64 && classId(p) < 10) { lvl1[n1] = p; o1s[n1++] = o; } }
        for (int i = 0; i < n1; i++) for (int o = 8; o < 0x400; o += 8) { u8 *p = *(u8 **)(lvl1[i] + o); if (p != h && p != lvl1[i]) consider(p, o1s[i], o); }
        { static int once = 0; if (once++ < 3) logf("first person: model search probed %d objects (hero +0x0..+0x4100, then %d objects one level down)", probes, n1); }
    }
    return cnt >= 20 ? best : nullptr;
}
// 0.18.5 body follow (owner: "when moving backwards, because there is no backpedalling animation, you see Jin's body. Could you have
// Jin's animation play forwards"). GoT has no backpedal: the stick pulled back turns Jin round to run at the camera, so in first person
// his chest is in front of you. Render-only fix in the same write point as the head hiding (Anim_Job, before the skin palette job):
// when Jin's facing is more than fp_body_follow_start_deg away from YOUR forward direction (the game camera's yaw + the VR turn - not
// the head's yaw, playbook ch12: looking aside must not swing the body), the whole render pose is turned about the vertical axis
// through the head, fully at fp_body_follow_full_deg, rate-limited - so his run animation plays facing forward while he moves back.
// Gameplay (world array: aim, AI, collisions, attachments) is untouched. Off while riding (lone gear joints in the skeleton's tail
// sit ~2 m up on the horse and at the root on foot: 644 / 651 in both skeleton dumps) and above fp_body_follow_max_speed.
static float refF[2] = {0, 0}; static bool refOk = false; static float headF[2] = {0, 0}; static bool headOk = false, headTurning = false; static std::atomic<int> ridingNow{0};   // game thread (hSlot43): your forward direction (world, horizontal)
static float bodyYawCur = 0; static std::atomic<float> bodyYaw{0}; static u64 bodyLastMs = 0; static float lastRoot[3] = {0, 0, 0}; static bool lastRootOk = false;
static int lone[16]; static int nLone = 0;
static std::atomic<u64> nBodyFrames{0}, nBodyRiding{0}, nBodyFast{0}, nBodyRefused{0}, nBodyJobs{0};
static std::atomic<float> bodyDeg{0}, facingDeg{0}, speedMs{0};
static float jobApplied = 0, jobPivot[2] = {0, 0};   // job thread: what the last render pose got (a job that copies the previous pose)
static void findLone() {   // validation: tail joints with no other shown joint within 15 cm (gear helpers: riding markers)
    nLone = 0; const float s = g_cfg.unitsPerM * 0.01f, r2 = (15.0f * s) * (15.0f * s); const float *root = (const float *)(node + 0x150);
    for (int i = nJ - nJ / 10; i < nJ && nLone < 16; i++) { if (i < 0) continue; const float *m = J(i); if (!saneLoose(m, root) || !shown(m)) continue; bool alone = true;
        for (int k = 0; k < nJ && alone; k++) { if (k == i) continue; const float *q = J(k); if (!shown(q)) continue; const float d0 = q[12] - m[12], d1 = q[13] - m[13], d2 = q[14] - m[14]; if (d0 * d0 + d1 * d1 + d2 * d2 <= r2) alone = false; }
        if (alone) lone[nLone++] = i; }
}
// 0.18.19 (owner: "at the beginning of the jump animation the VR IK arms stop working"; log: "held off: riding 27" on foot): ONE lone
// tail joint high was enough, and a jump lifts gear carried on the body (joint 600) past 150 cm above the root - the arms were
// switched off and the body snapped to the node's facing as if mounted. On the horse BOTH reins/saddle markers (644 / 651, ~202 cm)
// are up. Now at least two markers must be high (or the only one there is), for 0.25 s on and 0.25 s off.
static std::atomic<u64> nTClamped{0};
static std::atomic<float> underW[2]; static std::atomic<float> underCm{0}; static bool underMoved = false;
static std::atomic<int> underHold{0}; static std::atomic<u64> nUnderHeld{0}; static std::atomic<float> underFwdCm{0}, underLeftCm{0}, underJumpCm{0};   // 0.18.31
static float hzS[2] = {0, 0}; static std::atomic<float> hzOut[2], hzT[2]; static std::atomic<int> hzOk{0};   // 0.18.25 head offset in Jin's own axes (model units), the drawn-body move
static std::atomic<float> vertMs{0}; static std::atomic<u64> airAt{0}, nJumpHeld{0};   // 0.18.23
static std::atomic<float> rsCons[2], rsApplied{0}; static std::atomic<u64> nRsBlocked{0};   // 0.18.24 room-scale: distance Jin walked for you (world units, running total)
static std::atomic<u64> stabResets[8], stabFiltered{0}; static int stabLost[2] = {0, 0}; static std::atomic<float> stabSpeed[2];   // 0.18.24 hand stabilisation
static std::atomic<int> rideMarkersHigh{0}; static std::atomic<u64> nStopCaps{0}, nDeadTurns{0}, nSnapTurns{0}, nStopTurns{0}; static std::atomic<float> moveOffDeg{0}; static std::atomic<float> headPitchDeg{0}; static std::atomic<int> lookDownNow{0};   // 0.18.26
static bool ridingRaw(const float *root, const float *U) {
    const float s = g_cfg.unitsPerM * 0.01f; int high = 0;
    for (int k = 0; k < nLone; k++) { const float *m = J(lone[k]); if (!shown(m)) continue; const float d[3] = {m[12] - root[12], m[13] - root[13], m[14] - root[14]};
        if (d[0] * U[0] + d[1] * U[1] + d[2] * U[2] > 150.0f * s) high++; }
    rideMarkersHigh = high;
    return high >= (nLone >= 2 ? 2 : 1) && high > 0;
}
static bool rideState = false; static float rideFor = 0.0f;
static bool riding(const float *root, const float *U, float dt) {
    const bool raw = ridingRaw(root, U);
    {   // 0.18.23: on foot, a jump / its fall / its landing never STARTS riding (gear flies up past the markers' height)
        const u64 tj = GetTickCount64(), sinceJ = tj - vrin::jumpAt.load(), sinceAir = tj - airAt.load();
        if (raw && !rideState && (sinceJ < 1200 || (sinceJ < 4000 && sinceAir < 600))) { rideFor = 0.0f; nJumpHeld++; return rideState; } }
    if (raw != rideState) { rideFor += dt; if (rideFor >= 0.25f || dt <= 0.0f) { rideState = raw; rideFor = 0.0f; } } else rideFor = 0.0f;
    return rideState;
}
static bool stopWasMoving = false; static float stopStill = 0.0f;   // 0.18.28 the stop detector (reset with the root history)
static void bodyStep(const float *root, const float *U, const float *F, bool wasActive) {   // game thread, once per simulated tick
    const float PI = 3.14159265f, D = 57.2957795f;
    if (!lastRootOk || !wasActive) { stopWasMoving = false; stopStill = 0.0f; }
    const u64 now = GetTickCount64(); const float dt = bodyLastMs ? fminf((float)(now - bodyLastMs) * 0.001f, 0.1f) : 0.0f; bodyLastMs = now;
    float sp = 0; if (lastRootOk && dt > 0.0f) { float d[3] = {root[12] - lastRoot[0], root[13] - lastRoot[1], root[14] - lastRoot[2]}; const float u = d[0] * U[0] + d[1] * U[1] + d[2] * U[2]; for (int c = 0; c < 3; c++) d[c] -= u * U[c]; sp = len3(d) / dt / g_cfg.unitsPerM; }
    {   // 0.18.23 (owner: "at the end of a jump, the VR IK arms go back to animation-controlled"): the vertical speed of the root; the
        // landing comes > 1.2 s after the press on longer falls, so the jump window now lasts while Jin is in the air (+ 0.6 s)
        float vz = 0; if (lastRootOk && dt > 0.0f) vz = ((root[12] - lastRoot[0]) * U[0] + (root[13] - lastRoot[1]) * U[1] + (root[14] - lastRoot[2]) * U[2]) / dt / g_cfg.unitsPerM;
        vertMs = vz; if (fabsf(vz) > 0.8f) airAt = GetTickCount64();
    }
    {   // 0.18.24 room-scale: while the drive walks Jin toward your head, the part of his step along that direction (never more than the
        // offset left) is "yours" - gotvr's camera code takes it out of the head offset. A drive that gets him nowhere (a wall) pauses.
        static float driveFor = 0.0f, gotFor = 0.0f;
        if (GetTickCount64() < vrin::rsDriveUntil.load() && lastRootOk && dt > 0.0f) {
            const float dx = root[12] - lastRoot[0], dy = root[13] - lastRoot[1]; const float ex = vrin::rsDir[0].load(), ey = vrin::rsDir[1].load();
            const float left = sqrtf(underW[0].load() * underW[0].load() + underW[1].load() * underW[1].load());
            float a = dx * ex + dy * ey; a = a < 0.0f ? 0.0f : a > left ? left : a;
            if (std::isfinite(a) && a > 0.0f) { rsCons[0] = rsCons[0].load() + a * ex; rsCons[1] = rsCons[1].load() + a * ey; }
            driveFor += dt; gotFor += a / (g_cfg.unitsPerM * 0.01f);
            if (driveFor > 0.6f) { if (gotFor < 5.0f) { vrin::rsBlockUntil = GetTickCount64() + 1500; nRsBlocked++; } driveFor = 0.0f; gotFor = 0.0f; }
        } else { driveFor = 0.0f; gotFor = 0.0f; }
    }
    {   // 0.18.64 movement-direction guard (see vrin::moveFix): Jin running >= 2.5 m/s with the stick pushed (>= 0.8), not riding / lunging /
        // walked by room-scale - his actual run direction vs where the stick asks for; 60+ deg apart and steady for 0.5 s -> the stick is turned
        static float errAvg = 0.0f; static float badFor = 0.0f; static u64 coolUntil = 0; const u64 tn = vrin::nowMs();
        const bool cand = lastRootOk && dt > 0.0f && sp >= 2.5f && vrin::stickMag.load() >= 0.8f && tn - vrin::stickAt.load() < 150 && !ridingNow.load()
                          && tn >= vrin::lungeUntil.load() && vrin::rsMag.load() == 0.0f && vrin::cfg.moveHead && tn >= coolUntil;
        if (cand) { const float dx = root[12] - lastRoot[0], dy = root[13] - lastRoot[1];
            if (dx * dx + dy * dy > 1e-6f) { const float tr = atan2f(dy, dx) * 57.2957795f; float e = tr - vrin::stickWorldYaw.load(); while (e > 180) e -= 360; while (e < -180) e += 360;
                float de = e - errAvg; while (de > 180) de -= 360; while (de < -180) de += 360;
                errAvg += de * fminf(1.0f, dt / 0.15f); while (errAvg > 180) errAvg -= 360; while (errAvg < -180) errAvg += 360;
                if (fabsf(errAvg) > 60.0f && fabsf(de) < 25.0f) badFor += dt; else badFor = 0.0f;
                if (badFor > 0.5f) { float nf = vrin::moveFix.load() - errAvg; while (nf > 180) nf -= 360; while (nf < -180) nf += 360; vrin::moveFix = nf; vrin::nMoveFixes++;
                    logf("controls: Jin ran %.0f deg off where the stick pointed for 0.5 s (%.1f m/s) -> the stick turned %.0f deg (total %.0f) | head %.0f, game camera %.0f, view offset %.1f, stick %.0f, run %.0f deg - cleared at the next turn",
                         errAvg, sp, -errAvg, nf, vrin::headYawNow.load(), vrin::gameYaw.load(), vrin::viewHold.load(), vrin::stickWorldYaw.load(), tr);
                    errAvg = 0.0f; badFor = 0.0f; coolUntil = tn + 1500; } } }
        else badFor = 0.0f;
    }
    lastRoot[0] = root[12]; lastRoot[1] = root[13]; lastRoot[2] = root[14]; lastRootOk = true; speedMs = sp;
    if (!wasActive) bodyYawCur = 0;
    float target = 0; bool expo = false;
    const bool ride = riding(root, U, dt); ridingNow = ride ? 1 : 0;
    // 0.18.21 (owner: "the forward vector is being updated every frame I move - I only want it once I start moving"): the 0.18.19
    // capture at stops is gone, and so is 0.18.20's rewrite of the forward while the body turns past the threshold during a run. The
    // movement forward is now taken ONLY when you start moving from rest (vrinput.h), at a snap / smooth turn, at the start, after a
    // menu and at a recenter.
    // 0.18.16 (owner: "the body turns too much at the slightest turn either direction - keep the 35 deg threshold (editable) but even in
    // both directions; all movement animations in any direction should face the headset's forward, otherwise looking forward and moving
    // sideways shows the inside of Jin's shoulder"): the drawn body's heading is now kept in WORLD yaw (Hw) instead of relative to Jin's
    // node, which the game itself turns (to the run direction, toward the camera after a snap catch-up, turn-in-place): with a node-
    // relative heading every node turn dragged the drawn body along, and the dead zone sat wherever the node happened to point.
    //   standing (< 0.6 m/s): Hw keeps still while you look around; once your head is more than fp_body_head_deadzone_deg (35) away -
    //            either side, measured from where the hips are drawn - it turns to your head (tau fp_body_head_follow_s) until within 2 deg;
    //            a snap / smooth turn starts that turn at once (0.18.15)
    //   moving:  Hw follows your head's forward continuously (fp_body_move_follow_s 0.15) - running, strafing, backing up: the body
    //            faces where you look, the legs run in that frame
    // The pose is turned by Hw - node yaw (the hip squaring of 0.18.15 keeps the drawn HIPS on Hw while standing).
    static float Hw = 0.0f; static bool HwOk = false;
    const float nodeYaw = atan2f(F[1], F[0]);
    auto wrapPi = [&](float a) { if (!std::isfinite(a)) return 0.0f; if (fabsf(a) > 64.0f) a = fmodf(a, 2 * PI); while (a > PI) a -= 2 * PI; while (a < -PI) a += 2 * PI; return a; };   // 0.18.21: bounded
    if (!wasActive || !HwOk) { Hw = wrapPi(nodeYaw + bodyYawCur); HwOk = true; headTurning = false; }
    if (g_fp.bodyFollow && refOk) {
        const float L[3] = {U[1] * F[2] - U[2] * F[1], U[2] * F[0] - U[0] * F[2], U[0] * F[1] - U[1] * F[0]};   // U x F = left
        float rv[3] = {refF[0], refF[1], 0}; { const float u = rv[0] * U[0] + rv[1] * U[1]; for (int c = 0; c < 3; c++) rv[c] -= u * U[c]; }
        const float a = atan2f(rv[0] * L[0] + rv[1] * L[1] + rv[2] * L[2], rv[0] * F[0] + rv[1] * F[1] + rv[2] * F[2]); facingDeg = a * D;
        const float headYaw = headOk ? atan2f(headF[1], headF[0]) : wrapPi(nodeYaw + a);
        if (ride) { nBodyRiding++; headTurning = false; Hw = nodeYaw; }
        else if (sp > g_fp.bodyMaxSpeed) { nBodyFast++; headTurning = false; Hw = nodeYaw; }
        // 0.18.22 (owner: "Jin always rotates to match the headset after a small threshold (5-10 deg); movement totally headset oriented"):
        // with move_follows_head the body uses the same head follow standing AND moving (dead zone fp_body_head_deadzone_deg 7.5 around
        // the drawn body, then it turns to your head until within 2 deg); the captured-forward branch below is for move_follows_head=0
        else if ((sp < 0.6f || vrin::cfg.moveHead) && g_fp.bodyHead) {
            const float e = wrapPi(headYaw - Hw);
            { static u64 seenTurn = 0; const u64 tn = vrin::turnSeq.load(); if (tn != seenTurn) { seenTurn = tn; if (fabsf(e) * D > 2.0f && !headTurning) { headTurning = true; nSnapTurns++; } } }
            // 0.18.26 (owner: "if the player is looking down at the body, increase the follow threshold to 30"): below -35 deg pitch the
            // threshold is fp_body_head_deadzone_down_deg (30), back above -25 deg it is the normal one again
            static bool lookDown = false; { const float pt = headPitchDeg.load(); if (pt < -35.0f) lookDown = true; else if (pt > -25.0f) lookDown = false; }
            const float dead = lookDown ? fmaxf(g_fp.headDeadDeg, g_fp.headDeadDownDeg) : g_fp.headDeadDeg; lookDownNow = lookDown ? 1 : 0;
            if (!headTurning && fabsf(e) * D > dead) { headTurning = true; nDeadTurns++; }
            {   // 0.18.28 (owner: "set Jin's body rotation to the headset rotation whenever the player stops moving"): after moving (> 0.6 m/s),
                // once you are below 0.3 m/s for 0.2 s the body turns square to your headset (the same eased turn), whatever the dead zone
                bool &wasMoving = stopWasMoving; float &stillT = stopStill;
                if (sp > 0.6f) { wasMoving = true; stillT = 0.0f; }
                else if (wasMoving && sp < 0.3f) { stillT += dt; if (stillT >= 0.2f) { wasMoving = false; stillT = 0.0f; if (g_fp.bodySquareOnStop && !headTurning && fabsf(e) * D > 0.5f) { headTurning = true; nStopTurns++; } } }
                else if (wasMoving) stillT = 0.0f;
            }
            if (headTurning && fabsf(e) * D < 0.5f) headTurning = false;   // 0.18.22: 2 -> 0.5 deg, the body ends square to the view
            // 0.18.25 (owner: "when I first pass the threshold, have Jin's body rotate a bit slower - tween - until it catches up; now it snaps"):
            // the turn is a critically damped spring (eases in and out, no overshoot), ~90 % there after fp_body_turn_s (0.35 s)
            static float hv = 0.0f;
            if (headTurning && g_fp.bodyTurnS <= 0.0f) { Hw = headYaw; hv = 0.0f; }   // 0 = at once
            else if (headTurning && dt > 0.0f) { const float om = 3.9f / fmaxf(0.05f, g_fp.bodyTurnS), x = -e, tmp = (hv + om * x) * dt, ex = expf(-om * dt);
                const float x1 = (x + tmp) * ex; hv = (hv - om * tmp) * ex; Hw = wrapPi(headYaw + x1); if (!std::isfinite(Hw) || !std::isfinite(hv)) { Hw = headYaw; hv = 0.0f; } }
            else if (headTurning) {} else hv = 0.0f;
        }
        else { headTurning = false;   // 0.18.17: moving, the body faces the MOVEMENT forward (your headset's forward at the last snap / start / menu exit)
            // 0.18.20 (owner: "when moving, Jin's body and sword must still rotate when the player exceeds the look threshold, like standing
            // still"): beyond fp_body_head_deadzone_deg from the body the body turns to your head - and the movement forward turns with it
            // 0.18.21: the body faces movement forward + an offset; the offset (not the forward) takes the turn past the threshold and
            // stays until the forward is taken again (the next start from rest / snap / menu / recenter), which squares the body to it.
            static bool moveTurning = false, mvSeen = false; static float off = 0.0f, lastMv = 0.0f;
            const float mv = vrin::moveYawOk.load() ? vrin::moveYaw.load() / D : headYaw;
            if (!mvSeen || fabsf(wrapPi(mv - lastMv)) > 1e-4f) { mvSeen = true; lastMv = mv; off = 0.0f; moveTurning = false; }   // a new forward
            const float eh = wrapPi(headYaw - Hw);
            if (!moveTurning && fabsf(eh) * D > g_fp.headDeadDeg) { moveTurning = true; nDeadTurns++; }
            if (moveTurning) { const float eo = wrapPi(headYaw - (mv + off)); off = wrapPi(off + eo * (g_fp.headTau > 0.0f ? 1.0f - expf(-dt / g_fp.headTau) : 1.0f)); if (fabsf(eo) * D < 2.0f) moveTurning = false; }
            moveOffDeg = off * D;
            const float e = wrapPi(mv + off - Hw);
            Hw = wrapPi(Hw + e * (g_fp.moveTau > 0.0f ? 1.0f - expf(-dt / g_fp.moveTau) : 1.0f));
        }
        target = wrapPi(Hw - nodeYaw);
        while (target - bodyYawCur > PI) target -= 2 * PI; while (target - bodyYawCur < -PI) target += 2 * PI;   // the short way
    } else Hw = wrapPi(nodeYaw + bodyYawCur);
    const float step = g_fp.bodyTurnSpeed > 0.0f ? g_fp.bodyTurnSpeed / D * dt : 1e9f; float d = target - bodyYawCur;
    if (expo && g_fp.headTau > 0.0f) d *= 1.0f - expf(-dt / g_fp.headTau);
    bodyYawCur = fabsf(d) <= step ? bodyYawCur + d : bodyYawCur + (d > 0 ? step : -step);
    if (bodyYawCur > PI) bodyYawCur -= 2 * PI; else if (bodyYawCur < -PI) bodyYawCur += 2 * PI;
    if (fabsf(bodyYawCur) > 1e-4f) nBodyFrames++;
    bodyYaw.store(bodyYawCur, std::memory_order_relaxed); bodyDeg = bodyYawCur * D;
}
// 0.18.6 (owner: "the backwards running is good, but we need the sword to also go backwards"): attached items (katana, scabbard,
// bow...) are not in the skin - RE 0x57E0A0 (render gather of attachments): instance = attachment local x the hero's joint WORLD
// matrix (node+0x278, entry joint+1). So the world array is turned too, right after the hero's animation job (same angle, about
// the vertical through the head joint); the next job rewrites the whole array, so nothing accumulates, and the head joint's
// position (the eye anchor) does not move. Game code that reads Jin's joints this tick sees the turned body (the VR body).
static std::atomic<u64> nWorldTurned{0};
static void turnWorld(u8 *nd, float a, int head) {
    float *W = *(float **)(nd + 0x278); const float *R = (const float *)(nd + 0x150); const int n = *(int *)(nd + 0x280) - 1;
    if (!W || n < 1 || n > JOINT_CAP || head < 0 || head >= n) return;
    float U[3] = {R[8], R[9], R[10]}; { const float l = len3(U); if (!(l > 1e-4f)) return; for (auto &x : U) x /= l; }
    const float c = cosf(a), s = sinf(a), *Hp = W + (size_t)(head + 1) * 16 + 12; const float P[3] = {Hp[0], Hp[1], Hp[2]};
    auto rot = [&](float *v) { const float d = U[0] * v[0] + U[1] * v[1] + U[2] * v[2]; const float x[3] = {U[1] * v[2] - U[2] * v[1], U[2] * v[0] - U[0] * v[2], U[0] * v[1] - U[1] * v[0]};
        for (int k = 0; k < 3; k++) v[k] = v[k] * c + x[k] * s + U[k] * d * (1.0f - c); };   // Rodrigues about U (right-handed: fwd -> left)
    for (int j = 0; j <= n; j++) { float *m = W + (size_t)j * 16;   // entry 0 = the node's own copy (attachments with joint -1)
        rot(m); rot(m + 4); rot(m + 8); float d[3] = {m[12] - P[0], m[13] - P[1], m[14] - P[2]}; rot(d); m[12] = P[0] + d[0]; m[13] = P[1] + d[1]; m[14] = P[2] + d[2]; }
    nWorldTurned++;
}
static void turnPose(float *pose, int cnt, float a, float px, float py) {   // the whole model-space pose about the vertical through (px, py)
    const float c = cosf(a), s = sinf(a);
    for (int j = 0; j < cnt; j++) { float *m = pose + (size_t)j * 16;
        for (int r = 0; r < 3; r++) { const float x = m[4 * r], y = m[4 * r + 1]; m[4 * r] = c * x - s * y; m[4 * r + 1] = s * x + c * y; }
        const float x = m[12] - px, y = m[13] - py; m[12] = px + c * x - s * y; m[13] = py + s * x + c * y; }
}
static void unpublish() { hpubIdx = -1; }
static void publish() {   // game thread: hand the head set to the animation-job hook (seqlocked by the slot swap)
    const int k = hpubIdx.load() == 0 ? 1 : 0; HidePub &p = hpub[k];
    p.node = node; p.target = neckIdx; p.head = headIdx; p.n = 0;
    for (int i = 0; i < nHide && p.n < HIDE_CAP; i++) if (!strayed[hideSet[i]]) p.idx[p.n++] = (uint16_t)hideSet[i];
    hpubIdx = k;
}
static bool validate(u8 *h) {   // on hero / joint-array change only (VirtualQuery is not for the hot path)
    unpublish();
    nRevalidate++; valid = false; nJ = 0; headIdx = botIdx = neckIdx = -1; calibrated = false; nHide = 0; relInit = false; memset(strayed, 0, sizeof strayed);
    for (auto &f : strayFlag) f.store(0, std::memory_order_relaxed);
    hero = h; node = nullptr; joints = nullptr;
    if (!h || !readable(h, 0x4100)) return false;
    const int cid = classId(h); if (cid < 10 || cid >= 220) { logf("first person: hero %p class id %d is not a scene node -> third person", (void *)h, cid); return false; }
    int cnt = 0; u8 *n = findModel(h, nodeOff, nodeOff2, cnt);
    if (!n) {
        logf("first person: no skinned model with a skeleton found near the hero %p (best %d joints) -> third person, retrying", (void *)h, cnt);
        return false;
    }
    node = n; nodeClass = classId(n); joints = *(float **)(n + 0x278); nJ = cnt;
    const float *root = (const float *)(n + 0x150);
    // the head: the highest joint near the body axis (a raised hand or weapon is off to the side) - the top of the head
    int hidden = 0; for (int i = 0; i < nJ; i++) { const float *m = J(i); if (!saneLoose(m, root) || !shown(m)) hidden++; }
    float topUp = -1e9f; int dense = 0; const int top = pickHead(joints, nJ, root, topUp, &dense);   // BUG-75
    headIdx = top; botIdx = -1;
    if (top >= 0) { findLone(); char b[160]; int o = 0; for (int k = 0; k < nLone && o < 140; k++) o += snprintf(b + o, sizeof b - o, " %d", lone[k]); b[o] = 0;
        logf("first person: riding markers (lone gear joints in the skeleton's tail):%s", nLone ? b : " none"); }
    logf("first person: %d of %d joints are hidden or unused (zero scale) in this pose; joint count from %s", hidden, nJ, countSrc == 1 ? "the node (+0x280)" : "a scan of the joint array");
    logf("first person: hero %p (class %d) -> skinned model %p at hero+0x%X%s (class %d), %d joints, head = joint %d (%.0f cm above the model root; the highest near the body axis with %d joints within %.0f cm - lone gear is skipped)",
         (void *)h, cid, (void *)n, nodeOff < 0 ? 0 : nodeOff, nodeOff2 >= 0 ? " (one level down)" : "", nodeClass, nJ, top, topUp, dense, g_fp.hideRadiusCm);
    if (nodeOff2 >= 0) logf("first person: the model is at *(hero+0x%X)+0x%X", nodeOff, nodeOff2);
    valid = headIdx >= 0 && topUp > 50.0f; return valid;
}
static void dump() {   // Shift+F5: the hero skeleton in the hero's frame (cm), for picking the head set / eye offsets
    if (!valid) { logf("first person: skeleton dump - no valid hero"); return; }
    const float *root = (const float *)(node + 0x150), *H = J(headIdx);
    float ax[3][3]; for (int r = 0; r < 3; r++) { const float l = len3(root + 4 * r); for (int c = 0; c < 3; c++) ax[r][c] = l > 1e-4f ? root[4 * r + c] / l : 0; }
    logf("first person: skeleton of the hero's model %p - root (%.1f %.1f %.1f) rows fwd(%.3f %.3f %.3f) left(%.3f %.3f %.3f) up(%.3f %.3f %.3f); joint: fwd/left/up cm from the root, distance to the head joint %d, row lengths; neck (collapse target) %d",
         (void *)node, root[12], root[13], root[14], ax[0][0], ax[0][1], ax[0][2], ax[1][0], ax[1][1], ax[1][2], ax[2][0], ax[2][1], ax[2][2], headIdx, neckIdx);
    static u8 inSet[JOINT_CAP]; memset(inSet, 0, sizeof inSet); for (int k = 0; k < nHide; k++) inSet[hideSet[k]] = 1;
    for (int i = 0; i < nJ; i++) {
        const float *m = J(i); const float d[3] = {m[12] - root[12], m[13] - root[13], m[14] - root[14]}, dh[3] = {m[12] - H[12], m[13] - H[13], m[14] - H[14]};
        logf("   joint %3d: %7.1f %7.1f %7.1f | head %6.1f | rows %.3f %.3f %.3f%s%s%s%s", i, d[0] * ax[0][0] + d[1] * ax[0][1] + d[2] * ax[0][2], d[0] * ax[1][0] + d[1] * ax[1][1] + d[2] * ax[1][2],
             d[0] * ax[2][0] + d[1] * ax[2][1] + d[2] * ax[2][2], len3(dh), len3(m), len3(m + 4), len3(m + 8), i == headIdx ? " <- head" : "", i == neckIdx ? " <- neck (collapse target)" : "", inSet[i] ? " [hidden]" : "", strayed[i] ? " [strayed]" : "");
    }
}
// Animation-job hook (job threads, every character): the hero's head set collapsed in the render-pose buffer it just wrote.
typedef void (*FnAnimJob)(u8 *); static FnAnimJob oAnimJob;
// ---------------- 0.18.7 full-body IK stage 1: VR arms (fbik.h; playbook ch12, bldbrne-vr + its credits FRIK / BetterVR) ----------------
// The skeleton's parent table is found at run time (never hard-coded): the hero's animation job hands Anim_Eval its anim object
// (job+0x48) whose rig (+0x18) holds the bone count (+0x18) and the rest pose (+0x10, 48 bytes per bone, re/decomp_anim). The table
// is the int16 array, reachable from the rig / anim object / node, in which every joint's parent comes before it (one root) and
// whose parent-child distances in the live pose are bone-sized. Then the arm chains are found by shape (fbik::findArms: a hand =
// a joint with a finger subtree, two long bones above it ending at a shoulder high and off the body axis). Fail closed: no table,
// no chains -> the arms stay animated, the log says why.
static int16_t parents[JOINT_CAP]; static std::atomic<int> parState{0}; static u64 parTryMs = 0; static int parTries = 0;
static fbik::Arm armsD[2]; static std::atomic<int> armsReady{0}, armsFiltered{0}, armsWant{1};
static fbik::Leg legsD[2]; static std::atomic<int> legsReady{0};
// 0.18.9 arm ownership (owner: "the flute, climbing and takedowns driven by the game's animation"; bldbrne-vr gates arms by animation
// class with eased hand-over): the game owns the arms while (a) an input said so - the flute (until you move or dodge), a takedown /
// quick attack (2.5 s) - or (b) the animation itself raises a hand to head height (climbing, ledges, grappling, the flute at the mouth).
static std::atomic<int> idleMoved{0};   // 0.18.13: the idle hold changed this pose
static std::atomic<float> hipYawDeg{0};   // 0.18.15: the drawn hips' yaw relative to the node (before the hip squaring)
// 0.18.16 (owner: "could the body always follow directly below the HMD - I move to the right in reality, Jin's body follows"): the
// camera already goes where your head goes (eye anchor + the headset's offset since the last recentre); the DRAWN body now goes with it.
// The headset's horizontal offset (world, game thread) is turned into Jin's model space and added to every joint of the render pose and
// to the world matrices of everything but the head set (the eye anchor reads those - no feedback), so carried items move along.
// Clamped to fp_body_under_max_cm (60): beyond that you are leaving Jin's body; the game still decides where Jin actually is.
// (0.18.24: underW / underCm / underMoved declared earlier, with the room-scale state)
static std::atomic<u64> animUntil{0}, raisedUntil{0}; static std::atomic<int> animMove{0}; static std::atomic<u64> nAnimHolds{0}, nRaised{0};
static std::atomic<float> gateW{0}, standDz{0}, standApplied{0}; static std::atomic<u64> nStand{0};
struct ArmPub { float pos[2][3], fing[2][3], thumb[2][3]; float w[2]; int ok[2]; float palm[2][3], grip[2], trig[2], thumbC[2]; };
// 0.18.18 VR fingers (fingers.h): the hands' finger chains with their bind pose, found with the arms
static fing::Hand fingD[2]; static std::atomic<int> fingState{0}; static std::atomic<u64> nFingPosed{0}; static std::atomic<float> fingCurlOut[2], thumbOut[2];
static ArmPub apub[2]; static std::atomic<int> apubIdx{-1};
// 0.18.26 the katana draw (sword.h; game-thread logic in namespace swd): the job reads the state and moves the katana joint's subtree
struct SwordPub { int state = 0; float s = 0; float axisW[3] = {0, 0, 0}; int probe = -1; int joint = -1; int onHilt = 0, onSheath = 0; float holdW[2] = {0, 0}; int blade = -1; float mouthW[3] = {0, 0, 0}, snapW[2] = {0, 0}, hiltAx = 0, hiltT = 0, thumbGap = 0.3f, sheathR = 1.6f; int probeHold = -1, keep = -1; float wristW = 0, ctrlFwd[3] = {0, 0, 0}; float bladeW[3] = {0, 0, 0}, twoW = 0, twoGap = 10.0f, rawG[2][3] = {{0, 0, 0}, {0, 0, 0}}, sheathG = -6.0f; int twoHand = 0; int capNoSnap[2] = {0, 0}; int gameBlade = -1; int katOk = 0; float katR[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1}, katT[3] = {0, 0, 0}, katAx[3] = {0, 0, 1}, dSnapW = 0; float mouthL[3] = {0, 0, 0}; int mouthLok = 0; float leftShift = 0; };
static SwordPub spub[2]; static std::atomic<int> spubIdx{-1}; static std::atomic<u64> nHoldPosed[2]{};
static volatile LONG kSeq = 0; static float kGameW[16]; static std::atomic<int> kGameOk{0}; static std::atomic<u64> nSwordMoves{0};
// 0.18.38 (owner: "when I move, the katana is not anchored in Jin's grip and moves around it"): the game thread used the katana joint's
// WORLD matrix from the last animation job - one frame old. Walking / running, the body has moved on by then (5 m/s at 55 fps = 9 cm),
// so the sheath rail, the hands locked to it and the drawn katana's sheath frame all lagged the body by that much, in the direction you
// move. Now the job publishes the joint in Jin's MODEL space too and the game thread puts it into the world with the CURRENT node.
static float kGameM[16]; static std::atomic<int> kGameMOk{0};
static bool kGameRead(float *m, const u8 *nodeNow = nullptr) { float M[16]; bool mOk = false;
    for (int t = 0; t < 4; t++) { const LONG a = kSeq; if (a & 1) continue; MemoryBarrier(); memcpy(m, kGameW, 64); memcpy(M, kGameM, 64); mOk = kGameMOk.load() != 0; MemoryBarrier(); if (a == kSeq) {
        if (mOk && nodeNow) { const float *N = (const float *)(nodeNow + 0x150); float W[16];
            for (int r = 0; r < 4; r++) { const float *v = M + 4 * r; for (int c = 0; c < 3; c++) W[4 * r + c] = v[0] * N[c] + v[1] * N[4 + c] + v[2] * N[8 + c] + (r == 3 ? N[12 + c] : 0.0f); W[4 * r + 3] = r == 3 ? 1.0f : 0.0f; }
            bool fin = true; for (int i = 0; i < 16; i++) fin &= std::isfinite(W[i]) != 0; if (fin) memcpy(m, W, 64); }
        return kGameOk.load() != 0; } }
    return false; }
// 0.18.28 (owner: "the finger poses for grabbing the sword hilt with the right hand, then the sheath with the left"): a hand holding the
// sword takes a hold pose instead of the controller's curl, blended by the hand's hold weight (SwordPub.holdW, eased 0.12 s):
//  - right hand on the hilt / the drawn katana: all four fingers wrap the tsuka (a ~3 cm oval - knuckle / middle / end 65 / 85 / 45 deg,
//    a little less than a fist, which closes the tips on the palm), the index included (no trigger finger on a katana), the thumb wrapped
//    over the index and middle like a fist (kenjutsu grip: the hand round the hilt, the thumb down, not along it)
//  - left hand on the sheath at the mouth (the saya is a bit thicker, the koiguchi grip): fingers 60 / 80 / 45; the thumb stays nearly
//    straight, resting along the sheath toward the tsuba - the thumb that pushes the guard to free the blade in Jin's draw (koiguchi o kiru)
static const float kHoldLong[2][3] = {{60.0f, 80.0f, 45.0f}, {65.0f, 85.0f, 45.0f}}, kHoldThumb[2][3] = {{5.0f, 10.0f, 8.0f}, {15.0f, 40.0f, 55.0f}}, kHoldThumbCurl[2] = {1.0f, 1.0f};
static float holdWeight(int side) { const int k = spubIdx.load(std::memory_order_acquire); if (k < 0) return 0.0f; const float w = spub[k].holdW[side]; return std::isfinite(w) ? (w < 0 ? 0 : w > 1 ? 1 : w) : 0.0f; }
static void holdBlend(int side, fing::Curl &cu, fing::Tune &tn) {
    const float hw = holdWeight(side); if (hw <= 0.0f) return; std::atomic_fetch_add(&nHoldPosed[side], (u64)1);
    int tb = side; { const int k = spubIdx.load(std::memory_order_acquire); if (side == 0 && k >= 0 && spub[k].twoHand) tb = 1; }   // 0.18.32 the left hand on the hilt: the hilt grip
    for (int i = 0; i < 3; i++) { tn.longDeg[i] += (kHoldLong[tb][i] - tn.longDeg[i]) * hw; tn.thumbDeg[i] += (kHoldThumb[tb][i] - tn.thumbDeg[i]) * hw; }
    cu.grip += (1.0f - cu.grip) * hw; cu.index += (1.0f - cu.index) * hw; cu.thumb += (kHoldThumbCurl[tb] - cu.thumb) * hw;
}
// 0.18.29 the blade alone: the joints that could carry only the blade - the katana joint's descendants first, then joints within 35 cm
// of it that are not its ancestors and carry no hand / arm (small subtrees only) - nearest first; logged once with parent, distance
// and subtree size, cycled by Ctrl+F7 once the katana is calibrated (Ctrl+F8 keeps one as sword_blade_joint)
static int bladeCand[24]; static std::atomic<int> bladeCandN{-1}, bladeCandDesc{0}; static const float *g_animPose = nullptr; static int g_animCnt = 0;   // 0.18.34: the game's own pose of this job (snapTake), before the mod
static volatile LONG kdSeq = 0; static float kDrawnG[3], kDrawnA[3]; static std::atomic<u64> kDrawnMs{0};   // 0.18.32 the drawn katana as drawn (world)
static bool kDrawnRead(float *g, float *a) { for (int t = 0; t < 4; t++) { const LONG q = kdSeq; if (q & 1) continue; MemoryBarrier(); memcpy(g, kDrawnG, 12); memcpy(a, kDrawnA, 12); MemoryBarrier(); if (q == kdSeq) return GetTickCount64() - kDrawnMs.load() < 200; } return false; }
static volatile LONG kcSeq = 0; static float kChildL[3]; static std::atomic<int> kChild{-1}; static std::atomic<u64> nKatanaInFist{0};   // 0.18.31 the katana joint's child, in the katana joint's own frame
// 0.18.30 the fist's channel (measureFist, below solveArms)
static float fistGl[2][3], fistDl[2][3], thBl[2][3], thLen[2] = {0, 0}; static bool fistLocOk[2] = {false, false};
static float fistGm[2][3], fistDm[2][3]; static bool fistMOk[2] = {false, false};
static float drawnTgtP[3], drawnTgtA[3]; static bool drawnTgtOk = false; static std::atomic<u64> nKatFistFix{0}; static std::atomic<float> katFistErrCm{0};   // 0.18.36
static std::atomic<float> armExt[2]; static std::atomic<u64> nSnapped[2], nThumbAimed{0}, nWristFwd{0}, nTwoSteer{0}, nSwingStab{0}; static std::atomic<float> fistRadCm[2];

static void orthoCols(float *R) {   // Gram-Schmidt on the columns of a row-major rotation (after a blend)
    float a[3] = {R[0], R[3], R[6]}, b[3] = {R[1], R[4], R[7]}, c[3]; if (!fbik::norm3(a)) return; const float d = fbik::dot3(a, b); for (int k = 0; k < 3; k++) b[k] -= d * a[k]; if (!fbik::norm3(b)) return;
    fbik::cross3(a, b, c); for (int k = 0; k < 3; k++) { R[k * 3] = a[k]; R[k * 3 + 1] = b[k]; R[k * 3 + 2] = c[k]; } }
static void findBladeCands(const float *pose, int cnt, int j) {
    static uint16_t tmp[512]; int n = 0; float dd[24];
    auto isAnc = [&](int a) { for (int q = parents[j], g = 0; q >= 0 && g < 512; q = parents[q], g++) if (q == a) return true; return false; };
    auto isDesc = [&](int a) { for (int q = parents[a], g = 0; q >= 0 && g < 512; q = parents[q], g++) if (q == j) return true; return false; };
    auto hasArm = [&](int a) { const int ns = fbik::subtree(parents, cnt, a, tmp, 512); if (ns > 24) return true; for (int i = 0; i < ns; i++) for (int sd = 0; sd < 2; sd++) if (tmp[i] == armsD[sd].hand || tmp[i] == armsD[sd].clav) return true; return false; };
    const float *P0 = pose + (size_t)j * 16 + 12;
    auto add = [&](int a, float d) { if (n < 24) { bladeCand[n] = a; dd[n] = d; n++; } };
    for (int a = 0; a < cnt; a++) if (a != j && isDesc(a) && !hasArm(a)) { const float *Pa = pose + (size_t)a * 16 + 12; const float d[3] = {Pa[0] - P0[0], Pa[1] - P0[1], Pa[2] - P0[2]}; add(a, fbik::len3f(d)); }
    const int nDesc = n; bladeCandDesc = nDesc;
    // 0.18.34 (owner: "are you sure the sword doesn't have its own bone?"): the joints the game's own draw moved (census: 637 99 cm, 644,
    // 651 - children of the root, far from the sheathed katana) right after the katana's own children: a weapon attached to a bone the
    // draw moves would lift alone with it
    static const int kDrawJ[16] = {637, 644, 651, 645, 646, 647, 653, 654, 652, 593, 594, 586, 587, 588, 589, 590};   // 0.18.35 + the original 0.18.26 list (owner: "when I first found the sword bone it was raised alone")
    for (const int x : kDrawJ) if (x < cnt && x != j) { bool dup = false; for (int q = 0; q < n; q++) if (bladeCand[q] == x) dup = true;
        if (!dup) { const float *Pa = pose + (size_t)x * 16 + 12; const float d[3] = {Pa[0] - P0[0], Pa[1] - P0[1], Pa[2] - P0[2]}; add(x, fbik::len3f(d)); } }
    for (int pass = 0; pass < 24 && n < 24; pass++) { int best = -1; float bd = 35.0f * g_cfg.unitsPerM * 0.01f;
        for (int a = 0; a < cnt; a++) { if (a == j || isAnc(a) || isDesc(a)) continue; bool dup = false; for (int k = 0; k < n; k++) if (bladeCand[k] == a) dup = true; if (dup) continue;
            const float *Pa = pose + (size_t)a * 16 + 12; const float d[3] = {Pa[0] - P0[0], Pa[1] - P0[1], Pa[2] - P0[2]}; const float l = fbik::len3f(d); if (l < bd && !hasArm(a)) { bd = l; best = a; } }
        if (best < 0) break; add(best, bd); }
    char b[900]; int o = 0; b[0] = 0;
    for (int k = 0; k < n && o < 860; k++) { const int ns = fbik::subtree(parents, cnt, bladeCand[k], tmp, 512); o += snprintf(b + o, sizeof b - (size_t)o, " %d(parent %d, %.0f cm, %d joints%s)", bladeCand[k], parents[bladeCand[k]], dd[k] / (g_cfg.unitsPerM * 0.01f), ns, k < nDesc ? ", under the katana" : ""); }
    { const int ns = fbik::subtree(parents, cnt, j, tmp, 512); logf("katana: blade candidates for katana joint %d (parent %d, its subtree %d joints):%s - Ctrl+F7 lifts them one by one, Ctrl+F8 keeps the one that lifts ONLY the blade", j, parents[j], ns, n ? b : " none"); }
    bladeCandN = n;
}
// 0.18.32 (owner: "the sword still never left without the sheath with Ctrl+F7"): the sheathed katana is one piece of the model; the
// separate drawn katana is something the GAME shows when it draws. Ctrl+F9 asks the game to draw / sheathe once (the touchpad swipe) and
// takes a census of Jin's joints just before and 1.5 s after: which joints appeared / disappeared (scale) or moved, and the right
// hand's own subtree - the joint that carries the game's drawn katana, to hold with our hands instead of the game's animation.
static std::atomic<int> censusReq{0}; static std::atomic<u64> nCensus{0}; static std::atomic<float> d637{-1.0f};
// 0.18.37 the game's own drawn sword (owner: "I confirmed the sword bone": joint 637 carries the drawn sword alone while the GAME has its
// sword drawn; the sheathed sword is part of the katana joint 591's model, which then shows the empty sheath). gameDrawn: the game's own
// pose (before the mod) has 637 near the katana. relHand: how the game's own animated right hand holds that sword (637 in the hand's
// frame), learned while the game's sword is drawn and the relation holds still for 0.5 s, saved to gotvr.ini.
static std::atomic<u64> maskUntil{0}, autoAfterMs{0}, nAutoSheathe{0}; static std::atomic<int> autoSave{0}; static std::atomic<int> maskStance{0}, maskMoved{0}; static std::atomic<u64> nMasked{0}; static std::atomic<float> maskWOut{0};   // 0.18.39
static std::atomic<u64> nGap{0};
static std::atomic<int> gameDrawn{0}; static fing::Aff relHand; static std::atomic<int> relHandOk{0}; static std::atomic<int> relHandSave{0}; static std::atomic<u64> nGameBlade{0};
static fing::Aff sheathX; static bool sheathXok = false;
// 0.18.39 (owner: "I want to just grab the sword and have the sheath stay put ... I'm not saying don't draw the sword, but I don't want
// the animation to play, or at least have any visual effect / effect on Jin"): the game's separate sword (637) is used again, but:
//  - its place in your hand is OURS: where the sheathed katana model's sword would be (the same K as before, the same grip, the same
//    capture), through homeX = how 637 sits in the katana joint's frame when it is home in the scabbard. homeX is learned from the
//    game's own draw / sheathe: the frame its sword passes the katana joint's child (592, the sheathed sword's pivot, 24.9 cm out -
//    0.18.37's log has 637 at 24-25 cm from 591 exactly then), saved to gotvr.ini (sword_game_home_*)
//  - the game's draw / sheathe animation is MASKED: while it plays (and while the game stands in its drawn-sword stance) Jin's pose is
//    replaced by his own pose from just before - body, head (so the camera does not move), the sheath on the hip; only 637 is left to us
//  - gameDrawn: 637 away from its parking spot (where the game keeps it while sheathed: still in the model's frame, ~100 cm from 591)
static fing::Aff homeX; static std::atomic<int> homeOk{0}, homeSave{0}; static float homeBestCm = 1e9f; static std::atomic<float> homeDistCm{1e9f};
static float wroteM[2][16]; static const float *wroteBuf[2] = {nullptr, nullptr}; static int wroteGb = -1; static std::atomic<u64> nNotAnimated{0};   // 0.18.40 what the mod last wrote into 637, per pose buffer
static float parkP[3], parkPrev[3]; static std::atomic<bool> parkOk{false}; static int parkN = 0; static std::atomic<float> parkDistCm{-1.0f};
static void gameSwordScan(const float *pose, int cnt) {   // job thread, the game's own fresh pose (before the mod changes it)
    const int k = spubIdx.load(std::memory_order_acquire); if (k < 0) { gameDrawn = 0; return; } const int gb = spub[k].gameBlade, j = spub[k].joint;
    if (gb < 0 || gb >= cnt || j < 0 || j >= cnt || parState.load() <= 0) { gameDrawn = 0; return; }
    const float u = g_cfg.unitsPerM * 0.01f; const float *P7 = pose + (size_t)gb * 16 + 12, *P1 = pose + (size_t)j * 16 + 12;
    const float d1[3] = {P7[0] - P1[0], P7[1] - P1[1], P7[2] - P1[2]}; const float d = fbik::len3f(d1) / u;
    if (!parkOk) {   // parked: far from the katana and not moving in the model's frame for 30 poses
        const float dm[3] = {P7[0] - parkPrev[0], P7[1] - parkPrev[1], P7[2] - parkPrev[2]};
        if (d > 80.0f && parkN > 0 && fbik::len3f(dm) < 0.3f * u) parkN++; else parkN = d > 80.0f ? 1 : 0; memcpy(parkPrev, P7, 12);
        if (parkN >= 30) { memcpy(parkP, P7, 12); parkOk = true; logf("katana: the game's sword (joint %d) is parked at %.0f %.0f %.0f cm while the game's sword is sheathed - away from there = the game has drawn", gb, P7[0] / u, P7[1] / u, P7[2] / u); } }
    // 0.18.40: a game that has put its sword away stops animating 637 - the buffer then still holds what the MOD wrote last frame (the sword
    // in the scabbard or in your hand), which read as drawn. Bit for bit the mod's own matrix = not animated by the game = put away.
    bool notAnimated = false; for (int q = 0; q < 2; q++) if (wroteBuf[q] == pose && wroteGb == gb && memcmp(pose + (size_t)gb * 16, wroteM[q], 64) == 0) notAnimated = true;
    static int naRun = 0; naRun = notAnimated ? naRun + 1 : 0; if (notAnimated) nNotAnimated++;
    bool gd;
    if (naRun >= 3) gd = false;   // three poses running
    else if (parkOk) { const float dp[3] = {P7[0] - parkP[0], P7[1] - parkP[1], P7[2] - parkP[2]}; const float pd = fbik::len3f(dp) / u; parkDistCm = pd; gd = gameDrawn.load() ? pd > 5.0f : pd > 15.0f; }
    else gd = d < 90.0f;
    {   // 0.18.41 (owner: "sometimes the sheathing animation still played"; log 0.18.40: the game put its sword away by itself 18.3 s after every
        // draw - its out-of-combat auto-sheathe, with the blood flick): the game's own sword heading for its scabbard in its OWN pose (it
        // never comes within 35 cm of it in the drawn stance) = the auto-sheathe has started -> masked until it is through; the time it
        // took is learned, and from then on the mask starts 1.5 s before it is due (the flick comes first) and holds 4 s.
        static bool wasGd = false, autoNow = false; static u64 drawnSince = 0; const u64 now = GetTickCount64();
        if (gd && !wasGd) { drawnSince = now; autoNow = false; }
        const int ch0 = kChild.load(); float dcA = 1e9f; if (ch0 >= 0 && ch0 < cnt) { const float *Pc = pose + (size_t)ch0 * 16 + 12; const float q[3] = {P7[0] - Pc[0], P7[1] - Pc[1], P7[2] - Pc[2]}; dcA = fbik::len3f(q) / u; }
        auto holdMask = [&](u64 ms) { const u64 mu = now + ms; if (maskUntil.load() < mu) maskUntil = mu; };
        if (gd && drawnSince && now - drawnSince > 3000 && dcA < 35.0f) { if (!autoNow) { autoNow = true; const bool ours = now - g_drawChordAt.load() < 3000; if (!ours) nAutoSheathe++; const u64 t = now - drawnSince; if (!ours && (!autoAfterMs.load() || t < autoAfterMs.load())) { autoAfterMs = t; autoSave = 1; }
                logf("katana: the game is putting its own sword away by itself (%.1f s after it drew) - hidden on Jin and silenced; the next one is masked from %.1f s", t / 1000.0, (autoAfterMs.load() - 1500) / 1000.0);
                { const u64 mu = now + 1600; if (g_muteJinUntil.load() < mu) g_muteJinUntil = mu; } }   // 0.18.65 (owner: "I can sometimes hear the game sheathe the sword when I'm holding it")
            holdMask(900); }
        const u64 at = autoAfterMs.load(); if (gd && drawnSince && at && now + 1500 >= drawnSince + at && now < drawnSince + at + 4000) holdMask(300);
        if (gd && drawnSince && now - drawnSince < 1800) holdMask(200);   // 0.18.42 the draw's own animation after it lands (any draw: ours or the background redraw)
        if (!gd && wasGd) { if (autoNow) holdMask(1600); autoNow = false; drawnSince = 0; }
        wasGd = gd;
    }
    gameDrawn = gd ? 1 : 0;
    const int ch = kChild.load();
    if (gd && ch >= 0 && ch < cnt) { const float *Pc = pose + (size_t)ch * 16 + 12; const float dc3[3] = {P7[0] - Pc[0], P7[1] - Pc[1], P7[2] - Pc[2]}; const float dc = fbik::len3f(dc3) / u; if (dc < homeDistCm.load()) homeDistCm = dc;
        if (dc < 4.0f && dc < homeBestCm - 0.2f) { fing::Aff Pj = fing::fromPose(pose + (size_t)j * 16), Pji; if (fing::inv(Pj, Pji)) { homeX = fing::mul(Pji, fing::fromPose(pose + (size_t)gb * 16)); homeBestCm = dc; homeOk = 1; homeSave = 1;
            logf("katana: learned where the game's sword sits in the scabbard (joint %d %.1f cm from joint %d as the game drew / sheathed it) - it is held in your hand from now on, the sheath stays on the hip", gb, dc, ch); } } }
}
static void census(const float *pose, int cnt) {
    // 0.18.34: 2.5 s after the request, every 50 ms: the watched joints (the katana 591 and its child, the drawn-weapon candidates 637 /
    // 644 / 651, the right hand 561 - as ANIMATED by the game, before the mod's IK) - position from the root (cm) and how far it moved
    // since the press; 637 also in the katana joint's frame and in the animated right hand's frame (the game's own grip of the drawn sword).
    // At the end: every joint that moved > 20 cm or changed scale over the window (as in 0.18.32).
    static float A[JOINT_CAP][4]; static int phase = 0, n0 = 0; static u64 t0 = 0, lastS = 0; const u64 now = GetTickCount64();
    const float *ap = (g_animPose && g_animCnt >= cnt) ? g_animPose : pose;   // the game's own pose when there is one
    const float u = g_cfg.unitsPerM * 0.01f;
    auto snap = [&](float (*o)[4]) { for (int j = 0; j < cnt && j < JOINT_CAP; j++) { const float *m = ap + (size_t)j * 16; o[j][0] = m[12]; o[j][1] = m[13]; o[j][2] = m[14]; o[j][3] = sqrtf(m[0] * m[0] + m[1] * m[1] + m[2] * m[2]); } };
    auto relTo = [&](int base, int j, float *o) { if (base < 0 || base >= cnt || j < 0 || j >= cnt) return false; fing::Aff B = fing::fromPose(ap + (size_t)base * 16), Bi; if (!fing::inv(B, Bi)) return false;
        const fing::Aff J = fing::mul(Bi, fing::fromPose(ap + (size_t)j * 16)); for (int k = 0; k < 3; k++) o[k] = J.t[k] / u;
        o[3] = atan2f(J.R[3], J.R[0]) * 57.2957795f; o[4] = asinf(fminf(1.0f, fmaxf(-1.0f, -J.R[6]))) * 57.2957795f; o[5] = atan2f(J.R[7], J.R[8]) * 57.2957795f; return true; };
    const int watch[6] = {591, 592, 637, 644, 651, armsD[1].hand >= 0 ? armsD[1].hand : 561};
    if (censusReq.exchange(0)) { snap(A); n0 = cnt; t0 = now; lastS = 0; phase = 1;
        const fbik::Arm &R = armsD[1]; char b[1200]; int o = 0; b[0] = 0;
        for (int i = 0; i < R.nSub[3] && o < 1150; i++) { const int j = R.sub[3][i]; const float *m = ap + (size_t)j * 16; o += snprintf(b + o, sizeof b - (size_t)o, " %d(p%d s%.2f)", j, parents[j], sqrtf(m[0] * m[0] + m[1] * m[1] + m[2] * m[2])); }
        logf("katana census: before the game's draw / sheathe (%s pose) - the right hand's subtree (joint, parent, scale):%s", ap == pose ? "the drawn" : "the game's own", b);
        static const int kRelJ[4] = {637, 644, 651, 592}; for (const int x : kRelJ) { float r[6]; if (relTo(591, x, r)) logf("katana census:    joint %d (parent %d) in the katana joint 591's frame: (%.1f %.1f %.1f) cm, turned %.0f / %.0f / %.0f deg", x, x < cnt ? parents[x] : -1, r[0], r[1], r[2], r[3], r[4], r[5]); }
        return; }
    if (phase == 1) {
        const u64 el = now - t0;
        if (el - lastS >= 50 && el < 2500) { lastS = el - (el % 50); char b[700]; int o = 0; b[0] = 0;
            for (const int x : watch) { if (x < 0 || x >= cnt || x >= n0) continue; const float *m = ap + (size_t)x * 16; const float d[3] = {m[12] - A[x][0], m[13] - A[x][1], m[14] - A[x][2]};
                o += snprintf(b + o, sizeof b - (size_t)o, " %d(%.0f %.0f %.0f, moved %.0f)", x, m[12] / u, m[13] / u, m[14] / u, sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]) / u); }
            float rh[6], rk[6]; const int hj = watch[5]; const bool okH = relTo(hj, 637, rh), okK = relTo(591, 637, rk);
            if (okH && o < 640) o += snprintf(b + o, sizeof b - (size_t)o, " | 637 in the hand: (%.1f %.1f %.1f) %.0f/%.0f/%.0f", rh[0], rh[1], rh[2], rh[3], rh[4], rh[5]);
            if (okK && o < 680) snprintf(b + o, sizeof b - (size_t)o, " | in 591: (%.1f %.1f %.1f)", rk[0], rk[1], rk[2]);
            logf("katana census: +%4llu ms%s", (unsigned long long)el, b); }
        if (el >= 2500) { phase = 0; static float B[JOINT_CAP][4]; snap(B); char b[1800]; int o = 0, k = 0; b[0] = 0;
            for (int j = 0; j < cnt && j < n0 && j < JOINT_CAP && o < 1700; j++) { const float d[3] = {B[j][0] - A[j][0], B[j][1] - A[j][1], B[j][2] - A[j][2]}; const float mv = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]) / u;
                const bool sc = fabsf(B[j][3] - A[j][3]) > 0.3f; if (sc || mv > 20.0f) { k++; o += snprintf(b + o, sizeof b - (size_t)o, " %d(p%d moved %.0f cm, scale %.2f->%.2f)", j, parents[j], mv, A[j][3], B[j][3]); } }
            logf("katana census: 2.5 s after - %d joints changed:%s", k, k ? b : " none"); nCensus++; }
    }
}
static bool swordApply(u8 *nd, float *pose, int cnt) {   // job thread, after the arms, before the world matrices
    int sp0j = -1; { const int k0 = spubIdx.load(std::memory_order_acquire); if (k0 >= 0) sp0j = spub[k0].joint; }
    if (parState.load() > 0) census(pose, cnt);
    if (sp0j >= 0 && sp0j < cnt && 637 < cnt) { const float *a = pose + (size_t)sp0j * 16 + 12, *b = pose + 637 * 16 + 12; const float d[3] = {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; d637 = fbik::len3f(d) / (g_cfg.unitsPerM * 0.01f); }
    const int k = spubIdx.load(std::memory_order_acquire); if (k < 0) return false; const SwordPub sp = spub[k];
    const float *N = (const float *)(nd + 0x150); const int j = sp.joint;
    float W[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    if (j >= 0 && j < cnt) {   // the rail frame: the katana joint as drawn (body turn / shifts included), BEFORE the mod moves it
        const float *M = pose + (size_t)j * 16;
        for (int r = 0; r < 4; r++) { const float *v = M + 4 * r; for (int c = 0; c < 3; c++) W[4 * r + c] = v[0] * N[c] + v[1] * N[4 + c] + v[2] * N[8 + c] + (r == 3 ? N[12 + c] : 0.0f); W[4 * r + 3] = r == 3 ? 1.0f : 0.0f; }
        InterlockedIncrement(&kSeq); MemoryBarrier(); memcpy(kGameW, W, 64); memcpy(kGameM, M, 64); kGameOk = 1; kGameMOk = 1; MemoryBarrier(); InterlockedIncrement(&kSeq);
    }
    static uint16_t sub[512];
    auto moveSub = [&](int root, const fing::Aff &D) { const int n = fbik::subtree(parents, cnt, root, sub, 512); if (n <= 0) return false;
        for (int i = 0; i < n; i++) { float *m = pose + (size_t)sub[i] * 16; const fing::Aff a = fing::fromPose(m); fing::toPose(fing::mul(D, a), m); } return true; };
    if (j >= 0 && j < cnt && parState.load() > 0 && bladeCandN.load() < 0) findBladeCands(pose, cnt, j);
    if (j >= 0 && j < cnt && parState.load() > 0) {   // 0.18.31 the katana joint's child (592: the other end of the hilt?) in the katana joint's frame
        static int ch = -2, chFor = -1; if (chFor != j) { chFor = j; ch = -1; for (int a = 0; a < cnt; a++) if (parents[a] == j) { ch = a; break; } }
        if (ch >= 0) { fing::Aff Pj = fing::fromPose(pose + (size_t)j * 16), Pi; if (fing::inv(Pj, Pi)) { const float *c = pose + (size_t)ch * 16 + 12; float l[3];
                for (int k = 0; k < 3; k++) l[k] = Pi.R[k * 3] * c[0] + Pi.R[k * 3 + 1] * c[1] + Pi.R[k * 3 + 2] * c[2] + Pi.t[k];
                InterlockedIncrement(&kcSeq); MemoryBarrier(); memcpy(kChildL, l, 12); kChild = ch; MemoryBarrier(); InterlockedIncrement(&kcSeq); } }
    }   // 0.18.29 once: what could be the blade alone
    if (sp.probe >= 0 && sp.probe < cnt && parState.load() > 0) {   // the joint probe: lift the candidate 15 cm so you can see what it carries
        fing::Aff D; for (int i = 0; i < 9; i++) D.R[i] = (i % 4 == 0) ? 1.0f : 0.0f; D.t[0] = D.t[1] = 0; D.t[2] = 15.0f * g_cfg.unitsPerM * 0.01f;
        if (sp.probeHold >= 0 && sp.probeHold < cnt) {   // 0.18.31: lift the probe but hold this joint's subtree still (is it the sheath?)
            static uint16_t hs[512]; static float hsave[64][16]; int nh = fbik::subtree(parents, cnt, sp.probeHold, hs, 512); if (nh > 64) nh = 64; for (int i = 0; i < nh; i++) memcpy(hsave[i], pose + (size_t)hs[i] * 16, 64);
            const bool r = moveSub(sp.probe, D); for (int i = 0; i < nh; i++) memcpy(pose + (size_t)hs[i] * 16, hsave[i], 64); return r; }
        return moveSub(sp.probe, D); }
    static int lastState = sword::S_SHEATHED; static fing::Aff lastMt; static bool lastMtOk = false; static float blendT = 1.0f; static LARGE_INTEGER bq{}, bf{};
    const int gb = sp.gameBlade;
    // 0.18.39 the game's sword in our hand: while the game has it drawn and we know where it sits in the scabbard, 637 goes where the
    // katana model's sword would go (below); the katana joint (the sheath) is never moved.
    const bool gbMode = gb >= 0 && gb < cnt && j >= 0 && j < cnt && gameDrawn.load() && homeOk.load() && parState.load() > 0;
    if (gbMode && sp.state == sword::S_SHEATHED) {   // home in the scabbard (the game's sword still out until its sheathe is through)
        const fing::Aff P7 = fing::fromPose(pose + (size_t)gb * 16); fing::Aff P7i; lastMtOk = false; lastState = sp.state; if (!fing::inv(P7, P7i)) return false;
        nGameBlade++; return moveSub(gb, fing::mul(fing::mul(fing::fromPose(pose + (size_t)j * 16), homeX), P7i)); }
    if (sp.state == sword::S_SHEATHED || j < 0 || j >= cnt || parState.load() <= 0) { lastMtOk = false; lastState = sp.state; return false; }
    {   // 0.18.41 the game's sword briefly away (its auto-sheathe, before the background redraw lands): the sheath stays on the hip rather than
        // flying into your hand with its sword; after 3 s without the game's sword the old whole-katana fallback shows
        static u64 gapSince = 0; const u64 nowG = GetTickCount64();
        if (gb >= 0 && gb < cnt && homeOk.load() && parkOk && !gameDrawn.load()) { if (!gapSince) gapSince = nowG; if (nowG - gapSince < 3000) { lastMtOk = false; lastState = sp.state; nGap++; return false; } } else gapSince = 0; }
    const float n00 = N[0] * N[0] + N[1] * N[1] + N[2] * N[2], n11 = N[4] * N[4] + N[5] * N[5] + N[6] * N[6], n22 = N[8] * N[8] + N[9] * N[9] + N[10] * N[10]; if (!(n00 > 1e-6f && n11 > 1e-6f && n22 > 1e-6f)) return false;
    const float u = g_cfg.unitsPerM * 0.01f;
    // 0.18.29 (owner: "when drawing, the whole sheath and sword move - just the sword should move"): the BLADE joint (sword_blade_joint,
    // picked with Ctrl+F7 / Ctrl+F8) moves; the sheath (the katana joint, whose frame carries the rail) stays on the hip. If the blade
    // joint's subtree holds the sheath joint, the sheath's subtree is put back. No blade joint chosen yet: the whole katana moves (as before).
    const int b = (sp.blade >= 0 && sp.blade < cnt) ? sp.blade : j;
    const float am[3] = {(sp.axisW[0] * N[0] + sp.axisW[1] * N[1] + sp.axisW[2] * N[2]) / n00, (sp.axisW[0] * N[4] + sp.axisW[1] * N[5] + sp.axisW[2] * N[6]) / n11, (sp.axisW[0] * N[8] + sp.axisW[1] * N[9] + sp.axisW[2] * N[10]) / n22};
    const fing::Aff Pb = fing::fromPose(pose + (size_t)b * 16); fing::Aff Mt = Pb; for (int c = 0; c < 3; c++) Mt.t[c] += am[c] * sp.s * u;
    if (!bf.QuadPart) QueryPerformanceFrequency(&bf); LARGE_INTEGER qn; QueryPerformanceCounter(&qn); const float bdt = bq.QuadPart ? fminf((float)(qn.QuadPart - bq.QuadPart) / (float)bf.QuadPart, 0.1f) : 0.0f; bq = qn;
    if (sp.state == sword::S_DRAWN && sp.katOk) {
        // 0.18.33 (owner photos: the grip on the wrong side, the katana across the controller): the drawn katana is placed by the GAME
        // THREAD from your right controller (sp.kat*: the blade frame in the world - the hilt through your controller's grip point, the
        // blade along the controller's forward, the second hand's steering), and Jin's right fist is snapped onto it (solveArms) - the
        // katana is steered by the raw controller, the hand is drawn on it (playbook: the pose you draw and the pose you steer with are
        // different variables). Blade joint = K o Sf^-1 o (the blade joint as it sits in the sheath), all in the model's frame.
        fing::Aff Nf = fing::fromPose(N), Ni, K, Sfw = fing::fromPose(W), Si; memcpy(K.R, sp.katR, 36); memcpy(K.t, sp.katT, 12); memcpy(Sfw.t, sp.mouthW, 12);
        if (sp.mouthLok) for (int c = 0; c < 3; c++) Sfw.t[c] = sp.mouthL[0] * W[c] + sp.mouthL[1] * W[4 + c] + sp.mouthL[2] * W[8 + c] + W[12 + c];   // 0.18.38 the mouth from THIS frame's katana joint (the game thread's is a frame older)
        if (fing::inv(Nf, Ni) && fing::inv(Sfw, Si)) { Mt = fing::mul(Ni, fing::mul(K, fing::mul(Si, fing::mul(Nf, Pb)))); nKatanaInFist++;
            // 0.18.36 (owner: "the katana should never go beyond Jin's hand, even when the controller is stretched beyond it"; "it sometimes
            // strays around his hand - have it firmly stay in place"): the katana was placed where the CONTROLLER says; when the arm cannot
            // reach (or the hand turns less than asked) Jin's fist is elsewhere. Now the katana is carried by the fist as solved: turned by the
            // difference between the line the fist was asked for and the line it has, and moved by the difference between the asked and the
            // actual fist centre - it can never leave the hand.
            if (drawnTgtOk && fistMOk[1]) { float Da[3] = {fistDm[1][0], fistDm[1][1], fistDm[1][2]}; if (fbik::dot3(Da, drawnTgtA) < 0) for (auto &x : Da) x = -x;
                const fbik::Rot Rf = fbik::arc(drawnTgtA, Da); float Rn[9]; for (int r = 0; r < 3; r++) for (int c = 0; c < 3; c++) Rn[r * 3 + c] = Rf.m[r * 3] * Mt.R[c] + Rf.m[r * 3 + 1] * Mt.R[3 + c] + Rf.m[r * 3 + 2] * Mt.R[6 + c];
                float d[3] = {Mt.t[0] - drawnTgtP[0], Mt.t[1] - drawnTgtP[1], Mt.t[2] - drawnTgtP[2]}, e[3]; fbik::rotv(Rf, d, e); memcpy(Mt.R, Rn, 36); for (int c = 0; c < 3; c++) Mt.t[c] = fistGm[1][c] + e[c];
                const float er[3] = {fistGm[1][0] - drawnTgtP[0], fistGm[1][1] - drawnTgtP[1], fistGm[1][2] - drawnTgtP[2]}; katFistErrCm = fbik::len3f(er) / u; nKatFistFix++; } }
        if (lastState == sword::S_DRAWING && lastMtOk) blendT = 0.0f;   // it came free: ease from the rail to the hand (the draw's gain put the rail ahead)
    } else if (lastState == sword::S_DRAWN && sp.state == sword::S_SHEATHING && lastMtOk) blendT = 0.0f;   // the tip went into the mouth: ease onto the rail
    if (blendT < 1.0f && lastMtOk) { blendT = fminf(1.0f, blendT + bdt / 0.18f); const float w = 0.5f - 0.5f * cosf(blendT * 3.14159265f);
        for (int i2 = 0; i2 < 9; i2++) Mt.R[i2] = lastMt.R[i2] + (Mt.R[i2] - lastMt.R[i2]) * w; for (int c = 0; c < 3; c++) Mt.t[c] = lastMt.t[c] + (Mt.t[c] - lastMt.t[c]) * w;
        orthoCols(Mt.R); }
    lastState = sp.state; lastMt = Mt; lastMtOk = true;
    fing::Aff PbI; if (!fing::inv(Pb, PbI)) return false;
    if (gbMode) {   // 0.18.39: the katana joint where the drawn katana would put it, then the game's sword in it (homeX); the sheath stays
        const fing::Aff Jn = fing::mul(Mt, fing::mul(PbI, fing::fromPose(pose + (size_t)j * 16))); const fing::Aff P7 = fing::fromPose(pose + (size_t)gb * 16); fing::Aff P7i; if (!fing::inv(P7, P7i)) return false;
        nGameBlade++; if (moveSub(gb, fing::mul(fing::mul(Jn, homeX), P7i))) { nSwordMoves++; return true; } return false; }
    // the sheath's own subtree, kept when the blade joint carries it
    static uint16_t ssub[512]; static float ssave[64][16]; int ns = 0; bool keepSheath = false;
    const int kj = (sp.keep >= 0 && sp.keep < cnt && sp.keep != b) ? sp.keep : (b != j ? j : -1);   // 0.18.31: sword_keep_joint (the sheath under the katana joint)
    if (kj >= 0) { int nb = fbik::subtree(parents, cnt, b, sub, 512); for (int i = 0; i < nb; i++) if (sub[i] == kj) { keepSheath = true; break; }
        if (keepSheath) { ns = fbik::subtree(parents, cnt, kj, ssub, 512); if (ns > 64) ns = 64; for (int i = 0; i < ns; i++) memcpy(ssave[i], pose + (size_t)ssub[i] * 16, 64); } }
    if (moveSub(b, fing::mul(Mt, PbI))) { if (keepSheath) for (int i = 0; i < ns; i++) memcpy(pose + (size_t)ssub[i] * 16, ssave[i], 64); nSwordMoves++; return true; }
    return false;
}
static std::atomic<u64> nArmSolves{0}, nArmSkipped{0}, nLeftOnFist{0}; static std::atomic<float> armW[2];
static bool tryParentTable(u8 *obj, int n, const float *pose, int depth, char *where, int whereN) {
    if (!obj || ((u64)obj & 7) || (u64)obj < 0x10000 || (u64)obj > 0x7FFFFFFFFFFFull || !readable(obj, 0x200)) return false;
    for (int off = 0; off < 0x200; off += 8) {
        u8 *p = *(u8 **)(obj + off);
        if (!p || ((u64)p & 1) || (u64)p < 0x10000 || (u64)p > 0x7FFFFFFFFFFFull) continue;
        if (readable(p, (size_t)n * 2) && fbik::parentsOk((const int16_t *)p, n)) {
            const int16_t *pa = (const int16_t *)p; int good = 0, tot = 0;
            for (int i = 0; i < n; i++) { const int q = pa[i]; if (q < 0) continue; const float *a = pose + (size_t)i * 16, *b = pose + (size_t)q * 16;
                if (!fbik::shownJ(pose, i) || !fbik::shownJ(pose, q)) continue; tot++; const float d[3] = {a[12] - b[12], a[13] - b[13], a[14] - b[14]}; if (fbik::len3f(d) < 80.0f) good++; }
            if (tot > n / 3 && good * 10 >= tot * 9) { memcpy(parents, pa, (size_t)n * 2); snprintf(where, (size_t)whereN, "%p+0x%X -> %p (%d of %d bone lengths under 80 cm)", (void *)obj, off, (void *)p, good, tot); return true; }
        }
        if (depth > 0 && readable(p, 0x200) && tryParentTable(p, n, pose, depth - 1, where, whereN)) return true;
    }
    return false;
}
static void analyseArms(u8 *job, u8 *nd, const float *pose, int cnt) {   // job thread, the hero's fresh render pose (before the head is hidden)
    const u64 now = GetTickCount64(); if (now < parTryMs) return; parTryMs = now + 3000;
    if (parState.load() == 0) {
        char where[160] = ""; u8 *anim = *(u8 **)(job + 0x48); u8 *rig = anim && readable(anim + 0x18, 8) ? *(u8 **)(anim + 0x18) : nullptr;
        const int rc = rig && readable(rig + 0x18, 4) ? *(int *)(rig + 0x18) : -1;
        const bool ok = cnt >= 8 && cnt <= JOINT_CAP && (tryParentTable(rig, cnt, pose, 1, where, sizeof where) || tryParentTable(anim, cnt, pose, 1, where, sizeof where) || tryParentTable(nd, cnt, pose, 0, where, sizeof where));
        if (ok) { parState = 1; logf("arms: the skeleton's parent table found at %s (rig %p, rig bone count %d, pose joints %d)", where, (void *)rig, rc, cnt); }
        else if (++parTries >= 5) { parState = -1; logf("arms: no parent table found near the rig %p (rig bone count %d, pose joints %d) -> the arms stay animated (please send this log)", (void *)rig, rc, cnt); return; }
        else return;
    }
    if (parState.load() != 1 || armsReady.load()) return;
    static uint8_t skip[JOINT_CAP]; memset(skip, 0, sizeof skip);
    const int k = hpubIdx.load(std::memory_order_acquire); float headZ = 160.0f;
    if (k >= 0) { const HidePub &hp = hpub[k]; for (int i = 0; i < hp.n; i++) if (hp.idx[i] < JOINT_CAP && hp.idx[i] != hp.target) skip[hp.idx[i]] = 1; if (hp.head >= 0 && hp.head < cnt) headZ = pose[(size_t)hp.head * 16 + 14]; }
    char why[400]; const int f = fbik::findArms(parents, cnt, pose, headZ, skip, armsD, why, sizeof why);
    logf("arms: %s", why);
    { char lw[300]; const int nl = fbik::findLegs(parents, cnt, pose, headZ, skip, legsD, lw, sizeof lw); logf("legs: %s -> %s", lw, nl == 2 ? "stand tall available" : "stand tall off (legs not found)"); if (nl == 2) legsReady = 1; }
    if (f == 2) { armsReady = 1; logf("arms: both arm chains found -> Jin's arms follow your controllers (fp_arms = 1, Shift+F10 toggles)");
        for (int sd = 0; sd < 2; sd++) { char tb[160]; int o = 0; tb[0] = 0; for (int i = 0; i < armsD[sd].nTw && o < 150; i++) o += snprintf(tb + o, sizeof tb - (size_t)o, " %d (%.0f %%)", armsD[sd].tw[i], armsD[sd].twT[i] * 100.0f);
            logf("arms: %s forearm twist joints:%s -> the wrist's roll is shared along the forearm (fp_forearm_twist %.2f)", sd ? "right" : "left", armsD[sd].nTw ? tb : " none", g_fp.forearmTwist); }
        {   // 0.18.18 fingers: the bind pose from the skinning palette's inverse-bind matrices (node+0x2F8 -> +0x660 -> +0xA8, 3x4 each)
            u8 *p1 = readable(nd + 0x2F8, 8) ? *(u8 **)(nd + 0x2F8) : nullptr; u8 *p2 = p1 && readable(p1 + 0x660, 8) ? *(u8 **)(p1 + 0x660) : nullptr;
            const float *IB = p2 && readable(p2 + 0xA8, 8) ? *(const float **)(p2 + 0xA8) : nullptr; const int ibN = readable(nd + 0x280, 4) ? *(int *)(nd + 0x280) - 1 : 0;
            int mode = 0; auto bindOf = [&](int j, fing::Aff &o) -> bool { if (!IB || j < 0 || j >= ibN || !readable(IB + (size_t)j * 12, 48)) return false; const fing::Aff a = fing::fromRows34(IB + (size_t)j * 12);
                for (int k = 0; k < 12; k++) if (!std::isfinite(IB[(size_t)j * 12 + k])) return false; if (mode == 0) return fing::inv(a, o); o = a; return true; };
            // 0.18.19: both readings of the palette matrices are analysed; the one whose finger lengths match the animation wins
            char fw[2][2][260]; int okm[2] = {0, 0}, badm[2] = {0, 0}, linkm[2] = {0, 0};
            for (mode = 0; mode < 2; mode++) for (int sd = 0; sd < 2; sd++) { if (fing::analyse(fingD[sd], parents, cnt, pose, armsD[sd], bindOf, fw[mode][sd], sizeof fw[mode][sd])) okm[mode]++; badm[mode] += fingD[sd].bad; linkm[mode] += fingD[sd].links; }
            int pick = -1; for (int m = 0; m < 2; m++) if (okm[m] == 2 && (pick < 0 || badm[m] * linkm[pick] < badm[pick] * linkm[m])) pick = m;
            if (pick >= 0) { mode = pick; for (int sd = 0; sd < 2; sd++) fing::analyse(fingD[sd], parents, cnt, pose, armsD[sd], bindOf, fw[pick][sd], sizeof fw[pick][sd]); }
            fingState = pick >= 0 ? 1 : -1;
            logf("fingers: inverse-bind array %p (%d joints) -> %s", (void *)IB, ibN, pick == 0 ? "bind = the inverse of the palette matrices -> VR fingers ON (grip = fist)" : pick == 1 ? "bind = the palette matrices themselves -> VR fingers ON (grip = fist)" : "not usable -> the fingers stay animated (please send this log)");
            for (int m = 0; m < 2; m++) logf("   fingers, %s: left: %s | right: %s", m == 0 ? "inverse" : "direct", fw[m][0], fw[m][1]);
            {   // the hands' joint structure, once (parent, children, hand-local position animated / bind, link lengths)
                mode = pick >= 0 ? pick : 0;
                for (int sd = 0; sd < 2; sd++) { const fbik::Arm &A = armsD[sd]; if (A.hand < 0) continue; const float *Hm = pose + (size_t)A.hand * 16; fing::Aff Bh, BhI; const bool hb = bindOf(A.hand, Bh) && fing::inv(Bh, BhI);
                    logf("   %s hand %d structure (joint parent kids | animated hand-local x y z | bind hand-local x y z | link animated / bind cm):", sd ? "right" : "left", A.hand);
                    for (int i = 1; i < A.nSub[3]; i++) { const int j = A.sub[3][i], pj = parents[j]; int kids = 0; for (int k = 0; k < A.nSub[3]; k++) if (parents[A.sub[3][k]] == j) kids++;
                        const float *M = pose + (size_t)j * 16, *PM = pose + (size_t)pj * 16; float d[3] = {M[12] - Hm[12], M[13] - Hm[13], M[14] - Hm[14]}, al[3];
                        for (int r = 0; r < 3; r++) { float ax[3] = {Hm[4 * r], Hm[4 * r + 1], Hm[4 * r + 2]}; fbik::norm3(ax); al[r] = fbik::dot3(d, ax); }
                        const float la = sqrtf((M[12] - PM[12]) * (M[12] - PM[12]) + (M[13] - PM[13]) * (M[13] - PM[13]) + (M[14] - PM[14]) * (M[14] - PM[14]));
                        float bl[3] = {0, 0, 0}, lb = -1; fing::Aff Bj, Bp; if (hb && bindOf(j, Bj)) { const fing::Aff L = fing::mul(BhI, Bj); bl[0] = L.t[0]; bl[1] = L.t[1]; bl[2] = L.t[2]; if (bindOf(pj, Bp)) lb = fing::dist(Bj.t, Bp.t); }
                        logf("      %4d %4d %d | %6.1f %6.1f %6.1f | %6.1f %6.1f %6.1f | %5.1f / %5.1f", j, pj, kids, al[0], al[1], al[2], bl[0], bl[1], bl[2], la, lb); }
                }
            }
        }
    }
    else {
        parState = -2; logf("arms: only %d arm chain(s) found -> the arms stay animated (please send this log); the skeleton follows (joint parent fwd left up cm, model space, * = hidden)", f);
        for (int j = 0; j < cnt; j++) { const float *m = pose + (size_t)j * 16; logf("   arms joint %3d parent %3d %7.1f %7.1f %7.1f%s%s", j, (int)parents[j], m[12], m[13], m[14], fbik::shownJ(pose, j) ? "" : " *", skip[j] ? " [head]" : ""); }
    }
}
// 0.18.30 (owner, photos: "my right hand is misaligned" on the hilt, "the left hand needs to be at the sheath's mouth where the guard is,
// the thumb outstretched along the sheath's ridge, its nail touching the guard", "the misalignment when holding the sword"): the hand
// is no longer placed by the controller's grip point (Jin's fist is somewhere else relative to it, set by the hand trims) but by its OWN
// fist: the channel the curled fingers wrap. For each long finger the circle through its three bending joints (knuckle, middle, end)
// has its centre on the handle's axis (a finger wrapped round a cylinder); the centres' mean is the channel point G and index -> little
// is its direction D. Measured on the posed hand each frame and kept in the hand's own frame, so before the next solve the wrist target
// can be moved / turned so that G lands on the sword's line and D along it (the right hand: at the hilt where you took it; the left: at
// the sheath, as far below the mouth as its thumb reaches - the thumb is then aimed along the sheath's surface to the guard). With
// the katana free in the hand it is the other way round: the katana is placed so its hilt runs through G along D.
// 0.18.32 the two-handed line: turn the one-handed direction d toward the hand line L (both unit), 1:1 up to 55 deg and compressed
// asymptotically toward 90 deg past it (FEAR VR's soft steer limit), by fade (0..1)
static void twoSteer(const float *d, const float *L, float fade, float *out) {
    const float a = acosf(fminf(1.0f, fmaxf(-1.0f, fbik::dot3(d, L)))); const float soft = 0.96f, mx = 1.57f;
    const float a2 = a <= soft ? a : soft + (mx - soft) * (1.0f - expf(-(a - soft) / (mx - soft)));
    const fbik::Rot Rl = fbik::scaled(fbik::arc(d, L), (a > 1e-4f ? a2 / a : 1.0f) * fade); fbik::rotv(Rl, d, out); }
static bool circumC(const float *a, const float *b, const float *c, float *o) {
    const float ab[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]}, ac[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]}; float n[3]; fbik::cross3(ab, ac, n);
    const float nn = fbik::dot3(n, n); if (!(nn > 1e-6f)) return false; float x1[3], x2[3]; fbik::cross3(n, ab, x1); fbik::cross3(ac, n, x2);
    const float lab = fbik::dot3(ab, ab), lac = fbik::dot3(ac, ac); for (int k = 0; k < 3; k++) o[k] = a[k] + (lac * x1[k] + lab * x2[k]) / (2.0f * nn); return std::isfinite(o[0]) && std::isfinite(o[1]) && std::isfinite(o[2]); }
static bool measureFist(const float *pose, int side, float *G, float *D, float *radOut) {
    const fing::Hand &H = fingD[side]; if (!H.ok) return false; float cc[fing::MAXF][3]; bool isIdx[fing::MAXF]; int n = 0; float rad = 0;
    for (int c = 0; c < H.nf && n < fing::MAXF; c++) { const fing::Chain &C = H.f[c]; if (C.thumb || C.n - C.first < 3) continue;
        const float *a = fbik::P(pose, C.j[C.first]), *b = fbik::P(pose, C.j[C.first + 1]), *e = fbik::P(pose, C.j[C.first + 2]);
        if (!circumC(a, b, e, cc[n])) continue; const float r[3] = {b[0] - cc[n][0], b[1] - cc[n][1], b[2] - cc[n][2]}; rad += fbik::len3f(r); isIdx[n] = C.index; n++; }
    if (n < 2) return false; for (int k = 0; k < 3; k++) { G[k] = 0; for (int i = 0; i < n; i++) G[k] += cc[i][k] / (float)n; }
    int ix = 0; for (int i = 0; i < n; i++) if (isIdx[i]) ix = i; int lt = ix; float bd = -1; for (int i = 0; i < n; i++) { const float d[3] = {cc[i][0] - cc[ix][0], cc[i][1] - cc[ix][1], cc[i][2] - cc[ix][2]}; const float l = fbik::len3f(d); if (l > bd) { bd = l; lt = i; } }
    for (int k = 0; k < 3; k++) D[k] = cc[lt][k] - cc[ix][k]; if (!fbik::norm3(D)) return false; if (radOut) *radOut = rad / (float)n; return true; }
static int thumbChain(int side) { const fing::Hand &H = fingD[side]; for (int c = 0; c < H.nf; c++) if (H.f[c].thumb && H.f[c].n - H.f[c].first >= 2) return c; return -1; }
static void thumbTip(const float *pose, const fing::Chain &C, float *tip) { const float *l = fbik::P(pose, C.j[C.n - 1]), *q = fbik::P(pose, C.j[C.n - 2]); for (int k = 0; k < 3; k++) tip[k] = l[k] + (l[k] - q[k]) * 0.75f; }
static bool solveArms(u8 *nd, float *pose, int cnt, float bodyA) {   // job thread, after the head is hidden and the body turned
    if (!armsReady.load() || !armsWant.load() || !g_fp.arms || (ridingNow.load() && !g_fp.armsRiding)) { nArmSkipped++; return false; }   // 0.18.65: the "riding" test also fires on ropes / logs / climbs (gear lifted) - VR arms kept (arms_game_when_riding 0 = old)
    const int k = apubIdx.load(std::memory_order_acquire); if (k < 0) return false; const ArmPub ap = apub[k];
    const float *N = (const float *)(nd + 0x150); float n0[3] = {N[0], N[1], N[2]}, n1[3] = {N[4], N[5], N[6]}, n2[3] = {N[8], N[9], N[10]};
    if (!fbik::norm3(n0) || !fbik::norm3(n1) || !fbik::norm3(n2)) return false;
    auto toModelP = [&](const float *w, float *o) { const float d[3] = {w[0] - N[12], w[1] - N[13], w[2] - N[14]}; o[0] = fbik::dot3(d, n0); o[1] = fbik::dot3(d, n1); o[2] = fbik::dot3(d, n2); };
    auto toModelD = [&](const float *w, float *o) { o[0] = fbik::dot3(w, n0); o[1] = fbik::dot3(w, n1); o[2] = fbik::dot3(w, n2); };
    fbik::Params prm; prm.bodyYaw = bodyA; prm.twist = g_fp.forearmTwist; prm.stretch = g_fp.armStretch; bool any = false; drawnTgtOk = false;
    const int spk = spubIdx.load(std::memory_order_acquire); SwordPub sp; if (spk >= 0) sp = spub[spk];   // 0.18.30 the sword the hands hold
    for (int si = 0; si < 2; si++) {
        const int side = 1 - si;   // 0.18.32: the right hand first - the left one may take the drawn katana's hilt from it (two hands)
        const fbik::Arm &A = armsD[side]; if (!ap.ok[side] || ap.w[side] <= 0.0f || A.hand < 0 || A.hand >= cnt || A.clav < 0) continue;
        fbik::Target T; toModelP(ap.pos[side], T.pos); toModelD(ap.fing[side], T.fing); toModelD(ap.thumb[side], T.thumb); T.orient = true;
        const float w = ap.w[side] * (1.0f - gateW.load()); if (w <= 0.001f) continue;
        const float u = g_cfg.unitsPerM * 0.01f;
        // 0.18.30 / 0.18.33 the fist's channel onto the sword's line (see measureFist). The line and the point on it, per hand:
        //  - right, katana in the sheath / being drawn / going home: the rail, at s + hiltT out of the mouth (where you hold the hilt)
        //  - right, katana drawn: the drawn katana's hilt (sp.kat*, steered by your controller), at hiltT from the guard
        //  - left on the sheath: the rail, sp.sheathG from the mouth (your captured grip, else sword_sheath_hand_cm)
        //  - left on the drawn hilt (two hands): the drawn katana's hilt, sp.twoGap further toward the pommel than the right hand
        float Am[3] = {0, 0, 0}, P0[3] = {0, 0, 0}, ww = 0.0f, rotOnlyAx = 0.0f;   // rotOnlyAx: 0 = place on the line; -1 / +1 = only turn, with that sign of index -> little along it
        if (spk >= 0) {
            const bool drawnLine = sp.katOk && (side == 1 ? sp.dSnapW > 0.0f : sp.twoW > 0.0f);
            if (drawnLine) { float kt[3]; toModelD(sp.katAx, Am); toModelP(sp.katT, kt); const float off = (side == 1 ? sp.hiltT : sp.hiltT + sp.twoGap) * u;
                if (fbik::norm3(Am)) for (int k = 0; k < 3; k++) P0[k] = kt[k] + Am[k] * off; ww = side == 1 ? sp.dSnapW : sp.twoW;
                // 0.18.61 (owner: "the left hand does not stay locked with the hilt like the right hand when dual wielding, especially when
                // moving"): the katana is drawn where Jin's RIGHT fist actually ended up this pose (swordApply carries it by the solved fist),
                // but the left hand aimed at the controller's katana from the game thread - a tick older, so it slid off while you moved.
                // The left hand now takes the same correction the katana takes: turned and moved from the asked right fist to the solved one.
                if (side == 0 && drawnTgtOk && fistMOk[1]) { float Da[3] = {fistDm[1][0], fistDm[1][1], fistDm[1][2]}; if (fbik::dot3(Da, drawnTgtA) < 0) for (auto &x : Da) x = -x;
                    const fbik::Rot Rf = fbik::arc(drawnTgtA, Da); const float d[3] = {P0[0] - drawnTgtP[0], P0[1] - drawnTgtP[1], P0[2] - drawnTgtP[2]}; float e[3], a2[3]; fbik::rotv(Rf, d, e); fbik::rotv(Rf, Am, a2);
                    for (int k = 0; k < 3; k++) { P0[k] = fistGm[1][k] + e[k]; Am[k] = a2[k]; } nLeftOnFist++; } }
            else if (sp.snapW[side] > 0.0f && side == 0 && !sp.twoHand && sp.capNoSnap[0]) {   // 0.18.36 the captured left grip: kept where you put it, only turned
                // (owner: "the left hand is in the right place but inverted - the thumb should go up, toward the hilt"): the fist's channel is turned
                // onto the sheath's line with the index on the hilt side (index -> little pointing away from the hilt) about its own centre
                toModelD(sp.axisW, Am); if (fbik::norm3(Am)) { rotOnlyAx = -1.0f; ww = sp.snapW[0]; } }
            else if (sp.snapW[side] > 0.0f && !(side == 0 && sp.twoHand) && !sp.capNoSnap[side]) {   /* 0.18.35: a captured grip is shown as captured (no snap) */ float mo[3]; toModelD(sp.axisW, Am); toModelP(sp.mouthW, mo); const float off = (side == 1 ? sp.hiltAx : sp.sheathG) * u;
                if (fbik::norm3(Am)) for (int k = 0; k < 3; k++) P0[k] = mo[k] + Am[k] * off; ww = sp.snapW[side]; }
            ww *= sp.holdW[side] > 0.3f ? 1.0f : sp.holdW[side] / 0.3f;
        }
        if (ww > 0.0f && fistLocOk[side] && A.frame && fbik::len3f(Am) > 0.5f) {
            const fbik::Rot Rh = fbik::frameTo(A.fl, A.tl, T.fing, T.thumb); float G[3], D[3]; fbik::rotv(Rh, fistGl[side], G); for (int k = 0; k < 3; k++) G[k] += T.pos[k]; fbik::rotv(Rh, fistDl[side], D);
            const float sg = rotOnlyAx != 0.0f ? rotOnlyAx : fbik::dot3(D, Am) >= 0.0f ? 1.0f : -1.0f; const float tD[3] = {Am[0] * sg, Am[1] * sg, Am[2] * sg}; const fbik::Rot Rc = fbik::arc(D, tD);
            const fbik::Rot Rw = fbik::scaled(Rc, ww);
            if (rotOnlyAx != 0.0f) memcpy(P0, G, 12);   // turn about the fist's centre, keep it where it is
            if (side == 1 && spk >= 0 && sp.katOk && sp.dSnapW > 0.0f) { memcpy(drawnTgtP, P0, 12); memcpy(drawnTgtA, tD, 12); drawnTgtOk = true; }
            float rel[3] = {T.pos[0] - G[0], T.pos[1] - G[1], T.pos[2] - G[2]}, rr[3]; fbik::rotv(Rc, rel, rr);
            for (int k = 0; k < 3; k++) T.pos[k] += (P0[k] + rr[k] - T.pos[k]) * ww;
            float f2[3], t2[3]; fbik::rotv(Rw, T.fing, f2); fbik::rotv(Rw, T.thumb, t2); memcpy(T.fing, f2, 12); memcpy(T.thumb, t2, 12); nSnapped[side]++;
        }
        { const float *Sh = fbik::P(pose, A.sh); const float d[3] = {T.pos[0] - Sh[0], T.pos[1] - Sh[1], T.pos[2] - Sh[2]}; armExt[side] = fbik::len3f(d) / fmaxf(1e-3f, A.upper + A.lower); }
        fbik::solveArm(pose, A, side, T, w, prm); any = true;
        if (fingState.load() == 1 && g_fp.fingers) {   // 0.18.18: the fingers, detached from the animation
            fing::Curl cu; cu.grip = ap.grip[side]; cu.thumb = ap.thumbC[side]; cu.index = g_fp.indexTrigger ? ap.trig[side] : ap.grip[side];   // 0.18.20: index = the trigger, thumb = the thumb touch
            float pm[3]; toModelD(ap.palm[side], pm); fing::Tune tn; tn.sign = g_fp.fingerSign; tn.relaxed = g_fp.fingerRelaxed;
            holdBlend(side, cu, tn);   // 0.18.28 on the hilt / the sheath
            fing::apply(pose, fingD[side], cu, pm, w, tn); nFingPosed++;
            {   // 0.18.30 measure the fist (model + hand-local) for the next solve and the katana in the hand
                float G[3], D[3], rad = 0; fistMOk[side] = sp.holdW[side] > 0.5f && measureFist(pose, side, G, D, &rad);
                if (fistMOk[side]) { memcpy(fistGm[side], G, 12); memcpy(fistDm[side], D, 12); fistRadCm[side] = rad / (g_cfg.unitsPerM * 0.01f);
                    fing::Aff Mh = fing::fromPose(pose + (size_t)A.hand * 16), Mi; if (fing::inv(Mh, Mi)) {
                        for (int k = 0; k < 3; k++) { fistGl[side][k] = Mi.R[k * 3] * G[0] + Mi.R[k * 3 + 1] * G[1] + Mi.R[k * 3 + 2] * G[2] + Mi.t[k]; fistDl[side][k] = Mi.R[k * 3] * D[0] + Mi.R[k * 3 + 1] * D[1] + Mi.R[k * 3 + 2] * D[2]; }
                        const int tc = thumbChain(side); if (tc >= 0) { const fing::Chain &C = fingD[side].f[tc]; const float *pv = fbik::P(pose, C.j[C.first]); float tp[3]; thumbTip(pose, C, tp);
                            const float dl[3] = {tp[0] - pv[0], tp[1] - pv[1], tp[2] - pv[2]}; thLen[side] = fbik::len3f(dl);
                            for (int k = 0; k < 3; k++) thBl[side][k] = Mi.R[k * 3] * pv[0] + Mi.R[k * 3 + 1] * pv[1] + Mi.R[k * 3 + 2] * pv[2] + Mi.t[k]; }
                        fistLocOk[side] = true; } }
            }
            if (side == 0 && spk >= 0 && sp.snapW[0] > 0.0f && sp.holdW[0] > 0.0f && !sp.twoHand) {   // 0.18.30 the left thumb laid along the sheath, its tip at the guard
                const int tc = thumbChain(0); float Am[3], mouthM[3]; toModelD(sp.axisW, Am); toModelP(sp.mouthW, mouthM);
                if (tc >= 0 && fbik::norm3(Am)) { const fing::Chain &C = fingD[0].f[tc]; const float *pv0 = fbik::P(pose, C.j[C.first]); const float pv[3] = {pv0[0], pv0[1], pv0[2]};
                    const float u = g_cfg.unitsPerM * 0.01f; const float dm[3] = {pv[0] - mouthM[0], pv[1] - mouthM[1], pv[2] - mouthM[2]}; const float pa = fbik::dot3(dm, Am);
                    float rn[3] = {dm[0] - Am[0] * pa, dm[1] - Am[1] * pa, dm[2] - Am[2] * pa};
                    if (sp.capNoSnap[0]) { float tp[3]; thumbTip(pose, C, tp); const float d0[3] = {tp[0] - pv[0], tp[1] - pv[1], tp[2] - pv[2]};   // 0.18.36 captured: the thumb straight up the sheath, toward the hilt
                        fbik::moveSet(pose, C.sub[C.first], C.nSub[C.first], fbik::scaled(fbik::arc(d0, Am), sp.snapW[0] * sp.holdW[0]), pv); nThumbAimed++; }
                    else if (fbik::norm3(rn)) { float tp[3]; thumbTip(pose, C, tp); const float d0[3] = {tp[0] - pv[0], tp[1] - pv[1], tp[2] - pv[2]};
                        const float Pt[3] = {mouthM[0] - Am[0] * sp.thumbGap * u + rn[0] * sp.sheathR * u, mouthM[1] - Am[1] * sp.thumbGap * u + rn[1] * sp.sheathR * u, mouthM[2] - Am[2] * sp.thumbGap * u + rn[2] * sp.sheathR * u};
                        const float d1[3] = {Pt[0] - pv[0], Pt[1] - pv[1], Pt[2] - pv[2]};
                        fbik::moveSet(pose, C.sub[C.first], C.nSub[C.first], fbik::scaled(fbik::arc(d0, d1), sp.snapW[0] * sp.holdW[0]), pv); nThumbAimed++; } }
            }
        }
    }
    if (any) nArmSolves++;
    return any;
}
// the joint WORLD matrices of everything the mod moved, from the final render pose (held items follow the hands; world = pose x node)
// - except the head set, whose world entries keep the animated head (the eye anchor reads them; the stand-tall raise is added there)
static void worldFromPose(u8 *nd, const float *pose, int cnt, const HidePub &hp) {
    float *W = *(float **)(nd + 0x278); if (!W) return; const float *N = (const float *)(nd + 0x150);
    static uint8_t inHead[JOINT_CAP]; memset(inHead, 0, (size_t)(cnt < JOINT_CAP ? cnt : JOINT_CAP)); for (int i = 0; i < hp.n; i++) if (hp.idx[i] < JOINT_CAP && hp.idx[i] != hp.target) inHead[hp.idx[i]] = 1;
    for (int j = 0; j < cnt && j < JOINT_CAP; j++) { if (inHead[j]) continue; const float *M = pose + (size_t)j * 16; float *o = W + (size_t)(j + 1) * 16;
        for (int r = 0; r < 4; r++) { const float *v = M + 4 * r; for (int c = 0; c < 3; c++) o[4 * r + c] = v[0] * N[c] + v[1] * N[4 + c] + v[2] * N[8 + c] + (r == 3 ? N[12 + c] : 0.0f); } }
}
static void hideInPose(u8 *nd, float *pose) {
    const int k = hpubIdx.load(std::memory_order_acquire); if (k < 0) return;
    const HidePub &p = hpub[k]; if (p.node != nd || p.target < 0) return;
    if ((u32)(frameCtr() - hideFrame.load(std::memory_order_relaxed)) > 2u) return;   // first person not active this frame
    int cnt = nJ;
    for (int b = 0; b < 2; b++) if (*(float **)(nd + 0x508 + b * 0x18) == pose) { const int c = *(int *)(nd + 0x510 + b * 0x18); if (c > 0 && c < cnt) cnt = c; }
    if (p.target >= cnt || p.head >= cnt) { nJobsNoBuf++; return; }
    float *T = pose + (size_t)p.target * 16, *H = pose + (size_t)p.head * 16;
    const bool copied = len3(H) < 0.01f; const float H0[3] = {H[12], H[13], H[14]};
    if (copied) nSeenCollapsed++; else nSeenRestored++;   // already hidden on entry = the job copied the previous pose
    const float P[3] = {T[12], T[13], T[14]};
    int done = 0;
    for (int i = 0; i < p.n; i++) {
        const int j = p.idx[i]; if (j >= cnt) continue; float *m = pose + (size_t)j * 16;
        if (len3(m) >= 0.01f) {   // fresh from the animation: a head joint stays within 45 cm of the neck (a hand does not)
            const float d[3] = {m[12] - P[0], m[13] - P[1], m[14] - P[2]};
            if (j != p.head && len3(d) > 45.0f) { if (j < JOINT_CAP) strayFlag[j].store(1, std::memory_order_relaxed); continue; }
        }
        for (int c = 0; c < 12; c++) m[c] = 0.0f;
        m[12] = P[0]; m[13] = P[1]; m[14] = P[2]; done++;
    }
    nJobsHidden++; nJointsHidden += (u64)done; nCollapse++;
    {   // 0.18.5 body follow: turn the whole pose about the head's vertical (a copied pose already carries the last turn)
        // 0.18.15 (owner: "I need to look far left for the body to rotate, but only a little to the right - Jin is turned slightly ajar to
        // the right, the right shoulder too far back"): the game's standing stance is bladed ~32 deg to the right (IDLE/s), and the body
        // follow centred its dead zone on the NODE's facing, not on the hips you see. Standing, the hip line's yaw (fresh pose, model
        // space, after the idle hold - a held pose is already square) is smoothed and turned out with the body, so the hips face where
        // the body follow says; it fades out above 0.3 m/s (walking hips swing) and is off while riding.
        static float hipS = 0.0f, hipCorr = 0.0f; static u64 hipMs = 0;
        if (!copied) {
            const u64 nowh = GetTickCount64(); const float dth = hipMs ? fminf((float)(nowh - hipMs) * 0.001f, 0.1f) : 0.0f; hipMs = nowh;
            float raw = 0.0f; bool ok = false;
            if (g_fp.stanceSquare && legsReady.load() && !ridingNow.load()) { const int a0 = legsD[0].hip, b0 = legsD[1].hip;
                if (a0 >= 0 && b0 >= 0 && a0 < cnt && b0 < cnt) { const float lx = pose[(size_t)a0 * 16 + 12] - pose[(size_t)b0 * 16 + 12], ly = pose[(size_t)a0 * 16 + 13] - pose[(size_t)b0 * 16 + 13];
                    if (lx * lx + ly * ly > 1e-4f) { raw = atan2f(ly, lx) - 1.5707963f; while (raw > 3.14159265f) raw -= 6.2831853f; while (raw < -3.14159265f) raw += 6.2831853f; ok = fabsf(raw) < 0.8f; } } }
            if (!ok) raw = 0.0f;
            hipS += (raw - hipS) * (1.0f - expf(-dth / 0.15f));
            const float sp = speedMs.load(); const float ws = sp < 0.3f ? 1.0f : sp > 0.6f ? 0.0f : (0.6f - sp) / 0.3f;
            hipCorr = -hipS * ws; hipYawDeg = hipS * 57.2957795f;
        }
        const float a = bodyYaw.load(std::memory_order_relaxed) + hipCorr; const float s = g_cfg.unitsPerM * 0.01f;
        static float shApplied[2] = {0, 0}; float sh[2] = {0, 0};
        if (g_fp.bodyUnder && !ridingNow.load()) {
            const float *N = (const float *)(nd + 0x150); const float w0 = underW[0].load(), w1 = underW[1].load();
            const float n00 = N[0] * N[0] + N[1] * N[1] + N[2] * N[2], n11 = N[4] * N[4] + N[5] * N[5] + N[6] * N[6];
            if (n00 > 1e-6f && n11 > 1e-6f) { sh[0] = (w0 * N[0] + w1 * N[1]) / n00; sh[1] = (w0 * N[4] + w1 * N[5]) / n11;
                const float l = sqrtf(sh[0] * sh[0] + sh[1] * sh[1]), mx = g_fp.underMaxCm * s; if (l > mx && l > 1e-4f) { sh[0] *= mx / l; sh[1] *= mx / l; } }
            // 0.18.31 (owner: "sometimes Jin's body would randomly move behind me to the right"; the log: your head 57 cm from Jin's while
            // reaching for the sword, standing): leaning to the sword moved the whole body (and the sheath with it) under your head. While
            // a hand is near / on the katana the body stays where it is (your hips do not follow a lean either); it follows again after.
            static float held[2] = {0, 0}; if (underHold.load()) { sh[0] = held[0]; sh[1] = held[1]; nUnderHeld++; } else { held[0] = sh[0]; held[1] = sh[1]; }
            {   // where the offset points, in the drawn body's frame (forward / left of Jin as drawn), for the log
                const float b = bodyYaw.load(std::memory_order_relaxed), c = cosf(b), sn = sinf(b); underFwdCm = (c * sh[0] + sn * sh[1]) / s; underLeftCm = (-sn * sh[0] + c * sh[1]) / s; }
        }
        {   // 0.18.17 (owner: "Jin's body should smoothly move to follow the headset, rather than immediately centre below it"): low-pass
            // (fp_body_under_follow_s 0.5); the filter steps on fresh poses only (copied poses reuse the last value)
            static float shF[2] = {0, 0}; static u64 shMs = 0;
            if (!copied) { const u64 nw = GetTickCount64(); const float dts = shMs ? fminf((float)(nw - shMs) * 0.001f, 0.1f) : 0.0f; shMs = nw;
                const float k = g_fp.underTau > 0.0f ? 1.0f - expf(-dts / g_fp.underTau) : 1.0f; shF[0] += (sh[0] - shF[0]) * k; shF[1] += (sh[1] - shF[1]) * k; }
            { static float lastF[2] = {0, 0}; static u64 jMs = 0; const u64 nw2 = GetTickCount64(); if (nw2 - jMs >= 1000) { const float d[2] = {shF[0] - lastF[0], shF[1] - lastF[1]}; const float jl = sqrtf(d[0] * d[0] + d[1] * d[1]) / s; if (jl > underJumpCm.load()) underJumpCm = jl; lastF[0] = shF[0]; lastF[1] = shF[1]; jMs = nw2; } }
            sh[0] = shF[0]; sh[1] = shF[1]; underCm = sqrtf(sh[0] * sh[0] + sh[1] * sh[1]) / s;
        }
        {   // 0.18.25 (owner: "when I move and Jin's body moves too, the camera kind of rotates around the body"; "the headset follows the
            // body slowly"): the eyes sat on Jin's animated head offset from his root, smoothed for 1 s in WORLD axes. When Jin walks
            // somewhere (the room-scale drive, a sidestep) the game turns his node to the walking direction, his head offset (5-15 cm in
            // front of the root) swings round with it and the view slowly orbited after it. Now the head offset is smoothed in Jin's OWN
            // axes (a node turn does not change it) and placed along the DRAWN body's heading (the one facing your headset); the drawn
            // body is moved by the difference so its head is exactly there (body and view rigid to each other, as animated).
            static float T[2] = {0, 0}; static u64 hzMs = 0;
            if (!copied) {
                const u64 nw = GetTickCount64(); const float dz2 = hzMs ? fminf((float)(nw - hzMs) * 0.001f, 0.1f) : 0.0f; hzMs = nw;
                if (!hzOk.load() || !std::isfinite(hzS[0])) { hzS[0] = H0[0]; hzS[1] = H0[1]; }
                else { const float k = g_fp.horizTau > 0.0f ? 1.0f - expf(-dz2 / g_fp.horizTau) : 1.0f; hzS[0] += (H0[0] - hzS[0]) * k; hzS[1] += (H0[1] - hzS[1]) * k; }
                hzOut[0] = hzS[0]; hzOut[1] = hzS[1]; hzOk = 1;
                T[0] = T[1] = 0.0f;
                if (!ridingNow.load() && g_fp.headRigid) { const float b = bodyYaw.load(std::memory_order_relaxed), c = cosf(b), sn = sinf(b);
                    T[0] = (c * hzS[0] - sn * hzS[1]) - H0[0]; T[1] = (sn * hzS[0] + c * hzS[1]) - H0[1];
                    const float l = sqrtf(T[0] * T[0] + T[1] * T[1]), mx = 70.0f * s; if (!std::isfinite(l)) { T[0] = T[1] = 0.0f; } else if (l > mx) { T[0] *= mx / l; T[1] *= mx / l; nTClamped++; } }   // 0.18.37 (owner: "moving back / right / left, Jin's body keeps teleporting behind me"): 0.18.25 dropped T to 0 past 40 cm - running backward the body turned 180 deg puts the run lean (head ~20 cm ahead of the root) on the other side: T ~ 40 cm, flipping between T and 0 every frame. Now clamped smoothly at 70 cm
                hzT[0] = T[0]; hzT[1] = T[1];
            }
            sh[0] += T[0]; sh[1] += T[1];
        }
        auto shiftPose = [&](float dx, float dy) { if (dx == 0.0f && dy == 0.0f) return; for (int j = 0; j < cnt; j++) { pose[(size_t)j * 16 + 12] += dx; pose[(size_t)j * 16 + 13] += dy; } };
        if (!copied) {
            jobApplied = 0;
            if (a != 0.0f) {
                if (!(H0[2] > 60.0f * s) || sqrtf(H0[0] * H0[0] + H0[1] * H0[1]) > 80.0f * s) nBodyRefused++;   // not an upright model space: fail closed
                else { turnPose(pose, cnt, a, H0[0], H0[1]); jobApplied = a; jobPivot[0] = H0[0]; jobPivot[1] = H0[1]; nBodyJobs++;
                       if (g_fp.bodyAttach) turnWorld(nd, a, p.head); }
            }
        } else if (a != jobApplied && jobApplied != 0.0f) { turnPose(pose, cnt, a - jobApplied, jobPivot[0], jobPivot[1]); jobApplied = a; nBodyJobs++; }
        if (!copied) { shiftPose(sh[0], sh[1]); shApplied[0] = sh[0]; shApplied[1] = sh[1]; }
        else { shiftPose(sh[0] - shApplied[0], sh[1] - shApplied[1]); shApplied[0] = sh[0]; shApplied[1] = sh[1]; }
        underMoved = sh[0] != 0.0f || sh[1] != 0.0f;
    }
    if (!copied) {   // 0.18.7 / 0.18.9 FBIK: stand tall (legs), arm ownership gate, VR arms, then the world matrices of what moved
        const u64 now = GetTickCount64(); static u64 lastG = 0; const float dtg = lastG ? fminf((float)(now - lastG) * 0.001f, 0.1f) : 0.0f; lastG = now;
        { static float gt = 0; const bool own = now < animUntil.load() || now < raisedUntil.load(); gt = fminf(1.0f, fmaxf(0.0f, gt + (own ? dtg : -dtg) / 0.3f)); gateW = 0.5f - 0.5f * cosf(gt * 3.14159265f); }
        bool moved = jobApplied != 0.0f;
        const float dz = standDz.load();
        if (legsReady.load() && dz > 0.05f && g_fp.standTall) { const float ap = fbik::standTall(pose, cnt, legsD, dz, jobApplied); standApplied = ap; if (ap > 0.0f) { moved = true; nStand++; } } else standApplied = 0.0f;
        moved |= solveArms(nd, pose, cnt, jobApplied);
        moved |= swordApply(nd, pose, cnt);   // 0.18.26 the katana draw
        moved |= idleMoved.load() != 0;   // 0.18.13: the held idle pose (carried items follow it)
        moved |= maskMoved.load() != 0;   // 0.18.39: the game's draw / sheathe masked
        moved |= underMoved;               // 0.18.16: the body moved under the headset
        if (moved) worldFromPose(nd, pose, cnt, p);
    }
}
// ---------------- 0.18.13 idle hold (owner: "we need to stop Jin's idle animations during first person") ----------------
// Standing still, the game plays idle fidgets (weight shifts, looking around, breathing) - in first person they sway the body you look
// down at and, through the head joint, the view. Playbook ch02: blend weight, playback rate and position are three levers and "freeze
// at pose X" needs all three - we own none of the game's animation graph, so the hold is done on its OUTPUT instead: once Jin has
// stood still for fp_idle_settle_s (0.5 s, before the first fidget starts) the fresh render pose is captured, and while he keeps
// standing still every new pose is replaced by it (model space, so the game can still move / turn the node). Arms are solved on top,
// the head is hidden on top, the body follow turns on top. It lets go (0.2 s blend) the moment anything is not idle: any pad input
// (buttons, triggers, sticks - including the snap-turn catch-up), body speed above 0.3 m/s, the game owning the arms (flute,
// climbing, takedowns), riding, or the animation itself moving the torso / feet faster than 50 cm/s (hit reactions, scripted moves).
static std::atomic<int> idleOn{1}; static float idlePose[JOINT_CAP * 16]; static int idleCnt = 0; static bool idleCap = false;
static float idleW = 0.0f, idleStill = 0.0f; static u64 idleLastMs = 0; static float idlePrev[6][3]; static bool idlePrevOk = false;
static std::atomic<u64> nIdleHeld{0}, nIdleRelease{0}; static std::atomic<int> idleWhy{0};
static std::atomic<float> idleWOut{0};
static u64 (*idleNow)() = GetTickCount64;   // self-test clock
// 0.18.14 (owner: "standing still, Jin is naturally turned maybe 15 degrees to the right (left foot forward) - we want his body to
// face directly forwards"): the game's standing stance is bladed. When the stand pose is taken, the line through the hips and the line
// through the shoulders (left minus right, horizontal) give the stance's yaw in model space (x forward, y left: facing forward = the
// line along +y); the held pose is turned back by it about the head's vertical (so the eyes do not move). fp_stance_square = 0: off.
static std::atomic<float> idleSquareDeg{0};
static void idleSquare(float *pose, int cnt) {
    idleSquareDeg = 0.0f; if (!g_fp.stanceSquare) return;
    const int k = hpubIdx.load(std::memory_order_acquire); if (k < 0) return; const HidePub &hp = hpub[k]; if (hp.head < 0 || hp.head >= cnt) return;
    float lx = 0, ly = 0; int n = 0;
    auto pair = [&](int a, int b) { if (a < 0 || b < 0 || a >= cnt || b >= cnt) return; lx += pose[(size_t)a * 16 + 12] - pose[(size_t)b * 16 + 12]; ly += pose[(size_t)a * 16 + 13] - pose[(size_t)b * 16 + 13]; n++; };
    if (legsReady.load()) pair(legsD[0].hip, legsD[1].hip);
    if (armsReady.load()) pair(armsD[0].sh, armsD[1].sh);
    if (!n || lx * lx + ly * ly < 1e-4f) return;
    const float yaw = atan2f(ly, lx) - 1.5707963f;   // 0 = square to the node's forward; + = turned left
    if (fabsf(yaw) > 0.7f) return;                     // > 40 deg: not a stance (fail closed)
    const float *H = pose + (size_t)hp.head * 16; turnPose(pose, cnt, -yaw, H[12], H[13]); idleSquareDeg = yaw * 57.2957795f;
}
static bool idleApply(u8 *nd, float *pose, int cnt, bool inputQuiet) {   // job thread, a FRESH pose (head not yet hidden); true = the pose changed
    const u64 now = idleNow(); const float dt = idleLastMs ? fminf((float)(now - idleLastMs) * 0.001f, 0.1f) : 0.0f; idleLastMs = now;
    const int k = hpubIdx.load(std::memory_order_acquire); const HidePub *hp = k >= 0 ? &hpub[k] : nullptr;
    const float s = g_cfg.unitsPerM * 0.01f;
    float motion = 0.0f;   // fastest of neck and feet in model space (cm/s): the animation is doing something
    { int js[6]; int nj = 0; if (hp && hp->target >= 0 && hp->target < cnt) js[nj++] = hp->target;
      if (legsReady.load()) for (int sd = 0; sd < 2; sd++) { if (legsD[sd].foot >= 0 && legsD[sd].foot < cnt) js[nj++] = legsD[sd].foot; if (legsD[sd].hip >= 0 && legsD[sd].hip < cnt) js[nj++] = legsD[sd].hip; }
      for (int i = 0; i < nj; i++) { const float *m = pose + (size_t)js[i] * 16;
          if (idlePrevOk && dt > 0.0f) { const float d[3] = {m[12] - idlePrev[i][0], m[13] - idlePrev[i][1], m[14] - idlePrev[i][2]}; motion = fmaxf(motion, len3(d) / dt / s); }
          idlePrev[i][0] = m[12]; idlePrev[i][1] = m[13]; idlePrev[i][2] = m[14]; }
      idlePrevOk = nj > 0; }
    int why = 0;
    if (!idleOn.load() || !g_fp.idleHold) why = 1;
    else if (!inputQuiet) why = 2;
    else if (speedMs.load() > 0.3f) why = 3;
    else if (now < animUntil.load() || now < raisedUntil.load()) why = 4;
    else if (ridingNow.load()) why = 5;
    else if (motion > 50.0f) why = 6;
    else if (cnt <= 0 || cnt > JOINT_CAP) why = 7;
    idleWhy = why;
    if (why) { idleStill = 0.0f; if (idleW > 0.0f) { idleW = fmaxf(0.0f, idleW - dt / 0.2f); if (idleW <= 0.0f) nIdleRelease++; } }
    else idleStill += dt;
    if (!why && !idleCap && idleStill >= g_fp.idleSettle) { memcpy(idlePose, pose, (size_t)cnt * 64); idleCnt = cnt; idleCap = true; idleSquare(idlePose, cnt); }
    if (idleW <= 0.0f && why) idleCap = false;   // fully let go: the next stand captures a fresh pose
    if (!why && idleCap) idleW = fminf(1.0f, idleW + dt / 0.2f);
    idleWOut = idleW;
    if (!idleCap || idleW <= 0.0f || idleCnt != cnt) return false;
    const float w = 0.5f - 0.5f * cosf(idleW * 3.14159265f);
    if (w >= 0.999f) memcpy(pose, idlePose, (size_t)cnt * 64);
    else for (size_t i = 0; i < (size_t)cnt * 16; i++) pose[i] += (idlePose[i] - pose[i]) * w;
    nIdleHeld++;
    {   // the WORLD matrices too (the eye anchor reads the head's): world = pose x node for every joint, head set included
        float *W = *(float **)(nd + 0x278); const float *N = (const float *)(nd + 0x150);
        if (W) for (int j = 0; j < cnt; j++) { const float *M = pose + (size_t)j * 16; float *o = W + (size_t)(j + 1) * 16;
            for (int r = 0; r < 4; r++) { const float *v = M + 4 * r; for (int c = 0; c < 3; c++) o[4 * r + c] = v[0] * N[c] + v[1] * N[4 + c] + v[2] * N[8 + c] + (r == 3 ? N[12 + c] : 0.0f); } }
    }
    return true;
}
// 0.18.25 (owner: "make Jin's head come back when not in first person - cutscenes etc."; log 0.18.24: during FLAT / paused frames - the
// pause menu, the map, the frozen first frame of a cutscene - the hero's animation job does not run (0 jobs/s), so the LAST pose the mod
// wrote (head collapsed, body turned, VR arms) is what the game keeps drawing, headless). Every hero pose the mod is about to change is
// copied first (the job's pose buffer - two alternate - and the joint world matrices); when the game renders flat frames without re-
// animating, the copies are put back, so the frozen frame shows the game's own Jin, head and all.
struct Snap { float *pose = nullptr; int cnt = 0; u8 *nd = nullptr; float *W = nullptr; u64 seq = 0; bool dirty = false; float p[JOINT_CAP * 16]; float w[(JOINT_CAP + 1) * 16]; };
static Snap snap[2]; static u64 snapSeq = 0; static std::atomic<u64> nSnapRestores{0};
// 0.18.39 the game's draw / sheathe animation masked on Jin: while it plays (maskUntil, set by the game thread when it asks) and while the
// game stands in its drawn-sword stance for a draw the mod asked for (standing still - walking keeps the game's legs), every joint but
// the game's sword (637 and below) is replaced by Jin's own pose from before (the last fresh pose with the game's sword parked; standing
// still: the last one standing). The world matrices follow, so the eye anchor (the head) does not move either. Eased 0.15 s in, 0.3 s out.
static float maskRef[JOINT_CAP * 16], maskRefStand[JOINT_CAP * 16]; static int maskRefCnt = 0, maskStandCnt = 0; static float maskW = 0.0f; static u64 maskLastMs = 0; static const float *maskUse = nullptr;
static uint8_t maskExcl[JOINT_CAP]; static int maskExclFor = -2, maskExclCnt = 0;
static bool maskApply(u8 *nd, float *pose, int cnt, bool inputQuiet) {   // job thread, a FRESH pose of active first person, before the idle hold
    if (cnt <= 0 || cnt > JOINT_CAP) return false;
    const u64 now = GetTickCount64(); const float dt = maskLastMs ? fminf((float)(now - maskLastMs) * 0.001f, 0.1f) : 0.0f; maskLastMs = now;
    const int k = spubIdx.load(std::memory_order_acquire); const int gb = k >= 0 ? spub[k].gameBlade : -1;
    const bool gd = gameDrawn.load() != 0, trans = now < maskUntil.load(), still = speedMs.load() < 0.3f;
    if (!gd && !trans && maskW <= 0.0f) { memcpy(maskRef, pose, (size_t)cnt * 64); maskRefCnt = cnt; if (still) { memcpy(maskRefStand, pose, (size_t)cnt * 64); maskStandCnt = cnt; } }
    // the drawn stance standing still is held only while you give no pad input (0.3 s) and the game keeps Jin's head about where it was
    // (a crouch, an aim or anything else that really changes his pose shows through)
    bool stanceOk = maskStance.load() && gd && still && inputQuiet && maskStandCnt == cnt;
    if (stanceOk) { const int kh = hpubIdx.load(std::memory_order_acquire); const int hd = kh >= 0 ? hpub[kh].head : -1;
        if (hd >= 0 && hd < cnt) stanceOk = fabsf(pose[(size_t)hd * 16 + 14] - maskRefStand[(size_t)hd * 16 + 14]) < 20.0f * g_cfg.unitsPerM * 0.01f; }
    const bool want = g_fp.on && gb >= 0 && (trans || stanceOk);
    if (want && maskW <= 0.0f) maskUse = (still && maskStandCnt == cnt) ? maskRefStand : maskRefCnt == cnt ? maskRef : nullptr;   // the pose to hold, chosen as it starts
    maskW = fminf(1.0f, fmaxf(0.0f, maskW + (want ? dt / 0.15f : -dt / 0.3f))); maskWOut = maskW;
    if (maskW <= 0.0f || !maskUse) return false;
    if (maskExclFor != gb || maskExclCnt != cnt) { memset(maskExcl, 0, sizeof maskExcl); maskExclFor = gb; maskExclCnt = cnt; if (gb >= 0 && gb < cnt && parState.load() > 0) { static uint16_t sb[512]; const int n = fbik::subtree(parents, cnt, gb, sb, 512); for (int i = 0; i < n; i++) if (sb[i] < JOINT_CAP) maskExcl[sb[i]] = 1; } }
    const float w = 0.5f - 0.5f * cosf(maskW * 3.14159265f);
    static uint8_t legEx[JOINT_CAP]; static int legFor = -1; if (legFor != cnt && legsReady.load() && parState.load() > 0) { legFor = cnt; memset(legEx, 0, sizeof legEx); for (int sd = 0; sd < 2; sd++) { static uint16_t lb[512]; const int h = legsD[sd].hip; if (h < 0 || h >= cnt) continue; const int n = fbik::subtree(parents, cnt, h, lb, 512); for (int i = 0; i < n; i++) if (lb[i] < JOINT_CAP) legEx[lb[i]] = 1; } }
    const bool legsFree = !still && legFor == cnt;   // 0.18.41 walking: the game's legs keep walking, the upper body is held
    for (int jj = 0; jj < cnt; jj++) { if (maskExcl[jj] || (legsFree && legEx[jj])) continue; float *m = pose + (size_t)jj * 16; const float *r = maskUse + (size_t)jj * 16; if (w >= 0.999f) memcpy(m, r, 64); else for (int i = 0; i < 16; i++) m[i] += (r[i] - m[i]) * w; }
    nMasked++;
    { float *W = *(float **)(nd + 0x278); const float *N = (const float *)(nd + 0x150);
      if (W) for (int jj = 0; jj < cnt; jj++) { const float *M = pose + (size_t)jj * 16; float *o = W + (size_t)(jj + 1) * 16;
          for (int r = 0; r < 4; r++) { const float *v = M + 4 * r; for (int c = 0; c < 3; c++) o[4 * r + c] = v[0] * N[c] + v[1] * N[4 + c] + v[2] * N[8 + c] + (r == 3 ? N[12 + c] : 0.0f); } } }
    return true;
}
static void snapTake(u8 *nd, float *pose) {
    const bool willChange = (u32)(frameCtr() - hideFrame.load(std::memory_order_relaxed)) <= 2u;
    int sl = pose == snap[0].pose ? 0 : pose == snap[1].pose ? 1 : -1;
    if (!willChange) { if (sl >= 0) snap[sl].dirty = false; return; }   // a fresh pose the mod leaves alone
    if (sl < 0) sl = snap[0].seq <= snap[1].seq ? 0 : 1;
    int cnt = nJ; for (int b = 0; b < 2; b++) if (*(float **)(nd + 0x508 + b * 0x18) == pose) { const int c = *(int *)(nd + 0x510 + b * 0x18); if (c > 0 && c < cnt) cnt = c; }
    if (cnt <= 0 || cnt >= JOINT_CAP) return;
    Snap &S = snap[sl]; S.pose = pose; S.cnt = cnt; S.nd = nd; memcpy(S.p, pose, (size_t)cnt * 64);
    float *W = *(float **)(nd + 0x278); S.W = W; if (W) memcpy(S.w, W, (size_t)(cnt + 1) * 64);
    S.seq = ++snapSeq; S.dirty = true;
}
static void snapRestore() {   // game thread, while the game renders frozen / flat frames (the hero's job is not running)
    int last = -1; for (int i = 0; i < 2; i++) if (snap[i].dirty && (last < 0 || snap[i].seq > snap[last].seq)) last = i;
    if (last < 0) return;
    for (int i = 0; i < 2; i++) { Snap &S = snap[i]; if (!S.dirty || !S.pose || S.nd != snap[last].nd) { S.dirty = false; continue; }
        if (readable(S.pose, (size_t)S.cnt * 64)) memcpy(S.pose, S.p, (size_t)S.cnt * 64); S.dirty = false; }
    const Snap &L = snap[last]; if (L.W && L.W == *(float **)(L.nd + 0x278) && readable(L.W, (size_t)(L.cnt + 1) * 64)) memcpy(L.W, L.w, (size_t)(L.cnt + 1) * 64);
    nSnapRestores++; logf("first person: the game froze on a frame the mod had changed (pause / map / cutscene start) -> Jin's own pose put back (head shown)");
}
static void hAnimJob(u8 *job) {
    oAnimJob(job);
    if (!job) return;
    u8 *nd = *(u8 **)(job + 0xE8); if (!nd) return;
    const int k = hpubIdx.load(std::memory_order_acquire); if (k < 0 || hpub[k].node != nd) return;
    nHeroJobs++;
    float *pose = *(float **)(job + 0xA8); if (!pose) { nJobsNoBuf++; return; }
    snapTake(nd, pose);   // 0.18.25: the game's own pose / world matrices, before the mod changes them (put back when the game freezes)
    g_animPose = nullptr; g_animCnt = 0; for (int q = 0; q < 2; q++) if (snap[q].pose == pose && snap[q].cnt > 0) { g_animPose = snap[q].p; g_animCnt = snap[q].cnt; }   // 0.18.34
    {   // 0.18.39 the game's sword state (from its own pose) and the draw / sheathe animation masked - before anything reads the pose
        const int k3 = hpubIdx.load(std::memory_order_acquire);
        if (k3 >= 0 && hpub[k3].head >= 0 && hpub[k3].head < nJ && (u32)(frameCtr() - hideFrame.load(std::memory_order_relaxed)) <= 2u && len3(pose + (size_t)hpub[k3].head * 16) >= 0.01f) {
            int cnt = nJ; for (int b = 0; b < 2; b++) if (*(float **)(nd + 0x508 + b * 0x18) == pose) { const int c = *(int *)(nd + 0x510 + b * 0x18); if (c > 0 && c < cnt) cnt = c; }
            gameSwordScan(pose, cnt); maskMoved = maskApply(nd, pose, cnt, vrin::nowMs() - vrin::lastAct.load() > 300) ? 1 : 0;
        } else maskMoved = 0;
    }
    if (g_fp.arms && !armsReady.load() && parState.load() >= 0) { int cnt = nJ; for (int b = 0; b < 2; b++) if (*(float **)(nd + 0x508 + b * 0x18) == pose) { const int c = *(int *)(nd + 0x510 + b * 0x18); if (c > 0 && c < cnt) cnt = c; } analyseArms(job, nd, pose, cnt); }
    if (armsReady.load()) {   // 0.18.9: the ANIMATION lifts a hand to head height (climbing, ledges, grappling, the flute) -> the game owns the arms
        const int k2 = hpubIdx.load(std::memory_order_acquire); if (k2 >= 0 && hpub[k2].head >= 0 && hpub[k2].head < nJ) {
            const float hz = pose[(size_t)hpub[k2].head * 16 + 14], s = g_cfg.unitsPerM * 0.01f;
            // 0.18.19: a jump swings the arms up - not a climb. 0.18.23: ... and so does the landing: the window lasts while Jin is in the air
            // after a jump press (up to 4 s, a long fall) and 0.6 s past touching down
            const u64 tj = GetTickCount64(), sinceJ = tj - vrin::jumpAt.load(), sinceAir = tj - airAt.load();
            const bool jumping = sinceJ < 1200 || (sinceJ < 4000 && sinceAir < 600);
            if (jumping && sinceJ >= 1200) nJumpHeld++;
            bool swordHeld = false; { const int ks = spubIdx.load(std::memory_order_acquire); if (ks >= 0) swordHeld = spub[ks].state != sword::S_SHEATHED || spub[ks].onHilt; }   // 0.18.37: the game's draw / sheathe raises the animated hands - the katana in your hands keeps the VR arms
            // 0.18.62 (owner: "when moving across logs, ropes, keep VR arms"; "when grappling, the VR arms went away (good) but then came back
            // momentarily"): balancing holds the arms out at shoulder height, which counted as raised (head - 15 cm); now only a hand ABOVE the
            // head (arms_game_above_head_cm, 0) gives the arms to the game - a climb, a ledge, the grapple throw - and it keeps them for
            // arms_game_hold_ms (1200) after the hand last was up, so a grapple swing that dips the hands does not flick the VR arms back
            // 0.18.65 (owner, session 20: "stop taking the character's arms away when the hands go above the head, like during grappling,
            // and also on ropes and tree logs"; then: "I don't want VR hands during climbing and cutscenes, and also takedowns - that's the only
            // time"): arms_game_when_raised 2 (default) = CLIMBING only - a hand above the head while Jin moves slowly (under 1.2 m/s for
            // 0.3 s: a wall, a ledge; a grapple swing is fast) gives the arms to the game; 1 = any raised hand (0.18.62), 0 = never.
            // Cutscenes (first person off) and takedowns / assassinations (the attack button's hold) keep the game's arms as before.
            static float slowFor = 0.0f; static u64 slowMs = 0; { const u64 tn = GetTickCount64(); const float dts = slowMs ? fminf((tn - slowMs) * 0.001f, 0.1f) : 0.0f; slowMs = tn; slowFor = speedMs.load() < 1.2f ? slowFor + dts : 0.0f; }
            const bool raiseGate = g_fp.armsRaised == 1 || (g_fp.armsRaised == 2 && slowFor >= 0.3f);
            for (int sd = 0; sd < 2 && !jumping && !swordHeld && raiseGate; sd++) { const int h = armsD[sd].hand; if (h >= 0 && h < nJ && pose[(size_t)h * 16 + 14] > hz + g_fp.armsAboveHeadCm * s) { raisedUntil = GetTickCount64() + (u64)g_fp.armsHoldMs; nRaised++; break; } } } }
    {   // 0.18.13 idle hold: only a fresh pose of an active first person (a copied pose already carries it)
        const int k3 = hpubIdx.load(std::memory_order_acquire);
        if (k3 >= 0 && hpub[k3].head >= 0 && hpub[k3].head < nJ && (u32)(frameCtr() - hideFrame.load(std::memory_order_relaxed)) <= 2u && len3(pose + (size_t)hpub[k3].head * 16) >= 0.01f) {
            int cnt = nJ; for (int b = 0; b < 2; b++) if (*(float **)(nd + 0x508 + b * 0x18) == pose) { const int c = *(int *)(nd + 0x510 + b * 0x18); if (c > 0 && c < cnt) cnt = c; }
            idleMoved = idleApply(nd, pose, cnt, vrin::nowMs() - vrin::lastAct.load() > 300) ? 1 : 0;
        } else idleMoved = 0;
    }
    hideInPose(nd, pose);
    {   // 0.18.40 remember what this pose buffer's game sword (637) holds as the mod leaves it - if the game does not animate it next time, it is put away
        const int ks = spubIdx.load(std::memory_order_acquire); const int gb = ks >= 0 ? spub[ks].gameBlade : -1;
        if (gb >= 0 && gb < nJ) { static int nq = 1; const int q = wroteBuf[0] == pose ? 0 : wroteBuf[1] == pose ? 1 : (nq ^= 1);
            memcpy(wroteM[q], pose + (size_t)gb * 16, 64); wroteBuf[q] = pose; wroteGb = gb; }
    }
}
static void collapse() {}   // 0.17.2's world-array collapse is gone (the GPU never sees that array)
// BUG-70 (0.17.4: "the stutter only happens in the LEFT eye"): the anchor was computed once per tick at its FIRST render-camera
// update. On the simulated (left) tick that is the game's camera controller (SetLocal -> slot 43), before this tick's animation
// moved the hero, so the left eye sat on the previous step's body while the frozen (right) tick computed it from the new one:
// the two eyes of one pair had different eye anchors whenever the body or head animation moved. Now the left tick recomputes the
// anchor at every render-camera update (the last one before the frame snapshot sees the animated hero; the low-pass filters
// advance once per tick from the previous tick's state, so repeating is idempotent) and the right tick of the pair REUSES the
// left tick's final anchor - one body pose per pair, like one head-pose sample per pair (playbook ch10: both eyes from one moment).
static float frameAvFast = 1;
static float relUpC = 0, relHC[3] = {0, 0, 0}, latC = 0, latF = 0, frameAv = 1, frameAh = 1; static bool recompute = false; static u32 pairLeftFrame = 0xFFFFFFF0u;
static std::atomic<float> lrDiffMax{0}, headLat{0}; static std::atomic<u64> nRecompute{0}, nPairReuse{0};
static bool anchorFrom(float out[3]) {
    const float *root = (const float *)(node + 0x150), *H = J(headIdx);
    float U[3] = {root[8], root[9], root[10]}; { const float l = len3(U); if (!(l > 1e-4f)) return false; for (auto &x : U) x /= l; }
    float F[3] = {root[0], root[1], root[2]}; { const float d = F[0] * U[0] + F[1] * U[1] + F[2] * U[2]; for (int c = 0; c < 3; c++) F[c] -= d * U[c]; const float l = len3(F); if (l > 1e-4f) for (auto &x : F) x /= l; }
    if (bodyYawCur != 0.0f) {   // 0.18.5: the eyes sit in front of the face as drawn (turned with the body)
        const float L[3] = {U[1] * F[2] - U[2] * F[1], U[2] * F[0] - U[0] * F[2], U[0] * F[1] - U[1] * F[0]}, c = cosf(bodyYawCur), sn = sinf(bodyYawCur);
        for (int k = 0; k < 3; k++) F[k] = F[k] * c + L[k] * sn; }
    const float hp[3] = {H[12], H[13], H[14]};
    if (!std::isfinite(hp[0]) || len3(H) < 0.01f) return false;
    // 0.18.7 (owner: "not perfectly centred at the head, a bit more towards his left shoulder"): the head joint (123, the top of the
    // head) sits 3.3 cm (standing) / 4.0 cm (riding) to Jin's LEFT of the centre of the 480-joint head cluster (both skeleton dumps).
    // Sideways, the eyes now sit on that centre (+ fp_eye_left_cm); height and forward offset still come from the head joint.
    const float s0 = g_cfg.unitsPerM * 0.01f; const float Lw[3] = {U[1] * F[2] - U[2] * F[1], U[2] * F[0] - U[0] * F[2], U[0] * F[1] - U[1] * F[0]};
    float lat = g_fp.eyeLeftCm * s0;
    if (calibrated && nHide > 8) {
        double cs[3] = {0, 0, 0}; int k = 0;
        for (int i = 0; i < nHide; i++) { if (strayed[hideSet[i]]) continue; const float *m = J(hideSet[i]); if (!std::isfinite(m[12])) continue; cs[0] += m[12]; cs[1] += m[13]; cs[2] += m[14]; k++; }
        if (k > 8) { const float d[3] = {(float)(cs[0] / k) - hp[0], (float)(cs[1] / k) - hp[1], (float)(cs[2] / k) - hp[2]};
            float c = d[0] * Lw[0] + d[1] * Lw[1] + d[2] * Lw[2]; const float lim = 8.0f * s0; c = c > lim ? lim : c < -lim ? -lim : c;
            latF = latC + (c - latC) * frameAh; lat += latF; headLat = latF / s0; }   // 0.18.16: smoothed like the head offset (it followed every head twitch)
    }
    const float up = (hp[0] - root[12]) * U[0] + (hp[1] - root[13]) * U[1] + (hp[2] - root[14]) * U[2]; headUp = up;
    float hz[3]; for (int c = 0; c < 3; c++) hz[c] = (hp[c] - root[12 + c]) - up * U[c];
    {   // 0.18.37 (owner: "the camera smoothing when getting up from a crouch is too slow - the body moves before the headset"): the head's
        // height is smoothed (fp_vertical_smoothing_s 0.35) to calm the gait bob; a change bigger than a bob (crouch / stand: > 4 cm off)
        // now follows in 0.06 s, fully past 10 cm
        const float err = fabsf(up - relUpC) / (g_cfg.unitsPerM * 0.01f); const float kf = fminf(1.0f, fmaxf(0.0f, (err - 4.0f) / 6.0f));
        relUpF = relUpC + (up - relUpC) * (frameAv + (frameAvFast - frameAv) * kf); } for (int c = 0; c < 3; c++) relHF[c] = relHC[c] + (hz[c] - relHC[c]) * frameAh;
    const float s = g_cfg.unitsPerM * 0.01f;   // cm -> game units
    const float rz = standApplied.load();   // 0.18.9: the drawn body raised by stand tall - the eyes go with it
    float hw[3] = {relHF[0], relHF[1], relHF[2]};
    if (hzOk.load() && g_fp.headRigid) {   // 0.18.25: the smoothed head offset in Jin's own axes, along the drawn heading (see hideInPose)
        const float sc = sqrtf(root[0] * root[0] + root[1] * root[1] + root[2] * root[2]); const float hx = hzOut[0].load() * sc, hy = hzOut[1].load() * sc;
        if (ridingNow.load()) { float Fn[3] = {root[0], root[1], root[2]}; { const float d = Fn[0] * U[0] + Fn[1] * U[1] + Fn[2] * U[2]; for (int c = 0; c < 3; c++) Fn[c] -= d * U[c]; float l = len3(Fn); if (l > 1e-4f) for (auto &x : Fn) x /= l; }
            const float Ln[3] = {U[1] * Fn[2] - U[2] * Fn[1], U[2] * Fn[0] - U[0] * Fn[2], U[0] * Fn[1] - U[1] * Fn[0]}; for (int c = 0; c < 3; c++) hw[c] = Fn[c] * hx + Ln[c] * hy; }
        else for (int c = 0; c < 3; c++) hw[c] = F[c] * hx + Lw[c] * hy;
    }
    for (int c = 0; c < 3; c++) out[c] = root[12 + c] + U[c] * (relUpF + g_fp.eyeUpCm * s + rz) + hw[c] * g_fp.lean + F[c] * (g_fp.eyeFwdCm * s) + Lw[c] * lat;
    return true;
}
// render-camera update (hSlot43, CAM-A, VR pose present): true = first person; gm translation becomes the eye anchor
static bool update(float *gm) {
    const u32 f = frameCtr();
    if (f == lastFrame) {
        if (activeNow) { if (recompute) { float a[3]; if (anchorFrom(a)) { memcpy(anchor, a, sizeof a); nRecompute++; } } gm[12] = anchor[0]; gm[13] = anchor[1]; gm[14] = anchor[2]; }
        return activeNow;
    }
    const bool wasActive = activeNow; const u32 prevFrame = lastFrame;
    lastFrame = f; activeNow = false; recompute = false;
    if (!on.load() || g_cfg.xr != 4) { nOff++; lastReason = 1; return false; }
    if (const int r = ui::fpBlockReason()) { nBlockedUi++; lastReason = 10 + r; relInit = false; return false; }
    u8 *h = heroPtr();
    if (!h) { nNoHero++; lastReason = 2; valid = false; hero = nullptr; unpublish(); return false; }
    if (h == hero && valid && (*(float **)(node + 0x278) != joints || (countSrc == 1 && *(int *)(node + 0x280) - 1 != nJ))) {
        // another joint buffer or count (outfit / gear change): the indices may mean other joints now -> stop hiding, find the head again
        nJointSwaps++; valid = false; unpublish(); retryAt = 0; logf("first person: the hero's skeleton changed (joint buffer or count) -> head set recalibrated");
    }
    if (h != hero || !valid) {
        const u64 nowR = GetTickCount64(); if (h == hero && !valid && nowR < retryAt) { nNoHero++; lastReason = 3; return false; }
        retryAt = nowR + 3000; if (!validate(h)) { nNoHero++; lastReason = 3; return false; }
        armsReady = 0; armsFiltered = 0; if (parState.load() != -1) parState = 0; parTries = 0; parTryMs = 0; }   // 0.18.7: a new skeleton -> find the arms again
    const float *root = (const float *)(node + 0x150), *H = J(headIdx);
    float U[3] = {root[8], root[9], root[10]}; { const float l = len3(U); if (!(l > 1e-4f)) { lastReason = 4; return false; } for (auto &x : U) x /= l; }
    float F[3] = {root[0], root[1], root[2]}; { const float d = F[0] * U[0] + F[1] * U[1] + F[2] * U[2]; for (int c = 0; c < 3; c++) F[c] -= d * U[c]; const float l = len3(F); if (l > 1e-4f) for (auto &x : F) x /= l; }
    const float hp[3] = {H[12], H[13], H[14]};
    if (!std::isfinite(hp[0]) || len3(H) < 0.01f) {   // a hidden head joint in the world array (not ours any more): keep the last anchor
        if (relInit) { activeNow = true; nActive++; hideFrame = f; gm[12] = anchor[0]; gm[13] = anchor[1]; gm[14] = anchor[2]; return true; }
        lastReason = 5; return false;
    }
    const float up = (hp[0] - root[12]) * U[0] + (hp[1] - root[13]) * U[1] + (hp[2] - root[14]) * U[2]; headUp = up;
    const float cd[3] = {gm[12] - hp[0], gm[13] - hp[1], gm[14] - hp[2]}; camDist = len3(cd) / g_cfg.unitsPerM;
    if (camDist.load() > g_fp.maxCamDistM) { nFar++; lastReason = 6; relInit = false; return false; }   // a scripted / cinematic camera away from the hero
    const float s = g_cfg.unitsPerM * 0.01f;   // cm -> game units
    const bool rightOfPair = g_cfg.xr == 4 && g_pairPhase == 1 && wasActive && relInit && prevFrame == pairLeftFrame && f == pairLeftFrame + 1;
    if (rightOfPair) {   // the right eye of the pair: the left tick's final anchor (one body pose per pair); how far apart they would be
        const float sU = relUpF, sH[3] = {relHF[0], relHF[1], relHF[2]}; float a[3];
        if (anchorFrom(a)) { const float d[3] = {a[0] - anchor[0], a[1] - anchor[1], a[2] - anchor[2]}; const float dl = len3(d) / s; if (dl > lrDiffMax.load()) lrDiffMax = dl; }
        relUpF = sU; for (int c = 0; c < 3; c++) relHF[c] = sH[c]; nPairReuse++;
    } else {
        const u64 now = GetTickCount64(); const float dt = lastT ? (float)(now - lastT) * 0.001f : 0.0f; lastT = now;
        // the head's sustained lean away from the root (running posture, crouch) is followed slowly; the gait bob (~2 Hz) is not
        const float dtc = dt > 0.2f ? 0.2f : dt;
        if (!relInit) { float hz[3]; for (int c = 0; c < 3; c++) hz[c] = (hp[c] - root[12 + c]) - up * U[c]; relUpC = up; for (int c = 0; c < 3; c++) relHC[c] = hz[c]; latC = latF; relInit = true; frameAv = frameAh = 1; }
        else {
            relUpC = relUpF; for (int c = 0; c < 3; c++) relHC[c] = relHF[c]; latC = latF;   // the end state of the previous tick
            frameAv = g_fp.vertTau > 0.0f ? 1.0f - expf(-dtc / g_fp.vertTau) : 1.0f; frameAvFast = 1.0f - expf(-dtc / 0.06f); frameAh = g_fp.horizTau > 0.0f ? 1.0f - expf(-dtc / g_fp.horizTau) : 1.0f;
        }
        bodyStep(root, U, F, wasActive);   // 0.18.5
        {   // 0.18.9 stand tall: the standing head height is tracked (rises at once, sinks over ~20 s); a SHALLOW drop below it while
            // nearly still (the sword stance: knees bent) is given back by raising the drawn body with the feet planted (legs IK in the job)
            // and the eyes with it; a deep drop (sneaking crouch), riding, fast motion and the game owning the arms (climbing) are left alone
            static float standH = -1e9f; static u64 lt = 0; const u64 nw = GetTickCount64(); const float dt2 = lt ? fminf((float)(nw - lt) * 0.001f, 0.1f) : 0.0f; lt = nw;
            const float s1 = g_cfg.unitsPerM * 0.01f, hNow = relUpC;
            if (!wasActive || standH < -1e8f) standH = hNow; else if (hNow > standH) standH = hNow; else standH += (hNow - standH) * (1.0f - expf(-dt2 / 20.0f));
            const float drop = standH - hNow; const bool owned = GetTickCount64() < animUntil.load() || GetTickCount64() < raisedUntil.load();
            const float want = (g_fp.standTall && legsReady.load() && !ridingNow.load() && !owned && speedMs.load() < 2.5f && drop > 1.0f * s1 && drop < g_fp.stanceMaxCm * s1) ? drop : 0.0f;
            static float cur = 0; cur += (want - cur) * (dt2 > 0 ? 1.0f - expf(-dt2 / 0.15f) : 1.0f); standDz = cur;
        }
        float a[3]; if (anchorFrom(a)) memcpy(anchor, a, sizeof a);
        recompute = g_cfg.xr == 4 && g_pairPhase == 0; if (recompute) pairLeftFrame = f;
    }
    if (!calibrated && g_fp.hide) {   // the head set: joints within the radius of the head joint in this (first) pose
        nHide = 0; const float r = g_fp.hideRadiusCm * s;
        for (int i = 0; i < nJ && nHide < HIDE_CAP; i++) { if (strayed[i]) continue; const float *m = J(i); if (!saneLoose(m, root) || !shown(m)) continue; const float d[3] = {m[12] - hp[0], m[13] - hp[1], m[14] - hp[2]}; if (len3(d) <= r) hideSet[nHide++] = i; }
        // collapse target: the highest shown joint near the body axis that is outside the head set and below the head (the neck)
        static u8 inSet[JOINT_CAP]; memset(inSet, 0, sizeof inSet); for (int k = 0; k < nHide; k++) inSet[hideSet[k]] = 1;
        neckIdx = -1; float best = -1e9f;
        for (int i = 0; i < nJ; i++) { if (inSet[i]) continue; const float *m = J(i); if (!saneLoose(m, root) || !shown(m)) continue;
            const float d[3] = {m[12] - root[12], m[13] - root[13], m[14] - root[14]}; const float u = d[0] * U[0] + d[1] * U[1] + d[2] * U[2];
            const float hzz[3] = {d[0] - u * U[0], d[1] - u * U[1], d[2] - u * U[2]}; if (len3(hzz) < 30.0f && u < up && u > best) { best = u; neckIdx = i; } }
        if (neckIdx < 0) neckIdx = headIdx;
        calibrated = true; logf("first person: head set = %d joints within %.0f cm of joint %d (head %.0f cm above the root), collapsed onto joint %d (%.0f cm above the root) in the render pose; Shift+F5 logs the skeleton", nHide, g_fp.hideRadiusCm, headIdx, up / s, neckIdx, best / s);
        { static int once = 0; if (!once++) dump(); }
        publish();
    }
    if (calibrated) {   // joints the animation job saw stray from the neck (a hand by the face in the first pose): out of the set for good
        int drop = 0;
        for (int k = 0; k < nHide; k++) { const int j = hideSet[k]; if (j < JOINT_CAP && strayFlag[j].load(std::memory_order_relaxed) && !strayed[j]) { strayed[j] = 1; drop++; nDropped++; logf("first person: joint %d moved more than 45 cm from the neck -> not a head joint, no longer hidden", j); } }
        if (drop) { int w = 0; for (int k = 0; k < nHide; k++) if (!strayed[hideSet[k]]) hideSet[w++] = hideSet[k]; nHide = w; publish(); }
    }
    if (calibrated && armsReady.load() && !armsFiltered.load()) {   // 0.18.7: arm joints are never hidden with the head (a hand by the face)
        static uint8_t inArm[JOINT_CAP]; memset(inArm, 0, sizeof inArm); for (int sd = 0; sd < 2; sd++) for (int i = 0; i < armsD[sd].nSub[0]; i++) inArm[armsD[sd].sub[0][i]] = 1;
        int w = 0, dropped = 0; for (int q = 0; q < nHide; q++) { if (hideSet[q] < JOINT_CAP && inArm[hideSet[q]]) { dropped++; continue; } hideSet[w++] = hideSet[q]; } nHide = w;
        if (inArm[neckIdx < 0 ? 0 : neckIdx]) logf("arms: WARNING the head collapse target %d is an arm joint", neckIdx);
        armsFiltered = 1; publish(); logf("arms: %d arm joints taken out of the head set (%d left)", dropped, nHide);
    }
    if (g_fp.hide && calibrated) hideFrame = f;
    if (dumpReq.exchange(0)) dump();
    gm[12] = anchor[0]; gm[13] = anchor[1]; gm[14] = anchor[2];
    activeNow = true; nActive++; lastReason = 0;
    return true;
}
} // namespace fp

// 0.18.24 FLAT PLAY with logging (owner: "an ini setting that lets me play totally flat screen but keeps recording logs, so I can play more and
// run into climbing, takedowns etc. that you can log and see the flags for"). play_mode=flat in gotvr.ini: no headset, no stereo, the
// game's own camera and controls (xr forced to 0) - and an observer on the game thread:
//  - STATE/s: Jin's position, ground speed, vertical speed (in the air?), the pad's buttons / triggers;
//  - every small-integer field of the hero object (16 KB) that CHANGES is logged with its old and new value and the time (fields that
//    change on most ticks - timers, counters - are found automatically and left out): climbing, takedowns, stances, riding, the bow,
//    swimming... each switches flags there, and the log shows which, next to the button that started it;
//  - F8 writes a numbered MARK line ("I'm climbing now") so the moments are easy to find.
namespace obs {
static std::atomic<u64> cbtDummy[4]; static std::atomic<u64> *cbtHits = &cbtDummy[0], *cbtHitsHero = &cbtDummy[1], *cbtHealth = &cbtDummy[2], *cbtHealthHero = &cbtDummy[3];   // 0.18.43 set by cbt::install
// 0.18.44 (owner's design doc: data for the whole combat / traversal design): 23 tags - numpad 0-9, Ctrl+numpad 0-9, numpad / * -
static const int NTAG = 24;
static const char *kTag[NTAG] = {nullptr, "light attacks", "heavy attacks", "blocking", "parry", "perfect parry", "dodge / perfect dodge", "taking hits", "standoff", "finisher (crawling enemy)",
    "assassination (ground / from height)",
    "bow", "longbow + concentrated aim", "kunai", "blowgun", "heavenly strike", "dance of wrath", "grapple", "wall climbing", "ledges / ropes / logs / beams", "crouch / stealth",
    "healing / resolve", "cutscene", "horse"};
static const char *kTagKey[NTAG] = {"", "numpad 1", "numpad 2", "numpad 3", "numpad 4", "numpad 5", "numpad 6", "numpad 7", "numpad 8", "numpad 9", "numpad 0",
    "Ctrl+numpad 1", "Ctrl+numpad 2", "Ctrl+numpad 3", "Ctrl+numpad 4", "Ctrl+numpad 5", "Ctrl+numpad 6", "Ctrl+numpad 7", "Ctrl+numpad 8", "Ctrl+numpad 9", "Ctrl+numpad 0", "numpad /", "numpad *", "numpad -"};
// 0.18.44 keyboard / mouse presses (the owner plays flat on keyboard and mouse - the pad capture saw nothing): edges with their time
static const struct { int vk; const char *n; } kKeys[] = {{VK_LBUTTON, "LMB"}, {VK_RBUTTON, "RMB"}, {VK_MBUTTON, "MMB"}, {VK_XBUTTON1, "Mouse4"}, {VK_XBUTTON2, "Mouse5"},
    {VK_SPACE, "Space"}, {VK_LSHIFT, "LShift"}, {VK_LCONTROL, "LCtrl"}, {VK_LMENU, "LAlt"}, {VK_TAB, "Tab"}, {'Q', "Q"}, {'E', "E"}, {'R', "R"}, {'F', "F"}, {'C', "C"}, {'X', "X"},
    {'Z', "Z"}, {'V', "V"}, {'G', "G"}, {'T', "T"}, {'Y', "Y"}, {'H', "H"}, {'B', "B"}, {'1', "1"}, {'2', "2"}, {'3', "3"}, {'4', "4"}, {'W', "W"}, {'A', "A"}, {'S', "S"}, {'D', "D"}};
static const int NK = (int)(sizeof kKeys / sizeof kKeys[0]); static bool kDown[64]; static char keyLog[1024]; static int keyLogN = 0; static u32 wasdOr = 0;
static void keysTick(u32 ms) { for (int i = 0; i < NK; i++) { const bool d = (GetAsyncKeyState(kKeys[i].vk) & 0x8000) != 0; if (d != kDown[i]) { kDown[i] = d; if (i >= NK - 4) continue;   // WASD: only held (below)
            if (keyLogN < (int)sizeof keyLog - 24) keyLogN += snprintf(keyLog + keyLogN, sizeof keyLog - (size_t)keyLogN, " %s%s@%u", kKeys[i].n, d ? "+" : "-", ms); } } }
static std::atomic<int> tag{0}; static u64 tagAt = 0, tagN = 0;
static std::atomic<int> on{0}; static std::atomic<u64> nMarks{0};
static const int NB = 0x8000, NW = NB / 4;   // 0.18.45: 32 KB of the hero (was 16)
static u32 prevW[NW]; static uint16_t chg[NW]; static uint8_t noisy[NW]; static bool have = false; static u8 *lastHero = nullptr; static int nNoisy = 0;
struct Ev { u32 off, a, b; u32 ms; u8 isF; }; static Ev ev[192]; static int nEv = 0, nDropped = 0;
static float lastP[3] = {0, 0, 0}; static bool lastPOk = false; static LARGE_INTEGER fq{}, lastQ{}, secQ{}; static float vzMax = 0, spMax = 0, vzMin = 0; static u32 btnOr = 0; static u8 ltMax = 0, rtMax = 0;
static inline bool small(u32 v) { return v < 0x10000u; }
static inline bool plainF(u32 v) { float f; memcpy(&f, &v, 4); const float a = fabsf(f); return std::isfinite(f) && a >= 0.01f && a <= 100000.0f; }   // 0.18.42 a value like health / resolve / posture
// 0.18.45 (owner's 0.18.44 session: a parry can only be read AFTER it lands - the +12.5 health message - so a VR parry needs the ENEMY's
// attack wind-up): the enemy last involved in a hit (hit by Jin, or the attacker of a hit on Jin = hitInfo+0x48) is watched like the hero
static std::atomic<u8 *> enemyPtr{nullptr};
static const int NBE = 0x4000, NWE = NBE / 4;
static u32 eprev[NWE]; static uint16_t echg[NWE]; static uint8_t enoisy[NWE]; static u8 *eObj = nullptr; static Ev eev[128]; static int eN = 0, eDrop = 0, eNoisy = 0;
static void enemyTick(u32 ms) {
    u8 *e = enemyPtr.load(); if (!e || !readable(e, NBE)) { eObj = nullptr; return; }
    const u32 *W = (const u32 *)e;
    if (e != eObj) { eObj = e; memcpy(eprev, W, NBE); memset(echg, 0, sizeof echg); memset(enoisy, 0, sizeof enoisy); eNoisy = 0; eN = 0; logf("OBS: watching enemy %p (class %d) - the last one in a hit with Jin", (void *)e, fp::classId(e)); return; }
    for (int i = 0; i < NWE; i++) { const u32 v = W[i]; if (v == eprev[i]) continue; if (echg[i] < 0xFFFF) echg[i]++;
        if (!enoisy[i] && ((small(v) && small(eprev[i])) || (plainF(v) && plainF(eprev[i])))) { if (eN < (int)(sizeof eev / sizeof eev[0])) eev[eN++] = {(u32)(i * 4), eprev[i], v, ms, (u8)(small(v) ? 0 : 1)}; else eDrop++; }
        eprev[i] = v; } }
static void enemySecond() {
    if (!eObj) return; int nn = 0; for (int i = 0; i < NWE; i++) { if (echg[i] > 6 && !enoisy[i]) { enoisy[i] = 1; eNoisy++; nn++; } echg[i] = 0; }
    if (eN || eDrop) logf("   ENEMY/s: %p (class %d) health %.1f | changes %d (%d dropped; %d busy fields ignored, %d new)", (void *)eObj, fp::classId(eObj), readable(eObj + 0x1974, 4) ? *(const float *)(eObj + 0x1974) : -1.0f, eN, eDrop, eNoisy, nn);
    int shown = 0; for (int k = 0; k < eN && shown < 60; k++) { const Ev &x = eev[k]; if (enoisy[x.off / 4]) continue; shown++;
        if (x.isF) { float fa, fb; memcpy(&fa, &x.a, 4); memcpy(&fb, &x.b, 4); logf("      enemy+0x%04x: %g -> %g (float, +%u ms)", x.off, fa, fb, x.ms); } else logf("      enemy+0x%04x: %u -> %u (+%u ms)", x.off, x.a, x.b, x.ms); }
    eN = 0; eDrop = 0; }
static void tick() {   // game thread (GameTick), flat play only
    if (!on.load()) return;
    if (!fq.QuadPart) QueryPerformanceFrequency(&fq); LARGE_INTEGER qn; QueryPerformanceCounter(&qn); if (!secQ.QuadPart) secQ = qn;
    const float dt = lastQ.QuadPart ? (float)(qn.QuadPart - lastQ.QuadPart) / (float)fq.QuadPart : 0.0f; lastQ = qn;
    u8 *h = fp::heroPtr();
    if (!h || !readable(h, NB)) { if (have) logf("OBS: the hero is gone (loading / menu)"); have = false; lastPOk = false; return; }
    const u32 *W = (const u32 *)h;
    if (h != lastHero || !have) { lastHero = h; memcpy(prevW, W, NB); memset(chg, 0, sizeof chg); have = true; lastPOk = false; logf("OBS: hero %p (class id %d) - watching its first %d KB for flag changes", (void *)h, fp::classId(h), NB / 1024); return; }
    {   // where he is and how fast
        const float *M = (const float *)(h + 0x150); const float p[3] = {M[12], M[13], M[14]};
        if (std::isfinite(p[0]) && std::isfinite(p[1]) && std::isfinite(p[2])) {
            if (lastPOk && dt > 0.0005f && dt < 0.2f) { const float dx = p[0] - lastP[0], dy = p[1] - lastP[1], dz = p[2] - lastP[2];
                const float sp = sqrtf(dx * dx + dy * dy) / dt / g_cfg.unitsPerM, vz = dz / dt / g_cfg.unitsPerM; spMax = fmaxf(spMax, sp); vzMax = fmaxf(vzMax, vz); vzMin = fminf(vzMin, vz); }
            memcpy(lastP, p, 12); lastPOk = true; }
    }
    if (g_padHave[0][0]) { btnOr |= *(const uint16_t *)(g_padLast[0][0] + 4); ltMax = g_padLast[0][0][6] > ltMax ? g_padLast[0][0][6] : ltMax; rtMax = g_padLast[0][0][7] > rtMax ? g_padLast[0][0][7] : rtMax; }
    const u32 ms = (u32)((qn.QuadPart - secQ.QuadPart) * 1000 / fq.QuadPart);
    keysTick(ms); enemyTick(ms); for (int i = NK - 4; i < NK; i++) if (kDown[i]) wasdOr |= 1u << (i - (NK - 4));
    for (int i = 0; i < NW; i++) { const u32 v = W[i]; if (v == prevW[i]) continue;
        if (chg[i] < 0xFFFF) chg[i]++;
        if (!noisy[i] && ((small(v) && small(prevW[i])) || (plainF(v) && plainF(prevW[i])))) { if (nEv < (int)(sizeof ev / sizeof ev[0])) ev[nEv++] = {(u32)(i * 4), prevW[i], v, ms, (u8)(small(v) ? 0 : 1)}; else nDropped++; }
        prevW[i] = v; }
    if ((qn.QuadPart - secQ.QuadPart) >= fq.QuadPart) {   // once a second
        int newNoisy = 0; for (int i = 0; i < NW; i++) { if (chg[i] > 6 && !noisy[i]) { noisy[i] = 1; nNoisy++; newNoisy++; } chg[i] = 0; }
        logf("   STATE/s:%s%s Jin %.0f %.0f %.0f | ground speed up to %.1f m/s, vertical %+.1f..%+.1f m/s%s | pad buttons 0x%04x LT %d RT %d | hero flag changes %d (%d dropped; %d busy fields ignored, %d new)",
             tag.load() ? " TAG " : "", tag.load() ? kTag[tag.load()] : "", lastP[0], lastP[1], lastP[2], spMax, vzMin, vzMax, (vzMax > 0.8f || vzMin < -0.8f) ? " (in the air)" : "", btnOr, ltMax, rtMax, nEv, nDropped, nNoisy, newNoisy);
        enemySecond();
        if (keyLogN || wasdOr) { logf("   INPUT/s: keys%s | moving %s%s%s%s", keyLogN ? keyLog : " none", wasdOr & 1 ? "W" : "", wasdOr & 2 ? "A" : "", wasdOr & 4 ? "S" : "", wasdOr & 8 ? "D" : ""); keyLogN = 0; keyLog[0] = 0; wasdOr = 0; }   // 0.18.44 (key+ / key- at ms in this second)
        {   // 0.18.43 the combat observer's second (it is declared further down: counters read through these pointers)
            const int inc = readable(h + 0x39A0, 4) ? *(const int *)(h + 0x39A0) : -1;
            logf("   COMBAT/s: hits %llu (on Jin %llu), health changes %llu (Jin %llu), paired actions (hero+0x39A0: finisher / mount / assassination) %d, Jin's health %.1f of %.1f",
                 (unsigned long long)cbtHits->exchange(0), (unsigned long long)cbtHitsHero->exchange(0), (unsigned long long)cbtHealth->exchange(0), (unsigned long long)cbtHealthHero->exchange(0), inc,
                 readable(h + 0x1974, 4) ? *(const float *)(h + 0x1974) : -1.0f, readable(h + 0x1970, 4) ? *(const float *)(h + 0x1970) : -1.0f); }
        int shown = 0;
        for (int k = 0; k < nEv && shown < 60; k++) { const Ev &e = ev[k]; if (noisy[e.off / 4]) continue; shown++; if (e.isF) { float fa, fb; memcpy(&fa, &e.a, 4); memcpy(&fb, &e.b, 4); logf("      hero+0x%04x: %g -> %g (float, +%u ms)", e.off, fa, fb, e.ms); } else logf("      hero+0x%04x: %u -> %u (+%u ms)", e.off, e.a, e.b, e.ms); }
        nEv = 0; nDropped = 0; spMax = 0; vzMax = 0; vzMin = 0; btnOr = 0; ltMax = rtMax = 0; secQ = qn;
    }
}
static void mark() { const u64 n = ++nMarks; logf("==================== MARK %llu (F8) ====================", (unsigned long long)n); }
// 0.18.41 (owner: "I'm going to load a modded save to play flat screen with everything unlocked and log different actions - numpad 1 sword
// combat, 2 archery, 3 climbing, 4 grappling, 6 cutscene"): a numpad key starts that tag (a banner in the log), the same key again ends it,
// another key switches; every STATE/s line carries the tag, so the log can be cut by action (grep "TAG sword combat").
static void setTag(int t) { const int cur = tag.load(); const u64 now = GetTickCount64();
    if (cur) logf("==================== TAG %s END (%.1f s) ====================", kTag[cur], (now - tagAt) / 1000.0);
    if (t == cur) { tag = 0; return; }
    tag = t; tagAt = now; tagN++; logf("==================== TAG %s BEGIN (#%llu, %s) ====================", kTag[t], (unsigned long long)tagN, kTagKey[t]); }
} // namespace obs
// ---------------- 0.18.43 combat observer (the owner's flat-play session 0.18.42: sword combat, parries, hits taken) ----------------
// Found from that log + the binary (rva = image offset, this build only - both signature-checked, fail closed, observe only):
//  - hero+0x1974 float = health (+0x1970 = max). Its ONE runtime writer is SetHealth(character, float) at 0x69F3D0 - every damage,
//    heal (+122.5 per resolve heal) and regen tick (+7) of EVERY character goes through it.
//  - HandleHit(character, hitInfo) at 0x694080 (a virtual - four character vtables): drops repeats of hitInfo+0x2C (the hit id, a
//    16-entry ring at +0x1990), logs the hit into a 32-entry history at +0x19E0 (+0x19D8 index, +0x19DC count): time, id, +0x94 and
//    +0x98 (floats), +0x9C / +0xA0 / +0xA4 when +0x90 has bits 2 / 8 / 0x80; +0x90 bit 20 = the kill path (SetHealth 0, state 2).
//  - hero+0x3918.. = a list of PAIRED actions (16-byte entries, count at +0x39A0) - 0.18.45 correction (0.18.44 session): set 12 ms
//    after the click of a finisher on a crawling enemy, at a horse mount (E), around assassinations - not incoming attacks.
// Logged per call (rate-limited) so the next flat session tells which field is the damage, which the attacker, which the hit kind.
namespace cbt {
static const u32 RVA_SETHEALTH = 0x69F3D0, RVA_HANDLEHIT = 0x694080;
typedef void (*FnSetHealth)(u8 *, float); static FnSetHealth oSetHealth = nullptr;
typedef u64 (*FnHandleHit)(u8 *, u8 *); static FnHandleHit oHandleHit = nullptr;
static std::atomic<u64> nHealth{0}, nHits{0}, nHitsHero{0}, nHealthHero{0}; static std::atomic<int> secLogs{0}; static std::atomic<u64> secMs{0};
static bool allow() { const u64 now = GetTickCount64(); if (now - secMs.load() >= 1000) { secMs = now; secLogs = 0; } return secLogs.fetch_add(1) < 40; }
static float rdF(const u8 *p) { float v = 0; if (readable(p, 4)) memcpy(&v, p, 4); return v; }
static void callers(const char *what);   // below (WRIST_HUD.md W0)
static void hSetHealth(u8 *ch, float hp) {
    const float old = rdF(ch + 0x1974);
    if (g_playFlat && ch == fp::heroPtr() && hp - old > 20.0f) { static u64 last = 0; const u64 now = GetTickCount64(); if (now - last > 300) { last = now; char w[96]; snprintf(w, sizeof w, "Jin healed %.1f -> %.1f", old, hp); callers(w); } }   // WRIST_HUD.md W0
    oSetHealth(ch, hp); nHealth++;
    const bool hero = ch == fp::heroPtr(); if (hero) nHealthHero++;
    if (allow()) logf("COMBAT health: %s %p (class %d) %.1f -> %.1f (%+.1f) of %.1f%s", hero ? "JIN" : "other", (void *)ch, fp::classId(ch), old, hp, hp - old, rdF(ch + 0x1970), obs::tag.load() ? obs::kTag[obs::tag.load()] : "");
}
namespace rscan { static void event(const char *what); }
static int resScan = 0;   // 0.18.54: the resolve finder found it (hero+0x43A4 / max hero+0x4644 + 3) - off unless cbt_resolve_scan=1 (its snapshots cost time)   // 0.18.49 below
static void noteHit(u8 *ch, u8 *d);   // 0.18.47 hit templates + the enemy registry (below)
static bool candOn = false, candSeen = false; static u32 candFl = 0; static float candDmg = 0;
static bool parryReplay = false, parrySeen = false; static u32 parryFl = 0; static float parryDmg = 0;   // 0.18.61 block / deflect: the blow the game composed on our replay   // 0.18.55 record library: the blow the game composed from a real record
static bool diffWant(u8 *ch, u8 *d);   // below (0.18.48 diagnostics)
static void diffReal(u8 *ch, u8 *d, bool before);
static u64 hHandleHit(u8 *ch, u8 *d) {
    noteHit(ch, d);
    const bool dw = diffWant(ch, d); if (dw) diffReal(ch, d, true);
    const float before = rdF(ch + 0x1974); const u64 r = oHandleHit(ch, d); const float after = rdF(ch + 0x1974); nHits++;
    if (dw) diffReal(ch, d, false);
    const bool hero = ch == fp::heroPtr(); if (hero) nHitsHero++;
    { const u32 fk = d && readable(d + 0x90, 4) ? *(const u32 *)(d + 0x90) : 0u; const bool fight = (fk & 0x60000000u) == 0 && (fk & 0x41u) != 0;   // 0.18.46: not the spawn messages (0x6000000x set max health) - a real hit
      u8 *en = hero ? (d && readable(d + 0x48, 8) ? *(u8 **)(d + 0x48) : nullptr) : ch; if (fight && en && en != fp::heroPtr() && !((u64)en & 7)) obs::enemyPtr = en; }   // 0.18.45 the enemy to watch
    if (d && readable(d, 0xB0) && allow()) {
        const u32 fl = *(const u32 *)(d + 0x90);
        logf("COMBAT hit on %s %p (class %d): id %u flags 0x%08x | +94 %.2f +98 %.2f +9C %.2f +A0 %.2f +A4 %.2f | health %.1f -> %.1f | returned %llu%s%s", hero ? "JIN" : "other", (void *)ch, fp::classId(ch),
             *(const u32 *)(d + 0x2C), fl, rdF(d + 0x94), rdF(d + 0x98), rdF(d + 0x9C), rdF(d + 0xA0), rdF(d + 0xA4), before, after, (unsigned long long)(r & 0xFF), obs::tag.load() ? " | " : "", obs::tag.load() ? obs::kTag[obs::tag.load()] : "");
        char b[1200]; int o = 0; for (int q = 0; q < 0xB0 / 8 && o < 1150; q++) o += snprintf(b + o, sizeof b - (size_t)o, "%s%016llx", q % 4 ? " " : (q ? " | " : ""), (unsigned long long)*(const u64 *)(d + q * 8));
        logf("   hitInfo 0x00..0xAF: %s", b); }
    return r;
}
// ---------------- 0.18.47 combat stage 1: your katana's hits, delivered as the game's own hit messages ----------------
// How the game builds one (this build, all signature-checked; read from the binary - COMBAT_FINDINGS.md "Session 4"):
//  - a hit message is a 0x210-byte ref-counted object (+0x08 count) from its own pool: 0x7686D0 hands one out, the deleting destructor
//    (vtable+0x10) puts it back. 0x84B390(msg, target) = the base constructor + the hit vtable + a fresh hit id (+0x2C) + the target.
//  - +0x30 target, +0x48 attacker, +0x50, +0x168 are weak handles: 0x84BB50 sets one (the slot is registered with the object, which
//    nulls it when it goes). +0x58 / +0x78 are tag lists (u64 hashes; 0x519DB0 copies one). +0x1C8 / +0x1E0 more lists (left empty).
//  - +0x90 flags, +0x94 health change, +0x98 posture change, +0x9C..+0xA4 optional (flags 2 / 8 / 0x80), +0x170 48 bytes (flag
//    0x01000000), +0x1A0 (flag 0x02000000).
// The template: the last plain hit Jin landed with the game's own attack (flat play, or in VR before the katana comes out) - its numbers,
// tags and payload kept (gotvr.ini cbt_tpl_light / cbt_tpl_heavy), so a hit from your katana does what Jin's do: the same damage,
// posture, reaction, sound and blood. A blow that would kill is sent the way the game sends Jin's killing blows (0x40 cleared, -FLT_MAX).
// Enemies: characters (class 14 people, 16 dogs) seen in a hit with Jin (they hit him or he hit them) - kept as the game's own weak
// handles, so a dead / unloaded one is never touched. Only those are struck: allies and villagers never (sword_hit_any = 1 lifts it).
// Everything runs on the game thread (the hits are sent right after the game's tick) and fails closed: a signature miss, HandleHit
// seen off the game thread, no template yet, a character without this HandleHit -> no hit, logged.
static const u32 RVA_MSGPOOL = 0x7686D0, RVA_MSGCTOR = 0x84B390, RVA_MSGTAGS = 0x519DB0, RVA_SETHANDLE = 0x84BB50;
typedef u8 *(*FnPool)(); typedef u8 *(*FnCtor)(u8 *, u8 *); typedef void (*FnTags)(u8 *, const void *); typedef void (*FnSetH)(u8 **, u8 *);
static FnPool pPool = nullptr; static FnCtor pCtor = nullptr; static FnTags pTags = nullptr; static FnSetH pSetH = nullptr; static bool strikeOk = false;
static std::atomic<u64> nOnTick{0}, nOffTick{0};
struct Tpl { u32 ver, v28, m40, n58, n78, flags; u64 h38, t58[16], t78[16]; u8 r90[0x20]; u8 r170[0x40]; float tpos[3], hpos[3]; };
static Tpl tpl[2]; static bool tplOk[2] = {false, false}, tplLive[2] = {false, false}; static u8 *wk50[2] = {nullptr, nullptr}, *wk168[2] = {nullptr, nullptr};
static u8 *wkAny50 = nullptr;   // 0.18.50: +0x50 (class 24, Jin's weapon?) of Jin's latest real blow of any kind - session 7 sent blows from the ini template with none
static const char *kTplName[2] = {"light", "heavy"};
// settings (gotvr.ini, design-doc names)
static int hitOn = 1, hitAny = 0; static melee::Cfg mcfg; static float bladeCutCm = 70.0f, dmgMin = 0.75f, dmgMax = 1.10f, fullSpeed = 8.0f, rumble = 0.9f;
static int rumbleMs = 70, reachCm = 450; static float twoHandMult = 1.0f;
// 0.18.48 the design doc's attack parameters around the contact (attack.h): swing recognition, heavy charge, lunge, katana weight
static atk::Cfg acfg; static float swingRumble = 0.35f, swingRumbleHeavy = 0.7f, lungeSpeed = 4.5f; static int lungeOn = 1, heavySwingMs = 220;
static u32 secSwings = 0, secHeavy = 0, secLunges = 0, secCharges = 0; static atk::Intent intent; static atk::Weight weightLag; static u64 nWeightResets = 0;
// the enemy registry (game thread only)
static const int NR = 48; static u8 *reg[NR] = {}; static u64 regSeen[NR] = {}; static u8 regHostile[NR] = {}; static u32 regGen[NR] = {};
static float rdF3(const u8 *m, float *p) { if (!m || !readable(m + 0x150, 64)) return 0; const float *M = (const float *)(m + 0x150); p[0] = M[12]; p[1] = M[13]; p[2] = M[14]; return std::isfinite(p[0]) && std::isfinite(p[1]) && std::isfinite(p[2]) ? 1.0f : 0.0f; }
static int clsOf(u8 *c) { return c && !((u64)c & 7) && readable(c, 8) ? fp::classId(c) : -1; }
static bool alive(u8 *c) { const int k = clsOf(c); return (k == 14 || k == 16) && readable(c + 0x1970, 8) && rdF(c + 0x1974) > 0.0f; }
static void regNote(u8 *c, bool hostile, u64 now) {
    if (!pSetH || !c || c == fp::heroPtr()) return; const int k = clsOf(c); if (k != 14 && k != 16) return;
    int fr = -1, old = -1; for (int i = 0; i < NR; i++) { if (reg[i] == c) { regSeen[i] = now; if (hostile && !regHostile[i]) { regHostile[i] = 1; logf("STRIKE: enemy %p (class %d) now known hostile (in a hit with Jin)", (void *)c, k); } return; }
        if (!reg[i]) { if (fr < 0) fr = i; } else if (old < 0 || regSeen[i] < regSeen[old]) old = i; }
    const int i = fr >= 0 ? fr : old; if (i < 0) return;
    pSetH(&reg[i], c); regSeen[i] = now; regHostile[i] = hostile ? 1 : 0; regGen[i]++;   // the game's own weak handle: it nulls reg[i] when c goes
}
// 0.18.48 (owner session 5: most strikes refused - an enemy counted only after a hit with Jin): the game's own attitude query decides
// who is an enemy. 0x9552F0(table 0x17F7560, a, b) = a's attitude toward b (the faction index at character+0x600; 2 = hostile) - it is
// what the threat test 0x950C40 asks before an attacker enters a victim's threat list. Game thread only (it fills a small cache).
static const u32 RVA_ATTITUDE = 0x9552F0, RVA_ATTTABLE = 0x17F7560;
typedef u64 (*FnAtt)(u8 *, u8 *, u8 *); static FnAtt pAtt = nullptr;
static int attitude(u8 *a, u8 *b) { if (!pAtt || !a || !b || !readable(a + 0x600, 4) || !readable(b + 0x600, 4)) return -1; return (int)(pAtt(g_base + RVA_ATTTABLE, a, b) & 0xFF); }
static bool hostile(int i) { if (hitAny || regHostile[i]) return true; return attitude(reg[i], fp::heroPtr()) == 2; }
static bool listCopy(const u8 *d, u32 off, u64 *dst, u32 &n) { const u64 *p = *(u64 *const *)(d + off); const int c = *(const int *)(d + off + 8);
    if (c < 0 || c > 16 || (c && (!p || !readable(p, (size_t)c * 8)))) return false; n = (u32)c; if (c) memcpy(dst, p, (size_t)c * 8); return true; }
static void tplSave(int k) {
    wchar_t ini[MAX_PATH]; swprintf(ini, MAX_PATH, L"%ls\\gotvr.ini", g_dir); static wchar_t hx[sizeof(Tpl) * 2 + 4]; const u8 *b = (const u8 *)&tpl[k];
    for (size_t i = 0; i < sizeof(Tpl); i++) swprintf(hx + i * 2, 3, L"%02x", b[i]); WritePrivateProfileStringW(L"gotvr", k ? L"cbt_tpl_heavy" : L"cbt_tpl_light", hx, ini); }
static void tplLoad(const wchar_t *ini, int k) {
    static wchar_t hx[sizeof(Tpl) * 2 + 8]; const DWORD n = GetPrivateProfileStringW(L"gotvr", k ? L"cbt_tpl_heavy" : L"cbt_tpl_light", L"", hx, sizeof hx / sizeof hx[0], ini);
    if (n != sizeof(Tpl) * 2) return; Tpl t; u8 *b = (u8 *)&t; for (size_t i = 0; i < sizeof(Tpl); i++) { unsigned v = 0; if (swscanf(hx + i * 2, L"%2x", &v) != 1) return; b[i] = (u8)v; }
    if (t.ver != 1 || t.n58 > 16 || t.n78 > 16) return;
    if (t.flags & 0x10000000u) { logf("STRIKE: %s hit template in gotvr.ini carries the hit-reaction bit (0x%08x) - not used (the message path crashed with it, session 13); a plain blow replaces it", kTplName[k], t.flags); WritePrivateProfileStringW(L"gotvr", k ? L"cbt_tpl_heavy" : L"cbt_tpl_light", nullptr, ini); return; }
    tpl[k] = t; tplOk[k] = true; tplLive[k] = false;
    float dm; memcpy(&dm, t.r90 + 4, 4); logf("STRIKE: %s hit template from gotvr.ini (flags 0x%08x, health %.1f, %u + %u tags)", kTplName[k], t.flags, dm, t.n58, t.n78); }
static void capture(int k, u8 *ch, const u8 *d) {
    Tpl t; memset(&t, 0, sizeof t); t.ver = 1; t.v28 = *(const u32 *)(d + 0x28); t.m40 = *(const u32 *)(d + 0x40); t.h38 = *(const u64 *)(d + 0x38); t.flags = *(const u32 *)(d + 0x90);
    if (!listCopy(d, 0x58, t.t58, t.n58) || !listCopy(d, 0x78, t.t78, t.n78)) return;
    memcpy(t.r90, d + 0x90, 0x20); memcpy(t.r170, d + 0x170, 0x40); rdF3(ch, t.tpos); rdF3(fp::heroPtr(), t.hpos);
    float dn, dc; memcpy(&dn, t.r90 + 4, 4); memcpy(&dc, tpl[k].r90 + 4, 4);
    float pn, pc; memcpy(&pn, t.r90 + 8, 4); memcpy(&pc, tpl[k].r90 + 8, 4);
    const bool rNew = false, rOld = false;   // 0.18.57: the message path never carries the hit-reaction bit (session 13 crash)
    const bool first = !tplLive[k]; if (tplLive[k] && rNew == rOld && (fabsf(dn) > fabsf(dc) + 0.01f || (fabsf(dn) > fabsf(dc) - 0.01f && fabsf(pn) > fabsf(pc) + 0.01f))) return;   // the session's plainest blow (the opener of a combo, not its finisher)
    const bool same = tplOk[k] && !memcmp(&t, &tpl[k], offsetof(Tpl, tpos)); tpl[k] = t; tplOk[k] = true; tplLive[k] = true;
    u8 *o50 = *(u8 *const *)(d + 0x50), *o168 = (t.flags & 0x800000u) ? *(u8 *const *)(d + 0x168) : nullptr;
    if (o50 && ((u64)o50 & 7)) o50 = nullptr; if (o168 && ((u64)o168 & 7)) o168 = nullptr;
    pSetH(&wk50[k], o50); pSetH(&wk168[k], o168);   // weak handles: nulled by the game if those objects go
    if (!same) tplSave(k);
    if (first || !same) {
        const float *f = (const float *)t.r170; char b[400]; int o = 0; for (int i = 0; i < 16 && o < 380; i++) o += snprintf(b + o, sizeof b - (size_t)o, " %.2f", f[i]);
        logf("STRIKE: %s hit template %s from Jin's own hit on %p (class %d): flags 0x%08x health %.1f posture %.1f | +28 %u +38 0x%llx move %u | tags %u + %u | +50 %p (class %d) +168 %p (class %d) | lists +1D0 %d +1E8 %d | +170:%s | target at %.0f %.0f %.0f, Jin at %.0f %.0f %.0f",
             kTplName[k], first ? "taken" : "updated", (void *)ch, clsOf(ch), t.flags, dn, *(const float *)(t.r90 + 8), t.v28, (unsigned long long)t.h38, t.m40, t.n58, t.n78, (void *)o50, clsOf(o50), (void *)o168, clsOf(o168),
             *(const int *)(d + 0x1D0), *(const int *)(d + 0x1E8), b, t.tpos[0], t.tpos[1], t.tpos[2], t.hpos[0], t.hpos[1], t.hpos[2]); }
}
static void callers(const char *what) {   // WRIST_HUD.md W0: who sent it - the game frames (rva) of the call chain, to find where resolve is spent
    void *fr[16]; const USHORT n = RtlCaptureStackBackTrace(1, 16, fr, nullptr); char b[400]; int o = 0;
    for (int i = 0; i < n && o < 380; i++) { const u8 *a = (const u8 *)fr[i]; if (a >= g_base && a < g_base + 0x4000000) o += snprintf(b + o, sizeof b - (size_t)o, " 0x%llx", (unsigned long long)(a - g_base)); else o += snprintf(b + o, sizeof b - (size_t)o, " -"); }
    logf("VITALS: %s - called from (game rva, innermost first):%s", what, b); }
static bool injecting = false;   // 0.18.49: our own blow is being delivered (game thread) - not a template, not a diagnostic
static void noteHit(u8 *ch, u8 *d) {   // inside HandleHit, before the game handles it
    if (injecting) return;
    if (parryReplay && d && ch == fp::heroPtr() && readable(d + 0x90, 8)) { parrySeen = true; parryFl = *(const u32 *)(d + 0x90); parryDmg = rdF(d + 0x94); }
    if (g_playFlat && d && ch == fp::heroPtr() && readable(d + 0x90, 8) && (*(const u32 *)(d + 0x90) & 0x40000000u) && !(*(const u32 *)(d + 0x90) & 0x20000000u)) {
        static u64 last = 0; const u64 now = GetTickCount64(); if (now - last > 300) { last = now; char w[96]; snprintf(w, sizeof w, "a resolve heal on Jin (flags 0x%08x, %+.1f)", *(const u32 *)(d + 0x90), rdF(d + 0x94)); callers(w); } }
    const bool onT = GetCurrentThreadId() == t_tick.load(); (onT ? nOnTick : nOffTick)++;
    if (!onT || !strikeOk || !d || !readable(d, 0x210)) return;
    u8 *hero = fp::heroPtr(); if (!hero) return;
    u8 *at = *(u8 **)(d + 0x48); if ((u64)at & 7) at = nullptr; const u32 fl = *(const u32 *)(d + 0x90); const u64 now = GetTickCount64();
    if (fl & 0x60000000u) { if (ch != hero) regNote(ch, false, now); return; }   // spawn: set max health
    if (ch == hero) { if ((fl & 0x40000000u) && !(fl & 0x20000000u)) rscan::event("RESOLVE HEAL on Jin"); else if (fl & 0x04000000u) rscan::event("REVIVE of Jin (2 resolve)");
        else if (fl == 0x00000001u) rscan::event("PARRY by Jin (resolve gain?)");   // 0.18.51 (owner: "when I parry and gain resolve, note it"): marked by the mod itself
        if (at && at != hero) regNote(at, true, now); return; }      // something hit Jin: its attacker is hostile
    if (at == hero && candOn && !candSeen) { candSeen = true; candFl = fl; candDmg = rdF(d + 0x94); }   // 0.18.55 the record library judges by this
    if (at == hero) { regNote(ch, true, now); if (!(fl & 0x40u) && (fl & 1u) && rdF(d + 0x94) < -1e30f) rscan::event("KILL by Jin (resolve gain?)");
        else if (fl & 0x200u) rscan::event("STAGGER by Jin (resolve gain?)");
        { u8 *o50 = *(u8 *const *)(d + 0x50); if (o50 && !((u64)o50 & 7) && o50 != wkAny50 && clsOf(o50) == 24) pSetH(&wkAny50, o50); }
        { const float dd = rdF(d + 0x94); if ((fl & 0x41u) == 0x41u && !(fl & 0x74000000u) && std::isfinite(dd) && dd < -0.5f && dd > -5000.0f) capture((fl & 0x100u) ? 1 : 0, ch, d); } return; }   // 0.18.57: ordinary blows WITHOUT the hit-reaction bit only - session 13: a message carrying it (0x11C01041) without the game's own composition crashed the game   // Jin's own blow (0.18.48: only the plain light / heavy kinds - session 5 took a 0x11C01041 chip first)
    regNote(ch, false, now); if (at) regNote(at, false, now);
}
static bool hasHandleHit(u8 *c) {   // this character's class has the HandleHit we know at vtable+0x1D8 (where the game's delivery calls it)
    u8 *vt = *(u8 **)c; return vt && readable(vt + 0x1D8, 8) && *(u8 **)(vt + 0x1D8) == g_base + RVA_HANDLEHIT; }
// 0.18.49 (session 6: HHDIFF real == HHDIFF ours - HandleHit does the same for both; the flinch, sound and blood come from the delivery
// around it): the game delivers a hit with 0x85A770(msg) - it marks the message delivered (+0x28 = 3), keeps it in the ring of the 64
// latest hits (0x1434C3540, with a reference), fires event 0x16 on the target and 0x17 on the attacker (0x7E05F0(obj, id, 2, 1) - the
// reaction / sound / blood listeners), flag 0x08000000 a shake, then calls the target's HandleHit (vtable+0x1D8), flag 0x400 impact
// effects. Its caller 0x85A4F0 walks the message chain (+0x1F8) and replicates (multiplayer); 0x8504D0 composes (0x84B1A0 + 0x850880).
// 0.18.49: the game sends its own simple messages (the resolve heal, 0x68CE70) exactly this way: pool -> 0x84B390 -> fill -> 0x85A4F0
// -> release. 0x85A4F0 is what the katana uses too.
static const u32 RVA_DELIVER = 0x85A4F0; typedef void (*FnDeliver)(u8 *); static FnDeliver pDeliver = nullptr; static int deliverOn = 1;
// 0.18.50 diagnostics (flat play): the whole delivery (0x85A770: events 0x16 / 0x17 + HandleHit) diffed on the target for real blows
// and ours (DLVDIFF), to see whether the reaction fields (+0x1694 / +0x1698 / +0x16A4) are set inside it at all
static const u32 RVA_DELIVER1 = 0x85A770; static FnDeliver oDeliver1 = nullptr; static int nDlvReal = 0, nDlvOurs = 0; static u8 snapD[0x4000];
// 0.18.51 diagnostics (flat, session 8: DLVDIFF real == ours, still no flinch - the reaction comes from before the delivery): the game's
// own hit processing HitProc 0x8504D0(hitRecord, list) logged with its caller chain and the attack components' callbacks, and every
// script event 0x7E05F0(obj, id, ...) fired at a person within 400 ms after a real blow / ours, with its caller (EVENT lines)
static const u32 RVA_HITPROC = 0x8504D0, RVA_EVENT = 0x7E05F0;
typedef void (*FnHitProc)(u8 *, u8 *); static FnHitProc oHitProc = nullptr; typedef void *(*FnEvent)(u8 *, int, u32, u32, u64, u64); static FnEvent oEvent = nullptr;
static std::atomic<u64> evUntil{0}; static std::atomic<int> evTag{0}; static int nHitProc = 0, nEvLog = 0;
static void dumpMsg(const char *tag, const u8 *m);   // below
// 0.18.53 (session 9: the script events at the victim are the same for both; Jin's attack side runs HitProc 0x8504D0(record, list) =
// compose the message from the hit RECORD (0x84B1A0 + the composer 0x850880: damage, guard, parry, reaction), the attack component's
// callbacks (events 0x11 / 0x14 / 0x12 / 0x15 on Jin's weapon entity; 0x870FF0 flags the target's +0x9C8 ring when the hero hit it),
// then the delivery): the katana now runs that WHOLE path with a copy of Jin's latest real hit record, the target swapped
// (cbt_mode 2). The record: +0x30 descriptor flags, +0x48 target, +0x50 -> the attack definition (+0x88 hash, +0xA4 move),
// +0x58 attacker, +0x60 / +0x78 the attack component (class 24), +0x80 the attack hash, +0x90 contact point / velocity / direction.
static int cbtMode = 2; static bool forceMsg = false;   // 0.18.56 Ctrl+Shift+M: one blow by the message path (compare)
// 0.18.55 the RECORD LIBRARY (session 11: Ctrl+Shift+H made enemies flinch with NO animation once the record came from attack
// 0x95611255ef4e58cc - the game composed flags 0x11C01041 (bit 0x10000000 = the hit reaction) - while session 10's record (attack
// 0x74bf02936aeeeb47, flags 0x03C00041) never did; and a finisher's record (0x03C00001, -FLT_MAX) taken as the template made every blow
// after it a kill). So a record is now kept only after the game has composed its blow: an ordinary damaging blow (0x41 set, no
// heal / revive / spawn bits, finite damage), light / heavy by 0x100, one with the reaction bit always preferred. A record is 0x120
// bytes: +0x00 vtable, +0x30 descriptor, +0x48 target, +0x50 -> holder of the attack definition (inside Jin's moveset entry),
// +0x58 attacker, +0x60 / +0x78 the attack component (class 24), +0x68 an attack object, +0x70 / +0x80 the attack hash, +0x90..
// contact, +0xE0 the base damage, +0xF0 / +0xF8 the tag list. Saved to gotvr.ini with how to find its pointers again from Jin, so
// VR sessions need no real blow (cbt_rec_light / cbt_rec_heavy).
struct RecT { u8 b[0x120]; u64 tags[32]; int nTags = 0; bool ok = false, live = false; u32 fl = 0; float dmg = 0; float tgt[3] = {0, 0, 0}; u8 *hero = nullptr;
    int compOff = -1, compOff2 = -1, o68Off = -1, o68InComp = 0, k50 = -1, defJ = -1, defMode = 0, defB = 0, defC = 0; u32 vtRva = 0; u64 cell = 0, defHash = 0; u8 def[0xB0]; bool defOk = false; };
static RecT recs[4]; static RecT cand;   // 0.18.60 slots: 0 light, 1 heavy (people), 2 light, 3 heavy (dogs - session 16)
static const wchar_t *const kRecKey[4] = {L"cbt_rec_light", L"cbt_rec_heavy", L"cbt_rec_light_dog", L"cbt_rec_heavy_dog"};
static const char *const kRecName[4] = {"light", "heavy", "dog light", "dog heavy"};
static int recFind(u8 *base, int size, u64 v) { if (!base || !readable(base, (size_t)size)) return -1; for (int o = 0; o + 8 <= size; o += 8) if (*(u64 *)(base + o) == v) return o; return -1; }
static u8 *movesetEntry(u8 *comp, u64 hash) {   // Jin's moveset: (comp+0x28)+0x350 entries of 0xE0, the attack hash first, +0x358 count
    if (!comp || !readable(comp + 0x28, 8)) return nullptr; u8 *o = *(u8 **)(comp + 0x28); if (!o || ((u64)o & 7) || !readable(o + 0x350, 0x10)) return nullptr;
    const int n = *(int *)(o + 0x358); u8 *p = *(u8 **)(o + 0x350); if (n <= 0 || n > 4096 || !p || !readable(p, (size_t)n * 0xE0)) return nullptr;
    for (int k = 0; k < n; k++) if (*(u64 *)(p + (size_t)k * 0xE0) == hash) return p + (size_t)k * 0xE0; return nullptr; }
// 0.18.57 (session 13: the definition is neither in the moveset entry nor in Jin / the component / the owner): where it can be reached
// from the moveset entry for the hash (e) or the component's per-attack array (+0x5B0, 0x58 each, same index as the moveset):
// mode 1 *(e+a), 2 *(*(e+a)+b), 3 *(arr[idx]+a), 4 *(*(arr[idx]+a)+b)
static u8 *rdP(u8 *p) { return p && !((u64)p & 7) && readable(p, 8) ? *(u8 **)p : nullptr; }
static u8 *attackArr(u8 *comp, u8 *e) {
    u8 *o = rdP(comp + 0x28); if (!o || !readable(o + 0x350, 0x10)) return nullptr; u8 *base = *(u8 **)(o + 0x350); const int n = *(int *)(o + 0x358);
    if (!base || e < base || ((e - base) % 0xE0) || (e - base) / 0xE0 >= n) return nullptr; u8 *arr = rdP(comp + 0x5B0); const long long idx = (e - base) / 0xE0;
    return arr && readable(arr + idx * 0x58, 0x58) ? arr + idx * 0x58 : nullptr; }
// 0.18.58 (session 14: not within one step either): a path of up to three pointer steps from the moveset entry (root 1, 0xE0) or
// the per-attack entry (root 2, 0x58): mode = root * 10 + steps; offsets a, b, c
static u8 *defRoot(u8 *comp, u8 *e, int root) { return root == 1 ? e : attackArr(comp, e); }
static u8 *defVia(u8 *comp, u8 *e, int mode, int a, int b, int c = 0) {
    u8 *p = defRoot(comp, e, mode / 10); const int steps = mode % 10; if (!p || steps < 1 || steps > 3) return nullptr;
    const int off[3] = {a, b, c}; for (int k = 0; k < steps; k++) { if (k && (!p || ((u64)p & 7))) return nullptr; p = rdP(p + off[k]); } return p; }
static bool defSearch(u8 *comp, u8 *e, u8 *def, int &mode, int &a, int &b, int &c) {
    for (int root = 1; root <= 2; root++) { u8 *r = defRoot(comp, e, root); const int rs = root == 1 ? 0xE0 : 0x58; if (!r || !readable(r, (size_t)rs)) continue;
        for (int x = 0; x < rs; x += 8) { u8 *p1 = *(u8 **)(r + x); if (p1 == def) { mode = root * 10 + 1; a = x; b = c = 0; return true; } }
        for (int x = 0; x < rs; x += 8) { u8 *p1 = *(u8 **)(r + x); if (!p1 || ((u64)p1 & 7) || !readable(p1, 0x200)) continue;
            for (int y = 0; y < 0x200; y += 8) { u8 *p2 = *(u8 **)(p1 + y); if (p2 == def) { mode = root * 10 + 2; a = x; b = y; c = 0; return true; } } }
        for (int x = 0; x < rs; x += 8) { u8 *p1 = *(u8 **)(r + x); if (!p1 || ((u64)p1 & 7) || !readable(p1, 0x200)) continue;
            for (int y = 0; y < 0x200; y += 8) { u8 *p2 = *(u8 **)(p1 + y); if (!p2 || ((u64)p2 & 7) || p2 == p1 || p2 == r || !readable(p2, 0x100)) continue;
                for (int z = 0; z < 0x100; z += 8) if (*(u8 **)(p2 + z) == def) { mode = root * 10 + 3; a = x; b = y; c = z; return true; } } } }
    return false; }
static bool heapPtr(u8 *p) { return p && !((u64)p & 7) && (u64)p > 0x10000000ull && (u64)p < 0x7FFFFFFFFFFFull && !(p >= g_base && p < g_base + 0x4000000); }
static void recLearn(RecT &R) {   // how to find the record's pointers again from Jin (needed for a record saved for a later session, e.g. VR)
    u8 *hero = R.hero, *comp = *(u8 **)(R.b + 0x60), *o68 = *(u8 **)(R.b + 0x68), *h50 = *(u8 **)(R.b + 0x50); const u64 hash = *(u64 *)(R.b + 0x80);
    u8 *def = h50 && readable(h50, 8) ? *(u8 **)h50 : nullptr;
    // the attack component: in Jin (hero+off) or one step away (hero+off -> object +off2) (0.18.55: not directly in Jin)
    R.compOff = recFind(hero, 0x8000, (u64)comp); R.compOff2 = -1;
    if (R.compOff < 0 && readable(hero, 0x8000)) for (int o = 0; o < 0x8000 && R.compOff < 0; o += 8) { u8 *p = *(u8 **)(hero + o); if (!heapPtr(p) || p == comp) continue; const int o2 = recFind(p, 0x800, (u64)comp); if (o2 >= 0) { R.compOff = o; R.compOff2 = o2; } }
    R.o68Off = recFind(hero, 0x8000, (u64)o68); R.o68InComp = 0;
    if (R.o68Off < 0 && o68) { R.o68Off = recFind(comp, 0x1000, (u64)o68); R.o68InComp = R.o68Off >= 0; }
    // the attack holder: only its first qword (the attack definition) is ever read (HitSend 0x850880, the composer 0x84B1A0), so a record
    // rebuilt later carries its own cell holding the definition, found in Jin's moveset entry for the attack's hash
    u8 *o = comp && readable(comp + 0x28, 8) ? *(u8 **)(comp + 0x28) : nullptr; const int mn = o && readable(o + 0x358, 4) ? *(int *)(o + 0x358) : -1;
    u8 *e = movesetEntry(comp, hash); R.k50 = e && h50 >= e && h50 < e + 0xE0 ? (int)(h50 - e) : -1; R.defJ = -1; R.defMode = 0; R.defB = 0; R.defC = 0; R.defHash = def && readable(def + 0x88, 8) ? *(u64 *)(def + 0x88) : 0;
    // 0.18.59 (session 15: the definition is not reachable within three steps of either entry - it lives in its own asset block, 0xB0 per
    // definition): the game reads only +0x88 (attack hash), +0x9C and +0xA4 (move) of it (HitSend 0x850880, the composer 0x84B1A0), so a
    // rebuilt record carries its own copy of those 0xB0 bytes
    R.defOk = def && readable(def, 0xB0); if (R.defOk) memcpy(R.def, def, 0xB0);
    if (e && def) { int m, a, b, c2; if (defSearch(comp, e, def, m, a, b, c2)) { R.defMode = m; R.defJ = a; R.defB = b; R.defC = c2; } }
    R.vtRva = *(u8 **)R.b >= g_base && *(u8 **)R.b < g_base + 0x4000000 ? (u32)(*(u8 **)R.b - g_base) : 0;
    const bool can = R.compOff >= 0 && (o68 == nullptr || R.o68Off >= 0) && (R.k50 >= 0 || R.defMode > 0 || R.defOk) && R.vtRva;
    logf("REC: finding it again from Jin: component at hero+0x%x%s%x, +0x68 at %s+0x%x, moveset %d attacks (entry %s), holder %p = entry+0x%x, definition %p (hash 0x%llx move %d) via path %d +0x%x +0x%x +0x%x%s - %s",
         R.compOff, R.compOff2 >= 0 ? " -> +0x" : " (direct)", R.compOff2 >= 0 ? R.compOff2 : 0, R.o68InComp ? "component" : "hero", R.o68Off, mn, e ? "found" : "NOT found", (void *)h50, R.k50, (void *)def,
         def && readable(def + 0x88, 0x20) ? (unsigned long long)*(u64 *)(def + 0x88) : 0ull, def && readable(def + 0xA4, 4) ? (*(int *)(def + 0xA4) << 20) >> 20 : -1, R.defMode, R.defJ, R.defB, R.defC, R.defMode <= 0 && R.defOk ? " (not found - its 0xB0 bytes are carried instead)" : "", can ? "SAVED for VR (no real blow needed next session)" : "NOT all found: a later session needs one real blow");
    if (!can && g_playFlat) {   // flat diagnostics: where the holder / definition / component do live (for the next build)
        auto nearP = [&](const char *nm, u8 *base, int size, u8 *tgtp) { if (!base || !readable(base, (size_t)size) || !tgtp) return; for (int q = 0; q < size; q += 8) { u8 *p = *(u8 **)(base + q); if (!heapPtr(p)) continue; const long long dlt = (long long)(tgtp - p); if (dlt >= 0 && dlt < 0x4000) logf("   REC near: %s+0x%x = %p, target %p is +0x%llx past it", nm, q, (void *)p, (void *)tgtp, dlt); } };
        { u8 *ar = e ? attackArr(comp, e) : nullptr; char b1[1000]; int q1 = 0; for (int x = 0; e && x < 0xE0 && q1 < 980; x += 8) q1 += snprintf(b1 + q1, sizeof b1 - (size_t)q1, " %llx", (unsigned long long)*(u64 *)(e + x));
          logf("   REC moveset entry %p:%s", (void *)e, b1); q1 = 0; for (int x = 0; ar && x < 0x58 && q1 < 980; x += 8) q1 += snprintf(b1 + q1, sizeof b1 - (size_t)q1, " %llx", (unsigned long long)*(u64 *)(ar + x));
          logf("   REC per-attack entry (component+0x5B0) %p:%s", (void *)ar, ar ? b1 : " (none)"); }
        nearP("component", comp, 0x1000, h50); nearP("moveset owner", o, 0x1000, h50); nearP("+0x68 object", o68, 0x800, h50);
        logf("   REC near: holder in hero +0x%x, component +0x%x, owner +0x%x; definition in hero +0x%x, component +0x%x, owner +0x%x", recFind(hero, 0x8000, (u64)h50), recFind(comp, 0x1000, (u64)h50), recFind(o, 0x1000, (u64)h50), recFind(hero, 0x8000, (u64)def), recFind(comp, 0x1000, (u64)def), recFind(o, 0x1000, (u64)def)); } }
static bool recResolve(RecT &R, u8 *hero, u8 *out) {   // the record ready to send (pointers current for this Jin)
    memcpy(out, R.b, 0x120);
    if (R.live && R.hero == hero) return true;   // Jin is the same object as at capture
    if (R.compOff < 0 || (R.k50 < 0 && R.defMode <= 0 && !R.defOk) || !R.vtRva || !readable(hero + R.compOff, 8)) return false;
    u8 *comp = *(u8 **)(hero + R.compOff); if (R.compOff2 >= 0) { if (!heapPtr(comp) || !readable(comp + R.compOff2, 8)) return false; comp = *(u8 **)(comp + R.compOff2); }
    if (!comp || ((u64)comp & 7) || !readable(comp, 0x30) || clsOf(comp) != 24) return false;
    u8 *e = movesetEntry(comp, *(u64 *)(R.b + 0x80)); if (!e) return false;
    u8 *h50 = R.k50 >= 0 ? e + R.k50 : nullptr;
    if (!h50 && R.defMode <= 0 && R.defOk) { R.cell = (u64)R.def; h50 = (u8 *)&R.cell; }   // 0.18.59 our own copy of the definition
    if (!h50) { u8 *def = R.defMode ? defVia(comp, e, R.defMode, R.defJ, R.defB, R.defC) : nullptr; if (!def) return false;
        if (R.defHash && (!readable(def + 0x88, 8) || *(u64 *)(def + 0x88) != R.defHash)) return false;   /* the same attack definition as at capture */ if (!heapPtr(def) && !(def >= g_base && def < g_base + 0x4000000)) return false; if (!readable(def + 0x88, 0x20)) return false; R.cell = (u64)def; h50 = (u8 *)&R.cell; }
    u8 *o68 = nullptr; if (R.o68Off >= 0) { u8 *src = R.o68InComp ? comp : hero; if (!readable(src + R.o68Off, 8)) return false; o68 = *(u8 **)(src + R.o68Off); }
    *(u8 **)out = g_base + R.vtRva; *(u8 **)(out + 0x50) = h50; *(u8 **)(out + 0x58) = hero; *(u8 **)(out + 0x60) = comp; *(u8 **)(out + 0x78) = comp; *(u8 **)(out + 0x68) = o68;
    return true; }
static void recSave(int kind, const RecT &R) {
    if (R.compOff < 0 || (R.k50 < 0 && R.defMode <= 0 && !R.defOk) || !R.vtRva || (R.o68Off < 0 && *(const u64 *)(R.b + 0x68))) return; static char h[4096]; int o = 0;
    o += snprintf(h + o, sizeof h - (size_t)o, "5;%d;%d;%d;%d;%d;%d;%d;%d;%d;%llx;%u;%08x;%g;%d;", R.compOff, R.compOff2, R.o68Off, R.o68InComp, R.k50, R.defMode, R.defJ, R.defB, R.defC, (unsigned long long)R.defHash, R.vtRva, R.fl, R.dmg, R.nTags);
    for (int k = 0; k < R.nTags; k++) o += snprintf(h + o, sizeof h - (size_t)o, "%016llx", (unsigned long long)R.tags[k]); h[o++] = ';';
    for (int k = 0; k < 0x120; k++) o += snprintf(h + o, sizeof h - (size_t)o, "%02x", R.b[k]); h[o++] = ';';
    for (int k = 0; R.defOk && k < 0xB0; k++) o += snprintf(h + o, sizeof h - (size_t)o, "%02x", R.def[k]); h[o] = 0;
    wchar_t w[4096]; MultiByteToWideChar(CP_UTF8, 0, h, -1, w, 4096); wchar_t ini[MAX_PATH]; swprintf(ini, MAX_PATH, L"%ls\\gotvr.ini", g_dir);
    WritePrivateProfileStringW(L"gotvr", kRecKey[kind], w, ini); }
static void recLoad(int kind) {
    wchar_t ini[MAX_PATH]; swprintf(ini, MAX_PATH, L"%ls\\gotvr.ini", g_dir); wchar_t w[4096]; if (!GetPrivateProfileStringW(L"gotvr", kRecKey[kind], L"", w, 4096, ini)) return;
    char h[4096]; WideCharToMultiByte(CP_UTF8, 0, w, -1, h, 4096, nullptr, nullptr); RecT R; int ver = 0, nt = 0; unsigned fl = 0; float dmg = 0; int pos = 0;
    unsigned long long dh = 0;
    if (sscanf(h, "%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%llx;%u;%x;%g;%d;%n", &ver, &R.compOff, &R.compOff2, &R.o68Off, &R.o68InComp, &R.k50, &R.defMode, &R.defJ, &R.defB, &R.defC, &dh, &R.vtRva, &fl, &dmg, &nt, &pos) < 15 || ver != 5 || nt < 0 || nt > 32) { logf("REC: saved %s record unreadable - ignored", kRecName[kind]); return; }
    const char *c = h + pos; for (int k = 0; k < nt; k++) { unsigned long long v; if (sscanf(c, "%16llx", &v) != 1) return; R.tags[k] = v; c += 16; } if (*c++ != ';') return;
    for (int k = 0; k < 0x120; k++) { unsigned v; if (sscanf(c + 2 * k, "%2x", &v) != 1) return; R.b[k] = (u8)v; }
    c += 0x240; R.defOk = false; if (*c == ';') { c++; bool okd = strlen(c) >= 0x160; for (int k = 0; okd && k < 0xB0; k++) { unsigned v; if (sscanf(c + 2 * k, "%2x", &v) != 1) okd = false; else R.def[k] = (u8)v; } R.defOk = okd; }
    if (R.defOk && dh && *(u64 *)(R.def + 0x88) != dh) R.defOk = false;
    R.defHash = dh; R.nTags = nt; R.fl = fl; R.dmg = dmg; R.ok = true; R.live = false; recs[kind] = R;
    logf("REC: saved %s record loaded (attack 0x%llx, flags 0x%08x%s, damage %.1f) - the katana's blows need no real blow this session", kRecName[kind], (unsigned long long)*(u64 *)(R.b + 0x80), fl, (fl & 0x10000000u) ? " with the hit reaction" : "", -dmg); }
static void recJudge() {   // after the game composed and delivered a real blow of Jin's: keep its record?
    if (!candSeen) return; const u32 fl = candFl; const float d = candDmg;
    const bool ordinary = (fl & 0x41u) == 0x41u && !(fl & 0x64000000u) && std::isfinite(d) && d < -0.5f && d > -5000.0f; if (!ordinary) return;
    u8 *tg = *(u8 **)(cand.b + 0x48); const bool dog = tg && readable(tg, 8) && fp::classId(tg) == 16;
    const int kind = ((fl & 0x100u) ? 1 : 0) + (dog ? 2 : 0); RecT &R = recs[kind]; const bool react = (fl & 0x10000000u) != 0, oldReact = R.ok && (R.fl & 0x10000000u);
    if (R.ok && oldReact && !react) return;   // never trade a reacting blow for one without the reaction
    if (R.ok && react == oldReact && fabsf(d) > fabsf(R.dmg) + 0.5f) return;   // 0.18.60 (session 16: "light" blows of 150 / 170 / 225 seen): the plainest blow of its kind
    const bool newKind = !R.ok || *(u64 *)(R.b + 0x80) != *(u64 *)(cand.b + 0x80) || react != oldReact || !R.live;
    const int keepC = R.compOff, keepC2 = R.compOff2, keepO = R.o68Off, keepI = R.o68InComp, keepK = R.k50, keepJ = R.defJ, keepM = R.defMode, keepB = R.defB, keepD = R.defC; const u64 keepH = R.defHash; const bool keepDO = R.defOk; static u8 keepDef[0xB0]; memcpy(keepDef, R.def, 0xB0); const u32 keepV = R.vtRva;
    R = cand; R.ok = true; R.live = true; R.fl = fl; R.dmg = d;
    if (newKind) { logf("REC: %s record from Jin's attack 0x%llx (flags 0x%08x - %s, damage %.1f)", kRecName[kind], (unsigned long long)*(u64 *)(R.b + 0x80), fl, react ? "WITH the hit reaction" : "no hit-reaction bit", -d);
        recLearn(R); recSave(kind, R); } else { R.compOff = keepC; R.compOff2 = keepC2; R.o68Off = keepO; R.o68InComp = keepI; R.k50 = keepK; R.defJ = keepJ; R.defMode = keepM; R.defB = keepB; R.defC = keepD; R.defHash = keepH; if (!R.defOk) { R.defOk = keepDO; memcpy(R.def, keepDef, 0xB0); } R.vtRva = keepV; }
}
// 0.18.54 (session 10: the game's WHOLE hit path on a copy of Jin's record - identical message, delivery and events - still no flinch).
// Remaining difference: a real blow lands while Jin's own attack is playing. FLAT-ONLY diagnostic to test that (never in VR, never
// pressing any button - owner: no attack animations in VR): Ctrl+Shift+G arms the swing test; YOUR next attack press (left mouse / pad X)
// is followed cbt_swing_test_ms later by the katana's blow on the nearest enemy while your swing plays, Jin's own blows held back.
static u64 swingUntil = 0;
static std::atomic<u64> assistUntil{0}; static u64 nHeldBack = 0; static int assistHold = 1;   // flat swing test only
// 0.18.61 block / deflect (parry.h; design doc: "Blocking is done by simply holding their sword still when in contact with an enemy weapon.
// Deflecting requires some velocity into the opponent's weapon"; variant 2 "no player animation" - Jin's upper body is held by the mask while
// the katana is out). When the game is about to apply an ENEMY's blow to Jin (its hit record in HitProc) and the katana was in the blow's
// path, the blow is held back, the game's own guard (LB) is pressed - freshly for a deflect, held for a block - and the very same blow is
// replayed a moment later, so the GAME decides block / parry / perfect parry / guard break, and the enemy reacts the game's way.
static u64 qpcMs() { static LARGE_INTEGER fq{}; if (!fq.QuadPart) QueryPerformanceFrequency(&fq); LARGE_INTEGER qc; QueryPerformanceCounter(&qc); return (u64)(qc.QuadPart * 1000 / fq.QuadPart); }
static parry::Cfg pcfg; static parry::Hist bladeHist; static u64 bladeHistAt = 0; static int parryBladeMode = 0; static int parryOn = 1, parryShow = 1, deflectDelayMs = 50, blockDelayMs = 300; static float parryRumble = 0.8f;
struct PPend { bool on = false; u8 b[0x120]; u64 tags[32]; int nt = 0, kind = 0; u64 at = 0; u8 *att = nullptr; parry::Result r; };
static PPend ppend; static u64 nParry[3] = {0, 0, 0}, nParryMissed = 0;
// 0.18.64 (session 19: "I still couldn't get parrying or blocking to trigger at all, even just holding my sword up"; log: 3 contacts judged,
// the game's guard never accepted in time - and while the blow was held back the SAME swing hit Jin again 31-63 ms later, so he took it
// twice). parry_engine = mod (default): the MOD decides - a blow that meets the blade is simply not applied to Jin (block), and a parry
// also staggers the attacker (one light blow of the record library, parry_stagger_damage x its damage) - no guard button, no game sword
// draw, no stance. parry_engine = game: the 0.18.61 route (the game's guard pressed, the blow replayed), its replay later now (130 ms).
// Both: every further contact of the same attacker's swing within parry_swallow_ms is swallowed too (one swing = one defence).
static int parryEngine = 1, swallowMs = 450; static float staggerMult = 0.15f; static u8 *swAtt = nullptr; static u64 swUntil = 0, nSwallowed = 0, nStaggers = 0;
static u8 *staggerAt = nullptr; static u64 staggerDue = 0;
// 0.18.62 (owner: "can you not put a hitbox on the player held sword for parrying? maybe have that as a testable option"): parry_mode=blade
// - the enemy's own weapon instead of the blow's line. People share Jin's rig, so the enemy's skeleton is found the way Jin's is (the same
// offsets from the character, world matrices at node+0x278, entry joint+1), and its weapon is the segment from its right hand along its
// forearm (elbow -> hand), parry_enemy_blade_cm long (the game's own weapon joint 637 is used for the start when it is in that hand).
// Your katana's hitbox is the blade capsule (guard -> tip, radius parry_blade_tolerance_cm) - the same in every mode.
static float enemyBladeCm = 90.0f; static u64 nBladeFound = 0, nBladeMissing = 0;
static bool enemyBlade(u8 *en, melee::V3 &a, melee::V3 &b) {
    if (fp::nodeOff < 0) return false; u8 *n = rdP(en + fp::nodeOff); if (n && fp::nodeOff2 >= 0) n = rdP(n + fp::nodeOff2);
    if (!n || !readable(n + 0x278, 0x10)) return false; const float *W = *(float *const *)(n + 0x278); const int cnt = *(const int *)(n + 0x280) - 1;
    const int hd = fp::armsD[1].hand, el = fp::armsD[1].el; if (!W || cnt < 50 || cnt > fp::JOINT_CAP || hd < 0 || el < 0 || hd >= cnt || el >= cnt || !readable(W, (size_t)(cnt + 1) * 64)) return false;
    const float *H = W + (size_t)(hd + 1) * 16 + 12, *E = W + (size_t)(el + 1) * 16 + 12; float ep[3]; if (!rdF3(en, ep)) return false;
    const float dx = H[0] - ep[0], dy = H[1] - ep[1], dz = H[2] - ep[2]; if (!(dx * dx + dy * dy < 150.0f * 150.0f && dz > 20.0f && dz < 230.0f)) return false;   // a hand of THIS character
    float d[3] = {H[0] - E[0], H[1] - E[1], H[2] - E[2]}; const float l = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]); if (!(l > 5.0f && l < 60.0f)) return false; for (auto &x : d) x /= l;
    melee::V3 st = melee::v3(H[0], H[1], H[2]);
    if (637 < cnt) { const float *G = W + (size_t)(637 + 1) * 16 + 12; const float gx = G[0] - H[0], gy = G[1] - H[1], gz = G[2] - H[2]; if (gx * gx + gy * gy + gz * gz < 25.0f * 25.0f) st = melee::v3(G[0], G[1], G[2]); }
    a = st; b = melee::v3(st.x + d[0] * enemyBladeCm, st.y + d[1] * enemyBladeCm, st.z + d[2] * enemyBladeCm); return true; }
// 0.18.65 (owner, session 20: "I couldn't tell if the parry was going through ... there are no sfx / vfx"): until the game's own block /
// parry effects can be had, the mod gives its own - a synthesized steel clang (a block: lower and short; a parry: brighter, ringing),
// generated here (no game files), played on a worker thread through Windows (PlaySound -> the default output = the headset with
// Virtual Desktop), fx_sound_volume (0.7; 0 = off); and a rim flash in the headset (GoTVRHost's fx layer).
namespace sfx {
static std::vector<u8> wav[3]; static HANDLE ev = nullptr; static std::atomic<int> want{0}; static float vol = 0.7f; static std::atomic<u64> nPlayed{0};
typedef BOOL (WINAPI *FnPS)(LPCWSTR, HMODULE, DWORD); static FnPS ps = nullptr;
static void synth(int k, float f0, float durS, float bright, float noiseAmt) {
    const int SR = 44100, N = (int)(SR * durS); std::vector<int16_t> pcm((size_t)N); const float rat[6] = {1.0f, 2.76f, 5.40f, 8.93f, 13.34f, 18.64f}, amp[6] = {1.0f, 0.62f, 0.45f, 0.32f, 0.2f, 0.12f};
    u32 rng = 0x9E3779B9u ^ (u32)k; float lp = 0.0f;
    for (int i = 0; i < N; i++) { const float t = (float)i / SR; float v = 0.0f;
        for (int p = 0; p < 6; p++) { const float dec = durS * (p == 0 ? 0.55f : 0.35f / (1.0f + p * bright)); v += amp[p] * (p > 1 ? bright : 1.0f) * expf(-t / dec) * sinf(6.2831853f * f0 * rat[p] * t + p * 1.3f); }
        rng = rng * 1664525u + 1013904223u; const float n = ((rng >> 9) & 0xFFFF) / 32768.0f - 1.0f; const float hp = n - lp; lp += 0.25f * (n - lp);   // a bright noise burst = the impact
        v += noiseAmt * hp * expf(-t / 0.012f);
        const float atk = t < 0.002f ? t / 0.002f : 1.0f; v *= 0.30f * atk; pcm[(size_t)i] = (int16_t)(fmaxf(-1.0f, fminf(1.0f, v)) * 32000.0f); }
    std::vector<u8> &w = wav[k]; const u32 data = (u32)N * 2; w.resize(44 + data); u8 *b = w.data();
    auto u32le = [&](int o, u32 x) { memcpy(b + o, &x, 4); }; auto u16le = [&](int o, uint16_t x) { memcpy(b + o, &x, 2); };
    memcpy(b, "RIFF", 4); u32le(4, 36 + data); memcpy(b + 8, "WAVEfmt ", 8); u32le(16, 16); u16le(20, 1); u16le(22, 1); u32le(24, SR); u32le(28, SR * 2); u16le(32, 2); u16le(34, 16); memcpy(b + 36, "data", 4); u32le(40, data);
    memcpy(b + 44, pcm.data(), data); }
static DWORD WINAPI worker(void *) { for (;;) { WaitForSingleObject(ev, INFINITE); const int k = want.exchange(0); if (k > 0 && k < 3 && ps && !wav[k].empty()) { ps((LPCWSTR)wav[k].data(), nullptr, 0x0004 /*SND_MEMORY*/ | 0x0001 /*SND_ASYNC*/ | 0x0002 /*SND_NODEFAULT*/); nPlayed++; } } }
static void init(float volume) {
    vol = volume; if (vol <= 0.0f || ev) return; HMODULE wm = LoadLibraryW(L"winmm.dll"); ps = wm ? (FnPS)(void *)GetProcAddress(wm, "PlaySoundW") : nullptr; if (!ps) { logf("combat feedback: no PlaySound - no clang"); return; }
    synth(1, 520.0f, 0.30f, 0.6f, 0.9f); synth(2, 1180.0f, 0.65f, 1.0f, 0.7f);   // 1 block (low, short), 2 parry (bright, ringing)
    for (int k = 1; k < 3; k++) { int16_t *p = (int16_t *)(wav[k].data() + 44); const size_t n = (wav[k].size() - 44) / 2; for (size_t i = 0; i < n; i++) p[i] = (int16_t)(p[i] * fminf(1.0f, vol)); }
    ev = CreateEventW(nullptr, FALSE, FALSE, nullptr); HANDLE th = CreateThread(nullptr, 0, worker, nullptr, 0, nullptr); if (th) CloseHandle(th);
    logf("combat feedback: block / parry clang ready (volume %.2f)", vol); }
static void play(int k) { if (!ev) return; want = k; SetEvent(ev); }
static void event(int kind, float amp) {   // 1 block, 2 parry: sound + the host's rim flash
    play(kind); if (S) { S->fxKind = (u32)kind; S->fxAmp = amp; MemoryBarrier(); InterlockedIncrement(&S->fxSeq); } }
}
static bool parryTry(u8 *rec) {   // game thread, inside HitProc, an enemy's blow on Jin: defend it?
    u8 *hero = fp::heroPtr(); if (!hero || !readable(rec, 0x120)) return false;
    u8 *tg = *(u8 **)(rec + 0x48), *at = *(u8 **)(rec + 0x58); if (tg != hero || !at || at == hero || ((u64)at & 7) || !readable(at, 8)) return false;
    if (at == swAtt && GetTickCount64() < swUntil) { nSwallowed++; return true; }   // 0.18.64 the same swing again: already defended
    if (ppend.on) return false;
    const int ca = fp::classId(at); if (ca != 14 && ca != 16) return false;
    regNote(at, true, GetTickCount64());
    const u64 qn = qpcMs(); if (qn - bladeHistAt > 150) return false;   // the katana is not out in your hand right now
    // 0.18.62 (session 18: both blocks "went through" - the game had just put its own sword away by itself 0.1 s before): a guard needs the
    // game's sword out; if it is away, the game is asked to draw it now (hidden on Jin) and the blow waits a little longer for it
    const bool gameOut = fp::gameDrawn.load() != 0;
    float ap[3], hp3[3]; if (!rdF3(at, ap) || !rdF3(hero, hp3)) return false;
    const melee::V3 A = melee::v3(ap[0], ap[1], ap[2] + (ca == 16 ? 50.0f : 130.0f));
    const float *L = (const float *)(rec + 0x90); melee::V3 P = melee::v3(hp3[0], hp3[1], hp3[2] + 120.0f);
    if (std::isfinite(L[0] + L[1] + L[2])) { const float dx = L[0] - hp3[0], dy = L[1] - hp3[1], dz = L[2] - hp3[2]; if (dx * dx + dy * dy + dz * dz < 300.0f * 300.0f) P = melee::v3(L[0], L[1], L[2]); }
    melee::V3 J0 = A, J1 = P; bool viaBlade = false;
    if (parryBladeMode) { if (ca == 14 && enemyBlade(at, J0, J1)) { viaBlade = true; nBladeFound++; } else { J0 = A; J1 = P; nBladeMissing++; } }
    const parry::Result r = parry::judge(bladeHist, qn, J0, J1, pcfg);
    if (parryBladeMode && parryShow) logf("PARRY: %s", viaBlade ? "the enemy's weapon found (hand + forearm) - blade against blade" : "the enemy's weapon not found - the blow's line instead");
    if (!r.kind) { if (parryShow && r.distCm < pcfg.tolCm * 2.5f) logf("PARRY: no contact - the blade passed %.0f cm from the blow's line (tolerance %.0f cm)", r.distCm, pcfg.tolCm); nParryMissed++; return false; }
    const int n = *(int *)(rec + 0xF8); u64 *tp = *(u64 **)(rec + 0xF0); if (n < 0 || n > 32 || (n && (!tp || !readable(tp, (size_t)n * 8)))) return false;
    if (parryEngine == 1) {   // 0.18.64 the mod's own defence
        const u64 t = GetTickCount64(); swAtt = at; swUntil = t + (u64)swallowMs; nParry[r.kind]++;
        const bool par = r.kind == parry::DEFLECT; if (par) { staggerAt = at; staggerDue = t; }
        vrin::pulse(1, fminf(1.0f, parryRumble * (par ? 1.0f : 0.7f)), par ? 120 : 70); vrin::pulse(0, fminf(1.0f, parryRumble * (par ? 0.8f : 0.5f)), par ? 120 : 70);
        sfx::event(par ? 2 : 1, par ? 1.0f : 0.7f);
        if (parryShow) logf("PARRY: contact %.0f cm from the %s, the blade in it for %.0f ms before the blow -> %s by the mod (the blow not applied to Jin%s; its swing ignored for %d ms)",
                            r.distCm, viaBlade ? "enemy's weapon" : "blow's line", r.enteredMs, par ? "PARRY" : "BLOCK", par ? ", the attacker staggered" : "", swallowMs);
        return true; }
    // 0.18.63 (review): the draw is a toggle - asked only for a blow that met the blade, and never within 2.5 s of the last draw request
    if (!gameOut) { const u64 t = GetTickCount64(); if (t - g_drawChordAt.load() > 2500) { vrin::requestChord(vrin::XB_RIGHT | vrin::OX_RSR); g_drawChordAt = t; }
                    const u64 mu = t + 3000; if (fp::maskUntil.load() < mu) fp::maskUntil = mu; }
    memcpy(ppend.b, rec, 0x120); if (n) memcpy(ppend.tags, tp, (size_t)n * 8); ppend.nt = n; ppend.kind = r.kind; ppend.att = at; ppend.r = r;
    swAtt = at; swUntil = GetTickCount64() + (u64)swallowMs;
    const u64 now = GetTickCount64(); ppend.at = now + (u64)(r.kind == parry::DEFLECT ? deflectDelayMs : blockDelayMs) + (gameOut ? 0u : 450u); ppend.on = true; nParry[r.kind]++;
    // the game's own guard: a PARRY gets a fresh press just before the blow (the game's parry timing), a BLOCK a guard held well before it;
    // 0.18.63: a guard still held from a block is let go for 50 ms first so the parry is a new press (the blow waits as long)
    const u32 gms = (r.kind == parry::DEFLECT ? 160u : (u32)blockDelayMs + 250u) + (gameOut ? 0u : 450u);
    if (r.kind == parry::DEFLECT) ppend.at += vrin::forcePressFresh(vrin::XB_LB, gms, 50); else vrin::forcePress(vrin::XB_LB, gms);
    if (parryShow) logf("PARRY: contact %.0f cm from the blow's line, the blade in it for %.0f ms before the blow (tip %.1f m/s over %.0f cm) -> %s requested (the game's guard pressed, the blow replayed in %d ms)", r.distCm, r.enteredMs, r.speed, r.travelCm,
                        r.kind == parry::DEFLECT ? "PARRY" : "BLOCK", r.kind == parry::DEFLECT ? deflectDelayMs : blockDelayMs);
    return true; }
static bool strike(int i, float mult, bool kill, int kind, const char *src, const float *ctc);
static void parryTick() {   // game thread: replay the held-back blow once the guard is in
    if (staggerAt && GetTickCount64() >= staggerDue) {   // 0.18.64 mod engine: the parried attacker staggers (outside the game's hit processing)
        u8 *a = staggerAt; staggerAt = nullptr; int i = -1; for (int k = 0; k < NR; k++) if (reg[k] == a) { i = k; break; }
        if (i >= 0 && alive(a) && staggerMult > 0.0f) { nStaggers++; strike(i, staggerMult, false, 0, "parry stagger", nullptr); } }
    if (!ppend.on || GetTickCount64() < ppend.at) return; ppend.on = false;
    u8 *at = ppend.att; const u8 *b = ppend.b; u8 *c60 = *(u8 *const *)(b + 0x60), *c78 = *(u8 *const *)(b + 0x78), *h50 = *(u8 *const *)(b + 0x50);
    auto vtOk = [](u8 *o, u32 slot) { return !o || (readable(o, 8) && readable(*(u8 **)o, slot + 8)); };
    const bool ok = at && readable(at, 8) && (fp::classId(at) == 14 || fp::classId(at) == 16) && vtOk(c60, 0xB0) && vtOk(c78, 0xC8) && (!h50 || (readable(h50, 8) && readable(*(u8 **)h50, 0xA8)))
                    && *(u8 **)(g_base + 0x34C34F8) != nullptr && oHitProc && fp::heroPtr() && *(u8 *const *)(b + 0x48) == fp::heroPtr();
    if (!ok) { logf("PARRY: the held-back blow could not be replayed (its attacker or data is gone) - dropped"); return; }
    alignas(16) static u8 rec[0x120]; memcpy(rec, b, 0x120); *(int *)(rec + 8) = 0x40000000; *(u64 **)(rec + 0xF0) = ppend.tags; *(int *)(rec + 0xF8) = ppend.nt;
    struct Lst { u64 *p; int n, pad; int f, cap; u64 z; } lst{}; u8 *hero = fp::heroPtr(); const float h0 = rdF(hero + 0x1974);
    parryReplay = true; parrySeen = false; oHitProc(rec, (u8 *)&lst); parryReplay = false; const float h1 = rdF(hero + 0x1974);
    const bool held = h1 >= h0 - 0.01f;
    if (held) { vrin::pulse(1, fminf(1.0f, parryRumble), 80); vrin::pulse(0, fminf(1.0f, parryRumble * 0.8f), 80); }
    if (parryShow) logf("PARRY result: %s requested -> the game composed flags 0x%08x (%s), Jin's health %.1f -> %.1f%s", ppend.kind == parry::DEFLECT ? "PARRY" : "BLOCK", parrySeen ? parryFl : 0u,
                        !parrySeen ? "no blow reached Jin" : parryFl == 1u ? "PARRIED" : held ? "guarded" : "it went through", h0, h1, held ? "" : " (the game did not accept the guard - too late, or an unblockable attack)"); }
static void hHitProc(u8 *rec, u8 *list);
static void hHitProc(u8 *rec, u8 *list) {
    if (!injecting && !parryReplay && parryOn && g_cfg.xr == 4 && rec && GetCurrentThreadId() == t_tick.load() && parryTry(rec)) { *(int *)(rec + 0x38) = 3; return; }   // 0.18.61 held back: block / deflect
    const bool hold = !injecting && assistHold && GetTickCount64() < assistUntil.load() && rec && readable(rec, 0x60) && *(u8 **)(rec + 0x58) == fp::heroPtr() && *(u8 **)(rec + 0x48) != fp::heroPtr();
    if (!injecting && rec && readable(rec, 0x210) && *(u8 **)(rec + 0x58) == fp::heroPtr() && *(u8 **)(rec + 0x48) && *(u8 **)(rec + 0x48) != fp::heroPtr()
        && readable(*(u8 **)(rec + 0x48), 8) && (fp::classId(*(u8 **)(rec + 0x48)) == 14 || fp::classId(*(u8 **)(rec + 0x48)) == 16) && GetCurrentThreadId() == t_tick.load()) {   // Jin's own blow on a person
        // the descriptor's tag list (+0xF0 array / +0xF8 count, read by HitSend and copied by the composer) is heap memory owned by
        // the real record: keep our own copy, so the template never points at memory the game has freed
        const int n = *(int *)(rec + 0xF8); u64 *tp = *(u64 **)(rec + 0xF0);
        if (!hold && n >= 0 && n <= 32 && (!n || (tp && readable(tp, n * 8)))) { if (n) memcpy(cand.tags, tp, n * 8); cand.nTags = n;
        memcpy(cand.b, rec, 0x120); const float *M = (const float *)(*(u8 **)(rec + 0x48) + 0x150); cand.tgt[0] = M[12]; cand.tgt[1] = M[13]; cand.tgt[2] = M[14]; cand.hero = fp::heroPtr();
        candOn = true; candSeen = false;
        } }
    if (hold) { nHeldBack++; if (nHeldBack <= 40) logf("ASSIST: Jin's own blow on %p held back (the katana's blow replaces it)", (void *)*(u8 **)(rec + 0x48)); *(int *)(rec + 0x38) = 3; return; }
    if (g_playFlat && rec && readable(rec, 0x100) && *(u8 **)(rec + 0x48)) {
        evUntil = GetTickCount64() + 400; evTag = 1;
        if (nHitProc < 6) { nHitProc++; auto cb = [&](u32 co, u32 slot) -> unsigned long long { u8 *c = *(u8 **)(rec + co); if (!c || !readable(c, 8)) return 0; u8 *vt = *(u8 **)c; if (!vt || !readable(vt + slot, 8)) return 0; u8 *f = *(u8 **)(vt + slot); return f >= g_base && f < g_base + 0x4000000 ? (unsigned long long)(f - g_base) : 0; };
            logf("HITPROC: record %p target %p (class %d) | +60 callbacks a8 0x%llx b0 0x%llx | +78 callbacks c0 0x%llx c8 0x%llx | +58 %p +60 %p +78 %p", (void *)rec, (void *)*(u8 **)(rec + 0x48), fp::classId(*(u8 **)(rec + 0x48)), cb(0x60, 0xA8), cb(0x60, 0xB0), cb(0x78, 0xC0), cb(0x78, 0xC8), (void *)*(u8 **)(rec + 0x58), (void *)*(u8 **)(rec + 0x60), (void *)*(u8 **)(rec + 0x78));
            dumpMsg("hitRecord", rec); callers("HitProc (a real blow, the attack side)"); } }
    oHitProc(rec, list);
    if (candOn) { candOn = false; recJudge(); }
}
static void *hEvent(u8 *obj, int id, u32 a, u32 b, u64 c, u64 d) {
    if (g_playFlat && nEvLog < 300 && GetTickCount64() < evUntil.load() && obj && readable(obj, 8) && fp::classId(obj) >= 14 && fp::classId(obj) <= 16) {
        nEvLog++; const u8 *ra = (const u8 *)__builtin_return_address(0);
        logf("EVENT %s: id 0x%x (%d, %u, 0x%llx) on %p (class %d) from rva 0x%llx", injecting ? "ours" : (evTag.load() == 2 ? "after ours" : "real"), id, (int)a, b, (unsigned long long)c, (void *)obj, fp::classId(obj), (unsigned long long)(ra >= g_base ? ra - g_base : 0)); }
    return oEvent(obj, id, a, b, c, d);
}
static void hDeliver1(u8 *m) {
    u8 *tg = m && readable(m + 0x48, 8) ? *(u8 **)(m + 0x30) : nullptr, *at = m ? *(u8 **)(m + 0x48) : nullptr;
    const bool want = g_playFlat && tg && at == fp::heroPtr() && tg != at && readable(tg, sizeof snapD) && fp::classId(tg) == 14 && (injecting ? nDlvOurs < 8 : nDlvReal < 8);
    if (want) memcpy(snapD, tg, sizeof snapD);
    // 0.18.54 (session 10: only +0x00..+0xAF of the message was compared - identical): the WHOLE 0x210-byte message as delivered, and
    // for ours every qword that differs from Jin's latest real one (HitSend 0x850880 reads Jin's animation state while composing)
    if (want && readable(m, 0x210)) { static u8 lastReal[0x210]; static bool haveReal = false; dumpMsg(injecting ? "delivered ours" : "delivered real", m);
        if (!injecting) { memcpy(lastReal, m, 0x210); haveReal = true; }
        else if (haveReal) { char b[1500]; int o = 0, n = 0; for (int q = 0; q < 0x210; q += 8) { u64 a, c; memcpy(&a, lastReal + q, 8); memcpy(&c, m + q, 8); if (a == c) continue; n++; if (o < 1420) o += snprintf(b + o, sizeof b - (size_t)o, " +%03x:%llx>%llx", q, (unsigned long long)a, (unsigned long long)c); }
            logf("   MSGDIFF real -> ours: %d qwords differ:%s", n, b); } }
    oDeliver1(m);
    if (want && readable(tg, sizeof snapD)) { (injecting ? nDlvOurs : nDlvReal)++; char b[1500]; int o = 0, n = 0;
        for (int q = 0; q < (int)sizeof snapD; q += 4) { u32 a, c; memcpy(&a, snapD + q, 4); memcpy(&c, tg + q, 4); if (a == c) continue; n++; if (o < 1440) o += snprintf(b + o, sizeof b - (size_t)o, " +%04x:%x>%x", q, a, c); }
        logf("   DLVDIFF %s: the delivery changed %d dwords on %p (flags 0x%08x):%s", injecting ? "ours" : "real", n, (void *)tg, *(u32 *)(m + 0x90), b); }
}
static std::atomic<int> hkReq{0}; static u64 nStrikes = 0, nKills = 0, nRefused = 0; static float secPeak = 0;
static u64 lastRefuseLog = 0;
static bool loudRefuse = false;   // 0.18.48: a hotkey's refusal is always logged (session 5: throttled refusals looked like silent key presses)
static void refuse(const char *why) { nRefused++; const u64 now = GetTickCount64(); if (loudRefuse || now - lastRefuseLog > 2000) { lastRefuseLog = now; logf("STRIKE refused: %s", why); } }
// 0.18.48 diagnostics for the missing hit reaction (session 5: on a real hit the enemy's +0x1694 / +0x1698 / +0x16A4 are set, on ours
// not): in flat play the first real blows of Jin and the first injected ones log what HandleHit changed on the enemy (16 KB before /
// after), the whole 0x210-byte message, and the real blow's caller chain (the attack side, to decompile)
static u8 snapE[0x4000]; static int nDiffReal = 0, nDiffOurs = 0;
static void dumpMsg(const char *tag, const u8 *m) { if (!readable(m, 0x210)) return; for (int r = 0; r < 0x210; r += 0x80) { char b[600]; int o = 0; for (int q = r; q < r + 0x80 && q < 0x210 && o < 560; q += 8) o += snprintf(b + o, sizeof b - (size_t)o, " %016llx", (unsigned long long)*(const u64 *)(m + q)); logf("   MSG %s +%03x:%s", tag, r, b); } }
static void diffLog(const char *tag, const u8 *en) { if (!readable(en, sizeof snapE)) return; char b[1400]; int o = 0, n = 0;
    for (int q = 0; q < (int)sizeof snapE; q += 4) { u32 a, c; memcpy(&a, snapE + q, 4); memcpy(&c, en + q, 4); if (a == c) continue; n++; if (o < 1340) o += snprintf(b + o, sizeof b - (size_t)o, " +%04x:%x>%x", q, a, c); }
    logf("   HHDIFF %s: HandleHit changed %d dwords on %p:%s", tag, n, (void *)en, b); }
static bool diffWant(u8 *ch, u8 *d) { return !injecting && g_playFlat && nDiffReal < 6 && d && readable(d + 0x48, 8) && *(u8 **)(d + 0x48) == fp::heroPtr() && ch != fp::heroPtr() && clsOf(ch) == 14 && GetCurrentThreadId() == t_tick.load(); }
static void diffReal(u8 *ch, u8 *d, bool before) {
    if (before) { if (readable(ch, sizeof snapE)) memcpy(snapE, ch, sizeof snapE); dumpMsg("real", d); callers("Jin's own blow (the attack side)"); return; }
    nDiffReal++; diffLog("real", ch); }
static int msgState = 1;   // 0.18.48: 1 = the template's +0x28 (3, as on every real message) while HandleHit runs, 0 restored before the release
// one blow on registry entry i: mult scales the template's health / posture change; kill forces a killing blow
static bool strike(int i, float mult, bool kill, int kind, const char *src, const float *ctc = nullptr) {   // ctc: the blade's contact point + tip velocity (cm, cm/s), VR
    if (!strikeOk) { refuse("off (a signature did not match this build)"); return false; }
    if (nOnTick.load() == 0 || nOffTick.load() > 0) { refuse(nOffTick.load() ? "the game's hits run off the game thread - not safe" : "no game hit seen yet on the game thread"); return false; }
    { static bool loaded = false; if (!loaded && cbtMode == 2) { loaded = true; for (int k = 0; k < 4; k++) if (!recs[k].ok) recLoad(k); } }
    const bool recAny = cbtMode == 2 && oHitProc && !forceMsg && (recs[0].ok || recs[1].ok || recs[2].ok || recs[3].ok);
    if (kind && !tplOk[kind] && !recAny) { kind = 0; mult *= 1.6f; }   // no heavy template yet: a light blow 1.6x
    if (!tplOk[kind] && !recAny) { refuse("no hit template yet - land one ordinary blow with the game's own attack first (flat, or VR before drawing)"); return false; }
    u8 *hero = fp::heroPtr(), *en = i >= 0 && i < NR ? reg[i] : nullptr; if (!hero || !en || !alive(en)) { refuse("the enemy is gone"); return false; }
    if (!hasHandleHit(en)) { refuse("this character does not use the known HandleHit"); return false; }
    if (rdF(hero + 0x1974) <= 0.0f) { refuse("Jin is down"); return false; }
    if (cbtMode == 2 && oHitProc && !forceMsg) {   // 0.18.53 / 0.18.55 the game's whole hit path (composer, attack callbacks, delivery) on a record from the library
        // 0.18.60 the record for this target: a dog's own record if there is one, else the people's; heavy falls back to light x1.6
        const bool dogT = clsOf(en) == 16; float rm = mult; int rk = (kind ? 1 : 0) + (dogT ? 2 : 0);
        if (!recs[rk].ok && dogT) rk -= 2; if (rk % 2 && !recs[rk].ok) { rk -= 1; rm *= 1.6f; if (dogT && !recs[rk].ok && recs[2].ok) rk = 2; }
        if (!recs[rk].ok) { for (int k : {0, 2, 1, 3}) if (recs[k].ok) { rk = k; break; } }
        alignas(16) static u8 rec[0x120];
        if (!(*(u8 *)(en + 0xE2) & 4)) { static u64 lastE = 0; if (GetTickCount64() - lastE > 2000) { lastE = GetTickCount64(); logf("STRIKE: enemy %p does not take hits right now (+0xE2 bit 4 clear - HitProc would skip it)", (void *)en); } return false; }
        const bool poolOk = *(u8 **)(g_base + 0x34C34F8) != nullptr;   // HitProc's own pool pop has no empty check
        RecT &R = recs[rk];
        if (R.ok && poolOk && recResolve(R, hero, rec)) {
            u8 *at50 = *(u8 **)(rec + 0x50), *comp = *(u8 **)(rec + 0x60), *c78 = *(u8 **)(rec + 0x78);
            if (at50 && readable(at50, 8) && *(u8 **)at50 && readable(*(u8 **)at50, 0xA8) && comp && readable(comp, 8) && clsOf(comp) == 24 && (!c78 || (readable(c78, 8) && readable(*(u8 **)c78, 0xD0)))) {
                *(int *)(rec + 8) = 0x40000000;   // never freed by anyone
                *(u64 **)(rec + 0xF0) = R.tags; *(int *)(rec + 0xF8) = R.nTags;   // our own copy of the tag list
                *(u8 **)(rec + 0x48) = en; *(u8 **)(rec + 0x58) = hero; *(int *)(rec + 0x38) = 2;
                float *bd = (float *)(rec + 0xE0); if (kill) *bd = 1.0e7f; else if (std::isfinite(rm) && rm > 0) *bd *= rm;   // the base damage: swing speed, two hands, heavy fallback
                if (ctc && std::isfinite(ctc[0] + ctc[1] + ctc[2] + ctc[3] + ctc[4] + ctc[5])) {   // 0.18.58 VR: where and which way the blade really cut
                    float *v = (float *)(rec + 0x90); v[0] = ctc[0]; v[1] = ctc[1]; v[2] = ctc[2];   // +0x90 the contact point
                    float *w = (float *)(rec + 0xA0); w[0] = ctc[3]; w[1] = ctc[4]; w[2] = ctc[5];   // +0xA0 the blade's velocity (cm/s)
                    const float sp = sqrtf(ctc[3] * ctc[3] + ctc[4] * ctc[4] + ctc[5] * ctc[5]); if (sp > 1.0f) { float *dd = (float *)(rec + 0xB0); dd[0] = ctc[3] / sp; dd[1] = ctc[4] / sp; dd[2] = ctc[5] / sp; }   // +0xB0 its direction
                } else { float ep[3]; if (rdF3(en, ep) > 0) { float *v = (float *)(rec + 0x90); const float dx = v[0] - R.tgt[0], dy = v[1] - R.tgt[1], dz = v[2] - R.tgt[2];
                    if (R.live && dx * dx + dy * dy + dz * dz < 500.0f * 500.0f) { v[0] = ep[0] + dx; v[1] = ep[1] + dy; v[2] = ep[2] + dz; } else { v[0] = ep[0]; v[1] = ep[1]; v[2] = ep[2] + 120.0f; } } }
                struct Lst { u64 *p; int n, pad; int f, cap; u64 z; } lst{}; const float h0 = rdF(en + 0x1974);
                evUntil = GetTickCount64() + 400; evTag = 2; injecting = true; oHitProc(rec, (u8 *)&lst); injecting = false;
                const float h1 = rdF(en + 0x1974); nStrikes++; if (h1 <= 0.0f && h0 > 0.0f) nKills++;
                logf("STRIKE %s (game hit path, %s record 0x%llx%s, x%.2f) -> %p (class %d): health %.1f -> %.1f%s", src, kRecName[rk], (unsigned long long)*(u64 *)(rec + 0x80), (R.fl & 0x10000000u) ? " with the hit reaction" : " NO hit-reaction bit",
                     kill ? 0.0f : rm, (void *)en, clsOf(en), h0, h1, h1 == h0 ? " (no damage - guarded, parried or refused by the game)" : "");
                return true; } }
        if (!poolOk) logf("STRIKE: the game's hit-message pool is empty - this blow takes the message path");
        else if (!R.ok) { static u64 lastN = 0; if (GetTickCount64() - lastN > 5000) { lastN = GetTickCount64(); logf("STRIKE: no hit record yet (no saved one either) - this blow takes the message path (damage only, no reaction); land one ordinary blow with the game's attack once"); } }
        else { logf("STRIKE: the %s record's pointers could not be found for this Jin - message path; land one ordinary blow to refresh it", kRecName[rk]); R.ok = R.live ? false : R.ok; } }
    if (!tplOk[kind]) { if (kind && tplOk[0]) { kind = 0; mult *= 1.6f; } else { refuse("no hit template for the message path yet - land one ordinary blow with the game's attack"); return false; } }
    const Tpl &T = tpl[kind]; u8 *m = pPool(); if (!m) { refuse("the game's hit-message pool is empty"); return false; }
    pCtor(m, en); *(int *)(m + 8) += 1;   // our reference (released below - the game's own destructor returns it to the pool)
    *(u64 *)(m + 0x38) = T.h38; *(u32 *)(m + 0x40) = T.m40; pSetH((u8 **)(m + 0x48), hero); { u8 *w = wk50[kind] ? wk50[kind] : wkAny50; if (w) pSetH((u8 **)(m + 0x50), w); }
    struct Arr { const u64 *p; int n, pad; }; const Arr a58{T.t58, (int)T.n58, 0}, a78{T.t78, (int)T.n78, 0}; pTags(m + 0x58, &a58); pTags(m + 0x78, &a78);
    memcpy(m + 0x90, T.r90, 0x20); memcpy(m + 0x170, T.r170, 0x40); *(u32 *)(m + 0x90) &= ~0x10000000u;   // 0.18.57 never the hit-reaction bit here (session 13 crash)
    // 0.18.50 (session 7: our message had +0x168 = 0 and flag 0x800000 cleared; every real blow of Jin's carries +0x168 = Jin and +0x1A0 =
    // its combo step 1 / 2): the attacker handle is always Jin, the combo step at least 1
    u32 fl = *(u32 *)(m + 0x90); if (fl & 0x800000u) pSetH((u8 **)(m + 0x168), hero); else if (T.flags & 0x800000u) { fl |= 0x800000u; pSetH((u8 **)(m + 0x168), hero); }
    if ((fl & 0x02000000u) && *(u32 *)(m + 0x1A0) == 0) *(u32 *)(m + 0x1A0) = 1;
    float ep[3], hp3[3]; const bool epOk = rdF3(en, ep) > 0, hpOk = rdF3(hero, hp3) > 0;
    for (int g = 0; g < 3 && (fl & 0x01000000u); g++) { float *v = (float *)(m + 0x170 + g * 16);   // positions in the payload follow the target / Jin
        const float dt2 = (v[0] - T.tpos[0]) * (v[0] - T.tpos[0]) + (v[1] - T.tpos[1]) * (v[1] - T.tpos[1]) + (v[2] - T.tpos[2]) * (v[2] - T.tpos[2]);
        const float dh2 = (v[0] - T.hpos[0]) * (v[0] - T.hpos[0]) + (v[1] - T.hpos[1]) * (v[1] - T.hpos[1]) + (v[2] - T.hpos[2]) * (v[2] - T.hpos[2]);
        if (epOk && dt2 < 500.0f * 500.0f && dt2 <= dh2) for (int c = 0; c < 3; c++) v[c] = ep[c] + (v[c] - T.tpos[c]); else if (hpOk && dh2 < 500.0f * 500.0f) for (int c = 0; c < 3; c++) v[c] = hp3[c] + (v[c] - T.hpos[c]); }
    const float h0 = rdF(en + 0x1974); float dmg, post; memcpy(&dmg, m + 0x94, 4); memcpy(&post, m + 0x98, 4); dmg *= mult; post *= mult;
    const bool lethal = kill || h0 + dmg <= 0.0f; if (lethal) { fl &= ~0x40u; dmg = -FLT_MAX; }
    *(u32 *)(m + 0x90) = fl; memcpy(m + 0x94, &dmg, 4); memcpy(m + 0x98, &post, 4);
    const bool viaGame = pDeliver && deliverOn;
    const u32 hid = *(u32 *)(m + 0x2C), st0 = *(u32 *)(m + 0x28); if (msgState && !viaGame) *(u32 *)(m + 0x28) = T.v28;
    const bool dd = g_playFlat && nDiffOurs < 6 && readable(en, sizeof snapE); if (dd) { memcpy(snapE, en, sizeof snapE); dumpMsg("ours", m); }
    evUntil = GetTickCount64() + 400; evTag = 2; injecting = true; if (viaGame) { *(int *)(m + 0x28) = 2; pDeliver(m); } else oHandleHit(en, m); injecting = false;   // 0.18.49: the game's own delivery (reaction, sound, blood)
    if (dd) { nDiffOurs++; diffLog(viaGame ? "ours (game delivery)" : "ours", en); }
    if (!viaGame) *(u32 *)(m + 0x28) = st0;
    const float h1 = rdF(en + 0x1974); const int rc = --*(int *)(m + 8); if (rc == 0) (*(void (**)(u8 *, u64))(*(u8 **)m + 0x10))(m, 1);   // the game's deleting destructor: back to its pool
    nStrikes++; if (lethal) nKills++;
    logf("STRIKE %s (%s) -> %p (class %d, %s): health %.1f -> %.1f (x%.2f%s), flags 0x%08x, id %u%s", src, kTplName[kind], (void *)en, clsOf(en), regHostile[i] ? "hostile (in a hit with Jin)" : "hostile (attitude)", h0, h1, mult, lethal ? ", killing blow" : "", fl, hid, viaGame ? " [game delivery]" : (rc ? " (the game kept the message)" : ""));
    return true;
}
static int nearest(float maxCm) { u8 *hero = fp::heroPtr(); float hp3[3]; if (!hero || !rdF3(hero, hp3)) return -1; int best = -1; float bd = maxCm * maxCm;
    for (int i = 0; i < NR; i++) { if (!reg[i] || !alive(reg[i]) || !hostile(i)) continue; float p[3]; if (!rdF3(reg[i], p)) continue;
        const float d2 = (p[0] - hp3[0]) * (p[0] - hp3[0]) + (p[1] - hp3[1]) * (p[1] - hp3[1]) + (p[2] - hp3[2]) * (p[2] - hp3[2]); if (d2 < bd) { bd = d2; best = i; } } return best; }
// the VR blade (game thread, swd::frame): capsules of the enemies in reach, the decision core, hits queued for after the tick
static melee::Core core; struct Req { int i; float mult; u8 kill, kind; float ctc[6]; bool hasC; }; static Req q[8]; static int qn = 0;
static u32 secHits = 0; static u64 lastHeroMs = 0; static float lastHero[3] = {0, 0, 0};
// returns 1 = a blow landed (strong rumble), 2 = the blade touched an enemy too slowly (a light touch), 0 = nothing
static float parryHiltCm = 30.0f, parryTipCm = 12.0f;   // 0.18.65 the parry hitbox beyond the blade: below the guard (hilt + hands) / past the tip
static int vrBlade(const float *guardW, const float *tipW, bool twoHands, bool heavy, float *hitPos, float *hitSpeed) {
    {   // 0.18.65 (owner, session 20: "make sure the hitbox extends the entire blade and below the hilt as well, with good margin for
        // error, and a little past the blade's tip"): the parry history keeps the WHOLE sword - parry_hilt_extend_cm below the guard
        // (the hilt and your hands) to parry_tip_extend_cm past the tip - recorded before any strike gate (records not loaded yet, no hit
        // switch) so a guard works whenever the katana is in your hand; parry_blade_tolerance_cm (40) is the radius around it
        const melee::V3 g0 = melee::v3(guardW[0], guardW[1], guardW[2]), t0 = melee::v3(tipW[0], tipW[1], tipW[2]); const melee::V3 dv = melee::sub(t0, g0); const float L = melee::len(dv);
        if (L > 1.0f && std::isfinite(L)) { const melee::V3 u = melee::mul(dv, 1.0f / L); const u64 qb = qpcMs();
            if (qb - bladeHistAt >= 8) { bladeHist.push(melee::sub(g0, melee::mul(u, parryHiltCm)), melee::add(t0, melee::mul(u, parryTipCm)), qb); bladeHistAt = qb; } } }
    if (!hitOn || !strikeOk) return 0; u8 *hero = fp::heroPtr(); float hp3[3]; if (!hero || !rdF3(hero, hp3)) return 0;
    LARGE_INTEGER fq, qc; QueryPerformanceFrequency(&fq); QueryPerformanceCounter(&qc); const u64 now = (u64)(qc.QuadPart * 1000 / fq.QuadPart);   // our own monotonic clock
    melee::V3 sv; if (lastHeroMs && now > lastHeroMs && now - lastHeroMs < 100) { const float s = 1000.0f / (float)(now - lastHeroMs); sv = melee::v3((hp3[0] - lastHero[0]) * s, (hp3[1] - lastHero[1]) * s, (hp3[2] - lastHero[2]) * s); }
    lastHeroMs = now; memcpy(lastHero, hp3, 12);
    static float lastTipV[3]; static u64 lastTipMs = 0; float tipVel[3] = {0, 0, 0}; bool tipVelOk = false;   // 0.18.58 the blade's velocity at the contact (the record's swing direction)
    if (lastTipMs && now > lastTipMs && now - lastTipMs < 100) { const float s = 1000.0f / (float)(now - lastTipMs); for (int c = 0; c < 3; c++) tipVel[c] = (tipW[c] - lastTipV[c]) * s; tipVelOk = true; }
    lastTipMs = now; memcpy(lastTipV, tipW, 12);
    melee::Target tg[16]; int idx[16]; int nt = 0;
    for (int i = 0; i < NR && nt < 16; i++) { if (!reg[i] || !alive(reg[i]) || !hostile(i)) continue; float p[3]; if (!rdF3(reg[i], p)) continue;
        const float dx = p[0] - hp3[0], dy = p[1] - hp3[1]; if (dx * dx + dy * dy > (float)reachCm * reachCm) continue;
        const bool dog = clsOf(reg[i]) == 16; melee::Target &T = tg[nt]; T.id = i * 4096 + (int)(regGen[i] & 4095); T.a = melee::v3(p[0], p[1], p[2] + (dog ? 30.0f : 25.0f)); T.b = melee::v3(p[0], p[1], p[2] + (dog ? 55.0f : 170.0f)); T.r = dog ? 32.0f : 26.0f; idx[nt++] = i; }
    melee::V3 tip = melee::v3(tipW[0], tipW[1], tipW[2]), gd = melee::v3(guardW[0], guardW[1], guardW[2]);
    const melee::V3 bd = melee::sub(gd, tip); const float bl = melee::len(bd);
    if (bl > bladeCutCm && bl > 1.0f) gd = melee::add(tip, melee::mul(bd, bladeCutCm / bl));   // the cutting edge only (never the hilt)
    const u32 slow0 = core.tooSlow + core.tooShort; melee::Hit h[4]; const int nh = core.step(now, gd, tip, tg, nt, mcfg, h, 4, sv);
    if (core.peakSpeed > secPeak) secPeak = core.peakSpeed; core.peakSpeed = 0;
    int res = core.tooSlow + core.tooShort != slow0 ? 2 : 0;
    for (int k = 0; k < nh; k++) { int j = -1; for (int t = 0; t < nt; t++) if (tg[t].id == h[k].target) j = idx[t]; if (j < 0 || qn >= 8) continue;
        const float u = fminf(1.0f, fmaxf(0.0f, (h[k].speed - mcfg.minSpeed) / fmaxf(0.1f, fullSpeed - mcfg.minSpeed)));
        { Req &R = q[qn++]; R.i = j; R.mult = (dmgMin + (dmgMax - dmgMin) * u) * (twoHands ? twoHandMult : 1.0f); R.kill = 0; R.kind = (u8)(heavy ? 1 : 0);
          R.ctc[0] = h[k].at.x; R.ctc[1] = h[k].at.y; R.ctc[2] = h[k].at.z; R.ctc[3] = tipVel[0]; R.ctc[4] = tipVel[1]; R.ctc[5] = tipVel[2]; R.hasC = tipVelOk; } secHits++;
        if (res != 1) { rdF3(reg[j], hitPos); *hitSpeed = h[k].speed; } res = 1;
        logf("STRIKE: the blade met enemy %p at %.1f m/s closing (%.0f cm travel) - %s blow queued", (void *)reg[j], h[k].speed, h[k].travel, heavy ? "HEAVY" : "light"); }
    return res;
}
// v16 the wrist HUD's data (docs/WRIST_HUD.md): Jin's health to the host (it forces the wrist panel on at low health and after a
// change) and the low-health heartbeat in the left hand (design doc "Rumble feedback low health"; vit::Heartbeat - 60 bpm at the line,
// 130 near death, lub-dub, only within wrist_hud_heartbeat_mute_s of the last health lost)
static vit::ChangeTracker vitTrack; static vit::Heartbeat heart; static u64 lastHurt = 0, dubAt = 0; static u32 vitSeqN = 0;
static int heartOn = 1; static float heartGain = 0.6f, heartLow = 0.30f, heartMuteS = 20.0f, resolveMax = 10.0f;
static void vitalsTick() {
    u8 *h = fp::heroPtr(); vit::Status st; st.ok = h && readable(h + 0x1970, 8);
    if (st.ok) { st.hpMax = rdF(h + 0x1970); st.hp = rdF(h + 0x1974); st.ok = std::isfinite(st.hp) && std::isfinite(st.hpMax) && st.hpMax > 0; }
    const u64 now = GetTickCount64();
    // 0.18.54 WRIST_HUD.md W0 DONE. Resolve = hero+0x43A4 (float, pips; its only writer 0x8FB210 also mirrors it to [hero+0xB8]+0x7BC).
    // The MAXIMUM is the game's own progression value: hero+0x4644 + 3.0 - every game function that clamps, refills or shows resolve as a
    // fraction uses exactly that (0x8EA6D0 refill, 0x8F7200 res / (max), 0x8E1C80, 0x8E2470, 0x905FE0). So it grows with Jin's upgrades
    // (owner: "the resolve meter changes throughout player progression"). wrist_resolve_max is only the fallback if that read fails.
    { const bool rOk = st.ok && readable(h + 0x43A4, 4) && readable(h + 0x4644, 4);
      const float r = rOk ? rdF(h + 0x43A4) : -1.0f, mx = rOk ? rdF(h + 0x4644) + 3.0f : -1.0f;
      const bool mOk = std::isfinite(mx) && mx >= 1.0f && mx <= 30.0f; st.resMax = mOk ? mx : (rOk ? resolveMax : 0.0f);
      st.res = std::isfinite(r) && r >= 0.0f && r <= 30.0f ? fminf(r, st.resMax > 0 ? st.resMax : r) : 0.0f;
      static u64 lastV = 0; static float lr = -9, lm = -9;
      if (st.ok && (now - lastV > 10000 || fabsf(r - lr) > 0.001f || fabsf(mx - lm) > 0.001f)) { lastV = now; lr = r; lm = mx;
          logf("   VITALS: health %.1f / %.1f | resolve %.3f of %.3f (hero+0x43A4 / hero+0x4644 + 3%s)", st.hp, st.hpMax, r, st.resMax, mOk ? "" : " - INVALID, wrist_resolve_max used"); } }
    if (!S) { if (vitTrack.step(st) & vit::CHit) g_jinHurtAt = now; if (st.ok && st.hp <= 1.5f) g_jinDownAt = now; return; }   // flat: logged only (no headset, no wrist HUD)
    const uint32_t bits = vitTrack.step(st); if (bits & vit::CHit) { lastHurt = now; g_jinHurtAt = now; }
    if (st.ok && st.hp <= 1.5f) g_jinDownAt = now;
    gvWriteBegin(&S->vitSeq); S->vitOk = st.ok ? 1 : 0; S->vitHp = st.hp; S->vitHpMax = st.hpMax; S->vitRes = st.res; S->vitResMax = st.resMax; if (bits) { S->vitChanged = bits; S->vitChangedSeq = ++vitSeqN; } gvWriteEnd(&S->vitSeq);
    static u64 last = 0; const float dt = last ? fminf(0.1f, (now - last) * 0.001f) : 0.0f; last = now;
    const bool active = heartOn && g_cfg.xr == 4 && st.ok && vit::heartbeatFelt(now, lastHurt, heartMuteS);
    if (heart.step(dt, st.ok ? st.hp / st.hpMax : 1.0f, active, heartLow)) { vrin::pulse(0, fminf(1.0f, heartGain), 40); dubAt = now + 150; }
    if (dubAt && now >= dubAt) { dubAt = 0; vrin::pulse(0, fminf(1.0f, heartGain * 0.7f), 35); }
}
// 0.18.49 WRIST_HUD.md W0 - finding resolve (flat play only): the hero's 32 KB and the first 4 KB of every object it points to are
// snapshotted every 250 ms; when Jin spends resolve (a resolve heal, a revive) or earns it (a kill), what changed since the snapshot is
// logged at once and 0.6 s later (RSCAN lines: [hero+pointer offset]+offset: old -> new). The field that falls at every heal / revive
// and rises at kills is resolve.
namespace rscan {
static const int NOBJ = 640, OBJB = 0x1000; static u8 *buf = nullptr; static u8 *objp[NOBJ]; static uint16_t objOff[NOBJ]; static int nObj = 0; static u8 heroSnap[0x8000]; static u8 *heroS = nullptr;
static u64 lastSnap = 0, frozenUntil = 0, pendAt = 0; static char pendWhat[64]; static int nEvents = 0;
// 0.18.50 (session 7: a kill changed 5000+ fields - animation, positions, timers): fields that change between quiet snapshots are BUSY and
// left out (a decaying counter per field); resolve only moves at the events
static u8 *busy = nullptr; static u8 busyH[0x8000 / 4]; static u8 *prevP[NOBJ]; static int prevN = 0;
static inline void bump(u8 &b, bool ch) { b = ch ? (u8)(b > 191 ? 255 : b + 64) : (u8)(b - (b >> 3)); }
static void snap(u8 *h) {
    if (!buf) buf = (u8 *)VirtualAlloc(nullptr, (size_t)NOBJ * OBJB, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE); if (!busy) busy = (u8 *)VirtualAlloc(nullptr, (size_t)NOBJ * (OBJB / 4), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE); if (!buf || !busy) return;
    if (!readable(h, sizeof heroSnap)) return;
    if (heroS == h) for (int q = 0; q < (int)sizeof heroSnap; q += 4) bump(busyH[q / 4], memcmp(heroSnap + q, h + q, 4) != 0); else memset(busyH, 0, sizeof busyH);
    memcpy(heroSnap, h, sizeof heroSnap); heroS = h; for (int i = 0; i < nObj; i++) prevP[i] = objp[i]; prevN = nObj; nObj = 0;
    for (int off = 0; off < (int)sizeof heroSnap && nObj < NOBJ; off += 8) { u8 *p = *(u8 **)(heroSnap + off);
        if (!p || ((u64)p & 15) || (p >= g_base && p < g_base + 0x4000000) || (u64)p < 0x10000000ull || (u64)p > 0x7FFFFFFFFFFFull) continue;
        bool dup = false; for (int i = 0; i < nObj && !dup; i++) dup = objp[i] == p; if (dup || !readable(p, OBJB)) continue;
        u8 *bz = busy + (size_t)nObj * (OBJB / 4), *old = buf + (size_t)nObj * OBJB;
        if (nObj < prevN && prevP[nObj] == p) { for (int q = 0; q < OBJB; q += 4) bump(bz[q / 4], memcmp(old + q, p + q, 4) != 0); } else memset(bz, 0, OBJB / 4);
        objp[nObj] = p; objOff[nObj] = (uint16_t)off; memcpy(old, p, OBJB); nObj++; } }
static bool plain(u32 v) { float f; memcpy(&f, &v, 4); const float a = fabsf(f); return v < 0x10000u || (std::isfinite(f) && a >= 0.01f && a <= 100000.0f); }
// 0.18.52 (session 8: 1400-7000 fields changed per event, the log lines cut off): statistics instead of lists. For every field: how often it
// rose / fell at resolve GAIN events (kills, parries, staggers, Ctrl+Shift+R) and SPEND events (resolve heals, revives); the fields that
// rise at gains and fall at spends - and do little else - are reported as resolve candidates (RESOLVE? lines) after every event
static const int NH = 0x8000 / 4, NF = NH + NOBJ * (OBJB / 4);
struct St { uint16_t gu, gd, su, sd; }; static St *st = nullptr; static u8 *stSlot[NOBJ]; static int nGain = 0, nSpend = 0;
static inline float val(u32 v) { if (v < 0x10000u) return (float)v; float f; memcpy(&f, &v, 4); return f; }
static void diff(const char *what, int kind, bool count) {   // kind: 1 gain, 2 spend, 0 unknown
    u8 *h = fp::heroPtr(); if (!h || h != heroS || !buf) return;
    if (!st) { st = (St *)VirtualAlloc(nullptr, sizeof(St) * NF, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE); if (!st) return; }
    int n = 0;
    auto one = [&](int idx, u32 a, u32 c) { if (a == c || !plain(a) || !plain(c)) return; n++; if (!count || !kind) return; St &x = st[idx]; const bool up = val(c) > val(a);
        if (kind == 1) { if (up) { if (x.gu < 65535) x.gu++; } else if (x.gd < 65535) x.gd++; } else { if (up) { if (x.su < 65535) x.su++; } else if (x.sd < 65535) x.sd++; } };
    if (readable(h, sizeof heroSnap)) for (int q = 0; q < (int)sizeof heroSnap; q += 4) { if (q >= 0x1970 && q < 0x1A00) continue; if (busyH[q / 4] >= 40) continue; u32 a, c; memcpy(&a, heroSnap + q, 4); memcpy(&c, h + q, 4); one(q / 4, a, c); }
    for (int i = 0; i < nObj; i++) { if (*(u8 **)(h + objOff[i]) != objp[i] || !readable(objp[i], OBJB)) continue; if (stSlot[i] != objp[i]) { memset(st + NH + (size_t)i * (OBJB / 4), 0, sizeof(St) * (OBJB / 4)); stSlot[i] = objp[i]; }
        const u8 *bz = busy + (size_t)i * (OBJB / 4); for (int q = 0; q < OBJB; q += 4) { if (bz[q / 4] >= 40) continue; u32 a, c; memcpy(&a, buf + (size_t)i * OBJB + q, 4); memcpy(&c, objp[i] + q, 4); one(NH + i * (OBJB / 4) + q / 4, a, c); } }
    if (count && kind == 1) nGain++; if (count && kind == 2) nSpend++;
    logf("RSCAN %s: %d quiet fields changed (%d objects; gains so far %d, spends %d)", what, n, nObj, nGain, nSpend);
    if (!count || nGain + nSpend < 2) return;
    struct C { int idx, sc; }; C top[12]; int nt = 0;
    for (int idx = 0; idx < NF; idx++) { const St &x = st[idx]; const int good = x.gu + x.sd, bad = x.gd + x.su; if (good < 2 || bad * 2 > good) continue; const int sc = good * 4 - bad * 6;
        if (nt < 12) top[nt++] = {idx, sc}; else { int m = 0; for (int k = 1; k < 12; k++) if (top[k].sc < top[m].sc) m = k; if (sc > top[m].sc) top[m] = {idx, sc}; } }
    char b[1400]; int o = 0;
    for (int k = 0; k < nt && o < 1320; k++) { const int idx = top[k].idx; const St &x = st[idx]; u32 now = 0; char w[24];
        if (idx < NH) { memcpy(&now, h + idx * 4, 4); snprintf(w, sizeof w, "hero+%x", idx * 4); } else { const int sl = (idx - NH) / (OBJB / 4), q = ((idx - NH) % (OBJB / 4)) * 4; if (sl < nObj && readable(objp[sl] + q, 4)) memcpy(&now, objp[sl] + q, 4); snprintf(w, sizeof w, "[hero+%x]+%x", objOff[sl], q); }
        o += snprintf(b + o, sizeof b - (size_t)o, " %s=%g (gain +%u/-%u, spend -%u/+%u)", w, val(now), x.gu, x.gd, x.sd, x.su); }
    logf("RESOLVE? candidates (rise at gains, fall at spends):%s", nt ? b : " none yet"); }
static int pendKind = 0;
static void event(const char *what) {   // game thread
    if (!g_playFlat || !resScan || nEvents >= 80) return; nEvents++; const u64 now = GetTickCount64(); char w[96]; snprintf(w, sizeof w, "%s (at once, snapshot %llu ms old)", what, (unsigned long long)(now - lastSnap));
    const int kind = (strstr(what, "HEAL") || strstr(what, "REVIVE")) ? 2 : (strstr(what, "KILL") || strstr(what, "PARRY") || strstr(what, "STAGGER") || strstr(what, "MARK")) ? 1 : 0;
    diff(w, kind, false);
    snprintf(pendWhat, sizeof pendWhat, "%s (+0.6 s)", what); pendKind = kind; pendAt = now + 600; frozenUntil = now + 700; }
static void tick() {
    if (!g_playFlat) return; u8 *h = fp::heroPtr(); if (!h) return; const u64 now = GetTickCount64();
    if (pendAt && now >= pendAt) { pendAt = 0; diff(pendWhat, pendKind, true); }
    if (now >= frozenUntil && now - lastSnap >= 250) { lastSnap = now; snap(h); } }
} // namespace rscan
// 0.18.50 (session 7: still no flinch through the game's delivery): who writes an enemy's reaction fields on a real blow - hardware data
// watchpoints (flat play only). Ctrl+Shift+W arms DR0..DR2 on the nearest hostile enemy's +0x1694 / +0x1698 / +0x16A4 in every game
// thread; each write is logged with the writing instruction and its call chain (DBW lines); disarmed after 30 writes, 90 s or Ctrl+Shift+W.
namespace dbw {
struct Rec { u64 rip; u64 dr6; u32 v[3]; void *st[16]; uint16_t n; u32 tid; u64 ms; };
static Rec ring[64]; static std::atomic<u32> wi{0}; static u32 ri = 0; static std::atomic<u8 *> obj{nullptr}; static std::atomic<int> req{0}; static PVOID veh = nullptr;
static bool armed = false; static u64 armedAt = 0;
static LONG CALLBACK onStep(EXCEPTION_POINTERS *e) {
    if (e->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP) return EXCEPTION_CONTINUE_SEARCH;
    CONTEXT *c = e->ContextRecord; if (!(c->Dr6 & 7)) return EXCEPTION_CONTINUE_SEARCH;
    const u32 i = wi.fetch_add(1); if (i - ri < 64) { Rec &r = ring[i & 63]; r.rip = c->Rip; r.dr6 = c->Dr6; u8 *b = obj.load(); if (b) { r.v[0] = *(volatile u32 *)(b + 0x1694); r.v[1] = *(volatile u32 *)(b + 0x1698); r.v[2] = *(volatile u32 *)(b + 0x16A4); }
        r.n = RtlCaptureStackBackTrace(0, 16, r.st, nullptr); r.tid = GetCurrentThreadId(); r.ms = GetTickCount64(); }
    c->Dr6 = 0; return EXCEPTION_CONTINUE_EXECUTION; }
static int setAll(u8 *o) {   // hotkey thread: every other thread of the game gets the watchpoints (or loses them)
    const DWORD me = GetCurrentThreadId(), pid = GetCurrentProcessId(); HANDLE sn = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0); if (sn == INVALID_HANDLE_VALUE) return 0;
    THREADENTRY32 te{}; te.dwSize = sizeof te; int n = 0;
    for (BOOL ok = Thread32First(sn, &te); ok; ok = Thread32Next(sn, &te)) { if (te.th32OwnerProcessID != pid || te.th32ThreadID == me) continue;
        HANDLE t = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID); if (!t) continue;
        if (SuspendThread(t) != (DWORD)-1) { CONTEXT c{}; c.ContextFlags = CONTEXT_DEBUG_REGISTERS;
            if (GetThreadContext(t, &c)) { if (o) { c.Dr0 = (u64)(o + 0x1694); c.Dr1 = (u64)(o + 0x1698); c.Dr2 = (u64)(o + 0x16A4); c.Dr7 = (1ull | 4ull | 16ull) | (1ull << 16) | (3ull << 18) | (1ull << 20) | (3ull << 22) | (1ull << 24) | (3ull << 26); }
                else { c.Dr0 = c.Dr1 = c.Dr2 = 0; c.Dr7 = 0; } c.Dr6 = 0; if (SetThreadContext(t, &c)) n++; }
            ResumeThread(t); }
        CloseHandle(t); }
    CloseHandle(sn); return n; }
static void poll() {   // hotkey thread, every 50 ms
    const int r = req.exchange(0);
    if (r == 1 && obj.load()) { if (!veh) veh = AddVectoredExceptionHandler(1, onStep); const int n = setAll(obj.load()); armed = n > 0; armedAt = GetTickCount64(); logf("DBW: watching enemy %p +0x1694 / +0x1698 / +0x16A4 for writes in %d threads - now hit it with the game's own attack", (void *)obj.load(), n); }
    const u32 w = wi.load(); while (ri != w) { const Rec &x = ring[ri & 63]; ri++; char b[400]; int o = 0;
        for (int i = 0; i < x.n && o < 380; i++) { const u8 *a = (const u8 *)x.st[i]; if (a >= g_base && a < g_base + 0x4000000) o += snprintf(b + o, sizeof b - (size_t)o, " 0x%llx", (unsigned long long)(a - g_base)); else o += snprintf(b + o, sizeof b - (size_t)o, " -"); }
        logf("DBW write #%u (dr6 %llx: %s) by rva 0x%llx, thread %u%s: now +1694 %u +1698 %u +16A4 %u | chain:%s", ri, (unsigned long long)x.dr6, (x.dr6 & 1) ? "+1694" : (x.dr6 & 2) ? "+1698" : "+16A4",
             (unsigned long long)((const u8 *)x.rip >= g_base ? (const u8 *)x.rip - g_base : 0), x.tid, x.tid == t_tick.load() ? " (game thread)" : "", x.v[0], x.v[1], x.v[2], b); }
    if (armed && (r == 2 || w >= 30 || GetTickCount64() - armedAt > 90000)) { const int n = setAll(nullptr); armed = false; obj = nullptr; logf("DBW: watchpoints off (%d threads; %u writes caught)", n, w); }
}
} // namespace dbw
// 0.18.54 flat swing test (see swingUntil): armed by Ctrl+Shift+G, fired by the hotkey thread when you press attack
// 0.18.54 cbt_assist=1 (the OLD method, WITH Jin's attack animation, off by default, kept to compare): every blow presses the game's
// attack button (light X / heavy Y) for cbt_assist_press_ms, then lands cbt_assist_delay_ms later while that attack plays; Jin's own
// blows in the window are held back (the katana's blow replaces them).
static int assistOn = 0, assistDelayMs = 200, assistPressMs = 90; struct Pend { int i = -1; float mult = 1; u8 kill = 0, kind = 0; u64 at = 0; const char *src = ""; }; static Pend pend;
static void request(int i, float mult, bool kill, int kind, const char *src, const float *ctc = nullptr) {
    if (!assistOn) { strike(i, mult, kill, kind, src, ctc); return; }
    if (pend.i >= 0) { refuse("cbt_assist: a blow is already waiting for Jin's attack"); return; }
    const u64 now = GetTickCount64(); vrin::forcePress(kind ? vrin::XB_Y : vrin::XB_X, (u32)assistPressMs);
    pend.i = i; pend.mult = mult; pend.kill = kill ? 1 : 0; pend.kind = (u8)kind; pend.at = now + (u64)assistDelayMs; pend.src = src;
    assistUntil = now + (u64)assistDelayMs + 600; swingUntil = now + (u64)assistDelayMs + 600;
    logf("ASSIST (cbt_assist=1, with the attack animation): the game's %s attack pressed - the katana's blow on %p follows in %d ms", kind ? "heavy" : "light", (void *)reg[i], assistDelayMs); }
static void assistTick() { if (pend.i < 0 || GetTickCount64() < pend.at) return; const Pend p = pend; pend.i = -1; strike(p.i, p.mult, p.kill != 0, p.kind, p.src); }
static std::atomic<int> swingArmed{0}; static std::atomic<u64> swingFireAt{0}; static int swingDelayMs = 180;
static void swingTick() {
    const u64 at = swingFireAt.load(); if (!at || GetTickCount64() < at) return; swingFireAt = 0;
    loudRefuse = true; const int i = nearest(600.0f); if (i < 0) refuse("swing test: no hostile enemy within 6 m"); else strike(i, 1.0f, false, 0, "swing test (your attack playing)"); loudRefuse = false; }
static void tick() {   // game thread, after the game's tick
    { static u64 ps = 0; const u64 pn = GetTickCount64(); if (pn - ps >= 5000) { ps = pn; const u64 q = nVQ.exchange(0), h = nVQHit.exchange(0); const u64 cw = g_capWaits.exchange(0), cu = g_capWaitUs.exchange(0);
        logf("   PERF/5s: memory checks %llu kernel queries + %llu from the cache | game paced (cap %d pairs/s in VR now, max %d, adaptive %d: lowered %llu / raised %llu times; host %.0f fps of %.0f; %d menus): waited %llu times, %.1f ms on average", (unsigned long long)q, (unsigned long long)h, g_pairCap, g_pairCapMax, (int)g_paceAdaptive,
             (unsigned long long)g_capDowns.load(), (unsigned long long)g_capUps.load(), g_hostFps.load(), g_displayHz.load(), g_menuCap, (unsigned long long)cw, cw ? cu / 1000.0 / cw : 0.0);
        static u64 lp[5] = {0, 0, 0, 0, 0}; const u64 cp[5] = {nParry[1], nParry[2], nParryMissed, nBladeFound, nBladeMissing};
        if (memcmp(lp, cp, sizeof lp)) { memcpy(lp, cp, sizeof lp); logf("   PARRY/total: blocks %llu, parries %llu, blows that missed the blade %llu | parry_mode %s%s (enemy weapon found %llu, not found %llu)", (unsigned long long)cp[0], (unsigned long long)cp[1], (unsigned long long)cp[2],
                                         pcfg.mode == 1 ? "velocity" : parryBladeMode ? "blade" : "timing", parryEngine == 1 ? ", engine mod" : ", engine game", (unsigned long long)cp[3], (unsigned long long)cp[4]);
                                    logf("   PARRY/total: repeat contacts of a defended swing swallowed %llu, attackers staggered %llu", (unsigned long long)nSwallowed, (unsigned long long)nStaggers); } } }
    vitalsTick(); if (resScan) rscan::tick(); swingTick(); assistTick(); parryTick();
    if (!strikeOk) return;
    const int hk = hkReq.exchange(0);
    if (hk == 6) rscan::event("MARK (Ctrl+Shift+R)");
    if (hk == 5) { if (dbw::armed) dbw::req = 2; else { const int i = nearest(600.0f); if (i < 0 || !g_playFlat) logf("DBW: %s", g_playFlat ? "no hostile enemy within 6 m" : "flat play only"); else { dbw::obj = reg[i]; dbw::wi = 0; dbw::ri = 0; dbw::req = 1; } } }
    if (hk == 3) { int n = 0; for (int i = 0; i < NR; i++) if (reg[i]) { float p[3] = {0, 0, 0}; rdF3(reg[i], p); n++; logf("STRIKE registry %d: %p class %d %s (attitude to Jin %d, Jin's to it %d) health %.1f at %.0f %.0f %.0f", i, (void *)reg[i], clsOf(reg[i]), hostile(i) ? "HOSTILE" : "-", attitude(reg[i], fp::heroPtr()), attitude(fp::heroPtr(), reg[i]), rdF(reg[i] + 0x1974), p[0], p[1], p[2]); }
        logf("STRIKE registry: %d characters; templates light %s heavy %s; HandleHit on the game thread %llu, off it %llu", n, tplOk[0] ? (tplLive[0] ? "live" : "from ini") : "none", tplOk[1] ? (tplLive[1] ? "live" : "from ini") : "none", (unsigned long long)nOnTick.load(), (unsigned long long)nOffTick.load()); }
    else if (hk == 1 || hk == 2) { loudRefuse = true; const int i = nearest(600.0f); if (i < 0) refuse("no hostile enemy within 6 m"); else if (qn < 8) q[qn++] = {i, 1.0f, (u8)(hk == 2), 0}; }
    else if (hk == 7) { if (!g_playFlat) logf("SWING TEST: flat play only"); else { swingArmed = 1; logf("SWING TEST armed: press the game's attack (left mouse / pad X) near an enemy - the katana's blow lands %d ms into your swing, your own blow held back", swingDelayMs); } }
    else if (hk == 8) { loudRefuse = true; const int i = nearest(600.0f); if (i < 0) refuse("no hostile enemy within 6 m"); else { forceMsg = true; strike(i, 1.0f, false, 0, "hotkey (message path)"); forceMsg = false; } loudRefuse = false; }
    for (int k = 0; k < qn; k++) request(q[k].i, q[k].mult, q[k].kill != 0, q[k].kind, hk ? "hotkey" : "katana", q[k].hasC ? q[k].ctc : nullptr);
    qn = 0; loudRefuse = false;
    static u64 sec = 0; const u64 now = GetTickCount64(); if (now - sec >= 1000) { if (secHits || core.evaluated || secSwings || secCharges) logf("   HITS/s: blade contacts %u (too slow %u, too short %u, cooling %u), blows %u (heavy %u), closing speed up to %.1f m/s | swings %u, heavy charges %u (restarted by moving %u), lunges %u | total blows %llu (killing %llu), refused %llu",
        core.evaluated, core.tooSlow, core.tooShort, core.cooling, secHits, secHeavy, secPeak, secSwings, secCharges, intent.restarts, secLunges, (unsigned long long)nStrikes, (unsigned long long)nKills, (unsigned long long)nRefused);
        sec = now; secHits = 0; secPeak = 0; secSwings = secCharges = secHeavy = secLunges = 0; intent.restarts = 0; core.evaluated = core.tooSlow = core.tooShort = core.cooling = core.contacts = 0; }
}
static void installStrike(bool hooksOk) {
    static const Sig s3{"hit-message pool", RVA_MSGPOOL, "48 83 EC 28 48 8B 05 ?? ?? ?? ?? 48 85 C0 75 27"}, s4{"hit-message constructor", RVA_MSGCTOR, "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 48 8B FA 48 8B D9"},
        s5{"tag-list copy", RVA_MSGTAGS, "4C 8B DC 49 89 5B 18 49 89 73 20 41 56 48 83 EC 40 4C 8B 32 48 8B D9"}, s6{"weak-handle set", RVA_SETHANDLE, "48 89 5C 24 08 57 48 83 EC 20 48 8B D9 48 8B FA 48 8B 09 48 3B D1"};
    static const Sig s8{"hit delivery", RVA_DELIVER, "40 53 48 81 EC 80 02 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 84 24 70 02 00 00 48 8B D9 66 90 F7 83 90 00 00 00 FF FF"};
    const bool dl = checkSig(s8); logf("sig %s %s", s8.name, dl ? "PASS (blows delivered the game's way: reaction, sound, blood)" : "FAIL (HandleHit only: damage without a reaction)");
    static const Sig s7{"attitude query", RVA_ATTITUDE, "40 53 56 57 48 83 EC 40 4C 63 92 00 06 00 00 49 8B F8 4D 63 88 00 06 00 00"};
    const bool a = checkSig(s3), b = checkSig(s4), c = checkSig(s5), e = checkSig(s6), at = checkSig(s7);
    logf("sig %s %s (%s)", s7.name, at ? "PASS" : "FAIL", at ? "enemies by the game's own attitude" : "only enemies seen in a hit with Jin");
    wchar_t ini[MAX_PATH]; swprintf(ini, MAX_PATH, L"%ls\\gotvr.ini", g_dir);
    auto f = [&](const wchar_t *k, float d) { wchar_t v[32]; GetPrivateProfileStringW(L"gotvr", k, L"", v, 32, ini); return v[0] ? (float)_wtof(v) : d; };
    auto def = [&](const wchar_t *k, const wchar_t *v) { wchar_t t[8]; if (!GetPrivateProfileStringW(L"gotvr", k, L"", t, 8, ini)) WritePrivateProfileStringW(L"gotvr", k, v, ini); };
    def(L"sword_hit", L"1"); def(L"sword_hit_min_speed", L"2.2"); def(L"sword_hit_min_travel_cm", L"18"); def(L"sword_hit_contact_cm", L"6"); def(L"sword_hit_cooldown_ms", L"220");
    def(L"sword_hit_rearm_ms", L"120"); def(L"sword_hit_blade_cm", L"70"); def(L"sword_hit_damage_slow", L"0.75"); def(L"sword_hit_damage_fast", L"1.10"); def(L"sword_hit_fast_speed", L"8");
    def(L"sword_hit_rumble", L"0.9"); def(L"sword_hit_rumble_ms", L"70"); def(L"sword_hit_any", L"0");
    // 0.18.48 the design doc's [COMBAT DESIGN] attack parameters (README: the doc's name -> the key)
    def(L"sword_heavy_charge_ms", L"450"); def(L"sword_heavy_charge_move_cm", L"25"); def(L"sword_swing_rearm_speed", L"0.8");
    def(L"sword_lunge", L"1"); def(L"sword_lunge_cm", L"35"); def(L"sword_lunge_heavy_cm", L"60"); def(L"sword_lunge_speed_mod", L"0.5"); def(L"sword_lunge_cooldown_ms", L"600"); def(L"sword_lunge_stop_cm", L"70"); def(L"sword_lunge_stick_speed", L"4.5");
    def(L"sword_swing_rumble", L"0.35"); def(L"sword_swing_rumble_heavy", L"0.7"); def(L"sword_weight", L"1.0"); def(L"sword_weight_sag_deg", L"3"); def(L"sword_weight_two_hand", L"0.33"); def(L"sword_two_hand_damage", L"1.0"); def(L"cbt_msg_state", L"1"); def(L"cbt_deliver", L"1");
    hitOn = (int)f(L"sword_hit", 1) ? 1 : 0; mcfg.minSpeed = f(L"sword_hit_min_speed", 2.2f); mcfg.minTravelCm = f(L"sword_hit_min_travel_cm", 18.0f); mcfg.contactCm = f(L"sword_hit_contact_cm", 6.0f);
    mcfg.cooldownMs = (int)f(L"sword_hit_cooldown_ms", 220.0f); mcfg.rearmMs = (int)f(L"sword_hit_rearm_ms", 120.0f); bladeCutCm = f(L"sword_hit_blade_cm", 70.0f);
    dmgMin = f(L"sword_hit_damage_slow", 0.75f); dmgMax = f(L"sword_hit_damage_fast", 1.10f); fullSpeed = f(L"sword_hit_fast_speed", 8.0f); rumble = f(L"sword_hit_rumble", 0.9f); rumbleMs = (int)f(L"sword_hit_rumble_ms", 70.0f); hitAny = (int)f(L"sword_hit_any", 0) ? 1 : 0;
    acfg.minSpeed = mcfg.minSpeed; acfg.minTravelCm = mcfg.minTravelCm; acfg.rearmSpeed = f(L"sword_swing_rearm_speed", 0.8f); acfg.fullSpeed = fullSpeed;
    // 0.18.62 (session 18: "heavy attacks never triggered - hold trigger to charge, then just swing"): 10 cm of drift spent the charge
    // before the swing; the allowance is 25 cm now, migrated once; sword_heavy_two_hands 1 = only a two-handed grip charges
    { wchar_t t[8]; if (!GetPrivateProfileStringW(L"gotvr", L"sword_heavy_v", L"", t, 8, ini)) { WritePrivateProfileStringW(L"gotvr", L"sword_heavy_charge_move_cm", L"25", ini); WritePrivateProfileStringW(L"gotvr", L"sword_heavy_v", L"2", ini); } }
    def(L"sword_heavy_two_hands", L"0"); def(L"sword_heavy_swing_window_ms", L"300");
    acfg.chargeMs = (int)f(L"sword_heavy_charge_ms", 450.0f); acfg.chargeMoveCm = f(L"sword_heavy_charge_move_cm", 25.0f);
    acfg.heavyTwoHands = (int)f(L"sword_heavy_two_hands", 0.0f) != 0; acfg.heavySwingMs = (int)fmaxf(80.0f, fminf(1500.0f, f(L"sword_heavy_swing_window_ms", 300.0f)));
    lungeOn = (int)f(L"sword_lunge", 1) ? 1 : 0; acfg.lungeCm = f(L"sword_lunge_cm", 35.0f); acfg.lungeHeavyCm = f(L"sword_lunge_heavy_cm", 60.0f); acfg.lungeMod = f(L"sword_lunge_speed_mod", 0.5f);
    acfg.lungeCooldownMs = (int)f(L"sword_lunge_cooldown_ms", 600.0f); acfg.standoffCm = f(L"sword_lunge_stop_cm", 70.0f); lungeSpeed = fmaxf(0.5f, f(L"sword_lunge_stick_speed", 4.5f));
    def(L"sword_heavy_swing_rumble_ms", L"220"); heavySwingMs = (int)fmaxf(40.0f, fminf(600.0f, f(L"sword_heavy_swing_rumble_ms", 220.0f)));
    swingRumble = f(L"sword_swing_rumble", 0.35f); swingRumbleHeavy = f(L"sword_swing_rumble_heavy", 0.7f); acfg.weight = fmaxf(0.0f, fminf(5.0f, f(L"sword_weight", 1.0f)));
    { wchar_t mv[8]; if (!GetPrivateProfileStringW(L"gotvr", L"sword_weight_model", L"", mv, 8, ini)) {   // 0.18.58: the old default 0 (off) becomes the new default 1 once
          wchar_t wv[16]; GetPrivateProfileStringW(L"gotvr", L"sword_weight", L"", wv, 16, ini); if (!wv[0] || _wtof(wv) == 0.0f) { WritePrivateProfileStringW(L"gotvr", L"sword_weight", L"1.0", ini); acfg.weight = 1.0f; }
          WritePrivateProfileStringW(L"gotvr", L"sword_weight_model", L"2", ini); } }
    acfg.sagDeg = fmaxf(0.0f, fminf(15.0f, f(L"sword_weight_sag_deg", 3.0f))); acfg.twoHandFactor = fmaxf(0.05f, fminf(1.0f, f(L"sword_weight_two_hand", 0.33f))); twoHandMult = f(L"sword_two_hand_damage", 1.0f);
    msgState = (int)f(L"cbt_msg_state", 1) ? 1 : 0; deliverOn = (int)f(L"cbt_deliver", 1) ? 1 : 0;
    def(L"cbt_mode", L"2"); cbtMode = (int)f(L"cbt_mode", 2);
    if (cbtMode == 2) for (int k = 0; k < 4; k++) recLoad(k);   // 0.18.59 at start: a saved reacting record is never replaced by a plainer blow
    def(L"parry", L"1"); def(L"parry_mode", L"timing"); def(L"parry_timing_ms", L"220"); def(L"parry_blade_tolerance_cm", L"40"); def(L"parry_window_ms", L"300"); def(L"parry_min_speed", L"1.5"); def(L"parry_min_travel_cm", L"10");
    def(L"parry_window_mult", L"1.0"); def(L"perfect_parry_window_mult", L"1.0"); def(L"parry_rumble", L"0.8"); def(L"parry_show", L"1"); def(L"parry_deflect_delay_ms", L"130"); def(L"parry_block_delay_ms", L"300");
    parryOn = (int)f(L"parry", 1) ? 1 : 0; pcfg.tolCm = fmaxf(5.0f, f(L"parry_blade_tolerance_cm", 30.0f)); pcfg.windowMs = fmaxf(30.0f, f(L"parry_window_ms", 300.0f)); pcfg.minSpeed = f(L"parry_min_speed", 1.5f);
    pcfg.minTravelCm = f(L"parry_min_travel_cm", 10.0f); pcfg.windowMult = fmaxf(0.1f, f(L"parry_window_mult", 1.0f)); pcfg.perfectMult = fmaxf(0.1f, f(L"perfect_parry_window_mult", 1.0f)); parryRumble = f(L"parry_rumble", 0.8f);
    { wchar_t t[8]; if (!GetPrivateProfileStringW(L"gotvr", L"parry_v", L"", t, 8, ini)) { WritePrivateProfileStringW(L"gotvr", L"parry_deflect_delay_ms", L"130", ini); WritePrivateProfileStringW(L"gotvr", L"parry_v", L"2", ini); } }   // 0.18.64: 50 ms was before the game's guard was up
    def(L"fx_sound_volume", L"0.7"); sfx::init(fmaxf(0.0f, fminf(1.0f, f(L"fx_sound_volume", 0.7f))));
    def(L"parry_engine", L"mod"); def(L"parry_hilt_extend_cm", L"30"); def(L"parry_tip_extend_cm", L"12");
    if (GetPrivateProfileIntW(L"gotvr", L"parry_v", 0, ini) < 3) { WritePrivateProfileStringW(L"gotvr", L"parry_blade_tolerance_cm", L"40", ini); WritePrivateProfileStringW(L"gotvr", L"parry_v", L"3", ini); }   // 0.18.65: 30 -> 40 cm
    pcfg.tolCm = fmaxf(5.0f, f(L"parry_blade_tolerance_cm", 40.0f));
    parryHiltCm = fmaxf(0.0f, fminf(80.0f, f(L"parry_hilt_extend_cm", 30.0f))); parryTipCm = fmaxf(0.0f, fminf(60.0f, f(L"parry_tip_extend_cm", 12.0f))); def(L"parry_swallow_ms", L"450"); def(L"parry_stagger_damage", L"0.15");
    { wchar_t pe[16]; GetPrivateProfileStringW(L"gotvr", L"parry_engine", L"mod", pe, 16, ini); parryEngine = _wcsicmp(pe, L"game") == 0 ? 0 : 1; }
    swallowMs = (int)fmaxf(0.0f, fminf(1500.0f, f(L"parry_swallow_ms", 450.0f))); staggerMult = fmaxf(0.0f, fminf(2.0f, f(L"parry_stagger_damage", 0.15f)));
    parryShow = (int)f(L"parry_show", 1) ? 1 : 0; deflectDelayMs = (int)fmaxf(0.0f, fminf(300.0f, f(L"parry_deflect_delay_ms", 130.0f))); blockDelayMs = (int)fmaxf(0.0f, fminf(600.0f, f(L"parry_block_delay_ms", 300.0f)));
    { wchar_t pm[16]; GetPrivateProfileStringW(L"gotvr", L"parry_mode", L"timing", pm, 16, ini); pcfg.mode = _wcsicmp(pm, L"velocity") == 0 ? 1 : 0; parryBladeMode = _wcsicmp(pm, L"blade") == 0 ? 1 : 0; }
    def(L"parry_enemy_blade_cm", L"90"); enemyBladeCm = fmaxf(20.0f, fminf(250.0f, f(L"parry_enemy_blade_cm", 90.0f))); pcfg.parryMs = fmaxf(30.0f, f(L"parry_timing_ms", 220.0f));
    logf("katana block / parry: %s, %s (blade within %.0f cm of the blow's line in the last %.0f ms; %s; replay %d / %d ms) - parry_*", parryOn ? "ON" : "off", pcfg.mode ? "VELOCITY mode" : "TIMING mode", pcfg.tolCm, pcfg.windowMs * pcfg.windowMult,
         pcfg.mode ? "parry = the tip moving fast into it" : "parry = the blade arriving in the blow's path at most parry_timing_ms x perfect_parry_window_mult before it lands, block = there earlier", deflectDelayMs, blockDelayMs);
    def(L"cbt_swing_test_ms", L"180"); def(L"cbt_resolve_scan", L"0"); def(L"cbt_assist", L"0"); def(L"cbt_assist_delay_ms", L"200"); def(L"cbt_assist_press_ms", L"90");
    swingDelayMs = (int)fmaxf(0.0f, fminf(1000.0f, f(L"cbt_swing_test_ms", 180.0f))); resScan = (int)f(L"cbt_resolve_scan", 0) ? 1 : 0;
    assistOn = (int)f(L"cbt_assist", 0) ? 1 : 0; assistDelayMs = (int)fmaxf(0.0f, fminf(1000.0f, f(L"cbt_assist_delay_ms", 200.0f))); assistPressMs = (int)fmaxf(30.0f, fminf(400.0f, f(L"cbt_assist_press_ms", 90.0f)));
    logf("combat: cbt_assist %s%s; flat swing test (Ctrl+Shift+G) delay %d ms; resolve scan %s", assistOn ? "ON - the game's attack button is pressed for every blow (attack animation plays)" : "off - the mod never presses an attack button", assistOn ? "" : "", swingDelayMs, resScan ? "on" : "off");
    def(L"wrist_hud_heartbeat", L"1"); def(L"wrist_hud_heartbeat_gain", L"0.6"); def(L"wrist_hud_heartbeat_mute_s", L"20");
    def(L"wrist_resolve_max", L"10"); resolveMax = fmaxf(1.0f, fminf(20.0f, f(L"wrist_resolve_max", 10.0f)));
    heartOn = (int)f(L"wrist_hud_heartbeat", 1) ? 1 : 0; heartGain = f(L"wrist_hud_heartbeat_gain", 0.6f); heartMuteS = f(L"wrist_hud_heartbeat_mute_s", 20.0f); heartLow = f(L"wrist_hud_low_hp", 0.30f);
    logf("sig %s %s | %s %s | %s %s | %s %s", s3.name, a ? "PASS" : "FAIL", s4.name, b ? "PASS" : "FAIL", s5.name, c ? "PASS" : "FAIL", s6.name, e ? "PASS" : "FAIL");
    logf("katana weight: %.2f (spring-damper period %.0f ms one-handed, %.0f ms two-handed; gravity sag %.1f deg level one-handed, %.1f two-handed; lag 1:1 to %.0f deg, never past %.0f) - sword_weight / sword_weight_sag_deg / sword_weight_two_hand",
         acfg.weight, acfg.weight * 45.0f, acfg.weight * 45.0f * acfg.twoHandFactor, acfg.sagDeg * acfg.weight, acfg.sagDeg * acfg.weight * 0.2f, acfg.lagSoftDeg, acfg.lagMaxDeg);
    if (!hooksOk || !a || !b || !c || !e) { logf("katana hits: OFF (%s)", hooksOk ? "a signature does not match this build" : "the combat hooks are not in"); return; }
    pPool = (FnPool)(g_base + RVA_MSGPOOL); pCtor = (FnCtor)(g_base + RVA_MSGCTOR); pTags = (FnTags)(g_base + RVA_MSGTAGS); pSetH = (FnSetH)(g_base + RVA_SETHANDLE); strikeOk = true;
    if (at) pAtt = (FnAtt)(g_base + RVA_ATTITUDE);
    if (dl) pDeliver = (FnDeliver)(g_base + RVA_DELIVER);
    {   static const Sig s9{"hit delivery (inner)", RVA_DELIVER1, "48 89 5C 24 10 57 48 81 EC 30 01 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 84 24 00 01 00 00 C7 41 28 03 00 00 00"};
        if (g_playFlat && checkSig(s9)) { const MH_STATUS st = MH_CreateHook(g_base + RVA_DELIVER1, (void *)hDeliver1, (void **)&oDeliver1); logf("combat diagnostics: delivery diff hook -> %s (flat play)", MH_StatusToString(st)); }
        static const Sig s10{"hit processing", RVA_HITPROC, "48 89 7C 24 18 41 56 48 83 EC 20 48 8B 41 48 4C 8B C2 48 8B F9 48 85 C0 0F 84"}, s11{"script event", RVA_EVENT, "48 89 6C 24 18 48 89 74 24 20 57 41 54 41 55 41 56 41 57 48 83 EC 40 33 FF 48 8B E9 48 8B 49 20"};
        if (checkSig(s10)) { const MH_STATUS a1 = MH_CreateHook(g_base + RVA_HITPROC, (void *)hHitProc, (void **)&oHitProc); logf("combat: hit processing hook -> %s (the katana's blows through the game's whole hit path, cbt_mode %d)", MH_StatusToString(a1), cbtMode); }
        if (g_playFlat && checkSig(s11)) { const MH_STATUS a2 = MH_CreateHook(g_base + RVA_EVENT, (void *)hEvent, (void **)&oEvent); logf("combat diagnostics: script events -> %s (flat play)", MH_StatusToString(a2)); } }
    tplLoad(ini, 0); tplLoad(ini, 1);
    logf("katana hits: %s (min %.1f m/s closing, %.0f cm travel, contact +%.0f cm, cooldown %d ms, re-arm %d ms, cutting edge %.0f cm, damage x%.2f..x%.2f up to %.1f m/s, rumble %.2f / %d ms, %s; light template %s, heavy %s; Ctrl+Shift+H strikes / Ctrl+Shift+K kills the nearest hostile enemy, Ctrl+Shift+J lists them)",
         hitOn ? "ON" : "off (sword_hit = 0)", mcfg.minSpeed, mcfg.minTravelCm, mcfg.contactCm, mcfg.cooldownMs, mcfg.rearmMs, bladeCutCm, dmgMin, dmgMax, fullSpeed, rumble, rumbleMs, hitAny ? "ANY character in reach (sword_hit_any = 1)" : "proven-hostile enemies only",
         tplOk[0] ? "ready (gotvr.ini)" : "not yet - land one ordinary blow with the game's own attack", tplOk[1] ? "ready" : "not yet");
}
static void install() {
    static const Sig s1{"SetHealth (character+0x1974)", RVA_SETHEALTH, "48 89 5C 24 08 57 48 81 EC D0 00 00 00 48 8B D9 0F 29 B4 24 C0 00 00 00"};
    static const Sig s2{"HandleHit (character, hitInfo)", RVA_HANDLEHIT, "40 55 53 56 57 41 55 41 56 48 8D 6C 24 A8 48 81 EC 58 01 00 00 48 8B 05"};
    const bool a = checkSig(s1), b = checkSig(s2); logf("sig %s rva=0x%x %s | sig %s rva=0x%x %s", s1.name, s1.rva, a ? "PASS" : "FAIL", s2.name, s2.rva, b ? "PASS" : "FAIL");
    obs::cbtHits = &nHits; obs::cbtHitsHero = &nHitsHero; obs::cbtHealth = &nHealth; obs::cbtHealthHero = &nHealthHero;
    if (!a || !b) { logf("combat observer: OFF (a signature does not match this build)"); return; }
    const MH_STATUS x = MH_CreateHook(g_base + RVA_SETHEALTH, (void *)hSetHealth, (void **)&oSetHealth), y = MH_CreateHook(g_base + RVA_HANDLEHIT, (void *)hHandleHit, (void **)&oHandleHit);
    logf("combat observer: SetHealth -> %s, HandleHit -> %s (logs every health change and hit, 40 lines/s at most)", MH_StatusToString(x), MH_StatusToString(y));
    installStrike(x == MH_OK && y == MH_OK);
}
} // namespace cbt

// 0.18.25 3D ("8D") AUDIO (owner: "begin on 8D spatial audio for VR"; playbook ch20 "the listener is another tracked camera": steer the
// game's own audio engine with the same transformed head basis the eyes use, keep its mix; God of War VR D-159 does exactly that for
// Wwise). Ghost of Tsushima runs Wwise too (AK::SoundEngine exported: SetDefaultListeners(0, 1, 2) at boot 0x5B91A0, SetPosition
// 0xF12BF0). Static RE: the audio update 0x5B3E60 calls Audio_ListenerFromCamera 0x5BAC80, which reads CAM-B's (*0x141EDBCC8) camera
// matrix at +0x890 (rows forward, left, up, position - the game's third-person camera), blends the position toward the audio focus,
// smooths it (0.8 per update) and stores the listener matrix + its inverse in the audio manager (+0x90.. / +0xD0..); 0x5BA730 then
// hands it to Wwise for listeners 0-2 (front y mirrored, position through the world->audio matrix 0x1420A1CD0). In VR that camera
// does not turn with your head, so sounds kept their side whatever way you looked. The hook lends that function your HEAD (the eye
// midpoint in the game world, composed exactly like the eyes: anchor + level yaw + head pose) for the duration of the call, so the
// game itself derives every listener value from it; CAM-B's matrix is put back right after. First person in VR only (cutscenes and
// third person keep the game's listener); audio_head = 0 turns it off.
namespace aud {
typedef void (*FnL)(u8 *); static FnL oListener = nullptr;
static std::atomic<int> on{1}; static std::atomic<u64> nHead{0}, nGame{0};
static volatile LONG seq = 0; static float head[16]; static std::atomic<u64> headMs{0};
static void publish(const float *m) { InterlockedIncrement(&seq); MemoryBarrier(); memcpy(head, m, 64); head[3] = head[7] = head[11] = 0.0f; head[15] = 1.0f; MemoryBarrier(); InterlockedIncrement(&seq); headMs = GetTickCount64(); }
static bool read(float *m) { for (int t = 0; t < 4; t++) { const LONG a = seq; if (a & 1) continue; MemoryBarrier(); memcpy(m, head, 64); MemoryBarrier(); if (a == seq) return true; } return false; }
static u8 *camBNow() { return *(u8 **)(g_base + RVA_CAMB); }
static u8 *(*camB)() = camBNow;   // self-test: a stand-in camera
static bool (*useHead)() = []() -> bool { return g_cfg.xr == 4 && fp::activeNow && GetTickCount64() - headMs.load() < 250; };
static void hListener(u8 *mgr) {
    u8 *cam = on.load() ? camB() : nullptr; float H[16];
    if (!cam || !useHead() || !readable(cam + 0x890, 64) || !read(H)) { nGame++; oListener(mgr); return; }
    float save[16]; memcpy(save, cam + 0x890, 64); memcpy(cam + 0x890, H, 64);
    oListener(mgr);
    memcpy(cam + 0x890, save, 64); nHead++;
    { static int told = 0; if (!told++) logf("3D audio: the game's listener now follows your head (first person in VR; head %.0f %.0f %.0f, the game's camera was %.0f %.0f %.0f)", H[12], H[13], H[14], save[12], save[13], save[14]); }
}
// 0.18.26 (owner: "finish the audio with the gear / Jin's voice"; God of War VR D-162): with the listener on your head, an emitter on Jin's
// own head / face (his voice, breath, hat and collar gear) sits a few cm from it, so the smallest head turn swung it from ear to ear.
// Static RE: the audio update 0x5BA70E loops emitters through Audio_EmitterUpdate 0x5BC780(emitter): Wwise game object id +0x18,
// source +0x28 -> owner node +0x38, owner joint +0x48 (-1 = the node), local offset +0x50; the position is owner world (node +0x150
// or joint world array +0x278 [joint + 1]) x offset; the game sends SetPosition(id, AkTransform{front, top, position}) with front / top
// y mirrored and the position through the world -> audio matrix 0x1420A1CD0. After the game's update, an emitter within
// audio_self_radius_cm (35) of your head gets one more SetPosition (Wwise applies them in order): audio_self_ahead_cm (25) straight in
// front of your head, facing as your head - heard centred, in both ears. Further emitters (footsteps, the sword on your hip, other
// people) keep their real place: below / beside you is where they are.
typedef void (*FnE)(u8 *); static FnE oEmitter = nullptr;
typedef int (*FnSetPos)(u64, const float *); static FnSetPos setPos = nullptr;
static float selfRadiusCm = 35.0f, selfAheadCm = 25.0f; static std::atomic<int> selfOn{1}; static std::atomic<u64> nSelf{0}, nEmit{0};
// 0.18.27 (owner: "the performance was much worse this time"; log: 55 eye renders / s against ~120 in 0.18.25): 0.18.26 checked every
// emitter (~2100 / s) with five VirtualQuery calls - a system call that walks the whole address space - on the audio update. Now
// nothing is queried: the game itself has just used the emitter, its source and owner, so the only test is a pointer compare - is the
// owner Jin's own node? - and only his emitters (a handful) go any further.
static bool emitterWorld(u8 *src, float *p) {   // the game's own formula (0x5B5D90); src / owner already validated by the caller
    u8 *own = *(u8 **)(src + 0x38); const int j = *(int *)(src + 0x48); const float *L = (const float *)(src + 0x50);
    const float *M = (const float *)(own + 0x150);
    if (j >= 0) { const float *W = *(const float **)(own + 0x278); if (!W || j >= fp::nJ) return false; M = W + (size_t)(j + 1) * 16; }
    for (int c = 0; c < 3; c++) p[c] = L[0] * M[c] + L[1] * M[4 + c] + L[2] * M[8 + c] + M[12 + c];
    return std::isfinite(p[0]) && std::isfinite(p[1]) && std::isfinite(p[2]);
}
static const float *w2a = nullptr;   // the game's world -> audio matrix (self-test: a stand-in)
static void toAudio(const float *w, float *a) {   // world -> audio position (the game's matrix)
    if (!w2a) w2a = (const float *)(g_base + RVA_AUDIO_W2A); const float *X = w2a;
    for (int c = 0; c < 3; c++) a[c] = w[0] * X[c] + w[1] * X[4 + c] + w[2] * X[8 + c] + X[12 + c]; }
static bool (*selfUse)() = []() -> bool { return g_cfg.xr == 4 && fp::activeNow && GetTickCount64() - headMs.load() < 250; };
static void hEmitter(u8 *e) {
    oEmitter(e); nEmit++;
    if (!e || !selfOn.load() || !on.load() || !setPos || !selfUse()) return;
    u8 *hero = fp::node; if (!hero) return;
    u8 *src = *(u8 **)(e + 0x28); if (!src || *(u8 **)(src + 0x38) != hero) return;   // not Jin's: done (no memory queries on this path)
    float H[16]; if (!read(H)) return;
    const float sc = g_cfg.unitsPerM * 0.01f;
    if (GetTickCount64() < g_muteJinUntil.load()) {   // 0.18.65 silenced: every one of Jin's own sounds (the sword on his hip too) placed 10 km away for the moment
        const u64 id = *(const u64 *)(e + 0x18); if (id == 0 || id >= (u64)-32) return;
        const float fw[3] = {H[12], H[13], H[14] + 1.0e6f * sc}; float t2[9] = {H[0], -H[1], H[2], H[8], -H[9], H[10], 0, 0, 0}; toAudio(fw, t2 + 6); setPos(id, t2); g_nMuted++; return; }
    float w[3]; if (!emitterWorld(src, w)) return;
    const float d[3] = {w[0] - H[12], w[1] - H[13], w[2] - H[14]};
    if (d[0] * d[0] + d[1] * d[1] + d[2] * d[2] > selfRadiusCm * selfRadiusCm * sc * sc) return;
    const u64 id = *(const u64 *)(e + 0x18); if (id == 0 || id >= (u64)-32) return;
    const float wp[3] = {H[12] + H[0] * selfAheadCm * sc, H[13] + H[1] * selfAheadCm * sc, H[14] + H[2] * selfAheadCm * sc};
    float t[9] = {H[0], -H[1], H[2], H[8], -H[9], H[10], 0, 0, 0}; toAudio(wp, t + 6);
    setPos(id, t); nSelf++;
    { static int told = 0; if (!told++) logf("3D audio: Jin's own sound (game object %llx, %.0f cm from your head) centred %.0f cm in front of you", (unsigned long long)id, sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]) / sc, selfAheadCm); }
}
} // namespace aud

// 0.18.26 the katana draw, game thread (see sword.h and fp::swordApply). The katana is a joint of Jin's own skeleton: one of the lone
// "gear" joints at his left hip (the skeleton dump: 645 / 646 / 647 at 87-96 cm, 25 cm to his left; 652-654 the same on his right).
// Which one carries the katana is a guess until seen in the headset, so a probe is built in: Ctrl+F7 lifts the next candidate 15 cm
// (watch what moves on Jin's hips), Ctrl+F8 keeps the lifted one as the katana (saved as sword_joint). The rail: the sheath mouth and
// the draw direction (out of the sheath, along the hilt); a default from the joint and Jin's body until Ctrl+F10 calibrates it: hold
// your LEFT hand where the sheath mouth is and your RIGHT hand at the end of the hilt and press Ctrl+F10 (saved to gotvr.ini, in the
// katana joint's own frame, so it rides with his hip).
namespace swd {
static float grip[2][3]; static int gripOk[2] = {0, 0}; static float squeeze[2] = {0, 0}, trigger[2] = {0, 0}, stick[2][2] = {{0, 0}, {0, 0}}; static u32 btns[2] = {0, 0};
static sword::Sys sys; static sword::Cfg cfg; static int on = 1, joint = 646, calOk = 0; static float mouthL[3] = {0, 0, 0}, axisL[3] = {1, 0, 0};
static std::atomic<int> probeIdx{-1}, probeHold{-1}; static int keepJ = -1; static const int kCand[] = {646, 647, 645, 653, 654, 652, 591, 592, 593, 594, 586, 587, 588, 589, 590};
static float railMouth[3], railAxis[3]; static bool railOk = false; static std::atomic<int> calReq{0};
static int gameDraw = 0; static bool gameDrawn = false; static u64 nPinned[2] = {0, 0}; static float fw[2] = {0, 0};   // 0.18.28 (0.18.29: the game's draw off by default - owner: "free flowing combat, not the game's")
// 0.18.29 hands locked on the sword, the blade joint apart from the sheath, sheathing, the game's attacks held off
static float gripM[2][16]; static std::atomic<int> resetReq{0}; static int blade = -1; static std::atomic<int> probeMode{0};   // probeMode 0 = the sheath / katana joint (kCand), 1 = the blade (fp::bladeCand)
static fing::Aff lockL[2], drawLock; static bool lockOn[2] = {false, false}, drawLockOk = false; static float lw[2] = {0, 0}, bladeAxF[3] = {0, 0, 1};
static float twoSteerK = 0.5f, mouthBelowGuard = 4.5f, bladeOutCm = 45.0f, mouthShift = 0.0f; static float secTipBest = 1e9f, secTipA = 0, secTipP = 0, secTipD = 0; static float guardCfg = -1.0f, rightHandCm = 9.0f, leftHandCm = 4.5f; static float pubML[3] = {0, 0, 0}, pubAL[3] = {0, 0, 1}; static bool pubMLok = false; static std::atomic<int> adjReq[2]; /* 0.18.38 */
static float intoGripCm = 0.0f, thumbGapCm = 0.3f, sheathRCm = 1.6f; static int wristFwd = 1; static float wwt = 0.0f; static int fromJoints = 1, underHoldOn = 1; static float bladePitch = 0.0f; static bool twoHand = false, twoHover = false; static float twoGap = 10.0f, twoT = 0.0f; static u64 nTwoFrames = 0; /* 0.18.32 */ static float tw2 = 0.0f, fwdT = 0.0f, dsw = 0.0f, bladeSign = 0.0f; static int bladeSignCfg = 0; static fing::Aff cap[2]; static bool capOk[2] = {false, false}; static float drawRot[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1}; static bool drawRotOk = false; static int edgeFlip = 0; static int gameBladeJ = -1, useGameBlade = 0; static u64 gbReqAt = 0; static int gbReqKind = 0, gbResends = 0; static u64 nGbDraw = 0, nGbSheathe = 0; static std::atomic<int> capReq[2]; static u64 capAt[2] = {0, 0}; /* 0.18.33 */ static int blockAttacks = 1, maskWhileHeld = 1, keepDrawnMs = 4000; static u64 nKeepTaps = 0; static float secTipMs = 0.0f, lastTip[3] = {0, 0, 0}; static bool lastTipOk = false; static u64 nBlockedFrames = 0;
// per-second log accumulators
static u32 secBtn[2] = {0, 0}; static float secTrig[2] = {0, 0}, secGrip[2] = {0, 0}, secStick[2][2]; static u64 secHeldS = 0, secHeldH = 0, secFrames = 0; static float secMaxS = 0;
static void iniWrite(const wchar_t *k, float v) { wchar_t ini[MAX_PATH]; swprintf(ini, MAX_PATH, L"%ls\\gotvr.ini", g_dir); wchar_t b[32]; swprintf(b, 32, L"%.3f", v); WritePrivateProfileStringW(L"gotvr", k, b, ini); }
static void load(const wchar_t *ini) {
    auto f = [&](const wchar_t *k, float d) { wchar_t b[32]; GetPrivateProfileStringW(L"gotvr", k, L"", b, 32, ini); return b[0] ? (float)_wtof(b) : d; };
    on = GetPrivateProfileIntW(L"gotvr", L"sword_draw", 1, ini) ? 1 : 0; joint = GetPrivateProfileIntW(L"gotvr", L"sword_joint", 646, ini);
    cfg.bladeCm = f(L"sword_blade_cm", 72.0f); cfg.hiltCm = f(L"sword_hilt_cm", 25.0f); cfg.grabCm = f(L"sword_grab_cm", 12.0f); cfg.hiltGrabCm = f(L"sword_hilt_grab_cm", 10.0f); cfg.needSheathHand = GetPrivateProfileIntW(L"gotvr", L"sword_need_sheath_hand", 0, ini) ? 1 : 0; gameDraw = 0; blade = GetPrivateProfileIntW(L"gotvr", L"sword_blade_joint", -1, ini); keepJ = GetPrivateProfileIntW(L"gotvr", L"sword_keep_joint", -1, ini);
    { wchar_t t[8]; if (!GetPrivateProfileStringW(L"gotvr", L"sword_keep_drawn_ms", L"", t, 8, ini)) WritePrivateProfileStringW(L"gotvr", L"sword_keep_drawn_ms", L"4000", ini); keepDrawnMs = GetPrivateProfileIntW(L"gotvr", L"sword_keep_drawn_ms", 4000, ini); }
    // 0.18.62 (owner: "we need Jin's movement animations to still play where the headset is facing, otherwise when you move backwards you see
    // his body"): holding Jin's whole upper body while the katana is out froze his movement animation - OFF by default now (an 0.18.61 ini set
    // to 0 once); the game's sword is kept out by the guard tap instead (sword_keep_drawn_ms), and its draw / put-away stay masked as before
    { wchar_t t[8]; if (!GetPrivateProfileStringW(L"gotvr", L"sword_mask_v", L"", t, 8, ini)) { WritePrivateProfileStringW(L"gotvr", L"sword_mask_while_held", L"0", ini); WritePrivateProfileStringW(L"gotvr", L"sword_mask_v", L"2", ini); }
      if (GetPrivateProfileIntW(L"gotvr", L"sword_mask_v", 0, ini) < 3) { WritePrivateProfileStringW(L"gotvr", L"sword_mask_while_held", L"2", ini); WritePrivateProfileStringW(L"gotvr", L"sword_mask_v", L"3", ini); }   // 0.18.64: 2 = only while standing
      maskWhileHeld = GetPrivateProfileIntW(L"gotvr", L"sword_mask_while_held", 2, ini); if (maskWhileHeld < 0 || maskWhileHeld > 2) maskWhileHeld = 2; }
    cfg.drawCm = f(L"sword_draw_pull_cm", 40.0f); intoGripCm = f(L"sword_hand_into_grip_cm", 0.0f); thumbGapCm = f(L"sword_thumb_gap_cm", 0.3f); wristFwd = GetPrivateProfileIntW(L"gotvr", L"sword_blade_forward", 1, ini) ? 1 : 0; fromJoints = GetPrivateProfileIntW(L"gotvr", L"sword_rail_from_joints", 1, ini) ? 1 : 0; bladePitch = f(L"sword_blade_pitch_deg", 0.0f); bladeSignCfg = GetPrivateProfileIntW(L"gotvr", L"sword_blade_sign", 0, ini); bladeSignCfg = bladeSignCfg > 0 ? 1 : bladeSignCfg < 0 ? -1 : 0; underHoldOn = GetPrivateProfileIntW(L"gotvr", L"sword_hold_body_still", 1, ini) ? 1 : 0; sheathRCm = f(L"sword_sheath_radius_cm", 1.6f); cfg.freeExt = f(L"sword_free_arm_reach", 0.93f); cfg.freeMinFrac = f(L"sword_free_min_frac", 0.6f); blockAttacks = GetPrivateProfileIntW(L"gotvr", L"sword_block_game_attacks", 1, ini) ? 1 : 0;
    cfg.sheathTipCm = f(L"sword_sheathe_tip_cm", 12.0f); cfg.sheathDeg = f(L"sword_sheathe_deg", 55.0f); cfg.sheathOutCm = 12.0f;   /* 0.18.42 (owner: "increase the radius"; 0.18.41 log: tries at the mouth were 37-46 deg off the rail, caught only under 35) */
    calOk = GetPrivateProfileIntW(L"gotvr", L"sword_calibrated", 0, ini);
    for (int h = 0; h < 2; h++) { capOk[h] = GetPrivateProfileIntW(L"gotvr", h ? L"sword_cap_r_ok" : L"sword_cap_l_ok", 0, ini) != 0;
        if (capOk[h]) { wchar_t k2[40]; for (int q = 0; q < 12; q++) { swprintf(k2, 40, L"sword_cap_%ls_%d", h ? L"r" : L"l", q); const float v = f(k2, q % 4 == 0 && q < 9 ? 1.0f : 0.0f); if (q < 9) cap[h].R[q] = v; else cap[h].t[q - 9] = v; } } }
    drawRotOk = GetPrivateProfileIntW(L"gotvr", L"sword_drawrot_ok", 0, ini) != 0; if (drawRotOk) { wchar_t k2[40]; for (int q = 0; q < 9; q++) { swprintf(k2, 40, L"sword_drawrot_%d", q); drawRot[q] = f(k2, q % 4 == 0 ? 1.0f : 0.0f); } }
    edgeFlip = GetPrivateProfileIntW(L"gotvr", L"sword_edge_flip", 0, ini) ? 1 : 0;
    gameBladeJ = (int)f(L"sword_drawn_blade_joint", -1.0f); useGameBlade = GetPrivateProfileIntW(L"gotvr", L"sword_use_game_blade", 1, ini) ? 1 : 0;   /* 0.18.39 back on: the sword alone, the animation masked */
    { const float hc = f(L"sword_game_home_cm", -1.0f); if (hc >= 0.0f) { wchar_t k2[40]; fing::Aff R; for (int q = 0; q < 12; q++) { swprintf(k2, 40, L"sword_game_home_%d", q); const float v = f(k2, q % 4 == 0 && q < 9 ? 1.0f : 0.0f); if (q < 9) R.R[q] = v; else R.t[q - 9] = v; } fp::homeX = R; fp::homeOk = 1; fp::homeBestCm = hc; } }
    twoSteerK = fminf(1.0f, fmaxf(0.0f, f(L"sword_two_hand_steer", 0.5f)));
    mouthBelowGuard = f(L"sword_mouth_below_guard_cm", 4.5f); bladeOutCm = f(L"sword_blade_out_cm", 45.0f); mouthShift = f(L"sword_sheathe_shift_cm", 0.0f); { const float am = f(L"sword_game_autosheathe_ms", 0.0f); if (am > 3000.0f && am < 120000.0f) fp::autoAfterMs = (u64)am; } cfg.releaseHome = GetPrivateProfileIntW(L"gotvr", L"sword_release_home", 0, ini) ? 1 : 0; cfg.autoHome = GetPrivateProfileIntW(L"gotvr", L"sword_auto_home", 0, ini) ? 1 : 0;   /* 0.18.40 */
    {   // 0.18.38: the hands' places on the katana (owner: "the ini settings for grip placement don't seem to do anything"), and the
        // 0.18.37 game-sword keys removed (the game is never asked to draw / sheathe again)
        wchar_t b[32]; GetPrivateProfileStringW(L"gotvr", L"sword_guard_cm", L"auto", b, 32, ini); guardCfg = (b[0] && _wcsicmp(b, L"auto") != 0) ? (float)_wtof(b) : -1.0f;
        rightHandCm = f(L"sword_right_hand_cm", 9.0f); leftHandCm = f(L"sword_left_hand_cm", 4.5f);
        auto def2 = [&](const wchar_t *k, const wchar_t *v) { wchar_t c[8]; if (!GetPrivateProfileStringW(L"gotvr", k, L"", c, 8, ini)) WritePrivateProfileStringW(L"gotvr", k, v, ini); };
        def2(L"sword_guard_cm", L"auto"); def2(L"sword_right_hand_cm", L"9"); def2(L"sword_left_hand_cm", L"4.5");
        const wchar_t *gone[] = {L"sword_grip_lower_cm", L"sword_sheath_hand_cm", L"sword_game_draw", L"sword_game_grip_ok", L"taa_per_eye"};
        def2(L"sword_use_game_blade", L"1"); def2(L"sword_two_hand_steer", L"0.5"); def2(L"sword_mouth_below_guard_cm", L"4.5"); def2(L"sword_blade_out_cm", L"45"); def2(L"sword_sheathe_tip_cm", L"12"); def2(L"sword_sheathe_deg", L"55"); def2(L"sword_sheathe_shift_cm", L"0"); def2(L"sword_release_home", L"0"); def2(L"sword_auto_home", L"0");
        for (const wchar_t *k : gone) WritePrivateProfileStringW(L"gotvr", k, NULL, ini);
        for (int q = 0; q < 12; q++) { wchar_t k2[40]; swprintf(k2, 40, L"sword_game_grip_%d", q); WritePrivateProfileStringW(L"gotvr", k2, NULL, ini); } }
    if (false) { wchar_t k2[40]; fing::Aff R; for (int q = 0; q < 12; q++) { swprintf(k2, 40, L"sword_game_grip_%d", q); const float v = f(k2, q % 4 == 0 && q < 9 ? 1.0f : 0.0f); if (q < 9) R.R[q] = v; else R.t[q - 9] = v; } fp::relHand = R; fp::relHandOk = 1; }
    {   // 0.18.33 (owner: "I can't find the setting in the ini"): the katana settings are written into gotvr.ini with their defaults when missing
        auto def = [&](const wchar_t *k, const wchar_t *v) { wchar_t b[8]; if (!GetPrivateProfileStringW(L"gotvr", k, L"", b, 8, ini)) WritePrivateProfileStringW(L"gotvr", k, v, ini); };
        def(L"sword_blade_pitch_deg", L"0"); def(L"sword_blade_forward", L"1"); def(L"sword_draw_pull_cm", L"40");
        def(L"sword_free_min_frac", L"0.6"); def(L"sword_free_arm_reach", L"0.93"); def(L"sword_thumb_gap_cm", L"0.3"); def(L"sword_rail_from_joints", L"1"); def(L"sword_hold_body_still", L"1");
        def(L"sword_block_game_attacks", L"1"); def(L"sword_need_sheath_hand", L"0"); def(L"fp_index_tween_s", L"0.07"); def(L"fp_finger_tween_s", L"0.15"); }
    mouthL[0] = f(L"sword_mouth_x", 0); mouthL[1] = f(L"sword_mouth_y", 0); mouthL[2] = f(L"sword_mouth_z", 0); axisL[0] = f(L"sword_axis_x", 1); axisL[1] = f(L"sword_axis_y", 0); axisL[2] = f(L"sword_axis_z", 0);
    logf("katana draw: %s, katana (sheath) joint %d, blade joint %d%s, %s (blade %.0f cm, hilt %.0f cm, grab radius %.0f cm%s)", on ? "ON" : "off (sword_draw = 0)", joint, blade, blade < 0 ? " (not chosen: the whole katana + sheath moves - Ctrl+F7 / Ctrl+F8 picks the blade)" : "", calOk ? "rail calibrated" : "rail from the default guess (Ctrl+F10 calibrates)", cfg.bladeCm, cfg.hiltCm, cfg.grabCm, cfg.needSheathHand ? ", left hand on the sheath needed" : "");
}
static void calibrate(const float *W) {   // left grip = the sheath mouth, right grip = the end of the hilt -> in the joint's own frame
    if (!gripOk[0] || !gripOk[1]) { logf("katana draw: Ctrl+F10 needs both controllers tracked"); return; }
    fing::Aff A = fing::fromPose(W), Ai; if (!fing::inv(A, Ai)) return;
    fing::Aff Lm; for (int i = 0; i < 9; i++) Lm.R[i] = (i % 4 == 0) ? 1.0f : 0.0f; for (int c = 0; c < 3; c++) Lm.t[c] = grip[0][c];
    fing::Aff Rp = Lm; for (int c = 0; c < 3; c++) Rp.t[c] = grip[1][c];
    const fing::Aff ml = fing::mul(Ai, Lm), pl = fing::mul(Ai, Rp); float ax[3] = {pl.t[0] - ml.t[0], pl.t[1] - ml.t[1], pl.t[2] - ml.t[2]}; const float L = sqrtf(ax[0] * ax[0] + ax[1] * ax[1] + ax[2] * ax[2]);
    if (!(L > 3.0f)) { logf("katana draw: Ctrl+F10 - the hands are only %.1f units apart, hold them at the sheath mouth and the hilt end", L); return; }
    for (auto &x : ax) x /= L; const float u = g_cfg.unitsPerM * 0.01f;
    memcpy(mouthL, ml.t, 12); memcpy(axisL, ax, 12); cfg.hiltCm = L / u; calOk = 1; sys.reset();
    iniWrite(L"sword_mouth_x", mouthL[0]); iniWrite(L"sword_mouth_y", mouthL[1]); iniWrite(L"sword_mouth_z", mouthL[2]); iniWrite(L"sword_axis_x", axisL[0]); iniWrite(L"sword_axis_y", axisL[1]); iniWrite(L"sword_axis_z", axisL[2]);
    iniWrite(L"sword_hilt_cm", cfg.hiltCm); iniWrite(L"sword_calibrated", 1.0f);
    logf("katana draw: CALIBRATED - sheath mouth (%.1f %.1f %.1f) in joint %d, draw direction (%.2f %.2f %.2f), hilt %.0f cm (saved to gotvr.ini)", mouthL[0], mouthL[1], mouthL[2], joint, axisL[0], axisL[1], axisL[2], cfg.hiltCm);
    vrin::pulse(0, 0.6f, 40); vrin::pulse(1, 0.6f, 40);
}
// 0.18.29 a rotation matrix (row-major, column vectors) taken part of the way from the identity: R^w (through its quaternion)
static void rotPart(const float *R, float w, float *o) {
    float q[4]; const float tr = R[0] + R[4] + R[8];
    if (tr > 0.0f) { const float S = sqrtf(tr + 1.0f) * 2.0f; q[3] = 0.25f * S; q[0] = (R[7] - R[5]) / S; q[1] = (R[2] - R[6]) / S; q[2] = (R[3] - R[1]) / S; }
    else if (R[0] > R[4] && R[0] > R[8]) { const float S = sqrtf(1.0f + R[0] - R[4] - R[8]) * 2.0f; q[3] = (R[7] - R[5]) / S; q[0] = 0.25f * S; q[1] = (R[1] + R[3]) / S; q[2] = (R[2] + R[6]) / S; }
    else if (R[4] > R[8]) { const float S = sqrtf(1.0f + R[4] - R[0] - R[8]) * 2.0f; q[3] = (R[2] - R[6]) / S; q[0] = (R[1] + R[3]) / S; q[1] = 0.25f * S; q[2] = (R[5] + R[7]) / S; }
    else { const float S = sqrtf(1.0f + R[8] - R[0] - R[4]) * 2.0f; q[3] = (R[3] - R[1]) / S; q[0] = (R[2] + R[6]) / S; q[1] = (R[5] + R[7]) / S; q[2] = 0.25f * S; }
    if (q[3] < 0) for (auto &x : q) x = -x;
    const float id[4] = {0, 0, 0, 1}; float r[4]; stab::qslerp(id, q, w < 0 ? 0 : w > 1 ? 1 : w, r);
    const float x = r[0], y = r[1], z = r[2], ww = r[3];
    o[0] = 1 - 2 * (y * y + z * z); o[1] = 2 * (x * y - z * ww); o[2] = 2 * (x * z + y * ww);
    o[3] = 2 * (x * y + z * ww); o[4] = 1 - 2 * (x * x + z * z); o[5] = 2 * (y * z - x * ww);
    o[6] = 2 * (x * z - y * ww); o[7] = 2 * (y * z + x * ww); o[8] = 1 - 2 * (x * x + y * y);
}
// 0.18.31 the rail through the katana joint (origin) and its child cl (both in the katana joint's frame) when the child lies along the
// calibrated line (within 35 deg, 15..40 cm away): the end nearer the calibrated mouth is the mouth, the axis points to the other end
static bool railFromJoints(const float *mouthC, const float *axisC, const float *cl, float u, float *mOut, float *aOut, float &ang, float &d0, float &d1) {
    const float dl = sqrtf(cl[0] * cl[0] + cl[1] * cl[1] + cl[2] * cl[2]); if (!(dl > 15.0f * u && dl < 40.0f * u)) return false;
    const float dn[3] = {cl[0] / dl, cl[1] / dl, cl[2] / dl}; const float ca = dn[0] * axisC[0] + dn[1] * axisC[1] + dn[2] * axisC[2]; ang = acosf(fminf(1.0f, fabsf(ca))) * 57.2957795f;
    d0 = sqrtf(mouthC[0] * mouthC[0] + mouthC[1] * mouthC[1] + mouthC[2] * mouthC[2]);
    d1 = sqrtf((mouthC[0] - cl[0]) * (mouthC[0] - cl[0]) + (mouthC[1] - cl[1]) * (mouthC[1] - cl[1]) + (mouthC[2] - cl[2]) * (mouthC[2] - cl[2]));
    if (!(ang < 35.0f)) return false;
    if (d0 <= d1) { mOut[0] = mOut[1] = mOut[2] = 0.0f; for (int k = 0; k < 3; k++) aOut[k] = dn[k]; } else { memcpy(mOut, cl, 12); for (int k = 0; k < 3; k++) aOut[k] = -dn[k]; }
    return true;
}
// 0.18.38 where the hands go on the katana, in cm along the rail from its origin (the katana joint):
//  - the guard: sword_guard_cm, or "auto" = your captured left grip (Ctrl+F6, thumb on the guard) + a fist's half width, else 16.5
//  - the right fist: sword_right_hand_cm below the guard (Ctrl+PageDown / Ctrl+PageUp move it 1 cm down / up the hilt, live)
//  - the left fist on the sheath: sword_left_hand_cm below the guard (Ctrl+End / Ctrl+Home move it 1 cm down / up the sheath)
static float capAxial(int h) { const float u = g_cfg.unitsPerM * 0.01f; return (cap[h].t[0] * pubAL[0] + cap[h].t[1] * pubAL[1] + cap[h].t[2] * pubAL[2]) / u; }
static float guardNow() { if (guardCfg >= 0.0f) return guardCfg; if (capOk[0] && pubMLok) return capAxial(0) + cfg.handHalfCm; return 16.5f; }
static float sheathG() { return guardNow() - leftHandCm; }
static fing::Aff capShifted(int h) {   // a captured grip, moved along the rail to where the settings put that hand (rotation and the offset off the line kept)
    fing::Aff c = cap[h]; if (!pubMLok) return c; const float u = g_cfg.unitsPerM * 0.01f;
    const float want = h ? sword::Sys::rightT(cfg) : sheathG(), d = (want - capAxial(h)) * u; for (int k = 0; k < 3; k++) c.t[k] += pubAL[k] * d; return c; }
static bool swingStab(int h) { return h == 1 ? (sys.state == sword::S_DRAWN || sys.state == sword::S_SHEATHING) : twoHand; }   // 0.18.35
static void frame(fp::ArmPub &ap) {   // game thread, pair phase 0, first person active, after the controller poses (before ap is published)
    const int pr = probeIdx.load();
    float W[16]; const bool kOk = fp::kGameRead(W, fp::node);
    // (0.18.27's left-trigger calibration removed in 0.18.28 - owner: "unbind the trigger from the temporary Ctrl+F10 binding"; Ctrl+F10 stays)
    if (calReq.exchange(0) && kOk) calibrate(W);
    if (resetReq.exchange(0)) { sys.reset(); lockOn[0] = lockOn[1] = false; drawLockOk = false; }
    static LARGE_INTEGER fq{}, lq{}; if (!fq.QuadPart) QueryPerformanceFrequency(&fq); LARGE_INTEGER qn; QueryPerformanceCounter(&qn);
    const float dt = lq.QuadPart ? fminf((float)(qn.QuadPart - lq.QuadPart) / (float)fq.QuadPart, 0.1f) : 0.0f; lq = qn;
    railOk = false; pubMLok = false;
    if (on && kOk && fp::node) {
        float mL[3] = {mouthL[0], mouthL[1], mouthL[2]}, aL[3] = {axisL[0], axisL[1], axisL[2]};
        if (calOk && fromJoints) {   // 0.18.31 (owner photos: the hands snapped beside the hilt / the sheath): the calibration came from where your
            // CONTROLLERS were, a few cm off the drawn katana. If the katana joint's child lies along the calibrated line (the other end of
            // the hilt), the rail is the line through the two joints - the drawn hilt itself - with the end nearer the calibrated mouth as the mouth
            float cl[3]; int chn = -1; for (int t = 0; t < 4; t++) { const LONG a = fp::kcSeq; if (a & 1) continue; MemoryBarrier(); memcpy(cl, fp::kChildL, 12); chn = fp::kChild.load(); MemoryBarrier(); if (a == fp::kcSeq) break; chn = -1; }
            const float u = g_cfg.unitsPerM * 0.01f, dl = sqrtf(cl[0] * cl[0] + cl[1] * cl[1] + cl[2] * cl[2]);
            if (chn >= 0) { float ang = 0, d0 = 0, d1 = 0; const bool use = railFromJoints(mouthL, axisL, cl, u, mL, aL, ang, d0, d1);
                static int told = -1; if (told != (use ? 1 : 0)) { told = use ? 1 : 0;
                    logf("katana: joint %d's child %d is %.1f cm away, %.0f deg from the calibrated hilt line, calibrated mouth %.1f cm from joint %d / %.1f cm from %d -> rail %s", joint, chn, dl / u, ang, d0 / u, joint, d1 / u, chn,
                         use ? "FROM THE TWO JOINTS (the drawn hilt)" : "from the calibration (the joints are not along it)"); }
                if (use) cfg.hiltCm = dl / u; }
        }
        pubML[0] = mL[0]; pubML[1] = mL[1]; pubML[2] = mL[2]; { float al = sqrtf(aL[0] * aL[0] + aL[1] * aL[1] + aL[2] * aL[2]); if (al > 1e-4f) for (int c = 0; c < 3; c++) pubAL[c] = aL[c] / al; } pubMLok = calOk;
        { static bool toldG = false; if (pubMLok && !toldG) { toldG = true; logf("katana: hands on the katana - guard %.1f cm out along the rail (%s), right fist %.1f cm below it (sword_right_hand_cm; Ctrl+PageDown / PageUp), left fist %.1f cm below it on the sheath (sword_left_hand_cm; Ctrl+End / Home)", guardNow(), guardCfg >= 0.0f ? "sword_guard_cm" : capOk[0] ? "auto: your captured left grip" : "auto", rightHandCm, leftHandCm); } }
        if (calOk) { for (int c = 0; c < 3; c++) { railMouth[c] = mL[0] * W[c] + mL[1] * W[4 + c] + mL[2] * W[8 + c] + W[12 + c]; railAxis[c] = aL[0] * W[c] + aL[1] * W[4 + c] + aL[2] * W[8 + c]; } }
        else {   // the default guess: the joint is the mouth, the hilt points forward and 25 deg up along the drawn body's heading
            const float *N = (const float *)(fp::node + 0x150); float F[3] = {N[0], N[1], 0}; float l = sqrtf(F[0] * F[0] + F[1] * F[1]); if (l > 1e-4f) { F[0] /= l; F[1] /= l; }
            const float b = fp::bodyYawCur, c = cosf(b), sn = sinf(b); const float Fd[2] = {F[0] * c - F[1] * sn, F[0] * sn + F[1] * c};
            const float a = 25.0f / 57.2957795f; railAxis[0] = Fd[0] * cosf(a); railAxis[1] = Fd[1] * cosf(a); railAxis[2] = sinf(a); for (int c2 = 0; c2 < 3; c2++) railMouth[c2] = W[12 + c2]; }
        float l = sqrtf(railAxis[0] * railAxis[0] + railAxis[1] * railAxis[1] + railAxis[2] * railAxis[2]); if (l > 1e-4f) { for (auto &x : railAxis) x /= l; railOk = true; }
    }
    const float u = g_cfg.unitsPerM * 0.01f;
    // 0.18.29 the frames: the sheath at its mouth (the katana joint's own axes), the blade = the sheath frame slid s along the rail
    fing::Aff Sf; if (kOk) { Sf = fing::fromPose(W); memcpy(Sf.t, railMouth, 12); }
    auto bladeFrame = [&](float sv) { fing::Aff b = Sf; for (int c = 0; c < 3; c++) b.t[c] += railAxis[c] * sv * u; return b; };
    // 0.18.33 the blade's direction in your hand (owner: "when I hold the controller forward, the katana also faces forward"; photos: 0.18.31's
    // grip -Z pointed it up, 0.18.32's grip -Y across to the right): the controller's forward = left x up of its rows (row 2 x row 0),
    // pitched by sword_blade_pitch_deg about its side
    float bladeDir[3]; { const float *r0 = gripM[1], *r2 = gripM[1] + 8; fbik::cross3(r2, r0, bladeDir); }
    if (bladePitch != 0.0f) { float ax[3] = {gripM[1][8], gripM[1][9], gripM[1][10]}; if (fbik::norm3(ax)) { const fbik::Rot R = fbik::axisAngle(ax, bladePitch / 57.2957795f); float t2[3]; fbik::rotv(R, bladeDir, t2); memcpy(bladeDir, t2, 12); } }
    fbik::norm3(bladeDir);
    {   // which way is the controller's forward (the rows' handedness is not documented): the first time the katana comes free with your
        // controller held level-ish, its forward must point the way your head faces - latched for the session (sword_blade_sign overrides)
        if (bladeSignCfg != 0) bladeSign = (float)bladeSignCfg;
        else if (bladeSign == 0.0f && sys.state == sword::S_DRAWN && fp::headOk) { const float d = bladeDir[0] * fp::headF[0] + bladeDir[1] * fp::headF[1];
            if (fabsf(d) > 0.5f) { bladeSign = d > 0 ? 1.0f : -1.0f; logf("katana: the controller's forward = %s of its rows (latched for this session; sword_blade_sign = 1 / -1 overrides)", d > 0 ? "row 2 x row 0" : "row 0 x row 2"); } }
        const float sg = bladeSign == 0.0f ? 1.0f : bladeSign; for (auto &x : bladeDir) x *= sg;
    }
    // 0.18.33 the drawn katana (world), from the RIGHT CONTROLLER - never from where Jin's hand is drawn (playbook: steer with the raw pose):
    //  1. as you took the hilt: blade = grip o drawLock^-1 (the hilt through your grip point, along your fist as you held it in the sheath)
    //  2. turned about the held point so the blade runs along the controller's forward (eased in over 0.25 s after it comes free -
    //     the wrist turning the blade forward, as in a real draw); sword_blade_forward = 0 keeps 1.
    //  3. with the left hand on the hilt too: turned toward the line between your two controllers (twoSteer, a soft limit)
    // K1 (1 + 2) gates the second hand and K (+ 3) is the katana you see, so the second hand's steering never feeds back into its own gate.
    bool bOk = false; float tipW[3] = {0, 0, 0}, dirW[3] = {0, 0, 0}, guardW[3] = {0, 0, 0}, a1[3] = {0, 0, 0}, g1[3] = {0, 0, 0};
    fing::Aff Kd; for (int i2 = 0; i2 < 9; i2++) Kd.R[i2] = (i2 % 4 == 0) ? 1.0f : 0.0f; Kd.t[0] = Kd.t[1] = Kd.t[2] = 0;
    { const bool want = wristFwd && sys.state == sword::S_DRAWN; fwdT = fminf(1.0f, fmaxf(0.0f, fwdT + (want ? dt : -dt) / 0.25f)); }
    if (railOk && sys.state == sword::S_DRAWN && drawLockOk && gripOk[1]) {
        fing::Aff Li; if (fing::inv(drawLock, Li)) {
            const fing::Aff Kt = fing::mul(fing::fromPose(gripM[1]), Li); float at[3]; for (int c = 0; c < 3; c++) at[c] = Kt.R[c * 3] * bladeAxF[0] + Kt.R[c * 3 + 1] * bladeAxF[1] + Kt.R[c * 3 + 2] * bladeAxF[2];
            if (fbik::norm3(at)) {
                auto turnAbout = [&](const fing::Aff &A0, const fbik::Rot &R, const float *H) { fing::Aff B; for (int r = 0; r < 3; r++) for (int c = 0; c < 3; c++) B.R[r * 3 + c] = R.m[r * 3] * A0.R[c] + R.m[r * 3 + 1] * A0.R[3 + c] + R.m[r * 3 + 2] * A0.R[6 + c];
                    float d[3] = {A0.t[0] - H[0], A0.t[1] - H[1], A0.t[2] - H[2]}, e[3]; fbik::rotv(R, d, e); for (int c = 0; c < 3; c++) B.t[c] = H[c] + e[c]; return B; };
                float H[3]; for (int c = 0; c < 3; c++) H[c] = Kt.t[c] + at[c] * sys.hiltT * u;
                const float bt[3] = {-at[0], -at[1], -at[2]}; const float wf = 0.5f - 0.5f * cosf(fwdT * 3.14159265f);
                fing::Aff K1;
                if (drawRotOk) {   // 0.18.35 the captured drawn grip: the katana's turn relative to your controller, the hilt through the grip point at hiltT
                    const fing::Aff C = fing::fromPose(gripM[1]); float Rk[9]; for (int r = 0; r < 3; r++) for (int c = 0; c < 3; c++) Rk[r * 3 + c] = C.R[r * 3] * drawRot[c] + C.R[r * 3 + 1] * drawRot[3 + c] + C.R[r * 3 + 2] * drawRot[6 + c];
                    float Rd[9]; for (int r = 0; r < 3; r++) for (int c = 0; c < 3; c++) Rd[r * 3 + c] = Rk[r * 3] * Kt.R[c * 3] + Rk[r * 3 + 1] * Kt.R[c * 3 + 1] + Rk[r * 3 + 2] * Kt.R[c * 3 + 2];   // Rk Kt.R^T
                    float Rw[9]; rotPart(Rd, wf, Rw); for (int r = 0; r < 3; r++) for (int c = 0; c < 3; c++) K1.R[r * 3 + c] = Rw[r * 3] * Kt.R[c] + Rw[r * 3 + 1] * Kt.R[3 + c] + Rw[r * 3 + 2] * Kt.R[6 + c];
                    for (int c = 0; c < 3; c++) a1[c] = K1.R[c * 3] * bladeAxF[0] + K1.R[c * 3 + 1] * bladeAxF[1] + K1.R[c * 3 + 2] * bladeAxF[2]; fbik::norm3(a1);
                    for (int c = 0; c < 3; c++) { const float Hb = H[c] + (C.t[c] - H[c]) * wf; K1.t[c] = Hb - a1[c] * sys.hiltT * u; }
                } else { const fbik::Rot Rf = fbik::scaled(fbik::arc(bt, bladeDir), wf); K1 = turnAbout(Kt, Rf, H); fbik::rotv(Rf, at, a1); }
                memcpy(g1, K1.t, 12); Kd = K1;
                if (tw2 > 0.0f) { float L[3] = {grip[0][0] - grip[1][0], grip[0][1] - grip[1][1], grip[0][2] - grip[1][2]}; const float sep = sqrtf(L[0] * L[0] + L[1] * L[1] + L[2] * L[2]) / u;
                    if (fbik::norm3(L)) { const float fade = fminf(1.0f, fmaxf(0.0f, (sep - 4.0f) / 2.0f)) * (0.5f - 0.5f * cosf(tw2 * 3.14159265f)); float tg[3]; fp::twoSteer(a1, L, fade, tg);
                        float H1[3]; for (int c = 0; c < 3; c++) H1[c] = K1.t[c] + a1[c] * sys.hiltT * u; Kd = turnAbout(K1, fbik::scaled(fbik::arc(a1, tg), twoSteerK), H1);   /* 0.18.39 (owner: "when dual handing the sword angle changes a bit too much - make it half"): sword_two_hand_steer 0.5 */ fp::nTwoSteer++; } }
                if (cbt::acfg.weight > 0.0f) {   // 0.18.48 / 0.18.58 katana weight (design doc; Blade & Sorcery's spring-damper; attack.h Weight), turned about the held point
                    float dw0[3]; for (int c = 0; c < 3; c++) dw0[c] = Kd.R[c * 3] * bladeAxF[0] + Kd.R[c * 3 + 1] * bladeAxF[1] + Kd.R[c * 3 + 2] * bladeAxF[2];
                    // playbook ch02 (FEAR VR): an enumerated list of what resets the follow filter, each logged - a jump of the hilt (teleport,
                    // recenter, a body snap), a snap / smooth turn, the flat screen (menus, cutscenes), a frame too long or not finite
                    { static float lastH[3]; static bool haveH = false; static u64 lastTurn = 0; const char *why = nullptr;
                      const float jd = haveH ? sqrtf((Kd.t[0] - lastH[0]) * (Kd.t[0] - lastH[0]) + (Kd.t[1] - lastH[1]) * (Kd.t[1] - lastH[1]) + (Kd.t[2] - lastH[2]) * (Kd.t[2] - lastH[2])) / u : 0.0f;
                      if (!cbt::weightLag.have) why = nullptr; else if (!std::isfinite(jd) || jd > 40.0f) why = "the hilt jumped (teleport / recenter / body snap)";
                      else if (vrin::turnSeq.load() != lastTurn) why = "a snap / smooth turn"; else if (g_flatRender.load()) why = "the flat screen (menu / cutscene)"; else if (dt > 0.1f || dt <= 0.0f) why = "a frame too long";
                      if (why) { cbt::weightLag.have = false; cbt::nWeightResets++; static u64 lastLog = 0; const u64 tn = GetTickCount64(); if (tn - lastLog > 2000) { lastLog = tn; logf("SWORD: weight filter reset - %s", why); } }
                      memcpy(lastH, Kd.t, 12); haveH = true; lastTurn = vrin::turnSeq.load(); }
                    if (fbik::norm3(dw0)) { const atk::V3 lg0 = cbt::weightLag.step(atk::v3(-dw0[0], -dw0[1], -dw0[2]), dt, tw2, cbt::acfg); const float lgv[3] = {-lg0.x, -lg0.y, -lg0.z};   /* the filter works on the blade's FORWARD (tip side), so gravity lowers the tip */
                        float Hw[3]; for (int c = 0; c < 3; c++) Hw[c] = Kd.t[c] + dw0[c] * sys.hiltT * u; Kd = turnAbout(Kd, fbik::arc(dw0, lgv), Hw); } }
                for (int c = 0; c < 3; c++) dirW[c] = Kd.R[c * 3] * bladeAxF[0] + Kd.R[c * 3 + 1] * bladeAxF[1] + Kd.R[c * 3 + 2] * bladeAxF[2];
                if (fbik::norm3(dirW)) { memcpy(guardW, Kd.t, 12); for (int c = 0; c < 3; c++) tipW[c] = guardW[c] - dirW[c] * cfg.tipLenCm * u; bOk = true; }   // 0.18.41 the REAL tip (cfg.tipLenCm)
            }
        }
    }
    if (bOk) {   // swing groundwork: the tip's speed (m/s), the highest each second in SWORD/s
        if (lastTipOk && dt > 0.0f) { const float d[3] = {tipW[0] - lastTip[0], tipW[1] - lastTip[1], tipW[2] - lastTip[2]}; secTipMs = fmaxf(secTipMs, sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]) / g_cfg.unitsPerM / dt); }
        memcpy(lastTip, tipW, 12); lastTipOk = true;
        // 0.18.47 combat stage 1: the blade against the enemies in reach (closing speed at the contact, swept tip, re-arm, cooldown);
        // a blow is queued for after the tick and felt at once (sword_hit_rumble), a touch too slow to cut is a light tap
        // 0.18.48 the attack around it (attack.h, the design doc's parameters), body-relative so walking is never a swing: a recognised
        // swing ticks the hand (sword_swing_rumble); two hands + the right trigger held still for sword_heavy_charge_ms charges a HEAVY
        // blow (sword_heavy_charge_move_cm of hand movement allowed), felt as a rising hum and a thump when charged; a blow lunges
        // Jin toward the enemy (sword_lunge_*)
        const bool two = tw2 > 0.5f; float hb[3]; const bool hbOk = cbt::rdF3(fp::heroPtr(), hb) > 0; const float u2 = 100.0f / g_cfg.unitsPerM;
        LARGE_INTEGER fqA, qcA; QueryPerformanceFrequency(&fqA); QueryPerformanceCounter(&qcA); const u64 nowA = (u64)(qcA.QuadPart * 1000 / fqA.QuadPart);
        if (hbOk) {
            const atk::V3 tipR = atk::v3((tipW[0] - hb[0]) * u2, (tipW[1] - hb[1]) * u2, (tipW[2] - hb[2]) * u2);
            atk::V3 hands[2]; for (int h = 0; h < 2; h++) hands[h] = atk::v3((grip[h][0] - hb[0]) * u2, (grip[h][1] - hb[1]) * u2, (grip[h][2] - hb[2]) * u2);
            const uint32_t ev = cbt::intent.step(nowA, tipR, hands, two, trigger[1], cbt::acfg); static u64 lastHum = 0;
            if (ev & atk::E_CHARGE_START) vrin::pulse(1, 0.3f, 25);
            if (ev & atk::E_CHARGED) { vrin::pulse(0, cbt::swingRumbleHeavy, 90); vrin::pulse(1, cbt::swingRumbleHeavy, 90); cbt::secCharges++; }
            if (ev & atk::E_CHARGE_LOST) logf("katana: heavy charge %s", cbt::intent.heavies ? "let go" : "let go before a blow");
            if (cbt::intent.charge == 1 && nowA - lastHum >= 150) { lastHum = nowA; const float pr = fminf(1.0f, (float)(nowA - cbt::intent.chargeAt) / (float)cbt::acfg.chargeMs); vrin::pulse(0, 0.3f + 0.3f * pr, 25); vrin::pulse(1, 0.3f + 0.3f * pr, 25); }
            else if (cbt::intent.heavyReady() && nowA - lastHum >= 300) { lastHum = nowA; vrin::pulse(0, 0.3f, 25); vrin::pulse(1, 0.3f, 25); }
            if (ev & atk::E_SWING) { cbt::secSwings++; if (cbt::intent.heavyReady()) { vrin::pulse(0, 1.0f, (u32)cbt::heavySwingMs); vrin::pulse(1, 1.0f, (u32)cbt::heavySwingMs); logf("katana: HEAVY swing released (big rumble %d ms)", cbt::heavySwingMs); } else vrin::pulse(1, cbt::swingRumble, 30); }   /* 0.18.61 (owner: "I couldn't tell if the heavy went through"): a big rumble on the heavy swing itself */
        }
        const bool heavy = cbt::intent.heavyReady(); float hp[3] = {0, 0, 0}, hs = 0;
        const int hr = cbt::vrBlade(guardW, tipW, two, heavy, hp, &hs);
        if (hr == 1) {
            const float amp = fminf(1.0f, cbt::rumble * (heavy ? 1.0f : 0.85f)); vrin::pulse(1, amp, (u32)cbt::rumbleMs + (heavy ? 30u : 0u)); if (two) vrin::pulse(0, amp * 0.8f, (u32)cbt::rumbleMs);
            if (heavy) { cbt::intent.spend(); cbt::secHeavy++; }
            if (cbt::lungeOn && hbOk) { const float dx = hp[0] - hb[0], dy = hp[1] - hb[1], dist = sqrtf(dx * dx + dy * dy) * u2; const float d = cbt::intent.lunge(nowA, hs, dist, heavy, cbt::acfg);
                if (d > 0.0f) { const u32 ms = (u32)(d / (cbt::lungeSpeed * 100.0f) * 1000.0f); vrin::lunge(atan2f(dy, dx) * 57.2957795f, ms); cbt::secLunges++; logf("katana: lunge %.0f cm toward the enemy (%u ms, enemy %.0f cm away)", d, ms, dist); } }
        }
        else if (hr == 2) vrin::pulse(1, 0.35f, 30);
    } else { lastTipOk = false; cbt::weightLag.have = false; }
    // 0.18.33 (owner: "assign a hotkey so I can put my hand in the correct general area and you can capture it"): Ctrl+F5 captures your
    // RIGHT controller as it should hold the hilt, Ctrl+F6 your LEFT controller as it should hold the sheath. Press the key, then within
    // 3 s put the controller there (a tick each second on that controller); at 3 s its pose relative to the sheath is saved (gotvr.ini,
    // sword_cap_r_* / sword_cap_l_*) and used for every grab from then on (where along the hilt / sheath the hand sits, and how it is
    // turned). Ctrl+Shift+F5 / F6 clears it.
    for (int h = 0; h < 2; h++) {
        const int rq = capReq[h].exchange(0); const u64 nowC = GetTickCount64();
        if (rq == 2) { capOk[h] = false; capAt[h] = 0; if (h) { drawRotOk = false; wchar_t ini2[MAX_PATH]; swprintf(ini2, MAX_PATH, L"%ls\\gotvr.ini", g_dir); WritePrivateProfileStringW(L"gotvr", L"sword_drawrot_ok", L"0", ini2); } wchar_t ini[MAX_PATH]; swprintf(ini, MAX_PATH, L"%ls\\gotvr.ini", g_dir); WritePrivateProfileStringW(L"gotvr", h ? L"sword_cap_r_ok" : L"sword_cap_l_ok", L"0", ini); logf("katana: %s hand grip capture cleared", h ? "right" : "left"); }
        else if (rq == 1) { capAt[h] = nowC; vrin::pulse(h, 0.3f, 20); logf("katana: capturing your %s hand's grip in 3 s - %s", h ? "right" : "left", h && sys.state == sword::S_DRAWN ? "point the right controller straight ahead like the sword, and look straight ahead" : h ? "hold the right controller where Jin's right hand should hold the hilt" : "hold the left controller where Jin's left hand should hold the sheath"); }
        if (capAt[h]) { const u64 el = nowC - capAt[h]; static u64 ticked[2] = {0, 0};
            if (el / 1000 > ticked[h] && el < 3000) { ticked[h] = el / 1000; vrin::pulse(h, 0.25f, 15); }
            if (el >= 3000 && h == 1 && sys.state == sword::S_DRAWN && kOk && gripOk[1] && fp::headOk) { capAt[h] = 0; ticked[h] = 0;
                // 0.18.35 (owner photo: holding the controller forward, the katana points left; "the grip needs to rotate 90 deg about x and 90
                // about y"): with the katana DRAWN, Ctrl+F5 captures how it should sit in your hand - point the right controller straight ahead
                // like a sword and look straight ahead: the blade is set along your view's forward, its edge down, and that orientation is kept
                // relative to your controller from then on (sword_drawrot_*; Ctrl+Shift+F5 clears both right-hand captures)
                const float p = fp::headPitchDeg.load() / 57.2957795f; float fw[3] = {cosf(p) * fp::headF[0], cosf(p) * fp::headF[1], sinf(p)}; fbik::norm3(fw);
                float up[3] = {0, 0, 1}; { const float d = fbik::dot3(up, fw); for (int c = 0; c < 3; c++) up[c] -= d * fw[c]; fbik::norm3(up); }
                float aL[3] = {bladeAxF[0], bladeAxF[1], bladeAxF[2]}; fbik::norm3(aL);
                float eL[3] = {Sf.R[6], Sf.R[7], Sf.R[8]};   // world up in the sheath's frame (row 2 of R = R^T (0,0,1)): the edge, worn edge-up
                { const float d = fbik::dot3(eL, aL); for (int c = 0; c < 3; c++) eL[c] -= d * aL[c]; if (!fbik::norm3(eL)) { eL[0] = 1; eL[1] = 0; eL[2] = 0; } }
                const float aW[3] = {-fw[0], -fw[1], -fw[2]}; float eW[3] = {-up[0], -up[1], -up[2]}; if (edgeFlip) for (auto &x : eW) x = -x;
                const fbik::Rot Rt = fbik::frameTo(aL, eL, aW, eW); const fing::Aff C = fing::fromPose(gripM[1]);
                for (int r = 0; r < 3; r++) for (int c = 0; c < 3; c++) drawRot[r * 3 + c] = C.R[r] * Rt.m[c] + C.R[3 + r] * Rt.m[3 + c] + C.R[6 + r] * Rt.m[6 + c];   // C.R^T Rt
                drawRotOk = true; wchar_t ini[MAX_PATH]; swprintf(ini, MAX_PATH, L"%ls\\gotvr.ini", g_dir); wchar_t k2[40], b[32];
                for (int q = 0; q < 9; q++) { swprintf(k2, 40, L"sword_drawrot_%d", q); swprintf(b, 32, L"%.5f", drawRot[q]); WritePrivateProfileStringW(L"gotvr", k2, b, ini); }
                WritePrivateProfileStringW(L"gotvr", L"sword_drawrot_ok", L"1", ini); vrin::pulse(1, 0.8f, 60);
                logf("katana: DRAWN grip CAPTURED - the blade along your view (pitch %.0f deg), edge down, kept relative to your right controller (saved; Ctrl+Shift+F5 clears)", p * 57.2957795f); }
            if (el >= 3000) { capAt[h] = 0; ticked[h] = 0;
                const float dm[3] = {grip[h][0] - railMouth[0], grip[h][1] - railMouth[1], grip[h][2] - railMouth[2]}; const float dmc = sqrtf(dm[0] * dm[0] + dm[1] * dm[1] + dm[2] * dm[2]) / u;
                fing::Aff Si; if (!railOk || !kOk || !gripOk[h] || sys.state != sword::S_SHEATHED || !(dmc < 45.0f) || !fing::inv(Sf, Si)) logf("katana: %s grip NOT captured (%s; the controller %.0f cm from the sheath mouth, needs < 45 cm with the katana sheathed)", h ? "right" : "left", !railOk ? "no rail" : !gripOk[h] ? "controller not tracked" : sys.state != sword::S_SHEATHED ? "the katana is out" : "too far", dmc);
                else { cap[h] = fing::mul(Si, fing::fromPose(gripM[h])); capOk[h] = true; sys.reset(); lockOn[0] = lockOn[1] = false;
                    float ax[3]; for (int c = 0; c < 3; c++) ax[c] = Sf.R[c] * railAxis[0] + Sf.R[3 + c] * railAxis[1] + Sf.R[6 + c] * railAxis[2]; const float al = (cap[h].t[0] * ax[0] + cap[h].t[1] * ax[1] + cap[h].t[2] * ax[2]) / u;
                    wchar_t ini[MAX_PATH]; swprintf(ini, MAX_PATH, L"%ls\\gotvr.ini", g_dir); wchar_t k2[40], b[32];
                    for (int q = 0; q < 12; q++) { swprintf(k2, 40, L"sword_cap_%ls_%d", h ? L"r" : L"l", q); swprintf(b, 32, L"%.5f", q < 9 ? cap[h].R[q] : cap[h].t[q - 9]); WritePrivateProfileStringW(L"gotvr", k2, b, ini); }
                    WritePrivateProfileStringW(L"gotvr", h ? L"sword_cap_r_ok" : L"sword_cap_l_ok", L"1", ini);
                    {   // 0.18.38 the capture also sets the hand's place along the katana (the settings / hotkeys move it from there)
                        const float g = guardNow(); if (h) { rightHandCm = al - g; iniWrite(L"sword_right_hand_cm", rightHandCm); } else { leftHandCm = g - al; iniWrite(L"sword_left_hand_cm", leftHandCm); }
                        logf("katana: the guard is %.1f cm out along the rail (%s); %s fist %.1f cm below it", g, guardCfg >= 0.0f ? "sword_guard_cm" : "auto", h ? "right" : "left", h ? rightHandCm : leftHandCm); }
                    vrin::pulse(h, 0.8f, 60); logf("katana: %s grip CAPTURED - %.1f cm %s the mouth along the %s, %.1f cm off its line (saved; Ctrl+Shift+%s clears)", h ? "right" : "left", fabsf(al), al >= 0 ? "out of" : "below", h ? "hilt" : "sheath",
                        sqrtf(fmaxf(0.0f, dmc * dmc - al * al)), h ? "F5" : "F6"); } } }
    }
    for (int h = 0; h < 2; h++) {   // 0.18.38 live: Ctrl+PageDown / PageUp (right hand down / up the hilt), Ctrl+End / Home (left hand down / up the sheath)
        const int d = adjReq[h].exchange(0); if (!d) continue; const float u2 = g_cfg.unitsPerM * 0.01f;
        if (h) { const float t0 = sword::Sys::rightT(cfg); rightHandCm = fminf(40.0f, fmaxf(0.0f, rightHandCm + (float)d)); cfg.rightCm = rightHandCm; const float t1 = sword::Sys::rightT(cfg), m = (t1 - t0) * u2;
            if (lockOn[1]) for (int c = 0; c < 3; c++) lockL[1].t[c] += pubAL[c] * m;
            if (drawLockOk) for (int c = 0; c < 3; c++) drawLock.t[c] += pubAL[c] * m;
            iniWrite(L"sword_right_hand_cm", rightHandCm); logf("katana: right fist %.0f cm below the guard%s (sword_right_hand_cm, saved)", rightHandCm, t1 == t0 ? " - at the end of the hilt already" : ""); }
        else { leftHandCm = fminf(40.0f, fmaxf(-10.0f, leftHandCm + (float)d)); if (lockOn[0]) for (int c = 0; c < 3; c++) lockL[0].t[c] -= pubAL[c] * (float)d * u2;
            iniWrite(L"sword_left_hand_cm", leftHandCm); logf("katana: left fist %.1f cm below the guard on the sheath (sword_left_hand_cm, saved)", leftHandCm); }
        vrin::pulse(h, 0.25f, 15); }
    sword::Out o{}; const int st0 = sys.state; const bool hs0 = sys.onSheath, hh0 = sys.onHilt;
    if (railOk && pr < 0) {
        sword::In in{}; for (int h = 0; h < 2; h++) { memcpy(in.hand[h], grip[h], 12); in.grip[h] = squeeze[h]; in.valid[h] = gripOk[h] != 0; }
        memcpy(in.mouth, railMouth, 12); memcpy(in.axis, railAxis, 12); in.dt = dt; in.unitsPerCm = u;
        in.bladeOk = bOk; memcpy(in.tip, tipW, 12); memcpy(in.dir, dirW, 12); in.armExt = fp::armExt[1].load();
        // 0.18.38: the hilt and the sheath grip where they really are (the guard guardNow() out along the rail), the fists at the settings'
        // places (0.18.35's grab radius widened to the captured left grip is not needed any more: the sheath grab is centred on it)
        cfg.guardCm = guardNow(); cfg.rightCm = rightHandCm; cfg.leftAx = sheathG(); cfg.mouthAx = cfg.guardCm - mouthBelowGuard;   // 0.18.40 the tip is taken at the real mouth
        cfg.tipLenCm = fmaxf(10.0f, bladeOutCm - cfg.mouthAx);   // 0.18.41
        cfg.mouthAx = sheathG() + mouthShift;   // 0.18.42 (owner: "put it more where my left hand sits on the sheath"): the catch at the left fist (sword.h: the tracked left hand while it holds the sheath)
        sys.step(in, cfg, o);
        if (sys.state == sword::S_DRAWN && bOk) { const float sc = sys.tipPerp + fmaxf(0.0f, sys.tipAx - cfg.sheathOutCm) + fmaxf(0.0f, -cfg.sheathInWinCm - sys.tipAx) + 0.3f * fmaxf(0.0f, sys.tipDeg - cfg.sheathDeg); if (sc < secTipBest) { secTipBest = sc; secTipA = sys.tipAx; secTipP = sys.tipPerp; secTipD = sys.tipDeg; } }   // 0.18.41 the closest the tip came to being taken, per second
        if (sys.onHilt || sys.state == sword::S_DRAWN || sys.state == sword::S_SHEATHING) sys.hiltT = sword::Sys::rightT(cfg);   // live (Ctrl+PageUp / PageDown)
        for (int h = 0; h < 2; h++) if (o.pulseHand[h]) vrin::pulse(h, o.pulseAmp[h], (u32)o.pulseMs[h]);
        { static float acc = 0; acc += dt; if (o.rumble > 0.0f && acc >= 0.04f) { acc = 0; vrin::pulse(1, o.rumble, 45); vrin::pulse(0, o.rumble * 0.5f, 45); } }   // the blade rasping in the sheath
    } else if (sys.state != sword::S_SHEATHED || sys.onHilt || sys.onSheath) { sys.reset(); o.event = true; o.what = "katana draw paused (no rail / probe on)"; }
    {   // 0.18.29 (owner: "the right hand is not totally gripping the hilt, a little above, and free to rotate - it needs to be locked onto
        // the hilt"; "the left hand needs to grip the sheath higher up, at the mouth"): a hand that takes the sword is LOCKED to it -
        // position and rotation. At the grab the controller's handle axis (grip -Z, the line through your fist) is turned onto the
        // sword's line (the right hand's index side toward the guard, the left hand's toward the mouth - whichever way round you took
        // it), the fist's centre is put ON the line (right: where you took the hilt, riding out with the blade; left: sword_sheath_hand_cm
        // from the mouth, -3), and that pose is kept relative to the blade / the sheath until you let go. The whole hand target (wrist,
        // fingers, thumb, palm) moves rigidly about the grip point, sword_hand_into_grip_cm (2) toward the palm so the handle sits in
        // the fist. Eased in over 0.1 s; out over 0.25 s (after the draw the hand, with the katana, settles back onto your controller).
        for (int h = 0; h < 2; h++) {
            const bool ride = railOk && gripOk[h] && (h ? (sys.onHilt && sys.state != sword::S_DRAWN) : sys.onSheath);
            if (ride && !lockOn[h]) {
                if (h && sys.state == sword::S_SHEATHING && drawLockOk) { lockL[1] = drawLock; lockOn[1] = true; }   // back on the rail: the hand where it holds the blade
                else if (capOk[h]) {   // 0.18.33 your captured grip (Ctrl+F5 right on the hilt / Ctrl+F6 left on the sheath); 0.18.38 slid along the rail to the settings' place
                    lockL[h] = capShifted(h); lockOn[h] = true;
                    if (h) { drawLock = lockL[1]; drawLockOk = true; for (int c = 0; c < 3; c++) bladeAxF[c] = Sf.R[c] * railAxis[0] + Sf.R[3 + c] * railAxis[1] + Sf.R[6 + c] * railAxis[2]; } }
                else {
                    fing::Aff C = fing::fromPose(gripM[h]); float rod[3] = {C.R[0], C.R[3], C.R[6]}; fbik::norm3(rod);   // 0.18.33: the fist's tube along the sword again (0.18.30, "the angle is right")
                    const float sd = rod[0] * railAxis[0] + rod[1] * railAxis[1] + rod[2] * railAxis[2];
                    const float sg = h ? (sd > 0.3f ? 1.0f : -1.0f) : (sd < -0.3f ? -1.0f : 1.0f); const float tg[3] = {railAxis[0] * sg, railAxis[1] * sg, railAxis[2] * sg};
                    { const fbik::Rot A = fbik::arc(rod, tg); float Rn[9];
                        for (int r = 0; r < 3; r++) for (int c = 0; c < 3; c++) Rn[r * 3 + c] = A.m[r * 3] * C.R[c] + A.m[r * 3 + 1] * C.R[3 + c] + A.m[r * 3 + 2] * C.R[6 + c];
                        memcpy(C.R, Rn, 36); }
                    const float ta = h ? sys.s + sys.hiltT : sheathG(); for (int c = 0; c < 3; c++) C.t[c] = railMouth[c] + railAxis[c] * ta * u;
                    const fing::Aff F = h ? bladeFrame(sys.s) : Sf; fing::Aff Fi;
                    if (fing::inv(F, Fi)) { lockL[h] = fing::mul(Fi, C); lockOn[h] = true;
                        if (h) { drawLock = lockL[1]; drawLockOk = true; for (int c = 0; c < 3; c++) bladeAxF[c] = Sf.R[c] * railAxis[0] + Sf.R[3 + c] * railAxis[1] + Sf.R[6 + c] * railAxis[2]; } }
                }
            }
            if (!ride) lockOn[h] = false;
            if (!gripOk[h] || !railOk) { lw[h] = 0.0f; continue; }
            lw[h] = fminf(1.0f, fmaxf(0.0f, lw[h] + (ride && lockOn[h] ? dt / 0.1f : -dt / 0.25f))); if (lw[h] <= 0.0f) continue;
            const fing::Aff L = fing::mul(h ? bladeFrame(sys.s) : Sf, lockL[h]); const fing::Aff C = fing::fromPose(gripM[h]);
            float Q[9]; for (int r = 0; r < 3; r++) for (int c = 0; c < 3; c++) Q[r * 3 + c] = L.R[r * 3] * C.R[c * 3] + L.R[r * 3 + 1] * C.R[c * 3 + 1] + L.R[r * 3 + 2] * C.R[c * 3 + 2];   // L.R C.R^T
            const float w = 0.5f - 0.5f * cosf(lw[h] * 3.14159265f); float Qw[9]; rotPart(Q, w, Qw);
            auto rot = [&](float *v) { const float a[3] = {v[0], v[1], v[2]}; for (int c = 0; c < 3; c++) v[c] = Qw[c * 3] * a[0] + Qw[c * 3 + 1] * a[1] + Qw[c * 3 + 2] * a[2]; };
            float d[3] = {ap.pos[h][0] - C.t[0], ap.pos[h][1] - C.t[1], ap.pos[h][2] - C.t[2]}; rot(d); rot(ap.fing[h]); rot(ap.thumb[h]); rot(ap.palm[h]);
            for (int c = 0; c < 3; c++) ap.pos[h][c] = C.t[c] + (L.t[c] - C.t[c]) * w + d[c] + ap.palm[h][c] * intoGripCm * u * w;
            nPinned[h]++;
        }
        {   // 0.18.32 / 0.18.33 (owner: "dual wielding - the left hand can grab the hilt when it is held in the right hand"; 0.18.32 log: the
            // two-hand hold took and dropped every 50 ms - its gate used the katana AS DRAWN, which the second hand itself steers). Playbook
            // HAND-017 / FEAR VR: one hand OWNS (the right; letting go of it ends the hold), the support hand only steers; gated on geometry
            // against the UNSTEERED katana (K1, from the right controller alone): the left grip within 12 cm of the hilt's line, 4..30 cm
            // behind the right hand toward the pommel; squeeze 0.65 / 0.45; leaves at 20 cm off the line or outside -3..40 cm.
            const bool canTwo = railOk && sys.state == sword::S_DRAWN && bOk && gripOk[0] && !sys.onSheath;
            float t = 0, perp = 1e9f;
            if (canTwo) { float H[3]; for (int c = 0; c < 3; c++) H[c] = g1[c] + a1[c] * sys.hiltT * u; const float d[3] = {grip[0][0] - H[0], grip[0][1] - H[1], grip[0][2] - H[2]}; t = (d[0] * a1[0] + d[1] * a1[1] + d[2] * a1[2]) / u;
                float q[3]; for (int c = 0; c < 3; c++) q[c] = d[c] - a1[c] * t * u; perp = sqrtf(q[0] * q[0] + q[1] * q[1] + q[2] * q[2]) / u; }
            const bool inZone = canTwo && perp < 12.0f && t > 4.0f && t < 30.0f;
            float sepCm = 0.0f; { const float d2[3] = {grip[0][0] - grip[1][0], grip[0][1] - grip[1][1], grip[0][2] - grip[1][2]}; sepCm = sqrtf(d2[0] * d2[0] + d2[1] * d2[1] + d2[2] * d2[2]) / u; }
            if (inZone && !twoHover && !twoHand) vrin::pulse(0, 0.12f, 10); twoHover = inZone;
            if (!twoHand && inZone && squeeze[0] > 0.65f) { twoHand = true; twoT = t; vrin::pulse(0, 0.5f, 25); o.event = true; o.what = "left hand took the hilt too (two hands)"; }
            else if (twoHand && (!canTwo || squeeze[0] < 0.35f || sepCm > 90.0f)) {   /* 0.18.37 (owner: "increase the distance the left grip can stay attached by a good amount - when I swing it keeps coming apart"): only letting go, or the controllers 90 cm apart */   /* 0.18.36 (owner: "the left hand falls off too easily"; log: dropped at grip 1.0): once held, only letting go or a hand far off ends it */ twoHand = false; o.event = true; o.what = "left hand let go of the hilt (one hand)"; }
            if (twoHand) { twoGap = fminf(14.0f, fmaxf(8.0f, t)); nTwoFrames++; }
            tw2 = fminf(1.0f, fmaxf(0.0f, tw2 + (twoHand ? dt : -dt) / 0.15f));
        }
        // the finger hold poses (fingers wrapped round the hilt / the sheath), eased in and out over 0.12 s
        for (int h = 0; h < 2; h++) { const bool hf = railOk && gripOk[h] && (h ? (sys.onHilt || sys.state == sword::S_DRAWN || sys.state == sword::S_SHEATHING) : (sys.onSheath || twoHand));
            fw[h] = fminf(1.0f, fmaxf(0.0f, fw[h] + (hf ? dt : -dt) / 0.12f)); }
    }
    // 0.18.29 (owner: "I want free flowing combat... I do not want unsheathing the sword to begin any attack animations; when the player
    // unsheathes the sword they physically hold it, so all attack animations need to be disabled"): while your hand is on the hilt or
    // the blade is out, the game's light / heavy attack buttons (pad X / Y = Square / Triangle) are held off (sword_block_game_attacks)
    fp::underHold = (railOk && underHoldOn && ((sys.state != sword::S_DRAWN && (sys.onHilt || sys.onSheath)) || (sys.state == sword::S_DRAWN && sys.tipNear))) ? 1 : 0;   // 0.18.38 (owner: "Jin no longer moves underneath the headset"): only while a hand actually GRIPS the sheathed katana (or the tip is at the mouth) - a hand hanging within 25 cm of the hip held the body still most of the time
    { const bool out = on && (sys.onHilt || sys.state != sword::S_SHEATHED); vrin::blockMask = (blockAttacks && out) ? (u32)(vrin::XB_X | vrin::XB_Y | vrin::OX_RT | vrin::OX_LT | vrin::XB_BACK | vrin::XB_UP) : 0u;   /* 0.18.61 + Focus (LT) and standoff (RT): the triggers are the katana's while it is out */   /* 0.18.36: the triggers too (your index on the trigger while holding the katana fired R2 / L2 and counted as pad input - the idle hold let go) */ if (out && blockAttacks) nBlockedFrames++; }
    if (fp::autoSave.exchange(0)) iniWrite(L"sword_game_autosheathe_ms", (float)fp::autoAfterMs.load());   // 0.18.42 known from the start next time
    if (fp::homeSave.exchange(0)) { wchar_t ini[MAX_PATH]; swprintf(ini, MAX_PATH, L"%ls\\gotvr.ini", g_dir); wchar_t k2[40], b[32]; const fing::Aff R = fp::homeX;
        for (int q = 0; q < 12; q++) { swprintf(k2, 40, L"sword_game_home_%d", q); swprintf(b, 32, L"%.5f", q < 9 ? R.R[q] : R.t[q - 9]); WritePrivateProfileStringW(L"gotvr", k2, b, ini); }
        swprintf(b, 32, L"%.2f", fp::homeBestCm); WritePrivateProfileStringW(L"gotvr", L"sword_game_home_cm", b, ini); }
    {   // 0.18.39 / 0.18.40 the game's own sword (637) - drawn in the background, NEVER put away by the mod (owner 0.18.40: "I want sheathing
        // to be player driven, not an animation ... it keeps playing the sheathing animation multiple times"). The game is asked to draw
        // (touchpad swipe, a TOGGLE) only while your hand holds the hilt / the katana is out and the game's sword is known to be away, at
        // most once per 2.5 s. Its draw animation is masked on Jin (fp::maskApply). Sheathing is yours alone: the game's sword slides home
        // along the rail with your hand and stays there, in the scabbard, while the game thinks it is drawn.
        // (0.18.39's log: the "is it drawn" test read the mod's own last placement once the game had stopped animating its sword, so the
        // sheathe request was repeated every 2.5 s and each one toggled the game - draw, sheathe, draw ...)
        static u64 lastChord = 0; static bool weDrew = false; const u64 nw = GetTickCount64(); const bool gd = fp::gameDrawn.load() != 0;
        if (useGameBlade && gameBladeJ >= 0 && railOk && fp::parkOk) {
            const bool hold = sys.onHilt || sys.state != sword::S_SHEATHED;
            if (hold && !gd && nw - lastChord > 2500 && nw - g_drawChordAt.load() > 2500) { vrin::requestChord(vrin::XB_RIGHT | vrin::OX_RSR); lastChord = nw; g_drawChordAt = nw; weDrew = true; fp::maskUntil = nw + 3000; nGbDraw++; logf("katana: your hand is on the hilt - the game's own sword drawn in the background (its animation hidden)"); }
            if (hold && !gd && g_drawChordAt.load() == lastChord && nw - lastChord < 50) { const u64 mu = nw + 1500; if (g_muteJinUntil.load() < mu) g_muteJinUntil = mu; }   // 0.18.65 the hidden draw is silent too
            if (!gd && nw - lastChord > 2500) weDrew = false;   // the game put it away itself (a mount, a climb, an interaction)
            // 0.18.65 (owner, session 20: "after sheathing a sword, Jin's body stays locked in some 30 degree rotation to the left for a few seconds,
            // which looks weird when I move"): that was the game's drawn-sword stance - its sword stayed out (in the scabbard only for us) until
            // its own auto-sheathe. Once your katana is home and your hand has left the hilt for 0.4 s, the game is asked to sheathe too -
            // masked on Jin and silenced, one toggle (never within 2.5 s of another)
            { static u64 freeSince = 0; if (hold) freeSince = 0; else if (!freeSince) freeSince = nw;
              if (!hold && gd && weDrew && freeSince && nw - freeSince >= 400 && nw - lastChord > 2500 && nw - g_drawChordAt.load() > 2500) {
                  vrin::requestChord(vrin::XB_RIGHT | vrin::OX_RSR); lastChord = nw; g_drawChordAt = nw; weDrew = false; fp::maskUntil = nw + 2500; { const u64 mu = nw + 1800; if (g_muteJinUntil.load() < mu) g_muteJinUntil = mu; }
                  nGbSheathe++; logf("katana: your katana is home and your hand left the hilt - the game's own sword put away too (hidden and silent; no drawn stance)"); } }
            // 0.18.61 (owner: "sometimes when holding the sword Jin would still play the sheathing animation"; the game's out-of-combat
            // auto-sheathe came at 16-30 s, not at a fixed time): while the katana is in your hand the mask is held the whole time - Jin's
            // upper body keeps his pose from before the draw (the legs still walk), so no draw / stance / blood flick / sheathe of the game's shows
            // 0.18.64 (session 19: "sometimes when holding a sword Jin would take his sword stance, which turned his body"): mode 2 (default)
            // holds the pose from before the draw only while Jin STANDS (under 0.6 m/s) - no stance turn standing still, and the walking /
            // running animations still play the moment he moves (0.18.62's complaint about mode 1)
            if (sys.state == sword::S_DRAWN && (maskWhileHeld == 1 || (maskWhileHeld == 2 && fp::speedMs.load() < 0.6f))) { const u64 mu = nw + (maskWhileHeld == 2 ? 150 : 400); if (fp::maskUntil.load() < mu) fp::maskUntil = mu; }
            // 0.18.62 (owner: "the sheathing animation STILL played - not visually, but the sheath disappeared for a second and the sound still
            // happened"): the game puts its sword away by itself when nothing happens for a while (9.9 to 30 s after drawing) - masking hides
            // Jin's body but not the scabbard or the sound. While the katana is in your hand and no hostile is within 8 m, the game's guard is
            // tapped every sword_keep_drawn_ms (a guard counts as activity, so the timer never runs out; the tap is hidden by the mask)
            { static u64 lastKeep = 0; if (keepDrawnMs > 0 && sys.state == sword::S_DRAWN && gd && nw - lastKeep >= (u64)keepDrawnMs) { lastKeep = nw;
                  if (cbt::parryEngine == 0 && !cbt::ppend.on && !vrin::forceActive(vrin::XB_LB, nw) && cbt::nearest(800.0f) < 0) { vrin::forcePress(vrin::XB_LB, 60); nKeepTaps++; const u64 mu = nw + 350; if (fp::maskUntil.load() < mu) fp::maskUntil = mu; } } }   // the guard flick masked for 350 ms only
        }
        fp::maskStance = (weDrew && gd) ? 1 : 0;
    }
    {   // 0.18.28 the game's own draw / sheathe - 0.18.29: OFF by default (sword_game_draw = 0; owner: the physical sword replaces the game's)
        static int lastState = sword::S_SHEATHED;
        if (gameDraw && sys.state != lastState) {
            if (sys.state == sword::S_DRAWN && !gameDrawn) { vrin::requestChord(vrin::XB_RIGHT | vrin::OX_RSR); gameDrawn = true; logf("katana: asked the game to DRAW (touchpad swipe right)"); }
            else if (sys.state == sword::S_SHEATHED && gameDrawn) { vrin::requestChord(vrin::XB_RIGHT | vrin::OX_RSR); gameDrawn = false; logf("katana: asked the game to SHEATHE (touchpad swipe right)"); }
        }
        lastState = sys.state;
    }
    {   // publish for the job
        const int k = fp::spubIdx.load() == 0 ? 1 : 0; fp::SwordPub &sp = fp::spub[k]; sp.state = sys.state; sp.s = sys.s; memcpy(sp.axisW, railAxis, 12); sp.probe = pr; sp.probeHold = pr >= 0 ? probeHold.load() : -1; sp.keep = keepJ; sp.joint = on ? joint : -1; sp.blade = blade; memcpy(sp.mouthW, railMouth, 12); for (int h = 0; h < 2; h++) sp.snapW[h] = railOk ? 0.5f - 0.5f * cosf(lw[h] * 3.14159265f) : 0.0f; sp.hiltAx = sys.s + sys.hiltT; sp.hiltT = sys.hiltT; sp.thumbGap = thumbGapCm; sp.sheathR = sheathRCm; { const bool hw = wristFwd && railOk && gripOk[1] && (sys.onHilt || sys.state == sword::S_DRAWN || sys.state == sword::S_SHEATHING); wwt = fminf(1.0f, fmaxf(0.0f, wwt + (hw ? dt : -dt) / 0.15f)); sp.wristW = 0.5f - 0.5f * cosf(wwt * 3.14159265f); memcpy(sp.ctrlFwd, gripM[1], 12); }
        memcpy(sp.bladeW, bladeDir, 12); memcpy(sp.rawG[0], grip[0], 12); memcpy(sp.rawG[1], grip[1], 12); memcpy(sp.mouthL, pubML, 12); sp.mouthLok = pubMLok ? 1 : 0; sp.sheathG = sheathG(); sp.twoHand = twoHand ? 1 : 0; sp.twoGap = twoGap;
        sp.twoW = 0.5f - 0.5f * cosf(tw2 * 3.14159265f); sp.gameBlade = useGameBlade ? gameBladeJ : -1; sp.capNoSnap[0] = capOk[0] ? 1 : 0; sp.capNoSnap[1] = capOk[1] ? 1 : 0;
        sp.katOk = bOk ? 1 : 0; memcpy(sp.katR, Kd.R, 36); memcpy(sp.katT, Kd.t, 12); memcpy(sp.katAx, dirW, 12);
        { dsw = fminf(1.0f, fmaxf(0.0f, dsw + (bOk ? dt / 0.1f : -dt / 0.2f))); sp.dSnapW = 0.5f - 0.5f * cosf(dsw * 3.14159265f); }
        if (capOk[0]) { float ax[3]; for (int c = 0; c < 3; c++) ax[c] = Sf.R[c] * railAxis[0] + Sf.R[3 + c] * railAxis[1] + Sf.R[6 + c] * railAxis[2]; sp.sheathG = (cap[0].t[0] * ax[0] + cap[0].t[1] * ax[1] + cap[0].t[2] * ax[2]) / u; } sp.onHilt = (sys.onHilt || sys.state == sword::S_DRAWN) ? 1 : 0; sp.onSheath = sys.onSheath ? 1 : 0; for (int h = 0; h < 2; h++) sp.holdW[h] = 0.5f - 0.5f * cosf(fw[h] * 3.14159265f); fp::spubIdx.store(k, std::memory_order_release);
    }
    // logging (owner: "log player input when holding the katana in the sheath and out of it; log when the player holds the hilt and / or the sheath")
    const bool holding = sys.onSheath || sys.onHilt || sys.state != sword::S_SHEATHED;
    if (o.event) logf("katana: %s | state %s, left on sheath %d, right on hilt %d, blade out %.0f of %.0f cm | left: grip %.2f trigger %.2f buttons 0x%x stick %.2f %.2f | right: grip %.2f trigger %.2f buttons 0x%x stick %.2f %.2f",
                      o.what, sword::kStateName[sys.state], (int)sys.onSheath, (int)sys.onHilt, sys.s, cfg.bladeCm, squeeze[0], trigger[0], btns[0], stick[0][0], stick[0][1], squeeze[1], trigger[1], btns[1], stick[1][0], stick[1][1]);
    (void)st0; (void)hs0; (void)hh0;
    if (holding) { secFrames++; if (sys.onSheath) secHeldS++; if (sys.onHilt) secHeldH++; secMaxS = fmaxf(secMaxS, sys.s);
        for (int h = 0; h < 2; h++) { secBtn[h] |= btns[h]; secTrig[h] = fmaxf(secTrig[h], trigger[h]); secGrip[h] = fmaxf(secGrip[h], squeeze[h]); for (int a = 0; a < 2; a++) if (fabsf(stick[h][a]) > fabsf(secStick[h][a])) secStick[h][a] = stick[h][a]; } }
}
static void perSecond() {
    logf("   SWORD/s: %s | joint %d%s, rail %s | left hand %.0f cm from the sheath mouth, right hand %.0f cm from the hilt | frames held %llu (sheath %llu, hilt %llu), blade out up to %.0f cm | input while held - left: buttons 0x%x trigger %.2f grip %.2f stick %.2f %.2f; right: buttons 0x%x trigger %.2f grip %.2f stick %.2f %.2f | hands locked on the sword %llu / %llu frames, hold fingers %llu / %llu, game drawn %d | blade joint %d, tip speed up to %.1f m/s, tip vs mouth (closest this second) %.0f cm along / %.0f cm off / %.0f deg (taken at -%.0f..+%.0f along, < %.0f off, < %.0f deg), game attacks held off %llu frames (%llu presses) | fists snapped onto the sword left %llu right %llu (finger circle %.1f / %.1f cm), left thumb on the guard %llu, katana through the fist %llu, right arm reach %.2f, blade-forward wrist %llu, sheath joint held %d, two hands %llu frames (steered %llu, gap %.0f cm), game draw census %llu | start-up turn kicks %llu | joint 637 %.0f cm from the katana joint | game sword %s (joint %d, scabbard place %s, in your hand %llu frames, draw / sheathe asked %llu / %llu, animation masked %llu frames, closest to the scabbard %.1f cm, parked %s, the game's own put-aways %llu (after %.1f s), away %llu frames, not animated %llu) | swing filter frames %llu, drawn grip captured %d, katana carried by the fist %llu (fist off its target %.1f cm) | katana moves %llu",
         on ? sword::kStateName[sys.state] : "off", joint, probeIdx.load() >= 0 ? " (PROBE: the lifted candidate)" : "", railOk ? (calOk ? "calibrated" : "default guess") : "none", sys.dSheath > 1e8f ? -1.0f : sys.dSheath, sys.dHilt > 1e8f ? -1.0f : sys.dHilt,
         (unsigned long long)secFrames, (unsigned long long)secHeldS, (unsigned long long)secHeldH, secMaxS, secBtn[0], secTrig[0], secGrip[0], secStick[0][0], secStick[0][1], secBtn[1], secTrig[1], secGrip[1], secStick[1][0], secStick[1][1], (unsigned long long)nPinned[0], (unsigned long long)nPinned[1], (unsigned long long)fp::nHoldPosed[0].exchange(0), (unsigned long long)fp::nHoldPosed[1].exchange(0), (int)gameDrawn, blade, secTipMs, secTipBest < 1e8f ? secTipA : 0.0f, secTipBest < 1e8f ? secTipP : 0.0f, secTipBest < 1e8f ? secTipD : 0.0f, cfg.sheathInWinCm, cfg.sheathOutCm, cfg.sheathTipCm, cfg.sheathDeg, (unsigned long long)nBlockedFrames, (unsigned long long)vrin::nBlocked.exchange(0), (unsigned long long)fp::nSnapped[0].exchange(0), (unsigned long long)fp::nSnapped[1].exchange(0), fp::fistRadCm[0].load(), fp::fistRadCm[1].load(), (unsigned long long)fp::nThumbAimed.exchange(0), (unsigned long long)fp::nKatanaInFist.exchange(0), fp::armExt[1].load(), (unsigned long long)fp::nWristFwd.exchange(0), keepJ, (unsigned long long)nTwoFrames, (unsigned long long)fp::nTwoSteer.exchange(0), twoGap, (unsigned long long)fp::nCensus.load(), (unsigned long long)vrin::nKicks.load(), fp::d637.load(), fp::gameDrawn.load() ? "DRAWN" : "sheathed", gameBladeJ, fp::homeOk.load() ? "learned" : "not learned yet (draw once)", (unsigned long long)fp::nGameBlade.exchange(0), (unsigned long long)nGbDraw, (unsigned long long)nGbSheathe, (unsigned long long)fp::nMasked.exchange(0), [] { const float v = fp::homeDistCm.exchange(1e9f); return v > 1e8f ? -1.0f : v; }(), fp::parkOk ? "found" : "not yet", (unsigned long long)fp::nAutoSheathe.load(), fp::autoAfterMs.load() / 1000.0, (unsigned long long)fp::nGap.exchange(0), (unsigned long long)fp::nNotAnimated.exchange(0), (unsigned long long)fp::nSwingStab.exchange(0), (int)drawRotOk, (unsigned long long)fp::nKatFistFix.exchange(0), fp::katFistErrCm.load(), (unsigned long long)fp::nSwordMoves.exchange(0)); nPinned[0] = nPinned[1] = 0; nBlockedFrames = 0; nTwoFrames = 0; secTipMs = 0.0f; secTipBest = 1e9f;
    secFrames = secHeldS = secHeldH = 0; secMaxS = 0; for (int h = 0; h < 2; h++) { secBtn[h] = 0; secTrig[h] = secGrip[h] = 0; secStick[h][0] = secStick[h][1] = 0; }
}
} // namespace swd
namespace rsz { static std::atomic<int> applying{0}; }   // 0.16.3: the game is applying a new render size (main thread blocked)
// 0.18.9 / 0.18.11: the first-person near clip plane (see apply); state shared by both eyes, restored when first person ends
namespace nearp {
static float nearOrig = -1.0f; static bool nearSet = false; static int told = 0;
static void apply(float *g, float sw, u8 *cb, bool active) {
                    // 0.18.9 (owner: "Jin's arms were see-through when really close to the camera"): the near clip plane in first person.
                    // Reversed-Z, row vector (CAMERA_FINDINGS): A = g[10] = -s*n/(f-n), B = g[14] = n*f/(f-n); the camera's own near (+0x664)
                    // is set too, so everything that linearises depth from it agrees; the game's near comes back when first person ends.
                    float *pn = (float *)(cb + 0x664); const float fz = *(float *)(cb + 0x668);
                    const float want = g_fp.nearCm * g_cfg.unitsPerM * 0.01f;
                    if (active && want > 0.0f) {
                        const float n0 = nearSet ? nearOrig : *pn;
                        if (std::isfinite(n0) && std::isfinite(fz) && fz > n0 && n0 > want) {
                            // 0.18.11: the projection is accepted when it was built with the game's near OR already with ours (the game
                            // rebuilds it from +0x664 on some ticks and resets +0x664 to its own value on others - 0.18.9 then left the
                            // camera saying 10 cm under a 4 cm projection, so depth was linearised wrongly on those ticks). Either way both
                            // the projection and +0x664 end up at ours, on every tick of both eyes (NEAR/s counts them per eye).
                            const float A0 = -sw * n0 / (fz - n0), B0 = n0 * fz / (fz - n0), Aw = -sw * want / (fz - want), Bw = want * fz / (fz - want);
                            auto near_ = [&](float a, float b) { return fabsf(g[10] - a) <= 1e-3f * fabsf(a) + 1e-6f && fabsf(g[14] - b) <= 1e-3f * fabsf(b) + 1e-4f; };
                            if (near_(A0, B0) || near_(Aw, Bw)) {
                                g[10] = Aw; g[14] = Bw;
                                if (!nearSet) { nearOrig = n0; nearSet = true; } *pn = want;
                                if (!told++) logf("first person: near clip plane %.1f -> %.1f cm (far %.0f)", n0, want, fz);
                                fp::nearHits[g_pairPhase & 1]++;
                            } else { fp::nearMiss++; if (told < 3) { told++; logf("first person: the projection does not match near %.2f or %.2f / far %.0f (A %.6f vs %.6f, B %.3f vs %.3f) -> near plane left alone", n0, want, fz, g[10], A0, g[14], B0); } }
                        }
                    } else if (nearSet) { *pn = nearOrig; nearSet = false; }
}
} // namespace nearp
// 0.18.14's culling apex pull-back (planes at +0x4F0 widened in first person) was removed in 0.18.15: no help, black edges (owner).
static void hSlot43(void *cam) {
    // BUG-64c: during a size change the swapchain thread runs RTPool_OnResize -> Cam_SetAspect -> this slot on CAM-A; no eye
    // pose belongs to that call (the main thread is blocked in the apply) -> the engine's own update only
    if (rsz::applying.load()) { oSlot43(cam); return; }
    if ((u64)cam == camA() && isCam((u64)cam)) {
        c_slot43A++; t_slot43 = GetCurrentThreadId();
        currentPacket();
        const FramePkt &pk = g_cur;
        u8 *cb = (u8 *)cam;
        float *w = (float *)(cb + 0x140); // rows: fwd(0..3) left(4..7) up(8..11) pos(12..15), Z-up world
        bool vr = pk.valid;
        // native stereo: the second render is composed for the right eye (pose2/fov2), the first for the left
        const bool right = g_runEye == 1;
        const GvPose &epose = (right && pk.stereo) ? pk.pose2 : pk.pose;
        const float *efov = (right && pk.stereo) ? pk.fov2 : pk.fov;
        const bool offT = right && m_offTest.load();
        bool poseOn = vr || m_yaw.load() || m_armed.load() || offT;
        // BUG-20 (0.8.0 F1/headset): the engine renders CAMERA-RELATIVE. Cam_vUpdateFrustum43 builds a rotation-only
        // view-projection and hands the position (+0x170) separately to Cam_SetViewProjFrustum(cam, pos, VPrel), which
        // stores pos (+0x550), VPrel (+0x430), the absolute VP T(-pos)*VPrel (+0x240..) and the planes. Positions of
        // what the frame gathered are relative to the position the camera had then (the left eye). So the right eye
        // keeps the LEFT eye's position and gets its own rotation/frustum, and the eye offset D = C_R - C_L goes into
        // its relative VP as a translation row: VPrel2 = T(-D)*VP_R -> absolute = T(-C_L)T(-D)VP_R = T(-C_R)VP_R.
        bool rfix = false; float posL[3] = {0, 0, 0}, dlt[3] = {0, 0, 0};
        if (poseOn) {
            float gm[16]; memcpy(gm, cacheIn(c_world, (u64)cam, w), 64);
            { const float fx = gm[0], fy = gm[1]; if (fx * fx + fy * fy > 1e-6f) vrin::gameTick(atan2f(fy, fx) * 57.2957795f); }   // 0.18.0 snap turn closes its loop on this (0.18.12: in the same update)
            if (vr) fp::update(gm);   // 0.17.0: first person - the eye anchor replaces the game camera's position (yaw stays the game's)
            stipple::tick(vr && fp::activeNow);   // 0.18.10
            if (vr) {   // 0.18.5: your forward direction for the body follow (game camera yaw + the VR turn, not the head's yaw)
                float rf[2] = {gm[0], gm[1]}; const float n = sqrtf(rf[0] * rf[0] + rf[1] * rf[1]);
                if (n > 1e-4f) { rf[0] /= n; rf[1] /= n; const float hd = vrin::viewHold.load(); if (hd != 0.0f) { const float a = hd / 57.2957795f; rotateZ(rf, cosf(a), sinf(a)); }
                    fp::refF[0] = rf[0]; fp::refF[1] = rf[1]; fp::refOk = true; }
            }
            auto compose = [&](const GvPose *pose, float *o) {
                memcpy(o, gm, 64);
                if (pose) {
                    if (g_cfg.level) {   // anchor = game camera position + yaw only; the head owns pitch and roll
                        float fx = o[0], fy = o[1], n = sqrtf(fx * fx + fy * fy);
                        if (n > 1e-4f) { fx /= n; fy /= n; o[0] = fx; o[1] = fy; o[2] = 0; o[4] = -fy; o[5] = fx; o[6] = 0; o[8] = 0; o[9] = 0; o[10] = 1; }
                    }
                    { const float hd = vrin::viewHold.load(); if (hd != 0.0f) { const float a = hd / 57.2957795f, c = cosf(a), s = sinf(a); rotateZ(o + 0, c, s); rotateZ(o + 4, c, s); rotateZ(o + 8, c, s); } }   // 0.18.1: VR-owned turn (snap / smooth), before the head so head motion turns with it
                    float Rg[3][3], dg[3]; poseLocal(*pose, Rg, dg);
                    float r[3][4]; memcpy(r, o, 48);
                    for (int i = 0; i < 3; i++) for (int c = 0; c < 3; c++) { float v = 0; for (int k = 0; k < 3; k++) v += Rg[k][i] * r[k][c]; o[i * 4 + c] = v; }
                    if (g_cfg.headPos) for (int c = 0; c < 3; c++) { float v = 0; for (int k = 0; k < 3; k++) v += dg[k] * r[k][c]; o[12 + c] += v * g_cfg.unitsPerM; }
                }
                if (m_yaw.load()) { const float a = 5.0f * 3.14159265f / 180.0f, c = cosf(a), s = sinf(a); rotateZ(o + 0, c, s); rotateZ(o + 4, c, s); rotateZ(o + 8, c, s); }
            };
            float o[16]; compose(vr ? &epose : nullptr, o);
            if (vr) {   // 0.18.7 FBIK inputs (game thread)
                { const float n = sqrtf(o[0] * o[0] + o[1] * o[1]); fp::headPitchDeg = atan2f(o[2], n) * 57.2957795f; if (n > 1e-4f) { fp::headF[0] = o[0] / n; fp::headF[1] = o[1] / n; fp::headOk = true;
                    vrin::headYawNow = atan2f(fp::headF[1], fp::headF[0]) * 57.2957795f; vrin::headYawOk = 1; const u64 mc = vrin::moveCapAt.load(); if (mc && GetTickCount64() >= mc) { vrin::moveCapAt = 0; vrin::moveYaw = atan2f(fp::headF[1], fp::headF[0]) * 57.2957795f; vrin::moveYawOk = 1; vrin::nMoveCaps++; } } }   // 0.18.7: where your head looks (0.18.17: + the movement forward)
                if (S && fp::activeNow && g_pairPhase == 0) {   // 0.18.16 body under the headset: your head's horizontal offset from the eye anchor (world)
                    GvPose hc{}; u32 hv = 0; gvRead(&S->poseSeq, [&] { hc = S->head; hv = S->poseValid; });
                    {   // 0.18.24 room-scale: Jin's distance covered while the stick walked him under your head comes off your head offset
                        // (the tracking reference moves with him in the game camera's level frame), so the view stays where your head is
                        static float applied[2] = {0, 0}; const float t0 = fp::rsCons[0].load(), t1 = fp::rsCons[1].load(); const float dw[2] = {t0 - applied[0], t1 - applied[1]}; applied[0] = t0; applied[1] = t1;
                        if (g_refValid && (dw[0] != 0.0f || dw[1] != 0.0f) && std::isfinite(dw[0]) && std::isfinite(dw[1])) {
                            if (rsShiftRef(gm, vrin::viewHold.load(), dw)) fp::rsApplied = fp::rsApplied.load() + sqrtf(dw[0] * dw[0] + dw[1] * dw[1]) / (g_cfg.unitsPerM * 0.01f); }
                    }
                    if (hv) { float oa[16]; compose(&hc, oa); aud::publish(oa); }   // 0.18.25 3D audio: the listener = your head
                    if (hv && (g_fp.bodyUnder || g_fp.roomscale) && g_cfg.headPos) { float oc[16]; compose(&hc, oc); fp::underW[0] = oc[12] - gm[12]; fp::underW[1] = oc[13] - gm[13]; }
                    else { fp::underW[0] = 0.0f; fp::underW[1] = 0.0f; }
                    {   // 0.18.24 room-scale: publish the offset (cm) and whether it is safe to walk Jin now
                        const float s0 = g_cfg.unitsPerM * 0.01f; vrin::rsOff[0] = fp::underW[0].load() / s0; vrin::rsOff[1] = fp::underW[1].load() / s0;
                        const u64 tn = GetTickCount64(), sinceJ = tn - vrin::jumpAt.load(), sinceAir = tn - fp::airAt.load();
                        const bool jumpWin = sinceJ < 1200 || (sinceJ < 4000 && sinceAir < 600);
                        if (g_fp.roomscale && hv && !fp::ridingNow.load() && fp::gateW.load() < 0.5f && !jumpWin && fabsf(fp::vertMs.load()) < 0.8f) vrin::rsOkUntil = tn + 100;
                    }
                }
                if (S && fp::activeNow && g_pairPhase == 0) {   // 0.18.7 FBIK: the controllers' grip poses -> world wrist targets (the same composition as the eyes)
                    GvHand hd[2]; u32 act = 0; const bool got = gvRead(&S->inSeq, [&] { hd[0] = S->hand[0]; hd[1] = S->hand[1]; act = S->inActive; });
                    static u64 lastMs = 0; static float tw[2] = {0, 0}; const u64 nowMs = GetTickCount64(); const float dt = lastMs ? fminf((float)(nowMs - lastMs) * 0.001f, 0.1f) : 0.0f; lastMs = nowMs;
                    const int k = fp::apubIdx.load() == 0 ? 1 : 0; fp::ArmPub &ap = fp::apub[k]; const float sc = g_cfg.unitsPerM * 0.01f;
                    for (int h = 0; h < 2; h++) {
                        const bool ok = got && act && (hd[h].poseValid & 1);
                        tw[h] = fminf(1.0f, fmaxf(0.0f, tw[h] + (ok ? dt : -dt) / 0.25f)); ap.w[h] = 0.5f - 0.5f * cosf(tw[h] * 3.14159265f); fp::armW[h] = ap.w[h];   // 1 - cos blend in / out
                        if (!ok) swd::gripOk[h] = 0;
                        ap.ok[h] = ok || ap.w[h] > 0.0f; if (!ok) { fp::stabLost[h] = 1; continue; }
                        {   // 0.18.24 hand stabilisation (stab.h): One Euro on the grip pose in tracking space, reset on every discontinuity
                            static stab::Filt sf[2]; static LARGE_INTEGER sq0{}, sfq{}; static LARGE_INTEGER lastS[2]{}; static LONG seenRc = 0;
                            if (!sfq.QuadPart) QueryPerformanceFrequency(&sfq); LARGE_INTEGER qn; QueryPerformanceCounter(&qn); (void)sq0;
                            const float sdt = lastS[h].QuadPart ? (float)(qn.QuadPart - lastS[h].QuadPart) / (float)sfq.QuadPart : 0.0f; lastS[h] = qn;
                            static bool rcPend[2] = {false, false}; { const LONG rc = S->theaterRecenter; if (rc != seenRc) { seenRc = rc; rcPend[0] = rcPend[1] = true; } }
                            if (g_fp.stab) { stab::Cfg sc2; sc2.minHz = g_fp.stabMinHz; sc2.beta = g_fp.stabBeta; sc2.rotMinHz = g_fp.stabRotMinHz; sc2.rotBeta = g_fp.stabRotBeta; sc2.deadMm = g_fp.stabDeadMm; sc2.deadDeg = g_fp.stabDeadDeg;
                                if (swd::swingStab(h)) { sc2.dHz = 10.0f; sc2.rotBeta = 25.0f; sc2.beta = 30.0f; sc2.rotMinHz = 2.0f; fp::nSwingStab++; }   // 0.18.35 (owner: "the katana lagged behind my hand when I swung it")
                                if (fp::stabLost[h]) { fp::stabLost[h] = 0; sf[h].init = false; fp::stabResets[stab::R_LOST]++; }
                                const int r = stab::step(sf[h], hd[h].grip.q, hd[h].grip.p, true, sdt, sc2, rcPend[h] ? stab::R_RECENTER : stab::R_NONE); rcPend[h] = false;
                                if (r != stab::R_NONE) { fp::stabResets[r]++; } else fp::stabFiltered++; fp::stabSpeed[h] = sf[h].v; }
                            else sf[h].init = false;
                        }
                        float oh[16]; compose(&hd[h].grip, oh);   // rows: fwd = grip -Z, left = grip -X, up = grip +Y (world)
                        memcpy(swd::grip[h], oh + 12, 12); memcpy(swd::gripM[h], oh, 64); swd::gripOk[h] = 1; swd::squeeze[h] = hd[h].squeeze; swd::trigger[h] = hd[h].trigger; swd::btns[h] = hd[h].buttons; swd::stick[h][0] = hd[h].stick[0]; swd::stick[h][1] = hd[h].stick[1];   // 0.18.26 katana draw
                        // hand frame from the grip (rows: fwd = -Z, left = -X, up = +Y). [HEADSET] 0.18.7 (-Y fingers, -Z thumb): upside down; 0.18.9 (+Y, -Z):
                        // "only right when the controller points straight down" -> 0.18.10 = 0.18.9 pitched up 90 deg: fingers along -Z, thumb along -Y
                        // 0.18.12 (owner: "the hands rotate around a point near the wrist, not the wrist itself; palms down the controllers are a
                        // few cm higher than the hands"): the grip pose's origin is the centre of the fist around the handle, which sits on the PALM
                        // side of the forearm's axis. The wrist was put only 7 cm back along the hand from it, so the hand hung low and turned about a
                        // point below the real wrist. Now the wrist = grip - fingers * back + backOfHand * up + thumb * side (OpenXR grip +X is the palm
                        // normal pointing to the user's right for both hands, so the back of the hand is -X on the left, +X on the right; rows: row 1 =
                        // grip -X). All three turn with the hand trims, so the pivot stays the wrist whatever the trims are.
                        float fing[3] = {oh[0], oh[1], oh[2]}, th[3] = {-oh[8], -oh[9], -oh[10]}, bk[3];
                        for (int c = 0; c < 3; c++) bk[c] = oh[4 + c] * (h ? -1.0f : 1.0f);
                        auto spin = [&](const float *ax, float deg) { if (deg == 0.0f) return; float k3[3] = {ax[0], ax[1], ax[2]}; if (!fbik::norm3(k3)) return; const fbik::Rot R = fbik::axisAngle(k3, deg / 57.2957795f); float t[3]; fbik::rotv(R, fing, t); memcpy(fing, t, 12); fbik::rotv(R, th, t); memcpy(th, t, 12); fbik::rotv(R, bk, t); memcpy(bk, t, 12); };
                        spin(oh + 4, g_fp.handPitch); spin(oh + 8, g_fp.handYaw * (h ? -1.0f : 1.0f)); spin(oh + 0, g_fp.handRoll * (h ? -1.0f : 1.0f));
                        for (int c = 0; c < 3; c++) { ap.pos[h][c] = oh[12 + c] + (-fing[c] * g_fp.handBackCm + bk[c] * g_fp.handUpCm + th[c] * g_fp.handSideCm) * sc; ap.fing[h][c] = fing[c]; ap.thumb[h][c] = th[c]; ap.palm[h][c] = -bk[c]; }
                        {   // 0.18.18 finger curl (God of War VR: a light squeeze is already a fist on Quest; grip_fist_full 0.35), smoothed 60 ms
                            static float gS[2] = {0, 0}, tS[2] = {0, 0}; const float full = fmaxf(0.1f, g_fp.gripFull);
                            const float gt = fminf(1.0f, fmaxf(0.0f, (hd[h].squeeze - 0.05f) / (full - 0.05f))), tt = fminf(1.0f, fmaxf(0.0f, (hd[h].trigger - 0.05f) / 0.9f));
                            // 0.18.20 (owner: "detect when thumbs are on the sticks and the in-game thumbs also go down"): the thumb follows the thumb
                            // touch (stick, face buttons, thumb rest); the index rests a third closed with a finger on the trigger
                            static float thS[2] = {0, 0}; const float tht = (hd[h].buttons & (GV_BTN_THUMBTOUCH | GV_BTN_THUMBREST)) ? 1.0f : 0.0f;
                            const float ttc = fmaxf(tt, (hd[h].buttons & GV_BTN_TRIGTOUCH) ? 0.35f : 0.0f);
                            // 0.18.21 (owner: "I want the fingers to animate / tween to their poses, not just unnaturally snap to them"): 0.18.20
                            // followed the inputs with a 60 ms lag - the thumb / trigger TOUCHES are on / off and a light squeeze is already a
                            // full fist, so the fingers jumped. Now each curl is a critically damped spring (eases out AND in, no overshoot),
                            // ~90 % there after fp_finger_tween_s (0.15 s), on its own high-resolution clock (one step per hand per frame).
                            static float gV[2] = {0, 0}, tV[2] = {0, 0}, thV[2] = {0, 0}; static LARGE_INTEGER fq0{}, lastQ[2]{};
                            if (!fq0.QuadPart) QueryPerformanceFrequency(&fq0);
                            LARGE_INTEGER qn; QueryPerformanceCounter(&qn);
                            const float dq = lastQ[h].QuadPart ? fminf((float)(qn.QuadPart - lastQ[h].QuadPart) / (float)fq0.QuadPart, 0.1f) : 0.0f; lastQ[h] = qn;
                            const float om = 3.9f / fmaxf(0.02f, g_fp.fingerTween);
                            fing::spring(gS[h], gV[h], gt, om, dq); fing::spring(tS[h], tV[h], ttc, 3.9f / fmaxf(0.02f, g_fp.indexTween), dq); /* 0.18.31 the index (trigger / its touch) on its own, faster tween */ fing::spring(thS[h], thV[h], tht, om, dq);   // 0.18.41: the thumb's spring had been swallowed by that comment since 0.18.31 (owner: "the thumb pose is not responding to my thumb on the sticks")
                            (void)dt;
                            ap.grip[h] = gS[h]; ap.trig[h] = tS[h]; ap.thumbC[h] = thS[h]; fp::fingCurlOut[h] = gS[h]; fp::thumbOut[h] = thS[h];
                        }
                    }
                    // the controller hand order is left = 0, right = 1, the same as fbik's sides (Jin's left arm = +y)
                    swd::frame(ap);   // 0.18.26 the katana draw (0.18.28: before the arm targets are published - it pins the hands to the sword)
                    fp::apubIdx.store(k, std::memory_order_release);
                }
            }
            if (right && ((vr && pk.stereo) || offT)) {
                float oL[16]; compose(vr ? &pk.pose : nullptr, oL);
                if (offT) for (int c = 0; c < 3; c++) o[12 + c] = oL[12 + c] - oL[4 + c] * 0.5f * g_cfg.unitsPerM;   // row 1 = left -> 0.5 m to the right
                for (int c = 0; c < 3; c++) { posL[c] = oL[12 + c]; dlt[c] = o[12 + c] - oL[12 + c]; o[12 + c] = oL[12 + c]; }
                rfix = true;
            }
            cacheOut(c_world, o);
            // BUG-35 (0.10.2, playbook ch01 "cull-camera ownership" / CAM-002): Cam_UpdateWorld 0x8081F0 keeps a copy of
            // the world it computed at +0x850..+0x88F (the GAME camera) right before it calls this slot. Distance-,
            // shadow- and streaming-side systems read that copy (0x611E00, 0x649ED0, 0x728260, 0x74F1F0, 0x5A4740 …),
            // so with head yaw on top of the game camera they culled what the head looked at ("stay gone until I look
            // forward again"). The copy now carries the eye camera too.
            if (vr && g_cfg.xr == 4 && m_cullHead.load()) { memcpy(cb + 0x850, o, 64); c_cullHead++; }
            // Camera-cut flag (+0x7D8, read by the Streamline/DLSS pass as "reset history", cleared by the engine at
            // frame end in Frame_59F2B0). AFR history-reset test: the shared TAA/DLSS history otherwise blends
            // every frame with the other eye's (6 cm shifted) image -> ghosting/flicker.
            int hr = m_histReset.load(); if (hr < 0) hr = g_cfg.histReset;
            if (vr && hr) *(int *)(cb + 0x7D8) = 1;
        } else cacheOffFor(c_world, (u64)cam);
        if (g_flatCut.load() > 0) { g_flatCut--; *(int *)(cb + 0x7D8) = 1; }   // flat <-> stereo switch: no temporal history across it
        if (m_offTest.load()) *(int *)(cb + 0x7D8) = 1;   // F1: no TAA blending with the other render (0.8.1 F1 double image)

        // Projection the original reads (+0x200): the eye's own asymmetric frustum (VR) or F8's left half.
        // Row-vector, s = P23 = +-1. ndc = P00*t + P20/s. The game's own P20/P21 (TAA jitter) are kept.
        float *P = (float *)(cb + 0x200), *T = (float *)(cb + 0x680);
        if (vr || m_leftHalf.load()) {
            float g[16]; memcpy(g, cacheIn(c_proj, (u64)cam, P), 64);
            float gt[2]; memcpy(gt, cacheIn(c_tan, (u64)cam, T), 8);
            float tH = gt[0], tV = gt[1], sw = g[11];
            if (tH > 1e-4f && tV > 1e-4f && (fabsf(sw - 1) < 1e-3f || fabsf(sw + 1) < 1e-3f)) {
                float l, r, u, d;
                if (vr) { l = tanf(efov[0]); r = tanf(efov[1]); u = tanf(efov[2]); d = tanf(efov[3]); }
                else { l = -tH; r = 0.0f; d = -tV; u = tV; }
                const float jx = g[8], jy = g[9];
                g[0] = 2 / (r - l); g[8] = -sw * (r + l) / (r - l) + jx;
                g[5] = 2 / (u - d); g[9] = -sw * (u + d) / (u - d) + jy;
                nearp::apply(g, sw, cb, vr && fp::activeNow);
                cacheOut(c_proj, g);
                // half-tangents (+0x680/+0x684) widened to cover the eye frustum, for any consumer that culls or
                // sizes effects from them (review #10); restored with the projection
                if (vr && g_cfg.widen) { float nt[2] = {fmaxf(-l, r), fmaxf(u, -d)}; cacheOut(c_tan, nt); } else cacheOffFor(c_tan, (u64)cam);
            } else { cacheOffFor(c_tan, (u64)cam); if (!g_projBad.exchange(1)) logf("projection layout not as expected (tanH=%f tanV=%f P23=%f) -> not injected", tH, tV, sw); }
        } else { cacheOffFor(c_proj, (u64)cam); cacheOffFor(c_tan, (u64)cam); }
        if (rfix) {
            oSlot43(cam);   // right-eye rotation + frustum, left-eye position
            float V[16]; memcpy(V, cb + 0x430, 64);
            for (int j = 0; j < 4; j++) V[12 + j] -= dlt[0] * V[j] + dlt[1] * V[4 + j] + dlt[2] * V[8 + j];
            float p4[4] = {posL[0], posL[1], posL[2], 1.0f};
            typedef void (*FnSVPF)(void *, const float *, const float *); ((FnSVPF)(g_base + RVA_CAM_SETVPF))(cam, p4, V);
            g_rfixCalls++; g_lastDelta[0] = dlt[0]; g_lastDelta[1] = dlt[1]; g_lastDelta[2] = dlt[2];
            return;
        }
    } else c_slot43Other++;
    oSlot43(cam);
}

static void hSetProjRect(void *cam, const float *rect) {
    if ((u64)cam == camA()) {
        c_projrectA++; t_projrect = GetCurrentThreadId();
        memcpy(g_lastRect, rect, 16); g_rectSeq++;
    }
    oSetProjRect(cam, rect);
}

// ---------------- R1 census (GoTVR 0.6.2): what every per-frame RenderView is, and how it is submitted ----------------
// FrameView_5A07F0: each list view is submitted with RV_Submit(view, mode) where mode comes from its camera's
// kind (+0x660: 2 -> 3 = both gather sets, like the main view; 1 -> 0) and then fanned out to gather jobs
// (Rnd_AddView 0x787AE0). The census logs which views exist, their frusta, and their submit modes.
typedef void (*FnSubmit)(void *, u64); static FnSubmit oRVSubmit;
struct SubRec { u64 view; u32 mode; };
static SubRec g_sub[32]; static int g_subN = 0; static u32 g_subFrame = 0xFFFFFFFFu;   // game thread
// ---------------- R1-A: a second run of the 69-pass render graph per frame (STEREO_ROUTE session 7) ----------------
// RV_Post_9B5180(view, ctx) schedules the whole frame for one view from a per-frame pool of pass contexts
// (0x141A0AB50, 254 x 0x2B20) and Jobs_SubmitGraph only submits contexts not yet submitted, so a second
// RV_Post for a second RenderView queues a complete second render behind the first. R1-A uses the SAME camera
// (zero-delta re-entry gate): expected = image unchanged, ~2x render cost, no crash. F5 toggles it.
namespace r1 {
typedef void (*FnCtor)(void *); typedef void (*FnPost)(void *, void *);
static u8 *V2 = nullptr; static std::atomic<int> on{0}; static u32 activeFrame = 0xFFFFFFFFu; static bool active = false;
static FnV1 oFvList; static FnPost oRVPost;
static std::atomic<u64> posts{0}, appendFail{0}, ctorFail{0}, activated{0};
static std::atomic<u32> flagMain{0}, flagV2{0}, poolAfter{0}, poolMid{0};
static u8 *frameObj() { return g_base + 0x1EBD2C0; }
static u8 *mainView() { return frameObj() + 0x1ED70; }
static bool ensureV2() {
    if (V2) return true;
    V2 = (u8 *)VirtualAlloc(nullptr, RV_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!V2) { ctorFail++; return false; }
    ((FnCtor)(g_base + RVA_RV_CTOR))(V2);
    logf("R1: second RenderView constructed at %p (engine ctor 0x%x, size 0x%x)", (void *)V2, (u32)RVA_RV_CTOR, (u32)RV_SIZE);
    return true;
}
static bool thisFrame() { return active && activeFrame == frameCtr(); }
// Single-use per-frame state that the second run must not overflow (BUG-11): the frame view-constants object
// 0x1434A26F0 keeps 4 block pointers at +0x1B8 with a counter at +0x1D8 that one pass (0x77E810, pass 13) fills
// completely; the engine only resets it at frame end (0x77F340). Reset the counter (not the ring) between runs.
enum : u32 { RVA_VIEWCONST = 0x34A26F0 };
static std::atomic<u32> vcBefore{0};
// BUG-15: the same object carries the frame's TILED LIGHT LISTS. Pass 13 (0x77E810) allocates a constant block
// (+0x1B0) and a small GPU argument buffer (32-byte buffer object at +0x1E0/+0x200, chosen by frame parity; slots
// +0x1B8.. point into its staging memory). At frame end, after all passes are recorded, 0x77F340 bins the frame's
// lights into 32-px tiles and patches the light-list descriptors (+0x00, +0x10..+0x40, +0x50..+0x80) into the
// LATEST block, the per-tile counts into the latest slots, and uploads the latest buffer. Run 2's pass 13 replaced
// all of that: run 1's buffer was released and run 1's block kept the "no lights" defaults, so run 1 rendered with
// no local lights (darker, bluer at night) and the shared TAA/DLSS history carried that into the output.
// Fix: bank run 1's state between the runs (run 2 gets its own buffer object with the same 2-frame lifetime), and
// at frame end lend run 2's finished light lists to run 1 (same frame, same lights; playbook ch14 "one view is
// the authority, the other reuses"). F4 toggles it (A/B).
struct VcRun { u8 *blk; u8 obj[0x20]; u64 slot[4]; };
static VcRun run1; alignas(16) static u8 bank2[2][0x20]; static bool bankInit = false;
static bool banked = false; static u32 bankPar = 0; static std::atomic<int> lendOn{1};
static std::atomic<u64> lends{0}, lendSkip{0}, run2NoFill{0}; static std::atomic<u32> lastLightDesc{0}, lastCount0{0};
static u8 *vcObj(u8 *vc, u32 par) { return vc + (0xF + par) * 0x20; }
static std::atomic<int> pjDump{0}; static void pjLog(); static void bSnapReset(); static void bLog();
static void resetBetweenRuns() {
    u8 *vc = g_base + RVA_VIEWCONST;
    vcBefore = *(u32 *)(vc + 0x1D8);
    banked = false;
    if (lendOn.load() && vcBefore.load() == 4 && *(u8 **)(vc + 0x1B0)) {
        typedef void *(*FnInit)(void *);
        if (!bankInit) { ((FnInit)(g_base + RVA_FN_BUFINIT))(bank2[0]); ((FnInit)(g_base + RVA_FN_BUFINIT))(bank2[1]); bankInit = true; }
        bankPar = frameCtr() & 1;
        run1.blk = *(u8 **)(vc + 0x1B0); memcpy(run1.obj, vcObj(vc, bankPar), 0x20); memcpy(run1.slot, vc + 0x1B8, 32);
        memcpy(vcObj(vc, bankPar), bank2[bankPar], 0x20);   // run 2 releases/allocates its own buffer, not run 1's
        banked = true;
    } else if (lendOn.load()) lendSkip++;
    *(int *)(vc + 0x1D8) = 0;
}
// after run 2: if its pass 13 did not run, put run 1's state back untouched
static void afterSecondRun() {
    if (!banked) return;
    u8 *vc = g_base + RVA_VIEWCONST;
    if (*(u8 **)(vc + 0x1B0) == run1.blk || *(int *)(vc + 0x1D8) != 4) {
        memcpy(bank2[bankPar], vcObj(vc, bankPar), 0x20); memcpy(vcObj(vc, bankPar), run1.obj, 0x20);
        *(u8 **)(vc + 0x1B0) = run1.blk; memcpy(vc + 0x1B8, run1.slot, 32); *(int *)(vc + 0x1D8) = 4;
        banked = false; run2NoFill++;
    }
}
typedef void (*FnVcEnd)(u8 *); static FnVcEnd oVcEnd;
static void pjSnap();
static void hVcEnd(u8 *vc) {
    if (vc == g_base + RVA_VIEWCONST) {
        pjSnap(); bSnapReset(); if (pjDump.load()) { pjDump = 0; pjLog(); bLog(); }
        if (g_runEye == 1) { g_runEye = 0; u64 a = camA(); if (isCam(a)) hSlot43((void *)a); }   // back to the left eye (all second-render jobs are recorded)
    }
    if (!banked || vc != g_base + RVA_VIEWCONST) { oVcEnd(vc); return; }
    banked = false;
    u8 *blk2 = *(u8 **)(vc + 0x1B0); u64 slot2[4]; memcpy(slot2, vc + 0x1B8, 32);
    oVcEnd(vc);                                       // engine finalizes run 2: light lists -> blk2, counts -> slot2, upload
    if (blk2 && run1.blk && blk2 != run1.blk) {
        *(u32 *)run1.blk = *(u32 *)blk2;
        for (int i = 0; i < 4; i++) { *(u32 *)(run1.blk + 0x10 + 16 * i) = *(u32 *)(blk2 + 0x10 + 16 * i); *(u32 *)(run1.blk + 0x50 + 16 * i) = *(u32 *)(blk2 + 0x50 + 16 * i); }
        lastLightDesc = *(u32 *)blk2;
    }
    for (int i = 0; i < 4; i++) if (run1.slot[i] && slot2[i]) *(u32 *)run1.slot[i] = *(u32 *)slot2[i];
    if (slot2[0]) lastCount0 = *(u32 *)slot2[0];
    memcpy(bank2[bankPar], vcObj(vc, bankPar), 0x20);  // run 2's buffer: released by the next run 2 with this parity
    memcpy(vcObj(vc, bankPar), run1.obj, 0x20);        // run 1's buffer: released by the next run 1 with this parity
    typedef void (*FnFin)(void *); ((FnFin)(g_base + RVA_FN_BUFFINISH))(vcObj(vc, bankPar));   // upload run 1's counts
    lends++;
}
// Fail visibly (playbook ch17 R1): log any access violation on the game thread during the second run.
static std::atomic<int> inSecond{0}; static DWORD gameTid = 0; static PVOID veh = nullptr;
static LONG CALLBACK onFault(EXCEPTION_POINTERS *e) {
    if (!inSecond.load() || GetCurrentThreadId() != gameTid) return EXCEPTION_CONTINUE_SEARCH;
    EXCEPTION_RECORD *r = e->ExceptionRecord; CONTEXT *c = e->ContextRecord;
    if (r->ExceptionCode != EXCEPTION_ACCESS_VIOLATION && r->ExceptionCode != 0xC0000409 && r->ExceptionCode != EXCEPTION_STACK_OVERFLOW) return EXCEPTION_CONTINUE_SEARCH;
    u64 rip = (u64)r->ExceptionAddress;
    logf("R1 FAULT in second graph run: code=%08lx at rva 0x%llx (abs %p) %s addr %p | rax=%llx rbx=%llx rcx=%llx rdx=%llx rsi=%llx rdi=%llx r8=%llx r14=%llx",
         r->ExceptionCode, (unsigned long long)(rip - (u64)g_base), (void *)rip, r->NumberParameters > 1 ? (r->ExceptionInformation[0] ? "write" : "read") : "",
         r->NumberParameters > 1 ? (void *)r->ExceptionInformation[1] : nullptr, (unsigned long long)c->Rax, (unsigned long long)c->Rbx, (unsigned long long)c->Rcx,
         (unsigned long long)c->Rdx, (unsigned long long)c->Rsi, (unsigned long long)c->Rdi, (unsigned long long)c->R8, (unsigned long long)c->R14);
    // return addresses inside the game on the stack (the pass that called it)
    u64 *sp = (u64 *)c->Rsp; char b[600]; int n = 0;
    for (int i = 0; i < 160 && n < 560 && readable(sp + i, 8); i++) { u64 v = sp[i]; if (v > (u64)g_base + 0x1000 && v < (u64)g_base + 0x1200000) n += snprintf(b + n, sizeof b - n, " %llx", (unsigned long long)(v - (u64)g_base)); }
    b[n] = 0; logf("R1 FAULT stack (game rvas):%s", b); logflush();
    return EXCEPTION_CONTINUE_SEARCH;
}
// Per-pass census + bisection (BUG-15b). Pass_Job_9B55B0(ctx) executes a pass's draws: pass = ctx+0x2A50,
// view = ctx+0x2A60, draw range = view+0x13C4 + pass*8 (begin, end), filled by that view's visibility jobs.
// F3: the second run skips passes [skipFrom, 69): their inline callbacks are swapped for the engine's own no-op
// 0x3FCBD0 for the duration of the second RV_Post (null stays null) and V2's pass jobs return early. The final
// image then comes from run 1 for skipFrom <= 59 (run 2 executes after run 1 on the GPU and writes no output).
static std::atomic<int> skipFrom{0}; static const int skipSteps[] = {0, 14, 30, 45, 59, 65};
static std::atomic<int> frameSkip{0};   // (kept for logging) the F3 suffix in force this frame
// 0.7.8: arbitrary run-2 pass sets (bit i = pass i; passes < 14 are never masked: pass 13 builds the view constants).
// Sources OR-ed per frame: F3 suffix, the F2 harness state, and the harness result (fix).
static std::atomic<u64> autoM0{0}, autoM1{0}, fixM0{0}, fixM1{0}, frameM0{0}, frameM1{0};
static inline bool inMask(u64 m0, u64 m1, int i) { return i < 64 ? ((m0 >> i) & 1) : ((m1 >> (i - 64)) & 1); }
static inline void setBit(u64 &m0, u64 &m1, int i) { if (i < 64) m0 |= 1ull << i; else m1 |= 1ull << (i - 64); }
// 0.7.6: hook the pass BODY 0x9B55D0(ctx, splitFn): reached from the job thunk 0x9B55B0 AND from RV_Post's inline
// route (debug flag 0x141803B60 = 1 in the shipping exe + per-view pass bitset view+0x15F0 built by RV_Submit: the
// pass runs immediately on the game thread and is marked done). 0.7.5 hooked only the thunk and missed that route.
static std::atomic<u32> pjCnt[2][NPASS], pjJobs[2][NPASS], pjInl[2][NPASS]; static u32 pjLast[2][NPASS], pjLastJobs[2][NPASS], pjLastInl[2][NPASS]; static std::atomic<u64> pjSkipped{0};
typedef void (*FnBody)(void *, void *); static FnBody oPassBody;
static void hPassBody(void *ctxp, void *split) {
    u8 *ctx = (u8 *)ctxp; u8 *view = *(u8 **)(ctx + 0x2A60); int pass = *(int *)(ctx + 0x2A50);
    int idx = view == mainView() ? 0 : (V2 && view == V2) ? 1 : -1;
    if (idx >= 0 && pass >= 0 && pass < NPASS) {
        bool inl = (u8 *)__builtin_return_address(0) == g_base + RVA_INLINE_RET;
        int b = *(int *)(view + 0x13C4 + pass * 8), e = *(int *)(view + 0x13C8 + pass * 8);
        pjCnt[idx][pass] += (u32)(e > b ? e - b : 0); if (inl) pjInl[idx][pass]++; else pjJobs[idx][pass]++;
        if (idx == 1 && inMask(frameM0.load(), frameM1.load(), pass)) { pjSkipped++; return; }
    }
    oPassBody(ctxp, split);
}
static void pjReset() { for (int k = 0; k < 2; k++) for (int i = 0; i < NPASS; i++) { pjCnt[k][i] = 0; pjJobs[k][i] = 0; pjInl[k][i] = 0; } }
static void pjSnap() { for (int k = 0; k < 2; k++) for (int i = 0; i < NPASS; i++) { pjLast[k][i] = pjCnt[k][i]; pjLastJobs[k][i] = pjJobs[k][i]; pjLastInl[k][i] = pjInl[k][i]; } }
static void pjLog() {
    char b[1200]; int n = 0;
    logf("R1 pass census (last doubled frame; pass:draws/jobs+inline, main | V2), skipFrom=%d:", skipFrom.load());
    for (int i = 0; i < NPASS; i++) {
        if (!pjLast[0][i] && !pjLast[1][i] && !pjLastJobs[0][i] && !pjLastJobs[1][i] && !pjLastInl[0][i] && !pjLastInl[1][i]) continue;
        n += snprintf(b + n, sizeof b - n, " %d:%u/%u+%u|%u/%u+%u", i, pjLast[0][i], pjLastJobs[0][i], pjLastInl[0][i], pjLast[1][i], pjLastJobs[1][i], pjLastInl[1][i]);
        if (n > 1000) { logf("  %s", b); n = 0; }
    }
    if (n) logf("  %s", b);
}
// BUG-16 (0.7.5): the skip value was read separately for patch and restore; an F3 press in between restored a
// never-filled buffer (zeros) into the engine's pass table for every later render -> black scene, black menus.
// 0.7.6: pristine copy taken at install (verified against known entries), one skip value per frame, restore
// from the pristine copy only, and a self-check before every frame's first render.
static u64 pristine[NPASS][4]; static bool tabOk = false, tabRW = false; static std::atomic<u64> tabRepairs{0};
static u64 tabBase = 0;   // image base the table's pointers are relative to (= g_base in the game; unrelocated in the self-test)
static u64 nopPtr() { return tabBase + RVA_FN_NOP; }
static bool capturePassTable() {
    u64 *t = (u64 *)(g_base + RVA_PASSTAB);
    if (!readable(t, NPASS * 32)) return false;
    memcpy(pristine, t, sizeof pristine);
    tabBase = pristine[59][0] - RVA_SLPASS;
#ifdef GOTVR_SELFTEST
    bool baseOk = tabBase == (u64)g_base || tabBase == 0x140000000ull;
#else
    bool baseOk = tabBase == (u64)g_base;
#endif
    tabOk = baseOk && pristine[13][3] == tabBase + 0x609760 && pristine[0][3] == tabBase + RVA_FN_NOP && pristine[0][0] == tabBase + 0x60EDC0;
    logf("R1 pass table: %s (pass59.pre=%llx pass13.main=%llx)", tabOk ? "captured, verified" : "UNEXPECTED -> F3 disabled",
         (unsigned long long)(pristine[59][0] - tabBase), (unsigned long long)(pristine[13][3] - tabBase));
    return tabOk;
}
static void checkPassTable() {
    if (!tabOk || !tabRW) return;
    u64 *t = (u64 *)(g_base + RVA_PASSTAB); bool bad = false;
    for (int i = 0; i < NPASS; i++) if (t[i * 4] != pristine[i][0] || t[i * 4 + 3] != pristine[i][3]) { t[i * 4] = pristine[i][0]; t[i * 4 + 3] = pristine[i][3]; bad = true; }
    if (bad) { tabRepairs++; logf("R1 pass table was modified outside the second run -> restored from the pristine copy"); }
}
static void maskSecondRun(u64 m0, u64 m1, bool on) {
    if ((!m0 && !m1) || !tabOk) return;
    u64 *t = (u64 *)(g_base + RVA_PASSTAB);
    if (!tabRW) { DWORD old; if (!VirtualProtect(t, NPASS * 32, PAGE_READWRITE, &old)) return; tabRW = true; }
    for (int i = 14; i < NPASS; i++) {
        if (!inMask(m0, m1, i)) continue;
        u64 *e = t + i * 4;
        if (on) { e[0] = pristine[i][0] ? nopPtr() : 0; e[3] = pristine[i][3] ? nopPtr() : 0; }
        else { e[0] = pristine[i][0]; e[3] = pristine[i][3]; }
    }
}

// BUG-17 (0.7.7): main-only renderables. Frame_59FA60 walks the frame's renderable lists (frame+0x1EA18/+0x1EA30)
// and calls obj->Render(ctx) with a render context RCtx_Build_9B2DD0(ctx, mainView): ctx[0] = view, +0x18 = the
// view's batch-list lock (view+0x850), +0x20 = its batch list (view+0x840), +0x98/+0xA0/+0xA8 = 0x80-byte draw
// records / pass index / count. RCtx_FlushDraws_9B49E0 copies the records into the frame's global pools bucketed
// per pass and links the batch into ctx+0x20's view (also called mid-way when a context fills up). Only the main
// view gets those draws -> V2 had no draws on passes 0, 17 (30), 18 (8), 29, 51, 52, 57, 61 -> run 2 (the visible
// image) missed them -> darker. Fix: a second flush of a COPY of the context aimed at V2's list (records are copied
// by value; lights (+0x1C0, registered globally in RCtx_Finish) and FlushB items (global 0x141EBC740) stay single).
// Only for builders whose contexts V2 does not get itself (Frame_59FA60's two; the per-worker visibility jobs
// 0xB01776 already build V2's own). Builder per context = return address of RCtx_Build, kept per thread.
// F4 toggles the duplication (A/B). A census per builder (records into main / V2 / duplicated) is logged on F10.
static std::atomic<int> dupOn{1};
enum { NB = 8 };
static const u32 kBuilders[NB - 1] = {0x59FB44, 0x59FD48, 0x62EB00, 0x651C34, 0x65265C, 0x7840BC, 0xB0177B};
static bool kDup[NB] = {true, true, false, false, false, false, false, false};
static std::atomic<u32> bMain[NB], bV2[NB], bDup[NB]; static u32 bLast[3][NB];
struct CtxTag { void *ctx; int b; };
static thread_local CtxTag tTags[8]; static thread_local int tNext = 0;
typedef void (*FnBuild)(void *, void *); static FnBuild oRCtxBuild;
static int builderIdx(u32 ret) { for (int i = 0; i < NB - 1; i++) if (kBuilders[i] == ret) return i; return NB - 1; }
static void hRCtxBuild(void *ctx, void *view) {
    oRCtxBuild(ctx, view);
    int b = builderIdx((u32)((u8 *)__builtin_return_address(0) - g_base));
    for (auto &t : tTags) if (t.ctx == ctx) { t.b = b; return; }
    tTags[tNext] = {ctx, b}; tNext = (tNext + 1) & 7;
}
static int tagOf(void *ctx) { for (auto &t : tTags) if (t.ctx == ctx) return t.b; return NB - 1; }
static FnV1 oRCtxFlush;
static void hRCtxFlush(void *ctxp) {
    u8 *ctx = (u8 *)ctxp; u8 *view = *(u8 **)ctx; int n = *(int *)(ctx + 0xA8);
    if (n > 0 && V2 && thisFrame()) {
        int b = tagOf(ctxp);
        if (view == mainView()) {
            bMain[b] += (u32)n;
            if (kDup[b] && dupOn.load() && *(u8 **)(ctx + 0x20) == mainView() + 0x840 && *(u8 **)(ctx + 0x18) == mainView() + 0x850) {
                alignas(16) u8 c2[0x1D0]; memcpy(c2, ctx, sizeof c2);
                *(u8 **)c2 = V2; *(u8 **)(c2 + 0x18) = V2 + 0x850; *(u8 **)(c2 + 0x20) = V2 + 0x840;
                oRCtxFlush(c2);                               // V2's copy first: the real flush zeroes the count
                bDup[b] += (u32)n;
            }
        } else if (view == V2) bV2[b] += (u32)n;
    }
    oRCtxFlush(ctxp);
}
static void bSnapReset() { for (int i = 0; i < NB; i++) { bLast[0][i] = bMain[i].exchange(0); bLast[1][i] = bV2[i].exchange(0); bLast[2][i] = bDup[i].exchange(0); } }
static void bLog() {
    char b[600]; int n = 0;
    for (int i = 0; i < NB; i++) n += snprintf(b + n, sizeof b - n, " %x:%u/%u/%u", i < NB - 1 ? kBuilders[i] : 0, bLast[0][i], bLast[1][i], bLast[2][i]);
    logf("R1 renderable census (last frame; builder ret-rva: records into main / V2 / duplicated into V2), dup=%d:%s", dupOn.load(), b);
}
} // namespace r1

static void hRVSubmit(void *view, u64 mode) {
    u32 f = frameCtr(); if (f != g_subFrame) { g_subFrame = f; g_subN = 0; }
    if (g_subN < 32) g_sub[g_subN++] = {(u64)view, (u32)mode};
    oRVSubmit(view, mode);
    if ((u8 *)view == r1::mainView() && r1::thisFrame()) { oRVSubmit(r1::V2, mode); if (g_subN < 32) g_sub[g_subN++] = {(u64)r1::V2, (u32)mode}; }
}
// View-list builder (FrameView_5A0540): append the second view after the engine built the list, so the engine's own
// per-view loops (gather jobs via Rnd_AddView, RV_Kick, end-of-frame reset) process it like any list view.
static void hFvList(void *frame) {
    r1::oFvList(frame);
    u8 *f = (u8 *)frame; r1::active = false;
    if (!r1::on.load() || f != r1::frameObj() || f[0x80F09] < 2) return;   // gameplay frames only
    if (!r1::ensureV2()) return;
    *(u64 *)r1::V2 = *(u64 *)(f + 0x1EA08);                 // RV_BindCamera: same camera as the main view (R1-A)
    ((FnV1)(g_base + RVA_RV_PREP))(r1::V2);                 // RV_9B6FB0: per-view constants snapshot
    int n = *(int *)(f + 0x1EA98), cap = *(int *)(f + 0x1EAA4); u64 list = *(u64 *)(f + 0x1EA90);
    if (n < 1 || n >= cap || !list || !readable((void *)list, 8 * (size_t)(n + 1))) { r1::appendFail++; return; }
    ((u64 *)list)[n] = (u64)r1::V2; *(int *)(f + 0x1EA98) = n + 1;
    r1::active = true; r1::activeFrame = frameCtr();
}
// Range-limited graph submit for the second run (BUG-14). Jobs_SubmitGraph_4D7EB0 walks the WHOLE pass-context
// pool from index 0: contexts with work (+0x2A6C) and not done (+0x2A6D) go into a new job group (job fn 0x4DADF0),
// done ones are retired (completed += 1). Called a second time in a frame it re-queues/re-retires the first run's
// contexts, the completed count overshoots the allocated count, and Sched_EndFrameWait (completed == allocated)
// waits forever. During the second run only contexts allocated after the first run are processed — same steps,
// same engine functions, same group layout (header 0x28, entries 0x18: +8 job fn, +0x10 context).
typedef void (*FnSG)(u8 *, void *); static FnSG oSubmitGraph;
static std::atomic<u32> r1Mark{0}; static std::atomic<u64> sgRange{0}, sgQueued{0}, sgRetired{0};
static void hSubmitGraph(u8 *sched, void *waitlist) {
    u32 mark = r1Mark.load();
    if (!r1::inSecond.load() || !mark || sched != g_base + RVA_SCHED || GetCurrentThreadId() != r1::gameTid) { oSubmitGraph(sched, waitlist); return; }
    int count = *(int *)(sched + 0x2B5C60);
    if (count <= (int)mark) return;
    typedef u8 *(*FnAlloc)(void *, u32, u32); typedef void (*Fn2)(void *, void *);
    u8 *g = ((FnAlloc)(g_base + RVA_FN_ALLOC))(g_base + RVA_FRAMEALLOC, (u32)(count - (int)mark) * 0x18 + 0x28, 0x10);
    *(u64 *)g = 0; *(u32 *)(g + 8) = 0; *(u32 *)(g + 0xC) = 1; *(u32 *)(g + 0x10) = 0xFFFFFFFFu; *(u64 *)(g + 0x18) = 0; *(u64 *)(g + 0x20) = 0;
    bool any = false;
    for (int i = (int)mark; i < count; i++) {
        u8 *ctx = sched + 0x9290 + (u64)i * 0x2B20;
        if (!ctx[0x2A6C]) continue;
        if (ctx[0x2A6D]) { ((Fn2)(g_base + RVA_FN_RETIRE))(sched, ctx); sgRetired++; continue; }
        int k = *(int *)(g + 4); u8 *e = g + 0x28 + (u64)k * 0x18;
        *(u32 *)e = 0; *(u32 *)(e + 4) = 0; *(u64 *)(e + 8) = (u64)(g_base + RVA_FN_CTXJOB); *(u64 *)(e + 0x10) = (u64)ctx;
        *(int *)(g + 4) = k + 1; any = true; sgQueued++;
    }
    if (any) { ((Fn2)(g_base + RVA_FN_QUEUEGROUP))(sched, g); ((Fn2)(g_base + RVA_FN_LISTPUSH))(waitlist, g); }
    sgRange++;
}

// R1-B capture seam census (logging only): pass 59's pre-callback StreamlinePass_cb records the upscaler evaluate
// inline on the game thread into the pass context's native command list (**ctx) and writes the global output slot
// 0x141EBA130. Log what that output is (size/format: before or after tone mapping?) and the list, per run.
static const char *vtMod(void *obj, char *buf, size_t n);
static FnV1 oSLPass; static std::atomic<u64> slCalls[2]; static std::atomic<int> slDump{0};
static ID3D12Resource *slotRes(u32 slotRva) {
    u8 *o = *(u8 **)(g_base + slotRva); if (!o || !readable(o + 0x18, 0x20)) return nullptr;
    u8 *rec = (u8 *)(((u64) * (u32 *)(o + 0x18 + 0x14) << 32) | *(u32 *)(o + 0x18 + 0x10));
    if (!rec || !readable(rec, 0x40)) return nullptr;
    typedef u8 *(*FnRec)(u8 *); u8 *r = ((FnRec)(g_base + RVA_FN_RESREC))(rec);
    if (!r || !readable(r + 8, 8)) return nullptr; ID3D12Resource *res = *(ID3D12Resource **)(r + 8);
    return res && readable(res, 8) && readable(*(void **)res, 8) ? res : nullptr;
}
static void descRes(const char *tag, ID3D12Resource *r, char *b, size_t n) {
    char m[64]; if (!r) { snprintf(b, n, "%s=none", tag); return; }
    D3D12_RESOURCE_DESC d = r->GetDesc();
    snprintf(b, n, "%s=%p(%s) %llux%u fmt=%d dim=%d flags=%x", tag, (void *)r, vtMod(r, m, 64), (unsigned long long)d.Width, d.Height, (int)d.Format, (int)d.Dimension, (unsigned)d.Flags);
}
static void hSLPass(void *ctx) {
    dlss::prePass((u8 *)ctx);
    oSLPass(ctx);
    dlss::postPass((u8 *)ctx);
    int run = r1::inSecond.load() ? 1 : 0; slCalls[run]++;
    if (slDump.load() > 0) {
        slDump--;
        char a[200], b[200], m[64]; u64 w = readable(ctx, 8) ? *(u64 *)ctx : 0; u64 cl = w && readable((void *)w, 8) ? *(u64 *)w : 0;
        descRes("out(0x1EBA130)", slotRes(RVA_SLOT_UPS_OUT), a, sizeof a); descRes("in(0x1EBA148)", slotRes(RVA_SLOT_UPS_IN), b, sizeof b);
        logf("R1-B census pass59 run=%d frame=%u ctx=%p cmdlist=%p(%s) | %s | %s", run + 1, frameCtr(), ctx, (void *)cl, cl && readable((void *)cl, 8) ? vtMod((void *)cl, m, 64) : "-", a, b);
    }
}

// ---------------- native stereo (xr=3, 0.8.0; playbook ch17 "render the world twice, everything else once") ----------------
// First render = LEFT eye, second render = RIGHT eye, both from the same simulation state. Before the second
// render: (1) drain: wait until every pass context of the first render has completed (its worker jobs read the
// camera while recording; none may see the right eye); (2) compose CAM-A for the right eye through the slot-43
// hook (pose2/fov2; F1 test: +0.5 m to the right) and refresh V2's camera snapshot (RV_Snapshot_9B6AE0); (3) arm
// the left-eye capture. The camera stays on the right eye until the end-of-frame wait (all second-render jobs
// recorded), then the hook recomposes the left eye. Capture: the left image is copied INSIDE the second render's
// first pass context (pass 0, replaced in the pass table for that run only by capPre): engine transition of the
// back buffer to COPY_SOURCE (0x4FDD50: per-list tracking, first use is fixed up by the engine at submit), engine
// barrier flush (0x4FE780), then CopyTextureRegion into the shared left-eye texture -> in GPU order after
// everything the first render drew. The right image is the back buffer at Present (existing copy).
namespace pub { static void captureLeft(void *ctx); static void noteRun2(u32 frame); }
namespace meter { static void captureL(void *ctx); static std::atomic<int> pairWanted{0}; }
namespace r1 {
static std::atomic<int> capArm{0}; static std::atomic<u64> drainTimeouts{0}, drains{0}, eyeSwitches{0};
static void capPre(void *ctx) {
    if (capArm.exchange(0)) pub::captureLeft(ctx);
    meter::captureL(ctx);
    ((FnV1)pristine[0][0])(ctx);
}
static void drainFirst(u32 target) {
    u8 *sch = g_base + RVA_SCHED; u64 t0 = GetTickCount64();
    while (*(volatile int *)(sch + 0x2B5C64) < (int)target) { if (GetTickCount64() - t0 > 40) { drainTimeouts++; return; } SwitchToThread(); }
    drains++;
}
} // namespace r1

// ---------------- 0.16.0 render size for the headset (design doc: square aspect + resolution scaler) ----------------
// RE (BUG-62, ADDR-128..131): in windowed mode the game renders at its WINDOW CLIENT size, clamped to the "Forced aspect
// ratio" setting (16:9 here -> 2560x1440 inside the 2560x1600 window). SC_ComputeSizes 0x46C840(swapchain object, settings)
// derives every size record from settings +0x10/+0x14 (windowed width/height) and +0x28 (forced aspect): client/back
// buffer (SC+0x1C0), viewport (+0x1E8, aspect +0x208), output (+0x160) and render size (+0x190); when one changes the
// engine releases and recreates all its render targets (RTPool_OnResize 0x4DF360) and resizes the swapchain. The hook
// hands it W x H (square) and aspect "auto" for the duration of the call only (the settings themselves are restored, so
// nothing is saved), so the game itself builds a consistent W x H frame - back buffer included - while the desktop
// window keeps its own size (DXGI scales the mirror into it). The headset image is that back buffer. F2 cycles the size;
// a change is applied through the engine's own path: PostSCCmd(NxApp, 2) on the game thread (as the settings menu does).
namespace rsz {
static std::atomic<int> want{0};       // square size in pixels; 0 = the game's own size
static std::atomic<u64> calls{0}; static std::atomic<int> lastW{0}, lastH{0};
// BUG-64 (0.16.1: "HUD, menus and the pause screen turned into squares, stretched in height"): the game lays out its UI on a
// canvas built from the viewport aspect SC+0x208 (SetRenderDims 0x4DBB60: 3840 x 3840 at aspect 1) and its flat-frame camera
// takes the same aspect (RTPool_OnResize 0x4DF3D1 -> Cam_SetAspect 0x806F00: tanH = aspect * tanV). The image stays square
// (the headset eye), but the game is told 16:9 -> UI canvas 3840x2160, flat frames rendered with a 16:9 projection: every
// image is anamorphic (stretched vertically by 16:9) and the host shows the UI and theatre frames at 16:9 (GvShared v10
// dispAspect). Stereo frames are unaffected: the eye projection is written whole (P00/P11 from the eye tangents).
static std::atomic<float> ana{0.0f};    // display aspect of the images (0 = their own pixel aspect)
typedef u64 (*FnCS)(u8 *, u8 *); static FnCS oCS;
static u8 *scObj = nullptr;   // the swapchain object (SC+0x1A8 = render aspect: Movie_Draw letterboxes from it)
static u64 hCS(u8 *sc, u8 *st) {
    calls++; scObj = sc;
    {   // fail closed: a resize storm (the engine re-applying because the window and the swapchain sizes disagree) -> off
        static u64 t0 = 0; static int nIn = 0; const u64 now = GetTickCount64();
        if (now - t0 > 2000) { t0 = now; nIn = 0; }
        if (++nIn > 30 && want.load()) { want = 0; logf("render size: the game recomputed its sizes %d times in 2 s -> headset render size OFF (the game's own size); please send this log", nIn); }
    }
    const int n = want.load(); const bool windowed = st && st[0] == 0;
    u64 r;
    if (n >= 256 && windowed) {
        const u32 w0 = *(u32 *)(st + 0x10), h0 = *(u32 *)(st + 0x14); const int a0 = *(int *)(st + 0x28);
        *(u32 *)(st + 0x10) = (u32)n; *(u32 *)(st + 0x14) = (u32)n; *(int *)(st + 0x28) = 0;
        r = oCS(sc, st);
        *(u32 *)(st + 0x10) = w0; *(u32 *)(st + 0x14) = h0; *(int *)(st + 0x28) = a0;
        const float A = g_uiAspect;
        if (A > 0.5f) { const float a1 = *(float *)(sc + 0x208); *(float *)(sc + 0x208) = A; ana = A;
            if (fabsf(a1 - A) > 1e-3f) logf("render size: the game lays out its UI and flat frames for aspect %.3f (the image is %dx%d; shown at %.3f in the headset)", A, n, n, A); }
        else ana = 0.0f;
    } else {
        r = oCS(sc, st); ana = 0.0f;
        if (n >= 256 && !windowed) { static int once = 0; if (!once++) logf("render size: the game is in fullscreen mode - the headset render size works in WINDOWED mode only (set Display mode = Windowed)"); }
    }
    const int ow = *(int *)(sc + 0x160), oh = *(int *)(sc + 0x164);
    if (ow != lastW.load() || oh != lastH.load()) {
        lastW = ow; lastH = oh;
        logf("render size: game output %dx%d (render %dx%d, back buffer %dx%d, aspect %.3f)%s", ow, oh, *(int *)(sc + 0x190), *(int *)(sc + 0x194), *(int *)(sc + 0x1C0), *(int *)(sc + 0x1C4), *(float *)(sc + 0x208),
             n >= 256 && windowed ? " <- headset size from the mod" : " (the game's own size)");
    }
    return r;
}
// BUG-63 (0.16.0 froze the game on F2, fullscreen): PostSCCmd from the GameTick hook deadlocked. BUG-64c (0.16.1/0.16.2 crashed
// right after F2, windowed; dump GhostOfTsushima.exe.66416): 0.16.1 applied from the WINDOW thread, so the main thread kept
// ticking while the swapchain thread rebuilt the back buffers: the next tick's Comp_Draw 0x4FE620 -> NxD3D vt+0x138 0x43CC60 ->
// 0x437AF0 read back-buffer entry 0 of the D3D12 driver (+0x168 -> +0x30, entries of 0x40) while it was empty -> AV at
// exe+0x437B11 (null+0x10) on the tick thread. The game itself applies display changes ON THE MAIN THREAD: the window thread
// only queues messages (NxWnd_Queue 0x444F80); GameWindow_Pump 0x446530 (GameWindow vt+0x48, called first thing in NxApp::Update
// 0x4257B0 at the start of each tick) hands them to 0x445970, which calls NxApp vt+0x120 -> ApplyDisplay 0x421ED0 -> PostSCCmd(2):
// the main thread is blocked for the whole swapchain rebuild. F2 now does exactly that: the request is applied right after the
// game's own message pump, through ApplyDisplay (with its flags and window/Reflex follow-ups), with the current settings.
static std::atomic<int> pending{0}; static std::atomic<u64> applies{0}, pumpTid{0};
typedef u8 *(*FnApp)(); typedef char (*FnApply)(u8 *, u8 *);
static u8 *appNow() { u8 *svc = *(u8 **)(g_base + RVA_SERVICES_ROOT); return svc ? *(u8 **)svc : nullptr; }
static FnApp getApp = appNow; static FnApply apply = nullptr;   // apply = the game's ApplyDisplay (self-test: a stand-in)
static u8 *settingsOf(u8 *app) { typedef u8 *(*FnGet)(u8 *); return app ? ((FnGet)(*(void ***)app)[0x118 / 8])(app) : nullptr; }
static bool windowedNow() { u8 *st = settingsOf(getApp()); return st && st[0] == 0; }
static void applyNow() {   // main thread, between the game's message pump and the rest of its tick
    u8 *app = getApp(); u8 *st = settingsOf(app);
    if (!app || !st || !apply) { logf("render size: the game's display path is not available -> not applied"); return; }
    if (st[0] != 0) { logf("render size: the game is in FULLSCREEN mode -> not applied (set Display mode = Windowed)"); return; }
    alignas(16) static u8 copy[0x200]; memcpy(copy, st, 0x148);   // the current display settings (0x148 bytes), unchanged
    const u64 t0 = GetTickCount64(); applying = 1; const char r = apply(app, copy); applying = 0; applies++;
    logf("render size: applied on the game's main thread through its own display path (%llu ms, result %d)", (unsigned long long)(GetTickCount64() - t0), (int)r);
}
typedef char (*FnPump)(u8 *, float); static FnPump oPump;
static char hPump(u8 *w, float dt) {
    const char r = oPump(w, dt);
    if (!pumpTid.load()) pumpTid = GetCurrentThreadId();
    if (pending.exchange(0)) applyNow();
    mblur::tick(); shade::tick();
    return r;
}
static void request() {   // hotkey thread: never blocks; the main thread applies it at its next message pump
    if (!windowedNow()) { logf("render size: the game is in FULLSCREEN mode - set Display mode = Windowed in the game's settings for the headset render size (F2 does nothing in fullscreen)"); return; }
    if (!oPump) { logf("render size: the game's message pump is not hooked -> F2 unavailable"); return; }
    if (pending.exchange(1)) { logf("render size: a change is still waiting to be applied"); return; }
}
}
// ---------------- 0.18.6 motion blur off in VR (BUG-74, playbook ch14 #velocity-not-temporal) ----------------
// 0.18.5's Shift+F9 test: the artifact ("terrain and trees streak across the screen, extremely distorted, only on very fast head
// turns") follows the eye that is drawn right after the simulation step. That eye's PREVIOUS frame is the other eye of the
// previous pair, so the engine's velocity buffer holds a whole pair of head motion (+ the eye separation) and motion blur smears
// it; the other eye's previous frame is the same pose sample (velocity = eye separation only). The playbook: under alternate-eye
// rendering "disabling motion blur is a requirement". The game's PC settings block (vt+0x118 of the app, the one F2 uses;
// reflection table 0x1404600F0) holds MotionBlurScale as a float at +0x64 (FilmGrainStrength +0x68, DOFQuality +0x5C, Vignette
// +0x71). While xr=4 runs it is held at 0 on the main thread (message pump); the user's value is put back at exit.
// ---------------- 0.18.10 near stipple fade off in first person (owner: "Jin is still see-through") ----------------
// Not the near clip plane (0.18.9 set it to 4 cm, the projection confirmed it, still see-through). The game's ENVIRONMENT parameters
// (353 per set, 32-byte entries: value, +0x10 index, +0x14 type, +0x1C set flag; ctor 0x619E10, entry init 0xAFF100, defaults table
// 0x14180A980 + i*0x18) include NearStippleDist 200 cm / NearStippleDistClose 100 cm / DisableNearStippleFade (index 106, bool):
// the third-person dither that fades Jin out when the camera comes within 1-2 m. While first person is on, every ACTIVE set (the
// blend list 0x1433938A8, count 0x1433939A8, Env_SetA 0x61A9C0) and the two global sets get DisableNearStippleFade = 1 (each entry is
// checked: index 106, type 3); first person off -> the game's values come back.
// 0.18.12 (owner: "the arms are still transparent-ish close to the camera, like whatever is behind gets written on top of them"): the
// same environment sets carry NEAR_CLIP (index 5, 10 cm; the name table 0x141809260 + i*8): the camera takes its near plane from it
// every tick. 0.18.11 overrode only the projection and +0x664 in the render-camera update, so whatever the engine builds from the
// environment's near before that update (the reprojection that TAA uses to fetch last frame's colour, depth linearisation) still used
// 10 cm while the depth buffer was drawn with 2 cm: a pixel 20 cm away was reprojected as if it were 1 m away, so between the eyes
// (6.4 cm apart) TAA fetched history from the wrong place - the background behind the arm, blended on top of it. Now the environment
// itself says fp_near_cm while first person is on, so every consumer agrees; the projection override stays as the backstop.
namespace stipple {
static const u32 RVA_LIST = 0x33938A8, RVA_COUNT = 0x33939A8, RVA_G1 = 0x347E4D0, RVA_G2 = 0x347B870; enum { IDX = 106, IDX_NEAR = 5, IDX_FLOATREF = 104, IDX_RADH = 312, IDX_RADV = 313 };
struct Mod { u8 *set; int idx; u32 orig, origFlag; }; static Mod mods[128]; static int nMods = 0; static std::atomic<u64> nSets{0}, nBad{0}, nNear{0};
static bool entryOk(u8 *set, int idx) {
    u8 *e = set + idx * 0x20; if (!readable(e, 0x20) || *(int *)(e + 0x10) != idx) return false;
    if (idx == IDX) return *(int *)(e + 0x14) == 3;   // bool
    u8 *r = set + IDX_FLOATREF * 0x20;   // a float entry: the same type as NEAR_STIPPLE_DIST (a known float) in this set
    return readable(r, 0x20) && *(int *)(r + 0x10) == IDX_FLOATREF && *(int *)(r + 0x14) == *(int *)(e + 0x14) && *(int *)(e + 0x14) != 3;
}
static void put(u8 *set, int idx, u32 v) {
    if (!set || !entryOk(set, idx)) { if (set) nBad++; return; }
    u8 *e = set + idx * 0x20; int k = 0; for (; k < nMods; k++) if (mods[k].set == set && mods[k].idx == idx) break;
    if (k == nMods) { if (nMods >= 128) return; mods[nMods++] = {set, idx, *(u32 *)e, e[0x1C]}; }
    *(u32 *)e = v; e[0x1C] = 1; nSets++; if (idx == IDX_NEAR) nNear++;
}
static void on(u8 *set) {
    put(set, IDX, 1u);
    // 0.18.12 (NEAR_CLIP index 5 = fp_near_cm) and 0.18.13 (STIPPLE_NEAR_RADIUS 312/313 = 0) were removed in 0.18.15: no effect on the
    // arms (owner) - the environment keeps the game's values; the projection's 2 cm near (0.18.11) stays.
}
static void tick(bool fpOn) {   // game thread, once per frame
    static u32 last = 0xFFFFFFFFu; const u32 f = frameCtr(); if (f == last) return; last = f;
    if (g_cfg.xr != 4) return;
    u8 **list = (u8 **)(g_base + RVA_LIST); const int n = *(int *)(g_base + RVA_COUNT);
    if (fpOn) {
        on(g_base + RVA_G1); on(g_base + RVA_G2);
        if (n > 0 && n <= 32) for (int i = 0; i < n; i++) on(list[i]);
        { static int told = 0; if (!told++) logf("first person: environment sets changed: the game's near stipple fade (Jin dithered away within 1-2 m of the camera) off (%d entries, %llu rejected)", nMods, (unsigned long long)nBad.load()); }
    } else if (nMods) {   // back to the game's values - only in sets that still exist (globals or still in the blend list)
        for (int k = 0; k < nMods; k++) { bool live = mods[k].set == g_base + RVA_G1 || mods[k].set == g_base + RVA_G2;
            if (!live && n > 0 && n <= 32) for (int i = 0; i < n; i++) if (list[i] == mods[k].set) live = true;
            if (live && entryOk(mods[k].set, mods[k].idx)) { u8 *e = mods[k].set + mods[k].idx * 0x20; *(u32 *)e = mods[k].orig; e[0x1C] = (u8)mods[k].origFlag; } }
        nMods = 0;
    }
}
}
namespace mblur {
static float orig = -1.0f; static u8 *st0 = nullptr; static std::atomic<float> seen{-1.0f}; static std::atomic<u64> forced{0};
static void tick(bool force) {   // main thread (message pump)
    static u64 next = 0; const u64 now = GetTickCount64(); if (now < next && !force) return; next = now + 500;
    if (g_cfg.xr != 4) return;
    u8 *st = rsz::settingsOf(rsz::getApp()); if (!st || !readable(st + 0x64, 4)) return;
    const float v = *(float *)(st + 0x64); seen = v;
    { static int told = 0; if (!told++) logf("motion blur: the game's setting (MotionBlurScale) is %.2f%s", v, g_mblurOff.load() ? (v > 0 ? " -> held at 0 while in VR (motion_blur_off = 1)" : " (already off)") : " (motion_blur_off = 0: left alone)"); }
    if (!g_mblurOff.load() || !(v > 0.0f && v <= 100.0f)) return;
    if (st != st0 || orig < 0) { orig = v; st0 = st; }
    *(float *)(st + 0x64) = 0.0f; forced++;
}
static void restore() { if (st0 && orig > 0 && readable(st0 + 0x64, 4) && *(float *)(st0 + 0x64) == 0.0f) *(float *)(st0 + 0x64) = orig; }
}
// 0.18.21-0.18.24 see-through arms (BUG-80). Owner, 0.18.22: "only see-through when ambient occlusion (SSAO Quality) is on, normal
// when off; screen-space shadows no effect". 0.18.23 held AO at 0 by writing the settings block: "when I loaded in, AO was off but my
// hands were still see-through; I turned screen-space shadows off - not solved; turned them back on - fixed". Reading of that:
//  1. a write into the settings block is NOT applied: the menu showed AO off (it reads the block) but the renderer kept AO on. Only the
//     game's apply (ApplyDisplay 0x421ED0: copies the new settings to app+0x7A8 and hands old + new to the render service, vt+0x60 of
//     0x409460's object, then saves them) changes the renderer - the owner's menu toggle ran it, with our AO 0 inside.
//  2. after that apply (AO off, SSS off) the arms were STILL see-through; with SSS back on they were fixed: screen-space shadows OFF
//     shows the artefact as well (its fallback), so the clean state is AO OFF + SSS ON.
// So in first person (VR) the mod now asks for AO 0 and SSS >= 1 THROUGH THE GAME'S OWN APPLY, like the menu, on the main thread right
// after the message pump (the F2 render-size path, BUG-63/64), once, in gameplay; the player's values come back the same way on F5 /
// Shift+F6. The game saves its settings on apply, so they stay that way if you quit in VR (fp_ao_off = 0: the mod leaves them alone).
namespace shade {
static int oA = -1, oS = -1; static bool forced = false; static u64 nextTry = 0; static std::atomic<int> seenS{-1}, seenA{-1}; static std::atomic<u64> writes{0}, applies{0};
static bool gameplayNow();   // after ui (below)
static bool sane(int v) { return v >= 0 && v <= 8; }
static bool applyWith(u8 *app, u8 *st, int ao, int sss, const char *why) {   // main thread: the game's own apply with AO / SSS changed
    if (!rsz::apply || !app || !st) return false;
    alignas(16) static u8 copy[0x200]; memcpy(copy, st, 0x148); *(int *)(copy + 0x60) = ao; *(int *)(copy + 0x98) = sss;
    const u64 t0 = GetTickCount64(); rsz::applying = 1; const char r = rsz::apply(app, copy); rsz::applying = 0; applies++; writes++;
    logf("ambient occlusion: %s -> applied through the game's own settings path (AO %d, screen-space shadows %d; %llu ms, result %d)", why, ao, sss, (unsigned long long)(GetTickCount64() - t0), (int)r);
    return true;
}
static void tick(bool force) {   // main thread (message pump)
    static u64 next = 0; const u64 now = GetTickCount64(); if (now < next && !force) return; next = now + 500;
    u8 *app = rsz::getApp(); u8 *st = rsz::settingsOf(app); if (!st || !readable(st + 0x60, 4) || !readable(st + 0x98, 4)) return;
    const int A = *(int *)(st + 0x60), Ss = *(int *)(st + 0x98); seenS = Ss; seenA = A;
    if (!sane(A) || !sane(Ss)) { static int told = 0; if (!told++) logf("ambient occlusion: the settings block holds AO %d / SSS %d - not settings we know -> left alone", A, Ss); return; }
    const bool on = g_cfg.xr == 4 && mode.load() && fp::on.load();
    if (on) {
        if (A == 0 && Ss != 0) return;   // already clean
        if (!force && (now < nextTry || !gameplayNow())) return;   // not while a menu / loading screen is up (the player may be in the settings)
        static int tries = 0; if (!forced) tries = 0;
        if (tries >= 3) { static int told = 0; if (!told++) logf("ambient occlusion: the game did not take AO off after 3 applies -> stopped (set SSAO Quality = Off yourself)"); return; }
        tries++; nextTry = now + 5000;
        if (!forced) { oA = A; oS = Ss; forced = true; }
        else { if (A != 0) oA = A; }   // the player changed AO in the menu meanwhile: remember it for later, keep it off now
        applyWith(app, st, 0, Ss != 0 ? Ss : 1, "first person in VR: AO off, screen-space shadows on (see-through arms fix)");
    } else if (forced) {
        if (!force && !gameplayNow() && g_cfg.xr == 4 && fp::on.load()) return;
        forced = false; applyWith(app, st, oA >= 0 ? oA : A, oS >= 0 ? oS : Ss, "first person off: your own AO / screen-space shadow settings back");
    }
}
static void restore() { /* settings are applied and saved by the game itself; nothing to undo at exit */ }
}

// ---------------- UI, HUD and videos (0.12.0, playbook ch04 HUD-002/003/004) ----------------
// RE (STEREO_ROUTE session 14): the UI passes 60/61/64 draw into their own render target 0x143377510, bound by
// RT_Bind 0x7E34E0 from pass 60's pre-callback 0x6E3790, pass 61/64's setup 0x6F0860 and Ctx_InheritState 0x7E5170
// (new pass contexts re-bind the inherited target); the engine composites it over the frame later. RT_Bind sets the
// D3D viewport (Ctx_SetViewport 0x50ECA0 - its only caller) and scissor (Ctx_SetScissor 0x50EC60, also reached per draw
// from Ctx_ScissorNorm 0x7E52C0) from the bound rect ctx+0x2948. Unhandled, the 16:10 UI covers the whole ~100 degree
// eye frustum: huge and stretched (BUG-40).
// Fix (HUD-004 without a second layer): while the UI target is bound, viewport and scissor are mapped into a per-eye
// rect = where a flat panel hud_width_deg wide (the UI's own aspect), hud_distance in front of the head and
// hud_down_deg below eye level projects in THIS eye's frustum, including the eye's half-IPD offset, so both eyes
// converge on it at hud_distance. The UI layout itself is untouched (ctx+0x2948 stays full size) - only where it lands.
// Videos (HUD-003): Movie_Render 0x7A1040 -> Movie_Draw 0x7EAF80 draws a pre-rendered video full-screen into the back
// buffer at the end of pass 59. Frames with a video, or without a 3D world (frame flag +0x80F09 == 0: RV_Post starts at
// pass 59), are flat contexts: the HUD mapping is off (subtitles stay on the video) and the frame is tagged
// GV_SLOT_THEATER, so the host shows it on a world-locked screen at the image's real aspect (quad layer).
namespace ui {
typedef void (*FnRT)(u8 *, u8 *, int *, int); static FnRT oRT;
typedef void (*FnSC)(u8 *, u32, u32, u32, u32); static FnSC oSC;
typedef void (*FnVP)(u8 *, u32, u32, u32, u32);
typedef void (*FnMovie)(void *, void *, void *, void *); static FnMovie oMovie;
static std::atomic<int> hud{1}; static std::atomic<float> hudDeg{55.0f};   // F9 cycles the size / off
static std::atomic<int> hudHidden{0};   // 0.18.0: the left-hand-at-head gesture shows / hides the gameplay HUD
static std::atomic<u64> hudBoostUntil{0};   // 0.18.2: time of the last HUD gesture (+6 s; log / self-test)
static std::atomic<int> placeReq{0};          // 0.18.5: the HUD gesture re-places the room-fixed panel in front of you
static std::atomic<int> theaterMode{0};                                      // F10: 0 auto, 1 always, 2 never
static int crop = 0;                                                         // theater_crop: show only the 16:9 band of a video
struct View { bool ok; int eye; float tl, tr, tu, td, ex; bool wl; float wx0, wx1, wyT, wyB; };   // wl: world-locked panel tangents for this eye
struct WPanel { bool on; float P[3]; float w; }; static WPanel wp{};
static std::atomic<float> uiAspect{9.0f / 16.0f};   // UI target height / width (from the bound rect)

static View cur{};   // written on the game thread at the main view's RV_Post, read by that frame's UI pass jobs
static std::atomic<u32> post{0}, lastMovie{0x80000000u}; static std::atomic<bool> worldOff{false};
static std::atomic<u64> binds{0}, mapped{0}, scissors{0}, movieDraws{0}, flatPosts{0}, skipFlat{0}, skipNoView{0}, posts{0};
struct Flag { u32 frame, flags; }; static Flag ring[32], uiRing[32];
static std::atomic<u8 *> live[32];   // command contexts that currently have the UI target bound (and mapped)
static int liveFind(u8 *c) { for (int i = 0; i < 32; i++) if (live[i].load(std::memory_order_relaxed) == c) return i; return -1; }
static void liveAdd(u8 *c) { if (liveFind(c) >= 0) return; for (int i = 0; i < 32; i++) { u8 *z = nullptr; if (live[i].compare_exchange_strong(z, c)) return; } }
static void liveDel(u8 *c) { int i = liveFind(c); if (i >= 0) live[i].store(nullptr); }
static bool movieNow() { return post.load() - lastMovie.load() < 8; }   // a video was drawn in the last 8 frames (hysteresis)
// Pause (0.13.0, RE session 15): the game clock 0x141489DF0 keeps 28 time-scale "reasons" at +0x84 (Clock_SetScale
// 0x56F340). Reason 7 is the refcount of open UI screens that pause the game (0x7D6E10: screen+0x7F8 "pauses" ->
// first one sets scale 7 to 0, 0x7D7240 last one closes -> 1): pause menu, map, gear, options... Reason 8 = video
// (Movie_Tick), 9 = world load/reset. pause_channels (ini, bit mask) selects which reasons count as a flat context.
static u32 pauseMask = 1u << 7;
static float scaleOf(int ch) { return *(volatile float *)(g_base + RVA_CLOCK + 0x84 + ch * 4); }
static bool pausedNow() { for (int ch = 0; ch < 28; ch++) if ((pauseMask >> ch) & 1) { if (scaleOf(ch) == 0.0f) return true; } return false; }
static std::atomic<bool> flatNow{false}, pausedTick{false}; static std::atomic<u8> worldMode{0};
static std::atomic<bool> loadingTick{false}, titleTick{false}; static std::atomic<int> cropOn{0};
static bool flatRaw() { int t = theaterMode.load(); if (t == 1) return true; if (t == 2) return false; return g_ui.theater && (worldOff.load() || movieNow() || pausedTick.load() || loadingTick.load() || titleTick.load()); }
static bool flat() { return flatNow.load(); }   // decided once per tick (uiDecide), so the render, the UI mapping and the published flags agree
// UI target probe (for the next step, the UI on its own sharp layer): what resource the UI passes draw into
static std::atomic<ID3D12Resource *> probeNat{nullptr}; static std::atomic<u32> probeFlags{0}, probeCount{0}; static std::atomic<int> probeLogged{0};
// pixel rect (x0, y0, x1, y1) of the HUD panel in a W x H target for view v; false = no mapping
static bool panel(const View &v, int W, int H, float deg, float out[4]) {
    if (!v.ok || W <= 0 || H <= 0 || !(deg > 1.0f)) return false;
    const float an = rsz::ana.load();   // BUG-64: an anamorphic UI image is shown at the game's layout aspect
    const float d = g_ui.hudDist, hw = d * tanf(deg * 0.5f / 57.2957795f), hh = an > 0.5f ? hw / an : hw * (float)H / (float)W, cy = -d * tanf(g_ui.hudDown / 57.2957795f);
    float x0 = (-hw - v.ex) / d, x1 = (hw - v.ex) / d, yT = (cy + hh) / d, yB = (cy - hh) / d;   // tangents seen from this eye
    if (v.wl) { x0 = v.wx0; x1 = v.wx1; yT = v.wyT; yB = v.wyB; }                               // world-locked (start screen)
    const float sw = v.tr - v.tl, sh = v.tu - v.td; if (!(sw > 0.01f && sh > 0.01f)) return false;
    out[0] = (x0 - v.tl) / sw * W; out[2] = (x1 - v.tl) / sw * W; out[1] = (v.tu - yT) / sh * H; out[3] = (v.tu - yB) / sh * H;
    return true;
}
static u32 clampu(float x, float lo, float hi) { return x <= lo ? (u32)lo : x >= hi ? (u32)hi : (u32)(x + 0.5f); }
// coordinates in the full bound rect r -> the panel p (relative to r); viewport: free upper bound, scissor: clipped to r
static void mapRect(const int r[4], const float p[4], u32 a0, u32 a1, u32 b0, u32 b1, u32 o[4], bool clip) {
    const float W = (float)(r[2] - r[0]), H = (float)(r[3] - r[1]), sx = (p[2] - p[0]) / W, sy = (p[3] - p[1]) / H;
    const float hx = clip ? (float)r[2] : 32767.0f, hy = clip ? (float)r[3] : 32767.0f, lx = clip ? (float)r[0] : 0.0f, ly = clip ? (float)r[1] : 0.0f;
    o[0] = clampu(r[0] + p[0] + ((float)a0 - r[0]) * sx, lx, hx); o[1] = clampu(r[1] + p[1] + ((float)a1 - r[1]) * sy, ly, hy);
    o[2] = clampu(r[0] + p[0] + ((float)b0 - r[0]) * sx, lx, hx); o[3] = clampu(r[1] + p[1] + ((float)b1 - r[1]) * sy, ly, hy);
}
// ---- 0.14.0: per-tick UI mode (decided in uiDecide) ----
// HUD: gameplay. TITLE: start screen (time-scale reason 27 == 0 while a 3D world is shown and nothing pauses).
// CINEMA: a cinematic (the display service's "Letterbox" mode is pushed): letterbox bars, fades and subtitles belong to
// the whole frame, so the UI is neither mapped nor layered. FLAT: theatre screen (video / pause / no world / F10).
enum UiMode { M_HUD = 0, M_TITLE = 1, M_CINEMA = 2, M_FLAT = 3 };
static std::atomic<int> mode{M_HUD};
static int titleReason = 27;
static bool letterboxNow() {   // what the UI layout itself asks (0x6E9544): display service vt+0x70("Letterbox")
    u8 *svc = *(u8 **)(g_base + RVA_SERVICES); if (!svc) return false;   // the same derefs the UI layout does every frame
    u8 *o = *(u8 **)(svc + 0xC8); if (!o) return false;
    void **vt = *(void ***)o; if (!vt) return false;
    typedef char (*Fn)(void *, const void *); return ((Fn)vt[0x70 / 8])(o, g_base + RVA_STR_LETTERBOX) != 0;
}
// ---- 0.15.0 (BUG-58): the UI on its own sharp layer by REDIRECTING the UI passes' colour target (playbook ch04 "Redirect a
// whole category of draws by rebinding the target once", HUD-004). The UI passes 60/61/64 draw into the engine's final
// picture 0x143377510 (BUG-51: the 3D scene is already in it), so the UI cannot be copied out. Instead every bind of that
// target by a UI pass is redirected into one of our shared UI images: the engine binds its targets lazily in Ctx_FlushRT
// 0x4FEA80 (one OMSetRenderTargets with a fresh RTV per flush - BUG-50: why 0.14.1's RTV-identity lock-in failed); right
// after the engine's flush, if colour attachment 0 is the engine's UI texture and the context belongs to a UI pass, the
// same list gets OMSetRenderTargets(our image's RTV, the ENGINE's depth/stencil view) - the UI's private stencil masks keep
// working, its filter/mask targets (other textures) stay native. Each frame (chosen on the game thread at RV_Post): one
// shared image, cleared to transparent in pass 60's pre-callback (first UI pass in GPU order). Pass 65 (the full-screen
// fade) is un-redirected: the fade goes into the 3D picture as before. The 3D picture keeps no UI (HUD-less), the shared
// image (simultaneous access: no barriers) goes to the host as a quad at the UI's full resolution, sampled once by the
// runtime -> sharp. Kill switch: hud_layer=0 / F1 -> the 0.14 panel mapping.
static std::atomic<int> hostUiOk{0}, layerWant{1};
static std::atomic<u8 *> probeRec{nullptr};                 // the UI texture's engine resource record
static std::atomic<ID3D12Resource *> sh[GV_UITEX];          // shared UI images (created by pub)
static std::atomic<SIZE_T> shRtv[GV_UITEX];                 // our RTVs on them
static std::atomic<int> shReady{0}; static std::atomic<u32> shNext{0};
struct Pend { u32 frame; int k; }; static Pend pend[32];     // frame -> image to publish (set at pass 65)
struct KOf { u32 frame; int k; }; static KOf kOf[32];       // frame -> image the UI of that frame is drawn into (set at RV_Post)
static std::atomic<u64> lBinds{0}, lCopies{0}, lSkip{0}, fadeFix{0}, rdBinds{0}, rdClears{0}, rdNoK{0}, rdOther{0}, rdHidden{0};
static std::atomic<u64> rightSkip{0};
static std::atomic<u32> armedFrame{0}, rdFrame{0}; static int rdFailRun = 0; static std::atomic<int> rdRefused{0};   // per-frame proof that the redirect works
typedef void (*FnTrans)(u8 *, u8 *, int); typedef void (*FnFlush)(u8 *);
static FnTrans fnTrans = nullptr; static FnFlush fnFlush = nullptr;   // engine functions (kept for the census; self-test stubs them)
typedef ID3D12GraphicsCommandList GL;
static GL *fastList(u8 *ctx) { u8 *w = ctx ? *(u8 **)ctx : nullptr; return w ? *(GL **)w : nullptr; }   // what Ctx_SetViewport derefs
static std::atomic<int> f1Off{0};   // F1: layer off (A/B)
static bool layerReady() { return layerWant.load() && !f1Off.load() && !rdRefused.load() && hostUiOk.load() && shReady.load() && shRtv[0].load() && probeRec.load() && probeNat.load(); }
// BUG-72 (0.18.1: "after the map / pause menu I was in third person, then it went back to first"): the game fades the HUD after a
// pausing menu closes; that fade (veil, no bars) started the cutscene latch, which only ends when the camera's fov changes or after
// 15 s - and the 1 s "HUD large" window after a menu also counted as cinematic. The UI may still go over the whole view then, but
// first person is only given up for a CAMERA cutscene (bars in the 3D picture, the engine's letterbox rect, or bars on the HUD
// image); a fade-only start ends 0.5 s after the fade.
static std::atomic<int> camCut{0};
static int fpBlockReason() { if (flat()) return 1; const int m = mode.load(); return m == M_HUD ? 0 : m == M_CINEMA ? (camCut.load() ? 2 : 0) : m == M_TITLE ? 3 : 4; }
static bool layerOn() { const int m = mode.load(); return g_cfg.xr == 4 && hud.load() && (m == M_HUD || m == M_TITLE || m == M_CINEMA) && layerReady(); }
static int kNow() { const u32 f = frameCtr(); const KOf &x = kOf[f & 31]; return x.frame == f ? x.k : -1; }
// contexts of UI passes with the UI target bound while the layer is on
static std::atomic<u8 *> arm[32]; static std::atomic<int> armN{0};
static int armFind(u8 *c) { if (!armN.load(std::memory_order_relaxed)) return -1; for (int i = 0; i < 32; i++) if (arm[i].load(std::memory_order_relaxed) == c) return i; return -1; }
static void armAdd(u8 *c) { if (armFind(c) >= 0) return; for (int i = 0; i < 32; i++) { u8 *z = nullptr; if (arm[i].compare_exchange_strong(z, c)) { armN++; return; } } }
static void armDel(u8 *c) { int i = armFind(c); if (i >= 0) { u8 *x = c; if (arm[i].compare_exchange_strong(x, nullptr)) armN--; } }
// the engine target flush: re-bind our image right after the engine bound its own (same list, engine depth view)
typedef void (*FnFlushRT)(u8 *); static FnFlushRT oFlushRT;
static void hFlushRT(u8 *ctx) {
    const bool dirty = (ctx[0x340] & 1) != 0;
    oFlushRT(ctx);
    if (!dirty || armFind(ctx) < 0) return;
    { const int pass = *(int *)(ctx + 0x2A50); if (pass < 60 || pass > 65) { rdOther++; return; } }   // UI passes only, whatever happened to the arm
    u8 *att = *(u8 **)(ctx + 0x2C8);
    if (!att || *(u8 **)(att + 0x18) != probeRec.load()) { rdOther++; return; }   // colour 0 must be the UI texture (stale extra slots are dropped: we bind 1)
    const int k = kNow(); const SIZE_T h = k >= 0 ? shRtv[k].load() : 0; GL *gl = fastList(ctx);
    if (!h || !gl) { rdNoK++; return; }
    D3D12_CPU_DESCRIPTOR_HANDLE rt{h}, ds{}; const bool dep = *(u64 *)(ctx + 0x308) != 0;
    if (dep) ds.ptr = (SIZE_T)(*(u32 *)(ctx + 0x33C) * *(u32 *)(g_base + RVA_DSV_INC)) + (SIZE_T) * (u64 *)(g_base + RVA_DSV_BASE);
    gl->OMSetRenderTargets(1, &rt, TRUE, dep ? &ds : nullptr); rdBinds++; rdFrame++;
}
// pass 60 pre-callback (binds the UI target, clears its depth/stencil): clear this frame's UI image to transparent
typedef u64 (*FnUI60)(u8 *); static FnUI60 oUI60;
static u64 hUI60(u8 *ctx) {
    const u64 r = oUI60(ctx);
    if (armFind(ctx) >= 0) {
        const int k = kNow(); const SIZE_T h = k >= 0 ? shRtv[k].load() : 0; GL *gl = fastList(ctx);
        if (h && gl) { const float z[4] = {0, 0, 0, 0}; gl->ClearRenderTargetView(D3D12_CPU_DESCRIPTOR_HANDLE{h}, z, 0, nullptr); rdClears++; } else rdNoK++;
    }
    return r;
}
// ---- 0.15.0 cinematic detector (BUG-57) on the UI-only image: the game's cinematic letterbox (CUiLetterbox, driven by the
// scripts' PushLetterbox/PopLetterbox) and its full-screen UI fades are UI draws. On the UI image they are unambiguous: rows
// near the top AND bottom edge opaque black across the whole width (bars), or the whole image dark and covered (a fade).
// Three rows (3 %, 50 %, 97 % of the height) are copied at pass 65 into a readback ring and read 4 frames later. While it
// holds the UI layer covers each eye's whole view (GV_UI_FULL) - bars at the top/bottom of the view, fades over everything.
static ID3D12Resource *rbUi[8]; static u8 *rbMap[8]; static u32 rbFrame[8], rbPitch = 0, rbW = 0, rbH = 0; static DXGI_FORMAT rbFmt = DXGI_FORMAT_R8G8B8A8_UNORM;
static std::atomic<int> cine{0}, cineFade{0}; static int cinePos = 0, cineNeg = 0; static std::atomic<u64> cineReads{0}, cineHits{0};
static std::atomic<u32> histZero{0}, histPart{0}, histFull{0}, lastTop{0}, lastBot{0}, lastMid{0};
// 0.15.2 (BUG-57b): 0.15.1 never fired - one row at 3 %/97 % with "opaque (a >= 230) and pure black" missed the thick UI bars
// (squared alpha, HUD-004) and the UI fades (a translucent dark veil), and the THIN bars are not UI at all: they are in the 3D
// picture. Now 13 rows from edge to edge (kRowY) are sampled and measured as BANDS: rows that are bar-black counted from each
// edge inwards, and rows covered by a dark veil. The same rows are read from the 3D picture (sbar, the published eye image).
static const float kRowY[13] = {0.005f, 0.02f, 0.04f, 0.07f, 0.10f, 0.14f, 0.50f, 0.86f, 0.90f, 0.93f, 0.96f, 0.98f, 0.995f};
static const int NROW = 13, NS = 32;
struct Bands { int top, bot, veil, midLit; };
static int px8(const u8 *p, bool ten, int c) {   // channel c (0..3) as 0..255 of an 8-bit or 10:10:10:2 pixel
    if (!ten) return p[c];
    const u32 w = *(const u32 *)p; return c == 3 ? (int)((w >> 30) * 85) : (int)(((w >> (10 * c)) & 1023) >> 2);
}
static std::atomic<int> rowEdgeMax{0}, rowEdgeAvg{0};   // census: brightness of the outermost 3D-picture rows (bar colour)
static Bands bandsOf(const u8 *m, u32 pitch, u32 w, bool ui, bool ten, int thr = 12) {
    Bands r{0, 0, 0, 0}; bool rowBar[NROW]; int rowVeil = 0, edgeSum = 0, edgeMax = 0;
    for (int y = 0; y < NROW; y++) {
        int bar = 0, veil = 0, lit = 0;
        for (int i = 0; i < NS; i++) {
            const u32 x = (u32)((0.03f + 0.94f * (float)i / (NS - 1)) * (float)(w - 1));
            const u8 *q = m + (u64)y * pitch + (u64)x * 4;
            int mx = px8(q, ten, 0); const int g = px8(q, ten, 1), bb = px8(q, ten, 2), a = ui ? px8(q, ten, 3) : 255; if (g > mx) mx = g; if (bb > mx) mx = bb;
            if (ui) { if (a >= 150 && mx <= 20) bar++; if (a >= 20 && mx * 2 <= a) veil++; }
            else { if (mx <= thr) bar++; if (mx >= 32) lit++; if (y == 0 || y == NROW - 1) { edgeSum += mx; if (mx > edgeMax) edgeMax = mx; } }
        }
        rowBar[y] = bar >= NS - 4; if (veil >= NS - 3) rowVeil++;
        if (y == 6) r.midLit = lit;
    }
    for (int y = 0; y < 6 && rowBar[y]; y++) r.top++;
    for (int y = NROW - 1; y > 6 && rowBar[y]; y--) r.bot++;
    r.veil = rowVeil;
    if (!ui) { rowEdgeMax = edgeMax; rowEdgeAvg = edgeSum / (2 * NS); }
    return r;
}
static bool uiHit(const Bands &b) { return (b.top >= 1 && b.bot >= 1) || b.veil == NROW; }                                  // bars, or a veil over everything
static bool sceneHit(const Bands &b) { return b.top >= 1 && b.bot >= 1 && abs(b.top - b.bot) <= 1 && b.midLit >= 4; }      // symmetric black bands, picture between
static std::atomic<int> maxTop{0}, maxBot{0}, maxVeil{0}, sMaxTop{0}, sMaxBot{0}; static std::atomic<int> sceneBars{0}, sbarRst{0}; static std::atomic<u64> sReads{0}, sHits{0};
static void amax(std::atomic<int> &a, int v) { int o = a.load(); while (v > o && !a.compare_exchange_weak(o, v)) {} }
static void cineRead(u32 f) {   // pass 65 of frame f: the readback of frame f-4
    const u32 g = f - 4; const int s = (int)(g & 7);
    if (rbFrame[s] != g || !rbMap[s] || !rbPitch) return;
    const Bands b = bandsOf(rbMap[s], rbPitch, rbW, true, false); cineReads++;
    { const u8 *q = rbMap[s] + 6 * (u64)rbPitch; for (int i = 0; i < NS; i++) { const u8 *p = q + (u64)((0.03f + 0.94f * (float)i / (NS - 1)) * (float)(rbW - 1)) * 4; if (p[3] == 0) histZero++; else if (p[3] == 255) histFull++; else histPart++; } }
    lastTop = (u32)b.top; lastBot = (u32)b.bot; lastMid = (u32)b.veil; amax(maxTop, b.top); amax(maxBot, b.bot); amax(maxVeil, b.veil);
    if (uiHit(b)) { cineHits++; cineNeg = 0; if (b.veil != NROW) cineFade = 0; if (++cinePos >= 2 && !cine.load()) { cine = 1; cineFade = b.veil == NROW; logf("UI: cinematic %s on the HUD image (bar rows top %d / bottom %d, veil rows %d of %d) -> HUD over the whole view", b.veil == NROW ? "fade" : "letterbox", b.top, b.bot, b.veil, NROW); } }
    else { cinePos = 0; if (++cineNeg >= 24 && cine.load()) { cine = 0; logf("UI: cinematic bars/fade gone from the HUD image -> HUD panel"); } }
}
static void rowsCopy(GL *gl, ID3D12Resource *src, ID3D12Resource *dst, DXGI_FORMAT fmt, u32 w, u32 h, u32 pitch) {
    D3D12_TEXTURE_COPY_LOCATION from{src, D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, {}};
    for (int i = 0; i < NROW; i++) {
        u32 y = (u32)(kRowY[i] * (float)h); if (y >= h) y = h - 1;
        D3D12_TEXTURE_COPY_LOCATION to{dst, D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT, {}};
        to.PlacedFootprint.Offset = (u64)i * pitch; to.PlacedFootprint.Footprint = {fmt, w, 1, 1, pitch};
        D3D12_BOX box{0, y, 0, w, y + 1, 1}; gl->CopyTextureRegion(&to, 0, 0, 0, &from, &box);
    }
}
static std::atomic<u64> cineSizeSkip{0};
static void cineCopy(GL *gl, ID3D12Resource *src, u32 f) {
    const int s = (int)(f & 7); ID3D12Resource *dst = rbUi[s]; const u32 w = rbW, h = rbH, p = rbPitch; if (!dst || !p || !src) return;
    // BUG-64b: while a new render size rebuilds the UI images and this ring (present thread), a render thread may see an old image
    // with the new ring or the reverse - a copy box outside the image is a GPU fault. Both sides must match exactly.
    const D3D12_RESOURCE_DESC sd = src->GetDesc(), dd = dst->GetDesc();
    if (sd.Width != w || sd.Height != h || dd.Width < (u64)p * NROW || p < w * 4) { cineSizeSkip++; return; }
    rowsCopy(gl, src, dst, rbFmt, w, h, p); rbFrame[s] = f;
}
static bool active(float p[4], const int r[4]) {
    if (!hud.load() || g_cfg.xr != 4) return false;
    if (flat() || mode.load() == M_CINEMA) { skipFlat++; return false; }
    if (!panel(cur, r[2] - r[0], r[3] - r[1], hudDeg.load(), p)) { skipNoView++; return false; }
    return true;
}
static bool applyMap(u8 *ctx, bool bb, bool fullScissor);
static std::atomic<u64> uiPassMask{0}, uiPassSeen{0};   // census: which passes bind the UI target (bit = pass - 32)
static void hRT(u8 *ctx, u8 *target, int *rect, int slice) {
    liveDel(ctx);   // the bind's own scissor call belongs to the NEW target
    armDel(ctx);    // ...and so does the layer redirect
    const bool isUi = target && target == g_base + RVA_UI_TARGET;
    const int pass = *(int *)(ctx + 0x2A50);
    // BUG-58b: RT_Bind flushes its targets ITSELF (Ctx_FlushRT at its end) - arm before the bind, or the one flush that binds
    // the UI target is missed (0.15.0: "redirected flushes=0", the UI stayed full-size in the 3D picture)
    if (isUi && pass >= 60 && pass <= 65 && layerOn()) { armAdd(ctx); lBinds++; armedFrame++; }
    oRT(ctx, target, rect, slice);
    if (!isUi) return;
    binds++;
    {   // probe: colour attachment 0 (ctx+0x2C8, set by Ctx_BindColor 0x50E430) -> resource record (+0x18) -> native
        u8 *att = *(u8 **)(ctx + 0x2C8); u8 *rr = (att && readable(att + 0x18, 8)) ? *(u8 **)(att + 0x18) : nullptr;
        ID3D12Resource *nat = taa::nativeRec(rr);
        if (nat && nat != probeNat.load()) { probeNat = nat; probeRec = rr; probeFlags = *(u32 *)(target + 0xC8); probeCount = *(u32 *)(target + 0x618); probeLogged = 0; }
    }
    { const int *rr2 = (const int *)(ctx + 0x2948); if (rr2[2] > rr2[0] && rr2[3] > rr2[1]) { const float an = rsz::ana.load(); uiAspect = an > 0.5f ? 1.0f / an : (float)(rr2[3] - rr2[1]) / (float)(rr2[2] - rr2[0]); } }
    if (pass >= 32 && pass < 96) { uiPassMask |= 1ull << (pass - 32); uiPassSeen |= 1ull << (pass - 32); }
    if (layerOn()) {   // full-size UI, redirected into this frame's UI image at the flush (UI passes only: pass 59 writes the 3D picture here)
        if (!(pass >= 60 && pass <= 65)) lSkip++;
        return;
    }
    applyMap(ctx, (*(u32 *)(target + 0xC8) & 0x400000) != 0, false);
}
// map the bound UI target's viewport + scissor into this eye's panel rect (true = mapped)
static bool applyMap(u8 *ctx, bool bb, bool fullScissor) {
    const int *r = (const int *)(ctx + 0x2948); float p[4];
    if (r[2] - r[0] <= 0 || r[3] - r[1] <= 0 || !active(p, r)) return false;
    {   // exact float viewport (may lie partly outside the target: a world-locked panel slides off the edge instead of being
        // squashed; Ctx_SetViewport takes unsigned ints) - written where Ctx_SetViewport keeps it, then applied the same way
        float *vp = (float *)(ctx + 0x310); GL *gl = fastList(ctx);
        const float x0 = roundf((float)r[0] + p[0]), y0 = roundf((float)r[1] + p[1]), x1 = roundf((float)r[0] + p[2]), y1 = roundf((float)r[1] + p[3]);
        vp[0] = x0; vp[1] = y0; vp[2] = fmaxf(x1 - x0, 1.0f); vp[3] = fmaxf(y1 - y0, 1.0f); vp[4] = 0.0f; vp[5] = 1.0f;
        if (gl) gl->RSSetViewports(1, (const D3D12_VIEWPORT *)vp);
    }
    const u32 *sc = (const u32 *)(ctx + 0x328); u32 m[4];   // back-buffer targets set their scissor elsewhere: use the full rect
    if (bb || fullScissor) mapRect(r, p, (u32)r[0], (u32)r[1], (u32)r[2], (u32)r[3], m, true); else mapRect(r, p, sc[0], sc[1], sc[2], sc[3], m, true);
    oSC(ctx, m[0], m[1], m[2], m[3]);
    liveAdd(ctx); mapped++;
    return true;
}
static void hSC(u8 *ctx, u32 l, u32 t, u32 rr, u32 b) {
    if (liveFind(ctx) >= 0) {
        const int *r = (const int *)(ctx + 0x2948); float p[4];
        if (r[2] - r[0] > 0 && r[3] - r[1] > 0 && active(p, r)) { u32 m[4]; mapRect(r, p, l, t, rr, b, m, true); scissors++; oSC(ctx, m[0], m[1], m[2], m[3]); return; }
    }
    oSC(ctx, l, t, rr, b);
}
// BUG-53 (0.14.4): world-anchored UI (markers, prompts and icons placed on things in the world) is drawn by renderer
// class UI_WorldDraw 0x6E37D0 (vtable slot 8; it projects each element with the VIEW's own view-projection, view+0x5C0
// - i.e. already per eye and correct in stereo). Mapping it into the HUD panel squeezed world things into the panel
// and made them move with it ("the world stretches like the HUD when I look up and down"). Batch_Draw 0x5681A0
// (renderer, range{ctx, view, ...}) runs each record batch: for that class the full frame is restored around the call,
// then the panel mapping is re-applied for the screen-space UI after it. Other classes are counted (census).
typedef void (*FnBatch)(void **, u8 **); static FnBatch oBatch;
static std::atomic<u64> worldUi{0}; struct BCls { std::atomic<u32> fn; std::atomic<u32> n; }; static BCls bcls[12];
static void hBatch(void **obj, u8 **rng) {
    u8 *ctx = rng ? rng[0] : nullptr;
    if (!ctx || liveFind(ctx) < 0) { oBatch(obj, rng); return; }
    void **vt = obj ? *(void ***)obj : nullptr; void *draw = vt ? vt[8] : nullptr;
    const u32 fr = draw ? (u32)((u8 *)draw - g_base) : 0;
    for (auto &c : bcls) { u32 z = 0; if (c.fn.load() == fr) { c.n++; break; } if (c.fn.compare_exchange_strong(z, fr)) { c.n++; break; } }
    if (draw != (void *)(g_base + RVA_UI_WORLDDRAW)) { oBatch(obj, rng); return; }
    const int *r = (const int *)(ctx + 0x2948);
    if (r[2] > r[0] && r[3] > r[1]) {   // whole frame for the world-anchored elements
        liveDel(ctx);
        ((FnVP)(g_base + RVA_CTX_VIEWPORT))(ctx, (u32)r[0], (u32)r[1], (u32)r[2], (u32)r[3]); oSC(ctx, (u32)r[0], (u32)r[1], (u32)r[2], (u32)r[3]);
        oBatch(obj, rng); worldUi++;
        applyMap(ctx, false, true);   // the screen-space UI after it goes back onto the panel
        return;
    }
    oBatch(obj, rng);
}
// BUG-45 (0.14.0): pass 65's pre-callback 0x610460 -> 0x728400 draws the game's full-screen fade (to/from black) into
// the target its context INHERITED from the UI passes (Ctx_InheritState -> RT_Bind(UI target)), i.e. into the mapped
// panel / the layer. The fade belongs to the whole frame: full viewport + scissor, the engine's own UI image.
typedef u64 (*FnP65)(u8 *); static FnP65 oP65;
static u64 hP65(u8 *ctx) {
    if (armFind(ctx) >= 0) { armDel(ctx); ctx[0x340] |= 1; fadeFix++; }   // the fade goes into the 3D picture: the engine re-binds its own target at the fade's draw
    if (layerOn()) {   // the UI of this frame is complete (pass 64 is before us in GPU order): publish its image, sample it for the cinematic detector
        // fail closed (playbook: prove the fallback draws): UI binds armed but none redirected for 30 frames -> the 0.14 panel
        if (armedFrame.load() > 1 && rdFrame.load() == 0) { if (++rdFailRun >= 30 && !rdRefused.exchange(1)) logf("UI layer: the redirect is not taking effect (30 frames with UI binds, none redirected) -> HUD back on the 0.14 panel (F1 retries)"); }
        else if (rdFrame.load()) rdFailRun = 0;
        const int k = kNow(); GL *gl = fastList(ctx); const u32 f = frameCtr();
        // BUG-59: world-anchored prompts ("E: mount") are placed with THIS eye's view; xr=4 alternates eyes per frame, so
        // left/right images alternated by the eye offset -> the prompt jittered on the still panel. Only left-eye frames publish.
        if (cur.ok && cur.eye == 1) rightSkip++;
        else if (k >= 0 && gl) { pend[f & 31] = {f, k}; cineCopy(gl, sh[k].load(), f); cineRead(f); lCopies++; } else rdNoK++;
    }
    if (liveFind(ctx) >= 0) {
        const int *r = (const int *)(ctx + 0x2948);
        if (r[2] > r[0] && r[3] > r[1]) { ((FnVP)(g_base + RVA_CTX_VIEWPORT))(ctx, (u32)r[0], (u32)r[1], (u32)r[2], (u32)r[3]); oSC(ctx, (u32)r[0], (u32)r[1], (u32)r[2], (u32)r[3]); }
        liveDel(ctx); fadeFix++;
    }
    return oP65(ctx);
}
// BUG-64d (0.16.2: "pre-rendered cutscenes too smushed"): Movie_Draw letterboxes the 16:9 video from the RENDER aspect SC+0x1A8
// (swapchain vt+0xB0 = SC+0x188, +0x20), still 1:1 -> a 16:9 band in the square image, which the host then showed at 16:9
// (squashed). During the draw the render aspect is the layout aspect too: the video fills the anamorphic image like the UI.
static void hMovie(void *a, void *b, void *c, void *d) {
    lastMovie = post.load(); movieDraws++;
    u8 *sc = rsz::scObj; const float an = rsz::ana.load();
    if (sc && an > 0.5f) { float *ra = (float *)(sc + 0x1A8); const float r0 = *ra; *ra = an; oMovie(a, b, c, d); *ra = r0; return; }
    oMovie(a, b, c, d);
}
// 0.14.3 start screen (HUD-002, world-locked gravity-aligned panel) without a layer: the panel is a rectangle fixed in the
// runtime's LOCAL space, placed in front of the head (yaw only) when the start screen appears or on F11; every frame
// each eye's mapped rect is where that rectangle (billboarded, no roll) projects in THIS eye's frustum.
static void qrotInv(const float q[4], const float v[3], float o[3]) {   // conj(q) * v * q  (q = x, y, z, w)
    const float x = -q[0], y = -q[1], z = -q[2], w = q[3];
    const float tx = 2 * (y * v[2] - z * v[1]), ty = 2 * (z * v[0] - x * v[2]), tz = 2 * (x * v[1] - y * v[0]);
    o[0] = v[0] + w * tx + (y * tz - z * ty); o[1] = v[1] + w * ty + (z * tx - x * tz); o[2] = v[2] + w * tz + (x * ty - y * tx);
}
static float yawOf(const float *q) { const float fx = -2.0f * (q[0] * q[2] + q[3] * q[1]), fz = -(1.0f - 2.0f * (q[0] * q[0] + q[1] * q[1])); return atan2f(-fx, -fz); }
// game thread, every tick: the panel keeps its yaw in the room while you look around; after you have looked more than
// hud_follow_deg away for 0.5 s it eases back in front of you (hud_follow_speed); it follows your head position (you
// can not walk away from it) and stays gravity-aligned. F11 or entering a new context re-centres it at once.
static float wYaw = 0; static u64 wLastMs = 0, wAwaySince = 0; static bool wEasing = false;
static void follow(bool entered) {
    GvPose h{}; u32 ok = 0;
    if (!S || !gvRead(&S->poseSeq, [&] { h = S->head; ok = S->poseValid; }) || !ok) return;
    static LONG rc = 0; const LONG nrc = S->theaterRecenter;
    const float yh = yawOf(h.q); const u64 now = GetTickCount64(); const float dt = wLastMs ? fminf((float)(now - wLastMs) * 0.001f, 0.1f) : 0.0f; wLastMs = now;
    const bool pr = placeReq.exchange(0) != 0;
    if (!wp.on || entered || nrc != rc || pr) { rc = nrc; wYaw = yh; wEasing = false; wAwaySince = 0; if (!wp.on || entered || pr) logf("UI: panel placed in the room in front of you (yaw %.1f deg)%s", yh * 57.29578f, pr ? " - HUD gesture" : ""); }
    else {
        float dy = yh - wYaw; while (dy > 3.14159265f) dy -= 6.2831853f; while (dy < -3.14159265f) dy += 6.2831853f;
        const float lim = g_ui.followDeg / 57.29578f;
        if (fabsf(dy) > lim) { if (!wAwaySince) wAwaySince = now; if (now - wAwaySince > 500) wEasing = true; } else wAwaySince = 0;
        if (wEasing) { const float step = g_ui.followSpeed / 57.29578f * dt; if (fabsf(dy) <= step || fabsf(dy) < 2.0f / 57.29578f) { wYaw = yh; wEasing = false; } else wYaw += dy > 0 ? step : -step; }
    }
    const float d = g_ui.hudDist;
    wp.P[0] = h.p[0] - sinf(wYaw) * d; wp.P[1] = h.p[1] - d * tanf(g_ui.hudDown / 57.2957795f); wp.P[2] = h.p[2] - cosf(wYaw) * d;
    wp.w = 2.0f * d * tanf(hudDeg.load() * 0.5f / 57.2957795f); wp.on = true;
}
static void placeWorld(const GvPose &head) {
    const float *q = head.q; const float fx = -2.0f * (q[0] * q[2] + q[3] * q[1]), fz = -(1.0f - 2.0f * (q[0] * q[0] + q[1] * q[1]));
    float l = sqrtf(fx * fx + fz * fz), ux = 0, uz = -1; if (l > 1e-3f) { ux = fx / l; uz = fz / l; }
    const float d = g_ui.hudDist;
    wp.P[0] = head.p[0] + ux * d; wp.P[1] = head.p[1] - d * tanf(g_ui.hudDown / 57.2957795f); wp.P[2] = head.p[2] + uz * d;
    wp.w = 2.0f * d * tanf(hudDeg.load() * 0.5f / 57.2957795f); wp.on = true;
    logf("UI: start-screen panel fixed in the room %.2f m ahead (yaw %.1f deg), %.2f m wide", d, atan2f(-ux, -uz) * 57.29578f, wp.w);
}
static void beginPost() {   // game thread, main view, before its render graph is scheduled
    post++; posts++;
    for (auto &a : arm) a.store(nullptr); armN = 0;   // pass contexts are pooled and reused next frame: no arm survives a frame
    armedFrame = 0; rdFrame = 0;
    if (layerOn()) { const u32 f = frameCtr(); kOf[f & 31] = {f, (int)(shNext++ % GV_UITEX)}; }   // this frame's UI image (its UI passes run after this)
    worldMode = *(volatile u8 *)(r1::frameObj() + 0x80F09); worldOff = worldMode.load() == 0;
    View v{}; const FramePkt &k = g_cur;
    if (g_cfg.xr == 4 && k.valid && k.eye >= 0 && k.eye < 2) {
        v.ok = true; v.eye = k.eye; v.tl = tanf(k.fov[0]); v.tr = tanf(k.fov[1]); v.tu = tanf(k.fov[2]); v.td = tanf(k.fov[3]); v.ex = (k.eye == 0 ? -0.5f : 0.5f) * g_ipd;
        if ((mode.load() == M_TITLE || mode.load() == M_HUD) && wp.on) {   // where the room-fixed panel is, seen from this eye (LOCAL space eye pose)
            const float dv[3] = {wp.P[0] - k.pose.p[0], wp.P[1] - k.pose.p[1], wp.P[2] - k.pose.p[2]}; float e[3]; qrotInv(k.pose.q, dv, e);
            v.wl = true;
            if (e[2] < -0.1f) {
                const float z = -e[2], hw = 0.5f * wp.w / z, hh = 0.5f * wp.w * uiAspect.load() / z, cx = e[0] / z, cy = e[1] / z;
                v.wx0 = cx - hw; v.wx1 = cx + hw; v.wyT = cy + hh; v.wyB = cy - hh;
            } else { v.wx0 = -60.0f; v.wx1 = -59.99f; v.wyT = 60.0f; v.wyB = 59.99f; }   // behind you: far outside the view (scissored away)
        }
    }
    cur = v;
}
static void endPost() {   // game thread: the flat-context decision for this frame's published image
    const u32 f = frameCtr(); const bool fl = flat(), mv = movieNow(); if (fl) flatPosts++;
    // 0.16.0: a 16:9 video is letterboxed inside the square image -> only its band on the screen; 0.16.2 (BUG-64): the game is told
    // 16:9, so the video fills the (anamorphic) image and the host shows the whole image at 16:9 instead
    const float an = rsz::ana.load(); if (S) S->dispAspect = an;
    const bool sq = rsz::want.load() > 0 && !(an > 0.5f);
    ring[f & 31] = {f, fl ? (GV_SLOT_THEATER | ((crop || sq) && mv ? GV_SLOT_CROP169 : 0u) | (g_flatRender.load() ? GV_SLOT_FLAT : 0u)) : 0u};
    u32 uf = 0;
    if (layerOn()) {
        const float fa = *(volatile float *)(g_base + RVA_FADE + 0xC);   // the game's own fade (pass 65) covers the UI on the monitor
        if (fa > 0.3f) rdHidden++;
        else uf = mode.load() == M_CINEMA ? GV_UI_FULL : (mode.load() == M_TITLE || g_ui.follow) ? GV_UI_WORLD : GV_UI_HEAD;
        if (hudHidden.load() && mode.load() == M_HUD) uf = 0;
        // 0.18.5 (owner: "it is too big for me to see ... the hud should be in the smaller, world locked mode"): the gesture no longer
        // enlarges the HUD or locks it to the head. It places the normal room-fixed panel straight in front of you (ui::placeReq,
        // taken by follow()) and the default gesture's touchpad swipe-up makes the game show the objective on it.
        if (S) S->hudDeg = hudDeg.load();
    }
    uiRing[f & 31] = {f, uf};
    static int was = -1; const int st = (fl ? 1 : 0) | (mv ? 2 : 0) | (worldMode.load() << 2) | (pausedTick.load() ? 64 : 0);
    if (st != was) { logf("UI: %s (video=%d, paused=%d, world mode=%u, theatre setting=%s, flat render=%d)", fl ? "FLAT CONTEXT -> theatre screen" : "3D -> HUD panel", mv, (int)pausedTick.load(), worldMode.load(),
                          theaterMode.load() == 1 ? "always" : theaterMode.load() == 2 ? "never" : "auto", (int)g_flatRender.load()); was = st; }
}
static u32 flagsOf(u32 frame) { const Flag &x = ring[frame & 31]; return x.frame == frame ? x.flags : 0u; }
static u32 uiFlagsOf(u32 frame) { const Flag &x = uiRing[frame & 31]; return x.frame == frame ? x.flags : 0u; }
} // namespace ui
static bool shade::gameplayNow() { return ui::fpBlockReason() == 0 && fp::activeNow && !rsz::applying.load(); }   // 0.18.24
// ---------------- 0.18.0 VR controllers: the virtual pad (vrinput.h) wired into the XInput exports ----------------
void vrin::animHold(int sh) {
    const u64 now = GetTickCount64(); fp::nAnimHolds++;
    if (sh) { const u64 u = now + 2500; if (u > fp::animUntil.load()) fp::animUntil = u; }
    else { fp::animUntil = now + 60000; fp::animMove = 1; logf("controls: the game's animation owns Jin's arms (flute) until you move or dodge"); }
}
void vrin::animMoved() { if (fp::animMove.exchange(0)) fp::animUntil = GetTickCount64() + 300; }
void vrin::recenterView() { vrin::moveCapture(150); m_recenter = 1; if (S) InterlockedIncrement(&S->theaterRecenter); g_flatCut = 2; occ::reseed(); logf("controls: wheel -> recenter view (F11)"); }
void vrin::hudToggle() { ui::hudBoostUntil = GetTickCount64() + 6000; ui::hudHidden = 0; ui::placeReq = 1; logf("controls: HUD panel placed in front of you (left hand at the head + trigger)"); }
namespace vrin {
struct XS { DWORD packet; XINPUT_GAMEPAD g; };
static bool padImpl(DWORD, u8 *st, bool realOk) {
    XINPUT_GAMEPAD v; const bool flat = g_flatRender.load() || ui::fpBlockReason() != 0;
    const u64 fnow = GetTickCount64(); const u32 fm = (fnow >= forceFrom.load() && fnow < forceUntil.load()) ? forceMask.load() : 0u;   // 0.18.54 cbt_assist (off by default)
    if (!build(v, flat)) { if (!fm) return false; v = XINPUT_GAMEPAD{}; }   // no VR input ever: the real pad alone (unless cbt_assist presses)
    v.wButtons |= (WORD)fm;
    XS *x = (XS *)st; XINPUT_GAMEPAD g = realOk ? x->g : XINPUT_GAMEPAD{};
    // merge (playbook ch03: one lane per control by saturation): buttons OR, triggers max, each stick from the larger deflection
    g.wButtons |= v.wButtons; if (v.bLeftTrigger > g.bLeftTrigger) g.bLeftTrigger = v.bLeftTrigger; if (v.bRightTrigger > g.bRightTrigger) g.bRightTrigger = v.bRightTrigger;
    auto mag = [](SHORT a, SHORT b) { return (int)a * a + (int)b * b; };
    if (mag(v.sThumbLX, v.sThumbLY) > mag(g.sThumbLX, g.sThumbLY)) { g.sThumbLX = v.sThumbLX; g.sThumbLY = v.sThumbLY; }
    if (mag(v.sThumbRX, v.sThumbRY) > mag(g.sThumbRX, g.sThumbRY)) { g.sThumbRX = v.sThumbRX; g.sThumbRY = v.sThumbRY; }
    if (memcmp(&g, &last, sizeof g)) { last = g; packet++; }
    x->packet = (realOk ? x->packet : 0) + (packet << 16); x->g = g;   // a new packet number whenever either source changes
    return true;
}
static void rumbleImpl(DWORD, const void *v) { rumble((const XINPUT_VIBRATION *)v); }
static bool capsImpl(DWORD, u8 *c) {   // after the first VR activation the virtual pad stays connected for the process lifetime
    if (!everActive && !nForced.load()) return false;   // 0.18.54: cbt_assist connects the pad in flat play too
    if (c) { memset(c, 0, 20); c[0] = 1; c[1] = 1; *(WORD *)(c + 4) = 0xF3FF; c[6] = c[7] = 255; for (int k = 0; k < 4; k++) *(SHORT *)(c + 8 + 2 * k) = (SHORT)0xFFC0; *(WORD *)(c + 16) = 0xFFFF; *(WORD *)(c + 18) = 0xFFFF; }
    return true;
}
static void install(const wchar_t *ini) {
    loadCfg(ini);
    if (!cfg.enabled) { logf("controls: VR controllers off ([controls] enabled=0)"); return; }
    g_padHook = padImpl; g_rumbleHook = rumbleImpl; g_capsHook = capsImpl;
    logf("controls: VR controllers -> virtual Xbox pad (user 0), merged with a real pad; turn = %s %.0f deg; wheel: 1 %s, 2 %s, 3 %s",
         cfg.turnSmooth ? "smooth (speed" : "snap", cfg.turnSmooth ? cfg.smoothGain * 100.0f : cfg.snapDeg, cfg.wheelName[0], cfg.wheelName[1], cfg.wheelName[2]);
    for (int c = 0; c < C_N; c++) logf("controls:   %-28s -> mask 0x%08X (%s)", kCtl[c].key, cfg.out[c].m, kCtl[c].what);
}
} // namespace vrin

// game thread, start of every xr=4 tick: one flat-context decision per tick (render path, UI mapping and flags agree)
static void uiDecide() {
    ui::pausedTick = ui::pausedNow();
    {   // 0.14.5 flat contexts beyond pause/video:
        // - loading: world mode 1 or time-scale reason 9 (world load) at 0;
        // - black screen (BUG-55b): the game's own full-screen fade 0x14149AF80 (rgba, alpha at +0xC; pass 65 draws it) fully
        //   opaque in world mode 2 for 150 ms - the post-load streaming screen with its tips and other fade-to-black moments
        //   (nothing of the 3D world is visible then); back to stereo as soon as it starts to fade in (alpha < 0.99);
        // - start screen (BUG-54b): the title world's fixed camera - position (5907, 3490, 17158) +-300 with the title
        //   field of view 0.789 rad, the same in every log of this build (0.14.0, 0.14.2, 0.14.4); 0.14.4's "first world
        //   after boot" guess caught a 0.5 s staging world instead.
        const int wm = ui::worldMode.load();
        const bool loading = g_ui.loadTheater && (wm == 1 || ui::scaleOf(9) == 0.0f);
        const float alpha = *(volatile float *)(g_base + RVA_FADE + 0xC);
        static u64 blackSince = 0; static bool black = false; const u64 now = GetTickCount64();
        if (wm == 2 && alpha > 0.996f) { if (!blackSince) blackSince = now; if (now - blackSince > 150) black = true; } else { blackSince = 0; if (alpha < 0.99f || wm != 2) black = false; }
        bool title = false; float cp[3] = {0, 0, 0}, fov = 0;
        { u64 a = camA(); if (isCam(a)) { const float *w = (const float *)(a + 0x140); cp[0] = w[12]; cp[1] = w[13]; cp[2] = w[14]; fov = *(const float *)(a + 0x670); } }
        { const float dx = cp[0] - 5907.0f, dy = cp[1] - 3490.0f, dz = cp[2] - 17158.0f; title = wm == 2 && dx * dx + dy * dy + dz * dz < 300.0f * 300.0f && fabsf(fov - 0.789f) < 0.02f; }
        static bool wasT = false, wasB = false; if (title != wasT) { logf("UI: start screen %s (camera %.0f %.0f %.0f, fov %.3f)", title ? "-> theatre screen" : "left", cp[0], cp[1], cp[2], fov); wasT = title; }
        if (black != wasB) { logf("UI: %s (game fade alpha %.3f)", black ? "black screen (loading / fade to black) -> theatre screen" : "screen visible again -> stereo", alpha); wasB = black; }
        { static int lw = -1; if (wm != lw) { logf("UI: world mode %d (camera %.0f %.0f %.0f, fov %.3f)", wm, cp[0], cp[1], cp[2], fov); lw = wm; } }
        ui::loadingTick = loading || (g_ui.loadTheater && black); ui::titleTick = g_ui.menuTheater && title;
    }
    const bool fl = ui::flatRaw(), fr = fl && g_ui.flatRender;
    if (fr != g_flatRender.load()) { g_flatCut = 2; logf("UI: %s", fr ? "flat context -> the game renders a normal flat frame for the theatre screen" : "back to stereo"); }
    ui::flatNow = fl; g_flatRender = fr;
    {   // 0.15.5 (BUG-57e) cutscene state: the 0.15.4 log shows the THICK 32:9 bars are visible to the detector only ~0.5 s per
        // shot (the 3D picture's edge rows are bright the rest of the time), while the camera keeps the cutscene's own field of
        // view for the whole sequence (1.028 rad vs gameplay 0.935, back to 0.934 the moment it ends) and the THIN-bar shots set
        // the camera's projection rect to +-0.758 (the engine's own letterbox, ADDR-127). The cutscene fov is already set ~4 s
        // before the first bars, so a "gameplay fov" learned during HUD mode would be the cutscene's. So: any evidence (bars in
        // the 3D picture or the HUD image, or the letterbox rect) STARTS a cutscene; it HOLDS while the camera keeps the field of
        // view it had at the start, and ENDS when the fov changes (the cutscene camera hands back) with no evidence for 0.5 s,
        // or after 15 s without any evidence (safety).
        // 1 s after a pausing menu closes the HUD is also shown large (owner request; 3 s in 0.15.5).
        static float entryFov = 0; static bool cutHold = false, wasPaused = false; static u64 lastEv = 0, pauseUntil = 0;
        const u64 nowMs = GetTickCount64(); float cfov = 0, rectT = 1.0f;
        { u64 a = camA(); if (isCam(a)) { cfov = *(const float *)(a + 0x670); rectT = *(const float *)(a + 0x6A4 + 12); } }
        const bool rectLb = ui::worldMode.load() == 2 && rectT > 0.3f && rectT < 0.99f;
        bool ev = ui::sceneBars.load() || ui::cine.load() || rectLb;
        // 0.18.64 (owner, session 19: "when I died it went third person, which was good, but when I respawned I was still third person
        // until I went into the menu and back"; log: the death camera started a cutscene from dark picture edges, the respawn's load did
        // not end it, and the dark edges after the load kept "bars" held for 6 s): a load / black screen or a NEW hero (respawn) ends any
        // cutscene and makes the bar detector start over (a real cutscene after the load is found again within 3 frames)
        { static u8 *lastHero = nullptr; u8 *hh = fp::heroPtr(); const bool heroNew = hh && lastHero && hh != lastHero; if (hh) lastHero = hh;
          if ((ui::loadingTick.load() || heroNew) && (cutHold || ui::sceneBars.load())) { cutHold = false; ui::sceneBars = 0; ui::sbarRst = 1; lastEv = 0;
              logf("UI: cutscene state cleared (%s) -> first person again", heroNew ? "a new hero - respawn" : "a load / black screen"); } }
        ev = ui::sceneBars.load() || ui::cine.load() || rectLb;
        if (ev) lastEv = nowMs;
        static bool fadeOnly = false;
        if (cutHold && ev && (rectLb || ui::sceneBars.load() || (ui::cine.load() && !ui::cineFade.load()))) fadeOnly = false;   // real bars upgrade a fade start
        // 0.18.61 (owner: "when I died, the revive UI appeared in big screen at first before going on the world-locked HUD"): the fade
        // to the death / revive screen is not a cutscene - while Jin is down (1 health or less, and 8 s after) a FADE alone starts nothing
        // and ends a fade-only hold
        const bool down = nowMs - g_jinDownAt.load() < 8000;
        const bool fadeStart = !rectLb && !ui::sceneBars.load() && ui::cineFade.load();
        if (cutHold && fadeOnly && down) { cutHold = false; logf("UI: a fade while Jin is down (death / revive) - not a cutscene -> HUD panel"); }
        if (!cutHold && ev && !fl && !(down && fadeStart)) { cutHold = true; fadeOnly = !rectLb && !ui::sceneBars.load() && ui::cineFade.load(); entryFov = cfov; logf("UI: cutscene START (%s; camera fov %.3f) -> HUD over the whole view", rectLb ? "camera letterbox rect" : ui::sceneBars.load() ? "bars in the 3D picture" : "bars/fade on the HUD image", cfov); }
        else if (cutHold && !ev) {
            const u64 quiet = nowMs - lastEv; const bool moved = fabsf(cfov - entryFov) > 0.004f;
            if ((moved && quiet > 500) || quiet > 15000 || fl || (fadeOnly && quiet > 500)) { cutHold = false; logf("UI: cutscene END (%s; camera fov %.3f, was %.3f) -> HUD panel", fl ? "flat context" : fadeOnly ? "a fade only, gone" : moved ? "the camera's field of view changed" : "15 s without bars", cfov, entryFov); }
        }
        else if (cutHold && ev && fabsf(cfov - entryFov) > 0.004f) entryFov = cfov;   // a new shot with its own fov while bars are visible
        const bool pz = ui::pausedTick.load();
        if (wasPaused && !pz) { pauseUntil = nowMs + 1000; logf("UI: pause menu closed -> HUD large for 1 s"); }
        wasPaused = pz;
        // 0.18.61 (owner: "when the player gets hit, trigger the UI in full screen so the hit effect is full screen"): for ui_hit_fullscreen_ms
        // after Jin loses health the HUD layer covers the whole view (the game's red hit flash where it belongs) - 0 = off
        static int hitMs = -1; if (hitMs < 0) { wchar_t ini[MAX_PATH]; swprintf(ini, MAX_PATH, L"%ls\\gotvr.ini", g_dir); wchar_t t[16];
            // 0.18.62 (owner: "when I got hit, my whole screen would go black - totally unplayable"): OFF by default (and an ini from 0.18.61 set to 0
            // once); the red-vignette replacement comes later
            if (!GetPrivateProfileStringW(L"gotvr", L"ui_hit_fullscreen_v", L"", t, 16, ini)) { WritePrivateProfileStringW(L"gotvr", L"ui_hit_fullscreen_ms", L"0", ini); WritePrivateProfileStringW(L"gotvr", L"ui_hit_fullscreen_v", L"2", ini); }
            hitMs = GetPrivateProfileIntW(L"gotvr", L"ui_hit_fullscreen_ms", 0, ini); if (hitMs < 0) hitMs = 0; }
        const bool hitFlash = hitMs > 0 && g_jinHurtAt.load() && nowMs - g_jinHurtAt.load() < (u64)hitMs && !down;
        const bool cine = (cutHold || nowMs < pauseUntil || hitFlash) && ui::layerReady();
        ui::camCut = cutHold && !fadeOnly;   // first person yields only to a camera cutscene
        const bool title = ui::titleReason >= 0 && ui::titleReason < 28 && ui::scaleOf(ui::titleReason) == 0.0f && ui::worldMode.load() == 2 && !ui::pausedTick.load();
        const int m = fl ? ui::M_FLAT : cine ? ui::M_CINEMA : title ? ui::M_TITLE : ui::M_HUD;
        if (m != ui::mode.load()) { static const char *nm[4] = {"HUD (gameplay)", "START SCREEN (world-locked)", "CINEMATIC (UI over the whole view: letterbox bars, fades, subtitles)", "FLAT (theatre screen)"};
                                    logf("UI mode -> %s%s", nm[m], (m <= 1 && ui::layerReady()) ? " on the UI layer" : (m <= 1 ? " on the mapped panel" : "")); }
        if ((m == ui::M_TITLE || m == ui::M_HUD) && g_ui.follow) ui::follow(m != ui::mode.load());   // panel fixed in the room, lazy follow (HUD-002)
        else ui::wp.on = false;
        if (S) {   // v9: the host places the sharp UI quad where the panel is
            gvWriteBegin(&S->uiPoseSeq);
            if (ui::wp.on) { S->uiPos[0] = ui::wp.P[0]; S->uiPos[1] = ui::wp.P[1]; S->uiPos[2] = ui::wp.P[2]; S->uiYaw = ui::wYaw; S->uiWidth = ui::wp.w; S->uiPoseOk = 1; } else S->uiPoseOk = 0;
            gvWriteEnd(&S->uiPoseSeq);
        }
        ui::mode = m; }
    {   // census (log only): time-scale reasons that change, to name the menus (pause, map, photo mode, title screen...)
        static float last[28]; static bool init = false;
        for (int ch = 0; ch < 28; ch++) { float v = ui::scaleOf(ch); if (init && v != last[ch]) logf("UI census: clock time-scale reason %d: %g -> %g (world mode %u)", ch, last[ch], v, ui::worldMode.load()); last[ch] = v; }
        init = true;
    }
}
static void hRVPostInner(void *view, void *ctx);
static void hRVPost(void *view, void *ctx) {
    const bool mainV = (u8 *)view == r1::mainView();
    if (mainV) ui::beginPost();
    hRVPostInner(view, ctx);
    if (mainV) ui::endPost();
}
static void hRVPostInner(void *view, void *ctx) {
    if ((u8 *)view != r1::mainView() || !r1::thisFrame()) { r1::checkPassTable(); r1::oRVPost(view, ctx); return; }
    r1::flagMain = *((u8 *)view + 0x18); r1::flagV2 = r1::V2[0x18];
    r1::pjReset();
    r1::checkPassTable();
    const int sf = r1::tabOk ? r1::skipFrom.load() : 0;   // one mask for patch, restore and this frame's pass jobs
    r1::frameSkip = sf;
    u64 m0 = 0, m1 = 0;
    if (r1::tabOk) {
        if (sf > 0) for (int i = sf; i < NPASS; i++) r1::setBit(m0, m1, i);
        m0 |= r1::autoM0.load() | r1::fixM0.load(); m1 |= r1::autoM1.load() | r1::fixM1.load();
        m0 &= ~((1ull << 14) - 1);
    }
    r1::frameM0 = m0; r1::frameM1 = m1;
    r1::oRVPost(view, ctx);
    r1::poolMid = *(u32 *)(g_base + 0x1A0AB50 + 0x2B5C60);
    r1Mark = r1::poolMid.load();
    r1::resetBetweenRuns();
    if (!r1::veh) { r1::gameTid = GetCurrentThreadId(); r1::veh = AddVectoredExceptionHandler(1, r1::onFault); }
    { static bool first = true; if (first) { first = false; logf("R1: first second-graph run starting (frame %u, contexts after main %u, viewconst counter was %u)", frameCtr(), r1::poolMid.load(), r1::vcBefore.load()); logflush(); } }
    // native stereo / F1: the second render gets the right eye
    const bool stereo = (g_cur.valid && g_cur.stereo) || m_offTest.load();
    bool capSwap = false;
    if (stereo) {
        r1::drainFirst(r1::poolMid.load());
        g_runEye = 1; u64 a = camA(); if (isCam(a)) hSlot43((void *)a);
        typedef void (*FnSnap)(void *); ((FnSnap)(g_base + RVA_RV_SNAPSHOT))(r1::V2);
        r1::eyeSwitches++;
        pub::noteRun2(frameCtr());
    }
    {   // pass-0 wrapper of the second render: left-eye capture (xr=3) and/or the same-frame pair meter
        const bool xr3cap = g_cur.valid && g_cur.stereo && stereo;
        if ((xr3cap || meter::pairWanted.load()) && r1::tabOk) {
            u64 *t = (u64 *)(g_base + RVA_PASSTAB); DWORD old;
            if (r1::tabRW || VirtualProtect(t, NPASS * 32, PAGE_READWRITE, &old)) { r1::tabRW = true; r1::capArm = xr3cap ? 1 : 0; t[0] = (u64)&r1::capPre; capSwap = true; }
        }
    }
    r1::inSecond = 1;
    r1::maskSecondRun(m0, m1, true);
    r1::oRVPost(r1::V2, ctx);
    r1::maskSecondRun(m0, m1, false);
    if (capSwap) { ((u64 *)(g_base + RVA_PASSTAB))[0] = r1::pristine[0][0]; r1::capArm = 0; }                               // second complete render graph, queued behind the first
    r1::inSecond = 0; r1Mark = 0;
    r1::afterSecondRun();
    r1::poolAfter = *(u32 *)(g_base + 0x1A0AB50 + 0x2B5C60);
    r1::posts++;
}
static std::atomic<int> m_census{0};
static u32 rvaOf(u64 p) { return p >= (u64)g_base && p < (u64)g_base + 0x4000000 ? (u32)(p - (u64)g_base) : 0; }
static void censusCam(char *b, size_t n, u64 cam) {
    if (!cam || !readable((void *)cam, 0x8D0)) { snprintf(b, n, "cam=%p(unreadable)", (void *)cam); return; }
    float *w = (float *)(cam + 0x140); u64 a = camA(); float d = -1, dot = 0;
    if (isCam(a)) { float *m = (float *)(a + 0x140); float dx = w[12] - m[12], dy = w[13] - m[13], dz = w[14] - m[14]; d = sqrtf(dx * dx + dy * dy + dz * dz); dot = w[0] * m[0] + w[1] * m[1] + w[2] * m[2]; }
    snprintf(b, n, "cam=%p vt=%x kind=%d tan=(%.3f,%.3f) near=%.2f f668=%.4g f66c=%.4g fov=%.3f pos=(%.0f,%.0f,%.0f) distToMain=%.0f fwdDot=%.3f %s",
             (void *)cam, rvaOf(*(u64 *)cam), *(int *)(cam + 0x660), *(float *)(cam + 0x680), *(float *)(cam + 0x684), *(float *)(cam + 0x664),
             *(float *)(cam + 0x668), *(float *)(cam + 0x66C), *(float *)(cam + 0x670), w[12], w[13], w[14], d, dot, cam == a ? "=CAM-A" : "");
}
static const char *modesOf(u64 view, char *b, size_t n) {
    int k = 0; b[0] = 0;
    for (int i = 0; i < g_subN && k < (int)n - 8; i++) if (g_sub[i].view == view) k += snprintf(b + k, n - k, "%u,", g_sub[i].mode);
    if (!k) snprintf(b, n, "none"); return b;
}
static void censusViews(u8 *f) {
    char cb[400], mb[64];
    int nv = rd<int>(f + 0x1EA98); u64 list = rd<u64>(f + 0x1EA90);
    logf("R1 census frame %u: mode80f09=%u views=%d submits=%d", frameCtr(), f[0x80F09], nv, g_subN);
    if (nv > 0 && nv < 64 && readable((void *)list, 8 * (size_t)nv))
        for (int i = 0; i < nv; i++) { u64 v = ((u64 *)list)[i]; censusCam(cb, sizeof cb, rd<u64>((void *)v)); logf("   view[%d] %p%s submit=%s %s", i, (void *)v, v == (u64)(f + 0x1ED70) ? "(main)" : "", modesOf(v, mb, sizeof mb), cb); }
    int ns = rd<int>(g_base + RVA_SECVIEW_COUNT);
    for (int i = 0; i < ns && i < 32; i++) {
        u64 e = rd<u64>(g_base + RVA_SECREG + 8 * i); if (!e || !readable((void *)e, 0xF0)) continue;
        u64 owner = rd<u64>((void *)(e + 0xE0)), view = rd<u64>((void *)(e + 0xE8));
        logf("   secReg[%d] entry=%p vt=%x owner=%p ownerVt=%x enabled=%u view=%p", i, (void *)e, rvaOf(rd<u64>((void *)e)), (void *)owner, owner ? rvaOf(rd<u64>((void *)owner)) : 0,
             owner && readable((void *)(owner + 0x19b4), 1) ? *(u8 *)(owner + 0x19b4) : 255, (void *)view);
    }
    int n2 = rd<int>(g_base + RVA_LIST2_COUNT);
    for (int i = 0; i < n2 && i < 32; i++) { u64 v = rd<u64>(g_base + RVA_LIST2 + 8 * i); censusCam(cb, sizeof cb, rd<u64>((void *)v)); logf("   list2[%d] %p submit=%s %s", i, (void *)v, modesOf(v, mb, sizeof mb), cb); }
    int nc = 0; for (u64 c = rd<u64>(f + 0x1EA08); c && nc < 16; c = rd<u64>((void *)(c + 0x6F0)), nc++) { censusCam(cb, sizeof cb, c); logf("   frameCam[%d] %s", nc, cb); }
}

static void hFrameView(void *frame) {
    c_frameview++; t_frameview = GetCurrentThreadId();
    // The view snapshot (RV_Snapshot) is taken inside FrameView. Re-run the render-camera update first so
    // the snapshot carries THIS frame's eye even if the engine's own slot-43 calls come later in the tick.
    u64 a = camA();
    if (frameCtr() == g_curFrame) c_slot43BeforeFV++; else c_slot43AfterFV++;
    if (g_cfg.xr == 1 || g_cfg.xr == 2 || g_cfg.xr == 3 || g_cfg.xr == 4) { g_runEye = 0; if (isCam(a)) hSlot43((void *)a); }
    oFrameView(frame);
    u8 *f = (u8 *)frame;
    // R1-A: per-view activation the engine performs only for the main view at the end of FrameView_5A07F0
    // (RV_9E5DE0: view+0x18 = 1, visibility parameters and jobs, renderer registration 0x787D50). Without it
    // RV_Kick skipped V2 and the frame deadlocked waiting for its graph (BUG-13).
    if (r1::thisFrame() && f == r1::frameObj()) {
        typedef void (*FnAct)(void *, void *);
        ((FnAct)(g_base + RVA_RV_ACTIVATE))(f + 0x1EAB0, r1::V2);
        r1::activated++;
    }
    {   // R1 census every 5 s (census=1) and on F10
        static u64 lastC = 0; u64 now = GetTickCount64();
        if ((g_cfg.census && now - lastC > 5000) || m_census.exchange(0)) { lastC = now; if (readable(f + 0x1EA08, 0x100) && readable(f + 0x80F09, 1)) censusViews(f); }
    }
    if (!readable(f + 0x1EA90, 0x20) || !readable(f + 0x80F09, 1)) return;
    ViewSample v{}; v.frame = (u64)f;
    v.views = *(int *)(f + 0x1EA98); u64 list = *(u64 *)(f + 0x1EA90);
    v.mainCam = rd<u64>(f + 0x1ED70); v.frameCam = rd<u64>(f + 0x1EA08); v.mode80f09 = f[0x80F09];
    v.secCount = rd<int>(g_base + RVA_SECVIEW_COUNT); v.list2Count = rd<int>(g_base + RVA_LIST2_COUNT);
    u64 cams[16]; int nc = 0;
    if (v.views > 0 && v.views < 0x200 && readable((void *)list, 8 * (size_t)v.views))
        for (int i = 0; i < v.views && nc < 16; i++) {
            u64 view = ((u64 *)list)[i]; u64 cam = rd<u64>((void *)view); bool dup = false;
            for (int k = 0; k < nc; k++) dup |= cams[k] == cam; if (!dup) cams[nc++] = cam;
        }
    v.distinctCams = nc;
    EnterCriticalSection(&g_viewcs); v.seq = g_view.seq + 1; g_view = v; LeaveCriticalSection(&g_viewcs);
}

// manual vtable call (hidden return pointer ABI) for ID3D12Resource::GetDesc (slot 10)
static bool inImage(const void *p) { MEMORY_BASIC_INFORMATION m; return p && VirtualQuery(p, &m, sizeof m) && m.State == MEM_COMMIT && m.Type == MEM_IMAGE; }
static bool resDesc(u64 r, D3D12_RESOURCE_DESC *out) {
    if (!r || !readable((void *)r, 8)) return false;
    u64 vt = *(u64 *)r;
    if (!inImage((void *)vt) || !readable((void *)(vt + 80), 8)) return false;
    if (!inImage(*(void **)(vt + 80))) return false;
    typedef D3D12_RESOURCE_DESC *(*GD)(void *, D3D12_RESOURCE_DESC *); GD f = *(GD *)(vt + 80); f((void *)r, out); return true;
}

// ---------------- D3D12 chain (ADDR-042) ----------------
static ID3D12Device *g_dev; static ID3D12CommandQueue *g_queue;
static bool resolveD3D() {
    u64 svc = rd<u64>(g_base + RVA_SERVICES); if (!svc || !readable((void *)svc, 0x120)) return false;
    u64 d3d = rd<u64>((void *)(svc + 0x50)); if (!d3d || rd<u64>((void *)d3d) != (u64)(g_base + RVA_VT_NXD3DIMPL)) return false;
    if (rd<u32>((void *)(d3d + 0x160)) != 3) return false;
    u64 drv = rd<u64>((void *)(d3d + 0x168)); if (!drv || rd<u64>((void *)drv) != (u64)(g_base + RVA_VT_NXD3D12DRV)) return false;
    u64 dev = rd<u64>((void *)(drv + 0x38)), qa = rd<u64>((void *)(drv + 0x30)); u64 q = qa ? rd<u64>((void *)qa) : 0;
    if (!dev || !q) return false;
    g_dev = (ID3D12Device *)dev; g_queue = (ID3D12CommandQueue *)q; return true;
}
static const char *modOf(const void *p, char *buf, size_t n) {   // which module a vtable lives in (proxy diagnostics)
    HMODULE m = nullptr; buf[0] = 0;
    if (p && GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)p, &m) && m) {
        char f[MAX_PATH]; GetModuleFileNameA(m, f, MAX_PATH); const char *b = strrchr(f, '\\'); snprintf(buf, n, "%s", b ? b + 1 : f);
    } else snprintf(buf, n, "?");
    return buf;
}
static const char *vtMod(void *obj, char *buf, size_t n) { return modOf(obj && readable(obj, 8) ? *(void **)obj : nullptr, buf, n); }

// ---------------- frame publisher: back buffer -> shared eye texture, on the game's present queue ----------------
// ---------------- back buffer -> engine resource object (shared by the stereo capture and the pair meter) ----------------
// BUG-21: the engine's per-buffer resource object is found by the NATIVE back-buffer pointer in the resource table
// [0x141487D38] (0x18-byte entries, +0 -> per-buffer objects, object+8 = ID3D12Resource*), cached. The back buffer
// of frame F is predicted from the last presented frame (flip model: +1 per Present) and verified at Present.
static std::atomic<u32> g_wrapFrame{0};   // frame id of the Present currently in flight (engine wrapper -> native Present)
namespace bbmap {
static ID3D12Resource *nat[8]; static u8 *obj[8]; static u32 count = 0; static std::atomic<u64> scans{0};
static std::atomic<u32> lastFrame{0}, lastIdx{0}; static std::atomic<int> have{0};
static u8 *findObj(ID3D12Resource *r) {
    u8 *tab = *(u8 **)(g_base + RVA_RES_TABLE); if (!tab) return nullptr;
    scans++; bool blockOk = false;
    for (u32 i = 0; i < 200000; i++) {
        u8 *e = tab + (u64)i * 0x18;
        if ((i & 255) == 0) blockOk = readable(e, 0x18 * 256);
        if (!blockOk && !readable(e, 0x18)) break;
        u8 *arr = *(u8 **)e; if (!arr || ((u64)arr & 7) || !readable(arr, 8 * 4)) continue;
        for (int k = 0; k < 4; k++) { u8 *o = ((u8 **)arr)[k]; if (o && !((u64)o & 7) && readable(o + 8, 8) && *(ID3D12Resource **)(o + 8) == r) return o; }
    }
    return nullptr;
}
static u64 lastTry = 0, fastUntil = 0;
static bool ensure(IDXGISwapChain3 *sc) {
    if (count) return true;
    if (!sc) return false;
    u64 now = GetTickCount64(); if (lastTry && now - lastTry < (now < fastUntil ? 250u : 10000u)) return false; lastTry = now;   // 0.8.2 log: rescanned every frame
    DXGI_SWAP_CHAIN_DESC1 d{}; if (FAILED(sc->GetDesc1(&d)) || d.BufferCount == 0 || d.BufferCount > 8) return false;
    u32 found = 0;
    for (u32 i = 0; i < d.BufferCount; i++) { ID3D12Resource *r = nullptr; if (FAILED(sc->GetBuffer(i, IID_PPV_ARGS(&r)))) return false; nat[i] = r; r->Release(); obj[i] = findObj(r); found += obj[i] != nullptr; }
    count = d.BufferCount;
    logf("bbmap: %u back buffers, engine resource objects found for %u (%p %p %p)", count, found, (void *)obj[0], (void *)obj[1], count > 2 ? (void *)obj[2] : nullptr);
    if (found != count) { count = 0; return false; }
    return true;
}
static void notePresent(u32 frame, u32 idx) { lastFrame = frame; lastIdx = idx; have = 1; }
// BUG-64b (0.16.1 crashed right after F2 applied a new size): nat[] / obj[] are the swapchain's back buffers and the engine's
// objects for them - ResizeBuffers frees both, and the next left-eye capture called GetDesc on a freed back buffer. Dropped
// before every ResizeBuffers (pub::hResize, under pub::cs); rescanned 4x per second for 5 s afterwards (the engine builds its
// objects for the new buffers right after the resize)
static void invalidate() { count = 0; have = 0; for (auto &x : nat) x = nullptr; for (auto &x : obj) x = nullptr; lastTry = 0; fastUntil = GetTickCount64() + 5000; }
static bool predict(u32 frame, u32 &idx) { if (!have.load() || !count) return false; idx = (lastIdx.load() + (frame - lastFrame.load())) % count; return true; }
// record into an engine pass context: back buffer of this frame -> dst (texture or readback footprint)
static bool copyInto(void *ctx, u32 idx, const D3D12_TEXTURE_COPY_LOCATION &dst) {
    u64 w = *(u64 *)ctx; if (!w || !readable((void *)w, 8)) return false;
    ID3D12GraphicsCommandList *gl = *(ID3D12GraphicsCommandList **)w; if (!gl || !readable(gl, 8)) return false;
    ((void (*)(void *, u8 *, int))(g_base + RVA_RES_TRANSITION_IMPL))(ctx, obj[idx], D3D12_RESOURCE_STATE_COPY_SOURCE);
    ((void (*)(void *))(g_base + RVA_CTX_FLUSHBARRIERS))(ctx);
    D3D12_TEXTURE_COPY_LOCATION src{nat[idx], D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, {}};
    gl->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    return true;
}
} // namespace bbmap
namespace meter { static void onNative(IDXGISwapChain *self); static void *nsc; static IDXGISwapChain3 *sc; }
namespace pub {
typedef int (*PFN_slGetNativeInterface)(void *proxy, void **base);
static bool tried = false, ready = false;
static IDXGISwapChain3 *sc;              // native swapchain
static ID3D12CommandQueue *q;            // the queue the swapchain presents on (native)
static ID3D12Device *dev;                // its device
static ID3D12Resource *tex[GV_TEXCOUNT]; static HANDLE texH[GV_TEXCOUNT]; static ID3D12Fence *fence; static HANDLE fenceH;
static ID3D12DescriptorHeap *rtvHeap; static UINT rtvInc = 0; static bool rtvOk = false;   // preset indicator (0.9.3)
static std::atomic<u64> pairsPub{0}, mirrored{0}, flatPub{0}; static u32 lastLeftTi = 0xFFFFFFFF; static u32 lastLeftFrame = 0;
static u64 fenceVal = 0; static u32 W, H; static DXGI_FORMAT F;
static ID3D12CommandAllocator *alloc[3]; static ID3D12GraphicsCommandList *cl; static u64 allocVal[3]; static int ai = 0;
static u32 nextBuf[2];
static std::atomic<u64> published{0}, mismatch{0}, skippedBusy{0}, noPkt{0};

static void *nscRaw;
static void hookNative();
static bool hooked = false;
static CRITICAL_SECTION cs;   // init / texture rebuild / copy are mutually exclusive (native Present may be another thread)
static void *native(PFN_slGetNativeInterface fn, void *p) { void *n = nullptr; if (fn && p && fn(p, &n) == 0 && n) return n; return p; }

static void releaseTextures() {
    if (fence && fenceVal) { HANDLE ev = CreateEventW(nullptr, FALSE, FALSE, nullptr); if (fence->GetCompletedValue() < fenceVal) { fence->SetEventOnCompletion(fenceVal, ev); WaitForSingleObject(ev, 500); } CloseHandle(ev); }
    for (int i = 0; i < GV_TEXCOUNT; i++) { if (tex[i]) tex[i]->Release(); tex[i] = nullptr; if (texH[i]) CloseHandle(texH[i]); texH[i] = nullptr; }
}
static bool makeTextures(u32 w, u32 h, DXGI_FORMAT f) {
    releaseTextures();
    D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC d{}; d.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; d.Width = w; d.Height = h; d.DepthOrArraySize = 1; d.MipLevels = 1;
    d.Format = f; d.SampleDesc.Count = 1; d.Flags = D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS; // no barriers: implicit promotion/decay
    // RT-capable (preset indicator squares drawn by ClearRenderTargetView) only for plain typed colour formats
    const bool typed = f == DXGI_FORMAT_R8G8B8A8_UNORM || f == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB || f == DXGI_FORMAT_B8G8R8A8_UNORM || f == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB || f == DXGI_FORMAT_R10G10B10A2_UNORM || f == DXGI_FORMAT_R16G16B16A16_FLOAT;
    rtvOk = false;
    if (typed) {
        if (!rtvHeap) { D3D12_DESCRIPTOR_HEAP_DESC hd{}; hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; hd.NumDescriptors = GV_TEXCOUNT; if (FAILED(dev->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&rtvHeap)))) rtvHeap = nullptr; rtvInc = dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV); }
        if (rtvHeap) { d.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET; rtvOk = true; }
    }
    wchar_t nm[96], sfx[24];
    for (int i = 0; i < GV_TEXCOUNT; i++) {
        HRESULT hr = dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_SHARED, &d, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&tex[i]));
        if (FAILED(hr) && rtvOk) {   // RT flag refused for this format: drop the indicator, keep the transport
            for (int k = 0; k < i; k++) { tex[k]->Release(); tex[k] = nullptr; if (texH[k]) CloseHandle(texH[k]); texH[k] = nullptr; }
            d.Flags = D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS; rtvOk = false; i = -1; logf("pub: render-target flag refused (0x%08lx) -> no preset indicator", (unsigned long)hr); continue;
        }
        if (FAILED(hr)) { logf("pub: shared texture create failed 0x%08lx", (unsigned long)hr); return false; }
        if (rtvOk) { D3D12_CPU_DESCRIPTOR_HANDLE hnd = rtvHeap->GetCPUDescriptorHandleForHeapStart(); hnd.ptr += (SIZE_T)i * rtvInc; dev->CreateRenderTargetView(tex[i], nullptr, hnd); }
        _snwprintf(sfx, 24, L"_g%ld_tex%d", (long)(S->texGen + 1), i); sfx[23] = 0; gvName(nm, 96, GetCurrentProcessId(), sfx);
        hr = dev->CreateSharedHandle(tex[i], nullptr, GENERIC_ALL, nm, &texH[i]);
        if (FAILED(hr)) { logf("pub: CreateSharedHandle(tex) failed 0x%08lx", (unsigned long)hr); return false; }
    }
    W = w; H = h; F = f;
    S->texW = w; S->texH = h; S->texFmt = (u32)f; S->gameLuid = dev->GetAdapterLuid();
    for (auto &eyeSlots : S->slot) for (auto &sl : eyeSlots) { gvWriteBegin(&sl.seq); sl.fence = 0; sl.gen = 0; gvWriteEnd(&sl.seq); }   // old-generation entries are never used
    InterlockedIncrement(&S->texGen);
    logf("pub: shared eye textures %ux%u format %d x%d, generation %ld", w, h, (int)f, GV_TEXCOUNT, S->texGen);
    return true;
}
static bool init(void *self) {
    tried = true;
    if (fence && fenceH && sc && q && cl) {   // core objects exist (retry after a texture rebuild failure): textures only
        ID3D12Resource *bb = nullptr; if (FAILED(sc->GetBuffer(0, IID_PPV_ARGS(&bb)))) return false;
        D3D12_RESOURCE_DESC d = bb->GetDesc(); bb->Release();
        if (!makeTextures((u32)d.Width, d.Height, d.Format)) return false;
        if (!hooked) { hooked = true; hookNative(); }
        ready = true; return true;
    }
    if (!g_dev && !resolveD3D()) { tried = false; return false; }   // retry next frame
    char m1[64], m2[64];
    HMODULE sl = GetModuleHandleW(L"sl.interposer.dll");
    PFN_slGetNativeInterface gni = sl ? (PFN_slGetNativeInterface)GetProcAddress(sl, "slGetNativeInterface") : nullptr;
    IDXGISwapChain *psc = (IDXGISwapChain *)rd<u64>((u8 *)self + 0x30);
    if (!psc) { tried = false; return false; }
    IDXGISwapChain *nsc = (IDXGISwapChain *)native(gni, psc); nscRaw = nsc;
    logf("pub: swapchain %p (vtable in %s) native %p (vtable in %s), slGetNativeInterface %s", (void *)psc, vtMod(psc, m1, 64), (void *)nsc, vtMod(nsc, m2, 64), gni ? "found" : "absent");
    if (FAILED(nsc->QueryInterface(IID_PPV_ARGS(&sc)))) { logf("pub: IDXGISwapChain3 unavailable"); return false; }
    // BUG-64e (0.16.3 F2: "fatal error making a swap chain"): DXGI allows one swapchain per window, and this reference kept the
    // game's old swapchain alive when it re-created it -> CreateSwapChain failed. The mod never owns the swapchain: the
    // reference is given back at once and SC_Recreate (hooked) drops the pointer before the game destroys it.
    sc->Release();
    HRESULT hr = sc->GetDevice(IID_PPV_ARGS(&q));
    if (FAILED(hr) || !q) {
        logf("pub: native swapchain GetDevice(ID3D12CommandQueue) failed 0x%08lx", (unsigned long)hr); q = nullptr;
        if (g_cfg.queueFallback) {
            q = (ID3D12CommandQueue *)native(gni, g_queue);
            logf("pub: FALLBACK to the nx-layer queue0 %p (native %p, vtable in %s): present queue NOT verified", (void *)g_queue, (void *)q, vtMod(q, m1, 64));
            q->AddRef();
        } else return false;
    } else logf("pub: present queue %p (vtable in %s); nx queue0 %p native %p", (void *)q, vtMod(q, m1, 64), (void *)g_queue, native(gni, g_queue));
    D3D12_COMMAND_QUEUE_DESC qd = q->GetDesc(); if (qd.Type != D3D12_COMMAND_LIST_TYPE_DIRECT) { logf("pub: present queue type %d not DIRECT", (int)qd.Type); return false; }
    if (FAILED(q->GetDevice(IID_PPV_ARGS(&dev))) || !dev) { logf("pub: queue GetDevice failed"); return false; }
    logf("pub: device %p (vtable in %s); nx device %p native %p", (void *)dev, vtMod(dev, m1, 64), (void *)g_dev, native(gni, g_dev));
    for (auto &a : alloc) if (FAILED(dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&a)))) return false;
    if (FAILED(dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc[0], nullptr, IID_PPV_ARGS(&cl)))) return false;
    cl->Close();
    wchar_t nm[96]; gvName(nm, 96, GetCurrentProcessId(), L"_fence");
    if (!fence && FAILED(dev->CreateFence(0, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&fence)))) { fence = nullptr; logf("pub: shared fence create failed"); return false; }
    if (!fenceH && FAILED(dev->CreateSharedHandle(fence, nullptr, GENERIC_ALL, nm, &fenceH))) { fenceH = nullptr; fence->Release(); fence = nullptr; logf("pub: shared fence name failed"); return false; }
    ID3D12Resource *bb = nullptr; if (FAILED(sc->GetBuffer(0, IID_PPV_ARGS(&bb)))) return false;
    D3D12_RESOURCE_DESC d = bb->GetDesc(); bb->Release();
    if (!makeTextures((u32)d.Width, d.Height, d.Format)) return false;
    if (!hooked) { hooked = true; hookNative(); }
    ready = true; return true;
}

// The engine Present wrapper queues GPU waits on other fences right before it calls DXGI Present, so the
// copy happens in a hook on the NATIVE swapchain's Present (after those waits are on the queue), keyed by
// the frame id the wrapper was given (review #6). Fallback (logged): copy at the wrapper if the native
// hook never fires.
static std::atomic<u32> pendFrame{0}; static std::atomic<int> pending{0};
static std::atomic<u64> nativeCalls{0}, nativeMatched{0}, wrapperCalls{0}; static std::atomic<int> earlyMode{0};
typedef HRESULT(STDMETHODCALLTYPE *FnPresent)(IDXGISwapChain *, UINT, UINT);
typedef HRESULT(STDMETHODCALLTYPE *FnPresent1)(IDXGISwapChain1 *, UINT, UINT, const DXGI_PRESENT_PARAMETERS *);
static FnPresent oNatPresent; static FnPresent1 oNatPresent1;

// native stereo: left image captured in the second render's first pass context (game thread, in-frame)
struct CapL { u32 frame, ti, b; bool ok; };
static CapL capL[32]; static u32 capIdx[32]; static u32 run2Frame[32]; static std::atomic<u64> idxMiss{0}; static std::atomic<u64> capOk{0}, capFail{0}, capMiss{0}, stereoPub{0}, monoFallback{0};
static void noteRun2(u32 f) { run2Frame[f & 31] = f; }
// BUG-21 (0.8.0): the swapchain wrapper's vt+0x118 is the DISPLAY SETTINGS block, not a render target -> every left
// capture failed. 0.8.1 finds the engine's own per-buffer resource object by the NATIVE back buffer pointer: the
// resource table [0x141487D38] (0x18-byte entries, +0 -> array of per-buffer objects, object+8 = ID3D12Resource*)
// is scanned once per back buffer and cached. The transition then goes straight to the tracking core
// 0x4FDED0(ctx, object, state) (what 0x4FDD50 calls). The back buffer of frame F is predicted from the last
// presented frame (flip model: +1 per Present) and VERIFIED at Present: a wrong guess drops that left image.
static void captureLeft(void *ctx) {
    if (!ready || !tex[0]) { capFail++; return; }
    EnterCriticalSection(&cs);
    do {
        if (!bbmap::ensure(sc)) { static int once = 0; if (!once++) logf("stereo: engine objects for the back buffers not found -> no left capture"); break; }
        u32 f = frameCtr(), idx = 0; if (!bbmap::predict(f, idx)) break;
        ID3D12Resource *bb = bbmap::nat[idx];
        D3D12_RESOURCE_DESC d = bb->GetDesc(); if (d.Width != W || d.Height != H || d.Format != F) { static int once = 0; if (!once++) logf("stereo: back buffer %llux%u fmt %d != eye textures -> no left capture", (unsigned long long)d.Width, d.Height, (int)d.Format); break; }
        u32 b = nextBuf[0] % 3, ti = b;
        D3D12_TEXTURE_COPY_LOCATION dst{tex[ti], D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, {}};
        if (!bbmap::copyInto(ctx, idx, dst)) break;   // eye texture COMMON -> promoted to COPY_DEST
        nextBuf[0]++;
        capL[f & 31] = {f, ti, b, true}; capIdx[f & 31] = idx; capOk++;
        { static int once = 0; if (!once++) logf("stereo: first left-eye capture: frame %u back buffer #%u (last presented frame %u)", f, idx, bbmap::lastFrame.load()); }
        LeaveCriticalSection(&cs); return;
    } while (0);
    capFail++; LeaveCriticalSection(&cs);
}
// 0.14.0 UI layer: our own render target for the UI (same desc as the engine's UI texture; must match the shared images)
static u32 uiNext = 0; static ID3D12DescriptorHeap *uiHeap = nullptr; static bool uiRefused = false;
static DXGI_FORMAT typeless(DXGI_FORMAT f) {
    switch (f) { case DXGI_FORMAT_R8G8B8A8_UNORM: case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: case DXGI_FORMAT_R8G8B8A8_TYPELESS: return DXGI_FORMAT_R8G8B8A8_TYPELESS;
                 case DXGI_FORMAT_B8G8R8A8_UNORM: case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: case DXGI_FORMAT_B8G8R8A8_TYPELESS: return DXGI_FORMAT_B8G8R8A8_TYPELESS; default: return f; }
}
// v8: the UI images have the UI target's own size (BUG-47: 0.14.0 required the eye-image size and refused the layer
// when the game letterboxed a 2560x1440 render into a 2560x1600 window)
static ID3D12Resource *uiSh[GV_UITEX]; static HANDLE uiShH[GV_UITEX];
static void ensureUiTex() {
    ID3D12Resource *src = ui::probeNat.load(); if (!src || uiRefused) return;
    static ID3D12Resource *made = nullptr; static D3D12_RESOURCE_DESC madeD{};
    D3D12_RESOURCE_DESC d = src->GetDesc();
    if (made == src && ui::shReady.load()) return;
    if (made && (madeD.Width != d.Width || madeD.Height != d.Height || madeD.Format != d.Format)) {   // new size/format: new images
        ui::shReady = 0; for (int i = 0; i < GV_UITEX; i++) { taa::bury(uiSh[i]); uiSh[i] = nullptr; ui::sh[i] = nullptr; if (uiShH[i]) CloseHandle(uiShH[i]); uiShH[i] = nullptr; }
        logf("UI layer: UI texture changed -> rebuilding the UI images");
    }
    if (d.SampleDesc.Count != 1 || d.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D) { uiRefused = true; logf("UI layer: unexpected UI texture (samples %u) -> panel mapping", d.SampleDesc.Count); return; }
    if (!uiHeap) { D3D12_DESCRIPTOR_HEAP_DESC hd{}; hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; hd.NumDescriptors = GV_UITEX; if (FAILED(dev->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&uiHeap)))) { uiHeap = nullptr; uiRefused = true; logf("UI layer: RTV heap failed"); return; } }
    const UINT inc = dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    if (!uiSh[0]) {
        D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_DEFAULT;
        D3D12_RESOURCE_DESC sd = d; sd.Alignment = 0; sd.MipLevels = 1; sd.DepthOrArraySize = 1; sd.Flags = D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        const LONG gen = S->uiGen + 1; wchar_t nm[96], sfx[24];
        for (int i = 0; i < GV_UITEX; i++) {
            HRESULT hr = dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_SHARED, &sd, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&uiSh[i]));
            if (SUCCEEDED(hr)) { _snwprintf(sfx, 24, L"_u%ld_ui%d", (long)gen, i); sfx[23] = 0; gvName(nm, 96, GetCurrentProcessId(), sfx); hr = dev->CreateSharedHandle(uiSh[i], nullptr, GENERIC_ALL, nm, &uiShH[i]); }
            if (FAILED(hr)) { logf("UI layer: shared UI image %d failed 0x%08lx -> panel mapping", i, (unsigned long)hr); uiRefused = true; return; }
        }
        for (auto &sl : S->ui) { gvWriteBegin(&sl.seq); sl.fence = 0; sl.gen = 0; gvWriteEnd(&sl.seq); }
        S->uiW = (u32)d.Width; S->uiH = d.Height; S->uiFmt = (u32)d.Format; InterlockedExchange(&S->uiGen, gen);
        logf("UI layer: %d shared UI images %llux%u fmt %d ready, generation %ld (the UI passes draw into them instead of the engine's picture %p)", GV_UITEX, (unsigned long long)d.Width, d.Height, (int)d.Format, gen, (void *)src);
    }
    for (int i = 0; i < GV_UITEX; i++) {   // our RTVs on them (the redirect target; the engine's format, non-typeless)
        D3D12_CPU_DESCRIPTOR_HANDLE h = uiHeap->GetCPUDescriptorHandleForHeapStart(); h.ptr += (SIZE_T)i * inc;
        D3D12_RENDER_TARGET_VIEW_DESC rv{}; rv.Format = d.Format; rv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        if (d.Format == DXGI_FORMAT_R8G8B8A8_TYPELESS) rv.Format = DXGI_FORMAT_R8G8B8A8_UNORM; else if (d.Format == DXGI_FORMAT_B8G8R8A8_TYPELESS) rv.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        dev->CreateRenderTargetView(uiSh[i], &rv, h); ui::shRtv[i] = h.ptr; ui::sh[i] = uiSh[i];
    }
    // 0.16.2 (BUG-64b): the ring is made once with room for 4096-pixel rows (or wider); a new render size only changes the row size
    // used in it, so a render thread that reads the old and the new values mid-change still stays inside the mapped buffers
    static u32 rbCapPitch = 0; const u32 needPitch = ((u32)d.Width * 4 + 255) & ~255u;
    if (ui::rbUi[0] && needPitch > rbCapPitch) {   // wider than the ring: rebuilt (rows of the old ring are no longer read)
        ui::rbPitch = 0; for (int i = 0; i < 8; i++) { ui::rbMap[i] = nullptr; ui::rbFrame[i] = 0xFFFFFFFFu; if (ui::rbUi[i]) { taa::bury(ui::rbUi[i]); ui::rbUi[i] = nullptr; } }
    } else if (ui::rbUi[0] && ui::rbPitch && (ui::rbW != (u32)d.Width || ui::rbH != d.Height)) {
        ui::rbPitch = 0; for (auto &fr : ui::rbFrame) fr = 0xFFFFFFFFu; ui::rbW = (u32)d.Width; ui::rbH = d.Height; ui::rbPitch = needPitch;
        logf("UI layer: cinematic detector rows now %ux%u", ui::rbW, ui::rbH);
    }
    if (!ui::rbUi[0]) {   // cinematic detector: 8 readback buffers of 13 UI rows each, persistently mapped
        const u32 pitch = needPitch, capPitch = needPitch > 4096u * 4u ? needPitch : 4096u * 4u;
        D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_READBACK; D3D12_RESOURCE_DESC bd{}; bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; bd.Width = (u64)capPitch * ui::NROW; rbCapPitch = capPitch; bd.Height = 1; bd.DepthOrArraySize = 1; bd.MipLevels = 1; bd.SampleDesc.Count = 1; bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        bool okr = d.Format == DXGI_FORMAT_R8G8B8A8_UNORM || d.Format == DXGI_FORMAT_B8G8R8A8_UNORM || d.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB || d.Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
        for (int i = 0; i < 8 && okr; i++) {
            if (FAILED(dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&ui::rbUi[i])))) { okr = false; break; }
            D3D12_RANGE rr{0, (SIZE_T)bd.Width}; void *m = nullptr; if (FAILED(ui::rbUi[i]->Map(0, &rr, &m)) || !m) { okr = false; break; } ui::rbMap[i] = (u8 *)m; ui::rbFrame[i] = 0xFFFFFFFFu;
        }
        if (okr) { ui::rbFmt = d.Format; ui::rbW = (u32)d.Width; ui::rbH = d.Height; ui::rbPitch = pitch; logf("UI layer: cinematic detector ready (%d rows of %ux%u, format %d)", ui::NROW, ui::rbW, ui::rbH, (int)d.Format); }
        else { for (auto &r : ui::rbUi) if (r) { r->Release(); r = nullptr; } for (auto &m : ui::rbMap) m = nullptr; logf("UI layer: cinematic detector unavailable (format %d) -> cinematic UI stays on the HUD panel", (int)d.Format); }
    }
    made = src; madeD = d; ui::shReady = 1;
}
// BUG-56 (0.14.5): does the game really letterbox its 16:9 picture inside the 16:10 window (black bars), or scale it to
// fill? 0.14.1-0.14.4 assumed bars and cropped them; if the game scales instead, the crop magnifies the world vertically
// (it "stretches" when you look up/down). Now measured: two 32x4 pixel boxes of the window image - the middle of the top
// bar and the screen centre - are read back while the centre shows something; bars = the top box stays black.
namespace lbx {
static ID3D12Resource *rb[3]; static bool pend[3], tried = false; static int verdict = -1; static u32 nBlack = 0, nLit = 0, barH = 0;
static void readK(int k) {
    if (!pend[k] || !rb[k]) return; pend[k] = false;
    u8 *m = nullptr; D3D12_RANGE rr{0, 4096}; if (FAILED(rb[k]->Map(0, &rr, (void **)&m)) || !m) return;
    auto mx = [&](u32 off) { int v = 0; for (int y = 0; y < 4; y++) for (int x = 0; x < 32; x++) { const u8 *p = m + off + y * 256 + x * 4; v = p[0] > v ? p[0] : v; v = p[1] > v ? p[1] : v; v = p[2] > v ? p[2] : v; } return v; };
    const int top = mx(0), mid = mx(2048); D3D12_RANGE wr{0, 0}; rb[k]->Unmap(0, &wr);
    if (verdict >= 0 || mid <= 24) return;
    if (top <= 3) nBlack++; else nLit++;
    if (nBlack + nLit >= 90) { verdict = nLit * 20 < nBlack ? 1 : 0; logf("pub: letterbox check: top band black in %u of %u lit frames -> %s", nBlack, nBlack + nLit, verdict ? "the game draws BLACK BARS: only the picture goes to the headset" : "no bars (the game scales its picture): whole image to the headset"); }
}
static void record(ID3D12GraphicsCommandList *c, ID3D12Resource *bb, int k, u32 W, u32 H, u32 bar, DXGI_FORMAT F) {
    if (verdict >= 0 || bar < 8 || !(F == DXGI_FORMAT_R8G8B8A8_UNORM || F == DXGI_FORMAT_B8G8R8A8_UNORM || F == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB || F == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)) return;
    if (!rb[k]) { if (tried && !rb[0]) return; tried = true;
        D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_READBACK; D3D12_RESOURCE_DESC bd{}; bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; bd.Width = 4096; bd.Height = 1; bd.DepthOrArraySize = 1; bd.MipLevels = 1; bd.SampleDesc.Count = 1; bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        for (auto &r : rb) if (FAILED(dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&r)))) { logf("pub: letterbox check unavailable"); return; } }
    D3D12_TEXTURE_COPY_LOCATION src{bb, D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, {}};
    const u32 ys[2] = {bar / 2 - 2, H / 2}, offs[2] = {0, 2048};
    for (int i = 0; i < 2; i++) {
        D3D12_TEXTURE_COPY_LOCATION dst{rb[k], D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT, {}}; dst.PlacedFootprint.Offset = offs[i];
        dst.PlacedFootprint.Footprint = {F, 32, 4, 1, 256};
        D3D12_BOX box{W / 2 - 16, ys[i], 0, W / 2 + 16, ys[i] + 4, 1}; c->CopyTextureRegion(&dst, 0, 0, 0, &src, &box);
    }
    pend[k] = true; barH = bar;
}
}
// 0.15.2 (BUG-57b): the THIN cinematic bars are in the 3D picture (the HUD image never shows them). The same 13 rows are
// read from the published left-eye image; symmetric black bands at both edges with picture between = cinematic bars ->
// the HUD goes over the whole view (no small panel). Enter after 3 frames, leave after 20.
namespace sbar {
static ID3D12Resource *rb[3]; static u8 *map[3]; static bool pend[3], tried = false; static u32 pitch = 0, w = 0; static bool ten = false; static int pos = 0, neg = 0;
static void readK(int k) {
    if (!pend[k] || !map[k]) return; pend[k] = false;
    const ui::Bands b = ui::bandsOf(map[k], pitch, w, false, ten, 12), bh = ui::bandsOf(map[k], pitch, w, false, ten, 24); ui::sReads++;
    ui::amax(ui::sMaxTop, b.top); ui::amax(ui::sMaxBot, b.bot);
    // 0.15.3 (BUG-57c): the in-engine cutscenes are letterboxed in the 3D picture; 0.15.2 let go after 20 frames whenever a dark
    // shot or a cut broke "symmetric bars with picture between" (thick 32:9 bars: on, off, on...). Now: ENTER on 3 strict hits;
    // while in a cutscene HOLD on any frame with both edges black (dark shots, cuts to black); LEAVE after 90 frames (~1.5 s)
    // without -> a cutscene detector (playbook ch13: cinematic classifier with hysteresis)
    if (ui::sbarRst.exchange(0)) { pos = 0; neg = 0; }   // 0.18.64 a load / respawn: start over
    const bool hit = ui::sceneHit(b), hold = bh.top >= 1 && bh.bot >= 1;   // 0.15.4: bars are not pure black (grain): enter <= 12/255, hold <= 24/255
    if (hit) ui::sHits++;
    if (!ui::sceneBars.load()) { if (hit) { if (++pos >= 3) { ui::sceneBars = 1; neg = 0; logf("UI: cutscene (letterbox bars in the 3D picture, bar rows top %d / bottom %d of 6) -> HUD over the whole view", b.top, b.bot); } } else pos = 0; }
    else if (hold) neg = 0;
    else if (++neg >= 180) { ui::sceneBars = 0; pos = 0; logf("UI: cutscene over (no bars for 3 s) -> HUD panel"); }
}
static void record(ID3D12GraphicsCommandList *c, ID3D12Resource *bb, int k, u32 W, u32 H, DXGI_FORMAT F) {
    const bool t10 = F == DXGI_FORMAT_R10G10B10A2_UNORM;
    if (!(t10 || F == DXGI_FORMAT_R8G8B8A8_UNORM || F == DXGI_FORMAT_B8G8R8A8_UNORM || F == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB || F == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)) return;
    if (rb[0] && w != W) { for (int i = 0; i < 3; i++) { pend[i] = false; map[i] = nullptr; if (rb[i]) { taa::bury(rb[i]); rb[i] = nullptr; } } tried = false; }   // 0.16.0: new size
    if (!rb[k]) { if (tried && !rb[0]) return; tried = true; pitch = (W * 4 + 255) & ~255u; w = W; ten = t10;
        D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_READBACK; D3D12_RESOURCE_DESC bd{}; bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; bd.Width = (u64)pitch * ui::NROW; bd.Height = 1; bd.DepthOrArraySize = 1; bd.MipLevels = 1; bd.SampleDesc.Count = 1; bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        for (int i = 0; i < 3; i++) { void *m = nullptr; D3D12_RANGE rr{0, (SIZE_T)bd.Width};
            if (FAILED(dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&rb[i]))) || FAILED(rb[i]->Map(0, &rr, &m)) || !m) { logf("pub: 3D-picture bar detector unavailable"); for (auto &x : rb) if (x) { x->Release(); x = nullptr; } return; }
            map[i] = (u8 *)m; }
        logf("pub: 3D-picture bar detector ready (%d rows of %ux%u, format %d)", ui::NROW, W, H, (int)F); }
    if (w != W) return;
    ui::rowsCopy(c, bb, rb[k], F, W, H, pitch); pend[k] = true;
}
}
// 0.18.65 (owner, session 20: the black square behind a picked-up item's picture): Ctrl+Shift+U saves this frame's UI image (the HUD layer
// the headset shows) next to gotvr.ini as ui_dump_color.bmp (what is drawn) and ui_dump_alpha.bmp (how opaque) - press it while the square
// is on screen, and the pixels say which draw leaves black with full alpha
namespace uidump {
static std::atomic<int> req{0}; static ID3D12Resource *rb = nullptr; static u64 val = 0; static D3D12_PLACED_SUBRESOURCE_FOOTPRINT fpt{}; static u32 w = 0, h = 0; static DXGI_FORMAT fmt{};
static void writeBmp(const wchar_t *name, const u8 *base, bool alpha) {
    wchar_t path[MAX_PATH]; swprintf(path, MAX_PATH, L"%ls\\%ls", g_dir, name); FILE *f = _wfopen(path, L"wb"); if (!f) return;
    const u32 row = w * 3, pad = (4 - row % 4) % 4, img = (row + pad) * h; u8 hd[54] = {'B', 'M'}; const u32 fsz = 54 + img;
    memcpy(hd + 2, &fsz, 4); u32 v = 54; memcpy(hd + 10, &v, 4); v = 40; memcpy(hd + 14, &v, 4); int32_t ww = (int32_t)w, hh = -(int32_t)h; memcpy(hd + 18, &ww, 4); memcpy(hd + 22, &hh, 4);
    uint16_t pl = 1, bpp = 24; memcpy(hd + 26, &pl, 2); memcpy(hd + 28, &bpp, 2); memcpy(hd + 34, &img, 4); fwrite(hd, 1, 54, f);
    std::vector<u8> line(row + pad, 0); const bool bgra = fmt == DXGI_FORMAT_B8G8R8A8_UNORM || fmt == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    for (u32 y = 0; y < h; y++) { const u8 *src = base + fpt.Offset + (u64)y * fpt.Footprint.RowPitch;
        for (u32 x = 0; x < w; x++) { const u8 *p = src + x * 4; u8 r = bgra ? p[2] : p[0], g = p[1], b = bgra ? p[0] : p[2]; if (alpha) r = g = b = p[3]; line[x * 3] = b; line[x * 3 + 1] = g; line[x * 3 + 2] = r; }
        fwrite(line.data(), 1, line.size(), f); }
    fclose(f); }
static void poll(ID3D12Fence *fence) {
    if (!rb || !val || fence->GetCompletedValue() < val) return;
    void *m = nullptr; D3D12_RANGE rr{0, (SIZE_T)(fpt.Offset + (u64)fpt.Footprint.RowPitch * h)};
    if (SUCCEEDED(rb->Map(0, &rr, &m))) { writeBmp(L"ui_dump_color.bmp", (const u8 *)m, false); writeBmp(L"ui_dump_alpha.bmp", (const u8 *)m, true); D3D12_RANGE wr{0, 0}; rb->Unmap(0, &wr);
        logf("UI dump: ui_dump_color.bmp + ui_dump_alpha.bmp written (%ux%u, format %d) next to gotvr.ini", w, h, (int)fmt); }
    rb->Release(); rb = nullptr; val = 0; }
static void record(ID3D12Device *dv, ID3D12GraphicsCommandList *c, ID3D12Resource *img) {
    if (!req.load() || rb || !img) return; req = 0; D3D12_RESOURCE_DESC d = img->GetDesc();
    if (!(d.Format == DXGI_FORMAT_R8G8B8A8_UNORM || d.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB || d.Format == DXGI_FORMAT_B8G8R8A8_UNORM || d.Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB || d.Format == DXGI_FORMAT_R8G8B8A8_TYPELESS)) { logf("UI dump: format %d not supported", (int)d.Format); return; }
    u32 rows = 0; u64 rs = 0, total = 0; dv->GetCopyableFootprints(&d, 0, 1, 0, &fpt, &rows, &rs, &total); w = (u32)d.Width; h = d.Height; fmt = d.Format;
    D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_READBACK; D3D12_RESOURCE_DESC bd{}; bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; bd.Width = total; bd.Height = 1; bd.DepthOrArraySize = 1; bd.MipLevels = 1; bd.SampleDesc.Count = 1; bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    if (FAILED(dv->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&rb)))) { rb = nullptr; logf("UI dump: no readback buffer"); return; }
    D3D12_TEXTURE_COPY_LOCATION dst{rb, D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT, {}}; dst.PlacedFootprint = fpt; D3D12_TEXTURE_COPY_LOCATION src{img, D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, {}};
    c->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr); val = ~0ull; }   // the fence value is set right after this frame's signal
}
static void copyFrame(u32 frame) {
    uidump::poll(fence);
    FramePkt pk{}; FramePkt &rr = g_ring[frame & 31];
    bool got = gvRead(&rr.seq, [&] { pk.frame = rr.frame; pk.eye = rr.eye; pk.phase = rr.phase; pk.valid = rr.valid; pk.flat = rr.flat; pk.pose = rr.pose; memcpy(pk.fov, rr.fov, 16); pk.stereo = rr.stereo; pk.pose2 = rr.pose2; memcpy(pk.fov2, rr.fov2, 16); });
    if (!got || pk.frame != frame) { mismatch++; S->gameMismatch++; return; }
    if (!pk.valid && !pk.flat) { noPkt++; return; }
    int k = (ai + 1) % 3;
    if (fence->GetCompletedValue() < allocVal[k]) { skippedBusy++; return; }   // never block the game
    ID3D12Resource *bb = nullptr; const u32 curIdx = sc->GetCurrentBackBufferIndex();
    if (FAILED(sc->GetBuffer(curIdx, IID_PPV_ARGS(&bb)))) return;
    D3D12_RESOURCE_DESC d = bb->GetDesc();
    if (d.Width != W || d.Height != H || d.Format != F) {
        bb->Release(); logf("pub: back buffer changed to %llux%u fmt %d -> new textures", (unsigned long long)d.Width, d.Height, (int)d.Format);
        if (!makeTextures((u32)d.Width, d.Height, d.Format)) { logf("pub: texture rebuild failed; retrying later"); ready = false; tried = false; }
        return;
    }
    const bool st = pk.stereo && pk.valid, r2 = st && run2Frame[frame & 31] == frame;   // r2: this frame's image is the RIGHT eye
    int e = st ? (r2 ? 1 : 0) : (pk.eye < 0 ? 0 : pk.eye);
    u32 b = nextBuf[e]++ % 3; u32 ti = (u32)e * 3 + b;
    ai = k; alloc[k]->Reset(); cl->Reset(alloc[k], nullptr);
    D3D12_TEXTURE_COPY_LOCATION dst{tex[ti], D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, {}}, src{bb, D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, {}};
    cl->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);   // bb is PRESENT(=COMMON): promoted to COPY_SOURCE, decays back
    lbx::readK(k);   // allocator k is free -> its previous readback is complete
    sbar::readK(k);
    if (g_cfg.xr == 4 && pk.valid && pk.eye == 0 && !(ui::flagsOf(frame) & GV_SLOT_THEATER) && ui::layerReady()) sbar::record(cl, bb, k, W, H, F);
    if (ID3D12Resource *un = ui::probeNat.load()) { D3D12_RESOURCE_DESC ud = un->GetDesc(); if (ud.Width == W && ud.Height < H) lbx::record(cl, bb, k, W, H, (H - ud.Height) / 2, F); }
    {   // preset indicator: N white squares (N = history preset) on a black strip for 3 s after a change, both eyes
        u64 sh = g_presetShownAt.load();
        if (rtvOk && g_cfg.xr == 4 && sh && GetTickCount64() - sh < 3000) {
            D3D12_RESOURCE_BARRIER br{}; br.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; br.Transition.pResource = tex[ti]; br.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            br.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST; br.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET; cl->ResourceBarrier(1, &br);
            D3D12_CPU_DESCRIPTOR_HANDLE hnd = rtvHeap->GetCPUDescriptorHandleForHeapStart(); hnd.ptr += (SIZE_T)ti * rtvInc;
            const int n = g_preset.load() + 1; const LONG sq = (LONG)(H / 36 > 8 ? H / 36 : 8), gap = sq / 2, tw = n * sq + (n - 1) * gap;
            const LONG x0 = (LONG)W / 2 - tw / 2, y0 = (LONG)H * 2 / 5;
            D3D12_RECT bg{x0 - gap, y0 - gap, x0 + tw + gap, y0 + sq + gap}; const float black[4] = {0, 0, 0, 1}, white[4] = {1, 1, 1, 1};
            cl->ClearRenderTargetView(hnd, black, 1, &bg);
            D3D12_RECT r[8]; for (int i = 0; i < n && i < 8; i++) r[i] = {x0 + i * (sq + gap), y0, x0 + i * (sq + gap) + sq, y0 + sq};
            cl->ClearRenderTargetView(hnd, white, n < 8 ? n : 8, r);
            br.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET; br.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON; cl->ResourceBarrier(1, &br);
        }
    }
    // monitor = left eye only (0.9.5): under xr=4 consecutive presents alternate eyes, so the window showed both
    // views flickering at the refresh rate ("two frames overlapped"). On a right-eye present the back buffer is
    // overwritten with this pair's left image after it has been captured for the headset.
    if (g_cfg.xr == 4 && g_cfg.mirrorLeft && !st && pk.phase == 1 && lastLeftTi < GV_TEXCOUNT && tex[lastLeftTi] && lastLeftFrame == frame - 1) {
        D3D12_RESOURCE_BARRIER br{}; br.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; br.Transition.pResource = bb; br.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        br.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE; br.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST; cl->ResourceBarrier(1, &br);
        cl->CopyResource(bb, tex[lastLeftTi]);
        br.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST; br.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT; cl->ResourceBarrier(1, &br);
        mirrored++;
    }
    if (g_cfg.xr == 4 && !st && pk.phase == 0 && pk.valid) { lastLeftTi = ti; lastLeftFrame = frame; }
    if (ui::probeNat.load() && !ui::probeLogged.exchange(1)) {   // UI target probe (0.13.0): separate texture, or the back buffer itself?
        ID3D12Resource *un = ui::probeNat.load(); D3D12_RESOURCE_DESC ud = un->GetDesc(); bool isBB = un == bb;
        for (u32 i = 0; i < bbmap::count && i < 8; i++) isBB |= bbmap::nat[i] == un;
        logf("UI probe: UI target colour = %p %llux%u format %d flags 0x%x (target flags 0x%x, %u colour attachments) -> %s", (void *)un, (unsigned long long)ud.Width, ud.Height, (int)ud.Format, (unsigned)ud.Flags,
             ui::probeFlags.load(), ui::probeCount.load(), isBB ? "the BACK BUFFER (UI drawn over the frame)" : "a SEPARATE texture (composited later)");
    }
    int uiK = -1;   // 0.14.2 UI layer: the UI image this frame's pass 65 copied (same queue, earlier -> covered by fence v)
    if (g_cfg.xr == 4 && ui::layerWant.load()) ensureUiTex();
    { ui::Pend &pp = ui::pend[frame & 31]; if (pp.frame == frame && pp.k >= 0) { uiK = pp.k; pp.k = -1; } }
    if (uiK >= 0 && uidump::req.load()) uidump::record(dev, cl, ui::sh[uiK].load());
    cl->Close(); ID3D12CommandList *l[] = {cl}; q->ExecuteCommandLists(1, l);
    u64 v = ++fenceVal; q->Signal(fence, v); allocVal[k] = v; if (uidump::val == ~0ull) uidump::val = v;
    if (uiK >= 0) { GvSlot &us = S->ui[uiK]; const u32 uf = ui::uiFlagsOf(frame);   // 0 = hidden this frame (the game's fade covers it)
        gvWriteBegin(&us.seq); us.tex = (u32)uiK; us.gen = (u32)S->uiGen; us.fence = v; us.frame = frame; us.flags = uf; gvWriteEnd(&us.seq); }
    {   // v8: where the picture is inside the eye image (BUG-47b: a 2560x1440 render letterboxed into a 2560x1600 window)
        int32_t c[4] = {0, 0, 0, 0};
        if (ID3D12Resource *un = ui::probeNat.load()) { D3D12_RESOURCE_DESC ud = un->GetDesc(); const u32 uw = (u32)ud.Width, uh = ud.Height;
            if (uw && uh && uw <= W && uh <= H && (uw != W || uh != H)) { c[0] = (int32_t)(W - uw) / 2; c[1] = (int32_t)(H - uh) / 2; c[2] = (int32_t)uw; c[3] = (int32_t)uh; } }
        if (!ui::cropOn.load() || lbx::verdict != 1) memset(c, 0, sizeof c);   // crop only when black bars were measured
        if (memcmp(c, S->content, sizeof c)) { memcpy(S->content, c, sizeof c); logf("pub: picture inside the %ux%u eye image: %s %d,%d %dx%d", W, H, c[2] ? "letterboxed" : "full", c[0], c[1], c[2] ? c[2] : (int)W, c[3] ? c[3] : (int)H); }
    }
    bb->Release();
    if (st && r2) {   // native stereo: right = this image, left = the in-frame capture of the same frame (covered by fence v: same queue, earlier)
        { GvSlot &sl = S->slot[1][b]; gvWriteBegin(&sl.seq); sl.tex = ti; sl.gen = (u32)S->texGen; sl.fence = v; sl.frame = frame; sl.pose = pk.pose2; memcpy(sl.fov, pk.fov2, 16); sl.flags = 0; gvWriteEnd(&sl.seq); }
        CapL &c = capL[frame & 31];
        if (c.ok && c.frame == frame && capIdx[frame & 31] != curIdx) { c.ok = false; idxMiss++; }
        if (c.ok && c.frame == frame) { GvSlot &sl = S->slot[0][c.b]; gvWriteBegin(&sl.seq); sl.tex = c.ti; sl.gen = (u32)S->texGen; sl.fence = v; sl.frame = frame; sl.pose = pk.pose; memcpy(sl.fov, pk.fov, 16); sl.flags = 0; gvWriteEnd(&sl.seq); c.ok = false; stereoPub++; }
        else capMiss++;
    } else for (int s2 = 0; s2 < 2; s2++) {
        if (!st && pk.valid && pk.eye >= 0 && s2 != pk.eye) continue;   // AFR: this eye only; mono and stereo-without-second-render (menus): both eyes show this image
        GvSlot &sl = S->slot[s2][b];
        // xr=4: both eyes of a pair carry the LEFT eye's frame id (right = left + 1, same simulation tick), so the host
        // presents only complete pairs (BUG-25: per-eye newest mixed pair k+1's left with pair k's right = AFR-like)
        const u32 tag = (g_cfg.xr == 4 && pk.valid && pk.phase == 1) ? frame - 1 : frame;   // 0.18.5: by pair phase (eye order can be swapped)
        // flat frame (0.13.0): one image for both slots, tagged the same frame, always on the theatre screen
        const u32 fl = pk.valid ? ui::flagsOf(frame) : (ui::flagsOf(frame) | GV_SLOT_THEATER | GV_SLOT_FLAT);
        gvWriteBegin(&sl.seq); sl.tex = ti; sl.gen = (u32)S->texGen; sl.fence = v; sl.frame = tag; sl.pose = pk.pose; memcpy(sl.fov, pk.fov, 16); sl.flags = fl; gvWriteEnd(&sl.seq);
        if (!pk.valid) flatPub++;
        if (g_cfg.xr == 4 && pk.phase == 1) pairsPub++;
        if (st && s2 == 0) monoFallback++;
    }
    S->gameBeat = (LONG64)GetTickCount64(); S->gamePublished++; published++;
}
// DXGI may implement Present by calling Present1 (or an overlay may chain them): only the OUTERMOST call
// on a thread counts, otherwise one frame looks like two (0.6 run: 1.99 per frame -> false FG refusal).
static thread_local int natDepth = 0;
static std::atomic<u64> nestedCalls{0};
static void atNative(IDXGISwapChain *self) {
    if (natDepth > 1) { nestedCalls++; return; }
    {   // which back buffer each frame was presented from (bbmap prediction + verification)
        IDXGISwapChain3 *s3 = ((void *)self == (void *)sc || (void *)self == nscRaw) && sc ? sc : ((void *)self == meter::nsc ? meter::sc : nullptr);
        if (s3) bbmap::notePresent(g_wrapFrame.load(), s3->GetCurrentBackBufferIndex());
    }
    meter::onNative(self);
    nativeCalls++;
    if ((void *)self != (void *)sc && (void *)self != (void *)nscRaw) return;
    nativeMatched++;
    if (pending.exchange(0)) { EnterCriticalSection(&cs); if (ready) copyFrame(pendFrame.load()); LeaveCriticalSection(&cs); }
}
// DXGI_PRESENT_TEST presents nothing (the engine wrapper calls Present(sync, TEST) first to check occlusion,
// decompile 0x434CC0 line 184; 0.6.1 counted it as a frame and copied too early -> black screen, BUG-9).
static std::atomic<u64> testCalls{0};
static HRESULT STDMETHODCALLTYPE hNatPresent(IDXGISwapChain *self, UINT a, UINT b) {
    if (b & DXGI_PRESENT_TEST) { testCalls++; return oNatPresent(self, a, b); }
    natDepth++; atNative(self); HRESULT r = oNatPresent(self, a, b); natDepth--; return r;
}
static HRESULT STDMETHODCALLTYPE hNatPresent1(IDXGISwapChain1 *self, UINT a, UINT b, const DXGI_PRESENT_PARAMETERS *p) {
    if (b & DXGI_PRESENT_TEST) { testCalls++; return oNatPresent1(self, a, b, p); }
    natDepth++; atNative((IDXGISwapChain *)self); HRESULT r = oNatPresent1(self, a, b, p); natDepth--; return r;
}
// BUG-64b: the native ResizeBuffers / ResizeBuffers1 (the engine's SC_Resize 0x435510 calls vt+0x68 on the swapchain):
// everything that points at the old back buffers or at the engine's old UI texture is dropped first, under cs, so no capture,
// copy or probe can touch a freed resource; the eye textures, UI images and readback rings follow the new size on their own
typedef HRESULT(STDMETHODCALLTYPE *FnResize)(IDXGISwapChain *, UINT, UINT, UINT, DXGI_FORMAT, UINT);
typedef HRESULT(STDMETHODCALLTYPE *FnResize1)(IDXGISwapChain3 *, UINT, UINT, UINT, DXGI_FORMAT, UINT, const UINT *, IUnknown *const *);
static FnResize oResize; static FnResize1 oResize1; static std::atomic<u64> resizes{0};
static bool oursSc(void *self) { return self && (self == (void *)sc || self == nscRaw || self == meter::nsc); }
static void beforeResize() {
    bbmap::invalidate(); pending = 0;
    ui::probeNat = nullptr; ui::probeRec = nullptr; ui::probeLogged = 0;
}
static HRESULT STDMETHODCALLTYPE hResize(IDXGISwapChain *self, UINT n, UINT w, UINT h, DXGI_FORMAT f, UINT fl) {
    if (!oursSc(self)) return oResize(self, n, w, h, f, fl);
    EnterCriticalSection(&cs); beforeResize(); const HRESULT r = oResize(self, n, w, h, f, fl); LeaveCriticalSection(&cs);
    resizes++; logf("pub: swapchain ResizeBuffers %ux%u (%u buffers) -> 0x%08lx; back-buffer map and UI probe dropped", w, h, n, (unsigned long)r);
    return r;
}
static HRESULT STDMETHODCALLTYPE hResize1(IDXGISwapChain3 *self, UINT n, UINT w, UINT h, DXGI_FORMAT f, UINT fl, const UINT *nm, IUnknown *const *qs) {
    if (!oursSc(self)) return oResize1(self, n, w, h, f, fl, nm, qs);
    EnterCriticalSection(&cs); beforeResize(); const HRESULT r = oResize1(self, n, w, h, f, fl, nm, qs); LeaveCriticalSection(&cs);
    resizes++; logf("pub: swapchain ResizeBuffers1 %ux%u (%u buffers) -> 0x%08lx; back-buffer map and UI probe dropped", w, h, n, (unsigned long)r);
    return r;
}
static void hookResize(void *swap) {
    static bool done = false; if (done || !swap) return; done = true;
    IDXGISwapChain3 *s3 = nullptr; void **vt = *(void ***)swap;
    MH_STATUS a = MH_CreateHook(vt[13], (void *)hResize, (void **)&oResize); if (a == MH_OK) a = MH_EnableHook(vt[13]);
    MH_STATUS b = MH_ERROR_NOT_INITIALIZED;
    if (SUCCEEDED(((IUnknown *)swap)->QueryInterface(IID_PPV_ARGS(&s3))) && s3) { void **v3 = *(void ***)s3; b = MH_CreateHook(v3[39], (void *)hResize1, (void **)&oResize1); if (b == MH_OK) b = MH_EnableHook(v3[39]); s3->Release(); }
    logf("pub: native ResizeBuffers hook %s, ResizeBuffers1 %s", MH_StatusToString(a), MH_StatusToString(b));
}
// BUG-64e: the game re-creates its swapchain (SC_Recreate 0x46E3D0 = SCImpl vt+0x20: destroy vt+0x28, create vt+0x18) for some
// display changes instead of resizing it. Everything that points at the old swapchain or its buffers is dropped first; the
// publisher initialises again on the new swapchain at the next Present (new eye textures, the host follows the generation).
static std::atomic<u64> recreates{0};
static void dropSwapchain(const char *why) {
    EnterCriticalSection(&cs);
    beforeResize(); ready = false; tried = false; sc = nullptr; nscRaw = nullptr;
    LeaveCriticalSection(&cs);
    logf("pub: %s -> swapchain pointers dropped; the publisher starts again on the new swapchain", why);
}
typedef char (*FnRecreate)(u8 *, u8 *); static FnRecreate oRecreate;
static char hRecreate(u8 *scImpl, u8 *settings) {
    dropSwapchain("the game re-creates its swapchain"); recreates++;
    const char r = oRecreate(scImpl, settings);
    logf("pub: swapchain re-created by the game (result %d)", (int)r);
    return r;
}
static void hookNative() {
    hookResize(sc);
    void **vt = *(void ***)sc;
    MH_STATUS a = MH_CreateHook(vt[8], (void *)hNatPresent, (void **)&oNatPresent);
    MH_STATUS b = MH_CreateHook(vt[22], (void *)hNatPresent1, (void **)&oNatPresent1);
    if (a == MH_OK) a = MH_EnableHook(vt[8]);
    if (b == MH_OK) b = MH_EnableHook(vt[22]);
    char m1[64]; logf("pub: native Present hook %s, Present1 %s (in %s)", MH_StatusToString(a), MH_StatusToString(b), modOf(vt[8], m1, 64));
    if (a != MH_OK && b != MH_OK) earlyMode = 1;
}

// Engine Present wrapper (submit thread): note which frame is being presented.
static void onPresent(void *self, void *desc) {
    if (!S || (g_cfg.xr != 1 && g_cfg.xr != 2 && g_cfg.xr != 3 && g_cfg.xr != 4)) return;
    if (S->gameRefuse) return;
    if (!tried) {
        static u64 lastTry = 0; u64 now = GetTickCount64();
        if (lastTry && now - lastTry < 2000) return;   // retry at most every 2 s
        lastTry = now;
        EnterCriticalSection(&cs); bool okInit = init(self); LeaveCriticalSection(&cs);
        if (!okInit) { if (tried) logf("pub: init failed -> headset shows black, game stays flat"); return; }
    }
    if (!ready) return;
    {   // BUG-64e fallback: the engine's swapchain is no longer the one the publisher was set up on -> start again
        void *psc = (void *)rd<u64>((u8 *)self + 0x30);
        static void *lastPsc = nullptr;
        if (psc && psc != nscRaw && psc != lastPsc) {   // checked once per distinct pointer (a proxy differs from the native one)
            static PFN_slGetNativeInterface gni = nullptr; static bool gniTried = false;
            if (!gniTried) { gniTried = true; HMODULE slm = GetModuleHandleW(L"sl.interposer.dll"); if (slm) gni = (PFN_slGetNativeInterface)GetProcAddress(slm, "slGetNativeInterface"); }
            if (native(gni, psc) != nscRaw) { lastPsc = nullptr; dropSwapchain("the engine presents on a different swapchain"); return; }
            lastPsc = psc;
        }
    }
    if (!desc || !readable(desc, 8)) return;
    u32 frame = *(u32 *)desc;   // engine frame id of the image being presented (Present Reflex markers use it)
    u64 w = ++wrapperCalls;
    if (w == 240) {
        u64 m = nativeMatched.load();
        logf("pub: native Present calls per game Present: %.2f (matched %llu, all %llu, nested ignored %llu, test-only ignored %llu, wrapper %llu)", m / 240.0, (unsigned long long)m, (unsigned long long)nativeCalls.load(), (unsigned long long)nestedCalls.load(), (unsigned long long)testCalls.load(), (unsigned long long)w);
        if (m > 240 * 3 / 2) { S->gameRefuse = 1; logf("pub: about 2 presents per game frame = FRAME GENERATION is on -> VR images refused. Turn Frame Generation off in the game settings."); return; }
        if (m == 0 && !earlyMode.load()) { earlyMode = 1; logf("pub: native Present hook never matched -> copying at the engine wrapper (may capture before the final GPU waits)"); }
    }
    if (earlyMode.load()) { EnterCriticalSection(&cs); if (ready) copyFrame(frame); LeaveCriticalSection(&cs); return; }
    pendFrame.store(frame);                   // frame first, then the flag (review: no stale frame id)
    if (pending.exchange(1)) skippedBusy++;   // previous one was never consumed
}
} // namespace pub

// ---------------- brightness meter (0.7.8, playbook A5 flat harness): measures the presented image ----------------
// At the native Present (outermost call, before the original runs, same place pub copies from) the back buffer is
// copied into a READBACK buffer on the nx queue0 (the present queue in practice, BUG-7 note) a few times per
// second; the hotkey thread maps it after the fence and averages a 1-in-8 x 1-in-8 pixel grid (R, G, B and luma,
// 0..255 in the stored encoding). Works with xr=0. Never blocks the game.
namespace meter {
static bool tried = false, ok = false; static ID3D12CommandQueue *q; static ID3D12Device *dev;
static ID3D12CommandAllocator *alloc; static ID3D12GraphicsCommandList *cl; static ID3D12Fence *fence; static u64 fv = 0;
static ID3D12Resource *rb, *rbL; static D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp; static u32 W, H; static DXGI_FORMAT F; static u64 rbSize = 0;
// 0.8.2 same-frame pairs: L = back buffer at the start of the second render (= the first render's finished image),
// copied inside the engine's pass-0 list; R = the same frame's back buffer at Present. 0 idle, 1 armed, 2 L recorded,
// 3 R recorded + fenced. Never blocks the game.
static std::atomic<int> pst{0}; static std::atomic<u32> pFrame{0}, pIdx{0}; static u64 pFv = 0;
struct Pair { double L[4], R[4], mad; u64 t; u32 frame; };
static Pair lastPair{}; static std::atomic<u64> nPairs{0}, pairDrops{0};
static std::atomic<int> st{0};   // 0 idle, 1 requested, 2 in flight
static std::atomic<int> wanted{0};
struct Sample { double L, R, G, B; u64 t; };
static Sample last{}; static std::atomic<u64> nSamples{0}; static CRITICAL_SECTION lcs;
static bool makeRb(const D3D12_RESOURCE_DESC &d) {
    if (rb) { rb->Release(); rb = nullptr; }
    UINT rows; UINT64 rowSize, total; dev->GetCopyableFootprints(&d, 0, 1, 0, &fp, &rows, &rowSize, &total);
    D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_READBACK;
    D3D12_RESOURCE_DESC bd{}; bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; bd.Width = total; bd.Height = 1; bd.DepthOrArraySize = 1; bd.MipLevels = 1; bd.SampleDesc.Count = 1; bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    if (FAILED(dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&rb)))) { rb = nullptr; return false; }
    if (rbL) { rbL->Release(); rbL = nullptr; }
    if (FAILED(dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&rbL)))) rbL = nullptr;
    W = (u32)d.Width; H = d.Height; F = d.Format; rbSize = total; return true;
}
// engine wrapper thread: resolve the native swapchain + queue once
static void init(void *wrapperSelf) {
    tried = true;
    if (!g_dev && !resolveD3D()) { tried = false; return; }
    HMODULE slm = GetModuleHandleW(L"sl.interposer.dll");
    pub::PFN_slGetNativeInterface gni = slm ? (pub::PFN_slGetNativeInterface)GetProcAddress(slm, "slGetNativeInterface") : nullptr;
    IDXGISwapChain *psc = (IDXGISwapChain *)rd<u64>((u8 *)wrapperSelf + 0x30); if (!psc) { tried = false; return; }
    IDXGISwapChain *n = (IDXGISwapChain *)pub::native(gni, psc); nsc = n;
    if (FAILED(n->QueryInterface(IID_PPV_ARGS(&sc)))) { logf("meter: IDXGISwapChain3 unavailable"); return; }
    q = (ID3D12CommandQueue *)pub::native(gni, g_queue); if (!q) return; q->AddRef();
    if (FAILED(q->GetDevice(IID_PPV_ARGS(&dev)))) return;
    if (FAILED(dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc)))) return;
    if (FAILED(dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc, nullptr, IID_PPV_ARGS(&cl)))) return; cl->Close();
    if (FAILED(dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)))) return;
    ID3D12Resource *bb = nullptr; if (FAILED(sc->GetBuffer(0, IID_PPV_ARGS(&bb)))) return;
    D3D12_RESOURCE_DESC d = bb->GetDesc(); bb->Release();
    if (!makeRb(d)) { logf("meter: readback buffer failed"); return; }
    if (!pub::hooked) {   // the native Present hooks (shared with the publisher)
        pub::hooked = true; pub::hookResize(n); void **vt = *(void ***)n;
        MH_STATUS a = MH_CreateHook(vt[8], (void *)pub::hNatPresent, (void **)&pub::oNatPresent), b = MH_CreateHook(vt[22], (void *)pub::hNatPresent1, (void **)&pub::oNatPresent1);
        if (a == MH_OK) a = MH_EnableHook(vt[8]);
        if (b == MH_OK) b = MH_EnableHook(vt[22]);
        logf("meter: native Present hook %s, Present1 %s", MH_StatusToString(a), MH_StatusToString(b));
    }
    ok = true; logf("meter: ready, back buffer %ux%u format %d, readback %llu bytes", W, H, (int)F, (unsigned long long)rbSize);
}
static void onWrapper(void *self) {
    if ((!wanted.load() && !pairWanted.load()) || tried) return;
    EnterCriticalSection(&pub::cs); init(self); LeaveCriticalSection(&pub::cs);
}
// native Present (outermost): record the copy if one was requested
// game thread, inside the second render's pass-0 context
static void captureL(void *ctx) {
    if (!ok || !rbL || pst.load() != 1) return;
    if (!bbmap::ensure(sc)) return;
    u32 f = frameCtr(), idx = 0; if (!bbmap::predict(f, idx)) return;
    D3D12_TEXTURE_COPY_LOCATION dst{}; dst.pResource = rbL; dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; dst.PlacedFootprint = fp;
    if (!bbmap::copyInto(ctx, idx, dst)) return;
    pFrame = f; pIdx = idx; pst = 2;
}
static void onNative(IDXGISwapChain *self) {
    if (!ok || (void *)self != nsc) return;
    if (pst.load() == 2) {   // pair: the right image of the same frame
        u32 wf = g_wrapFrame.load(), pf = pFrame.load();
        if (wf == pf) {
            if (sc->GetCurrentBackBufferIndex() != pIdx.load()) { pairDrops++; pst = 1; }
            else {
                ID3D12Resource *bb = nullptr;
                if (SUCCEEDED(sc->GetBuffer(sc->GetCurrentBackBufferIndex(), IID_PPV_ARGS(&bb)))) {
                    alloc->Reset(); cl->Reset(alloc, nullptr);
                    D3D12_TEXTURE_COPY_LOCATION dst{}, src{}; dst.pResource = rb; dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; dst.PlacedFootprint = fp;
                    src.pResource = bb; src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                    cl->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
                    cl->Close(); ID3D12CommandList *l[] = {cl}; q->ExecuteCommandLists(1, l); q->Signal(fence, ++fv); pFv = fv;
                    bb->Release(); pst = 3;
                }
            }
        } else if ((int)(wf - pf) > 0) { pairDrops++; pst = 1; }
        return;
    }
    if (st.load() != 1) return;
    ID3D12Resource *bb = nullptr; if (FAILED(sc->GetBuffer(sc->GetCurrentBackBufferIndex(), IID_PPV_ARGS(&bb)))) return;
    D3D12_RESOURCE_DESC d = bb->GetDesc();
    if (d.Width != W || d.Height != H || d.Format != F) { if (!makeRb(d)) { bb->Release(); ok = false; return; } }
    alloc->Reset(); cl->Reset(alloc, nullptr);
    D3D12_TEXTURE_COPY_LOCATION dst{}, src{}; dst.pResource = rb; dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; dst.PlacedFootprint = fp;
    src.pResource = bb; src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX; src.SubresourceIndex = 0;
    cl->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);   // PRESENT(=COMMON) promotes to COPY_SOURCE and decays back
    cl->Close(); ID3D12CommandList *l[] = {cl}; q->ExecuteCommandLists(1, l); q->Signal(fence, ++fv);
    bb->Release(); st = 2;
}
// hotkey thread: request / read back
static void stats(const u8 *base, double *o) {   // mean R, G, B, luma over the 1/64 grid
    double sr = 0, sg = 0, sb = 0; u64 n = 0;
    for (u32 y = 4; y < H; y += 8) {
        const u32 *row = (const u32 *)(base + (u64)y * fp.Footprint.RowPitch);
        for (u32 x = 4; x < W; x += 8) {
            u32 v = row[x]; double R, G, B;
            if (F == DXGI_FORMAT_R10G10B10A2_UNORM) { R = (v & 1023) / 4.011765; G = ((v >> 10) & 1023) / 4.011765; B = ((v >> 20) & 1023) / 4.011765; }
            else if (F == DXGI_FORMAT_B8G8R8A8_UNORM || F == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB) { B = v & 255; G = (v >> 8) & 255; R = (v >> 16) & 255; }
            else { R = v & 255; G = (v >> 8) & 255; B = (v >> 16) & 255; }
            sr += R; sg += G; sb += B; n++;
        }
    }
    if (!n) n = 1; o[0] = sr / n; o[1] = sg / n; o[2] = sb / n; o[3] = 0.2126 * o[0] + 0.7152 * o[1] + 0.0722 * o[2];
}
static double madOf(const u8 *a, const u8 *b) {   // mean |difference| per channel on the grid (0 = identical images)
    double s = 0; u64 n = 0;
    for (u32 y = 4; y < H; y += 8) {
        const u32 *ra = (const u32 *)(a + (u64)y * fp.Footprint.RowPitch), *rb_ = (const u32 *)(b + (u64)y * fp.Footprint.RowPitch);
        for (u32 x = 4; x < W; x += 8) { u32 p = ra[x], q2 = rb_[x]; for (int c = 0; c < 3; c++) { int d = (int)((p >> (c * 8)) & 255) - (int)((q2 >> (c * 8)) & 255); s += d < 0 ? -d : d; } n += 3; }
    }
    return n ? s / n : 0;
}
static CRITICAL_SECTION pcs;
static bool latestPair(Pair &o, u64 since) { EnterCriticalSection(&pcs); o = lastPair; LeaveCriticalSection(&pcs); return o.t && o.t >= since; }
static void poll(u64 now) {
    static u64 lastReq = 0;
    if (!ok) return;
    if (pairWanted.load()) {
        if (pst.load() == 3 && fence->GetCompletedValue() >= pFv) {
            void *pr = nullptr, *pl = nullptr; D3D12_RANGE r{0, (SIZE_T)rbSize}, w{0, 0};
            if (SUCCEEDED(rb->Map(0, &r, &pr)) && pr) {
                if (SUCCEEDED(rbL->Map(0, &r, &pl)) && pl) {
                    Pair p{}; stats((const u8 *)pl + fp.Offset, p.L); stats((const u8 *)pr + fp.Offset, p.R);
                    p.mad = madOf((const u8 *)pl + fp.Offset, (const u8 *)pr + fp.Offset); p.t = now; p.frame = pFrame.load();
                    EnterCriticalSection(&pcs); lastPair = p; LeaveCriticalSection(&pcs); nPairs++;
                    rbL->Unmap(0, &w);
                }
                rb->Unmap(0, &w);
            }
            pst = 1;
        } else if (pst.load() == 0) pst = 1;
        return;
    } else if (pst.load()) pst = 0;
    if (st.load() == 2 && fence->GetCompletedValue() >= fv) {
        void *p = nullptr; D3D12_RANGE r{0, (SIZE_T)rbSize};
        if (SUCCEEDED(rb->Map(0, &r, &p)) && p) {
            double sr = 0, sg = 0, sb = 0; u64 n = 0; const u8 *base = (const u8 *)p + fp.Offset;
            for (u32 y = 4; y < H; y += 8) {
                const u32 *row = (const u32 *)(base + (u64)y * fp.Footprint.RowPitch);
                for (u32 x = 4; x < W; x += 8) {
                    u32 v = row[x]; double R, G, B;
                    if (F == DXGI_FORMAT_R10G10B10A2_UNORM) { R = (v & 1023) / 4.011765; G = ((v >> 10) & 1023) / 4.011765; B = ((v >> 20) & 1023) / 4.011765; }
                    else if (F == DXGI_FORMAT_B8G8R8A8_UNORM || F == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB) { B = v & 255; G = (v >> 8) & 255; R = (v >> 16) & 255; }
                    else { R = v & 255; G = (v >> 8) & 255; B = (v >> 16) & 255; }
                    sr += R; sg += G; sb += B; n++;
                }
            }
            D3D12_RANGE w{0, 0}; rb->Unmap(0, &w);
            if (n) { EnterCriticalSection(&lcs); last = {0.2126 * sr / n + 0.7152 * sg / n + 0.0722 * sb / n, sr / n, sg / n, sb / n, now}; LeaveCriticalSection(&lcs); nSamples++; }
        }
        st = 0;
    }
    if (wanted.load() && st.load() == 0 && now - lastReq >= 100) { lastReq = now; st = 1; }
}
static bool latest(Sample &o, u64 since) { EnterCriticalSection(&lcs); o = last; LeaveCriticalSection(&lcs); return o.t >= since && o.t; }
} // namespace meter

// ---------------- F2: automatic bisection of the second run's passes by measured brightness (0.7.8) ----------------
// Pre-registered (playbook ch06 + A5.4): A = F5 off (control), B = F5 on (the dark result). Each state is held
// HOLD ms (exposure adapts over time) and measured over its last MEAS ms. A and B are measured twice (start and
// end of a phase) -> noise floor. Gate: |A-B| must exceed max(1.0, 3 x noise), else "no measurable effect".
// Score of a state = (L - B) / (A - B): ~1 = looks like F5 off (the skipped set contains the cause), ~0 = dark.
// Phase 1 skips one block of run-2 passes at a time (the rest of run 2 still runs; skipped passes leave run 1's
// results in the globally slotted targets). Phase 2 tests the single passes of every block that scored > 0.6.
// Result: the culprit passes stay skipped in run 2 (fixM) and are logged; a final A/B/fix check is measured.
namespace harness {
enum { HOLD = 5000, MEAS = 1500, RECOVER = 2500, MAXST = 48 };
struct St { char name[24]; int f5; u64 m0, m1; double L, R, G, B; int n; double fps; };
static St sts[MAXST]; static int nst = 0, cur = -1, phase = 0; static u64 t0 = 0, presents0 = 0, posts0 = 0;
static double acc[4]; static int accN = 0; static u64 lastSampleT = 0;
static std::atomic<int> running{0};
static void addRecover() { if (nst >= MAXST) return; St &x = sts[nst++]; snprintf(x.name, sizeof x.name, "recover"); x.f5 = 1; x.m0 = x.m1 = 0; x.n = 0; x.L = x.R = x.G = x.B = 0; x.fps = 0; }
static void add(const char *nm, int f5, u64 m0, u64 m1) { if (nst >= MAXST) return; St &x = sts[nst++]; snprintf(x.name, sizeof x.name, "%s", nm); x.f5 = f5; x.m0 = m0; x.m1 = m1; x.n = 0; x.L = x.R = x.G = x.B = 0; x.fps = 0; }
static void addRange(int a, int b) { u64 m0 = 0, m1 = 0; for (int i = a; i < b; i++) r1::setBit(m0, m1, i); char nm[24]; if (b - a == 1) snprintf(nm, sizeof nm, "skip %d", a); else snprintf(nm, sizeof nm, "skip %d-%d", a, b - 1); add(nm, 1, m0, m1); addRecover(); }
// 0.8.1: 14..25 measured without effect (0.7.8 log); 59.. (TAA, UI, final passes) would just show the first
// render's finished image (a false "fix": the right eye would get the left image), so they are not candidates.
static const int kBlocks[][2] = {{26, 29}, {29, 33}, {33, 37}, {37, 41}, {41, 45}, {45, 49}, {49, 53}, {53, 56}, {56, 59}};
static int phase1End = 0;
static void enter(int i, u64 now, u64 presents) {
    cur = i; t0 = now; presents0 = presents; posts0 = r1::posts.load(); accN = 0; acc[0] = acc[1] = acc[2] = acc[3] = 0;
    r1::on = sts[i].f5; r1::autoM0 = sts[i].m0; r1::autoM1 = sts[i].m1;
}
static void start(u64 now, u64 presents) {
    nst = 0; phase = 1;
    add("A (F5 off)", 0, 0, 0); add("B (F5 on)", 1, 0, 0);
    for (auto &b : kBlocks) addRange(b[0], b[1]);
    add("A (F5 off)", 0, 0, 0); add("B (F5 on)", 1, 0, 0);
    phase1End = nst;
    r1::fixM0 = 0; r1::fixM1 = 0; meter::wanted = 1; running = 1;
    logf("F2 AUTO TEST started: %d states x %d ms. Stand still in a quiet spot, don't touch the controls, keep the game in front.", nst, HOLD);
    enter(0, now, presents);
}
static bool scoreAB(int from, int to, double &A, double &B, double &noise) {
    double a[2], b[2]; int na = 0, nb = 0;
    for (int i = from; i < to; i++) { if (!strncmp(sts[i].name, "A (", 3) && na < 2) a[na++] = sts[i].L; if (!strncmp(sts[i].name, "B (", 3) && nb < 2) b[nb++] = sts[i].L; }
    if (na < 2 || nb < 2) return false;
    A = (a[0] + a[1]) / 2; B = (b[0] + b[1]) / 2; noise = fabs(a[0] - a[1]) + fabs(b[0] - b[1]);
    return true;
}
static void finish(const char *why) {
    running = 0; r1::autoM0 = 0; r1::autoM1 = 0; r1::on = 1; meter::wanted = 0;
    logf("F2 AUTO TEST ended: %s. F5 is ON; skipped run-2 passes now: %016llx %016llx", why, (unsigned long long)r1::fixM0.load(), (unsigned long long)r1::fixM1.load());
    logflush();
}
static u64 blk0 = 0, blk1 = 0;   // phase-1 culprit blocks (fallback if no single pass explains it)
static void applyFix(u64 c0, u64 c1, const char *how, u64 now, u64 presents) {
    r1::fixM0 = c0; r1::fixM1 = c1;
    char b[240]; int n = 0; for (int i = 14; i < NPASS; i++) if (r1::inMask(c0, c1, i)) n += snprintf(b + n, sizeof b - n, " %d", i);
    logf("F2 RESULT (%s): the darkening comes from run-2 pass(es)%s -> now skipped in run 2 (run 1 is the authority for them)", how, b);
    phase = 3; int first = nst; phase1End = first;
    add("A (F5 off)", 0, 0, 0); add("B (F5 on)", 1, 0, 0); add("fixed (F5 on)", 1, 0, 0); add("A (F5 off)", 0, 0, 0); add("B (F5 on)", 1, 0, 0); add("fixed (F5 on)", 1, 0, 0);
    enter(first, now, presents);
}
static void endPhase(u64 now, u64 presents) {
    double A, B, noise; int from = phase == 1 ? 0 : phase1End;
    if (!scoreAB(from, nst, A, B, noise)) { finish("missing controls"); return; }
    logf("F2 phase %d controls: A=%.2f B=%.2f (A-B=%.2f) noise=%.2f", phase, A, B, A - B, noise);
    if (phase == 3) {   // verification
        double F = 0; int nf = 0; for (int i = from; i < nst; i++) if (!strncmp(sts[i].name, "fixed", 5)) { F += sts[i].L; nf++; }
        F /= nf ? nf : 1;
        double sc = fabs(A - B) > 1e-6 ? (F - B) / (A - B) : 0;
        logf("F2 VERIFY: A=%.2f B=%.2f fixed=%.2f -> fixed score %.2f (1 = same as F5 off)%s", A, B, F, sc, sc > 0.6 ? "" : "  <- NOT confirmed, fix kept for inspection");
        finish("done"); return;
    }
    if (fabs(A - B) < 1.0 || fabs(A - B) < 3 * noise) {
        if (phase == 2 && (blk0 || blk1)) { applyFix(blk0, blk1, "phase-2 controls too noisy -> whole block", now, presents); return; }
        finish("no measurable brightness difference between F5 off and on here (try a darker / night spot, stand still)"); return;
    }
    u64 c0 = 0, c1 = 0; int hits = 0;
    for (int i = from; i < nst; i++) {
        if (sts[i].name[0] != 's') continue;
        double sc = (sts[i].L - B) / (A - B);
        logf("F2   %-12s score=%.2f (L=%.2f fps=%.0f)%s", sts[i].name, sc, sts[i].L, sts[i].fps, sc > 0.6 ? "  <- contains the cause" : "");
        if (sc > 0.6) { c0 |= sts[i].m0; c1 |= sts[i].m1; hits++; }
    }
    if (phase == 1) {
        if (!hits) { finish("no single block restores the brightness (the cause spans several blocks) -> nothing applied"); return; }
        blk0 = c0; blk1 = c1;
        phase = 2; int first = nst;
        add("A (F5 off)", 0, 0, 0); add("B (F5 on)", 1, 0, 0);
        for (int i = 14; i < NPASS; i++) if (r1::inMask(c0, c1, i)) addRange(i, i + 1);
        add("A (F5 off)", 0, 0, 0); add("B (F5 on)", 1, 0, 0);
        phase1End = first;
        { int ns = 0; for (int i = first; i < nst; i++) ns += sts[i].name[0] == 's'; logf("F2 phase 2: %d single passes", ns); }
        enter(first, now, presents); return;
    }
    // phase 2
    if (hits) applyFix(c0, c1, "single passes", now, presents);
    else applyFix(blk0, blk1, "no single pass alone -> whole block", now, presents);
}
static void tick(u64 now, u64 presents) {
    if (!running.load() || cur < 0) return;
    St &x = sts[cur];
    // fix: during B states in phase 3 the fix must be off, during "fixed" on
    if (phase == 3) { bool fx = !strncmp(x.name, "fixed", 5); static u64 keep0 = 0, keep1 = 0; if (r1::fixM0.load() || r1::fixM1.load()) { keep0 = r1::fixM0; keep1 = r1::fixM1; } if (!fx && x.f5) { r1::fixM0 = 0; r1::fixM1 = 0; } else if (fx) { r1::fixM0 = keep0; r1::fixM1 = keep1; } }
    meter::Sample sm;
    if (now - t0 >= HOLD - MEAS && meter::latest(sm, t0 + HOLD - MEAS) && sm.t != lastSampleT) { lastSampleT = sm.t; acc[0] += sm.L; acc[1] += sm.R; acc[2] += sm.G; acc[3] += sm.B; accN++; }
    if (!strcmp(x.name, "recover")) { if (now - t0 < RECOVER) return; if (cur + 1 < nst) enter(cur + 1, now, presents); else endPhase(now, presents); return; }
    { meter::Sample q; if (x.name[0] == 's' && meter::latest(q, t0 + 500) && (q.L > 120 || q.L < 2)) { logf("F2 state %-12s broke the picture (luma %.1f) -> ended early, not a candidate", x.name, q.L); x.n = 1; x.L = -1000; x.fps = 0; if (cur + 1 < nst) enter(cur + 1, now, presents); else endPhase(now, presents); return; } }
    if (now - t0 < HOLD) return;
    x.n = accN; x.L = accN ? acc[0] / accN : 0; x.R = accN ? acc[1] / accN : 0; x.G = accN ? acc[2] / accN : 0; x.B = accN ? acc[3] / accN : 0;
    x.fps = (presents - presents0) * 1000.0 / (now - t0);
    logf("F2 state %2d/%d %-14s luma=%6.2f rgb=(%6.2f %6.2f %6.2f) samples=%d fps=%.0f", cur + 1, nst, x.name, x.L, x.R, x.G, x.B, x.n, x.fps);
    if (!accN) { finish("the meter produced no samples (see 'meter:' lines)"); return; }
    if (x.f5 && (r1::posts.load() - posts0) * 2 < (presents - presents0)) { finish("the second render was not active (menu/cutscene/photo mode?) - run it during normal gameplay"); return; }
    if (cur + 1 < nst) { enter(cur + 1, now, presents); return; }
    endPhase(now, presents);
}
} // namespace harness

// ---------------- F2 (0.8.2): same-frame sweep of the second render's passes ----------------
// With the same camera the second render must reproduce the first render's image exactly; the difference of the
// same frame's two images (meter pairs: L at the start of the second render, R at Present) IS the artifact, immune
// to motion. Skipping a normal pass in the second render changes nothing (it leaves the first render's identical
// result in the globally slotted targets). Skipping the pass that adds the extra light makes the difference vanish;
// so do later passes that rebuild the final image from scratch -> the EARLIEST pass whose skip removes the
// difference is the cause. Result applied (that pass is taken from the first render = "one eye is the authority",
// playbook ch14) and saved in gotvr.ini (right_eye_reuse=...), so it is loaded at every start.
namespace sweep {
enum { BASE = 1500, STEP = 650, SETTLE = 250, FIRST = 14, LAST = 68 };
static std::atomic<int> running{0}; static int phase = 0, cur = 0; static u64 t0 = 0;
static double acc[3]; static int accN = 0; static u64 lastT = 0;
static double baseD[3]; static int baseN = 0; static double passD[NPASS][3]; static int passN[NPASS]; static double fixD[3]; static int fixN = 0;
static bool savedOn = false; static int prevSkipFrom = 0;
static void setMaskFor(int pass) { u64 m0 = 0, m1 = 0; if (pass >= 0) r1::setBit(m0, m1, pass); r1::autoM0 = m0; r1::autoM1 = m1; }
static void begin(u64 now) {
    if (!r1::tabOk) { logf("F2: pass table not verified -> unavailable"); return; }
    savedOn = r1::on.load() != 0; prevSkipFrom = r1::skipFrom.load(); r1::skipFrom = 0;
    r1::on = 1; meter::pairWanted = 1; running = 1; phase = 0; cur = FIRST; t0 = now; accN = 0; baseN = 0; fixN = 0;
    for (int i = 0; i < NPASS; i++) passN[i] = 0;
    r1::fixM0 = 0; r1::fixM1 = 0; setMaskFor(-1);
    logf("F2 SWEEP started: compares the two renders of the SAME frame, one pass at a time (~40 s). You can keep playing; avoid menus.");
}
static void end(const char *why) {
    running = 0; setMaskFor(-1); meter::pairWanted = 0; r1::skipFrom = prevSkipFrom; if (!savedOn && g_cfg.xr != 3) r1::on = 0;
    logf("F2 SWEEP ended: %s", why); logflush();
}
static void saveIni(int pass) {
    wchar_t ini[MAX_PATH]; swprintf(ini, MAX_PATH, L"%ls\\gotvr.ini", g_dir);
    wchar_t v[16]; swprintf(v, 16, L"%d", pass);
    WritePrivateProfileStringW(L"gotvr", L"right_eye_reuse", v, ini);
}
static double proj(const double *d) { double n = baseD[0] * baseD[0] + baseD[1] * baseD[1] + baseD[2] * baseD[2]; return n > 1e-9 ? (d[0] * baseD[0] + d[1] * baseD[1] + d[2] * baseD[2]) / n : 0; }
static void finish(u64 now) {
    for (int c = 0; c < 3; c++) baseD[c] /= baseN ? baseN : 1;
    double mag = sqrt(baseD[0] * baseD[0] + baseD[1] * baseD[1] + baseD[2] * baseD[2]);
    logf("F2 baseline (same frame, second render minus first): dR=%.2f dG=%.2f dB=%.2f |d|=%.2f (pairs %d)", baseD[0], baseD[1], baseD[2], mag, baseN);
    if (baseN < 3) { end("too few same-frame pairs (menus? loading?)"); return; }
    if (mag < 0.4) { end("the two renders are already identical here -> nothing to fix in this spot (try at night outdoors)"); return; }
    int culprit = -1; char line[1400]; int n = 0;
    for (int i = FIRST; i <= LAST; i++) {
        if (!passN[i]) continue;
        double d[3]; for (int c = 0; c < 3; c++) d[c] = passD[i][c] / passN[i];
        double sc = proj(d);
        n += snprintf(line + n, sizeof line - n, " %d:%.2f", i, sc); if (n > 1300) { logf("F2   remaining share per skipped pass:%s", line); n = 0; }
        if (culprit < 0 && sc < 0.25) culprit = i;
    }
    if (n) logf("F2   remaining share per skipped pass (1 = no change, 0 = difference gone):%s", line);
    if (culprit < 0) { end("no single pass explains the difference -> nothing applied"); return; }
    if (culprit >= 59) { end("only the final passes remove it (they rebuild the image) -> the cause is not a single earlier pass; nothing applied"); return; }
    u64 m0 = 0, m1 = 0; r1::setBit(m0, m1, culprit); r1::fixM0 = m0; r1::fixM1 = m1;
    logf("F2 RESULT: the second render's extra light comes from pass %d -> the second render now takes pass %d from the first render", culprit, culprit);
    phase = 2; t0 = now; accN = 0; fixN = 0; fixD[0] = fixD[1] = fixD[2] = 0;
    g_fixCulprit = culprit;
}
static void tick(u64 now) {
    if (!running.load()) return;
    meter::Pair p;
    bool fresh = meter::latestPair(p, t0 + SETTLE) && p.t != lastT;
    if (fresh) lastT = p.t;
    double d[3] = {p.R[0] - p.L[0], p.R[1] - p.L[1], p.R[2] - p.L[2]};
    if (phase == 0) {   // baseline
        if (fresh) { for (int c = 0; c < 3; c++) baseD[c] += d[c]; baseN++; }
        if (now - t0 >= BASE) { phase = 1; cur = FIRST; setMaskFor(cur); t0 = now; }
        return;
    }
    if (phase == 1) {
        if (fresh) { for (int c = 0; c < 3; c++) passD[cur][c] += d[c]; passN[cur]++; }
        if (now - t0 >= STEP) {
            if ((cur - FIRST) % 11 == 10) { setMaskFor(-1); phase = 3; t0 = now; return; }   // baseline refresh
            if (++cur > LAST) { setMaskFor(-1); finish(now); return; }
            setMaskFor(cur); t0 = now;
        }
        return;
    }
    if (phase == 3) {   // intermediate baseline, then continue
        if (fresh) { for (int c = 0; c < 3; c++) baseD[c] += d[c]; baseN++; }
        if (now - t0 >= BASE) { phase = 1; if (++cur > LAST) { finish(now); return; } setMaskFor(cur); t0 = now; }
        return;
    }
    if (phase == 2) {   // verify the fix
        if (fresh) { for (int c = 0; c < 3; c++) fixD[c] += d[c]; fixN++; }
        if (now - t0 >= 2000) {
            for (int c = 0; c < 3; c++) fixD[c] /= fixN ? fixN : 1;
            double sc = proj(fixD);
            logf("F2 VERIFY: with pass %d taken from the first render the remaining difference is %.0f%% of before (dR=%.2f dG=%.2f dB=%.2f, pairs %d)", g_fixCulprit, sc * 100, fixD[0], fixD[1], fixD[2], fixN);
            if (fixN < 3) { r1::fixM0 = 0; r1::fixM1 = 0; end("too few pairs to verify the fix -> removed (run F2 again during normal play)"); }
            else if (sc < 0.35) { saveIni(g_fixCulprit); end("fix confirmed and saved (gotvr.ini right_eye_reuse)"); }
            else { r1::fixM0 = 0; r1::fixM1 = 0; end("fix NOT confirmed -> removed"); }
        }
    }
}
} // namespace sweep

static std::atomic<u64> g_presents{0};
static u64 hPresent(void *self, void *desc) {
    g_presents++; c_present++;
    if (desc && readable(desc, 4)) g_wrapFrame = *(u32 *)desc; t_present = GetCurrentThreadId();
    if (desc && readable(desc, 4)) { g_presFrame = *(u32 *)desc; if (g_presEv) SetEvent(g_presEv); }
    static u64 lastRes = 0; u64 now = GetTickCount64();
    if (now - lastRes > 1000 && desc && readable(desc, 0x48)) { // EXP-06: sample the four tagged resources once per second
        lastRes = now; ResSample s{}; u8 *d = (u8 *)desc;
        for (int i = 0; i < 4; i++) { s.ptr[i] = *(u64 *)(d + 0x08 + 0x10 * i); s.state[i] = *(u32 *)(d + 0x10 + 0x10 * i); s.valid[i] = resDesc(s.ptr[i], &s.desc[i]); }
        EnterCriticalSection(&g_rescs); s.seq = g_res.seq + 1; g_res = s; LeaveCriticalSection(&g_rescs);
    }
    meter::onWrapper(self);
    if (g_cfg.xr) pub::onPresent(self, desc);
    return oPresent(self, desc);
}

// ---------------- host process ----------------
static bool startHost() {
    wchar_t nm[96]; gvName(nm, 96, GetCurrentProcessId(), nullptr);
    g_map = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(GvShared), nm);
    if (!g_map) { logf("host: shared block create failed %lu", GetLastError()); return false; }
    S = (GvShared *)MapViewOfFile(g_map, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(GvShared));
    if (!S) return false;
    memset(S, 0, sizeof *S); S->magic = GV_MAGIC; S->version = GV_VERSION; S->gamePid = GetCurrentProcessId(); S->mode = (u32)g_cfg.xr;
    S->theaterDist = g_ui.thDist; S->theaterWidth = g_ui.thWidth; S->theaterHeight = g_ui.thHeight; S->theaterBackdrop = (u32)g_ui.thBackdrop;
    S->hudDist = g_ui.hudDist; S->hudDeg = g_ui.hudDeg; S->hudDown = g_ui.hudDown; S->uiPremul = (u32)g_ui.premul;
    S->poseLeadCfgMs = g_poseLeadMs; S->hostGpuPrio = -1;
    if (g_cfg.xr == 4 && g_gameGpuPrio) {   // BUG-69 (0.17.3): a GPU-bound game starved the streamer's encoder and the host (headset frames 44-60/s while
        // the game kept its rate; black edges on fast turns). WDDM scheduling class BELOW_NORMAL (1) for the game process only.
        typedef LONG(APIENTRY * FnPrio)(HANDLE, int); HMODULE gd = GetModuleHandleW(L"gdi32.dll"); if (!gd) gd = LoadLibraryW(L"gdi32.dll");
        FnPrio f = gd ? (FnPrio)GetProcAddress(gd, "D3DKMTSetProcessSchedulingPriorityClass") : nullptr; const LONG st = f ? f(GetCurrentProcess(), 1) : -1;
        logf("game GPU scheduling priority: %s", st == 0 ? "BELOW NORMAL (the headset streamer and GoTVRHost go first)" : "unchanged (not available)");
    }
    wchar_t exe[MAX_PATH], cmd[MAX_PATH + 32]; swprintf(exe, MAX_PATH, L"%ls\\GoTVRHost.exe", g_dir);
    if (GetFileAttributesW(exe) == INVALID_FILE_ATTRIBUTES) {   // also accept it next to the game exe
        wchar_t alt[MAX_PATH]; wcscpy(alt, g_dir); if (wchar_t *sl = wcsrchr(alt, L'\\')) wcscpy(sl, L"\\GoTVRHost.exe");
        logf("host: %ls not found (err %lu); trying %ls", exe, GetLastError(), alt);
        if (GetFileAttributesW(alt) != INVALID_FILE_ATTRIBUTES) wcscpy(exe, alt);
    }
    swprintf(cmd, MAX_PATH + 32, L"\"%ls\" %lu", exe, GetCurrentProcessId());
    // 0.14.1: CreateProcessW took 27-35 s at game start in every log (the loader/AV is busy while the game boots), and
    // the hooks were only enabled after it returned -> the intro videos played before VR was live. Start it on its own
    // thread; the game side runs flat until the host is fresh.
    static wchar_t sExe[MAX_PATH], sCmd[MAX_PATH + 32]; wcscpy(sExe, exe); wcscpy(sCmd, cmd);
    HANDLE t = CreateThread(nullptr, 0, [](void *) -> DWORD {
        // 0.15.4: the PlayStation launcher runs before the game draws anything; with the OpenXR session already running the
        // headset was black the whole time. host_start=1 (default): the session starts at the game's first presented frame,
        // so until then Virtual Desktop / SteamVR shows its own home (the desktop with the launcher). 0 = at once (old).
        if (g_hostStart == 1) { const u64 w0 = GetTickCount64(); logf("host: waiting for the game's first frame before starting the headset session (launcher shows in Virtual Desktop / SteamVR home)");
            while (c_present.load() == 0) Sleep(100);
            logf("host: first game frame after %.1f s -> starting the headset session", (GetTickCount64() - w0) / 1000.0); }
        STARTUPINFOW si{}; si.cb = sizeof si; PROCESS_INFORMATION pi{}; const u64 t0 = GetTickCount64();
        if (!CreateProcessW(sExe, sCmd, nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, g_dir, &si, &pi)) { logf("host: cannot start %ls (err %lu) -> headset off, game flat", sExe, GetLastError()); return 1; }
        CloseHandle(pi.hThread); g_hostProc = pi.hProcess;
        logf("host: started GoTVRHost.exe pid %lu (mode %d, CreateProcess took %llu ms); its log is GoTVR\\host.log", pi.dwProcessId, g_cfg.xr, (unsigned long long)(GetTickCount64() - t0));
        return 0; }, nullptr, 0, nullptr);
    if (!t) { logf("host: thread failed %lu", GetLastError()); return false; }
    CloseHandle(t);
    return true;
}

// ---------------- worker ----------------
static HMODULE g_self;
static void report() {
    static u64 p_tick, p_present, p_fv, p_s43A, p_s43O, p_slA, p_slO, p_pr;
    u64 tk = c_tick, pr = c_present, fv = c_frameview, sA = c_slot43A, sO = c_slot43Other, lA = c_setlocalA, lO = c_setlocalOther, rA = c_projrectA;
    logf("census/s tick=%llu present=%llu frameview=%llu slot43(camA)=%llu slot43(other)=%llu setlocal(camA)=%llu setlocal(other)=%llu projrect(camA)=%llu | tid tick=%u present=%u frameview=%u slot43=%u projrect=%u | mut armed=%d leftHalf=%d yaw=%d",
         (unsigned long long)(tk - p_tick), (unsigned long long)(pr - p_present), (unsigned long long)(fv - p_fv), (unsigned long long)(sA - p_s43A), (unsigned long long)(sO - p_s43O),
         (unsigned long long)(lA - p_slA), (unsigned long long)(lO - p_slO), (unsigned long long)(rA - p_pr), t_tick.load(), t_present.load(), t_frameview.load(), t_slot43.load(), t_projrect.load(),
         m_armed.load(), m_leftHalf.load(), m_yaw.load());
    p_tick = tk; p_present = pr; p_fv = fv; p_s43A = sA; p_s43O = sO; p_slA = lA; p_slO = lO; p_pr = rA;
    if (g_rectSeq.load()) logf("   projrect(camA) last=(%.6f,%.6f,%.6f,%.6f)", g_lastRect[0], g_lastRect[1], g_lastRect[2], g_lastRect[3]);
    ViewSample v; EnterCriticalSection(&g_viewcs); v = g_view; LeaveCriticalSection(&g_viewcs);
    if (v.seq) logf("   views: frame=%p list=%d distinctCams=%d mainView.cam=%p frame.cam=%p camA=%p mode80f09=%u secRegistry=%d list2=%d",
                    (void *)v.frame, v.views, v.distinctCams, (void *)v.mainCam, (void *)v.frameCam, (void *)camA(), v.mode80f09, v.secCount, v.list2Count);
    ResSample r; EnterCriticalSection(&g_rescs); r = g_res; LeaveCriticalSection(&g_rescs);
    if (r.seq) {
        static const char *nm[4] = {"motionvec+08", "depth+18", "hudless+28", "ui+38"};
        char b[1024]; int n = 0;
        for (int i = 0; i < 4; i++) n += snprintf(b + n, sizeof b - n, " %s=%p", nm[i], (void *)r.ptr[i]) , n += r.valid[i] ? snprintf(b + n, sizeof b - n, "(%llux%u fmt%d st0x%x)", (unsigned long long)r.desc[i].Width, r.desc[i].Height, (int)r.desc[i].Format, r.state[i]) : snprintf(b + n, sizeof b - n, "(-)");
        logf("   present desc:%s", b);
    }
}
static void reportCallers() {
    char b[1500]; int n = 0; b[0] = 0;
    for (auto &c : g_callers) { u64 rva = c.rva.load(); if (!rva) break; n += snprintf(b + n, sizeof b - n, " %llx:%llu/%llu", (unsigned long long)rva, (unsigned long long)c.n.load(), (unsigned long long)c.nA.load()); if (n > 1400) break; }
    logf("setlocal callers (retRVA:total/camA):%s", n ? b : " (none)");
}

// 0.18.11 (owner: "an option to change the rotation of the hands manually using F1, 2 and 3, and then you save it as default"):
// F1 / F2 / F3 turn both hands about pitch / yaw / roll by 5 degrees (Shift = the other way); every press is written to gotvr.ini
// (fp_hand_pitch_deg / _yaw_deg / _roll_deg), which is read at start, so the last setting is the default from then on.
// The old F1 / F2 / F3 functions (HUD layer A/B, render size, letterbox crop) moved to Ctrl+F1 / Ctrl+F2 / Ctrl+F3 (0.18.12: Ctrl+Alt+F1 / F2 / F3; Ctrl+F1..F3 now move the wrist point).
// 0.18.15's Shift+F7 TAA A/B (preset TAA / per-eye TAA / no history) made no difference to the arm overlay (owner) - removed in 0.18.16.
static void handTrim(int ax, float d) {   // ax 0..2 = pitch / yaw / roll (deg, F1..F3); 3..5 = wrist back / up / toward the thumb (cm, Ctrl+F1..F3; 0.18.12)
    float *const vs[6] = {&g_fp.handPitch, &g_fp.handYaw, &g_fp.handRoll, &g_fp.handBackCm, &g_fp.handUpCm, &g_fp.handSideCm};
    float &v = *vs[ax]; float nv = v + d;
    if (ax < 3) { while (nv > 180.0f) nv -= 360.0f; while (nv <= -180.0f) nv += 360.0f; } else nv = fminf(ax == 3 ? 20.0f : 10.0f, fmaxf(-10.0f, nv));
    if (fabsf(nv) < 1e-3f) nv = 0.0f;
    v = nv;
    static const wchar_t *key[6] = {L"fp_hand_pitch_deg", L"fp_hand_yaw_deg", L"fp_hand_roll_deg", L"fp_hand_back_cm", L"fp_hand_up_cm", L"fp_hand_side_cm"};
    static const char *nm[6] = {"pitch", "yaw", "roll", "wrist back", "wrist up (toward the back of the hand)", "wrist toward the thumb"};
    wchar_t ini[MAX_PATH]; swprintf(ini, MAX_PATH, L"%ls\\gotvr.ini", g_dir); wchar_t b[32]; swprintf(b, 32, L"%.1f", nv);
    const BOOL ok = WritePrivateProfileStringW(L"gotvr", key[ax], b, ini);
    logf("HOTKEY %sF%d hand %s %+.0f -> %.1f %s (angles %.1f / %.1f / %.1f deg, wrist %.1f / %.1f / %.1f cm) %s", ax < 3 ? "" : "Ctrl+", ax % 3 + 1, nm[ax], d, nv, ax < 3 ? "deg" : "cm",
         g_fp.handPitch, g_fp.handYaw, g_fp.handRoll, g_fp.handBackCm, g_fp.handUpCm, g_fp.handSideCm, ok ? "saved to gotvr.ini as the default" : "NOT saved (gotvr.ini not writable)");
}
static DWORD WINAPI worker(void *) {
    wchar_t exe[MAX_PATH]; DWORD n = GetModuleFileNameW(nullptr, exe, MAX_PATH);
    wcscpy(g_dir, exe); wchar_t *sl = wcsrchr(g_dir, L'\\'); if (sl) wcscpy(sl, L"\\GoTVR"); CreateDirectoryW(g_dir, nullptr);
    wchar_t lp[MAX_PATH], pp[MAX_PATH]; swprintf(lp, MAX_PATH, L"%ls\\gotvr.log", g_dir); swprintf(pp, MAX_PATH, L"%ls\\gotvr.prev.log", g_dir);
    MoveFileExW(lp, pp, MOVEFILE_REPLACE_EXISTING);
    g_log = CreateFileW(lp, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, 0, nullptr);
    loadConfig();
    logf("IDENTITY %s built %s %s", GOTVR_VERSION, __DATE__, __TIME__);
    logf("pid=%lu exe=%ls config: xr=%d census=%d hotkeys=%d", GetCurrentProcessId(), exe, g_cfg.xr, g_cfg.census, g_cfg.hotkeys); (void)n;
    g_base = (u8 *)GetModuleHandleW(nullptr);
    u32 e = *(u32 *)(g_base + 0x3C), ts = *(u32 *)(g_base + e + 8);
    char sha[65]; sha256File(exe, sha); bool shaok = strcmp(sha, EXPECTED_SHA) == 0;
    int pass = 0, total = (int)(sizeof kSigs / sizeof kSigs[0]);
    for (auto &s : kSigs) { bool r = checkSig(s); pass += r; logf("sig %s rva=0x%x %s", s.name, s.rva, r ? "PASS" : "FAIL"); }
    logf("base=%p pe_timestamp=%u sha256=%s %s; sigs %d/%d", g_base, ts, sha, shaok ? "MATCH" : "MISMATCH", pass, total);
    if (!(shaok && ts == EXPECTED_TIMESTAMP && pass == total)) { logf("UNSUPPORTED BUILD -> no hooks, XInput passthrough only"); logflush(); return 0; }
    {   // frame-id guard: GameTick's first instruction is `inc dword [rip+X]` on the Reflex frame counter
        int32_t disp = *(int32_t *)(g_base + RVA_GAMETICK + 15); u32 tgt = RVA_GAMETICK + 19 + (u32)disp;
        logf("frame counter: GameTick increments rva 0x%x (expected 0x%x) %s", tgt, (u32)RVA_FRAMECTR, tgt == RVA_FRAMECTR ? "PASS" : "FAIL");
        if (tgt != RVA_FRAMECTR && g_cfg.xr) { logf("frame counter guard failed -> xr disabled"); g_cfg.xr = 0; }
    }
    g_presEv = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    logf("render sync (xr=4): %s", g_renderSync == 1 ? "the right-eye tick waits for the left-eye frame to be presented" : g_renderSync == 2 ? "every stereo tick waits for the previous frame to be presented" : "off");
    if (MH_Initialize() != MH_OK) { logf("MinHook init failed"); return 0; }
    struct H { u32 rva; void *det; void **orig; const char *name; } hooks[] = {
        {RVA_GAMETICK, (void *)hGameTick, (void **)&oGameTick, "GameTick"}, {RVA_PRESENT, (void *)hPresent, (void **)&oPresent, "Present"},
        {RVA_SETLOCAL, (void *)hSetLocal, (void **)&oSetLocal, "SetLocalMatrix"}, {RVA_SLOT43, (void *)hSlot43, (void **)&oSlot43, "vUpdateFrustum43"},
        {RVA_SETPROJRECT, (void *)hSetProjRect, (void **)&oSetProjRect, "SetProjRect"}, {RVA_FRAMEVIEW, (void *)hFrameView, (void **)&oFrameView, "FrameView"},
        {RVA_RVSUBMIT, (void *)hRVSubmit, (void **)&oRVSubmit, "RV_Submit"},
        {RVA_FV_LIST, (void *)hFvList, (void **)&r1::oFvList, "FrameView_ViewList"}, {RVA_RV_POST, (void *)hRVPost, (void **)&r1::oRVPost, "RV_Post"},
        {RVA_SUBMITGRAPH, (void *)hSubmitGraph, (void **)&oSubmitGraph, "Jobs_SubmitGraph"},
        {RVA_VC_END, (void *)r1::hVcEnd, (void **)&r1::oVcEnd, "VC_FrameEnd"}, {RVA_PASSBODY, (void *)r1::hPassBody, (void **)&r1::oPassBody, "Pass_Body"}, {RVA_CLOCK_UPDATE, (void *)hClock, (void **)&oClock, "Clock_Update"}, {RVA_CAM_TEMPORAL, (void *)hCamT, (void **)&oCamT, "Cam_TemporalUpdate"}, {RVA_CAMS_STOREPREV, (void *)occ::hStorePrev, (void **)&occ::oSP, "Cam_StorePrev"}, {RVA_RV_OCC, (void *)occ::hOcc, (void **)&occ::oOcc, "RV_OcclusionSetup"}, {RVA_AA_RESOLVE, (void *)taa::hRes, (void **)&taa::oRes, "AA_Resolve"}, {RVA_SL_CONSTS, (void *)dlss::hSLC, (void **)&dlss::oSLC, "Cam_BuildSLConstants"}, {RVA_NGX_CREATE, (void *)dlss::hNgxCreate, (void **)&dlss::oNgxCreate, "NGX_CreateFeature"}, {RVA_NGX_EVAL, (void *)dlss::hNgxEval, (void **)&dlss::oNgxEval, "NGX_EvaluateFeature_C"}, {RVA_NGX_RELEASE, (void *)dlss::hNgxRelease, (void **)&dlss::oNgxRelease, "NGX_ReleaseFeature"}, {RVA_AA_VELOCITY, (void *)taa::hVel, (void **)&taa::oVel, "AA_Velocity"}, {RVA_TAA_DRAW, (void *)taa::hDraw, (void **)&taa::oDraw, "TAA_Draw"}, {RVA_RCTX_BUILD, (void *)r1::hRCtxBuild, (void **)&r1::oRCtxBuild, "RCtx_Build"}, {RVA_RCTX_FLUSH, (void *)r1::hRCtxFlush, (void **)&r1::oRCtxFlush, "RCtx_FlushDraws"}, {RVA_SLPASS, (void *)hSLPass, (void **)&oSLPass, "StreamlinePass"},
        {RVA_RT_BIND, (void *)ui::hRT, (void **)&ui::oRT, "RT_Bind"}, {RVA_CTX_SCISSOR, (void *)ui::hSC, (void **)&ui::oSC, "Ctx_SetScissor"}, {RVA_MOVIE_DRAW, (void *)ui::hMovie, (void **)&ui::oMovie, "Movie_Draw"}, {RVA_PASS65_PRE, (void *)ui::hP65, (void **)&ui::oP65, "Pass65_pre (fade)"}, {RVA_BATCH_DRAW, (void *)ui::hBatch, (void **)&ui::oBatch, "Batch_Draw"},
        {RVA_UI60_PRE, (void *)ui::hUI60, (void **)&ui::oUI60, "UI_Pass60_pre (clears the UI image)"}, {RVA_CTX_FLUSHRT, (void *)ui::hFlushRT, (void **)&ui::oFlushRT, "Ctx_FlushRT (UI layer redirect)"},
        {RVA_SC_COMPUTESIZES, (void *)rsz::hCS, (void **)&rsz::oCS, "SC_ComputeSizes (headset render size)"},
        {RVA_WND_PUMP, (void *)rsz::hPump, (void **)&rsz::oPump, "GameWindow_Pump (F2 size change on the main thread)"},
        {RVA_SC_RECREATE, (void *)pub::hRecreate, (void **)&pub::oRecreate, "SC_Recreate (swapchain re-creation)"},
        {RVA_ANIM_JOB, (void *)fp::hAnimJob, (void **)&fp::oAnimJob, "Anim_Job (first-person head hiding in the render pose)"},
        {RVA_OCC_VIEWFN, (void *)occ::hOccView, (void **)&occ::oOccView, "Occ_View (foliage culling gets the engine view)"},
        {RVA_AUDIO_LISTENER, (void *)aud::hListener, (void **)&aud::oListener, "Audio_ListenerFromCamera (3D audio: the listener is your head)"},
        {RVA_AUDIO_EMITTER, (void *)aud::hEmitter, (void **)&aud::oEmitter, "Audio_EmitterUpdate (3D audio: Jin's own voice / gear centred)"},
    };
    for (auto &h : hooks) { MH_STATUS st = MH_CreateHook(g_base + h.rva, h.det, h.orig); logf("hook %s -> %s", h.name, MH_StatusToString(st)); if (st != MH_OK) { logf("hook failed -> aborting all hooks"); MH_Uninitialize(); return 0; } }
    cbt::install();   // 0.18.43 combat observer (its own signature check; off on a mismatch, the rest runs)
    // the shared block must exist before any hook can read it
    if (g_cfg.xr && !startHost()) { logf("xr disabled for this run"); if (!S) g_cfg.xr = 0; }
    if (g_cfg.xr == 4 && S) { wchar_t ini[MAX_PATH]; swprintf(ini, MAX_PATH, L"%ls\\gotvr.ini", g_dir); vrin::install(ini); }
    { wchar_t ini[MAX_PATH]; swprintf(ini, MAX_PATH, L"%ls\\gotvr.ini", g_dir); swd::load(ini); }
    vrin::rsOn = g_fp.roomscale; vrin::rsDeadCm = g_fp.rsDeadCm; vrin::rsStopCm = g_fp.rsStopCm < g_fp.rsDeadCm ? g_fp.rsStopCm : g_fp.rsDeadCm * 0.5f;
    r1::capturePassTable();
    {   // F2 result from an earlier session: the second render takes this pass from the first render
        wchar_t ini[MAX_PATH]; swprintf(ini, MAX_PATH, L"%ls\\gotvr.ini", g_dir);
        int reuse = GetPrivateProfileIntW(L"gotvr", L"right_eye_reuse", -1, ini);
        if (reuse >= 14 && reuse < 59 && r1::tabOk) { u64 m0 = 0, m1 = 0; r1::setBit(m0, m1, reuse); r1::fixM0 = m0; r1::fixM1 = m1; g_fixCulprit = reuse; logf("right_eye_reuse=%d (saved F2 result): the second render takes pass %d from the first render", reuse, reuse); }
    }
    if (g_cfg.xr == 3) {
        if (r1::tabOk) { r1::on = 1; logf("xr=3 NATIVE STEREO: second render (right eye) on from the start; history reset every frame = %d", g_cfg.histReset); }
        else { g_cfg.xr = 2; logf("xr=3 needs the verified pass table -> falling back to xr=2 (alternate-eye)"); if (S) S->mode = 2; }
    }
    rsz::apply = (rsz::FnApply)(g_base + RVA_APPLY_DISPLAY);
    fp::on = g_fp.on; if (g_cfg.xr == 4) logf("first person: %s (F5 toggles; eye %.0f cm up / %.0f cm forward of the head joint, head hiding %s)", g_fp.on ? "ON" : "off", g_fp.eyeUpCm, g_fp.eyeFwdCm, g_fp.hide ? "on" : "off");
    if (g_cfg.xr == 4) { rsz::want = g_renderSize; logf("render size: %s", g_renderSize ? "square headset render size set before the swapchain exists (render_size in gotvr.ini, F2 cycles)" : "the game's own size (render_size=0)"); if (g_renderSize) logf("render size: %d x %d per eye", g_renderSize, g_renderSize); }
    MH_STATUS st = MH_EnableHook(MH_ALL_HOOKS); logf("hooks enabled: %s", MH_StatusToString(st));
    aud::setPos = (aud::FnSetPos)GetProcAddress(GetModuleHandleW(nullptr), "?SetPosition@SoundEngine@AK@@YA?AW4AKRESULT@@_KAEBVAkTransform@@@Z"); logf("3D audio: Wwise SetPosition %s", aud::setPos ? "found (export)" : "NOT found -> Jin's own sounds stay where the game puts them");
    { wchar_t ini[MAX_PATH]; swprintf(ini, MAX_PATH, L"%ls\\gotvr.ini", g_dir); wchar_t b[32]; GetPrivateProfileStringW(L"gotvr", L"audio_self_centre", L"1", b, 32, ini); aud::selfOn = _wtoi(b) != 0;
      GetPrivateProfileStringW(L"gotvr", L"audio_self_radius_cm", L"35", b, 32, ini); { const float v = (float)_wtof(b); if (v >= 0 && v <= 200) aud::selfRadiusCm = v; }
      GetPrivateProfileStringW(L"gotvr", L"audio_self_ahead_cm", L"25", b, 32, ini); { const float v = (float)_wtof(b); if (v >= 0 && v <= 200) aud::selfAheadCm = v; } }
    aud::on = g_audioHead; logf("3D audio: %s", g_audioHead ? "the Wwise listener follows your head in first person (audio_head = 1)" : "the game's own listener (audio_head = 0)");
    if (g_playFlat) { obs::on = 1; logf("PLAY MODE FLAT (play_mode=flat): no headset - the game's own camera and controls; the log records Jin's state and hero flag changes every second (STATE/s), F8 writes a MARK line"); }
    if (g_cfg.xr == 4) {
        dlss::mode = g_cfg.dlssMode; if (g_cfg.dlssMode) dlss::install();
        logf("per-eye DLSS mode %d (0 = off: recommended, use the game's TAA; 1/2 experimental)", g_cfg.dlssMode);
        ui::hud = g_ui.hud; ui::hudDeg = g_ui.hudDeg; ui::crop = g_ui.thCrop; ui::cropOn = g_ui.crop; ui::pauseMask = g_ui.pauseMask; ui::layerWant = g_ui.layer; ui::titleReason = g_ui.titleReason;
        ui::fnTrans = (ui::FnTrans)(g_base + RVA_RES_TRANSITION); ui::fnFlush = (ui::FnFlush)(g_base + RVA_CTX_FLUSHBARRIERS);
        logf("UI: HUD panel %s (%.0f deg wide, converges at %.2f m, %.1f deg below eye level); theatre screen %s (%.1f m away, %.1f m wide, backdrop %d, crop %d, flat render %d, pause reasons 0x%x)",
             g_ui.hud ? "on" : "off", g_ui.hudDeg, g_ui.hudDist, g_ui.hudDown, g_ui.theater ? "auto" : "never (F10 forces it)", g_ui.thDist, g_ui.thWidth, g_ui.thBackdrop, ui::crop, g_ui.flatRender, g_ui.pauseMask);
        if (!g_ui.theater) ui::theaterMode = 2;
        occ::fastDeg = g_fastTurnDeg; occ::fastDegLeft = g_fastTurnDegLeft; occ::viewMode = g_foliageOcc; occ::fastMode = g_fastTurnMode; logf("fast head-turn limit for occlusion history: left eye %.2f deg, right eye %.1f deg per eye frame pair", occ::fastDegLeft, occ::fastDeg);
        bool ok = cutp::apply(); applyPreset(ok ? g_cfg.histMode : 4);
        // 0.18.38: the 0.18.37 per-eye TAA on top of the preset is gone (owner: "it did not fix the issue and made things look worse")
        { wchar_t ini[MAX_PATH]; swprintf(ini, MAX_PATH, L"%ls\\gotvr.ini", g_dir); WritePrivateProfileStringW(L"gotvr", L"taa_per_eye", NULL, ini); }
        if (!ok) logf("cut overrides unavailable -> history preset 5 RESET (0.9.0 behaviour)");
        logf("history preset %s", kPresets[g_preset.load()].name);
    }
    logflush();
    LONG lastHostState = -1; bool hostExitLogged = false;
    bool prev[11] = {}; u64 lastRep = GetTickCount64(), lastCallers = lastRep;
    for (;;) {
        Sleep(50);
        if (g_cfg.hotkeys) {   // 0.18.41 numpad action tags for the flat-play observer log (NumLock on)
            static bool pn[obs::NTAG] = {}; const bool ctl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
            for (int t = 1; t < obs::NTAG; t++) { int vk; if (t <= 9) vk = VK_NUMPAD0 + t; else if (t == 10) vk = VK_NUMPAD0; else if (t <= 19) vk = VK_NUMPAD0 + (t - 10); else if (t == 20) vk = VK_NUMPAD0; else vk = t == 21 ? VK_DIVIDE : t == 22 ? VK_MULTIPLY : VK_SUBTRACT;
                const bool wantCtl = t >= 11 && t <= 20; const bool dn = (GetAsyncKeyState(vk) & 0x8000) != 0 && (t > 20 || ctl == wantCtl); if (dn && !pn[t]) obs::setTag(t); pn[t] = dn; } }
        if (g_cfg.hotkeys) {   // 0.18.47 katana-hit tests: Ctrl+Shift+H strikes / Ctrl+Shift+K kills the nearest hostile enemy, Ctrl+Shift+J lists them
            cbt::dbw::poll();   // 0.18.50 data watchpoints (Ctrl+Shift+W, flat play)
            {   // 0.18.54 flat swing test: the first attack press after Ctrl+Shift+G (left mouse or pad X)
                static bool was = true; const bool dn = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) || (g_padHave[0][0] && (*(const WORD *)(g_padLast[0][0] + 4) & 0x4000));
                if (cbt::swingArmed.load() && dn && !was) { const u64 n0 = GetTickCount64(); cbt::swingArmed = 0; cbt::swingUntil = n0 + 1200; cbt::assistUntil = n0 + (u64)cbt::swingDelayMs + 600; cbt::swingFireAt = n0 + (u64)cbt::swingDelayMs;
                    logf("SWING TEST: your attack pressed - the katana's blow follows in %d ms", cbt::swingDelayMs); }
                was = dn; }
            { static bool pu = false; const bool du = (GetAsyncKeyState(VK_CONTROL) & 0x8000) && (GetAsyncKeyState(VK_SHIFT) & 0x8000) && (GetAsyncKeyState('U') & 0x8000);
              if (du && !pu) { pub::uidump::req = 1; logf("HOTKEY Ctrl+Shift+U: the UI image of the next frame will be saved (ui_dump_color.bmp / ui_dump_alpha.bmp)"); } pu = du; }
            static bool ph[7] = {}; const int hk[7] = {'H', 'K', 'J', 'W', 'R', 'G', 'M'}; const bool cs = (GetAsyncKeyState(VK_CONTROL) & 0x8000) && (GetAsyncKeyState(VK_SHIFT) & 0x8000);
            for (int i = 0; i < 7; i++) { const bool dn = cs && (GetAsyncKeyState(hk[i]) & 0x8000); if (dn && !ph[i]) { cbt::hkReq = i == 3 ? 5 : i == 4 ? 6 : i == 5 ? 7 : i == 6 ? 8 : i + 1; logf("HOTKEY Ctrl+Shift+%c: %s", hk[i], i == 6 ? "strike the nearest hostile enemy by the MESSAGE path (comparison)" : i == 5 ? "swing test (the blow during your own attack)" : i == 0 ? "strike the nearest hostile enemy" : i == 1 ? "kill the nearest hostile enemy" : i == 2 ? "list the enemies the katana can hit" : i == 3 ? "watch the nearest enemy's reaction fields (data watchpoints)" : "resolve scan mark (what changed on Jin just now)"); } ph[i] = dn; } }
        if (g_cfg.hotkeys && g_cfg.xr == 4 && !g_playFlat) {   // 0.18.38 the hands' places on the katana, live (saved in gotvr.ini)
            static bool pa[4] = {}; const int ak[4] = {VK_NEXT, VK_PRIOR, VK_END, VK_HOME};
            for (int i = 0; i < 4; i++) { const bool dn = (GetAsyncKeyState(ak[i]) & 0x8000) != 0;
                if (dn && !pa[i] && (GetAsyncKeyState(VK_CONTROL) & 0x8000)) { swd::adjReq[i < 2 ? 1 : 0] += (i % 2 == 0) ? 1 : -1;
                    logf("HOTKEY Ctrl+%s: %s hand 1 cm %s", i == 0 ? "PageDown" : i == 1 ? "PageUp" : i == 2 ? "End" : "Home", i < 2 ? "right" : "left", i % 2 == 0 ? (i < 2 ? "down the hilt (toward the pommel)" : "down the sheath") : (i < 2 ? "up the hilt (toward the guard)" : "up the sheath (toward the guard)")); }
                pa[i] = dn; } }
        if (g_cfg.xr == 4 && g_ui.layer) ui::hostUiOk = S && S->uiOk ? 1 : 0;
        if (g_cfg.hotkeys) {
            const int keys[11] = {VK_F7, VK_F8, VK_F9, VK_F10, VK_F11, VK_F6, VK_F5, VK_F4, VK_F3, VK_F2, VK_F1};
            for (int i = 0; i < 11; i++) {
                bool down = (GetAsyncKeyState(keys[i]) & 0x8000) != 0;
                if (down && !prev[i] && (!g_playFlat || i == 1)) {   // 0.18.24 flat play: only F8 (MARK); the VR test keys stay off
                    if (g_cfg.xr == 4 && (GetAsyncKeyState(VK_CONTROL) & 0x8000) && !(GetAsyncKeyState(VK_MENU) & 0x8000) && (i == 0 || i == 1 || i == 2 || i == 3 || i == 5 || i == 6)) {   // 0.18.26 katana draw tools (0.18.32: Ctrl+F9 the game draw census)
                        // 0.18.29: once the katana is calibrated, Ctrl+F7 / Ctrl+F8 pick the BLADE joint (only the blade moves on a draw);
                        // Ctrl+Shift+F7 / Ctrl+Shift+F8 pick the katana (sheath) joint as before
                        const bool sheathPick = (GetAsyncKeyState(VK_SHIFT) & 0x8000) || !swd::calOk || fp::bladeCandN.load() <= 0;
                        if (i == 0) { static int ci = -1, cb = -1; const int mode = sheathPick ? 0 : 1;
                            if (mode != swd::probeMode.load()) { ci = cb = -1; swd::probeMode = mode; }
                            if (mode == 0) { const int n = (int)(sizeof swd::kCand / sizeof swd::kCand[0]); ci = (ci + 1) % (n + 1);
                                if (ci == n) { swd::probeIdx = -1; logf("HOTKEY Ctrl+F7 katana joint probe OFF"); }
                                else { swd::probeIdx = swd::kCand[ci]; logf("HOTKEY Ctrl+F7 katana joint probe: joint %d lifted 15 cm - look at Jin's hips; Ctrl+F8 keeps it as the katana, Ctrl+F7 the next one", swd::kCand[ci]); } }
                            else {   // 0.18.31: first the katana lifted with each joint under it HELD still (one of them may be the sheath), then the candidates
                                const int nd = fp::bladeCandDesc.load(), n = fp::bladeCandN.load() + nd; cb = (cb + 1) % (n + 1);
                                if (cb == n) { swd::probeIdx = -1; swd::probeHold = -1; logf("HOTKEY Ctrl+F7 blade probe OFF"); }
                                else if (cb < nd) { swd::probeHold = fp::bladeCand[cb]; swd::probeIdx = swd::joint; logf("HOTKEY Ctrl+F7 BLADE probe %d of %d: the katana (joint %d) lifted 15 cm with joint %d HELD STILL - if only the sword lifts and the sheath stays, Ctrl+F8 keeps this [the game has the sword %s: joint 637 %.0f cm from the katana]", cb + 1, n, swd::joint, fp::bladeCand[cb], fp::d637.load() < 90.0f ? "DRAWN" : "sheathed", fp::d637.load()); }
                                else { const int c2 = cb - nd; swd::probeHold = -1; swd::probeIdx = fp::bladeCand[c2]; logf("HOTKEY Ctrl+F7 BLADE probe %d of %d: joint %d lifted 15 cm - if ONLY the sword (not the sheath) lifts, Ctrl+F8 keeps it; Ctrl+F7 the next one [the game has the sword %s: joint 637 %.0f cm from the katana]", cb + 1, n, fp::bladeCand[c2], fp::d637.load() < 90.0f ? "DRAWN" : "sheathed", fp::d637.load()); } } }
                        else if (i == 1) { const int pj = swd::probeIdx.load();
                            if (pj >= 0 && swd::probeMode.load() == 1 && fp::d637.load() < 90.0f) {   // 0.18.36 found while the GAME has the sword drawn: the drawn sword's own joint
                                swd::iniWrite(L"sword_drawn_blade_joint", (float)pj); swd::probeIdx = -1; swd::probeHold = -1;
                                logf("HOTKEY Ctrl+F8 with the game's sword DRAWN: joint %d carries the drawn sword alone (saved as sword_drawn_blade_joint - the next build uses it)", pj); }
                            else if (pj >= 0 && swd::probeMode.load() == 1) { const int ph = swd::probeHold.load(); swd::blade = pj; swd::keepJ = ph; swd::probeIdx = -1; swd::probeHold = -1; swd::iniWrite(L"sword_blade_joint", (float)pj); swd::iniWrite(L"sword_keep_joint", (float)ph); swd::resetReq = 1;
                                logf("HOTKEY Ctrl+F8 BLADE joint = %d%s (saved): only it moves on a draw, the sheath stays on the hip", pj, ph >= 0 ? ", with the sheath joint held still" : ""); if (ph >= 0) logf("   sheath joint (held still) = %d", ph); }
                            else if (pj >= 0) { swd::joint = pj; swd::probeIdx = -1; swd::calOk = 0; swd::blade = -1; fp::bladeCandN = -1; swd::iniWrite(L"sword_joint", (float)pj); swd::iniWrite(L"sword_calibrated", 0.0f); swd::iniWrite(L"sword_blade_joint", -1.0f);
                                               logf("HOTKEY Ctrl+F8 katana joint = %d (saved; the rail is the default guess until Ctrl+F10, the blade joint is reset)", pj); }
                            else logf("HOTKEY Ctrl+F8: no probe running (Ctrl+F7 first) - the katana joint stays %d, the blade joint %d", swd::joint, swd::blade); }
                        else if (i == 5 || i == 6) { const int hh = i == 6 ? 1 : 0; swd::capReq[hh] = (GetAsyncKeyState(VK_SHIFT) & 0x8000) ? 2 : 1; logf("HOTKEY Ctrl+%sF%d: %s %s hand grip capture", (GetAsyncKeyState(VK_SHIFT) & 0x8000) ? "Shift+" : "", i == 6 ? 5 : 6, (GetAsyncKeyState(VK_SHIFT) & 0x8000) ? "clear the" : "start the", hh ? "right" : "left"); }
                        else if (i == 2) { fp::censusReq = 1; logf("HOTKEY Ctrl+F9: joint census now and 1.5 s after (0.18.38: the game is never asked to draw / sheathe - draw it yourself with an attack if you need it)"); }
                        else { swd::calReq = 1; logf("HOTKEY Ctrl+F10 katana rail calibration: left hand at the sheath mouth, right hand at the end of the hilt"); }
                        prev[i] = down; continue;
                    }
                    if (i == 0 && g_cfg.xr == 4 && (GetAsyncKeyState(VK_SHIFT) & 0x8000)) {   // 0.18.16: the TAA A/B made no difference (owner) -> near-plane A/B
                        static float saved = 2.0f; if (g_fp.nearCm > 0.0f) { saved = g_fp.nearCm; g_fp.nearCm = 0.0f; } else g_fp.nearCm = saved > 0.0f ? saved : 2.0f;
                        logf("HOTKEY Shift+F7 first-person near clip -> %s %.1f cm", g_fp.nearCm > 0.0f ? "ours" : "the game's own (10 cm), was", g_fp.nearCm > 0.0f ? g_fp.nearCm : saved); }
                    else if (i == 0) {
                        if (g_cfg.xr == 4) { m_cullHead ^= 1; logf("HOTKEY F7 culling/shadow camera follows your head -> %d", m_cullHead.load()); }
                        else { m_armed ^= 1; logf("HOTKEY F7 armed zero-delta -> %d", m_armed.load()); }
                    }
                    if (i == 1 && g_playFlat) { obs::mark(); }
                    else if (i == 1 && g_cfg.xr == 4 && (GetAsyncKeyState(VK_SHIFT) & 0x8000)) {
                        const int m = (occ::viewMode.load() + 1) % 3; occ::viewMode = m;
                        logf("HOTKEY Shift+F8 foliage/grass culling view -> %s", m == 1 ? "1 the engine's own view" : m == 2 ? "2 no occlusion view (default)" : "0 the per-eye-rewritten view (0.17.4)");
                    } else if (i == 1) {
                        if (g_cfg.xr == 4) { int m = occ::fastMode.load() % 3 + 1; if (m == 3) m = 0; occ::fastMode = m; occ::setVoFast(occ::fast.load());
                                             logf("HOTKEY F8 fast head turns -> %s", m == 1 ? "1 culling memory off during the turn" : m == 2 ? "2 game's own culling memory during the turn" : "0 no special handling"); }
                        else { m_leftHalf ^= 1; logf("HOTKEY F8 projection left-half (EXP-03b) -> %d", m_leftHalf.load()); }
                    }
                    if (i == 2 && g_cfg.xr == 4 && (GetAsyncKeyState(VK_SHIFT) & 0x8000)) {
                        g_eyeSwap = !g_eyeSwap.load(); logf("HOTKEY Shift+F9 the simulated tick renders the %s eye (pop-in diagnostic: does it follow?)", g_eyeSwap.load() ? "RIGHT" : "LEFT (default)");
                    } else if (i == 2) {
                        if (g_cfg.xr == 4) {   // HUD panel size: 40 -> 55 -> 70 -> 85 -> off -> 40
                            static const float sz[4] = {40, 55, 70, 85}; float nx = 0;
                            if (!ui::hud.load()) nx = sz[0]; else for (int j = 0; j < 4; j++) if (sz[j] > ui::hudDeg.load() + 0.5f) { nx = sz[j]; break; }
                            if (nx > 0) { ui::hud = 1; ui::hudDeg = nx; if (S) S->hudDeg = nx; logf("HOTKEY F9 HUD panel %.0f deg wide", nx); } else { ui::hud = 0; logf("HOTKEY F9 HUD panel OFF (UI stretched over the lens)"); } }
                        else { m_yaw ^= 1; logf("HOTKEY F9 camera yaw +5 deg, idempotent (EXP-02) -> %d", m_yaw.load()); }
                    }
                    const bool ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0, shf = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
                    const bool alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
                    if (g_cfg.xr == 4 && !ctrl && !alt && (i == 10 || i == 9 || i == 8)) handTrim(10 - i, shf ? -5.0f : 5.0f);
                    else if (g_cfg.xr == 4 && ctrl && !alt && (i == 10 || i == 9 || i == 8)) handTrim(13 - i, shf ? -1.0f : 1.0f);   // 0.18.12: wrist position
                    else if (i == 10 && g_cfg.xr == 4) { if (ui::rdRefused.exchange(0)) { ui::rdFailRun = 0; ui::f1Off = 1; } ui::f1Off = !ui::f1Off.load(); logf("HOTKEY F1 HUD -> %s", ui::f1Off.load() ? "drawn into the 3D image (0.14 panel, A/B)" : "its own sharp layer"); }
                    else if (i == 10) { m_offTest ^= 1; logf("HOTKEY F1 second render's camera 0.5 m to the right (flat stereo proof; needs F5 on) -> %d", m_offTest.load()); }
                    else if (i == 9 && g_cfg.xr == 4) {   // Ctrl+Alt+F2: headset render size 1536 -> 1792 -> 2048 -> 2304 -> 2560 -> game's own -> 1536
                        static const int sz[6] = {1536, 1792, 2048, 2304, 2560, 0}; int cur = rsz::want.load(), nx = sz[0];
                        for (int j = 0; j < 6; j++) if (sz[j] == cur) { nx = sz[(j + 1) % 6]; break; }
                        rsz::want = nx; logf("HOTKEY F2 headset render size -> %s %d", nx ? "square" : "the game's own (0)", nx); rsz::request(); }
                    else if (i == 9) { if (sweep::running.load()) sweep::end("cancelled with F2"); else sweep::begin(GetTickCount64()); }
                    else if (i == 8 && g_cfg.xr == 4) { ui::cropOn = !ui::cropOn.load(); logf("HOTKEY F3 letterbox crop -> %s", ui::cropOn.load() ? "ON (only the 16:9 picture goes to the headset)" : "OFF (the whole window image)"); }
                    else if (i == 8) { static int k = 0; k = (k + 1) % 6; r1::skipFrom = r1::skipSteps[k]; r1::pjDump = 1;
                                  if (r1::skipFrom.load()) logf("HOTKEY F3 second run skips passes %d..68 (bisection; image should come from run 1)", r1::skipFrom.load()); else logf("HOTKEY F3 second run complete again (no skipped passes)"); }
                    if (i == 3 && g_cfg.xr == 4 && (GetAsyncKeyState(VK_SHIFT) & 0x8000)) { fp::armsWant = !fp::armsWant.load(); logf("HOTKEY Shift+F10 VR arms (full-body IK) -> %s", fp::armsWant.load() ? "ON" : "off (animated arms)"); }
                    else if (i == 3) {
                        if (g_cfg.xr == 4) { int m = ui::theaterMode.load() == 1 ? 0 : 1; ui::theaterMode = m; logf("HOTKEY F10 theatre screen -> %s", m == 1 ? "ALWAYS (everything on the flat screen)" : "auto (videos and menus without a 3D world)"); }   // 0.18.2: "never" only from the ini (it left pause menus in 3D)
                        else { m_burst = 1; m_census = 1; slDump = 2; r1::pjDump = 1; logf("HOTKEY F10 snapshot + R1 view census"); } }
                    if (i == 7 && g_cfg.xr == 4 && (GetAsyncKeyState(VK_SHIFT) & 0x8000)) {
                        // 0.18.19 arm overlay test (owner: "the grass in black and white can be seen through the arm" - headset only, the monitor
                        // never shows it, solid when looking down-left): the HUD panel is a separate layer the headset draws OVER the game image,
                        // room-fixed in front of you. Anything in the UI image where nothing is drawn (colour with alpha 0) is ADDED by a
                        // premultiplied layer. Shift+F4: HUD premultiplied (as before) -> straight alpha (stray colour hidden) -> HUD hidden.
                        static int st = 0; st = (st + 1) % 3;
                        g_ui.premul = st == 1 ? 0 : 1; if (S) S->uiPremul = (u32)g_ui.premul; ui::hudHidden = st == 2 ? 1 : 0;
                        logf("HOTKEY Shift+F4 HUD layer -> %s", st == 0 ? "shown, premultiplied alpha (as before)" : st == 1 ? "shown, straight alpha (colour where the UI is transparent is not added)" : "hidden (the arm overlay test)");
                    }
                    else if (i == 7 && g_cfg.xr == 4) { g_ui.follow = !g_ui.follow; logf("HOTKEY F4 HUD panel -> %s", g_ui.follow ? "stays put in the room" : "fixed to your view"); }
                    else if (i == 7) { r1::dupOn ^= 1; r1::pjDump = 1; logf("HOTKEY F4 R1 main-only renderables duplicated into the second view (BUG-17 fix, A/B) -> %d", r1::dupOn.load()); }
                    if (i == 4) { vrin::moveCapture(150); m_recenter = 1; if (S) InterlockedIncrement(&S->theaterRecenter); g_flatCut = 2; occ::reseed(); logf("HOTKEY F11 recentre head tracking (and the theatre screen) + all per-eye histories reset (like the pause menu)"); }
                    if (i == 6 && g_cfg.xr == 4) {   // 0.17.0: F5 first person on/off, Shift+F5 hero skeleton into the log
                        if (GetAsyncKeyState(VK_SHIFT) & 0x8000) { fp::dumpReq = 1; logf("HOTKEY Shift+F5 hero skeleton dump requested (logged at the next first-person frame)"); }
                        else { fp::on = !fp::on.load(); logf("HOTKEY F5 first person -> %s", fp::on.load() ? "ON" : "OFF (the game's third-person camera)"); } }
                    else if (i == 6) { r1::on ^= 1; slDump = 2; logf("HOTKEY F5 R1-A second render-graph run (same camera) -> %d", r1::on.load()); }
                    if (i == 5 && g_cfg.xr == 4 && (GetAsyncKeyState(VK_SHIFT) & 0x8000)) {   // 0.18.21 arm shading test (see namespace shade)
                        const int m = shade::mode.load() ? 0 : 1; shade::mode = m;
                        logf("HOTKEY Shift+F6 first-person AO fix -> %s (the game holds AO %d, screen-space shadows %d; applied at the next gameplay frame)", m ? "ON: AO off + screen-space shadows on" : "off: your own settings back", shade::seenA.load(), shade::seenS.load());
                    }
                    else if (i == 5) {
                        if (g_cfg.xr == 4 && cutp::applied) { applyPreset(g_preset.load() + 1); logf("HOTKEY F6 history preset %s", kPresets[g_preset.load()].name); }
                        else { int cur = m_histReset.load(); if (cur < 0) cur = g_cfg.histReset; m_histReset = !cur; logf("HOTKEY F6 temporal history reset every VR frame -> %d", !cur); }
                    }
                }
                prev[i] = down;
            }
        }
        u64 now = GetTickCount64();
        meter::poll(now); harness::tick(now, g_presents.load()); sweep::tick(now);
        if (g_cfg.census && now - lastRep >= 1000) {
            lastRep = now; report();
            if (S) {
                static u64 pp, pm, pb, pn, pv;
                u64 a = pub::published, b = pub::mismatch, c = pub::skippedBusy, d = pub::noPkt, e = c_pktValid;
                logf("   xr/s: packets=%llu published=%llu frameIdMismatch=%llu busySkip=%llu noPose=%llu | host state=%ld beat=%llums ago frames=%lld copied=%lld pairs shown=%lld | slot43 before/after FrameView %llu/%llu",
                     (unsigned long long)(e - pv), (unsigned long long)(a - pp), (unsigned long long)(b - pm), (unsigned long long)(c - pb), (unsigned long long)(d - pn),
                     S->hostState, (unsigned long long)(now - (u64)S->hostBeat), (long long)S->hostFrames, (long long)S->hostCopied, (long long)S->hostPairs,
                     (unsigned long long)c_slot43BeforeFV.load(), (unsigned long long)c_slot43AfterFV.load());
                pp = a; pm = b; pb = c; pn = d; pv = e;
            }
        }
        {   // R1 stall watchdog: ticks stopped while F5 is on -> log where the game and submit threads are waiting
            static u64 lastTick = 0, lastChange = 0; static bool dumped = false; u64 tk = c_tick.load();
            if (tk != lastTick) { lastTick = tk; lastChange = now; dumped = false; }
            else if (r1::on.load() && !dumped && lastChange && now - lastChange > 3000) {
                dumped = true; logf("R1 STALL: no game tick for %llu ms with F5 on", (unsigned long long)(now - lastChange));
                const DWORD tids[2] = {t_tick.load(), t_present.load()}; const char *nm[2] = {"game", "submit"};
                for (int k = 0; k < 2; k++) {
                    HANDLE th = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, tids[k]);
                    if (!th) { logf("   %s thread %lu: OpenThread failed %lu", nm[k], tids[k], GetLastError()); continue; }
                    if (SuspendThread(th) != (DWORD)-1) {
                        CONTEXT c{}; c.ContextFlags = CONTEXT_FULL;
                        if (GetThreadContext(th, &c)) {
                            char b[900]; int n = 0; u64 *sp = (u64 *)c.Rsp;
                            for (int i = 0; i < 400 && n < 860 && readable(sp + i, 8); i++) { u64 v = sp[i]; if (v > (u64)g_base + 0x1000 && v < (u64)g_base + 0x1200000) n += snprintf(b + n, sizeof b - n, " %llx", (unsigned long long)(v - (u64)g_base)); }
                            b[n] = 0; char mm[64];
                            logf("   %s thread %lu: rip=%p (%s%s0x%llx) stack game rvas:%s", nm[k], tids[k], (void *)c.Rip, modOf((void *)c.Rip, mm, sizeof mm), "+",
                                 (unsigned long long)(c.Rip - (u64)GetModuleHandleA(mm[0] && mm[0] != '?' ? mm : nullptr)), b);
                        }
                        ResumeThread(th);
                    }
                    CloseHandle(th);
                }
                logflush();
            }
        }
        { static u64 lastSync = 0; if (g_cfg.xr == 4 && now - lastSync >= 1000) { lastSync = now; static u64 pfz = 0, psm = 0; u64 fz = g_frozenTicks, sm = g_simTicks;
            static u64 pcut = 0, ph[cutp::NG] = {}; u64 cu = c_engineCuts, h[cutp::NG]; for (int g = 0; g < cutp::NG; g++) h[g] = cutp::groupHits(g);
            static u64 ppr = 0; u64 pr = pub::pairsPub;
            { static u64 pc = 0, pf = 0, pb = 0, pd = 0, pk = 0; u64 c = occ::calls, f = occ::fixed, b = occ::skipBranch, d = occ::skipDims, k = occ::skipBank;
              static u64 pft = 0; u64 ft = occ::fastTicks;
              { static u64 a0, a1, q00, q01, q10, q11; const u64 b0 = occ::trkOwn, b1 = occ::trkEngine, r00 = occ::parEye[0][0], r01 = occ::parEye[0][1], r10 = occ::parEye[1][0], r11 = occ::parEye[1][1];
                logf("   OCCTRK/s: own-slot tests=%llu engine pairing=%llu | parity seen by the left eye p0=%llu p1=%llu, right eye p0=%llu p1=%llu", (unsigned long long)(b0 - a0), (unsigned long long)(b1 - a1),
                     (unsigned long long)(r00 - q00), (unsigned long long)(r01 - q01), (unsigned long long)(r10 - q10), (unsigned long long)(r11 - q11)); a0 = b0; a1 = b1; q00 = r00; q01 = r01; q10 = r10; q11 = r11; }
              logf("   OCC/s: setup calls=%llu per-eye fixed=%llu | not steady-state=%llu dims differ=%llu no bank=%llu | eye=%d | fast-turn frames=%llu (left %llu, right %llu; last %.2f deg, head %.2f deg, limit left %.2f / right %.1f)", (unsigned long long)(c - pc), (unsigned long long)(f - pf), (unsigned long long)(b - pb), (unsigned long long)(d - pd), (unsigned long long)(k - pk), g_camEye.load(), (unsigned long long)(ft - pft), (unsigned long long)occ::fastL.exchange(0), (unsigned long long)occ::fastR.exchange(0), occ::lastAngle, occ::lastHeadAngle.load(), occ::fastDegLeft, occ::fastDeg); pft = ft;
              logf("   OCCVIEW/s: foliage culling view mode %d | calls %llu, engine view %llu, none %llu", occ::viewMode.load(), (unsigned long long)occ::viewCalls.exchange(0), (unsigned long long)occ::viewEngine.exchange(0), (unsigned long long)occ::viewNull.exchange(0));
              pc = c; pf = f; pb = b; pd = d; pk = k; }
            { static u64 a0, a1, a2, a3, a4, a5, a6, a7, vs0, vw0; u64 b0 = taa::runs, b1 = taa::drawFix, b2 = taa::velFix, b3 = taa::restores, b4 = taa::saves, b5 = taa::resets, b6 = taa::stale, b7 = taa::texFail;
              { static u64 q0, q1, q2, q3, q4, q5, q6; u64 w0 = ui::binds, w1 = ui::mapped, w2 = ui::scissors, w3 = ui::movieDraws, w4 = ui::flatPosts, w5 = ui::skipNoView, w6 = ui::posts;
                static u64 q7; u64 w7 = pub::flatPub;
                logf("   UI/s: frames=%llu UI binds=%llu mapped=%llu scissors mapped=%llu | video draws=%llu flat frames=%llu (flat-rendered published %llu, paused=%d, world mode %u) | no eye view=%llu | panel %s %.0f deg, eye %d, theatre frames shown by host=%lld",
                     (unsigned long long)(w6 - q6), (unsigned long long)(w0 - q0), (unsigned long long)(w1 - q1), (unsigned long long)(w2 - q2), (unsigned long long)(w3 - q3), (unsigned long long)(w4 - q4), (unsigned long long)(w7 - q7),
                     (int)ui::pausedTick.load(), ui::worldMode.load(), (unsigned long long)(w5 - q5), ui::hud.load() ? "on" : "off", ui::hudDeg.load(), ui::cur.eye, S ? (long long)S->hostTheater : -1LL);
                q0 = w0; q1 = w1; q2 = w2; q3 = w3; q4 = w4; q5 = w5; q6 = w6; q7 = w7;
                static u64 r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10; u64 x0 = ui::lBinds, x1 = ui::lCopies, x2 = ui::lSkip, x3 = ui::fadeFix, x4 = ui::rdBinds, x5 = ui::rdClears, x6 = ui::rdNoK, x7 = ui::rdOther, x8 = ui::rdHidden, x9 = ui::cineReads, x10 = ui::cineHits;
                const u64 pm = ui::uiPassMask.exchange(0); char pb[160]; int pn = 0; pb[0] = 0; for (int b = 0; b < 64 && pn < 150; b++) if ((pm >> b) & 1) pn += snprintf(pb + pn, sizeof pb - pn, " %d", b + 32);
                const u32 hz = ui::histZero.exchange(0), hp = ui::histPart.exchange(0), hf = ui::histFull.exchange(0);
                logf("   UILAYER/s: mode=%d ready=%d (images=%d hostChain=%d record=%p F1off=%d) | UI-pass binds armed=%llu redirected flushes=%llu clears=%llu published=%llu hidden by fade=%llu | no image=%llu other targets=%llu non-UI-pass binds=%llu | pass-65 un-redirects=%llu | passes binding the UI target:%s | host frames with UI layer=%lld",
                     ui::mode.load(), (int)ui::layerReady(), ui::shReady.load(), ui::hostUiOk.load(), (void *)ui::probeRec.load(), ui::f1Off.load(), (unsigned long long)(x0 - r0), (unsigned long long)(x4 - r4), (unsigned long long)(x5 - r5), (unsigned long long)(x1 - r1), (unsigned long long)(x8 - r8),
                     (unsigned long long)(x6 - r6), (unsigned long long)(x7 - r7), (unsigned long long)(x2 - r2), (unsigned long long)(x3 - r3), pn ? pb : " none", S ? (long long)S->hostUi : -1LL);
                { u64 a = camA(); float cf[6] = {0, 0, 0, 0, 0, 0}; if (isCam(a)) { cf[0] = *(const float *)(a + 0x670); for (int j = 0; j < 4; j++) cf[1 + j] = *(const float *)(a + 0x6A4 + 4 * j); cf[5] = (float)*(const u8 *)(a + 0x7D8); }
                  logf("   CAMCENSUS/s: fov %.3f rect +0x6A4 %.3f %.3f %.3f %.3f cut %.0f | cutscene(bars)=%d | outermost 3D rows brightness avg %d max %d (of 255) | HUD images skipped (right eye)=%llu | size recomputes=%llu (output %dx%d, UI/flat aspect %.3f) swapchain resizes=%llu UI row copies skipped while resizing=%llu", cf[0], cf[1], cf[2], cf[3], cf[4], cf[5], ui::sceneBars.load(), ui::rowEdgeAvg.load(), ui::rowEdgeMax.load(), (unsigned long long)ui::rightSkip.exchange(0), (unsigned long long)rsz::calls.exchange(0), rsz::lastW.load(), rsz::lastH.load(), rsz::ana.load(), (unsigned long long)pub::resizes.load(), (unsigned long long)ui::cineSizeSkip.exchange(0)); }
                  { static const char *rn[] = {"active", "F5 off", "no hero", "hero not usable", "no up axis", "head not animated yet", "camera far from the hero (scripted)"};
                    const int r = fp::lastReason.load(); const char *rs = r >= 10 ? (r == 11 ? "flat/theatre frame" : r == 12 ? "cutscene" : r == 13 ? "start screen" : "UI mode") : (r >= 0 && r <= 6 ? rn[r] : "-");
                    static u64 f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10; const u64 g0 = fp::nActive, g1 = fp::nBlockedUi, g2 = fp::nNoHero, g3 = fp::nFar, g4 = fp::nCollapse, g5 = fp::nSeenCollapsed, g6 = fp::nSeenRestored, g7 = fp::nHeroJobs, g8 = fp::nJobsHidden, g9 = fp::nJobsNoBuf, g10 = fp::nJointsHidden;
                    logf("   FP/s: first person %s (frames %llu, game camera kept: UI %llu, no hero %llu, far camera %llu) | hero %p joints %d head %d neck %d | head set %d (dropped %llu) | hero animation jobs %llu, head hidden in %llu (%.0f joints each; no pose buffer %llu; pose already hidden on entry %llu / fresh %llu) | head %.0f cm above the root, game camera %.1f m from the head | eyes on the head centre: %.1f cm sideways from the head joint",
                         rs, (unsigned long long)(g0 - f0), (unsigned long long)(g1 - f1), (unsigned long long)(g2 - f2), (unsigned long long)(g3 - f3), (void *)fp::hero, fp::nJ, fp::headIdx, fp::neckIdx, fp::nHide, (unsigned long long)fp::nDropped.load(),
                         (unsigned long long)(g7 - f7), (unsigned long long)(g8 - f8), g8 > f8 ? (double)(g10 - f10) / (double)(g8 - f8) : 0.0, (unsigned long long)(g9 - f9), (unsigned long long)(g5 - f5), (unsigned long long)(g6 - f6), fp::headUp.load() / (g_cfg.unitsPerM * 0.01f), fp::camDist.load(), fp::headLat.load());
                    (void)g4; (void)f4;
                    { const u64 ca = camA(); const float cn = isCam(ca) ? *(const float *)(ca + 0x664) : -1.0f;
                    logf("   NEAR/s: first-person near plane %.1f cm (0 = game's own; camera now %.1f, environment entries set %llu) applied left %llu right %llu, projection not recognised %llu | hand trims pitch %.1f yaw %.1f roll %.1f deg (F1/F2/F3, Shift = back), wrist back %.1f up %.1f thumb %.1f cm (Ctrl+F1/F2/F3) | AO fix (fp_ao_off, Shift+F6) %d: the game holds screen-space shadows %d, ambient occlusion %d (applies %llu)", g_fp.nearCm, cn, (unsigned long long)stipple::nNear.exchange(0),
                         (unsigned long long)fp::nearHits[0].exchange(0), (unsigned long long)fp::nearHits[1].exchange(0), (unsigned long long)fp::nearMiss.exchange(0), g_fp.handPitch, g_fp.handYaw, g_fp.handRoll, g_fp.handBackCm, g_fp.handUpCm, g_fp.handSideCm, shade::mode.load(), shade::seenS.load(), shade::seenA.load(), (unsigned long long)shade::writes.exchange(0)); }
                    { static const char *iw[] = {"holding", "off", "pad input", "moving", "game owns the arms", "riding", "animation moving", "pose size"}; const int w = fp::idleWhy.load();
                      logf("   UNDER/s: body under the headset %s, offset %.1f cm (max %.0f) = %.1f cm forward / %.1f cm left of Jin as drawn, moved up to %.1f cm in a second, held while a hand is at the katana %llu frames", g_fp.bodyUnder ? "on" : "off (fp_body_under_hmd)", fp::underCm.load(), g_fp.underMaxCm, fp::underFwdCm.load(), fp::underLeftCm.load(), fp::underJumpCm.exchange(0), (unsigned long long)fp::nUnderHeld.exchange(0));
                      logf("   IDLE/s: idle hold %s | weight %.2f | stance turned %.1f deg to face forward | held poses %llu, let go %llu", w >= 0 && w <= 7 ? iw[w] : "-", fp::idleWOut.load(), -fp::idleSquareDeg.load(), (unsigned long long)fp::nIdleHeld.exchange(0), (unsigned long long)fp::nIdleRelease.exchange(0)); }
                    logf("   FINGERS/s: %s | posed %llu | grip curl left %.2f right %.2f | thumb down left %.2f right %.2f | hand stabilisation %s: filtered %llu, hand speed left %.2f right %.2f m/s, resets: first %llu lost %llu recenter %llu jump %llu time step %llu non-finite %llu", fp::fingState.load() == 1 ? (g_fp.fingers ? "VR fingers on" : "off (fp_fingers)") : fp::fingState.load() < 0 ? "not usable (animated)" : "not analysed yet", (unsigned long long)fp::nFingPosed.exchange(0), fp::fingCurlOut[0].load(), fp::fingCurlOut[1].load(), fp::thumbOut[0].load(), fp::thumbOut[1].load(), g_fp.stab ? "on" : "off", (unsigned long long)fp::stabFiltered.exchange(0), fp::stabSpeed[0].load(), fp::stabSpeed[1].load(), (unsigned long long)fp::stabResets[1].exchange(0), (unsigned long long)fp::stabResets[2].exchange(0), (unsigned long long)fp::stabResets[3].exchange(0), (unsigned long long)fp::stabResets[4].exchange(0), (unsigned long long)fp::stabResets[5].exchange(0), (unsigned long long)fp::stabResets[6].exchange(0));
                    logf("   ARMS/s: %s | solves %llu, held off %llu | hand blend left %.2f right %.2f | parent table %d | game owns the arms %.2f (input holds %llu, animation raised a hand %llu) | stand tall %.1f cm applied %.1f (%llu poses) | forearm twist left %.0f right %.0f deg (%d / %d twist joints), arm stretch left %.1f right %.1f cm, jump windows held %llu", fp::armsReady.load() ? (fp::armsWant.load() && g_fp.arms ? "VR arms ON" : "VR arms off (Shift+F10 / fp_arms)") : "arm chains not found yet",
                         (unsigned long long)fp::nArmSolves.exchange(0), (unsigned long long)fp::nArmSkipped.exchange(0), fp::armW[0].load(), fp::armW[1].load(), fp::parState.load(),
                         fp::gateW.load(), (unsigned long long)fp::nAnimHolds.exchange(0), (unsigned long long)fp::nRaised.exchange(0), fp::standDz.load() / (g_cfg.unitsPerM * 0.01f), fp::standApplied.load() / (g_cfg.unitsPerM * 0.01f), (unsigned long long)fp::nStand.exchange(0), fbik::lastTwistDeg[0], fbik::lastTwistDeg[1], fp::armsD[0].nTw, fp::armsD[1].nTw, fbik::lastStretchCm[0] / (g_cfg.unitsPerM * 0.01f), fbik::lastStretchCm[1] / (g_cfg.unitsPerM * 0.01f), (unsigned long long)fp::nJumpHeld.exchange(0));
                    swd::perSecond();
                    logf("   AUDIO/s: listener %s: head %llu / game camera %llu updates | emitters %llu, Jin's own centred %llu", aud::on.load() ? "head (first person)" : "the game's camera (audio_head = 0)", (unsigned long long)aud::nHead.exchange(0), (unsigned long long)aud::nGame.exchange(0), (unsigned long long)aud::nEmit.exchange(0), (unsigned long long)aud::nSelf.exchange(0));
                    logf("   ROOMSCALE/s: %s | your head %.0f cm from Jin's | walks %llu, covered for you %.0f cm, blocked %llu", g_fp.roomscale ? (GetTickCount64() < vrin::rsOkUntil.load() ? "on" : "on (held: riding / climbing / jump / menu)") : "off (fp_roomscale)", sqrtf(vrin::rsOff[0].load() * vrin::rsOff[0].load() + vrin::rsOff[1].load() * vrin::rsOff[1].load()), (unsigned long long)vrin::nRsDrive.exchange(0), fp::rsApplied.exchange(0), (unsigned long long)fp::nRsBlocked.exchange(0));
                    logf("   BODY/s: body drawn facing your forward: frames %llu, render poses turned %llu (items %llu), now %.0f deg | Jin faces %.0f deg from your forward, speed %.1f m/s | stance hips %.0f deg (squared while standing) | turns: dead zone %llu, snap %llu, on stopping %llu; body turned past the forward while moving %.0f deg, forward taken at move starts %llu (all %llu) | held off: riding %llu, fast %llu, refused %llu (fp_body_follow %d)",
                         (unsigned long long)fp::nBodyFrames.exchange(0), (unsigned long long)fp::nBodyJobs.exchange(0), (unsigned long long)fp::nWorldTurned.exchange(0), fp::bodyDeg.load(), fp::facingDeg.load(), fp::speedMs.load(), fp::hipYawDeg.load(), (unsigned long long)fp::nDeadTurns.exchange(0), (unsigned long long)fp::nSnapTurns.exchange(0), (unsigned long long)fp::nStopTurns.exchange(0), fp::moveOffDeg.load(), (unsigned long long)vrin::nMoveStarts.load(), (unsigned long long)vrin::nMoveCaps.load(),
                         (unsigned long long)fp::nBodyRiding.exchange(0), (unsigned long long)fp::nBodyFast.exchange(0), (unsigned long long)fp::nBodyRefused.exchange(0), g_fp.bodyFollow);
                    f0 = g0; f1 = g1; f2 = g2; f3 = g3; f4 = g4; f5 = g5; f6 = g6; f7 = g7; f8 = g8; f9 = g9; f10 = g10; }
                { static u64 s0, s1; u64 y0 = ui::sReads, y1 = ui::sHits;
                logf("   UICINE/s: HUD image reads=%llu hits=%llu cinematic=%d (max bar rows top/bottom %d/%d of 6, max veil rows %d of %d) | 3D picture reads=%llu hits=%llu bars=%d (max bar rows top/bottom %d/%d) | HUD alpha at mid-row samples: zero %u, partial %u, full %u (HUD-004: squared alpha shows as few full)",
                     (unsigned long long)(x9 - r9), (unsigned long long)(x10 - r10), ui::cine.load(), ui::maxTop.exchange(0), ui::maxBot.exchange(0), ui::maxVeil.exchange(0), ui::NROW,
                     (unsigned long long)(y0 - s0), (unsigned long long)(y1 - s1), ui::sceneBars.load(), ui::sMaxTop.exchange(0), ui::sMaxBot.exchange(0), hz, hp, hf); s0 = y0; s1 = y1; }
                r0 = x0; r1 = x1; r2 = x2; r3 = x3; r4 = x4; r5 = x5; r6 = x6; r7 = x7; r8 = x8; r9 = x9; r10 = x10;
                static u64 wu0; u64 wu = ui::worldUi; char cb[400]; int cn = 0; cb[0] = 0;
                for (auto &c : ui::bcls) { u32 f = c.fn.load(); if (!f) break; cn += snprintf(cb + cn, sizeof cb - cn, " %x:%u", f, c.n.exchange(0)); if (cn > 360) break; }
                logf("   UIDRAW/s: world-anchored UI batches drawn full-frame=%llu | renderer draw functions seen on the panel (rva:batches):%s", (unsigned long long)(wu - wu0), cn ? cb : " none"); wu0 = wu; }
              logf("   TAA/s: same-eye matrices=%llu draw=%llu velocity=%llu (private %llu, swapped %llu) | history restored=%llu saved=%llu | resets=%llu stale=%llu bank failures=%llu source switches=%llu recreations=%llu",
                   (unsigned long long)(b0 - a0), (unsigned long long)(b1 - a1), (unsigned long long)(b2 - a2), (unsigned long long)(taa::velSplit - vs0), (unsigned long long)(taa::velSwaps - vw0), (unsigned long long)(b3 - a3), (unsigned long long)(b4 - a4), (unsigned long long)(b5 - a5), (unsigned long long)(b6 - a6), (unsigned long long)(b7 - a7), (unsigned long long)taa::srcChanges.load(), (unsigned long long)taa::recreations.load());
              {   // 0.18.7 (BUG-74, owner: "that anti-aliasing change might have been when we tried DLSS"): which anti-aliasing the game runs.
                  // Since 0.11.0 every session shows TAA reset on every frame (restored=0): the game's own TAA is not in use, an upscaler
                  // (DLSS / DLAA / FSR / XeSS, the game's Upscaling setting) does the anti-aliasing with ONE history for both eyes - the
                  // eye drawn first in each pair inherits the other eye's view from a moment earlier (streaks on fast head turns).
                  static u64 r0 = 0, c0 = 0; const u64 r1n = taa::resets.load(), c1n = slCalls[0].load(); u8 *st = rsz::settingsOf(rsz::getApp());
                  const int um = st && readable(st + 0xB0, 8) ? *(int *)(st + 0xB0) : -1, uq = st && readable(st + 0xB0, 8) ? *(int *)(st + 0xB4) : -1;
                  const u64 dr = r1n - r0, dc = c1n - c0; r0 = r1n; c0 = c1n;
                  logf("   AA/s: game TAA resets=%llu | upscaler pass runs=%llu | the game's Upscaling setting: method %d quality %d", (unsigned long long)dr, (unsigned long long)dc, um, uq);
                  static int warned = 0; if (g_cfg.xr == 4 && dr > 40 && !warned) { warned = 1;
                      logf("AA WARNING: the game's TAA is reset every frame -> an upscaler (DLSS / DLAA / FSR / XeSS) is doing the anti-aliasing with one history for both eyes: streaks in one eye on fast head turns. Set the game's Upscaling method to Off / TAA (BUG-74)."); }
              }
              if (taa::dN) { logf("   TAA camera: |current - previous| same-eye %.2f cm, cross-eye %.2f cm; max |same-eye reprojection - engine| %.4f (n=%llu)",
                   taa::dSameSum / taa::dN, taa::dCrossSum / taa::dN, taa::dRepN ? taa::dRepSum / taa::dRepN : -1.0, (unsigned long long)taa::dN);
                   taa::dSameSum = taa::dCrossSum = taa::dRepSum = 0; taa::dN = taa::dRepN = 0; }
              { static u64 q0, q1, q2, q3, q4, q5, q6, q7; u64 x0 = dlss::nTag, x1 = dlss::nConst, x2 = dlss::nEval, x3 = dlss::nEvalDlss, x4 = dlss::nOpt, x5 = dlss::nRemap, x6 = dlss::nConstFake, x7 = dlss::nVelSwap;
                static u64 qn0, qn1; u64 xn0 = dlss::nNgxEval, xn1 = dlss::nNgxRight;
                if (xn0 - qn0) logf("   DLSS(NGX)/s: mode=%d evaluates=%llu right-eye on its own feature=%llu | right-eye feature %p%s | motion-vector swaps=%llu",
                     dlss::mode.load(), (unsigned long long)(xn0 - qn0), (unsigned long long)(xn1 - qn1), dlss::ngxH1.load(), dlss::ngxFailed.load() ? " (create FAILED)" : "", (unsigned long long)(x7 - q7));
                qn0 = xn0; qn1 = xn1;
                if (x2 - q2 || x1 - q1) logf("   DLSS(SL)/s: mode=%d hooked=%d right-eye viewport ready=%d | tags=%llu constants=%llu evaluates=%llu (DLSS %llu) options=%llu | remapped=%llu this-eye constants=%llu motion-vector swaps=%llu",
                     dlss::mode.load(), dlss::hooked.load(), dlss::optsSecond.load(), (unsigned long long)(x0 - q0), (unsigned long long)(x1 - q1), (unsigned long long)(x2 - q2), (unsigned long long)(x3 - q3), (unsigned long long)(x4 - q4), (unsigned long long)(x5 - q5), (unsigned long long)(x6 - q6), (unsigned long long)(x7 - q7));
                q0 = x0; q1 = x1; q2 = x2; q3 = x3; q4 = x4; q5 = x5; q6 = x6; q7 = x7; }
              if (pub::dev) { HRESULT rr = pub::dev->GetDeviceRemovedReason(); static bool told = false; if (rr != S_OK && !told) { told = true; logf("D3D12 DEVICE REMOVED: reason 0x%08lx", (unsigned long)rr); } }
              a0 = b0; a1 = b1; a2 = b2; a3 = b3; a4 = b4; a5 = b5; a6 = b6; a7 = b7; vs0 = taa::velSplit; vw0 = taa::velSwaps; }
            static u64 pmir = 0, pch = 0; u64 mir = pub::mirrored, ch = c_cullHead; logf("   MONITOR/s: left-eye mirror copies=%llu | cull camera follows head=%d (%llu/s)", (unsigned long long)(mir - pmir), m_cullHead.load(), (unsigned long long)(ch - pch)); pmir = mir; pch = ch;
            if (S) { char p0[96], p1[96]; gvRead(&S->inSeq, [&] { memcpy(p0, S->inProfile[0], 96); memcpy(p1, S->inProfile[1], 96); }); p0[95] = p1[95] = 0;
                logf("   INPUT/s: polls %llu, live %llu, stale %llu | snap turns %llu, wheel opened %llu / used %llu, head gestures %llu (HUD %llu) | rumble max %d%% | left %s, right %s",
                     (unsigned long long)vrin::nPolls.exchange(0), (unsigned long long)vrin::nActive.exchange(0), (unsigned long long)vrin::nStale.exchange(0), (unsigned long long)vrin::nSnaps.exchange(0),
                     (unsigned long long)vrin::nWheelOpen.exchange(0), (unsigned long long)vrin::nWheelPick.exchange(0), (unsigned long long)vrin::nGest.exchange(0), (unsigned long long)vrin::nHud.exchange(0), vrin::rumbleMax.exchange(0), p0[0] ? p0 : "-", p1[0] ? p1 : "-"); }
            if (S) logf("   POSE/s: simulated eye %s (Shift+F9) | prediction lead %.1f ms (new pairs reach the display %.1f ms after their prediction) | host GPU priority %ld | render sync %d: waited %llu (%.1f ms total, timeouts %llu), already presented %llu | first person: left/right eye anchor apart by up to %.2f cm before the pair reuse, reused %llu, recomputed %llu",
                        g_eyeSwapLatched ? "RIGHT (swapped)" : "left", S->poseLeadUs / 1000.0, S->poseLagUs / 1000.0, (long)S->hostGpuPrio, g_renderSync, (unsigned long long)g_syncNeeded.exchange(0), g_syncWaitUs.exchange(0) / 1000.0, (unsigned long long)g_syncTimeouts.exchange(0), (unsigned long long)g_syncAlready.exchange(0),
                        (double)fp::lrDiffMax.exchange(0), (unsigned long long)fp::nPairReuse.exchange(0), (unsigned long long)fp::nRecompute.exchange(0));
            logf("   SYNC/s: simulated (left-eye) ticks=%llu frozen (right-eye) ticks=%llu | stereo pairs published=%llu | history preset %s | camera cuts=%llu | cut readers/s VV=%llu VO=%llu L=%llu T1=%llu T2=%llu",
                 (unsigned long long)(sm - psm), (unsigned long long)(fz - pfz), (unsigned long long)(pr - ppr), kPresets[g_preset.load()].name, (unsigned long long)(cu - pcut),
                 (unsigned long long)(u32)(h[0] - ph[0]), (unsigned long long)(u32)(h[1] - ph[1]), (unsigned long long)(u32)(h[2] - ph[2]), (unsigned long long)(u32)(h[3] - ph[3]), (unsigned long long)(u32)(h[4] - ph[4]));
            pfz = fz; psm = sm; pcut = cu; ppr = pr; for (int g = 0; g < cutp::NG; g++) ph[g] = h[g]; } }
        static u64 lastR1 = 0;
        if (g_cfg.census && r1::on.load() && now - lastR1 >= 1000) {
            lastR1 = now;
            static u64 pp = 0, pa = 0; u64 a = r1::posts, b = r1::appendFail;
            logf("   R1-A/s: second graph runs=%llu activations=%llu appendFail=%llu ctorFail=%llu | view+0x18 main=%u v2=%u | pass contexts after main=%u after second=%u | viewconst slots used by run 1=%u | second-run submit: calls=%llu queued=%llu retired=%llu | sched completed/allocated=%d/%d",
                 (unsigned long long)(a - pp), (unsigned long long)r1::activated.load(), (unsigned long long)(b - pa), (unsigned long long)r1::ctorFail.load(), r1::flagMain.load(), r1::flagV2.load(), r1::poolMid.load(), r1::poolAfter.load(), r1::vcBefore.load(), (unsigned long long)sgRange.load(), (unsigned long long)sgQueued.load(), (unsigned long long)sgRetired.load(), *(int *)(g_base + RVA_SCHED + 0x2B5C64), *(int *)(g_base + RVA_SCHED + 0x2B5C60));
            pp = a; pa = b;
            static u64 pl = 0, s0 = 0, s1 = 0; u64 l = r1::lends, c0 = slCalls[0], c1 = slCalls[1];
            logf("   R1 light lists: lend=%d lends/s=%llu skipped=%llu run2-no-pass13=%llu lightDesc=%u tile-count0=%u | pass59 calls/s run1=%llu run2=%llu | F3 skipFrom=%d skipped V2 pass jobs=%llu | F4 dup=%d records duplicated into V2 last frame=%u",
                 r1::lendOn.load(), (unsigned long long)(l - pl), (unsigned long long)r1::lendSkip.load(), (unsigned long long)r1::run2NoFill.load(), r1::lastLightDesc.load(), r1::lastCount0.load(),
                 (unsigned long long)(c0 - s0), (unsigned long long)(c1 - s1), r1::skipFrom.load(), (unsigned long long)r1::pjSkipped.load(), r1::dupOn.load(), r1::bLast[2][0] + r1::bLast[2][1]);
            pl = l; s0 = c0; s1 = c1;
            static u64 pso = 0, pcf = 0, pcm = 0, pmf = 0, pes = 0;
            u64 so = pub::stereoPub, cf = pub::capFail, cm = pub::capMiss, mf = pub::monoFallback, es = r1::eyeSwitches;
            logf("   STEREO/s: eye switches=%llu (drains %llu, drain timeouts %llu) | published both-eye frames=%llu left-capture fails=%llu right-without-left=%llu menu/mono frames=%llu | wrong-buffer drops=%llu | right-eye camera fixes=%llu last eye offset=(%.1f %.1f %.1f) | F1 offset test=%d",
                 (unsigned long long)(es - pes), (unsigned long long)r1::drains.load(), (unsigned long long)r1::drainTimeouts.load(), (unsigned long long)(so - pso), (unsigned long long)(cf - pcf), (unsigned long long)(cm - pcm), (unsigned long long)(mf - pmf), (unsigned long long)pub::idxMiss.load(), (unsigned long long)g_rfixCalls.load(), g_lastDelta[0], g_lastDelta[1], g_lastDelta[2], m_offTest.load());
            pso = so; pcf = cf; pcm = cm; pmf = mf; pes = es;
            if (meter::pairWanted.load()) { meter::Pair pp; meter::latestPair(pp, 0);
                logf("   PAIR: same-frame first render rgb=(%.2f %.2f %.2f) second render rgb=(%.2f %.2f %.2f) mean|diff|=%.2f pairs=%llu drops=%llu",
                     pp.L[0], pp.L[1], pp.L[2], pp.R[0], pp.R[1], pp.R[2], pp.mad, (unsigned long long)meter::nPairs.load(), (unsigned long long)meter::pairDrops.load()); }
        }
        if (S && S->hostState != lastHostState) { lastHostState = S->hostState; logf("host state -> %ld (%s)", lastHostState, S->hostMsg); if (lastHostState == GV_HOST_RUNNING) g_autoRecentreAt = GetTickCount64() + 1500; }
        {   // 0.18.23 the headset's own recenter (hold the Meta button): the host forwards the runtime's reference-space change; 300 ms
            // later (the runtime moves LOCAL at its changeTime, the poses must already be the new ones) the game view recentres like F11
            static LONG seen = -1; static u64 at = 0; const LONG rs = S ? S->hostRecenterSeq : 0;
            if (seen < 0) seen = rs; else if (rs != seen) { seen = rs; at = GetTickCount64() + 300; }
            if (at && GetTickCount64() >= at) { at = 0; nMetaRecenters++; vrin::recenterView(); logf("recentre: the headset's own recenter (Meta button) -> game view recentred"); }
        }
        if (g_autoRecentreAt && GetTickCount64() >= g_autoRecentreAt) { g_autoRecentreAt = 0; m_recenter = 1; if (S) InterlockedIncrement(&S->theaterRecenter); logf("recentre: automatic, headset session started (F11 recentres any time)"); }
        { static bool first3d = false; if (!first3d && g_cfg.xr == 4 && ui::mode.load() == ui::M_HUD && !ui::flat()) { first3d = true; m_recenter = 1; if (S) InterlockedIncrement(&S->theaterRecenter); logf("recentre: automatic, first 3D gameplay after start"); } }
        if (g_hostProc && !hostExitLogged && WaitForSingleObject(g_hostProc, 0) == WAIT_OBJECT_0) { DWORD c = 0; GetExitCodeProcess(g_hostProc, &c); hostExitLogged = true; logf("host process exited (code %lu); headset off, game continues flat. See GoTVR\\host.log", c); }
        if (now - lastCallers >= 10000 || m_burst.exchange(0)) {
            lastCallers = now; reportCallers();
            u64 a = camA();
            if (isCam(a)) { float *w = (float *)(a + 0x140); logf("   camA=%p fwd=(%.4f,%.4f,%.4f) pos=(%.2f,%.2f,%.2f) fov=%.4f near=%.2f", (void *)a, w[0], w[1], w[2], w[12], w[13], w[14], *(float *)(a + 0x670), *(float *)(a + 0x664)); }
            logflush();
        }
    }
    return 0;
}

#ifdef GOTVR_SELFTEST
// Test-only: map the real game exe as an image (no execution), run guards and build all hooks on it.
extern "C" __declspec(dllexport) int GoTVR_SelfTest(const wchar_t *exePath) {
    InitializeCriticalSection(&g_logcs);
    wcscpy(g_dir, L"."); g_log = CreateFileW(L"selftest.log", GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, 0, nullptr);
    g_base = (u8 *)LoadLibraryExW(exePath, nullptr, DONT_RESOLVE_DLL_REFERENCES);
    if (!g_base) { logf("map failed %lu", GetLastError()); return 1; }
    u32 e = *(u32 *)(g_base + 0x3C), ts = *(u32 *)(g_base + e + 8);
    char sha[65]; sha256File(exePath, sha);
    int pass = 0; for (auto &s : kSigs) { bool r = checkSig(s); pass += r; logf("sig %s %s", s.name, r ? "PASS" : "FAIL"); }
    logf("mapped base=%p ts=%u sha=%s %s sigs %d/%d", g_base, ts, sha, strcmp(sha, EXPECTED_SHA) ? "MISMATCH" : "MATCH", pass, (int)(sizeof kSigs / sizeof kSigs[0]));
    if (MH_Initialize() != MH_OK) return 2;
    struct H { u32 rva; void *det; void **orig; const char *name; } hooks[] = {
        {RVA_GAMETICK, (void *)hGameTick, (void **)&oGameTick, "GameTick"}, {RVA_PRESENT, (void *)hPresent, (void **)&oPresent, "Present"},
        {RVA_SETLOCAL, (void *)hSetLocal, (void **)&oSetLocal, "SetLocalMatrix"}, {RVA_SLOT43, (void *)hSlot43, (void **)&oSlot43, "vUpdateFrustum43"},
        {RVA_SETPROJRECT, (void *)hSetProjRect, (void **)&oSetProjRect, "SetProjRect"}, {RVA_FRAMEVIEW, (void *)hFrameView, (void **)&oFrameView, "FrameView"},
        {RVA_RVSUBMIT, (void *)hRVSubmit, (void **)&oRVSubmit, "RV_Submit"},
        {RVA_FV_LIST, (void *)hFvList, (void **)&r1::oFvList, "FrameView_ViewList"}, {RVA_RV_POST, (void *)hRVPost, (void **)&r1::oRVPost, "RV_Post"},
        {RVA_SUBMITGRAPH, (void *)hSubmitGraph, (void **)&oSubmitGraph, "Jobs_SubmitGraph"},
        {RVA_VC_END, (void *)r1::hVcEnd, (void **)&r1::oVcEnd, "VC_FrameEnd"}, {RVA_PASSBODY, (void *)r1::hPassBody, (void **)&r1::oPassBody, "Pass_Body"}, {RVA_CLOCK_UPDATE, (void *)hClock, (void **)&oClock, "Clock_Update"}, {RVA_CAM_TEMPORAL, (void *)hCamT, (void **)&oCamT, "Cam_TemporalUpdate"}, {RVA_CAMS_STOREPREV, (void *)occ::hStorePrev, (void **)&occ::oSP, "Cam_StorePrev"}, {RVA_RV_OCC, (void *)occ::hOcc, (void **)&occ::oOcc, "RV_OcclusionSetup"}, {RVA_AA_RESOLVE, (void *)taa::hRes, (void **)&taa::oRes, "AA_Resolve"}, {RVA_SL_CONSTS, (void *)dlss::hSLC, (void **)&dlss::oSLC, "Cam_BuildSLConstants"}, {RVA_NGX_CREATE, (void *)dlss::hNgxCreate, (void **)&dlss::oNgxCreate, "NGX_CreateFeature"}, {RVA_NGX_EVAL, (void *)dlss::hNgxEval, (void **)&dlss::oNgxEval, "NGX_EvaluateFeature_C"}, {RVA_NGX_RELEASE, (void *)dlss::hNgxRelease, (void **)&dlss::oNgxRelease, "NGX_ReleaseFeature"}, {RVA_AA_VELOCITY, (void *)taa::hVel, (void **)&taa::oVel, "AA_Velocity"}, {RVA_TAA_DRAW, (void *)taa::hDraw, (void **)&taa::oDraw, "TAA_Draw"}, {RVA_RCTX_BUILD, (void *)r1::hRCtxBuild, (void **)&r1::oRCtxBuild, "RCtx_Build"}, {RVA_RCTX_FLUSH, (void *)r1::hRCtxFlush, (void **)&r1::oRCtxFlush, "RCtx_FlushDraws"}, {RVA_SLPASS, (void *)hSLPass, (void **)&oSLPass, "StreamlinePass"},
        {RVA_RT_BIND, (void *)ui::hRT, (void **)&ui::oRT, "RT_Bind"}, {RVA_CTX_SCISSOR, (void *)ui::hSC, (void **)&ui::oSC, "Ctx_SetScissor"}, {RVA_MOVIE_DRAW, (void *)ui::hMovie, (void **)&ui::oMovie, "Movie_Draw"}, {RVA_PASS65_PRE, (void *)ui::hP65, (void **)&ui::oP65, "Pass65_pre (fade)"}, {RVA_BATCH_DRAW, (void *)ui::hBatch, (void **)&ui::oBatch, "Batch_Draw"},
        {RVA_UI60_PRE, (void *)ui::hUI60, (void **)&ui::oUI60, "UI_Pass60_pre (clears the UI image)"}, {RVA_CTX_FLUSHRT, (void *)ui::hFlushRT, (void **)&ui::oFlushRT, "Ctx_FlushRT (UI layer redirect)"},
        {RVA_SC_COMPUTESIZES, (void *)rsz::hCS, (void **)&rsz::oCS, "SC_ComputeSizes (headset render size)"},
        {RVA_WND_PUMP, (void *)rsz::hPump, (void **)&rsz::oPump, "GameWindow_Pump (F2 size change on the main thread)"},
        {RVA_SC_RECREATE, (void *)pub::hRecreate, (void **)&pub::oRecreate, "SC_Recreate (swapchain re-creation)"},
        {RVA_ANIM_JOB, (void *)fp::hAnimJob, (void **)&fp::oAnimJob, "Anim_Job (first-person head hiding in the render pose)"},
        {RVA_OCC_VIEWFN, (void *)occ::hOccView, (void **)&occ::oOccView, "Occ_View (foliage culling gets the engine view)"},
    };
    int bad = 0;
    for (auto &h : hooks) {
        MH_STATUS st = MH_CreateHook(g_base + h.rva, h.det, h.orig); bad += st != MH_OK;
        u8 *tr = (u8 *)*h.orig; char hex[64] = ""; if (tr) for (int i = 0; i < 16; i++) sprintf(hex + 3 * i, "%02X ", tr[i]);
        logf("hook %s create=%s trampoline=%p [%s]", h.name, MH_StatusToString(st), tr, hex);
    }
    MH_STATUS en = MH_EnableHook(MH_ALL_HOOKS); logf("enable=%s", MH_StatusToString(en));
    for (auto &h : hooks) { u8 *t = g_base + h.rva; logf("patched %s: %02X %02X %02X %02X %02X", h.name, t[0], t[1], t[2], t[3], t[4]); }
    MH_STATUS dis = MH_DisableHook(MH_ALL_HOOKS); logf("disable=%s", MH_StatusToString(dis));
    // BUG-16 regression test: pass-table masking on the mapped image, including the 0.7.5 race order
    bool tabTest = r1::capturePassTable();
    if (tabTest) {
        u64 *t = (u64 *)(g_base + RVA_PASSTAB); auto same = [&] { return memcmp(t, r1::pristine, sizeof r1::pristine) == 0; };
        u64 s0 = 0, s1 = 0; for (int i = 14; i < NPASS; i++) r1::setBit(s0, s1, i);
        r1::maskSecondRun(s0, s1, true);
        bool masked = true; for (int i = 14; i < NPASS; i++) masked &= (t[i * 4] == (r1::pristine[i][0] ? r1::nopPtr() : 0)) && (t[i * 4 + 3] == (r1::pristine[i][3] ? r1::nopPtr() : 0));
        for (int i = 0; i < 14; i++) masked &= t[i * 4] == r1::pristine[i][0] && t[i * 4 + 3] == r1::pristine[i][3];
        r1::maskSecondRun(s0, s1, false); bool r1ok = same();
        r1::maskSecondRun(0, 0, true); r1::maskSecondRun(s0, s1, false); bool raceOk = same();          // 0.7.5 race order
        u64 b0 = 0, b1 = 0; for (int i = 20; i < 26; i++) r1::setBit(b0, b1, i); r1::setBit(b0, b1, 66);
        r1::maskSecondRun(b0, b1, true);
        bool blockOk = true; for (int i = 0; i < NPASS; i++) { bool want = (i >= 20 && i < 26) || i == 66; blockOk &= want ? (t[i * 4] == (r1::pristine[i][0] ? r1::nopPtr() : 0) && t[i * 4 + 3] == (r1::pristine[i][3] ? r1::nopPtr() : 0)) : (t[i * 4] == r1::pristine[i][0] && t[i * 4 + 3] == r1::pristine[i][3]); }
        r1::maskSecondRun(b0, b1, false); blockOk &= same();
        r1::maskSecondRun(s0, s1, true); t[40 * 4] = 0; r1::checkPassTable(); bool healOk = same();      // self-repair
        tabTest = masked && r1ok && raceOk && healOk && blockOk;
        logf("pass-table test: mask=%d restore=%d race-order=%d block-mask=%d self-repair=%d -> %s", masked, r1ok, raceOk, blockOk, healOk, tabTest ? "PASS" : "FAIL");
    }
    bool restored = true; for (auto &s : kSigs) restored &= checkSig(s); logf("signatures after disable: %s", restored ? "restored" : "NOT RESTORED");
    bool occOk = false;
    {   // BUG-26 per-eye occlusion test: steady-state view, slot p source -> slot (1-p) + this eye's banked camera
        static u8 view[0x200]; memset(view, 0, sizeof view);
        int *par = (int *)(g_base + RVA_PARITY); u8 *en = g_base + RVA_OCC_ENABLE; u32 *dims = (u32 *)(g_base + RVA_OCC_DIMS);
        DWORD op; VirtualProtect(par, 4, PAGE_READWRITE, &op); VirtualProtect(en, 1, PAGE_READWRITE, &op); VirtualProtect(dims, 0x40, PAGE_READWRITE, &op);
        const int sp = *par; const u8 se = *en; const u32 sd0 = dims[0], sd1 = *(u32 *)((u8 *)dims + 0x34);
        *par = 1; *en = 1; dims[0] = (720u << 16) | 1280u; *(u32 *)((u8 *)dims + 0x34) = (720u << 16) | 1280u;
        *(u8 **)(view + 0x110) = g_base + RVA_OCC_SLOTS + 0x20; view[0x120] = 0;
        occ::oOcc = [](u8 *) {}; const int sx = g_cfg.xr; g_cfg.xr = 4; occ::on = 1; g_camEye = 1;
        occ::Bank &bk = occ::bank[1]; bk.ok = true; for (int i = 0; i < 16; i++) bk.vp[i] = (float)(i + 1); for (int i = 0; i < 4; i++) bk.pos[i] = 100.0f + i;
        occ::hOcc(view);
        const float *o = (const float *)(view + 0x60);
        occOk = *(u8 **)(view + 0x110) == g_base + RVA_OCC_SLOTS && occ::fixed.load() == 1 &&
                fabsf(o[0] - 640.0f * (1 + 4)) < 1e-3f && fabsf(o[1] - (-360.0f * 2 + 360.0f * 4)) < 1e-3f && o[2] == 3 && o[3] == 4 &&
                ((float *)(view + 0xA0))[0] == 100.0f && ((float *)(view + 0xE0))[3] == 16.0f && ((float *)(view + 0xB0))[0] == 1.0f;
        view[0x120] = 1; *(u8 **)(view + 0x110) = g_base + RVA_OCC_SLOTS + 0x20; occ::hOcc(view);   // first frame after a cut: untouched
        occOk &= *(u8 **)(view + 0x110) == g_base + RVA_OCC_SLOTS + 0x20 && occ::fixed.load() == 1;
        {   // 0.18.9 tracked slot ownership: an eye tests only against a slot its own last frame drew, with that frame's camera
            static u8 fcam[0x700]; u8 **pc = (u8 **)(g_base + RVA_CAMA); DWORD op2; VirtualProtect(pc, 8, PAGE_READWRITE, &op2); u8 *const scam = *pc; *pc = fcam;
            const bool sfo = g_stFrameOn; const u32 sfr = g_stFrame; g_stFrameOn = true; occ::on = 2; occ::owner[0].ok = occ::owner[1].ok = false; occ::fixed = 0; occ::trkOwn = 0; occ::trkEngine = 0;
            auto frame = [&](u32 f, int eye, int par2, float tag) { g_stFrame = f; g_camEye = eye; *par = par2; for (int i = 0; i < 16; i++) ((float *)(fcam + 0x430))[i] = tag + i; ((float *)(fcam + 0x550))[0] = tag;
                view[0x120] = 0; *(u8 **)(view + 0x110) = g_base + RVA_OCC_SLOTS + par2 * 0x20; memset(view + 0xA0, 0, 16); occ::hOcc(view); };
            bool tk = true;
            frame(10, 0, 0, 100.0f); tk &= occ::trkEngine.load() == 1 && *(u8 **)(view + 0x110) == g_base + RVA_OCC_SLOTS;                 // nothing of its own yet: engine
            frame(11, 1, 1, 200.0f); tk &= occ::trkEngine.load() == 2;                                                                    // the left eye's slot is not the right's
            frame(12, 0, 0, 300.0f); tk &= *(u8 **)(view + 0x110) == g_base + RVA_OCC_SLOTS + 0x20 && ((float *)(view + 0xA0))[0] == 100.0f && occ::trkOwn.load() == 1;   // its own (frame 10, slot 1) with frame 10's camera
            frame(13, 1, 0, 400.0f); tk &= *(u8 **)(view + 0x110) == g_base + RVA_OCC_SLOTS && ((float *)(view + 0xA0))[0] == 200.0f && occ::trkOwn.load() == 2;            // parity did not toggle: still its own slot 0 (frame 11)
            frame(14, 0, 1, 500.0f); tk &= occ::trkEngine.load() == 3 && *(u8 **)(view + 0x110) == g_base + RVA_OCC_SLOTS + 0x20;          // its slot was overwritten by the other eye: engine pairing
            occOk &= tk; if (!tk) logf("   tracked occlusion: FAIL (own %llu engine %llu)", (unsigned long long)occ::trkOwn.load(), (unsigned long long)occ::trkEngine.load());
            *pc = scam; g_stFrameOn = sfo; g_stFrame = sfr; g_camEye = -1; occ::owner[0].ok = occ::owner[1].ok = false; occ::trkOwn = 0; occ::trkEngine = 0;
        }
        {   // 0.18.11 preset 7: per-eye frustum culling - the setup is not touched and every Occ_View caller gets no occlusion view
            occ::on = 3; g_camEye = 1; *par = 1; occ::fixed = 0; view[0x120] = 0; *(u8 **)(view + 0x110) = g_base + RVA_OCC_SLOTS + 0x20; memset(view + 0xA0, 0x5A, 16);
            occ::hOcc(view); bool m3 = occ::fixed.load() == 0 && *(u8 **)(view + 0x110) == g_base + RVA_OCC_SLOTS + 0x20 && view[0xA0] == 0x5A && view[0xAF] == 0x5A;
            const int svm = occ::viewMode.load(); occ::viewMode = 1; const u64 vn = occ::viewNull.load();
            m3 &= occ::hOccView(g_base + RVA_OCC_VIEW) == nullptr && occ::viewNull.load() == vn + 1;   // even with the engine-view setting
            occ::viewMode = svm; g_camEye = -1; occOk &= m3; if (!m3) logf("   per-eye frustum culling (mode 3): FAIL");
        }
        *par = sp; *en = se; dims[0] = sd0; *(u32 *)((u8 *)dims + 0x34) = sd1; g_cfg.xr = sx; occ::on = 0; occ::bank[1].ok = false; occ::fixed = 0;
        logf("per-eye occlusion test (0.9.4 bank, tracked slots, preset 7 per-eye frustum culling): %s", occOk ? "PASS" : "FAIL");
    }
    bool fastOk = false;
    {   // BUG-36: fast-turn detection between this eye's previous world and the current one
        static u8 cam[0x900]; memset(cam, 0, sizeof cam); occ::Bank &bk = occ::bank[0]; memset(&bk, 0, sizeof bk); bk.ok = true;
        float *pw = (float *)(bk.w180 + 0x40); pw[0] = 1; pw[5] = 1; pw[10] = 1;
        auto setYaw = [&](float deg) { float a = deg / 57.2957795f, c = cosf(a), sn = sinf(a); float *w = (float *)(cam + 0x140); memset(w, 0, 64); w[0] = c; w[1] = sn; w[4] = -sn; w[5] = c; w[10] = 1; };
        const int sx = g_cfg.xr; g_cfg.xr = 4; occ::on = 1; g_pairPhase = 1; occ::fastDeg = 2.0f; occ::fastDegLeft = 0.6f; occ::bank[1] = bk;
        setYaw(1.0f); occ::fastCheck(cam); bool slow = !occ::fast.load() && fabsf(occ::lastAngle - 1.0f) < 0.05f;
        setYaw(5.0f); occ::fastCheck(cam); bool quick = occ::fast.load() && fabsf(occ::lastAngle - 5.0f) < 0.05f;
        setYaw(0.5f); occ::fastCheck(cam); bool back = !occ::fast.load();
        g_pairPhase = 0; setYaw(1.0f); occ::fastCheck(cam); const bool leftLow = occ::fast.load();   // the left eye: 1 deg is already a gap
        setYaw(0.3f); occ::fastCheck(cam); const bool leftBack = !occ::fast.load();
        occ::on = 3; g_pairPhase = 1; setYaw(5.0f); occ::fastCheck(cam); const bool noCut3 = !occ::fast.load();   // preset 7: never a fast-turn cut
        occ::bank[1].ok = false; g_pairPhase = 0;
        fastOk = slow && quick && back && leftLow && leftBack && noCut3;
        g_cfg.xr = sx; occ::on = 0; bk.ok = false; occ::fastTicks = 0;
        logf("fast-turn detection test: right eye 1 deg slow=%d, 5 deg fast=%d, back=%d; left eye 1 deg fast=%d, 0.3 deg back=%d; preset 7 never cuts=%d -> %s", slow, quick, back, leftLow, leftBack, noCut3, fastOk ? "PASS" : "FAIL");
    }
    bool dlssOk = false;
    {   // BUG-37: right-eye frames go to viewport value+OFF in slEvaluateFeature / slSetTag / slSetConstants; left untouched
        static u32 seenV; static bool seenOrig;
        static dlss::VP vp{}; memset(&vp, 0, sizeof vp); memcpy(vp.type, dlss::kVpGuid, 16); vp.ver = 1; vp.value = 0;
        dlss::oEval = [](u32, const void *, const void **in, u32 n, void *) { seenV = 0xFFFFFFFF; for (u32 i = 0; i < n; i++) { const dlss::VP *x = (const dlss::VP *)in[i]; if (!memcmp(x->type, dlss::kVpGuid, 16)) { seenV = x->value; seenOrig = x == &vp; } } return 0; };
        dlss::oSetTag = [](const void *v, const void *, u32, void *) { seenV = ((const dlss::VP *)v)->value; return 0; };
        const int sx = g_cfg.xr; g_cfg.xr = 4; dlss::mode = 1; dlss::optsSecond = true;
        static u8 other[64]; memset(other, 0x5A, sizeof other); const void *in[2] = {other, &vp};
        g_camEye = 1; dlss::hEval(0, nullptr, in, 2, nullptr); bool r1 = seenV == dlss::OFF && !seenOrig && vp.value == 0;
        g_camEye = 0; dlss::hEval(0, nullptr, in, 2, nullptr); bool l1 = seenV == 0 && seenOrig;
        g_camEye = 1; dlss::hSetTag(&vp, nullptr, 0, nullptr); bool r2 = seenV == dlss::OFF;
        dlss::optsSecond = false; dlss::hSetTag(&vp, nullptr, 0, nullptr); bool fc = seenV == 0;   // no options on viewport 1 yet: no remap
        // BUG-39 NGX: right-eye evaluates of the game's DLSS feature go to a second feature created on first use
        static void *seenH; static int creates;
        dlss::oNgxCreate = [](void *, u32 f, void *, void **out) -> u32 { creates++; *out = (void *)(uintptr_t)(0x1000 + creates); return f == 1 ? 1u : 0xBAD00001u; };
        dlss::oNgxEval = [](void *, void *h, void *, void *) -> u32 { seenH = h; return 1u; };
        dlss::oNgxRelease = [](void *) -> u32 { return 1u; };
        void *h0 = nullptr; dlss::hNgxCreate(nullptr, 1, nullptr, &h0);
        g_camEye = 0; dlss::hNgxEval(nullptr, h0, nullptr, nullptr); bool n1 = seenH == h0;
        g_camEye = 1; dlss::hNgxEval(nullptr, h0, nullptr, nullptr); bool n2 = seenH != h0 && seenH == dlss::ngxH1.load() && creates == 2;
        dlss::hNgxEval(nullptr, h0, nullptr, nullptr); bool n3 = seenH == dlss::ngxH1.load() && creates == 2;   // created once
        dlss::mode = 0; dlss::hNgxEval(nullptr, h0, nullptr, nullptr); bool n4 = seenH == h0; dlss::mode = 1;   // F9 off
        dlss::hNgxRelease(h0); bool n5 = dlss::ngxH0.load() == nullptr && dlss::ngxH1.load() == nullptr;
        logf("per-eye DLSS (NGX) test: left own=%d right second=%d once=%d off=%d release=%d", n1, n2, n3, n4, n5);
        dlssOk = r1 && l1 && r2 && fc && n1 && n2 && n3 && n4 && n5;
        g_cfg.xr = sx; dlss::mode = 1; dlss::optsSecond = false; g_camEye = -1;
        logf("per-eye DLSS viewport test: right->%u left->0 tag-remap fail-closed: %d %d %d %d -> %s", dlss::OFF, r1, l1, r2, fc, dlssOk ? "PASS" : "FAIL");
    }
    bool taaOk = false;
    {   // BUG-27: TAA_Draw / AA_Velocity receive a camera copy with this eye's reprojection; stale identity -> live camera
        static u8 cam[0x900]; memset(cam, 0, sizeof cam); for (int i = 0; i < 4; i++) ((float *)(cam + 0x550))[i] = 1.5f + i;
        for (int i = 0; i < 0xC0; i++) cam[0x2C0 + i] = 0x11; for (int i = 0; i < 0x30; i++) cam[0x400 + i] = 0x22; cam[0x6B7] = 0x5A;
        u8 **ca = (u8 **)(g_base + RVA_CAMA); DWORD op; VirtualProtect(ca, 8, PAGE_READWRITE, &op); u8 *sca = *ca; *ca = cam;
        const int sx = g_cfg.xr; g_cfg.xr = 4; taa::on = 7;
        taa::Rep r{}; r.ok = true; r.eye = 1; memcpy(r.pos, cam + 0x550, 16); memset(r.r2c0, 0x77, 0xC0); memset(r.r400, 0x88, 0x30);
        EnterCriticalSection(&taa::repcs); taa::rep = r; LeaveCriticalSection(&taa::repcs);
        static u8 *seen; static bool okv; seen = nullptr; okv = false;
        taa::oVel = [](u8 *, u8 *, u8 *c) { seen = c; okv = c[0x2C0] == 0x77 && c[0x37F] == 0x77 && c[0x400] == 0x88 && c[0x42F] == 0x88 && c[0x6B7] == 0x5A && c[0x2BF] == 0 && c[0x430] == 0; };
        seen = nullptr; taa::hVel(nullptr, nullptr, cam); bool a1 = seen == cam;   // no velocity resource: the pass sees the live camera only
        { u8 *f = taa::fakeCam(cam, r); okv = f[0x2C0] == 0x77 && f[0x37F] == 0x77 && f[0x400] == 0x88 && f[0x42F] == 0x88 && f[0x6B7] == 0x5A && f[0x2BF] == 0 && f[0x430] == 0; a1 &= okv; }
        taa::oDraw = [](u64, u8 *, u8 *c, void *, void *, u64, u32, float) { seen = c; okv = c[0x340] == 0x77 && c[0x37C] == 0x77; };
        taa::hDraw(0, nullptr, cam, nullptr, nullptr, 0, 0, 1.0f); bool a2 = seen == cam;   // history not restored -> engine chain untouched
        taa::selfTestDraw = true; taa::hDraw(0, nullptr, cam, nullptr, nullptr, 0, 0, 1.0f); a2 &= okv && seen != cam;
        ((float *)(cam + 0x550))[0] = 9.0f; seen = nullptr;   // camera moved on: rep no longer belongs to it
        taa::hDraw(0, nullptr, cam, nullptr, nullptr, 0, 0, 1.0f); bool a3 = seen == cam; taa::selfTestDraw = false;
        // BUG-30: RT handle -> *(+0x10) resource record -> (+0x38 < 0: +0x20 object) -> object+8 native
        static u8 rt[0x90], rr[0x80], ob[0x10]; memset(rt, 0, sizeof rt); memset(rr, 0, sizeof rr);
        *(u8 **)(rt + 0x10) = rr; *(int *)(rr + 0x38) = -1; *(u8 **)(rr + 0x20) = ob; *(void **)(ob + 8) = (void *)0x1234560;
        bool a4 = taa::nativeOf(rt) == (ID3D12Resource *)0x1234560 && taa::nativeOf(rr) != (ID3D12Resource *)0x1234560;
        taaOk = a1 && a2 && a3 && a4 && taa::stale.load() == 1;
        *ca = sca; g_cfg.xr = sx; taa::on = 0; taa::rep.ok = false; taa::stale = 0; taa::drawFix = 0; taa::velFix = 0;
        logf("per-eye TAA camera test: velocity=%d draw=%d stale-fallback=%d rt->resource=%d -> %s", a1, a2, a3, a4, taaOk ? "PASS" : "FAIL");
    }
    bool cutOk = true;
    {   // BUG-23 cut-override test: site bytes, stub semantics executed for real, patch + restore on the mapped image
        for (auto &st : cutp::kSites) { bool ok = cutp::sigOk(g_base, st); cutOk &= ok; logf("cut site 0x%X %s: %s", st.rva, st.name, ok ? "PASS" : "FAIL"); }
        cutp::pg = cutp::allocNear(g_base); cutOk &= cutp::pg != nullptr;
        u8 *tb = (u8 *)VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
        static u8 fake[0x800];
        for (int i = 0; cutp::pg && tb && i < cutp::NS; i++) {
            const cutp::Site &st = cutp::kSites[i];
            for (int cut = 0; cut < 2; cut++) for (int force = 0; force < 2; force++) {
                // thunk: save non-volatiles; base = fake; cmpReg = 0; jmp stub; back: setne al; movzx eax,al; restore; ret
                int n = 0; u8 *c = tb; const u8 sv[8] = {0x53, 0x56, 0x57, 0x55, 0x41, 0x54, 0x41, 0x55};   // push rbx rsi rdi rbp r12 r13
                memcpy(c, sv, 8); n = 8; c[n++] = 0x41; c[n++] = 0x56; c[n++] = 0x41; c[n++] = 0x57;              // push r14 r15
                c[n++] = (u8)(0x48 | (st.base >= 8 ? 1 : 0)); c[n++] = (u8)(0xB8 | (st.base & 7)); u64 fa = (u64)fake; memcpy(c + n, &fa, 8); n += 8;   // mov base, imm64
                if (st.cmpReg >= 0) { u8 r = (u8)st.cmpReg; if (r >= 8) c[n++] = 0x45; c[n++] = 0x31; c[n++] = (u8)(0xC0 | ((r & 7) << 3) | (r & 7)); }   // xor r32,r32
                int jmpAt = n; c[n++] = 0xFF; c[n++] = 0x25; u32 z = 0; memcpy(c + n, &z, 4); n += 4; u64 sa = (u64)cutp::pg->code[i]; memcpy(c + n, &sa, 8); n += 8; (void)jmpAt;
                u8 *back = c + n;
                c[n++] = 0x0F; c[n++] = 0x95; c[n++] = 0xC0; c[n++] = 0x0F; c[n++] = 0xB6; c[n++] = 0xC0;   // setne al; movzx eax,al
                c[n++] = 0x41; c[n++] = 0x5F; c[n++] = 0x41; c[n++] = 0x5E; c[n++] = 0x41; c[n++] = 0x5D; c[n++] = 0x41; c[n++] = 0x5C; c[n++] = 0x5D; c[n++] = 0x5F; c[n++] = 0x5E; c[n++] = 0x5B; c[n++] = 0xC3;
                cutp::emit(cutp::pg->code[i], st, i, (u64)back); FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
                *(int *)(fake + 0x7D8) = cut; cutp::setForce(st.group, force);
                int r = ((int (*)())tb)(); int want = cut | force;
                if (r != want) { cutOk = false; logf("cut stub %d (0x%X) cut=%d force=%d -> %d, expected %d FAIL", i, st.rva, cut, force, r, want); }
            }
        }
        logf("cut stubs executed (%d sites x 4 cases), hit counters %ld..", cutp::NS, cutp::pg ? (long)cutp::pg->hits[0] : -1L);
        if (cutp::pg) for (int g = 0; g < cutp::NG; g++) cutp::setForce(g, 0);
        // real patch on the mapped image, then restore
        if (cutp::pg) { VirtualFree(cutp::pg, 0, MEM_RELEASE); cutp::pg = nullptr; }
        bool ap = cutp::apply(); bool jm = true; for (auto &st : cutp::kSites) jm &= g_base[st.rva] == 0xE9;
        cutp::remove(); bool back = true; for (auto &st : cutp::kSites) back &= cutp::sigOk(g_base, st);
        cutOk &= ap && jm && back;
        logf("cut override test: stubs %s, apply=%d jumps=%d restore=%d -> %s", cutOk ? "ok" : "BAD", ap, jm, back, cutOk ? "PASS" : "FAIL");
    }
    // F2 harness simulation: fake meter; image dark (L=40) when F5 is on unless pass 50 is skipped in run 2 (L=60);
    // F5 off = 60. Noise +-0.3. Expect phase 1 -> block 47-52, phase 2 -> pass 50, verify score ~1.
    bool simOk = false;
    {
        InitializeCriticalSection(&meter::lcs); InitializeCriticalSection(&meter::pcs); meter::ok = true;
        u64 now = 1000000, pres = 0; unsigned seed = 1; harness::start(now, pres);
        for (int it = 0; it < 400000 && harness::running.load(); it++) {
            now += 10; pres += 1; if (r1::on.load()) r1::posts++;
            u64 m0 = r1::autoM0.load() | r1::fixM0.load(), m1 = r1::autoM1.load() | r1::fixM1.load();
            double L = !r1::on.load() ? 60 : (r1::inMask(m0, m1, 50) ? 60 : 40);
            seed = seed * 1103515245 + 12345; L += ((seed >> 16) % 600) / 1000.0 - 0.3;
            if (it % 10 == 0) { EnterCriticalSection(&meter::lcs); meter::last = {L, L, L, L, now}; LeaveCriticalSection(&meter::lcs); }
            harness::tick(now, pres);
        }
        simOk = !harness::running.load() && r1::fixM0.load() == (1ull << 50) && r1::fixM1.load() == 0;
        logf("F2 harness simulation: %s (fix mask %016llx %016llx)", simOk ? "PASS" : "FAIL", (unsigned long long)r1::fixM0.load(), (unsigned long long)r1::fixM1.load());
        r1::fixM0 = 0; r1::fixM1 = 0; r1::on = 0; meter::ok = false;
    }
    bool sweepOk = false;
    {   // F2 sweep simulation: same-frame difference (3,7,13) unless pass 33 (the planted cause) or any pass >= 59 is skipped
        r1::tabOk = true; meter::ok = true; u64 now = 5000000; unsigned seed = 7; g_fixCulprit = -1;
        sweep::begin(now);
        for (int it = 0; it < 200000 && sweep::running.load(); it++) {
            now += 10;
            u64 m0 = r1::autoM0.load() | r1::fixM0.load(), m1 = r1::autoM1.load() | r1::fixM1.load();
            bool gone = r1::inMask(m0, m1, 33); for (int i = 59; i < NPASS; i++) gone |= r1::inMask(m0, m1, i);
            if (it % 8 == 0) {
                seed = seed * 1103515245 + 12345; double nz = ((seed >> 16) % 400) / 1000.0 - 0.2;
                meter::Pair p{}; p.L[0] = 14 + nz; p.L[1] = 21; p.L[2] = 25; p.R[0] = p.L[0] + (gone ? 0 : 3) + nz; p.R[1] = 21 + (gone ? 0 : 7); p.R[2] = 25 + (gone ? 0 : 13) - nz; p.t = now;
                EnterCriticalSection(&meter::pcs); meter::lastPair = p; LeaveCriticalSection(&meter::pcs);
            }
            sweep::tick(now);
        }
        sweepOk = !sweep::running.load() && g_fixCulprit == 33 && r1::fixM0.load() == (1ull << 33);
        logf("F2 sweep simulation: %s (culprit %d)", sweepOk ? "PASS" : "FAIL", g_fixCulprit);
        r1::fixM0 = 0; r1::fixM1 = 0; r1::on = 0; meter::ok = false;
    }
    bool rsOk = false;
    {   // 0.16.0 render size hook: windowed -> the engine sees W x H and aspect auto during the call, settings restored after;
        // fullscreen -> untouched; size 0 -> untouched
        static u8 sc[0x300], st[0x100]; static u32 seenW, seenH; static int seenA;
        rsz::oCS = [](u8 *c, u8 *t) -> u64 { seenW = *(u32 *)(t + 0x10); seenH = *(u32 *)(t + 0x14); seenA = *(int *)(t + 0x28); *(int *)(c + 0x160) = (int)seenW; *(int *)(c + 0x164) = (int)seenH; return 1; };
        memset(st, 0, sizeof st); *(u32 *)(st + 0x10) = 2560; *(u32 *)(st + 0x14) = 1600; *(int *)(st + 0x28) = 3;
        rsz::want = 2048; rsz::hCS(sc, st); rsOk = seenW == 2048 && seenH == 2048 && seenA == 0 && *(u32 *)(st + 0x10) == 2560 && *(u32 *)(st + 0x14) == 1600 && *(int *)(st + 0x28) == 3 && *(int *)(sc + 0x160) == 2048;
        st[0] = 1; rsz::hCS(sc, st); rsOk &= seenW == 2560 && seenH == 1600 && seenA == 3; st[0] = 0;
        rsz::want = 0; rsz::hCS(sc, st); rsOk &= seenW == 2560 && seenA == 3;
        logf("render size hook test (windowed override + restore, fullscreen untouched, off): %s", rsOk ? "PASS" : "FAIL");
        // 0.16.2 (BUG-64): the square render tells the game 16:9 (SC+0x208, UI canvas + flat camera) -> anamorphic images
        const float ua = g_uiAspect; g_uiAspect = 16.0f / 9.0f; *(float *)(sc + 0x208) = 1.0f;
        rsz::want = 2048; rsz::hCS(sc, st); bool an = fabsf(*(float *)(sc + 0x208) - 16.0f / 9.0f) < 1e-5f && fabsf(rsz::ana.load() - 16.0f / 9.0f) < 1e-5f;
        st[0] = 1; *(float *)(sc + 0x208) = 1.6f; rsz::hCS(sc, st); an &= rsz::ana.load() == 0.0f && *(float *)(sc + 0x208) == 1.6f; st[0] = 0;
        g_uiAspect = 0.0f; *(float *)(sc + 0x208) = 1.0f; rsz::hCS(sc, st); an &= rsz::ana.load() == 0.0f && *(float *)(sc + 0x208) == 1.0f;
        g_uiAspect = 16.0f / 9.0f; rsz::hCS(sc, st); float p2[4]; ui::View vv{true, 0, -1, 1, 1, -1, 0};
        an &= ui::panel(vv, 2048, 2048, 90, p2) && fabsf((p2[3] - p2[1]) - 2048.0f * 9.0f / 16.0f) < 0.5f;   // panel at 16:9 in the square eye
        rsz::want = 0; rsz::hCS(sc, st); an &= rsz::ana.load() == 0.0f; g_uiAspect = ua;
        logf("anamorphic UI aspect test (square render laid out for 16:9, fullscreen / ui_aspect=0 / off untouched, panel 16:9): %s", an ? "PASS" : "FAIL");
        rsOk &= an;
        rsz::oCS = nullptr; rsz::lastW = 0; rsz::lastH = 0;
    }
    {   // 0.16.3 (BUG-64c): F2 is applied on the main thread right after the game's message pump, through ApplyDisplay with the
        // current settings; fullscreen: not applied; nothing pending: the pump only
        static u8 st[0x200]; static void *vt[64]; static u8 *app[2]; static int nApply = 0, nPump = 0; static bool sawApplying = false, sameCopy = false;
        for (int i = 0; i < 0x148; i++) st[i] = (u8)(i * 7 + 1); st[0] = 0;
        vt[0x118 / 8] = (void *)(u8 *(*)(u8 *))[](u8 *) -> u8 * { return st; }; app[0] = (u8 *)vt;
        const auto sg = rsz::getApp; const auto sa = rsz::apply; const auto sp = rsz::oPump;
        rsz::getApp = []() -> u8 * { return (u8 *)app; };
        rsz::apply = [](u8 *a, u8 *c) -> char { nApply++; sawApplying = rsz::applying.load() == 1; sameCopy = a == (u8 *)app && c != st && !memcmp(c, st, 0x148); return 1; };
        rsz::oPump = [](u8 *, float) -> char { nPump++; return 1; };
        bool ok = rsz::windowedNow();
        rsz::hPump(nullptr, 0.016f); ok &= nPump == 1 && nApply == 0;                 // nothing pending
        rsz::request(); rsz::hPump(nullptr, 0.016f); ok &= nPump == 2 && nApply == 1 && sawApplying && sameCopy && rsz::applying.load() == 0;
        rsz::hPump(nullptr, 0.016f); ok &= nApply == 1;                                // once per request
        st[0] = 1; rsz::request(); ok &= rsz::pending.load() == 0;                     // fullscreen: refused at the key
        rsz::pending = 1; rsz::hPump(nullptr, 0.016f); ok &= nApply == 1; st[0] = 0;   // ...and at the pump
        {   // 0.18.6 motion blur held at 0 in VR (the same settings block, +0x64), the user's value back at exit
            const int sx = g_cfg.xr; g_cfg.xr = 4; *(float *)(st + 0x64) = 0.75f; mblur::tick(true); bool mb = *(float *)(st + 0x64) == 0.0f && mblur::forced.load() == 1;
            mblur::restore(); mb &= *(float *)(st + 0x64) == 0.75f;
            g_mblurOff = 0; mblur::tick(true); mb &= *(float *)(st + 0x64) == 0.75f; g_mblurOff = 1;   // motion_blur_off = 0: untouched
            g_cfg.xr = 0; mblur::tick(true); mb &= *(float *)(st + 0x64) == 0.75f;                        // not in VR: untouched
            g_cfg.xr = sx; mblur::orig = -1.0f; mblur::st0 = nullptr; mblur::forced = 0; *(float *)(st + 0x64) = 0;
            ok &= mb; logf("motion blur test (held at 0 in VR, restored at exit, untouched when off / not in VR): %s", mb ? "PASS" : "FAIL");
            {   // 0.18.24 first-person AO fix through the game's own apply: AO 0 + SSS >= 1 applied once, the menu's later choice remembered, the
                // player's values applied back when first person ends, unknown values / fp_ao_off = 0 untouched
                static u8 *stP; static int nAp; stP = st; nAp = 0; const auto sa2 = rsz::apply;
                rsz::apply = [](u8 *, u8 *c) -> char { nAp++; memcpy(stP, c, 0x148); return 1; };   // ApplyDisplay copies the new settings in
                const int sx2 = g_cfg.xr, so = fp::on.load(); g_cfg.xr = 4; fp::on = 1; *(int *)(st + 0x98) = 0; *(int *)(st + 0x60) = 2; shade::forced = false; shade::oA = shade::oS = -1;
                shade::mode = 1; shade::tick(true); bool sh = *(int *)(st + 0x60) == 0 && *(int *)(st + 0x98) == 1 && nAp == 1;
                shade::tick(true); sh &= nAp == 1;                                                                 // clean: no second apply
                *(int *)(st + 0x60) = 3; shade::tick(true); sh &= *(int *)(st + 0x60) == 0 && nAp == 2;           // the player picks 3 in the menu: off again
                fp::on = 0; shade::tick(true); sh &= *(int *)(st + 0x60) == 3 && *(int *)(st + 0x98) == 0 && nAp == 3;   // first person off: the player's own
                shade::tick(true); sh &= nAp == 3;
                fp::on = 1; *(int *)(st + 0x60) = 77; shade::tick(true); sh &= *(int *)(st + 0x60) == 77 && nAp == 3;   // not a setting we know
                shade::mode = 0; *(int *)(st + 0x60) = 2; shade::tick(true); sh &= *(int *)(st + 0x60) == 2 && nAp == 3;   // fp_ao_off = 0: untouched
                rsz::apply = sa2; g_cfg.xr = sx2; fp::on = so; *(int *)(st + 0x98) = 0; *(int *)(st + 0x60) = 0; shade::writes = 0; shade::applies = 0; shade::mode = 1; shade::forced = false; shade::oA = shade::oS = -1;
                ok &= sh; logf("first-person AO fix test (AO off + screen-space shadows on through the game's apply, once; the menu's choice kept; your settings back after F5; unknown values and fp_ao_off = 0 untouched): %s (applies %d)", sh ? "PASS" : "FAIL", nAp);
            }
            {   // 0.18.10 near stipple fade: switched off in the active environment sets while first person is on, the game's value back after
                static u8 fset[0x2C58]; memset(fset, 0, sizeof fset); u8 *e = fset + 106 * 0x20; *(int *)(e + 0x10) = 106; *(int *)(e + 0x14) = 3; e[0] = 0; e[0x1C] = 0;
                u8 *eh = fset + 312 * 0x20; *(int *)(eh + 0x10) = 312; *(int *)(eh + 0x14) = 1; *(float *)eh = 1.0f;
                u8 *en = fset + 5 * 0x20, *er = fset + 104 * 0x20; *(int *)(en + 0x10) = 5; *(int *)(en + 0x14) = 1; *(float *)en = 10.0f; *(int *)(er + 0x10) = 104; *(int *)(er + 0x14) = 1; *(float *)er = 200.0f;
                static u8 bad[0x2C58]; memset(bad, 0, sizeof bad);   // not an environment set (index / type do not match): untouched
                u8 **lst = (u8 **)(g_base + stipple::RVA_LIST); int *cnt = (int *)(g_base + stipple::RVA_COUNT); DWORD o1, o2; VirtualProtect(lst, 16, PAGE_READWRITE, &o1); VirtualProtect(cnt, 4, PAGE_READWRITE, &o2);
                u8 *s0 = lst[0], *s1 = lst[1]; const int c0 = *cnt; lst[0] = fset; lst[1] = bad; *cnt = 2;
                const int sx2 = g_cfg.xr; g_cfg.xr = 4; const bool sfo = g_stFrameOn; const u32 sfr = g_stFrame; g_stFrameOn = true;
                const float snc = g_fp.nearCm; g_fp.nearCm = 2.0f;
                g_stFrame = 500; stipple::tick(true); bool st = e[0] == 1 && e[0x1C] == 1 && bad[106 * 0x20] == 0 && *(float *)en == 10.0f && en[0x1C] == 0 && *(float *)er == 200.0f && bad[5 * 0x20] == 0 && *(float *)eh == 1.0f;   // 0.18.15: near clip / focus radius left to the game
                g_stFrame = 501; stipple::tick(false); st &= e[0] == 0 && e[0x1C] == 0 && stipple::nMods == 0 && *(float *)eh == 1.0f;
                *(int *)(en + 0x14) = 3; g_stFrame = 502; stipple::tick(true); st &= *(float *)en == 10.0f; g_stFrame = 503; stipple::tick(false);   // NEAR_CLIP not typed like a float: untouched
                g_fp.nearCm = snc;
                lst[0] = s0; lst[1] = s1; *cnt = c0; g_cfg.xr = sx2; g_stFrameOn = sfo; g_stFrame = sfr;
                ok &= st; logf("environment test (near stipple fade off in first person, near clip / focus radius untouched, checked entries only, game values back after): %s", st ? "PASS" : "FAIL");
            }
        }
        rsz::getApp = sg; rsz::apply = sa; rsz::oPump = sp; rsz::pending = 0; rsz::applies = 0; rsz::pumpTid = 0;
        logf("main-thread size change test (after the message pump, ApplyDisplay with the current settings, once, never in fullscreen): %s", ok ? "PASS" : "FAIL");
        rsOk &= ok;
    }
    {   // 0.17.0/0.17.3 first person: eye anchor on the root + head height; head set hidden in the RENDER POSE right after the
        // hero's animation job (rows zero, onto the neck), world array untouched, stray hand dropped, nothing hidden once FP stops
        static u8 code[8] = {0xB8, 0x2A, 0, 0, 0, 0xC3}; static void *vt[8]; vt[4] = code;
        static u8 hero[0x4200]; static float jt[32 * 16], pose[2][32 * 16]; static u8 job[0x100]; memset(hero, 0, sizeof hero); memset(jt, 0, sizeof jt); memset(pose, 0, sizeof pose); memset(job, 0, sizeof job);
        *(void ***)hero = vt; float *root = (float *)(hero + 0x150);
        auto ident = [](float *m, float x, float y, float z) { memset(m, 0, 64); m[0] = 1; m[5] = 1; m[10] = 1; m[15] = 1; m[12] = x; m[13] = y; m[14] = z; };
        ident(root, 1000, 2000, 300); *(float **)(hero + 0x278) = jt; *(int *)(hero + 0x280) = 31;   // node count = 30 joints + the node
        *(char *)(hero + 0x2DA) = 0; *(float **)(hero + 0x508) = pose[0]; *(int *)(hero + 0x510) = 30; *(float **)(hero + 0x520) = pose[1]; *(int *)(hero + 0x528) = 30;
        static float hand; hand = 160;
        auto anim = [&](float handUp) { ident(jt, 1000, 2000, 300); for (int i = 0; i < 30; i++) ident(jt + (i + 1) * 16, 1000, 2000, 300 + i * 6.0f); ident(jt + 30 * 16, 1000, 2000, 300 + handUp); ident(jt + 31 * 16, 1e9f, 0, 0); ident(jt + 2 * 16, 1003, 2000, 300 + 205.0f);   /* BUG-75: joint 1 = lone gear above the head (horse) */
            for (int k = 0; k < 12; k++) jt[9 * 16 + k] = 0.0f; };   // joint 8: a hidden gear slot (zero scale) inside the skeleton
        anim(hand);   // joint 29 = a hand next to the face in the first pose
        const auto shp = fp::heroPtr; static u8 *hp; hp = hero; fp::heroPtr = []() -> u8 * { return hp; };
        const auto soj = fp::oAnimJob;   // the "animation job": model-space render pose of this tick (joint i at i * 6 cm, the hand at `hand`)
        fp::oAnimJob = [](u8 *j) { float *P = *(float **)(j + 0xA8); for (int i = 0; i < 30; i++) { float *m = P + i * 16; memset(m, 0, 64); m[0] = m[5] = m[10] = m[15] = 1; m[14] = i * 6.0f; } P[29 * 16 + 14] = hand; P[1 * 16 + 12] = 3; P[1 * 16 + 14] = 205; };   // joint 1 = the lone gear (as in the world array)
        *(u8 **)(job + 0xE8) = hero; *(float **)(job + 0xA8) = pose[0];
        const int sxr = g_cfg.xr; g_cfg.xr = 4; ui::mode = ui::M_HUD; ui::flatNow = false; const FpCfg sfp = g_fp; g_fp = FpCfg{}; g_fp.vertTau = 0; g_fp.eyeUpCm = -3.0f; g_fp.eyeLeftCm = 0.0f; g_fp.headTau = 0.25f;   /* the anchor checks below were written for these */ fp::on = 1; g_stFrameOn = true; g_stFrame = 100;
        float gm[16]; ident(gm, 1000, 1700, 450);   // game camera 3 m behind
        bool ok = fp::update(gm) && fp::nJ == 30 && fp::headIdx == 28 && fp::countSrc == 1;
        ok &= fabsf(gm[12] - (1000 + g_fp.eyeFwdCm)) < 0.01f && fabsf(gm[13] - 2000) < 0.01f && fabsf(gm[14] - (300 + 168 + g_fp.eyeUpCm)) < 0.01f;   // eyes relative to the top of the head
        ok &= fp::nHide == 5 && fp::neckIdx == 24;   // joints 25..28 (150..168 cm) + the hand at 160 (within 22 cm); the neck = joint 24 (144 cm)
        ok &= len3_st(jt + 29 * 16) > 0.9f;          // the world array is not touched any more
        fp::hAnimJob(job);
        const float *P = pose[0];
        ok &= len3_st(P + 28 * 16) == 0.0f && len3_st(P + 28 * 16 + 4) == 0.0f && fabsf(P[28 * 16 + 14] - 144) < 0.01f;   // the head: hidden on the neck
        ok &= len3_st(P + 25 * 16) == 0.0f && len3_st(P + 29 * 16) == 0.0f && len3_st(P + 24 * 16) > 0.9f && len3_st(P + 20 * 16) > 0.9f;   // + the hand by the face; neck and chest drawn
        *(u8 **)(job + 0xE8) = hero + 8; fp::hAnimJob(job); ok &= len3_st(pose[0] + 25 * 16) > 0.9f; *(u8 **)(job + 0xE8) = hero;   // another character: untouched
        {   // BUG-70: the left tick recomputes at every camera update (the hero moved after the first one); the right tick reuses it
            auto shift = [&](float dx) { root[12] += dx; for (int i = 0; i < 31; i++) jt[i * 16 + 12] += dx; };
            float g3[16]; shift(10); ident(g3, 1000, 1700, 450); bool r1 = fp::update(g3) && fabsf(g3[12] - (1010 + g_fp.eyeFwdCm)) < 0.01f;   // same left tick, later update
            shift(40); g_pairPhase = 1; g_stFrame = 101; ident(g3, 1000, 1700, 450); r1 &= fp::update(g3) && fabsf(g3[12] - (1010 + g_fp.eyeFwdCm)) < 0.01f;   // right tick: the left's final anchor
            r1 &= fp::lrDiffMax.load() > 39.0f && fp::nPairReuse.load() == 1; shift(-50); g_pairPhase = 0; fp::lrDiffMax = 0; fp::nPairReuse = 0; fp::nRecompute = 0;
            ok &= r1; if (!r1) logf("   first-person pair-anchor reuse: FAIL");
        }
        g_stFrame = 102; hand = 60; anim(hand);   // the hand drops to 60 cm (84 cm from the neck)
        fp::hAnimJob(job); ok &= len3_st(pose[0] + 29 * 16) > 0.9f && len3_st(pose[0] + 27 * 16) == 0.0f;   // the stray hand is drawn, the head is not
        { const int sps = fp::parState.load(); fp::parState = 1; fp::censusReq = 1; fp::hAnimJob(job); logf("   census request consumed by the job: %s", fp::censusReq.load() == 0 ? "yes" : "NO"); fp::censusReq = 0; fp::parState = sps; }
        ok &= fp::update(gm) && fp::nHide == 4 && fp::nDropped.load() == 1;   // ...and it leaves the set for good
        g_stFrame = 103; ui::mode = ui::M_CINEMA; ui::camCut = 1; float gm2[16]; ident(gm2, 1000, 1700, 450); ok &= !fp::update(gm2) && gm2[13] == 1700;   // cutscene: the game's camera
        g_stFrame = 107; fp::hAnimJob(job); ok &= len3_st(pose[0] + 27 * 16) > 0.9f;   // ...and the head is drawn again
        ui::mode = ui::M_HUD; ui::camCut = 0; g_stFrame = 108; anim(100); ident(gm2, 1000, 1700 - 2000, 450); ok &= !fp::update(gm2) && fp::lastReason.load() == 6;   // camera 20 m away: scripted
        {   // 0.18.5 body follow: Jin runs back towards you (faces +x, your forward is -x) -> his pose is drawn turned round
            // 0.18.16: moving, the body faces your HEAD's forward (world heading); here you look back (-x) while Jin runs at 2 m/s
            float g4[16]; g_fp.bodyTurnSpeed = 0; const float smt = g_fp.moveTau, sht = g_fp.headTau; g_fp.moveTau = 0; g_fp.headTau = 0; const float sbts = g_fp.bodyTurnS; g_fp.bodyTurnS = 0.0f; const float sdd = g_fp.headDeadDeg; g_fp.headDeadDeg = 45.0f; const int smh = vrin::cfg.moveHead; vrin::cfg.moveHead = 1; fp::refF[0] = -1; fp::refF[1] = 0; fp::refOk = true; fp::nLone = 0;
            const bool sho = fp::headOk; fp::headOk = true; fp::headF[0] = -1; fp::headF[1] = 0; const int sbu = g_fp.bodyUnder; g_fp.bodyUnder = 0;
            const float *rt = (const float *)(fp::node + 0x150); fp::lastRoot[0] = rt[12] - 20.0f; fp::lastRoot[1] = rt[13]; fp::lastRoot[2] = rt[14]; fp::lastRootOk = true; fp::bodyLastMs = GetTickCount64() - 100;
            g_stFrame = 109; ident(g4, 1000, 1700, 450); bool b = fp::update(g4) && fabsf(fabsf(fp::bodyDeg.load()) - 180.0f) < 0.5f;
            b &= fabsf(g4[12] - (1000 - g_fp.eyeFwdCm)) < 0.05f;   // the eyes in front of the turned face
            fp::hAnimJob(job); const float *Q = pose[0];
            b &= fabsf(Q[20 * 16 + 0] + 1.0f) < 1e-3f && fabsf(Q[20 * 16 + 5] + 1.0f) < 1e-3f && fabsf(Q[20 * 16 + 14] - 120.0f) < 0.01f && fabsf(Q[20 * 16 + 12]) < 0.01f;   // chest turned round, on the head's axis
            b &= len3_st(Q + 28 * 16) == 0.0f;   // the head stays hidden
            b &= fabsf(jt[21 * 16 + 0] + 1.0f) < 1e-3f && fabsf(jt[21 * 16 + 12] - 1000.0f) < 0.01f && fabsf(jt[21 * 16 + 14] - 420.0f) < 0.01f;   // 0.18.6: joint 20's WORLD matrix turned on the head axis (items follow)
            b &= fabsf(jt[2 * 16 + 12] - 997.0f) < 0.01f && fabsf(jt[2 * 16 + 14] - 505.0f) < 0.01f && fp::nWorldTurned.load() == 1;   // joint 1 (3 cm in front) is now 3 cm behind
            fp::lone[0] = 1; fp::nLone = 1; g_stFrame = 110; ident(g4, 1000, 1700, 450);   // riding: joint 1 (lone gear) is 205 cm up
            b &= fp::update(g4) && fabsf(fp::bodyDeg.load()) < 0.01f && fp::nBodyRiding.load() > 0;
            fp::hAnimJob(job); b &= fabsf(pose[0][20 * 16 + 0] - 1.0f) < 1e-3f;   // ...drawn as animated
            fp::nLone = 0; fp::refF[0] = cosf(0.5f); fp::refF[1] = sinf(0.5f); fp::headF[0] = cosf(0.5f); fp::headF[1] = sinf(0.5f); fp::lastRootOk = false; fp::bodyLastMs = GetTickCount64() - 100;
            g_stFrame = 111; ident(g4, 1000, 1700, 450);   // standing, looking 29 deg aside: inside the dead zone, no turn
            b &= fp::update(g4) && fabsf(fp::bodyDeg.load()) < 0.01f;
            fp::headF[0] = cosf(-0.5f); fp::headF[1] = sinf(-0.5f); fp::bodyLastMs = GetTickCount64() - 100; g_stFrame = 112; ident(g4, 1000, 1700, 450);   // 29 deg the other way: no turn either
            b &= fp::update(g4) && fabsf(fp::bodyDeg.load()) < 0.01f;
            {   // 0.18.16 body under the headset: the head 15 cm to the left in the world -> the drawn body and its world matrices move with it
                g_fp.bodyUnder = 1; const float sut = g_fp.underTau; g_fp.underTau = 0.0f; fp::underW[0] = 0.0f; fp::underW[1] = 15.0f; fp::bodyLastMs = GetTickCount64() - 100; g_stFrame = 113; ident(g4, 1000, 1700, 450); b &= fp::update(g4);
                fp::underW[0] = 0.0f; fp::underW[1] = 15.0f; fp::hAnimJob(job);
                const bool bu = fabsf(pose[0][20 * 16 + 13] - 15.0f) < 0.01f && fabsf(jt[21 * 16 + 13] - 2015.0f) < 0.01f && fabsf(fp::underCm.load() - 15.0f) < 0.01f;
                fp::underW[1] = 500.0f; fp::hAnimJob(job); const bool bm = fabsf(pose[0][20 * 16 + 13] - g_fp.underMaxCm) < 0.01f;   // clamped
                b &= bu && bm; if (!(bu && bm)) logf("   body under the headset: FAIL (pose y %.2f, world y %.2f)", pose[0][20 * 16 + 13], jt[21 * 16 + 13]);
                fp::underW[1] = 0.0f; fp::underCm = 0; g_fp.underTau = sut;
            }
            g_fp.moveTau = smt; g_fp.headTau = sht; g_fp.bodyTurnS = sbts; g_fp.headDeadDeg = sdd; vrin::cfg.moveHead = smh; fp::headOk = sho; g_fp.bodyUnder = sbu; fp::lastRootOk = false; fp::bodyLastMs = 0; vrin::moveYawOk = 0; vrin::moveCapAt = 0;
            fp::refOk = false; fp::bodyYawCur = 0; fp::bodyYaw = 0; fp::jobApplied = 0; fp::nBodyRiding = 0; fp::nBodyFrames = 0; fp::nBodyJobs = 0; fp::nWorldTurned = 0;
            ok &= b; if (!b) logf("   first-person body follow: FAIL (body %.1f deg, eye x %.2f)", fp::bodyDeg.load(), g4[12]);
        }
        fp::heroPtr = shp; fp::oAnimJob = soj; fp::unpublish(); g_fp = sfp; g_cfg.xr = sxr; g_stFrameOn = false; fp::valid = false; fp::hero = nullptr; fp::node = nullptr; fp::joints = nullptr; fp::lastFrame = 0xFFFFFFFFu; fp::on = 1;
        fp::nActive = 0; fp::nBlockedUi = 0; fp::nNoHero = 0; fp::nFar = 0; fp::nCollapse = 0; fp::nSeenCollapsed = 0; fp::nSeenRestored = 0; fp::nDropped = 0; fp::nRevalidate = 0; fp::nHeroJobs = 0; fp::nJobsHidden = 0; fp::nJointsHidden = 0; fp::nJobsNoBuf = 0;
        logf("first-person test (anchor on root + head height + forward; body drawn facing your forward when Jin runs back, not while riding; head set hidden in the render pose after the hero's animation job, onto the neck; world array untouched; other characters untouched; stray hand dropped; cutscene / far camera keep the game camera and show the head): %s", ok ? "PASS" : "FAIL");
        rsOk &= ok;
    }
    {   // 0.18.7 FBIK stage 1 (fbik.h): arm chains found by shape on a synthetic skeleton; two-bone IK reaches the wrist target with the
        // bone lengths kept, the elbow bends down/out, fingers stay rigid on the hand, the hand takes the grip frame; far targets stop short
        static int16_t par[64]; static float pz[64 * 16]; int n = 0;
        auto add = [&](int p, float x, float y, float z) { float *m = pz + n * 16; memset(m, 0, 64); m[0] = m[5] = m[10] = m[15] = 1; m[12] = x; m[13] = y; m[14] = z; par[n] = (int16_t)p; return n++; };
        const int root = add(-1, 0, 0, 100), spine = add(root, 0, 0, 130), neck = add(spine, 0, 0, 150); add(neck, 0, 0, 165);
        int hands[2], shs[2];
        for (int sd = 0; sd < 2; sd++) { const float y = sd == 0 ? 1.0f : -1.0f;
            const int cl = add(neck, 0, 5 * y, 145), sh = add(cl, 0, 18 * y, 145), el = add(sh, 0, 18 * y, 115), hd = add(el, 0, 18 * y, 88);
            hands[sd] = hd; shs[sd] = sh;
            const float fy[4] = {16.5f, 18.0f, 19.5f, 21.0f};
            for (int f = 0; f < 4; f++) { const int r = add(hd, 1, fy[f] * y, 80); add(r, 1, fy[f] * y, 77); }
            const int tb = add(hd, 4, 15.5f * y, 84); add(tb, 6, 15.5f * y, 82);
            add(hd, 60, 18 * y, 88); add(el, 0, 18 * y, 101); }   // 0.18.8: a held item 60 cm out under the hand + a forearm twist joint (0.18.7 found no chains on Jin)
        int feetJ[2], kneeJ[2], hipJ[2];
        for (int sd = 0; sd < 2; sd++) { const float y = sd == 0 ? 1.0f : -1.0f; const int hip = add(root, 0, 10 * y, 85), kn = add(hip, 20, 10 * y, 45), ft = add(kn, 0, 10 * y, 5); add(ft, 12, 10 * y, 2); feetJ[sd] = ft; kneeJ[sd] = kn; hipJ[sd] = hip; }   // knees bent (sword stance)
        static fbik::Arm A[2]; char why[400]; bool ok = fbik::parentsOk(par, n) && fbik::findArms(par, n, pz, 165.0f, nullptr, A, why, sizeof why) == 2;
        ok &= A[0].hand == hands[0] && A[1].hand == hands[1] && A[0].sh == shs[0] && A[1].sh == shs[1] && A[0].frame && A[1].frame && A[0].nSub[3] == 12;
        if (!ok) logf("   fbik chains: %s", why);
        fbik::Target T; T.pos[0] = 40; T.pos[1] = 25; T.pos[2] = 130; T.fing[0] = 1; T.fing[1] = 0; T.fing[2] = 0; T.thumb[0] = 0; T.thumb[1] = 0; T.thumb[2] = 1; T.orient = true;
        const float fr0[3] = {pz[(hands[0] + 1) * 16 + 12] - pz[hands[0] * 16 + 12], pz[(hands[0] + 1) * 16 + 13] - pz[hands[0] * 16 + 13], pz[(hands[0] + 1) * 16 + 14] - pz[hands[0] * 16 + 14]};
        fbik::Params prm; fbik::solveArm(pz, A[0], 0, T, 1.0f, prm);
        auto dist = [&](int a, const float *b) { const float d[3] = {pz[a * 16 + 12] - b[0], pz[a * 16 + 13] - b[1], pz[a * 16 + 14] - b[2]}; return fbik::len3f(d); };
        auto djj = [&](int a, int b) { return dist(a, pz + b * 16 + 12); };
        ok &= dist(A[0].hand, T.pos) < 0.5f && fabsf(djj(A[0].sh, A[0].el) - 30.0f) < 0.1f && fabsf(djj(A[0].el, A[0].hand) - 27.0f) < 0.1f;   // reached, lengths kept
        ok &= pz[A[0].el * 16 + 14] < 0.5f * (pz[A[0].sh * 16 + 14] + T.pos[2]) && pz[A[0].el * 16 + 13] > 18.0f;   // elbow down and out (Jin's left = +y)
        ok &= fabsf(fbik::len3f(fr0) - djj(hands[0], hands[0] + 1)) < 0.05f;   // the finger root rides rigidly with the hand
        {   // 0.18.23 forearm twist: the twist joint half way down the forearm takes half the hand's roll about the forearm
            const int tj = hands[0] + 12; bool tw = A[0].nTw == 1 && A[0].tw[0] == tj && fabsf(A[0].twT[0] - 14.0f / 27.0f) < 0.02f;
            const float ax[3] = {pz[A[0].hand * 16 + 12] - pz[A[0].el * 16 + 12], pz[A[0].hand * 16 + 13] - pz[A[0].el * 16 + 13], pz[A[0].hand * 16 + 14] - pz[A[0].el * 16 + 14]}; float axn[3] = {ax[0], ax[1], ax[2]}; fbik::norm3(axn);
            float best = 2; int k = 0; for (int q = 0; q < 3; q++) { float v[3] = {pz[A[0].el * 16 + 4 * q], pz[A[0].el * 16 + 4 * q + 1], pz[A[0].el * 16 + 4 * q + 2]}; fbik::norm3(v); if (fabsf(fbik::dot3(v, axn)) < best) { best = fabsf(fbik::dot3(v, axn)); k = q; } }
            float got = 0; tw &= fbik::twistAngle(pz + A[0].el * 16 + 4 * k, pz + tj * 16 + 4 * k, axn, got);
            const float want = fbik::lastTwistDeg[0] * A[0].twT[0]; tw &= fabsf(got * 57.2957795f - want) < 1.0f && fabsf(fbik::lastTwistDeg[0]) > 5.0f;
            tw &= fabsf(djj(tj, A[0].el) - 14.0f) < 0.05f;   // it stays on the forearm
            ok &= tw; logf("   forearm twist: %s (hand roll %.1f deg, twist joint at %.0f %% turned %.1f deg)", tw ? "PASS" : "FAIL", fbik::lastTwistDeg[0], A[0].twT[0] * 100.0f, got * 57.2957795f);
        }
        { float f[3] = {0, 0, 0}; int c = 0; for (int q = hands[0] + 1; q < hands[0] + 11; q++) if (par[q] == hands[0] && q != hands[0] + 9) { for (int k = 0; k < 3; k++) f[k] += pz[q * 16 + 12 + k] - pz[hands[0] * 16 + 12 + k]; c++; }
          fbik::norm3(f); ok &= f[0] > 0.95f; }   // the fingers point along the grip's finger direction (+x)
        float far2[3] = {80, 20, 145}; T.pos[0] = far2[0]; T.pos[1] = far2[1]; T.pos[2] = far2[2]; const float c0[3] = {pz[A[0].clav * 16 + 12], pz[A[0].clav * 16 + 13], pz[A[0].clav * 16 + 14]};
        const float s0[3] = {pz[A[0].sh * 16 + 12], pz[A[0].sh * 16 + 13], pz[A[0].sh * 16 + 14]};
        fbik::solveArm(pz, A[0], 0, T, 1.0f, prm);
        ok &= dist(A[0].hand, far2) > 1.0f && fabsf(djj(A[0].sh, A[0].el) - 30.0f) < 0.1f && fabsf(djj(A[0].el, A[0].hand) - 27.0f) < 0.1f;   // out of reach: stops short, no stretch
        ok &= dist(A[0].clav, c0) < 0.01f && dist(A[0].sh, s0) > 0.5f;   // the clavicle shrugged toward it (the shoulder moved, the clavicle root did not)
        {   // 0.18.26 arm stretch: the same far target with 10 % stretch - the hand gets closer, the upper arm is longer, the forearm is not
            const float dShort = dist(A[0].hand, far2); fbik::Params p2 = prm; p2.stretch = 0.10f; fbik::solveArm(pz, A[0], 0, T, 1.0f, p2);
            const float up = djj(A[0].sh, A[0].el), lo = djj(A[0].el, A[0].hand);
            const bool st = dist(A[0].hand, far2) < dShort - 1.0f && up > 30.5f && up <= 30.0f + 5.7f + 0.05f && fabsf(lo - 27.0f) < 0.1f && fbik::lastStretchCm[0] > 0.5f;
            ok &= st; logf("   arm stretch: %s (upper arm %.1f cm, forearm %.1f cm, %.1f cm closer)", st ? "PASS" : "FAIL", up, lo, dShort - dist(A[0].hand, far2));
            fbik::solveArm(pz, A[0], 0, T, 1.0f, prm);
        }
        {   // 0.18.9 stand tall: legs found, the body raised 5 cm, the feet stay where they were planted, knees straighter, lengths kept
            static fbik::Leg L[2]; char lw[300]; bool lg = fbik::findLegs(par, n, pz, 165.0f, nullptr, L, lw, sizeof lw) == 2 && L[0].foot == feetJ[0] && L[1].knee == kneeJ[1] && L[0].hip == hipJ[0];
            float f0[3] = {pz[feetJ[0] * 16 + 12], pz[feetJ[0] * 16 + 13], pz[feetJ[0] * 16 + 14]}; const float h0 = pz[hipJ[0] * 16 + 14], kx0 = pz[kneeJ[0] * 16 + 12];
            const float got = fbik::standTall(pz, n, L, 5.0f, 0.0f);
            lg &= fabsf(got - 5.0f) < 1e-3f && dist(feetJ[0], f0) < 0.1f && fabsf(pz[hipJ[0] * 16 + 14] - (h0 + 5.0f)) < 1e-3f && pz[kneeJ[0] * 16 + 12] < kx0 - 1.0f && pz[kneeJ[0] * 16 + 12] > 0.0f;
            lg &= fabsf(djj(hipJ[0], kneeJ[0]) - L[0].thigh) < 0.05f && fabsf(djj(kneeJ[0], feetJ[0]) - L[0].shin) < 0.05f && fabsf(pz[feetJ[0] * 16 + 0] - 1.0f) < 1e-3f;   // foot flat as animated
            lg &= fbik::standTall(pz, n, L, 50.0f, 0.0f) < 5.0f;   // never past a straight leg
            ok &= lg; if (!lg) logf("   fbik legs: %s (raise %.2f)", lw, got);
        }
        {   // 0.18.13 idle hold: captured after standing still, held while quiet, let go on input; world matrices follow
            static float W[(8 + 1) * 16]; static u8 nd[0x300]; memset(nd, 0, sizeof nd); memset(W, 0, sizeof W); *(float **)(nd + 0x278) = W;
            float *N = (float *)(nd + 0x150); N[0] = N[5] = N[10] = N[15] = 1.0f; N[12] = 1000.0f;
            static float A[8 * 16], B[8 * 16], P[8 * 16]; for (int j = 0; j < 8; j++) for (int c = 0; c < 16; c++) { A[j * 16 + c] = (c % 5 == 0) ? 1.0f : 0.0f; B[j * 16 + c] = A[j * 16 + c]; } for (int j = 0; j < 8; j++) { A[j * 16 + 14] = 10.0f * j; B[j * 16 + 14] = 10.0f * j + 3.0f; }
            static u64 t = 1000; fp::idleNow = [] { return t; }; const int sLeg = fp::legsReady.load(), sk = fp::hpubIdx.load(); fp::legsReady = 0; fp::hpubIdx = -1; fp::idleLastMs = 0; fp::idleW = 0; fp::idleCap = false; fp::idleStill = 0; fp::idlePrevOk = false;
            fp::speedMs = 0; fp::animUntil = 0; fp::raisedUntil = 0; fp::ridingNow = 0; const int sih = g_fp.idleHold; g_fp.idleHold = 1;
            bool it = true; for (int i = 0; i < 7; i++) { t += 100; memcpy(P, A, sizeof P); fp::idleApply(nd, P, 8, true); }   // stands 0.6 s: captured, blending in
            for (int i = 0; i < 3; i++) { t += 100; memcpy(P, B, sizeof P); it &= fp::idleApply(nd, P, 8, true); }               // a slow fidget: the stand pose is held
            it &= fabsf(P[3 * 16 + 14] - 30.0f) < 1e-3f && fabsf(W[4 * 16 + 14] - 30.0f) < 1e-3f && fabsf(W[4 * 16 + 12] - 1000.0f) < 1e-3f;
            for (int i = 0; i < 3; i++) { t += 100; memcpy(P, B, sizeof P); fp::idleApply(nd, P, 8, false); }                    // pad input: let go
            it &= fabsf(P[3 * 16 + 14] - 33.0f) < 1e-3f && !fp::idleCap && fp::idleWhy.load() == 2;
            for (int i = 0; i < 3; i++) { t += 100; memcpy(P, B, sizeof P); fp::idleApply(nd, P, 8, true); }
            fp::speedMs = 1.0f; t += 100; memcpy(P, B, sizeof P); fp::idleApply(nd, P, 8, true); it &= fp::idleWhy.load() == 3 && fp::idleStill == 0.0f;   // walking: not idle
            fp::speedMs = 0; fp::legsReady = sLeg; fp::hpubIdx = sk; fp::idleNow = GetTickCount64; fp::idleLastMs = 0; fp::idleW = 0; fp::idleCap = false; g_fp.idleHold = sih;
            ok &= it; logf("idle hold test (stand pose taken after standing still, held through a slow fidget, world matrices follow, let go on pad input / walking): %s", it ? "PASS" : "FAIL");
        }
        {   // 0.18.15 a snap turn brings the body round even inside the 35 deg dead zone (standing, head follow)
            float root[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}; const float U[3] = {0, 0, 1}, F[3] = {1, 0, 0};
            const bool sro = fp::refOk, sho = fp::headOk; const int sbf = g_fp.bodyFollow, sbh = g_fp.bodyHead; const float sby = fp::bodyYawCur; const int sln = fp::nLone;
            g_fp.bodyFollow = 1; g_fp.bodyHead = 1; fp::refOk = true; fp::headOk = true; fp::refF[0] = 1; fp::refF[1] = 0; fp::nLone = 0; fp::bodyYawCur = 0; fp::headTurning = false; fp::lastRootOk = false;
            const float sdd = g_fp.headDeadDeg; g_fp.headDeadDeg = 45.0f;
            const float a = 20.0f / 57.2957795f; fp::headF[0] = cosf(a); fp::headF[1] = sinf(a);   // looking 20 deg left: inside the dead zone
            for (int i = 0; i < 5; i++) { fp::bodyLastMs = GetTickCount64() - 50; fp::bodyStep(root, U, F, true); }
            bool tb = fabsf(fp::bodyYawCur) < 1e-4f;   // no turn yet
            vrin::turnSeq++; for (int i = 0; i < 60; i++) { fp::bodyLastMs = GetTickCount64() - 50; fp::bodyStep(root, U, F, true); }
            tb &= fabsf(fp::bodyYawCur - a) < 2.5f / 57.2957795f;   // after the snap: the body faces the view
            fp::refOk = sro; fp::headOk = sho; g_fp.bodyFollow = sbf; g_fp.bodyHead = sbh; fp::bodyYawCur = sby; fp::nLone = sln; fp::headTurning = false; fp::bodyLastMs = 0; fp::lastRootOk = false; fp::bodyYaw = 0; g_fp.headDeadDeg = sdd;
            ok &= tb; logf("snap-turn body test (inside the dead zone nothing turns; after a snap the hips come round to the view): %s", tb ? "PASS" : "FAIL");
        }
        {   // 0.18.25 the body turn eases in (no snap) and settles on the view: 40 deg look past the 20 deg threshold
            float root[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}; const float U[3] = {0, 0, 1}, F[3] = {1, 0, 0};
            const bool sro = fp::refOk, sho = fp::headOk; const int sbf = g_fp.bodyFollow, sbh = g_fp.bodyHead; const float sby = fp::bodyYawCur, sdd = g_fp.headDeadDeg, sbts = g_fp.bodyTurnS; const int sln = fp::nLone;
            g_fp.bodyFollow = 1; g_fp.bodyHead = 1; g_fp.headDeadDeg = 20.0f; g_fp.bodyTurnS = 0.35f; fp::refOk = true; fp::headOk = true; fp::refF[0] = 1; fp::refF[1] = 0; fp::nLone = 0; fp::bodyYawCur = 0; fp::headTurning = false; fp::lastRootOk = false;
            fp::headF[0] = 1; fp::headF[1] = 0; for (int i = 0; i < 80; i++) { fp::bodyLastMs = GetTickCount64() - 50; fp::bodyStep(root, U, F, true); }
            const float b0 = fp::bodyYawCur, a = 40.0f / 57.2957795f; fp::headF[0] = cosf(a); fp::headF[1] = sinf(a);
            fp::bodyLastMs = GetTickCount64() - 50; fp::bodyStep(root, U, F, true); const float b1 = fp::bodyYawCur - b0;
            for (int i = 0; i < 7; i++) { fp::bodyLastMs = GetTickCount64() - 50; fp::bodyStep(root, U, F, true); } const float b8 = fp::bodyYawCur - b0;
            for (int i = 0; i < 40; i++) { fp::bodyLastMs = GetTickCount64() - 50; fp::bodyStep(root, U, F, true); } const float bEnd = fp::bodyYawCur - b0;
            const float tgt = a - b0; const bool tw = b1 < 0.25f * tgt && b8 > 0.8f * tgt && fabsf(bEnd - tgt) < 1.0f / 57.2957795f;
            fp::refOk = sro; fp::headOk = sho; g_fp.bodyFollow = sbf; g_fp.bodyHead = sbh; fp::bodyYawCur = sby; g_fp.headDeadDeg = sdd; g_fp.bodyTurnS = sbts; fp::nLone = sln; fp::headTurning = false; fp::bodyLastMs = 0; fp::lastRootOk = false; fp::bodyYaw = 0;
            ok &= tw; logf("body turn tween test (past the threshold the body eases round: %.0f %% after 50 ms, %.0f %% after 0.4 s, settled %.1f deg): %s", 100.0f * b1 / tgt, 100.0f * b8 / tgt, bEnd * 57.2957795f, tw ? "PASS" : "FAIL");
        }
        {   // 0.18.28 stopping squares the body to the headset: running with a 12 deg look (inside the 20 deg dead zone) keeps the body; stop -> it turns
            float root[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}; const float U[3] = {0, 0, 1}, F[3] = {1, 0, 0};
            const bool sro = fp::refOk, sho = fp::headOk; const int sbf = g_fp.bodyFollow, sbh = g_fp.bodyHead, smh = vrin::cfg.moveHead, ssq = g_fp.bodySquareOnStop; const float sby = fp::bodyYawCur, sdd = g_fp.headDeadDeg, sbts = g_fp.bodyTurnS, sup = g_cfg.unitsPerM; const int sln = fp::nLone;
            bool sq = true; float bk[2] = {0, 0}, bMove[2] = {0, 0};
            for (int pass = 0; pass < 2; pass++) {
                g_fp.bodySquareOnStop = pass == 0 ? 1 : 0; g_fp.bodyFollow = 1; g_fp.bodyHead = 1; vrin::cfg.moveHead = 1; g_fp.headDeadDeg = 20.0f; g_fp.bodyTurnS = 0.35f; g_cfg.unitsPerM = 100.0f;
                fp::refOk = true; fp::headOk = true; fp::refF[0] = 1; fp::refF[1] = 0; fp::nLone = 0; fp::bodyYawCur = 0; fp::headTurning = false; fp::lastRootOk = false; root[12] = 0; fp::bodyLastMs = GetTickCount64() - 50; fp::bodyStep(root, U, F, false);
                auto stepN = [&](int n, float mv) { for (int i = 0; i < n; i++) { root[12] += mv; fp::bodyLastMs = GetTickCount64() - 50; fp::bodyStep(root, U, F, true); } };
                fp::headF[0] = 1; fp::headF[1] = 0; stepN(60, 5.0f);
                const float a = 12.0f / 57.2957795f; fp::headF[0] = cosf(a); fp::headF[1] = sinf(a); stepN(40, 5.0f); bMove[pass] = fp::bodyYawCur;
                stepN(60, 0.0f); bk[pass] = fp::bodyYawCur;
            }
            const float a = 12.0f / 57.2957795f;
            sq = fabsf(bMove[0]) < 1.0f / 57.2957795f && fabsf(bk[0] - a) < 1.0f / 57.2957795f && fabsf(bk[1]) < 1.0f / 57.2957795f;
            g_fp.bodySquareOnStop = ssq; vrin::cfg.moveHead = smh; g_fp.headDeadDeg = sdd; g_fp.bodyTurnS = sbts; g_cfg.unitsPerM = sup;
            fp::refOk = sro; fp::headOk = sho; g_fp.bodyFollow = sbf; g_fp.bodyHead = sbh; fp::bodyYawCur = sby; fp::nLone = sln; fp::headTurning = false; fp::bodyLastMs = 0; fp::lastRootOk = false; fp::bodyYaw = 0; fp::speedMs = 0;
            ok &= sq; logf("stop squares the body test (running, 12 deg look inside the dead zone: body %.1f deg; stopped: %.1f deg, with fp_body_square_on_stop=0: %.1f deg): %s", bMove[0] * 57.2957795f, bk[0] * 57.2957795f, bk[1] * 57.2957795f, sq ? "PASS" : "FAIL");
        }
        {   // 0.18.21 moving: the forward is NOT rewritten when the body turns past the threshold; a new forward squares the body to it
            float root[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}; const float U[3] = {0, 0, 1}, F[3] = {1, 0, 0};
            const bool sro = fp::refOk, sho = fp::headOk; const int sbf = g_fp.bodyFollow, sbh = g_fp.bodyHead; const float sby = fp::bodyYawCur, sup = g_cfg.unitsPerM; const int sln = fp::nLone;
            const float smy = vrin::moveYaw.load(); const int smo = vrin::moveYawOk.load();
            g_fp.bodyFollow = 1; g_fp.bodyHead = 1; g_cfg.unitsPerM = 100.0f; fp::refOk = true; fp::headOk = true; fp::refF[0] = 1; fp::refF[1] = 0; fp::nLone = 0; fp::bodyYawCur = 0; fp::headTurning = false; fp::lastRootOk = false;
            vrin::moveYaw = 0.0f; vrin::moveYawOk = 1; const float sdd = g_fp.headDeadDeg; g_fp.headDeadDeg = 45.0f; const int smh = vrin::cfg.moveHead; vrin::cfg.moveHead = 0;
            auto run = [&](int n) { for (int i = 0; i < n; i++) { root[12] += 5.0f; fp::bodyLastMs = GetTickCount64() - 50; fp::bodyStep(root, U, F, true); } };   // 1 m/s
            const float a = 60.0f / 57.2957795f; fp::headF[0] = 1; fp::headF[1] = 0; run(60);
            const float b0 = fp::bodyYawCur; bool mt = fabsf(b0) < 0.5f / 57.2957795f;
            fp::headF[0] = cosf(a); fp::headF[1] = sinf(a); run(80);   // looking 60 deg left while running: past the 45 deg threshold
            const float b1 = fp::bodyYawCur, m1 = vrin::moveYaw.load();
            mt &= fabsf(b1 - a) < 2.5f / 57.2957795f && m1 == 0.0f;   // the body turned, the forward did not
            if (!mt) logf("   moving body: FAIL (straight %.2f deg, looking 60 deg left: body %.2f deg, forward %.2f deg)", b0 * 57.2957795f, b1 * 57.2957795f, m1);
            vrin::moveYaw = 60.0f; run(80); mt &= fabsf(fp::bodyYawCur - a) < 1.0f / 57.2957795f && fabsf(fp::moveOffDeg.load()) < 1e-3f;   // a new forward (start from rest): offset cleared, body on it
            const float mb = fp::bodyYawCur;
            {   // 0.18.22 move_follows_head (default): running, a look 5 deg aside does nothing, 12 deg turns the body to the view
                vrin::cfg.moveHead = 1; g_fp.headDeadDeg = 7.5f; fp::bodyYawCur = 0; fp::headTurning = false; fp::headF[0] = 1; fp::headF[1] = 0; run(60);
                const float a5 = 5.0f / 57.2957795f, a12 = 12.0f / 57.2957795f; const float b00 = fp::bodyYawCur;
                fp::headF[0] = cosf(a5); fp::headF[1] = sinf(a5); run(40); const float b5 = fp::bodyYawCur;
                fp::headF[0] = cosf(a12); fp::headF[1] = sinf(a12); run(60); const float b12 = fp::bodyYawCur;
                const bool hm = fabsf(b00) < 0.6f / 57.2957795f && fabsf(b5 - b00) < 0.2f / 57.2957795f && fabsf(b12 - a12) < 0.6f / 57.2957795f; mt &= hm;
                if (!hm) logf("   moving body, head mode: FAIL (straight %.2f, 5 deg look %.2f, 12 deg look %.2f deg)", b00 * 57.2957795f, b5 * 57.2957795f, b12 * 57.2957795f);
            }
            g_fp.headDeadDeg = sdd; vrin::cfg.moveHead = smh;
            fp::refOk = sro; fp::headOk = sho; g_fp.bodyFollow = sbf; g_fp.bodyHead = sbh; fp::bodyYawCur = sby; fp::nLone = sln; fp::headTurning = false; fp::bodyLastMs = 0; fp::lastRootOk = false; fp::bodyYaw = 0; g_cfg.unitsPerM = sup;
            vrin::moveYaw = smy; vrin::moveYawOk = smo; fp::speedMs = 0; fp::moveOffDeg = 0;
            ok &= mt; logf("moving body test (past the threshold while running the body turns to the view and the movement forward stays; a new forward clears the turn): %s (body %.1f deg)", mt ? "PASS" : "FAIL", mb * 57.2957795f);
        }
        {   // 0.18.14 stance squared: hips / shoulders 15 deg to the right -> turned back about the head
            static float Q[8 * 16]; memset(Q, 0, sizeof Q); for (int j = 0; j < 8; j++) { Q[j * 16] = Q[j * 16 + 5] = Q[j * 16 + 10] = Q[j * 16 + 15] = 1.0f; }
            const float a = -15.0f / 57.2957795f, lx = -sinf(a) * 10.0f, ly = cosf(a) * 10.0f;   // the left direction turned by -15 deg
            Q[0 * 16 + 12] = 5.0f; Q[0 * 16 + 14] = 170.0f; Q[1 * 16 + 12] = lx; Q[1 * 16 + 13] = ly; Q[1 * 16 + 14] = 90.0f; Q[2 * 16 + 12] = -lx; Q[2 * 16 + 13] = -ly; Q[2 * 16 + 14] = 90.0f;
            const int sk = fp::hpubIdx.load(); const fp::HidePub sh0 = fp::hpub[0]; fp::hpub[0].head = 0; fp::hpubIdx = 0;
            const int sl = fp::legsReady.load(), sa2 = fp::armsReady.load(); const int h0 = fp::legsD[0].hip, h1 = fp::legsD[1].hip; fp::legsReady = 1; fp::armsReady = 0; fp::legsD[0].hip = 1; fp::legsD[1].hip = 2;
            const int ssq = g_fp.stanceSquare; g_fp.stanceSquare = 1; fp::idleSquare(Q, 8);
            bool sq = fabsf(Q[1 * 16 + 12] - Q[2 * 16 + 12]) < 1e-3f && Q[1 * 16 + 13] > Q[2 * 16 + 13] && fabsf(fp::idleSquareDeg.load() + 15.0f) < 0.01f && Q[0 * 16 + 12] == 5.0f;
            fp::hpub[0] = sh0; fp::hpubIdx = sk; fp::legsReady = sl; fp::armsReady = sa2; fp::legsD[0].hip = h0; fp::legsD[1].hip = h1; g_fp.stanceSquare = ssq;
            ok &= sq; logf("stance squared test (a stance turned 15 deg to the right is turned back to face forward about the head): %s", sq ? "PASS" : "FAIL");
        }
        {   // 0.18.11 near plane: a projection built with the game's near or already with ours both end at ours, +0x664 follows, garbage is left alone, the game's near returns after
            static u8 cb[0x700]; memset(cb, 0, sizeof cb); float *pn = (float *)(cb + 0x664); *pn = 10.0f; *(float *)(cb + 0x668) = 1e7f;
            const float snc = g_fp.nearCm, sup = g_cfg.unitsPerM; g_fp.nearCm = 2.0f; g_cfg.unitsPerM = 100.0f; const float f = 1e7f, sw = -1.0f;
            auto mk = [&](float *g, float n) { memset(g, 0, 64); g[10] = -sw * n / (f - n); g[14] = n * f / (f - n); g[11] = sw; };
            float g[16]; const u64 h0 = fp::nearHits[0].load(), m0 = fp::nearMiss.load(); g_pairPhase = 0;
            mk(g, 10.0f); nearp::apply(g, sw, cb, true); bool nt = fabsf(g[14] - 2.0f) < 1e-3f && *pn == 2.0f;
            *pn = 10.0f; mk(g, 2.0f); nearp::apply(g, sw, cb, true); nt &= fabsf(g[14] - 2.0f) < 1e-3f && *pn == 2.0f;       // the game reset its near after rebuilding with ours
            mk(g, 37.0f); nearp::apply(g, sw, cb, true); nt &= fabsf(g[14] - 37.0f) < 1e-2f && fp::nearMiss.load() == m0 + 1;   // not a projection we know: untouched
            nt &= fp::nearHits[0].load() == h0 + 2;
            mk(g, 10.0f); nearp::apply(g, sw, cb, false); nt &= *pn == 10.0f && fabsf(g[14] - 10.0f) < 1e-3f && !nearp::nearSet;   // first person ended: the game's own near
            g_fp.nearCm = snc; g_cfg.unitsPerM = sup; ok &= nt;
            logf("near plane test (game-built or ours accepted on every tick, camera near follows, unknown projection untouched, restored after first person): %s", nt ? "PASS" : "FAIL");
        }
        {   // 0.18.18 VR fingers: chains + thumb + index found, bind fingers placed on a turned hand (animation ignored), grip curls toward the palm
            static int16_t par[40]; static float Q[40 * 16], Bp[40 * 16]; int nj = 0;
            auto put = [&](int parent, float x, float y, float z) { float *m = Q + (size_t)nj * 16; memset(m, 0, 64); m[0] = m[5] = m[10] = m[15] = 1; m[12] = x; m[13] = y; m[14] = z; par[nj] = (int16_t)parent; return nj++; };
            put(-1, 0, 0, 0);   // 0 the hand
            int th0 = put(0, 3, 4, -1), th1 = put(th0, 5, 5, -1); put(th1, 7, 6, -1);   // the thumb (toward +y)
            const int palmJ = put(0, 4, 0, 0);   // 0.18.19: a palm joint the four long fingers share, and helper leaves on the fingers
            int tips[4]; for (int f = 0; f < 4; f++) { const float y = 3.0f - 2.0f * f; int b0 = put(palmJ, 8, y, 0); put(b0, 8.5f, y, 0.6f); int b1 = put(b0, 11, y, 0); tips[f] = put(b1, 13, y, 0); put(b1, 11.3f, y, 0.4f); }
            const int sock = put(0, 4, 0, -3);   // a weapon socket under the hand (not a finger)
            memcpy(Bp, Q, sizeof Q);   // the bind: straight fingers
            for (int j = 1; j < nj; j++) if (par[j] > 0) { Q[(size_t)j * 16 + 14] -= 2.0f * (j % 3); Q[(size_t)j * 16 + 12] -= 0.5f; }   // the animation: bent fingers (lengths roughly kept)
            fbik::Arm A; A.hand = 0; A.frame = true; A.fl[0] = 1; A.tl[1] = 1; A.nSub[3] = nj; for (int j = 0; j < nj; j++) A.sub[3][j] = (uint16_t)j;
            for (int j = 1; j < nj; j++) if (par[j] > 0) { float *m = Q + (size_t)j * 16; const float *pp = Q + (size_t)par[j] * 16, *bm = Bp + (size_t)j * 16, *bp = Bp + (size_t)par[j] * 16;   // keep the animated lengths = the bind lengths
                const float bl = sqrtf((bm[12] - bp[12]) * (bm[12] - bp[12]) + (bm[13] - bp[13]) * (bm[13] - bp[13]) + (bm[14] - bp[14]) * (bm[14] - bp[14]));
                float d[3] = {m[12] - pp[12], m[13] - pp[13], m[14] - pp[14]}; fbik::norm3(d); m[12] = pp[12] + d[0] * bl; m[13] = pp[13] + d[1] * bl; m[14] = pp[14] + d[2] * bl; }
            auto bindOf = [&](int j, fing::Aff &o) -> bool { if (j < 0 || j >= nj) return false; o = fing::fromPose(Bp + (size_t)j * 16); return true; };
            static fing::Hand H; char fw[200]; bool fg = fing::analyse(H, par, nj, Q, A, bindOf, fw, sizeof fw) && H.nf == 5;
            int nth = 0, nix = 0; for (int c = 0; c < H.nf; c++) { nth += H.f[c].thumb; nix += H.f[c].index; if (H.f[c].thumb) fg &= H.f[c].j[0] == th0; if (H.f[c].index) fg &= H.f[c].j[0] == tips[0] - 3 && H.f[c].n == 3; }
            fg &= nth == 1 && nix == 1;
            // turn the hand 90 deg (yaw) and move it; the animated fingers stay as they were relative to it
            const float sockOld[3] = {Q[sock * 16 + 12], Q[sock * 16 + 13], Q[sock * 16 + 14]};
            { const fbik::Rot R = fbik::axisAngle((const float[3]){0, 0, 1}, 1.5707963f); const float pv[3] = {0, 0, 0}; fbik::moveSet(Q, A.sub[3], nj, R, pv); for (int j = 0; j < nj; j++) Q[(size_t)j * 16 + 13] += 100.0f; }
            fing::Tune tn; tn.relaxed = 0.0f; fing::Curl open; const float palm[3] = {0, 0, -1};
            fing::apply(Q, H, open, palm, 1.0f, tn);
            // straight bind fingers on the turned hand: the index tip at hand + R (13, 3, 0) = (-3, 113, 0)
            fg &= fabsf(Q[tips[0] * 16 + 12] + 3.0f) < 1e-3f && fabsf(Q[tips[0] * 16 + 13] - 113.0f) < 1e-3f && fabsf(Q[tips[0] * 16 + 14]) < 1e-3f;
            fg &= fabsf(Q[sock * 16 + 12] - (-sockOld[1])) < 1e-3f && fabsf(Q[sock * 16 + 13] - (100.0f + sockOld[0])) < 1e-3f && fabsf(Q[sock * 16 + 14] - sockOld[2]) < 1e-3f;   // the socket kept its animated offset
            fing::Curl fist; fist.grip = fist.index = fist.thumb = 1.0f; const float baseZ = Q[(tips[1] - 3) * 16 + 14];
            fing::apply(Q, H, fist, palm, 1.0f, tn);
            const float *tp = Q + (size_t)tips[1] * 16 + 12, *mp = Q + (size_t)(tips[1] - 1) * 16 + 12, *bp = Q + (size_t)(tips[1] - 3) * 16 + 12;
            const float l1 = sqrtf((mp[0] - bp[0]) * (mp[0] - bp[0]) + (mp[1] - bp[1]) * (mp[1] - bp[1]) + (mp[2] - bp[2]) * (mp[2] - bp[2]));
            fg &= tp[2] < baseZ - 2.0f && fabsf(l1 - 3.0f) < 1e-3f;   // curled toward the palm (down), lengths kept
            ok &= fg; logf("VR fingers test (finger chains, thumb + index, bind fingers on the IK hand regardless of the animation, sockets keep their offset, a fist curls toward the palm): %s (%s)", fg ? "PASS" : "FAIL", fw);
        }
        {   // 0.18.21 Jin's own hands (bind + animated joints from the 0.18.20 live log, hand-local cm): grip = a fist with every finger in its
            // own plane (0.18.20 bent the middle joints sideways), tips on the palm; thumb touch = the thumb across the curled index / middle
            struct J { int p; float a[3], b[3]; };
            static const J L[] = {   // index 0 = the hand (513); parents are indices into this table
                {-1, {0, 0, 0}, {0, 0, 0}},
                {0, {0, 0, 0}, {0, 0, 0}}, {1, {0, 50, 0}, {0, 50, 0}},                                                 // 514, 515 helper
                {0, {3.0f, -0.1f, -1.7f}, {3.0f, -0.1f, -1.8f}}, {3, {10.0f, 0.4f, 0.2f}, {9.9f, 0.3f, 0.1f}}, {4, {10.9f, 3.6f, 1.3f}, {13.3f, 0.6f, 0.8f}}, {5, {8.9f, 3.2f, 0.9f}, {15.3f, 0.7f, 1.2f}}, {3, {10.1f, -1.0f, 0.8f}, {10.0f, -1.3f, 0.5f}},   // index 516-520
                {0, {3.3f, -0.8f, -0.3f}, {3.3f, -0.8f, -0.3f}}, {8, {9.5f, -0.1f, 2.3f}, {9.4f, -0.2f, 2.2f}}, {9, {10.5f, 3.9f, 3.1f}, {13.3f, 0.2f, 3.6f}}, {10, {8.3f, 3.4f, 2.4f}, {15.6f, 0.5f, 4.4f}},   // middle 521-524
                {0, {1.6f, -0.5f, 2.0f}, {1.7f, -0.5f, 2.0f}}, {12, {6.9f, 0.4f, 5.7f}, {6.8f, 0.3f, 5.6f}}, {13, {7.8f, 3.3f, 5.8f}, {9.4f, 0.7f, 7.3f}}, {14, {6.4f, 2.8f, 4.7f}, {10.9f, 0.9f, 8.3f}}, {12, {7.5f, -0.9f, 5.8f}, {7.3f, -1.2f, 5.6f}},   // little 525-529
                {0, {2.3f, -0.8f, 1.4f}, {2.3f, -0.8f, 1.3f}}, {17, {8.4f, 0.0f, 4.2f}, {8.4f, 0.0f, 4.1f}}, {18, {9.3f, 3.5f, 4.7f}, {11.6f, 0.4f, 5.8f}}, {19, {7.0f, 2.9f, 3.5f}, {14.0f, 0.7f, 7.0f}},   // ring 530-533
                {0, {3.3f, 1.2f, -1.6f}, {3.3f, 1.2f, -1.6f}}, {21, {7.1f, 2.8f, -2.3f}, {6.9f, 2.9f, -2.6f}}, {22, {9.6f, 3.9f, -1.5f}, {9.5f, 4.2f, -3.2f}},   // thumb 534-536
                {0, {7.2f, 0.1f, 2.2f}, {7.2f, 0.0f, 2.2f}} };   // socket 537
            const int nj = (int)(sizeof L / sizeof L[0]); bool jf = true; char fw2[2][200] = {{0}, {0}}; float rep[4] = {0, 0, 0, 0};
            for (int side = 0; side < 2; side++) {   // the right hand is the left mirrored through the hand (x, y and z negated, as in the log)
                const float sx = side ? -1.0f : 1.0f; static int16_t par[32]; static float Q[32 * 16], Bp[32 * 16];
                for (int j = 0; j < nj; j++) { float *m = Q + (size_t)j * 16, *b = Bp + (size_t)j * 16; memset(m, 0, 64); memset(b, 0, 64); m[0] = m[5] = m[10] = m[15] = b[0] = b[5] = b[10] = b[15] = 1;
                    m[12] = L[j].a[0] * sx; m[13] = L[j].a[1] * sx; m[14] = L[j].a[2] * sx; b[12] = L[j].b[0] * sx; b[13] = L[j].b[1] * sx; b[14] = L[j].b[2] * sx; par[j] = (int16_t)L[j].p; }
                fbik::Arm A; A.hand = 0; A.frame = true; A.fl[0] = sx; A.tl[1] = sx; A.nSub[3] = nj; for (int j = 0; j < nj; j++) A.sub[3][j] = (uint16_t)j;
                auto bindOf = [&](int j, fing::Aff &o) -> bool { if (j < 0 || j >= nj) return false; o = fing::fromPose(Bp + (size_t)j * 16); return true; };
                static fing::Hand H; jf &= fing::analyse(H, par, nj, Q, A, bindOf, fw2[side], sizeof fw2[side]) && H.nf == 5;
                if (!H.ok) continue;
                int thC = -1, ixC = -1; for (int c = 0; c < H.nf; c++) { if (H.f[c].thumb) thC = c; if (H.f[c].index) ixC = c; }
                jf &= thC >= 0 && H.f[thC].j[0] == 21 && ixC >= 0 && H.f[ixC].j[0] == 3;
                fing::Tune tn; fing::Curl fist; fist.grip = fist.index = fist.thumb = 1.0f; const float palm[3] = {0, 0, 1};
                fing::apply(Q, H, fist, palm, 1.0f, tn);
                const int knuck[4] = {4, 9, 13, 18}, endJ[4] = {6, 11, 15, 20};
                for (int f = 0; f < 4; f++) {   // each long finger: its end joint in front of the palm (+y left / -y right), not off to the side
                    const float *e = Q + (size_t)endJ[f] * 16 + 12; const float y = e[1] * sx, x = e[0] * sx, z = e[2] * sx, dz = fabsf(z - L[knuck[f]].b[2]);
                    const bool okf = y > 2.0f && y < 5.0f && x > 4.0f && x < 11.0f && dz < 1.5f; jf &= okf;
                    if (!okf) logf("   Jin fingers: %s finger %d end joint at (%.1f %.1f %.1f) - not a fist", side ? "right" : "left", f, x, y, z);
                }
                const float *ip = Q + 23 * 16 + 12; const float ipx = ip[0] * sx, ipy = ip[1] * sx, ipz = ip[2] * sx;   // the thumb's end joint over the curled index / middle
                const bool okt = ipy > 3.8f && ipz > -1.0f && ipz < 2.5f && ipx > 6.5f && ipx < 10.5f; jf &= okt;
                if (!side) { rep[0] = ipx; rep[1] = ipy; rep[2] = ipz; rep[3] = Q[6 * 16 + 13]; }
                if (!okt) logf("   Jin fingers: %s thumb end joint at (%.1f %.1f %.1f) - not across the fist", side ? "right" : "left", ipx, ipy, ipz);
            }
            {   // the tween: a critically damped spring reaches ~90 %% in fp_finger_tween_s, never overshoots, is still on the way after a frame
                float x = 0, v = 0; const float om = 3.9f / 0.15f; bool sp = true; float mx = 0; int n = 0;
                for (float t = 0; t < 0.6f; t += 0.011f, n++) { fing::spring(x, v, 1.0f, om, 0.011f); mx = fmaxf(mx, x); if (n == 0) sp &= x < 0.1f; if (n == 13) sp &= x > 0.85f && x < 0.97f; }
                sp &= mx <= 1.0f + 1e-5f && x > 0.999f; jf &= sp; if (!sp) logf("   finger tween spring: FAIL (max %.4f end %.4f)", mx, x);
            }
            ok &= jf; logf("Jin's hands test (grip = a fist on his own bind fingers, each finger bending in its own plane, the thumb across the fist, both hands; the tween eases in and settles): %s (left thumb end %.1f %.1f %.1f, index end height %.1f; %s)", jf ? "PASS" : "FAIL", rep[0], rep[1], rep[2], rep[3], fw2[0]);
        }
        {   // 0.18.24 hand stabilisation (stab.h): a still hand's jitter shrinks, a moving hand is followed closely, every discontinuity resets
            stab::Filt F; stab::Cfg c; float q[4], p[3]; const float dt = 1.0f / 90.0f; uint32_t rs = 12345; auto rnd = [&]() { rs = rs * 1664525u + 1013904223u; return ((rs >> 8) / 16777216.0f) - 0.5f; };
            double eRaw = 0, eOut = 0; bool st = true;
            float pr[3] = {0, 0, 0}, po[3] = {0, 0, 0};   // 0.18.26: jitter = frame-to-frame motion (the deadband may hold a still hand a mm off centre)
            for (int i = 0; i < 400; i++) { q[0] = 0; q[1] = 0; q[2] = 0; q[3] = 1; p[0] = 0.002f * rnd(); p[1] = 1.0f + 0.002f * rnd(); p[2] = -0.3f + 0.002f * rnd();
                const float r0[3] = {p[0], p[1], p[2]}; stab::step(F, q, p, true, dt, c);
                if (i > 100) for (int k = 0; k < 3; k++) { eRaw += (r0[k] - pr[k]) * (r0[k] - pr[k]); eOut += (p[k] - po[k]) * (p[k] - po[k]); }
                for (int k = 0; k < 3; k++) { pr[k] = r0[k]; po[k] = p[k]; } }
            const float ratio = (float)sqrt(eOut / eRaw); st &= ratio < 0.5f;
            float lag = 0; for (int i = 0; i < 90; i++) { q[0] = 0; q[1] = 0; q[2] = 0; q[3] = 1; const float x = 0.01f + i * dt * 1.0f; p[0] = x; p[1] = 1.0f; p[2] = -0.3f; stab::step(F, q, p, true, dt, c); lag = x - p[0]; }   // 1 m/s
            st &= lag < 0.02f && lag > 0.0f;
            p[0] = 0.6f; p[1] = 1.0f; p[2] = -0.3f; st &= stab::step(F, q, p, true, dt, c) == stab::R_JUMP && p[0] == 0.6f;   // a 50 cm jump: raw
            p[0] = 0.61f; st &= stab::step(F, q, p, true, dt, c, stab::R_RECENTER) == stab::R_RECENTER && p[0] == 0.61f;
            p[0] = NAN; st &= stab::step(F, q, p, true, dt, c) == stab::R_NONFINITE && !F.init;
            p[0] = 0.2f; st &= stab::step(F, q, p, true, dt, c) == stab::R_FIRST && p[0] == 0.2f;
            p[0] = 0.2f; st &= stab::step(F, q, p, true, 0.5f, c) == stab::R_DT;
            {   // rotation: 0.3 deg jitter about a still orientation shrinks
                stab::Filt G; double aR = 0, aO = 0; const float base[4] = {0, 0.3826834f, 0, 0.9238795f};
                for (int i = 0; i < 300; i++) { const float e = 0.3f / 57.2957795f * rnd(); float qq[4] = {base[0] + e * 0.5f, base[1], base[2] + e * 0.5f, base[3]}; stab::qnorm(qq); const float ar = stab::qangle(qq, base);
                    static float lr[4], lo[4]; float raw4[4] = {qq[0], qq[1], qq[2], qq[3]}; (void)ar;
                    float pp[3] = {0, 1, 0}; stab::step(G, qq, pp, true, dt, c); if (i > 100) { const float a1 = stab::qangle(raw4, lr), a2 = stab::qangle(qq, lo); aR += a1 * a1; aO += a2 * a2; }
                    memcpy(lr, raw4, 16); memcpy(lo, qq, 16); }
                st &= sqrt(aO / aR) < 0.6;
            }
            ok &= st; logf("hand stabilisation test (One Euro: still-hand jitter x%.2f, lag at 1 m/s %.1f cm, resets on jump / recenter / non-finite / first pose / bad time step): %s", ratio, lag * 100.0f, st ? "PASS" : "FAIL");
        }
        {   // 0.18.24 flat play observer: a hero flag flipping is logged with old / new values; a field changing every tick is set aside
            static u8 hb[0x8000]; memset(hb, 0, sizeof hb); *(int *)(hb + 0x200) = 3; float *M = (float *)(hb + 0x150); M[0] = M[5] = M[10] = M[15] = 1.0f;
            const auto shp = fp::heroPtr; fp::heroPtr = []() -> u8 * { return hb; }; obs::on = 1; obs::have = false; obs::nEv = 0; obs::nNoisy = 0; memset(obs::noisy, 0, sizeof obs::noisy);
            obs::tick();                                                     // snapshot
            *(int *)(hb + 0x200) = 7; *(float *)(hb + 0x300) = 1.5f; obs::tick();   // a flag 3 -> 7 (logged); a float (not a flag: ignored)
            bool ob = obs::nEv == 1 && obs::ev[0].off == 0x200 && obs::ev[0].a == 3 && obs::ev[0].b == 7;
            for (int k = 0; k < 10; k++) { *(int *)(hb + 0x204) = k + 1; obs::tick(); }   // a counter
            obs::secQ.QuadPart -= obs::fq.QuadPart; obs::tick();                            // the second is over: the counter is set aside
            ob &= obs::noisy[0x204 / 4] == 1 && obs::noisy[0x200 / 4] == 0 && obs::nEv == 0;
            fp::heroPtr = shp; obs::on = 0; obs::have = false; obs::lastHero = nullptr; obs::nNoisy = 0; memset(obs::noisy, 0, sizeof obs::noisy); obs::secQ.QuadPart = 0; obs::lastQ.QuadPart = 0;
            ok &= ob; logf("flat play observer test (a hero flag change logged with old / new value, floats ignored, a per-tick counter set aside): %s", ob ? "PASS" : "FAIL");
        }
        {   // 0.18.26 katana draw state machine: sheath + hilt grabs with hysteresis, the blade rides the rail, rumbles while moving, frees at the top
            sword::Sys S2; sword::Cfg c; c.drawCm = c.drawnFrac * c.bladeCm; c.needSheathHand = 1; c.releaseHome = 1;   /* 0.18.40: the 0.18.26 return on let-go, as an option */ sword::In in{}; sword::Out o{}; in.unitsPerCm = 1.0f; in.dt = 1.0f / 90.0f; in.valid[0] = in.valid[1] = true; in.axis[0] = 1;
            auto at = [&](int h, float x, float y, float g) { in.hand[h][0] = x; in.hand[h][1] = y; in.hand[h][2] = 0; in.grip[h] = g; };
            at(0, 0, 0, 0.0f); at(1, 50, 30, 0.0f); S2.step(in, c, o); bool sw = !S2.onSheath && !S2.onHilt;
            at(0, 2, 0, 0.8f); S2.step(in, c, o); sw &= S2.onSheath && o.pulseHand[0];                                   // left hand takes the sheath (a pulse)
            at(0, 15, 0, 0.8f); S2.step(in, c, o); sw &= S2.onSheath;                                                   // 15 cm: still held (leaves at 21)
            at(0, 2, 0, 0.8f); at(1, 12.5f, 2, 0.8f); S2.step(in, c, o); sw &= S2.onHilt && S2.s == 0.0f && o.pulseHand[1];   // right hand on the hilt
            at(1, 40.0f, 2, 0.8f); S2.step(in, c, o); sw &= S2.state == sword::S_DRAWING && fabsf(S2.s - 27.5f) < 0.01f && o.rumble > 0.0f;
            at(1, 30.0f, 2, 0.8f); S2.step(in, c, o); sw &= fabsf(S2.s - 17.5f) < 0.01f;                                 // back along the rail
            at(1, 12.5f + 70.0f, 2, 0.8f); S2.step(in, c, o); sw &= S2.state == sword::S_DRAWN && o.pulseAmp[1] > 0.8f;    // near the top: free
            at(1, 0, 60, 0.8f); S2.step(in, c, o); sw &= S2.state == sword::S_DRAWN;                                      // in the hand anywhere
            at(1, 0, 60, 0.0f); S2.step(in, c, o); sw &= S2.state == sword::S_SHEATHED && S2.s == 0.0f;                   // let go (stage 1: back in the sheath)
            sword::Sys S3; at(0, 50, 50, 0.0f); at(1, 12.5f, 0, 0.8f); S3.step(in, c, o); at(1, 40, 0, 0.8f); S3.step(in, c, o); sw &= S3.onHilt && S3.s == 0.0f;   // no hand on the sheath: no slide
            {   // 0.18.28 the hilt is a segment: grabbed near the pommel (22 cm out) as well as near the guard; 12 cm to the side is too far
                sword::Sys S4; at(0, 2, 0, 0.8f); at(1, 22.0f, 6, 0.8f); S4.step(in, c, o); sw &= S4.onSheath && S4.onHilt && S4.s == 0.0f;
                at(1, 50.0f, 6, 0.8f); S4.step(in, c, o); sw &= fabsf(S4.s - 28.0f) < 0.01f;   // no jump at the grab: the blade comes out as far as the hand moved
                sword::Sys S5; at(1, 12.5f, 12, 0.8f); S5.step(in, c, o); sw &= !S5.onHilt;
                if (!sw) logf("   katana hilt segment: FAIL (S4 on %d/%d s %.2f, S5 %d)", (int)S4.onSheath, (int)S4.onHilt, S4.s, (int)S5.onHilt);
            }
            {   // 0.18.29 the draw gain (40 cm of pull frees the blade), sliding home when let go, sheathing by the tip, pulling back out
                sword::Cfg g; sword::Sys S6; bool gk = true; at(0, 2, 0, 0.8f); at(1, 12.5f, 2, 0.8f); S6.step(in, g, o);
                at(1, 32.5f, 2, 0.8f); S6.step(in, g, o); gk &= S6.state == sword::S_DRAWING && fabsf(S6.s - 20.0f * g.gain()) < 0.05f;
                at(1, 12.5f + 40.5f, 2, 0.8f); S6.step(in, g, o); gk &= S6.state == sword::S_DRAWN;
                at(1, 53.0f, 2, 0.0f); S6.step(in, g, o); gk &= S6.state == sword::S_DRAWN; at(1, 53.0f, 2, 0.8f);   // 0.18.40: let go - still in the hand
                in.bladeOk = true; const float cA = cosf(0.7854f), sA = sinf(0.7854f);
                in.tip[0] = 2; in.tip[1] = 0; in.tip[2] = 0; in.dir[0] = cA; in.dir[1] = sA; in.dir[2] = 0; S6.step(in, g, o); gk &= S6.state == sword::S_DRAWN;   // 45 deg off: not taken
                in.dir[0] = 1; in.dir[1] = 0; S6.step(in, g, o); gk &= S6.state == sword::S_SHEATHING && fabsf(S6.s - 74.0f) < 0.01f;         // lined up at the mouth: on the rail where the blade is (0.18.40: no clamp, no jump)
                in.bladeOk = false; at(1, 53.0f - 20.0f, 2, 0.0f); S6.step(in, g, o); gk &= S6.state == sword::S_SHEATHING && fabsf(S6.s - (74.0f - 20.0f * g.gain())) < 0.05f;   // pushed in (grip or not)
                at(1, 53.0f - 45.0f, 2, 0.8f); S6.step(in, g, o); gk &= S6.state == sword::S_SHEATHED && S6.s == 0.0f && S6.onHilt;           // home, the hand still on the hilt
                at(1, 8.0f + 21.0f, 2, 0.8f); S6.step(in, g, o); gk &= S6.state == sword::S_DRAWING;                                            // draw again at once
                const float sKeep = S6.s; at(1, 29.0f, 2, 0.0f); S6.step(in, g, o); gk &= S6.state == sword::S_DRAWING && !S6.homing;           // 0.18.40 let go: the blade stays put
                for (int t = 0; t < 30; t++) S6.step(in, g, o); gk &= S6.state == sword::S_DRAWING && fabsf(S6.s - sKeep) < 0.01f;
                { sword::Cfg ga = g; ga.autoHome = 1; at(1, 29.0f, 2, 0.8f); S6.step(in, ga, o); gk &= S6.onHilt; at(1, 29.0f, 2, 0.0f); S6.step(in, ga, o); gk &= S6.homing;   // sword_auto_home = 1: it slides home
                  for (int t = 0; t < 30; t++) S6.step(in, ga, o); gk &= S6.state == sword::S_SHEATHED && S6.s == 0.0f; }
                {   // 0.18.40 the tip taken at the REAL mouth (mouthAx), up to 25 cm inside it; 10 cm outside is not yet
                    sword::Cfg gm = g; gm.mouthAx = 13.0f; sword::Sys S13; at(1, 12.5f, 2, 0.8f); S13.step(in, gm, o); at(1, 53.0f, 2, 0.8f); S13.step(in, gm, o); gk &= S13.state == sword::S_DRAWN;
                    in.bladeOk = true; in.dir[0] = 1; in.dir[1] = 0; in.tip[0] = 23; S13.step(in, gm, o); gk &= S13.state == sword::S_DRAWN;
                    in.tip[0] = -7; S13.step(in, gm, o); gk &= S13.state == sword::S_SHEATHING && fabsf(S13.s - 65.0f) < 0.01f; in.bladeOk = false; in.tip[0] = 2; }
                // pulled back out of a sheathing
                sword::Sys S7; at(1, 12.5f, 2, 0.8f); S7.step(in, g, o); at(1, 53.0f, 2, 0.8f); S7.step(in, g, o); in.bladeOk = true; in.dir[0] = 1; S7.step(in, g, o); in.bladeOk = false;
                at(1, 53.0f + 7.0f, 2, 0.8f); S7.step(in, g, o); gk &= S7.state == sword::S_DRAWN;
                if (!gk) logf("   katana gain / sheathe: FAIL (S6 %d s %.2f, S7 %d)", S6.state, S6.s, S7.state);
                in.bladeOk = false; sw &= gk;
                // the hand lock's partial rotation: none at 0, all at 1, half twice = all
                const fbik::Rot R = fbik::axisAngle((const float[3]){0.267f, 0.535f, 0.802f}, 1.2f); float R0[9], R1[9], Rh[9], Rhh[9]; swd::rotPart(R.m, 0.0f, R0); swd::rotPart(R.m, 1.0f, R1); swd::rotPart(R.m, 0.5f, Rh);
                for (int r = 0; r < 3; r++) for (int cc = 0; cc < 3; cc++) Rhh[r * 3 + cc] = Rh[r * 3] * Rh[cc] + Rh[r * 3 + 1] * Rh[3 + cc] + Rh[r * 3 + 2] * Rh[6 + cc];
                float e0 = 0, e1 = 0, e2 = 0; for (int i = 0; i < 9; i++) { e0 = fmaxf(e0, fabsf(R0[i] - ((i % 4 == 0) ? 1.0f : 0.0f))); e1 = fmaxf(e1, fabsf(R1[i] - R.m[i])); e2 = fmaxf(e2, fabsf(Rhh[i] - R.m[i])); }
                const bool rp = e0 < 1e-5f && e1 < 2e-3f && e2 < 2e-3f; sw &= rp; if (!rp) logf("   hand lock rotation blend: FAIL (%.5f %.5f %.5f)", e0, e1, e2);
            }
            {   // 0.18.30 right hand only; past half the blade an arm at 90 % reach frees it; the fist clear of the guard; the finger circle centre
                sword::Cfg g; sword::Sys S8; bool fk = true; at(0, 80, 80, 0.0f); at(1, 12.5f, 2, 0.8f); in.armExt = 0.5f; S8.step(in, g, o); fk &= S8.onHilt && !S8.onSheath && fabsf(S8.hiltT - (g.guardCm + g.rightCm)) < 0.01f;
                at(1, 12.5f + 28.0f, 2, 0.8f); S8.step(in, g, o); fk &= S8.state == sword::S_DRAWING && S8.s > g.freeMinFrac * g.bladeCm;   // no left hand needed
                in.armExt = 0.95f; S8.step(in, g, o); fk &= S8.state == sword::S_DRAWN; in.armExt = 0.0f;
                sword::Sys S9; at(1, -5.0f, 2, 0.8f); S9.step(in, g, o); fk &= S9.onHilt && fabsf(S9.hiltT - (g.guardCm + g.rightCm)) < 0.01f;
                {   // 0.18.38 the guard away from the rail's origin: the hilt is guardCm .. guardCm + hiltCm, the fist rightCm below the guard, clamped
                    sword::Cfg g2 = g; g2.guardCm = 15.0f; g2.rightCm = 9.0f; sword::Sys S10; at(1, 5.0f, 2, 0.8f); S10.step(in, g2, o); fk &= !S10.onHilt;   // 10 cm short of the guard: not the hilt
                    at(1, 30.0f, 2, 0.8f); S10.step(in, g2, o); fk &= S10.onHilt && fabsf(S10.hiltT - 24.0f) < 0.01f;
                    g2.rightCm = 60.0f; sword::Sys S11; S11.step(in, g2, o); fk &= S11.onHilt && fabsf(S11.hiltT - (15.0f + g2.hiltCm - g2.handHalfCm)) < 0.01f;
                    g2.leftAx = 10.0f; sword::Sys S12; at(0, 10.0f, 3, 0.8f); S12.step(in, g2, o); fk &= S12.onSheath; at(0, 0.0f, 0, 0.0f); }
                const float a[3] = {2, 0, 0}, b[3] = {0, 2, 0}, e[3] = {-2, 0, 0}; float cc[3]; fk &= fp::circumC(a, b, e, cc) && fabsf(cc[0]) < 1e-4f && fabsf(cc[1]) < 1e-4f && fabsf(cc[2]) < 1e-4f;
                const float a2[3] = {11, 5, 3}, b2[3] = {10, 6, 3}, e2[3] = {9, 5, 3}; fk &= fp::circumC(a2, b2, e2, cc) && fabsf(cc[0] - 10) < 1e-3f && fabsf(cc[1] - 5) < 1e-3f;
                if (!fk) logf("   katana one-hand / reach / fist circle: FAIL (S8 %d s %.1f, S9 hiltT %.2f)", S8.state, S8.s, S9.hiltT);
                sw &= fk;
            }
            {   // 0.18.47 the katana-hit decision core (melee.h): a fast slash hits once per pass, a slow push / walking the blade in does not,
                // an enemy running onto it does, re-arm after leaving, a hitch is no swing, the target table never fills
                const bool mk = melee::selfTest(); if (!mk) logf("   katana hits decision core: FAIL"); sw &= mk; }
            {   // WRIST_HUD.md framework: the wrist vitals core (fills, change edges, heartbeat, glance, hand at the head)
                const bool vk = vit::selfTest(); if (!vk) logf("   wrist vitals core: FAIL"); sw &= vk; }
            {   // 0.18.48 the attack intent (attack.h): swing recognition and re-arm, heavy charge (two hands + trigger held still), lunge distances / cooldown / stop short, katana weight
                const bool ak = atk::selfTest(); if (!ak) logf("   katana attack intent core: FAIL"); sw &= ak;
                const bool pk = parry::selfTest(); if (!pk) logf("   katana block / deflect core: FAIL"); sw &= pk; }
            {   // 0.18.31 the rail from the two joints: the child along the calibrated line -> mouth at the nearer end, axis to the far end
                const float mC[3] = {-3.6f, -3.3f, -6.7f}, aC[3] = {0.005f, 0.188f, -0.982f}; float mO[3], aO[3], ang, d0, d1;
                const float c1[3] = {0, 2, -25}; bool rj = swd::railFromJoints(mC, aC, c1, 1.0f, mO, aO, ang, d0, d1) && mO[2] == 0.0f && aO[2] < -0.9f;
                const float c2[3] = {20, 0, 0}; rj &= !swd::railFromJoints(mC, aC, c2, 1.0f, mO, aO, ang, d0, d1);   // across the line: the calibration stays
                const float c3[3] = {0, 0, -5}; rj &= !swd::railFromJoints(mC, aC, c3, 1.0f, mO, aO, ang, d0, d1);    // too close
                if (!rj) logf("   katana rail from joints: FAIL"); sw &= rj;
            }
            {   // 0.18.32 two hands: 30 deg off -> the hand line exactly; 120 deg off -> compressed under 90 deg; fade 0 -> unchanged
                const float d[3] = {1, 0, 0}; const float L1[3] = {cosf(0.5236f), sinf(0.5236f), 0}, L2[3] = {cosf(2.094f), sinf(2.094f), 0}; float o1[3], o2[3], o3[3];
                fp::twoSteer(d, L1, 1.0f, o1); fp::twoSteer(d, L2, 1.0f, o2); fp::twoSteer(d, L2, 0.0f, o3);
                const float a1 = acosf(fminf(1.0f, o1[0])) * 57.2957795f, a2 = acosf(fminf(1.0f, o2[0])) * 57.2957795f;
                const bool ts = fabsf(a1 - 30.0f) < 0.1f && a2 > 55.0f && a2 < 90.0f && fabsf(o3[0] - 1.0f) < 1e-5f; sw &= ts;
                if (!ts) logf("   two-hand steer: FAIL (%.1f, %.1f deg)", a1, a2);
            }
            ok &= sw; logf("katana draw test (grab sheath + hilt with hysteresis and pulses, blade rides the rail with rumble, free at the top, no slide without the sheath hand; 0.18.29: draw gain, slide home, sheathing by the tip, pulling back out, hand lock rotation; 0.18.30: right hand alone, freed at arm reach, fist clear of the guard, finger circle centre): %s", sw ? "PASS" : "FAIL");
        }
        {   // 0.18.28 hold fingers: holdW 1 -> the hilt / sheath tables, full curl, the index wrapped too; 0 -> untouched; the game draw chord queued
            const int sk = fp::spubIdx.load(); fp::SwordPub sv0 = fp::spub[0]; fp::spub[0] = fp::SwordPub{}; fp::spub[0].holdW[0] = 1.0f; fp::spub[0].holdW[1] = 0.5f; fp::spubIdx = 0;
            fing::Curl c1; c1.grip = 0.2f; c1.index = 0.0f; c1.thumb = 0.0f; fing::Tune t1; fp::holdBlend(0, c1, t1);
            fing::Curl c2; c2.grip = 0.2f; c2.index = 0.0f; c2.thumb = 0.0f; fing::Tune t2; fp::holdBlend(1, c2, t2);
            fp::spub[0].holdW[0] = 0.0f; fing::Curl c3; c3.grip = 0.2f; fing::Tune t3; fp::holdBlend(0, c3, t3);
            bool hb = fabsf(c1.grip - 1.0f) < 1e-4f && fabsf(c1.index - 1.0f) < 1e-4f && fabsf(t1.longDeg[0] - 60.0f) < 1e-3f && fabsf(t1.thumbDeg[1] - 10.0f) < 1e-3f
                && fabsf(c2.index - 0.5f) < 1e-4f && fabsf(t2.longDeg[1] - 92.5f) < 1e-3f && fabsf(c3.grip - 0.2f) < 1e-6f && fabsf(t3.longDeg[0] - 80.0f) < 1e-6f;
            fp::spub[0] = sv0; fp::spubIdx = sk;
            const u64 q0 = vrin::extReqSeq.load(); vrin::requestChord(vrin::XB_RIGHT | vrin::OX_RSR); hb &= vrin::extReqSeq.load() == q0 + 1 && vrin::extReqMask.load() == (u32)(vrin::XB_RIGHT | vrin::OX_RSR); vrin::extSeen = vrin::extReqSeq.load();   // not played into later tests
            ok &= hb; logf("katana hold fingers test (sheath table full on the left, half-blended hilt on the right, none when not holding; the draw chord queued): %s", hb ? "PASS" : "FAIL");
        }
        {   // 0.18.63 forced guard presses: a short tap never shortens a held guard; a parry press after a held guard is released first
            const u64 n0 = GetTickCount64(); vrin::forcePress(vrin::XB_LB, 600); const u64 u0 = vrin::forceUntil.load(); vrin::forcePress(vrin::XB_LB, 60);
            bool fpk = vrin::forceUntil.load() == u0 && vrin::forceActive(vrin::XB_LB, n0 + 1);
            const u32 gap = vrin::forcePressFresh(vrin::XB_LB, 160, 50); fpk &= gap == 50 && !vrin::forceActive(vrin::XB_LB, n0 + 10) && vrin::forceActive(vrin::XB_LB, vrin::forceFrom.load() + 1);
            vrin::forceUntil = 0; vrin::forceMask = 0; fpk &= vrin::forcePressFresh(vrin::XB_LB, 160, 50) == 0; vrin::forceUntil = 0; vrin::forceMask = 0;
            ok &= fpk; logf("forced guard press test (tap inside a held guard, a fresh parry press): %s", fpk ? "PASS" : "FAIL"); }
        {   // 0.18.24 room-scale: shifting the reference takes exactly Jin's step out of the head offset (any reference yaw, any view yaw)
            const GvPose sref = g_ref; const bool sv = g_refValid; bool rs = true;
            for (int t = 0; t < 3; t++) {
                const float ry = 0.7f * t - 0.5f, vh = 25.0f * t - 10.0f, cy = 1.1f * t + 0.3f; const float gm[16] = {cosf(cy), sinf(cy), 0, 0, -sinf(cy), cosf(cy), 0, 0, 0, 0, 1, 0, 100, 200, 160, 1};
                g_ref.q[0] = 0; g_ref.q[1] = sinf(ry / 2); g_ref.q[2] = 0; g_ref.q[3] = cosf(ry / 2); g_ref.p[0] = 0.1f; g_ref.p[1] = 1.6f; g_ref.p[2] = -0.2f; g_refValid = true;
                GvPose hd{}; hd.q[3] = 1; hd.p[0] = 0.4f; hd.p[1] = 1.7f; hd.p[2] = -0.5f;
                auto world = [&](float *w) { float Rg[3][3], dg[3]; poseLocal(hd, Rg, dg); float fx = gm[0], fy = gm[1]; const float h = vh / 57.2957795f, c = cosf(h), sn = sinf(h);
                    const float r[3][3] = {{fx * c - fy * sn, fx * sn + fy * c, 0}, {-(fx * sn + fy * c), fx * c - fy * sn, 0}, {0, 0, 1}};
                    for (int k = 0; k < 3; k++) w[k] = 0; for (int k = 0; k < 3; k++) for (int c2 = 0; c2 < 3; c2++) w[c2] += dg[k] * r[k][c2] * g_cfg.unitsPerM; };
                float w1[3], w2[3]; world(w1); const float dw[2] = {12.0f, -7.0f}; rs &= rsShiftRef(gm, vh, dw); world(w2);
                rs &= fabsf(w2[0] - (w1[0] - 12.0f)) < 0.01f && fabsf(w2[1] - (w1[1] + 7.0f)) < 0.01f && fabsf(w2[2] - w1[2]) < 0.01f;
            }
            g_ref = sref; g_refValid = sv;
            ok &= rs; logf("room-scale reference test (Jin's step comes off the head offset exactly, for any reference / view yaw): %s", rs ? "PASS" : "FAIL");
        }
        {   // 0.18.25 3D audio: during the game's listener update CAM-B's matrix is your head; right after it is the game's again; off = untouched
            static float camb[0x900 / 4]; static float seen[16]; memset(camb, 0, sizeof camb); float *cm = camb + 0x890 / 4; for (int k = 0; k < 16; k++) cm[k] = (float)(100 + k);
            const auto sc = aud::camB; const auto so = aud::oListener; const auto su = aud::useHead; const int son = aud::on.load();
            aud::camB = []() -> u8 * { return (u8 *)camb; }; aud::oListener = [](u8 *) { memcpy(seen, camb + 0x890 / 4, 64); }; aud::useHead = []() -> bool { return true; }; aud::on = 1;
            float H[16] = {0, 1, 0, 7, -1, 0, 0, 7, 0, 0, 1, 7, 500, 600, 170, 7}; aud::publish(H);
            aud::hListener(nullptr); bool au = seen[0] == 0.0f && seen[1] == 1.0f && seen[12] == 500.0f && seen[14] == 170.0f && seen[3] == 0.0f && seen[15] == 1.0f && cm[0] == 100.0f && cm[12] == 112.0f;
            aud::on = 0; aud::hListener(nullptr); au &= seen[12] == 112.0f && cm[12] == 112.0f;   // off: the game's camera
            aud::on = 1; aud::useHead = []() -> bool { return false; }; aud::hListener(nullptr); au &= seen[12] == 112.0f;   // not first person: the game's camera
            aud::camB = sc; aud::oListener = so; aud::useHead = su; aud::on = son; aud::nHead = 0; aud::nGame = 0;
            ok &= au; logf("3D audio test (the listener is the head during the game's listener update, the game's camera restored after; off / third person untouched): %s", au ? "PASS" : "FAIL");
        }
        {   // 0.18.26 Jin's own sounds: an emitter 10 cm from the head is re-sent 25 cm in front of the head (both ears); one 1 m away is left
            static u8 code[8] = {0xB8, 40, 0, 0, 0, 0xC3, 0, 0}; static void *vt[8]; vt[4] = code; static u8 own[0x300]; memset(own, 0, sizeof own); *(void ***)own = vt;
            float *OM = (float *)(own + 0x150); OM[0] = OM[5] = OM[10] = OM[15] = 1.0f; OM[12] = 1000; OM[13] = 2000; OM[14] = 160;
            static u8 src[0x80]; memset(src, 0, sizeof src); *(u8 **)(src + 0x38) = own; *(int *)(src + 0x48) = -1; float *Lo = (float *)(src + 0x50); Lo[0] = 10; Lo[3] = 1;
            static u8 em[0x40]; memset(em, 0, sizeof em); *(u64 *)(em + 0x18) = 0x1234; *(u8 **)(em + 0x28) = src;
            static float I4[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}; static float got[9]; static u64 gid; static int nset;
            const auto so = aud::oEmitter; const auto ssp = aud::setPos; const auto su = aud::selfUse; const auto sw = aud::w2a; const int son = aud::on.load(), sso = aud::selfOn.load(); nset = 0;
            aud::oEmitter = [](u8 *) {}; aud::setPos = [](u64 id, const float *t) -> int { gid = id; memcpy(got, t, 36); nset++; return 1; }; aud::selfUse = []() -> bool { return true; }; aud::w2a = I4; aud::on = 1; aud::selfOn = 1;
            const float sup = g_cfg.unitsPerM; g_cfg.unitsPerM = 100.0f;
            float H[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1000, 2000, 160, 1}; aud::publish(H);
            u8 *const snd = fp::node; const int snj2 = fp::nJ; fp::node = own; fp::nJ = 10;
            aud::hEmitter(em); bool vs = nset == 1 && gid == 0x1234 && fabsf(got[6] - 1025.0f) < 0.01f && fabsf(got[7] - 2000.0f) < 0.01f && got[0] == 1.0f && got[5] == 1.0f;
            Lo[0] = 100; aud::hEmitter(em); vs &= nset == 1;                    // 1 m away: where the game put it
            Lo[0] = 10; aud::selfOn = 0; aud::hEmitter(em); vs &= nset == 1;    // off
            { static u8 other[0x300]; memcpy(other, own, sizeof other); *(u8 **)(src + 0x38) = other; aud::hEmitter(em); vs &= nset == 1; *(u8 **)(src + 0x38) = own; }   // someone else's emitter: untouched
            fp::node = snd; fp::nJ = snj2;
            aud::oEmitter = so; aud::setPos = ssp; aud::selfUse = su; aud::w2a = sw; aud::on = son; aud::selfOn = sso; aud::nSelf = 0; aud::nEmit = 0; g_cfg.unitsPerM = sup;
            ok &= vs; logf("3D audio own-sound test (an emitter at the head is heard centred in front, one a metre away keeps its place, off = untouched): %s", vs ? "PASS" : "FAIL");
        }
        {   // 0.18.25 frozen frames: the game's own pose and world matrices come back (head shown) when the mod had changed them
            static float P[40 * 16], Wd[41 * 16]; static u8 ndb[0x600]; memset(ndb, 0, sizeof ndb); *(float **)(ndb + 0x278) = Wd;
            for (int k = 0; k < 40 * 16; k++) P[k] = (float)k; for (int k = 0; k < 41 * 16; k++) Wd[k] = (float)(10000 + k);
            const int snj = fp::nJ; fp::nJ = 40; const u32 shf = fp::hideFrame.load(); g_stFrameOn = true; g_stFrame = 500; fp::hideFrame = 500; for (auto &q : fp::snap) { q.pose = nullptr; q.dirty = false; q.seq = 0; q.nd = nullptr; q.W = nullptr; q.cnt = 0; }
            fp::snapTake(ndb, P); P[5 * 16 + 12] = 0.0f; Wd[6 * 16 + 12] = -1.0f;   // the mod hides a joint
            fp::snapRestore(); bool sr = P[5 * 16 + 12] == (float)(5 * 16 + 12) && Wd[6 * 16 + 12] == (float)(10000 + 6 * 16 + 12);
            P[5 * 16 + 12] = 0.0f; fp::snapRestore(); sr &= P[5 * 16 + 12] == 0.0f;   // once only
            g_stFrame = 510; fp::snapTake(ndb, P); P[5 * 16 + 12] = 3.0f; fp::snapRestore(); sr &= P[5 * 16 + 12] == 3.0f;   // a fresh pose the mod leaves alone: nothing to undo
            fp::nJ = snj; fp::hideFrame = shf; g_stFrameOn = false; for (auto &q : fp::snap) { q.pose = nullptr; q.dirty = false; q.seq = 0; q.nd = nullptr; q.W = nullptr; q.cnt = 0; } fp::nSnapRestores = 0;
            ok &= sr; logf("frozen-frame restore test (the game's own pose / world matrices put back once, nothing done for a pose the mod left alone): %s", sr ? "PASS" : "FAIL");
        }
        {   // 0.18.11 F1/F2/F3 hand trims: 5-degree steps, wrap at +-180, saved to gotvr.ini and read back as the default
            const float sp0 = g_fp.handPitch, sy0 = g_fp.handYaw, sr0 = g_fp.handRoll; g_fp.handPitch = 0; g_fp.handYaw = 175.0f; g_fp.handRoll = 0;
            handTrim(0, 5.0f); handTrim(0, 5.0f); handTrim(1, 5.0f); handTrim(2, -5.0f);
            wchar_t ini[MAX_PATH]; swprintf(ini, MAX_PATH, L"%ls\\gotvr.ini", g_dir); wchar_t b[32];
            GetPrivateProfileStringW(L"gotvr", L"fp_hand_pitch_deg", L"x", b, 32, ini); const float rp = (float)_wtof(b);
            GetPrivateProfileStringW(L"gotvr", L"fp_hand_yaw_deg", L"x", b, 32, ini); const float ry = (float)_wtof(b);
            GetPrivateProfileStringW(L"gotvr", L"fp_hand_roll_deg", L"x", b, 32, ini); const float rr = (float)_wtof(b);
            const bool ht2 = g_fp.handPitch == 10.0f && fabsf(g_fp.handYaw - 180.0f) < 1e-3f && g_fp.handRoll == -5.0f && rp == 10.0f && fabsf(ry - 180.0f) < 1e-3f && rr == -5.0f;
            ok &= ht2; logf("hand trim hotkey test (F1/F2/F3 steps, wrap, saved to and read back from gotvr.ini): %s (pitch %.1f yaw %.1f roll %.1f; ini %.1f %.1f %.1f)", ht2 ? "PASS" : "FAIL", g_fp.handPitch, g_fp.handYaw, g_fp.handRoll, rp, ry, rr);
            g_fp.handPitch = sp0; g_fp.handYaw = sy0; g_fp.handRoll = sr0;
            WritePrivateProfileStringW(L"gotvr", L"fp_hand_pitch_deg", nullptr, ini); WritePrivateProfileStringW(L"gotvr", L"fp_hand_yaw_deg", nullptr, ini); WritePrivateProfileStringW(L"gotvr", L"fp_hand_roll_deg", nullptr, ini);
        }
        logf("full-body IK arm test (chains found by shape, two-bone reach, lengths kept, elbow down/out, rigid fingers, grip frame, clavicle reach for far targets; legs: stand tall with feet planted): %s", ok ? "PASS" : "FAIL");
        rsOk &= ok;
    }
    {   // 0.18.0 VR controllers: canonical controls -> virtual pad (deadzone, mapping, snap turn closed loop, wheel, head gesture,
        // staleness -> neutral but connected, flat context, rumble)
        static GvShared fake; GvShared *const sS = S; S = &fake; memset(&fake, 0, sizeof fake);
        static u64 tNow; tNow = 100000; const auto sNow = vrin::nowMs; vrin::nowMs = []() -> u64 { return tNow; };
        static int hudCalls = 0; vrin::cfg = vrin::Cfg{};
        { const char *def[vrin::C_N]; for (int c = 0; c < vrin::C_N; c++) def[c] = vrin::kCtl[c].def; for (int c = 0; c < vrin::C_N; c++) vrin::cfg.out[c] = vrin::parseOut(def[c]); }
        vrin::cfg.wheel[0] = vrin::parseOut("down"); vrin::cfg.wheel[1] = vrin::parseOut("right"); vrin::cfg.wheel[2] = vrin::parseOut("left"); strcpy(vrin::cfg.wheelName[0], "Heal"); vrin::cfg.nWheel = 3;
        const bool shh = ui::hudHidden.load();
        auto push = [&](auto f) { gvWriteBegin(&fake.inSeq); f(); fake.inActive = 1; fake.inSamples++; gvWriteEnd(&fake.inSeq); };
        fake.poseValid = 1; fake.head.q[3] = 1; fake.head.p[1] = 1.6f;
        XINPUT_GAMEPAD g; bool ok = !vrin::build(g, false);   // nothing from the host yet: no pad
        push([&] { fake.hand[0].stick[0] = 0.1f; fake.hand[0].stick[1] = 1.0f; fake.hand[1].buttons = GV_BTN_PRIMARY; for (auto &h : fake.hand) { h.poseValid = 1; h.grip.q[3] = 1; h.grip.p[1] = 1.0f; } });
        ok &= vrin::build(g, false) && (g.wButtons & vrin::XB_A) && g.sThumbLY > 32000 && abs(g.sThumbLX) < 3500;   // jump, move (radial deadzone keeps the angle)
        vrin::lastYawOk = false; vrin::gameTick(0.0f); tNow += 10;
        push([&] { fake.hand[1].buttons = 0; fake.hand[1].stick[0] = 1.0f; });
        ok &= vrin::build(g, false) && fabsf(vrin::viewHold.load() + 30.0f) < 0.01f && g.sThumbRX > 20000;   // snap: the view turns 30 deg at once, the game camera is sent after it
        push([&] { fake.hand[0].stick[0] = 0; fake.hand[0].stick[1] = 1.0f; });
        vrin::gameTick(-12.0f); tNow += 10; vrin::build(g, false); ok &= fabsf(vrin::viewHold.load() + 18.0f) < 0.01f && g.sThumbRX > 0 && g.sThumbLX > 9000;   // view stays put; forward = where you look
        vrin::gameTick(-29.8f); tNow += 10; vrin::build(g, false); ok &= fabsf(vrin::viewHold.load() + 0.2f) < 0.01f && g.sThumbRX == 0;   // caught up
        tNow += 10; vrin::build(g, false); ok &= g.sThumbRX == 0 && fabsf(vrin::viewHold.load() + 0.2f) < 0.01f;   // holding the stick does not repeat
        push([&] { fake.hand[0].stick[1] = 0; fake.hand[1].stick[0] = 0; }); tNow += 300; vrin::gameTick(-40.0f); vrin::build(g, false); ok &= fabsf(vrin::viewHold.load() + 0.2f) < 0.01f;   // the game's own turns pass through
        push([&] { fake.hand[1].stick[0] = 0; fake.hand[0].buttons |= GV_BTN_PRIMARY; });   // wheel opens (X held, 0.18.24); right stick up = option 1
        tNow += 10; vrin::build(g, false); ok &= !vrin::wheel.open;   // 0.18.61: not yet - a quick press heals
        tNow += 200; push([&] {}); vrin::build(g, false); tNow += 200; push([&] {}); vrin::build(g, false); ok &= vrin::wheel.open && g.bLeftTrigger == 0;   // held: the wheel
        push([&] { fake.hand[1].stick[1] = 1.0f; }); tNow += 10; vrin::build(g, false); ok &= vrin::wheel.sel == 0 && g.sThumbRY == 0 && g.sThumbRX == 0;
        push([&] { fake.hand[1].stick[1] = 0.0f; }); tNow += 10; vrin::build(g, false); ok &= (g.wButtons & vrin::XB_DOWN) && !vrin::wheel.open;   // released onto Heal
        tNow += 200; vrin::build(g, false); ok &= !(g.wButtons & vrin::XB_DOWN) && !vrin::wheel.open;   // one tap; closed until the trigger is let go
        {   // 0.18.7: five options (90 - i*72 deg); Play flute = D-pad right, then the right stick flicked left; Recenter view = F11
            vrin::cfg.nWheel = 5; vrin::cfg.wheel[3] = vrin::parseOut("dpad_right+rs_left"); vrin::cfg.wheel[4] = vrin::parseOut("recenter"); m_recenter = 0;
            bool w5 = true;
            push([&] { fake.hand[0].buttons &= ~GV_BTN_PRIMARY; }); tNow += 10; vrin::build(g, false);
            push([&] { fake.hand[0].buttons |= GV_BTN_PRIMARY; }); tNow += 10; vrin::build(g, false); tNow += 200; push([&] {}); vrin::build(g, false); tNow += 200; push([&] {}); vrin::build(g, false); w5 &= vrin::wheel.open; if (!(vrin::wheel.open)) logf("   w5 step %d FAIL", 1);
            push([&] { fake.hand[1].stick[0] = -0.588f; fake.hand[1].stick[1] = -0.809f; }); tNow += 10; vrin::build(g, false); w5 &= vrin::wheel.sel == 3; if (!(vrin::wheel.sel == 3)) logf("   w5 step %d FAIL", 2);
            push([&] { fake.hand[1].stick[0] = 0; fake.hand[1].stick[1] = 0; }); tNow += 10; vrin::build(g, false); w5 &= (g.wButtons & vrin::XB_RIGHT) && g.sThumbRX == 0; if (!((g.wButtons & vrin::XB_RIGHT) && g.sThumbRX == 0)) logf("   w5 step %d FAIL", 3);   // flute: D-pad first
            push([&] {}); tNow += 150; vrin::build(g, false); w5 &= (g.wButtons & vrin::XB_RIGHT) && g.sThumbRX < -30000; if (!((g.wButtons & vrin::XB_RIGHT) && g.sThumbRX < -30000)) logf("   w5 step %d FAIL", 4);   // ...then the flick left
            push([&] {}); tNow += 320; vrin::build(g, false); w5 &= !(g.wButtons & vrin::XB_RIGHT) && g.sThumbRX == 0; if (!(!(g.wButtons & vrin::XB_RIGHT) && g.sThumbRX == 0)) logf("   w5 step %d FAIL", 5);
            push([&] { fake.hand[0].buttons &= ~GV_BTN_PRIMARY; }); tNow += 10; vrin::build(g, false);
            push([&] { fake.hand[0].buttons |= GV_BTN_PRIMARY; }); tNow += 10; vrin::build(g, false); tNow += 200; push([&] {}); vrin::build(g, false); tNow += 200; push([&] {}); vrin::build(g, false);
            push([&] { fake.hand[1].stick[0] = -0.951f; fake.hand[1].stick[1] = 0.309f; }); tNow += 10; vrin::build(g, false); w5 &= vrin::wheel.sel == 4; if (!(vrin::wheel.sel == 4)) logf("   w5 step %d FAIL", 6);
            push([&] { fake.hand[1].stick[0] = 0; fake.hand[1].stick[1] = 0; }); tNow += 10; vrin::build(g, false); w5 &= m_recenter.load() == 1 && g.wButtons == 0; if (!(m_recenter.load() == 1 && g.wButtons == 0)) logf("   w5 step %d FAIL", 7);   // recenter, no pad output
            // 0.18.61 a quick press (under wheel_hold_ms) = heal (D-pad down), no wheel
            vrin::cfg.wheelTap = vrin::parseOut("down"); vrin::cfg.wheelHoldMs = 300; const bool w5a = w5;
            push([&] { fake.hand[0].buttons &= ~GV_BTN_PRIMARY; }); tNow += 10; vrin::build(g, false); tNow += 500; vrin::build(g, false);
            push([&] { fake.hand[0].buttons |= GV_BTN_PRIMARY; }); tNow += 10; vrin::build(g, false); w5 &= !vrin::wheel.open;
            push([&] { fake.hand[0].buttons &= ~GV_BTN_PRIMARY; }); tNow += 80; vrin::build(g, false); w5 &= (g.wButtons & vrin::XB_DOWN) && !vrin::wheel.open;
            tNow += 300; vrin::build(g, false); w5 &= !(g.wButtons & vrin::XB_DOWN);
            if (w5a && !w5) logf("   quick-press heal step: FAIL"); else if (!w5a) logf("   wheel pick steps: FAIL");
            m_recenter = 0; vrin::cfg.nWheel = 3; ok &= w5; if (!w5) logf("   five-option wheel (flute, recenter) / quick-press heal: FAIL");
        }
        push([&] { fake.hand[0].trigger = 0; fake.hand[0].buttons &= ~GV_BTN_PRIMARY; fake.hand[1].grip.p[1] = 1.6f; fake.hand[1].grip.p[0] = 0.1f; fake.hand[1].trigger = 1.0f; });   // right hand at the head + trigger
        tNow += 10; vrin::build(g, false); ok &= (g.wButtons & vrin::XB_RIGHT) && g.sThumbRY == 0 && g.bRightTrigger == 0;   // guiding wind: D-pad right first, no shot
        push([&] {}); tNow += 150; vrin::build(g, false); ok &= (g.wButtons & vrin::XB_RIGHT) && g.sThumbRY > 30000 && g.bRightTrigger == 0;   // ...then the flick up while it is held
        push([&] {}); tNow += 140; vrin::build(g, false); push([&] {}); tNow += 140; vrin::build(g, false); ok &= (g.wButtons & vrin::XB_RIGHT) && g.sThumbRY == 0;   // the flick ends before the D-pad
        tNow += 500; vrin::build(g, false); ok &= !(g.wButtons & vrin::XB_RIGHT) && g.bRightTrigger == 0;   // one gesture only
        push([&] { fake.hand[1].trigger = 0; fake.hand[1].grip.p[1] = 1.0f; fake.hand[0].grip.p[1] = 1.6f; fake.hand[0].trigger = 1.0f; });
        ui::hudBoostUntil = 0; tNow += 10; vrin::build(g, false); ok &= ui::hudBoostUntil.load() > GetTickCount64() && ui::placeReq.exchange(0) == 1;   // left hand at the head: the room-fixed HUD panel placed in front
        push([&] { fake.hand[0].trigger = 0; fake.hand[0].grip.p[1] = 1.0f; fake.hand[1].trigger = 0.5f; fake.hand[1].stick[0] = 0.8f; });
        tNow += 10; vrin::build(g, true); ok &= g.bRightTrigger > 120 && g.sThumbRX > 20000;   // menus: plain pad (stick raw, trigger analog)
        tNow += 400; ok &= vrin::build(g, false) && g.wButtons == 0 && g.sThumbRX == 0 && g.bRightTrigger == 0;   // no new sample for 400 ms: neutral, still connected
        XINPUT_VIBRATION vb{65535, 0}; vrin::rumble(&vb); ok &= fake.hapLevel[0] > 0.99f && fake.hapLevel[1] == 0.0f;
        {   // 0.18.17: the view's world yaw survives a menu; the left stick's forward is the captured movement yaw
            push([&] { for (auto &h : fake.hand) { h.stick[0] = 0; h.stick[1] = 0; h.trigger = 0; h.squeeze = 0; h.buttons = 0; } });
            vrin::lastYawOk = false; vrin::gameTick(10.0f); vrin::viewHold = 5.0f; vrin::wasFlat = false; tNow += 10; vrin::build(g, true);   // into a menu: view at 15 deg
            vrin::gameTick(40.0f); tNow += 10; vrin::build(g, true); tNow += 10; vrin::build(g, false);                                          // the game camera turned to 40 in the menu
            bool mf = fabsf(vrin::viewHold.load() + 25.0f) < 0.01f;                                                                           // out: the view faces 15 again
            vrin::moveYaw = 90.0f; vrin::moveYawOk = 1; vrin::headYawOk = 0; vrin::gameTick(0.0f); vrin::viewHold = 0.0f;
            push([&] { fake.hand[0].stick[1] = 1.0f; }); tNow += 10; vrin::build(g, false); mf &= g.sThumbLX < -30000 && abs(g.sThumbLY) < 2000;   // forward = 90 deg left of the game camera
            vrin::moveYawOk = 0; push([&] { fake.hand[0].stick[1] = 0.0f; }); vrin::build(g, false);
            // 0.18.20: a move from rest takes the headset's forward as the movement forward
            vrin::headYawNow = 60.0f; vrin::headYawOk = 1; tNow += 400; push([&] { fake.hand[0].stick[1] = 0.0f; }); vrin::build(g, false);
            push([&] { fake.hand[0].stick[1] = 1.0f; }); tNow += 10; vrin::build(g, false); mf &= vrin::moveYawOk.load() && fabsf(vrin::moveYaw.load() - 60.0f) < 0.01f;
            push([&] { fake.hand[0].stick[1] = 0.0f; }); tNow += 10; vrin::build(g, false); vrin::moveYawOk = 0; vrin::headYawOk = 0;
            {   // 0.18.22 move_follows_head: the stick's forward follows the headset WHILE moving (no capture needed)
                const int smh = vrin::cfg.moveHead; vrin::cfg.moveHead = 1; vrin::gameTick(0.0f); vrin::headYawNow = 90.0f; vrin::headYawOk = 1; vrin::moveYaw = 0.0f; vrin::moveYawOk = 1;
                push([&] { fake.hand[0].stick[1] = 1.0f; }); tNow += 10; vrin::build(g, false); const bool h1 = g.sThumbLX < -30000 && abs(g.sThumbLY) < 2000;   // 90 deg left
                vrin::headYawNow = -90.0f; tNow += 10; vrin::build(g, false); const bool h2 = g.sThumbLX > 30000 && abs(g.sThumbLY) < 2000;   // you look right, still moving: right
                vrin::cfg.moveHead = 0; tNow += 10; vrin::build(g, false); const bool h3 = abs(g.sThumbLX) < 2000 && g.sThumbLY > 30000;   // captured mode: the old forward (0)
                push([&] { fake.hand[0].stick[1] = 0.0f; }); tNow += 10; vrin::build(g, false); vrin::moveYawOk = 0; vrin::headYawOk = 0; vrin::cfg.moveHead = smh;
                mf &= h1 && h2 && h3; if (!(h1 && h2 && h3)) logf("   movement follows the headset: FAIL (%d %d %d)", h1, h2, h3);
            }
            {   // 0.18.24 room-scale drive: your head 30 cm to the world's +y (game camera along +x) -> the stick walks left; within 6 cm it stops;
                // your own stick wins; a blocked drive pauses
                const int so = vrin::rsOn; vrin::rsOn = 1; vrin::rsDeadCm = 15.0f; vrin::rsStopCm = 6.0f; vrin::gameTick(0.0f); vrin::headYawOk = 0; vrin::moveYawOk = 0; vrin::viewHold = 0.0f;
                vrin::rsOff[0] = 0.0f; vrin::rsOff[1] = 30.0f; vrin::rsOkUntil = tNow + 100000; vrin::rsBlockUntil = 0;
                push([&] { fake.hand[0].stick[0] = 0; fake.hand[0].stick[1] = 0; }); tNow += 10; vrin::build(g, false); bool r1 = g.sThumbLX < -9000 && abs(g.sThumbLY) < 1500;
                vrin::rsOff[1] = 10.0f; tNow += 10; vrin::build(g, false); r1 &= g.sThumbLX < -9000;                       // still walking (hysteresis)
                vrin::rsOff[1] = 4.0f; tNow += 10; vrin::build(g, false); r1 &= g.sThumbLX == 0;                            // arrived
                vrin::rsOff[1] = 10.0f; tNow += 10; vrin::build(g, false); r1 &= g.sThumbLX == 0;                           // below the start distance: stays
                vrin::rsOff[1] = 30.0f; push([&] { fake.hand[0].stick[1] = 1.0f; }); tNow += 10; vrin::build(g, false); r1 &= g.sThumbLY > 20000 && abs(g.sThumbLX) < 2000;   // your stick wins
                push([&] { fake.hand[0].stick[1] = 0.0f; }); vrin::rsBlockUntil = tNow + 1000; tNow += 10; vrin::build(g, false); r1 &= g.sThumbLX == 0;   // blocked: pauses
                vrin::rsOkUntil = 0; vrin::rsBlockUntil = 0; tNow += 10; vrin::build(g, false); r1 &= g.sThumbLX == 0;      // not safe (riding / climbing): nothing
                vrin::rsOff[0] = vrin::rsOff[1] = 0.0f; vrin::rsOn = so;
                mf &= r1; if (!r1) logf("   room-scale drive: FAIL (LX %d LY %d)", g.sThumbLX, g.sThumbLY);
            }
            ok &= mf; if (!mf) logf("   menu direction / movement forward: FAIL (hold %.2f, LX %d LY %d)", vrin::viewHold.load(), g.sThumbLX, g.sThumbLY);
        }
        ui::hudHidden = shh; ui::hudBoostUntil = 0; (void)hudCalls; vrin::nowMs = sNow; S = sS; vrin::everActive = false; vrin::snap = vrin::Snap{}; vrin::wheel = vrin::Wheel{}; vrin::viewHold = 0; vrin::gameYawOk = 0; vrin::catching = false; vrin::bleedUntil = 0; vrin::lastYawOk = false; vrin::lastPoll = 0;
        logf("VR controller test (deadzone, mapping, snap turn at once with the game camera catching up behind it, stick follows the view, item wheel pick/tap/close, guiding-wind and HUD head gestures, menus, staleness, rumble): %s", ok ? "PASS" : "FAIL");
        rsOk &= ok;
    }
    {   // BUG-70: render sync - the right tick waits for the left frame's Present (event from the render thread), with a timeout
        if (!g_presEv) g_presEv = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        g_presFrame = 41; g_syncNeeded = 0; g_syncAlready = 0; g_syncTimeouts = 0;
        HANDLE th = CreateThread(nullptr, 0, [](void *) -> DWORD { Sleep(8); g_presFrame = 42; SetEvent(g_presEv); return 0; }, nullptr, 0, nullptr);
        waitPresented(42); WaitForSingleObject(th, 1000); CloseHandle(th);
        bool ok = g_syncNeeded.load() == 1 && g_syncTimeouts.load() == 0 && g_presFrame.load() == 42;
        waitPresented(42); ok &= g_syncAlready.load() == 1;                      // already presented: no wait
        waitPresented(50); ok &= g_syncTimeouts.load() == 1;                    // never presented: gives up after 40 ms
        waitPresented(50); waitPresented(50); ok &= g_syncTimeouts.load() == 3 && g_syncPause == 300;   // 3 in a row: paused
        g_syncPause = 0; g_syncMiss = 0; g_presFrame = 0; g_syncNeeded = 0; g_syncAlready = 0; g_syncTimeouts = 0; g_syncWaitUs = 0;
        logf("render sync test (waits for the previous frame's Present, no wait when presented, 40 ms timeout, pauses after 3 timeouts): %s", ok ? "PASS" : "FAIL");
        rsOk &= ok;
    }
    {   // 0.16.4 (BUG-64e): the game re-creating its swapchain -> the publisher lets go of it BEFORE the old one is destroyed
        static u8 fakeSc[64], fakeRes[64]; static bool clean = false; void *const svRaw = pub::nscRaw; IDXGISwapChain3 *const svSc = pub::sc; const bool svReady = pub::ready, svTried = pub::tried;
        pub::nscRaw = fakeSc; pub::sc = (IDXGISwapChain3 *)fakeSc; pub::ready = true; pub::tried = true; bbmap::count = 3; bbmap::nat[0] = (ID3D12Resource *)fakeRes; ui::probeNat = (ID3D12Resource *)fakeRes;
        pub::oRecreate = [](u8 *, u8 *) -> char { clean = !pub::sc && !pub::nscRaw && !pub::ready && !pub::tried && bbmap::count == 0 && !ui::probeNat.load(); return 1; };
        const char r = pub::hRecreate(fakeSc, fakeRes); const bool ok = r == 1 && clean && pub::recreates.load() == 1;
        pub::oRecreate = nullptr; pub::recreates = 0; pub::nscRaw = svRaw; pub::sc = svSc; pub::ready = svReady; pub::tried = svTried; bbmap::count = 0; bbmap::fastUntil = 0; bbmap::lastTry = 0;
        logf("swapchain re-creation test (publisher drops the swapchain and its buffers before the game destroys them): %s", ok ? "PASS" : "FAIL");
        rsOk &= ok;
    }
    {   // 0.16.2 (BUG-64b): ResizeBuffers drops the back-buffer map and the UI probe BEFORE the swapchain frees them
        static u8 fakeSc[64], fakeRes[64]; static bool seenClean = false; void *const svRaw = pub::nscRaw; pub::nscRaw = fakeSc;
        bbmap::count = 3; bbmap::have = 1; bbmap::nat[0] = (ID3D12Resource *)fakeRes; bbmap::obj[0] = fakeRes; ui::probeNat = (ID3D12Resource *)fakeRes; ui::probeRec = fakeRes;
        pub::oResize = [](IDXGISwapChain *, UINT, UINT, UINT, DXGI_FORMAT, UINT) -> HRESULT { seenClean = bbmap::count == 0 && !bbmap::have.load() && !bbmap::nat[0] && !bbmap::obj[0] && !ui::probeNat.load() && !ui::probeRec.load(); return S_OK; };
        const HRESULT hr = pub::hResize((IDXGISwapChain *)fakeSc, 3, 2304, 2304, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
        bbmap::count = 3; bool other = false; static bool otherCalled = false;
        pub::oResize = [](IDXGISwapChain *, UINT, UINT, UINT, DXGI_FORMAT, UINT) -> HRESULT { otherCalled = true; return S_OK; };
        pub::hResize((IDXGISwapChain *)fakeRes, 3, 100, 100, DXGI_FORMAT_R8G8B8A8_UNORM, 0); other = otherCalled && bbmap::count == 3;   // someone else's swapchain: untouched
        const bool ok = hr == S_OK && seenClean && other && bbmap::fastUntil > GetTickCount64();
        bbmap::count = 0; bbmap::fastUntil = 0; bbmap::lastTry = 0; pub::nscRaw = svRaw; pub::oResize = nullptr; pub::resizes = 0;
        logf("swapchain resize test (back-buffer map + UI probe dropped before ResizeBuffers, other swapchains untouched): %s", ok ? "PASS" : "FAIL");
        rsOk &= ok;
    }
    bool uiOk = false;
    {   // 0.12.0 HUD panel + theatre: panel geometry, convergence, and the real Ctx_SetViewport code driven by the RT_Bind hook
        const int sx = g_cfg.xr; g_cfg.xr = 4; g_ui.hudDist = 2.0f; g_ui.hudDown = 0.0f; g_ui.theater = 1; g_ui.loadTheater = 0; g_ui.menuTheater = 0;
        float p[4]; ui::View v{true, 0, -1, 1, 1, -1, 0};
        bool geo = ui::panel(v, 1000, 625, 90, p) && fabsf(p[0]) < 1e-3f && fabsf(p[2] - 1000) < 1e-3f && fabsf(p[1] - 117.1875f) < 1e-2f && fabsf(p[3] - 507.8125f) < 1e-2f;
        ui::View vl{true, 0, -1, 1, 1, -1, -0.032f}, vr{true, 1, -1, 1, 1, -1, 0.032f}; float pl[4], pr[4];
        geo &= ui::panel(vl, 1000, 625, 60, pl) && ui::panel(vr, 1000, 625, 60, pr) && fabsf((pl[0] + pl[2]) * 0.5f - 508.0f) < 0.05f && fabsf((pr[0] + pr[2]) * 0.5f - 492.0f) < 0.05f && fabsf(pl[1] - pr[1]) < 1e-3f;
        static u8 ctx[0x3000]; memset(ctx, 0, sizeof ctx);
        static float vpRec[6]; static int vpN = 0; static u32 scRec[4]; static int scN = 0;
        typedef void (*Vt)(void *, UINT, const void *);
        static void *vt[64]; vt[21] = (void *)(Vt)[](void *, UINT, const void *q) { memcpy(vpRec, q, sizeof vpRec); vpN++; };
        static u64 L[1], A[1]; L[0] = (u64)vt; A[0] = (u64)L; *(u64 *)ctx = (u64)A;
        ui::oRT = [](u8 *c, u8 *t, int *, int) { int *r = (int *)(c + 0x2948); r[0] = 0; r[1] = 0; r[2] = 2560; r[3] = 1600; u32 *q = (u32 *)(c + 0x328); q[0] = 0; q[1] = 0; q[2] = 2560; q[3] = 1600; *(u8 **)(c + 0x2940) = t; };
        ui::oSC = [](u8 *, u32 l, u32 t, u32 r, u32 b) { scRec[0] = l; scRec[1] = t; scRec[2] = r; scRec[3] = b; scN++; };
        ui::oMovie = [](void *, void *, void *, void *) {};
        ui::hud = 1; ui::hudDeg = 90; ui::theaterMode = 2; ui::cur = v; ui::pauseMask = 0; uiDecide();
        ui::hRT(ctx, g_base + RVA_UI_TARGET, nullptr, -1);
        bool bind = vpN == 1 && vpRec[0] == 0 && vpRec[1] == 300 && vpRec[2] == 2560 && vpRec[3] == 1000 && vpRec[4] == 0 && vpRec[5] == 1 &&
                    scN == 1 && scRec[0] == 0 && scRec[1] == 300 && scRec[2] == 2560 && scRec[3] == 1300;
        ui::hSC(ctx, 1280, 800, 2560, 1600); bind &= scN == 2 && scRec[0] == 1280 && scRec[1] == 800 && scRec[2] == 2560 && scRec[3] == 1300;
        {   // BUG-53: world-anchored UI batches get the whole frame, the screen-space UI after them the panel again
            static bool during = false; static void *wvt[16]; wvt[8] = (void *)(g_base + RVA_UI_WORLDDRAW); static void *wobj[1]; wobj[0] = (void *)wvt;
            static void *ovt[16]; ovt[8] = (void *)(g_base + 0x123456); static void *oobj[1]; oobj[0] = (void *)ovt;
            static u8 *rng[2]; rng[0] = ctx; rng[1] = nullptr;
            ui::oBatch = [](void **, u8 **) { during = vpRec[1] == 0 && vpRec[3] == 1600 && ui::liveFind((u8 *)rng[0]) < 0; };
            ui::hBatch(wobj, rng); bind &= during && vpRec[1] == 300 && vpRec[3] == 1000 && ui::liveFind(ctx) >= 0;
            during = false; ui::hBatch(oobj, rng); bind &= !during && vpRec[1] == 300;   // other renderers: untouched (stay on the panel)
            logf("world-anchored UI test: %s", bind ? "PASS" : "FAIL");
            vpN = 1; scN = 2;   // the later checks count from the state after the bind + one scissor
        }
        u8 other[0x100] = {}; ui::hRT(ctx, other, nullptr, -1); ui::hSC(ctx, 10, 10, 20, 20);
        bind &= vpN == 1 && scN == 3 && scRec[0] == 10 && scRec[3] == 20;                                    // other targets untouched
        ui::theaterMode = 1; uiDecide(); ui::hRT(ctx, g_base + RVA_UI_TARGET, nullptr, -1); bind &= vpN == 1;             // flat context: UI full screen
        ui::theaterMode = 0; ui::post = 100; ui::worldOff = false; uiDecide(); bool mv = !ui::flat() && !g_flatRender.load();
        ui::hMovie(nullptr, nullptr, nullptr, nullptr); uiDecide(); mv &= ui::flat() && ui::movieNow() && g_flatRender.load() && g_flatCut.load() == 2;
        ui::endPost(); mv &= (ui::flagsOf(frameCtr()) & (GV_SLOT_THEATER | GV_SLOT_FLAT)) == (GV_SLOT_THEATER | GV_SLOT_FLAT);
        ui::post = 108; uiDecide(); mv &= !ui::flat() && !g_flatRender.load(); ui::worldOff = true; uiDecide(); mv &= ui::flat(); ui::worldOff = false;
        {   // pause: time-scale reason 7 at 0 = a pausing menu is open -> flat context; other reasons ignored by default
            float *sc = (float *)(g_base + RVA_CLOCK + 0x84); DWORD op; VirtualProtect(sc, 28 * 4, PAGE_READWRITE, &op); float sv[28]; memcpy(sv, sc, sizeof sv);
            for (int i = 0; i < 28; i++) sc[i] = 1.0f;
            ui::pauseMask = 0x80; uiDecide(); mv &= !ui::flat();
            sc[9] = 0; uiDecide(); mv &= !ui::flat();
            sc[7] = 0; uiDecide(); mv &= ui::flat() && ui::pausedTick.load() && g_flatRender.load();
            g_ui.flatRender = 0; uiDecide(); mv &= ui::flat() && !g_flatRender.load(); g_ui.flatRender = 1;   // theater_flat_render=0: screen, stereo image
            sc[7] = 1; uiDecide(); mv &= !ui::flat();
            memcpy(sc, sv, sizeof sv);
        }
        ui::cur = ui::View{}; ui::theaterMode = 2; ui::pauseMask = 0; uiDecide(); ui::hRT(ctx, g_base + RVA_UI_TARGET, nullptr, -1); bind &= vpN == 1;   // no eye view (flat mode/menus before VR): untouched
        bool lay = false;
        {   // 0.15.0 UI layer by redirect (BUG-58): UI-pass binds of the UI target are armed (pass 59 is not), pass 60 clears this
            // frame's image, the engine's flush is followed by OMSetRenderTargets(our RTV, the engine's depth view), other targets
            // and clean flushes are left alone, pass 65 un-redirects (dirty -> the engine re-binds) and publishes; flags per mode
            static int omN = 0, clN = 0; static UINT omNum = 0; static SIZE_T omRt = 0, omDs = 0, clH = 0; static BOOL omSingle = FALSE;
            vt[46] = (void *)(void(STDMETHODCALLTYPE *)(void *, UINT, const D3D12_CPU_DESCRIPTOR_HANDLE *, BOOL, const D3D12_CPU_DESCRIPTOR_HANDLE *))
                [](void *, UINT n, const D3D12_CPU_DESCRIPTOR_HANDLE *r, BOOL sg, const D3D12_CPU_DESCRIPTOR_HANDLE *d) { omN++; omNum = n; omRt = r ? r->ptr : 0; omSingle = sg; omDs = d ? d->ptr : 0; };
            vt[48] = (void *)(void(STDMETHODCALLTYPE *)(void *, D3D12_CPU_DESCRIPTOR_HANDLE, const FLOAT *, UINT, const D3D12_RECT *))[](void *, D3D12_CPU_DESCRIPTOR_HANDLE h, const FLOAT *, UINT, const D3D12_RECT *) { clN++; clH = h.ptr; };
            static u8 rec[0x80], rec2[0x80], att[0x40], att2[0x40], dep[0x40]; *(u8 **)(att + 0x18) = rec; *(u8 **)(att2 + 0x18) = rec2;
            ui::probeRec = rec; ui::probeNat = (ID3D12Resource *)(void *)rec; ui::hostUiOk = 1; ui::layerWant = 1; ui::f1Off = 0;
            for (int i = 0; i < GV_UITEX; i++) { ui::sh[i] = (ID3D12Resource *)(void *)(att + i); ui::shRtv[i] = 0x7000 + (SIZE_T)i * 0x20; } ui::shReady = 1;
            ui::theaterMode = 2; ui::pauseMask = 0; uiDecide(); ui::mode = ui::M_HUD; ui::cur = v; ui::hud = 1;
            const u32 f = frameCtr(); ui::kOf[f & 31] = {f, 1};
            *(int *)(ctx + 0x2A50) = 60; *(u8 **)(ctx + 0x2C8) = att; *(u8 **)(ctx + 0x2D0) = att2; *(u8 **)(ctx + 0x308) = dep; *(u32 *)(ctx + 0x33C) = 5;
            const int vp0 = vpN; bool ok = ui::layerReady() && ui::layerOn();
            ui::oFlushRT = [](u8 *c) { c[0x340] &= ~1; };
            {   // BUG-58b: like the engine, RT_Bind binds AND flushes inside the call (through the hooked flush)
                static decltype(ui::oRT) keepRT; keepRT = ui::oRT;
                ui::oRT = [](u8 *c, u8 *t, int *r, int sl) { keepRT(c, t, r, sl); c[0x340] |= 1; ui::hFlushRT(c); };
                ui::hRT(ctx, g_base + RVA_UI_TARGET, nullptr, -1); ui::oRT = keepRT; }
            ok &= vpN == vp0 && ui::armFind(ctx) >= 0 && omN == 1 && omRt == 0x7020;                                // armed BEFORE the bind's own flush; full size
            omN = 0;
            ui::oUI60 = [](u8 *) -> u64 { return 0; }; ui::hUI60(ctx); ok &= clN == 1 && clH == 0x7020;             // this frame's image cleared
            ctx[0x340] |= 1; ui::hFlushRT(ctx);
            const SIZE_T wantDs = (SIZE_T)(5u * *(u32 *)(g_base + RVA_DSV_INC)) + (SIZE_T) * (u64 *)(g_base + RVA_DSV_BASE);
            ok &= omN == 1 && omNum == 1 && omRt == 0x7020 && omSingle && omDs == wantDs;                                   // our RTV + the engine's depth view
            ui::hFlushRT(ctx); ok &= omN == 1;                                                                          // clean flush: untouched
            *(u8 **)(ctx + 0x2C8) = att2; ctx[0x340] |= 1; ui::hFlushRT(ctx); ok &= omN == 1 && ui::rdOther.load() >= 1;    // another texture: native
            *(u8 **)(ctx + 0x2C8) = att;
            {   static u8 c2[0x3000]; memset(c2, 0, sizeof c2); *(u64 *)c2 = (u64)A; *(int *)(c2 + 0x2A50) = 59;      // pass 59 (the 3D picture) is never redirected
                ui::hRT(c2, g_base + RVA_UI_TARGET, nullptr, -1); ok &= ui::armFind(c2) < 0; }
            ctx[0x340] = 0; ui::oP65 = [](u8 *) -> u64 { return 0; };
            ui::cur.eye = 1; ui::pend[f & 31] = {0, -1}; ui::hP65(ctx); ok &= ui::pend[f & 31].k == -1 && ui::rightSkip.load() == 1; ui::cur.eye = 0;   // right-eye frame: not published (BUG-59)
            ui::hRT(ctx, g_base + RVA_UI_TARGET, nullptr, -1);
            ui::hP65(ctx); ok &= ui::armFind(ctx) < 0 && (ctx[0x340] & 1) && ui::pend[f & 31].frame == f && ui::pend[f & 31].k == 1;   // un-redirected, published
            ui::hRT(ctx, g_base + RVA_UI_TARGET, nullptr, -1); ui::hRT(ctx, att2, nullptr, -1); ok &= ui::armFind(ctx) < 0;  // another target disarms
            {   float *fa = (float *)(g_base + RVA_FADE + 0xC); DWORD op2; VirtualProtect(fa, 4, PAGE_READWRITE, &op2); const float fs = *fa; *fa = 0;
                g_ui.follow = 1; ui::endPost(); ok &= ui::uiFlagsOf(frameCtr()) == GV_UI_WORLD;
                g_ui.follow = 0; ui::endPost(); ok &= ui::uiFlagsOf(frameCtr()) == GV_UI_HEAD;
                ui::mode = ui::M_CINEMA; ui::endPost(); ok &= ui::uiFlagsOf(frameCtr()) == GV_UI_FULL;
                *fa = 0.9f; ui::endPost(); ok &= ui::uiFlagsOf(frameCtr()) == 0u; *fa = fs; ui::mode = ui::M_HUD; }
            ui::f1Off = 1; ui::hRT(ctx, g_base + RVA_UI_TARGET, nullptr, -1); ok &= vpN == vp0 + 1 && ui::armFind(ctx) < 0;   // F1: the 0.14 panel mapping
            ui::f1Off = 0;
            bool cn = true;
            {   // 0.15.2 band detectors on synthetic rows (13 rows edge to edge): HUD image - nothing / thick bars / squared-alpha bars /
                // a dark veil / a HUD element in the top row; 3D picture - thin symmetric bars / black sky only / fully black; hysteresis
                static u8 rows[13 * 1024]; const u32 pitch = 1024, w = 256;
                auto row = [&](int y, u8 r, u8 g, u8 bb, u8 a) { for (u32 x = 0; x < w; x++) { u8 *q = rows + y * pitch + x * 4; q[0] = r; q[1] = g; q[2] = bb; q[3] = a; } };
                auto all = [&](u8 r, u8 g, u8 bb, u8 a) { for (int y = 0; y < 13; y++) row(y, r, g, bb, a); };
                all(0, 0, 0, 0); ui::Bands q = ui::bandsOf(rows, pitch, w, true, false); cn &= !ui::uiHit(q);
                for (int y = 0; y < 4; y++) row(y, 0, 0, 0, 255); for (int y = 9; y < 13; y++) row(y, 0, 0, 0, 255);
                q = ui::bandsOf(rows, pitch, w, true, false); cn &= ui::uiHit(q) && q.top == 4 && q.bot == 4;
                all(0, 0, 0, 0); row(0, 0, 0, 0, 180); row(1, 0, 0, 0, 180); row(12, 0, 0, 0, 180); q = ui::bandsOf(rows, pitch, w, true, false); cn &= ui::uiHit(q) && q.top == 2 && q.bot == 1;
                all(0, 0, 0, 60); q = ui::bandsOf(rows, pitch, w, true, false); cn &= ui::uiHit(q) && q.veil == 13;
                all(0, 0, 0, 0); row(0, 0, 0, 0, 255); for (u32 x = 0; x < 60; x++) rows[x * 4 + 3] = 0; row(12, 0, 0, 0, 255); q = ui::bandsOf(rows, pitch, w, true, false); cn &= !ui::uiHit(q);
                all(90, 80, 70, 255); row(0, 0, 0, 0, 255); row(1, 0, 0, 0, 255); row(11, 0, 0, 0, 255); row(12, 0, 0, 0, 255); q = ui::bandsOf(rows, pitch, w, false, false); cn &= ui::sceneHit(q) && q.top == 2 && q.bot == 2;
                all(90, 80, 70, 255); for (int y = 0; y < 4; y++) row(y, 0, 0, 0, 255); q = ui::bandsOf(rows, pitch, w, false, false); cn &= !ui::sceneHit(q);
                all(0, 0, 0, 255); q = ui::bandsOf(rows, pitch, w, false, false); cn &= !ui::sceneHit(q);
                // hysteresis: 2 hits to enter, 24 misses to leave (the readback of frame f is read at frame f + 4)
                ui::rbPitch = pitch; ui::rbW = w; ui::rbMap[0] = rows; ui::cine = 0; ui::cinePos = 0; ui::cineNeg = 0;
                all(0, 0, 0, 0); row(0, 0, 0, 0, 255); row(12, 0, 0, 0, 255);
                ui::rbFrame[0] = 8; ui::cineRead(12); cn &= ui::cine.load() == 0; ui::rbFrame[0] = 16; ui::cineRead(20); cn &= ui::cine.load() == 1;
                all(0, 0, 0, 0); for (int i = 0; i < 23; i++) { ui::rbFrame[0] = 24 + 8 * i; ui::cineRead(28 + 8 * i); } cn &= ui::cine.load() == 1;
                ui::rbFrame[0] = 24 + 8 * 23; ui::cineRead(28 + 8 * 23); cn &= ui::cine.load() == 0;
                ui::rbPitch = 0; ui::rbW = 0; ui::rbMap[0] = nullptr; ui::cinePos = ui::cineNeg = 0;
                {   // 0.15.3 cutscene latch on the 3D picture: 3 strict hits enter; dark shot / cut to black (edges black, no picture)
                    // holds; 89 frames without bars keep it, the 90th leaves
                    pub::sbar::map[0] = rows; pub::sbar::pitch = pitch; pub::sbar::w = w; pub::sbar::ten = false; ui::sceneBars = 0; pub::sbar::pos = pub::sbar::neg = 0;
                    auto feed = [&]() { pub::sbar::pend[0] = true; pub::sbar::readK(0); };
                    all(90, 80, 70, 255); row(0, 0, 0, 0, 255); row(1, 0, 0, 0, 255); row(11, 0, 0, 0, 255); row(12, 0, 0, 0, 255);
                    feed(); feed(); cn &= ui::sceneBars.load() == 0; feed(); cn &= ui::sceneBars.load() == 1;
                    all(0, 0, 0, 255); for (int i = 0; i < 200; i++) feed(); cn &= ui::sceneBars.load() == 1;            // dark shot: held
                    all(14, 14, 14, 255); row(6, 90, 80, 70, 255); for (int i = 0; i < 200; i++) feed(); cn &= ui::sceneBars.load() == 1;   // grainy bars (14/255): held
                    all(90, 80, 70, 255); for (int i = 0; i < 179; i++) feed(); cn &= ui::sceneBars.load() == 1; feed(); cn &= ui::sceneBars.load() == 0;
                    pub::sbar::map[0] = nullptr; pub::sbar::pitch = 0; pub::sbar::w = 0; ui::sceneBars = 0;
                }
                logf("cinematic band detector test (HUD image: none/thick/squared/veil/partial; 3D picture: thin/sky/black; hysteresis): %s", cn ? "PASS" : "FAIL");
            }
            lay = ok && cn; logf("UI layer test (redirect at the target flush): arm/no-mapping/clear/rebind+depth/clean/other/pass59/pass65/flags/F1 -> %s (binds %d, clears %d)", ok ? "PASS" : "FAIL", omN, clN);
            ui::probeRec = nullptr; ui::probeNat = nullptr; ui::hostUiOk = 0; ui::shReady = 0; for (auto &x : ui::sh) x = nullptr; for (auto &x : ui::shRtv) x = 0;
            ui::layerWant = 1; ui::mode = ui::M_HUD; *(u8 **)(ctx + 0x2C8) = nullptr; *(u8 **)(ctx + 0x308) = nullptr; *(int *)(ctx + 0x2A50) = 0; g_ui.follow = 1;
        }
        {   // 0.14.3 world-locked start screen: a panel ahead appears to the RIGHT after turning the head 30 deg left, and moves with
            // the eye position (stereo), hidden when behind
            const float q0[4] = {0, 0, 0, 1}, q30[4] = {0, sinf(15.0f / 57.29578f), 0, cosf(15.0f / 57.29578f)}, P[3] = {0, 0, -2}; float e0[3], e1[3];
            ui::qrotInv(q0, P, e0); ui::qrotInv(q30, P, e1);
            const bool wlok = fabsf(e0[0]) < 1e-5f && fabsf(e0[2] + 2) < 1e-5f && fabsf(e1[0] / -e1[2] - tanf(30.0f / 57.29578f)) < 1e-4f && e1[2] < 0;
            const float qb[4] = {0, 1, 0, 0}; float eb[3]; ui::qrotInv(qb, P, eb);   // turned around: panel behind (+z)
            lay &= wlok && eb[2] > 1.9f; logf("world-locked panel geometry: %s (30 deg left -> centre tangent %.3f)", wlok && eb[2] > 1.9f ? "PASS" : "FAIL", e1[0] / -e1[2]);
        }
        {   // 0.14.4 loading screens -> flat: world mode 1 or time-scale reason 9 at 0
            float *sc = (float *)(g_base + RVA_CLOCK + 0x84); DWORD op; VirtualProtect(sc, 28 * 4, PAGE_READWRITE, &op); float sv[28]; memcpy(sv, sc, sizeof sv);
            for (int i = 0; i < 28; i++) sc[i] = 1.0f;
            g_ui.loadTheater = 1; ui::theaterMode = 0; ui::pauseMask = 0x80; ui::worldOff = false; ui::post = 500; ui::lastMovie = 0;
            ui::worldMode = 2; uiDecide(); bool ld = !ui::flat();
            ui::worldMode = 1; uiDecide(); ld &= ui::flat() && ui::loadingTick.load();
            ui::worldMode = 2; sc[9] = 0; uiDecide(); ld &= ui::flat(); sc[9] = 1; uiDecide(); ld &= !ui::flat();
            {   // black screen (game fade fully opaque in world mode 2 for 150 ms) -> flat; fading in -> stereo at once
                float *fa = (float *)(g_base + RVA_FADE + 0xC); DWORD op2; VirtualProtect(fa, 4, PAGE_READWRITE, &op2); const float fs = *fa;
                *fa = 1.0f; uiDecide(); ld &= !ui::flat(); Sleep(200); uiDecide(); ld &= ui::flat(); *fa = 0.95f; uiDecide(); ld &= !ui::flat(); *fa = fs; }
            memcpy(sc, sv, sizeof sv); g_ui.loadTheater = 0;
            lay &= ld; logf("loading-screen theatre test (world mode 1, reason 9, black screen): %s", ld ? "PASS" : "FAIL");
        }
        uiOk = geo && bind && mv && lay;
        logf("UI test: panel geometry=%d convergence (left %.1f / right %.1f px) bind+scissor mapping=%d video/world/pause flat detection + flat render=%d -> %s",
             (int)geo, (pl[0] + pl[2]) * 0.5f, (pr[0] + pr[2]) * 0.5f, (int)bind, (int)mv, uiOk ? "PASS" : "FAIL");
        ui::theaterMode = 0; ui::hudDeg = 55; ui::pauseMask = 0x80; ui::flatNow = false; g_flatRender = false; g_flatCut = 0; g_cfg.xr = sx; g_ui = UiCfg{};
    }
    MH_Uninitialize(); logflush(); CloseHandle(g_log);
    return bad || en != MH_OK || dis != MH_OK || !restored || !tabTest || !simOk || !sweepOk || !cutOk || !occOk || !taaOk || !fastOk || !dlssOk || !uiOk || !rsOk ? 3 : 0;
}
#endif

extern "C" BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, void *) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_self = h; DisableThreadLibraryCalls(h);
        InitializeCriticalSection(&g_logcs); InitializeCriticalSection(&g_viewcs); InitializeCriticalSection(&g_rescs); InitializeCriticalSection(&pub::cs); InitializeCriticalSection(&meter::lcs); InitializeCriticalSection(&meter::pcs); InitializeCriticalSection(&taa::repcs); InitializeCriticalSection(&taa::bcs);
#ifndef GOTVR_SELFTEST
        HANDLE t = CreateThread(nullptr, 0, worker, nullptr, 0, nullptr); if (t) CloseHandle(t);
#endif
    } else if (reason == DLL_PROCESS_DETACH) {
#ifndef GOTVR_SELFTEST
        mblur::restore(); shade::restore();
#endif
        logflush();
    }
    return TRUE;
}
