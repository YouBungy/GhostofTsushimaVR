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
#include "MinHook.h"
#include "gotvr_shared.h"

typedef uint64_t u64; typedef uint32_t u32; typedef uint8_t u8;
#define GOTVR_VERSION "GoTVR 0.13.0 (xr=4 SYNCHRONIZED-SEQUENTIAL stereo: both eyes from one simulation tick, pair-atomic presentation, per-eye occlusion + TAA history, left-eye monitor mirror, HUD on a converged panel, theatre screen for videos/pause/menus (flat-rendered); xr=3 experimental)"
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
    RVA_CLOCK_UPDATE = 0x56ED80, RVA_CLOCK = 0x1489DF0, RVA_CAM_TEMPORAL = 0x808EF0, RVA_CAMS_STOREPREV = 0x8085D0, RVA_RV_OCC = 0x9E5350, RVA_OCC_SLOTS = 0x3389240, RVA_OCC_DIMS = 0x33891A4, RVA_PARITY = 0x148A3F8, RVA_OCC_ENABLE = 0x1804010, RVA_AA_RESOLVE = 0x614EB0, RVA_AA_VELOCITY = 0x614720, RVA_TAA_DRAW = 0x7E7D10, RVA_FRAMEID = 0x19F1A50, RVA_AA_OBJ = 0x3376280, RVA_SL_CONSTS = 0x807140, RVA_NGX_CREATE = 0x4B91B0, RVA_NGX_EVAL = 0x4B9300, RVA_NGX_RELEASE = 0x4B96B0,
    RVA_PASSJOB = 0x9B55B0, RVA_PASSBODY = 0x9B55D0, RVA_INLINE_RET = 0x9B5431, RVA_PASSTAB = 0x1210930, RVA_FN_NOP = 0x3FCBD0, NPASS = 69,
    RVA_RT_BIND = 0x7E34E0, RVA_CTX_VIEWPORT = 0x50ECA0, RVA_CTX_SCISSOR = 0x50EC60, RVA_MOVIE_DRAW = 0x7EAF80, RVA_UI_TARGET = 0x3377510, RVA_WORLD_ON = 0x5A0450, RVA_UI60_PRE = 0x6E3790,   // Reflex frame id: incremented first thing in GameTick; Present tags desc[0] with it
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
static u64 padGet(int which, FARPROC f, u64 a, u64 b, u64 c, u64 d) {
    DWORD i = (DWORD)a; u8 *st = (u8 *)b;
    if (i < 4 && st && g_freezeNow.load() && g_padHave[which][i]) { memcpy(st, g_padLast[which][i], 16); return 0; }
    u64 r = f ? ((FG)f)(a, b, c, d) : 1167;
    if (r == 0 && i < 4 && st) { memcpy(g_padLast[which][i], st, 16); g_padHave[which][i] = true; }
    return r;
}
extern "C" u64 WINAPI XInputGetState(u64 a, u64 b, u64 c, u64 d) { return padGet(0, xreal("XInputGetState"), a, b, c, d); }
extern "C" u64 WINAPI Ord100(u64 a, u64 b, u64 c, u64 d) { return padGet(1, xreal((const char *)(uintptr_t)100), a, b, c, d); }
FWD(XInputSetState, 1167) FWD(XInputGetCapabilities, 1167) FWD(XInputEnable, 0)
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
static bool readable(const void *p, size_t n) {
    MEMORY_BASIC_INFORMATION m; if (!p || !VirtualQuery(p, &m, sizeof m)) return false;
    if (m.State != MEM_COMMIT || (m.Protect & PAGE_GUARD) || (m.Protect & PAGE_NOACCESS)) return false;
    return (const u8 *)p + n <= (const u8 *)m.BaseAddress + m.RegionSize;
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
static float g_fastTurnDeg = 2.0f; static int g_fastTurnMode = 1; static float g_fovMarginDeg = 3.0f;
struct UiCfg { int hud = 1; float hudDeg = 55.0f, hudDist = 1.6f, hudDown = 3.0f; int theater = 1; float thDist = 3.0f, thWidth = 3.2f, thHeight = 0.0f; int thBackdrop = 0; int thCrop = 0; int flatRender = 1; u32 pauseMask = 0x80; };
static UiCfg g_ui;   // 0.12.0 UI/HUD/video settings (playbook ch04)
static std::atomic<bool> g_flatRender{false};   // 0.13.0 xr=4: this tick is a flat context rendered as a normal flat frame (game thread decides at tick start)
static std::atomic<int> g_flatCut{0};           // camera cuts still to force after a flat <-> stereo switch
static float g_ipd = 0.064f;   // metres between the eyes (from the runtime's eye poses; game thread)
static void loadConfig() {
    wchar_t ini[MAX_PATH]; swprintf(ini, MAX_PATH, L"%ls\\gotvr.ini", g_dir);
    if (GetFileAttributesW(ini) == INVALID_FILE_ATTRIBUTES) {
        const char *def =
            "; GoTVR 0.6 settings. Restart the game after editing.\r\n"
            "[gotvr]\r\n"
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
            "; history_mode (xr=4): 1..5 (see README), default 1; F6 cycles them, the number shows as squares in the headset\r\n"
            "; monitor_left_eye (xr=4): 1 = the game window shows the left eye only (default), 0 = every rendered frame\r\n"
            "; cull_follows_head (xr=4): 1 = distance/shadow culling uses your eye camera (default; F7 toggles), 0 = the game camera\r\n"
            "; fov_margin_deg (xr=4): render each eye this many degrees wider than the lens on every side (3.0) so fast turns never show an edge\r\n"
            "; dlss_per_eye (xr=4): 0 = off (default; use the game's TAA, DLSS is not supported in VR), 1/2 = experimental per-eye DLSS\r\n"
            "; hud (xr=4): 1 = HUD and menus drawn on a flat panel in front of you at the right aspect ratio (default), 0 = stretched over the lens\r\n"
            "; hud_width_deg / hud_distance / hud_down_deg: panel width in degrees (55), distance it converges at in metres (1.6), centre below eye level (3); F9 cycles sizes\r\n"
            "; theater (xr=4): 1 = videos and world-less menus on a world-locked 16:9 screen (default), 0 = never; F10 cycles auto/always/never\r\n"
            "; theater_distance / theater_width / theater_height: screen distance (3.0 m), width (3.2 m), centre height vs your eyes (0.0 m)\r\n"
            "; theater_backdrop: 0 = black room around the screen (default), 1 = the game world behind it\r\n"
            "; theater_flat_render: 1 = while the screen is up the game renders a normal flat frame (default), 0 = the stereo image\r\n"
            "; pause_channels: game time-scale reasons that put the game on the screen (bit mask, 0x80 = pausing menus)\r\n"
            "; fast_turn_deg / fast_turn_mode (xr=4): head turn per eye frame pair above which culling memory is not trusted (2.0);\r\n"
            ";   mode 1 = off during the turn (default), 2 = game's own memory during the turn, 0 = no special handling (F8 cycles)\r\n";
        HANDLE f = CreateFileW(ini, GENERIC_WRITE, 0, nullptr, CREATE_NEW, 0, nullptr);
        if (f != INVALID_HANDLE_VALUE) { DWORD w; WriteFile(f, def, (DWORD)strlen(def), &w, nullptr); CloseHandle(f); }
    }
    g_cfg.xr = GetPrivateProfileIntW(L"gotvr", L"xr", 0, ini);
    g_cfg.census = GetPrivateProfileIntW(L"gotvr", L"census", 1, ini);
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
    g_cfg.dlssMode = GetPrivateProfileIntW(L"gotvr", L"dlss_per_eye", 0, ini); if (g_cfg.dlssMode < 0 || g_cfg.dlssMode > 2) g_cfg.dlssMode = 0;
    {   auto fl = [&](const wchar_t *k, float def, float lo, float hi) { wchar_t b[32], d[32]; swprintf(d, 32, L"%g", def); GetPrivateProfileStringW(L"gotvr", k, d, b, 32, ini); float v = (float)_wtof(b); return (v >= lo && v <= hi) ? v : def; };
        g_ui.hud = GetPrivateProfileIntW(L"gotvr", L"hud", 1, ini) ? 1 : 0;
        g_ui.hudDeg = fl(L"hud_width_deg", 55.0f, 20.0f, 120.0f); g_ui.hudDist = fl(L"hud_distance", 1.6f, 0.5f, 20.0f); g_ui.hudDown = fl(L"hud_down_deg", 3.0f, -30.0f, 30.0f);
        g_ui.theater = GetPrivateProfileIntW(L"gotvr", L"theater", 1, ini) ? 1 : 0;
        g_ui.thDist = fl(L"theater_distance", 3.0f, 0.5f, 30.0f); g_ui.thWidth = fl(L"theater_width", 3.2f, 0.3f, 40.0f); g_ui.thHeight = fl(L"theater_height", 0.0f, -5.0f, 5.0f);
        g_ui.thBackdrop = GetPrivateProfileIntW(L"gotvr", L"theater_backdrop", 0, ini) ? 1 : 0; g_ui.thCrop = GetPrivateProfileIntW(L"gotvr", L"theater_crop", 0, ini) ? 1 : 0;
        g_ui.flatRender = GetPrivateProfileIntW(L"gotvr", L"theater_flat_render", 1, ini) ? 1 : 0;
        { wchar_t b[32]; GetPrivateProfileStringW(L"gotvr", L"pause_channels", L"0x80", b, 32, ini); g_ui.pauseMask = (u32)wcstoul(b, nullptr, 0) & 0x0FFFFFFFu; } }
    m_cullHead = GetPrivateProfileIntW(L"gotvr", L"cull_follows_head", 1, ini) ? 1 : 0;
    { wchar_t b[32]; GetPrivateProfileStringW(L"gotvr", L"fast_turn_deg", L"2.0", b, 32, ini); g_fastTurnDeg = (float)_wtof(b); if (!(g_fastTurnDeg > 0.1f && g_fastTurnDeg < 90.0f)) g_fastTurnDeg = 2.0f; }
    { wchar_t b[32]; GetPrivateProfileStringW(L"gotvr", L"fov_margin_deg", L"3.0", b, 32, ini); g_fovMarginDeg = (float)_wtof(b); if (!(g_fovMarginDeg >= 0.0f && g_fovMarginDeg <= 15.0f)) g_fovMarginDeg = 3.0f; }
    g_fastTurnMode = GetPrivateProfileIntW(L"gotvr", L"fast_turn_mode", 1, ini); if (g_fastTurnMode < 0 || g_fastTurnMode > 2) g_fastTurnMode = 1;
    // xr=4 (0.9.2, BUG-23): history preset 1..5 (see kPresets / README); default 1 = HYBRID
    g_cfg.histMode = GetPrivateProfileIntW(L"gotvr", L"history_mode", 1, ini) - 1; if (g_cfg.histMode < 0 || g_cfg.histMode > 4) g_cfg.histMode = 0;
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
static std::atomic<int> m_histReset{-1}; // F6: force the camera-cut flag every VR frame (TAA/DLSS history reset A/B); -1 = use ini
static std::atomic<u64> c_engineCuts{0};  // camera cuts the ENGINE raised itself (seen at Cam_TemporalUpdate entry)

// ---------------- shared block with GoTVRHost.exe ----------------
static GvShared *S; static HANDLE g_map, g_hostProc;
static bool hostFresh() { return S && S->hostState == GV_HOST_RUNNING && (u64)GetTickCount64() - (u64)S->hostBeat < 1000; }

// One packet per game frame, built on the game thread the first time the frame touches the render
// camera, and found again at Present by the frame id the engine tags its Present with (desc[0]).
struct FramePkt { volatile LONG seq; u32 frame; int eye; bool valid; GvPose pose; float fov[4]; bool stereo; GvPose pose2; float fov2[4]; bool flat; };
static int g_pairPhase = 0;   // xr=4: 0 = left eye, simulated tick; 1 = right eye, frozen tick (game thread)
struct PairSample { bool ok; GvPose e[2]; float f[2][4]; }; static PairSample g_pairSample{};
static std::atomic<u64> g_frozenTicks{0}, g_simTicks{0}; static bool g_lockGame = false;
static int g_runEye = 0; static std::atomic<u64> g_rfixCalls{0}; static float g_lastDelta[3];
static int g_fixCulprit = -1;   // pass the second render takes from the first (F2 result / right_eye_reuse)   // native stereo: which eye CAM-A is composed for right now (0 = left/first render, 1 = right/second render). Game thread.
static std::atomic<int> m_offTest{0};   // F1: second render's camera 50 cm to the right (flat proof of a real second camera)
static FramePkt g_ring[32];
static FramePkt g_cur; static u32 g_curFrame = 0xFFFFFFFFu;   // game thread only
static u32 frameCtr() { return *(volatile u32 *)(g_base + RVA_FRAMECTR); }
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
static std::atomic<u64> calls{0}, fixed{0}, skipBranch{0}, skipDims{0}, skipBank{0};
typedef void (*FnSP)(u8 *); static FnSP oSP;
static void hStorePrev(u8 *cam) {
    oSP(cam);   // prev slots := current = the camera of the LAST tick, i.e. eye g_camEye
    if (g_cfg.xr != 4 || cam != *(u8 **)(g_base + RVA_CAMA)) return;
    int e = g_camEye.load(); if (e < 0 || e > 1) return;
    Bank &b = bank[e]; memcpy(b.vp, cam + 0x380, 64); memcpy(b.pos, cam + 0x560, 16);
    memcpy(b.w180, cam + 0x180, 0x80); memcpy(b.w380, cam + 0x380, 0x80); memcpy(b.w560, cam + 0x560, 0x10); b.t674 = *(u32 *)(cam + 0x674); b.ok = true;
}
static void reseed() { bank[0].ok = bank[1].ok = false; }
// BUG-36 (0.10.3, playbook ch14 "treat a pose gap as a camera cut" / STR-015 "detect your own pose gaps"): the per-eye
// occlusion history is two ticks old. A fast head turn makes that gap large: the HiZ test reprojects across a big
// rotation, objects entering from outside the old view test against clamped edge depth and are culled until the turn
// slows ("only when I move my head fast, even if I look at the object"). When this eye's rotation since its previous
// frame exceeds fast_turn_deg, this tick's occlusion history is declared invalid (the engine's own cut path for the
// occlusion setup, as on a camera cut): everything is drawn for that frame.
static float fastDeg = 2.0f; static std::atomic<int> fastMode{1};   // 1 = occlusion history invalid, 2 = engine's cross-eye (1 frame) history
static std::atomic<bool> fast{false}; static std::atomic<u64> fastTicks{0}; static float lastAngle = 0;
static void setVoFast(bool f);   // defined after the cut overrides
static void fastCheck(u8 *cam) {
    bool f = false;
    const int e = g_pairPhase;
    if (on.load() && g_cfg.xr == 4 && bank[e].ok) {
        const float *cw = (const float *)(cam + 0x140), *pw = (const float *)(bank[e].w180 + 0x40);   // +0x1C0 = that tick's world
        float worst = 1.0f;
        for (int r = 0; r < 3; r++) {
            float d = 0, la = 0, lb = 0; for (int k = 0; k < 3; k++) { d += cw[r * 4 + k] * pw[r * 4 + k]; la += cw[r * 4 + k] * cw[r * 4 + k]; lb += pw[r * 4 + k] * pw[r * 4 + k]; }
            if (la > 1e-6f && lb > 1e-6f) worst = fminf(worst, d / sqrtf(la * lb));
        }
        lastAngle = acosf(fmaxf(-1.0f, fminf(1.0f, worst))) * 57.2957795f;
        f = fastMode.load() != 0 && lastAngle > fastDeg;
    }
    if (f) fastTicks++;
    if (f != fast.load()) { fast = f; setVoFast(f); }
}
typedef void (*FnOcc)(u8 *); static FnOcc oOcc;
static void hOcc(u8 *v) {
    oOcc(v); calls++;
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
    {"1 PER-EYE OCCLUSION + PER-EYE TAA (this eye's history and reprojection)", 0, 0, 0, 0, 0, 0, 1, 3},
    {"2 PER-EYE OCCLUSION, engine TAA (the 'great' one)", 0, 0, 0, 0, 0, 0, 1, 0},
    {"3 PER-EYE OCCLUSION, TAA reset every frame", 0, 0, 0, 0, 1, 0, 1, 0},
    {"4 KEPT: everything cross-eye", 0, 0, 0, 0, 0, 0, 0, 0},
    {"5 RESET: global cut every frame (0.9.0)", 1, 0, 0, 0, 0, 0, 0, 0},
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
static void hGameTick() {
    c_tick++; t_tick = GetCurrentThreadId();
    if (g_cfg.xr != 4) { oGameTick(); return; }
    uiDecide();   // 0.13.0: flat context for this tick (pause / video / no world / F10)
    if (g_flatRender.load()) { g_simTicks++; oGameTick(); g_pairPhase = 0; return; }   // every tick simulated and shown; the next stereo pair starts with the left eye
    const bool freeze = g_pairPhase == 1 && S && hostFresh() && S->imagesOk && !S->gameRefuse;
    float *clk = (float *)(g_base + RVA_CLOCK); float saved[24];
    if (freeze) { memcpy(saved, clk, sizeof saved); clk[2] = 0; clk[6] = 0; clk[0xe] = 0; clk[0x12] = 0; g_lockGame = true; g_freezeNow = 1; g_frozenTicks++; }
    else g_simTicks++;
    oGameTick();
    if (freeze) { g_freezeNow = 0; g_lockGame = false; memcpy(clk, saved, sizeof saved); }   // the next tick's early code sees tick A's steps again
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

// Build this frame's packet from the host's latest located views (game thread).
static void buildPacket(u32 f, FramePkt &p) {
    p.frame = f; p.valid = false; p.eye = -1; p.stereo = false; p.flat = false;
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
        int e = g_pairPhase; p.eye = e; p.pose = g_pairSample.e[e]; memcpy(p.fov, g_pairSample.f[e], 16);
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
    gvWriteBegin(&r.seq); r.frame = g_cur.frame; r.eye = g_cur.eye; r.valid = g_cur.valid; r.flat = g_cur.flat; r.pose = g_cur.pose; memcpy(r.fov, g_cur.fov, 16);
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

static void hSlot43(void *cam) {
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
            auto compose = [&](const GvPose *pose, float *o) {
                memcpy(o, gm, 64);
                if (pose) {
                    if (g_cfg.level) {   // anchor = game camera position + yaw only; the head owns pitch and roll
                        float fx = o[0], fy = o[1], n = sqrtf(fx * fx + fy * fy);
                        if (n > 1e-4f) { fx /= n; fy /= n; o[0] = fx; o[1] = fy; o[2] = 0; o[4] = -fy; o[5] = fx; o[6] = 0; o[8] = 0; o[9] = 0; o[10] = 1; }
                    }
                    float Rg[3][3], dg[3]; poseLocal(*pose, Rg, dg);
                    float r[3][4]; memcpy(r, o, 48);
                    for (int i = 0; i < 3; i++) for (int c = 0; c < 3; c++) { float v = 0; for (int k = 0; k < 3; k++) v += Rg[k][i] * r[k][c]; o[i * 4 + c] = v; }
                    if (g_cfg.headPos) for (int c = 0; c < 3; c++) { float v = 0; for (int k = 0; k < 3; k++) v += dg[k] * r[k][c]; o[12 + c] += v * g_cfg.unitsPerM; }
                }
                if (m_yaw.load()) { const float a = 5.0f * 3.14159265f / 180.0f, c = cosf(a), s = sinf(a); rotateZ(o + 0, c, s); rotateZ(o + 4, c, s); rotateZ(o + 8, c, s); }
            };
            float o[16]; compose(vr ? &epose : nullptr, o);
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
static std::atomic<int> theaterMode{0};                                      // F10: 0 auto, 1 always, 2 never
static int crop = 0;                                                         // theater_crop: show only the 16:9 band of a video
struct View { bool ok; int eye; float tl, tr, tu, td, ex; };
static View cur{};   // written on the game thread at the main view's RV_Post, read by that frame's UI pass jobs
static std::atomic<u32> post{0}, lastMovie{0x80000000u}; static std::atomic<bool> worldOff{false};
static std::atomic<u64> binds{0}, mapped{0}, scissors{0}, movieDraws{0}, flatPosts{0}, skipFlat{0}, skipNoView{0}, posts{0};
struct Flag { u32 frame, flags; }; static Flag ring[32];
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
static bool flatRaw() { int t = theaterMode.load(); if (t == 1) return true; if (t == 2) return false; return g_ui.theater && (worldOff.load() || movieNow() || pausedTick.load()); }
static bool flat() { return flatNow.load(); }   // decided once per tick (uiDecide), so the render, the UI mapping and the published flags agree
// UI target probe (for the next step, the UI on its own sharp layer): what resource the UI passes draw into
static std::atomic<ID3D12Resource *> probeNat{nullptr}; static std::atomic<u32> probeFlags{0}, probeCount{0}; static std::atomic<int> probeLogged{0};
// pixel rect (x0, y0, x1, y1) of the HUD panel in a W x H target for view v; false = no mapping
static bool panel(const View &v, int W, int H, float deg, float out[4]) {
    if (!v.ok || W <= 0 || H <= 0 || !(deg > 1.0f)) return false;
    const float d = g_ui.hudDist, hw = d * tanf(deg * 0.5f / 57.2957795f), hh = hw * (float)H / (float)W, cy = -d * tanf(g_ui.hudDown / 57.2957795f);
    const float x0 = (-hw - v.ex) / d, x1 = (hw - v.ex) / d, yT = (cy + hh) / d, yB = (cy - hh) / d;   // tangents seen from this eye
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
static bool active(float p[4], const int r[4]) {
    if (!hud.load() || g_cfg.xr != 4) return false;
    if (flat()) { skipFlat++; return false; }
    if (!panel(cur, r[2] - r[0], r[3] - r[1], hudDeg.load(), p)) { skipNoView++; return false; }
    return true;
}
static void hRT(u8 *ctx, u8 *target, int *rect, int slice) {
    liveDel(ctx);   // the bind's own scissor call belongs to the NEW target
    oRT(ctx, target, rect, slice);
    if (!target || target != g_base + RVA_UI_TARGET) return;
    binds++;
    {   // probe: colour attachment 0 (ctx+0x2C8, set by Ctx_BindColor 0x50E430) -> resource record (+0x18) -> native
        u8 *att = *(u8 **)(ctx + 0x2C8); u8 *rr = (att && readable(att + 0x18, 8)) ? *(u8 **)(att + 0x18) : nullptr;
        ID3D12Resource *nat = taa::nativeRec(rr);
        if (nat != probeNat.load()) { probeNat = nat; probeFlags = *(u32 *)(target + 0xC8); probeCount = *(u32 *)(target + 0x618); probeLogged = 0; }
    }
    const int *r = (const int *)(ctx + 0x2948); float p[4];
    if (r[2] - r[0] <= 0 || r[3] - r[1] <= 0 || !active(p, r)) return;
    u32 v[4]; mapRect(r, p, (u32)r[0], (u32)r[1], (u32)r[2], (u32)r[3], v, false);
    ((FnVP)(g_base + RVA_CTX_VIEWPORT))(ctx, v[0], v[1], v[2], v[3]);
    const bool bb = (*(u32 *)(target + 0xC8) & 0x400000) != 0;   // back-buffer targets set their scissor elsewhere: use the full rect
    const u32 *sc = (const u32 *)(ctx + 0x328); u32 m[4];
    if (bb) mapRect(r, p, (u32)r[0], (u32)r[1], (u32)r[2], (u32)r[3], m, true); else mapRect(r, p, sc[0], sc[1], sc[2], sc[3], m, true);
    oSC(ctx, m[0], m[1], m[2], m[3]);
    liveAdd(ctx); mapped++;
}
static void hSC(u8 *ctx, u32 l, u32 t, u32 rr, u32 b) {
    if (liveFind(ctx) >= 0) {
        const int *r = (const int *)(ctx + 0x2948); float p[4];
        if (r[2] - r[0] > 0 && r[3] - r[1] > 0 && active(p, r)) { u32 m[4]; mapRect(r, p, l, t, rr, b, m, true); scissors++; oSC(ctx, m[0], m[1], m[2], m[3]); return; }
    }
    oSC(ctx, l, t, rr, b);
}
static void hMovie(void *a, void *b, void *c, void *d) { lastMovie = post.load(); movieDraws++; oMovie(a, b, c, d); }
static void beginPost() {   // game thread, main view, before its render graph is scheduled
    post++; posts++;
    worldMode = *(volatile u8 *)(r1::frameObj() + 0x80F09); worldOff = worldMode.load() == 0;
    View v{}; const FramePkt &k = g_cur;
    if (g_cfg.xr == 4 && k.valid && k.eye >= 0 && k.eye < 2) {
        v.ok = true; v.eye = k.eye; v.tl = tanf(k.fov[0]); v.tr = tanf(k.fov[1]); v.tu = tanf(k.fov[2]); v.td = tanf(k.fov[3]); v.ex = (k.eye == 0 ? -0.5f : 0.5f) * g_ipd;
    }
    cur = v;
}
static void endPost() {   // game thread: the flat-context decision for this frame's published image
    const u32 f = frameCtr(); const bool fl = flat(), mv = movieNow(); if (fl) flatPosts++;
    ring[f & 31] = {f, fl ? (GV_SLOT_THEATER | (crop && mv ? GV_SLOT_CROP169 : 0u) | (g_flatRender.load() ? GV_SLOT_FLAT : 0u)) : 0u};
    static int was = -1; const int st = (fl ? 1 : 0) | (mv ? 2 : 0) | (worldMode.load() << 2) | (pausedTick.load() ? 64 : 0);
    if (st != was) { logf("UI: %s (video=%d, paused=%d, world mode=%u, theatre setting=%s, flat render=%d)", fl ? "FLAT CONTEXT -> theatre screen" : "3D -> HUD panel", mv, (int)pausedTick.load(), worldMode.load(),
                          theaterMode.load() == 1 ? "always" : theaterMode.load() == 2 ? "never" : "auto", (int)g_flatRender.load()); was = st; }
}
static u32 flagsOf(u32 frame) { const Flag &x = ring[frame & 31]; return x.frame == frame ? x.flags : 0u; }
} // namespace ui

// game thread, start of every xr=4 tick: one flat-context decision per tick (render path, UI mapping and flags agree)
static void uiDecide() {
    ui::pausedTick = ui::pausedNow();
    const bool fl = ui::flatRaw(), fr = fl && g_ui.flatRender;
    if (fr != g_flatRender.load()) { g_flatCut = 2; logf("UI: %s", fr ? "flat context -> the game renders a normal flat frame for the theatre screen" : "back to stereo"); }
    ui::flatNow = fl; g_flatRender = fr;
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
static bool ensure(IDXGISwapChain3 *sc) {
    if (count) return true;
    if (!sc) return false;
    static u64 lastTry = 0; u64 now = GetTickCount64(); if (lastTry && now - lastTry < 10000) return false; lastTry = now;   // 0.8.2 log: rescanned every frame
    DXGI_SWAP_CHAIN_DESC1 d{}; if (FAILED(sc->GetDesc1(&d)) || d.BufferCount == 0 || d.BufferCount > 8) return false;
    u32 found = 0;
    for (u32 i = 0; i < d.BufferCount; i++) { ID3D12Resource *r = nullptr; if (FAILED(sc->GetBuffer(i, IID_PPV_ARGS(&r)))) return false; nat[i] = r; r->Release(); obj[i] = findObj(r); found += obj[i] != nullptr; }
    count = d.BufferCount;
    logf("bbmap: %u back buffers, engine resource objects found for %u (%p %p %p)", count, found, (void *)obj[0], (void *)obj[1], count > 2 ? (void *)obj[2] : nullptr);
    if (found != count) { count = 0; return false; }
    return true;
}
static void notePresent(u32 frame, u32 idx) { lastFrame = frame; lastIdx = idx; have = 1; }
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
static void copyFrame(u32 frame) {
    FramePkt pk{}; FramePkt &rr = g_ring[frame & 31];
    bool got = gvRead(&rr.seq, [&] { pk.frame = rr.frame; pk.eye = rr.eye; pk.valid = rr.valid; pk.flat = rr.flat; pk.pose = rr.pose; memcpy(pk.fov, rr.fov, 16); pk.stereo = rr.stereo; pk.pose2 = rr.pose2; memcpy(pk.fov2, rr.fov2, 16); });
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
    if (g_cfg.xr == 4 && g_cfg.mirrorLeft && !st && pk.eye == 1 && lastLeftTi < GV_TEXCOUNT && tex[lastLeftTi] && lastLeftFrame == frame - 1) {
        D3D12_RESOURCE_BARRIER br{}; br.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; br.Transition.pResource = bb; br.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        br.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE; br.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST; cl->ResourceBarrier(1, &br);
        cl->CopyResource(bb, tex[lastLeftTi]);
        br.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST; br.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT; cl->ResourceBarrier(1, &br);
        mirrored++;
    }
    if (g_cfg.xr == 4 && !st && pk.eye == 0 && pk.valid) { lastLeftTi = ti; lastLeftFrame = frame; }
    if (ui::probeNat.load() && !ui::probeLogged.exchange(1)) {   // UI target probe (0.13.0): separate texture, or the back buffer itself?
        ID3D12Resource *un = ui::probeNat.load(); D3D12_RESOURCE_DESC ud = un->GetDesc(); bool isBB = un == bb;
        for (u32 i = 0; i < bbmap::count && i < 8; i++) isBB |= bbmap::nat[i] == un;
        logf("UI probe: UI target colour = %p %llux%u format %d flags 0x%x (target flags 0x%x, %u colour attachments) -> %s", (void *)un, (unsigned long long)ud.Width, ud.Height, (int)ud.Format, (unsigned)ud.Flags,
             ui::probeFlags.load(), ui::probeCount.load(), isBB ? "the BACK BUFFER (UI drawn over the frame)" : "a SEPARATE texture (composited later)");
    }
    cl->Close(); ID3D12CommandList *l[] = {cl}; q->ExecuteCommandLists(1, l);
    u64 v = ++fenceVal; q->Signal(fence, v); allocVal[k] = v;
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
        const u32 tag = (g_cfg.xr == 4 && pk.valid && pk.eye == 1) ? frame - 1 : frame;
        // flat frame (0.13.0): one image for both slots, tagged the same frame, always on the theatre screen
        const u32 fl = pk.valid ? ui::flagsOf(frame) : (ui::flagsOf(frame) | GV_SLOT_THEATER | GV_SLOT_FLAT);
        gvWriteBegin(&sl.seq); sl.tex = ti; sl.gen = (u32)S->texGen; sl.fence = v; sl.frame = tag; sl.pose = pk.pose; memcpy(sl.fov, pk.fov, 16); sl.flags = fl; gvWriteEnd(&sl.seq);
        if (!pk.valid) flatPub++;
        if (g_cfg.xr == 4 && pk.eye == 1) pairsPub++;
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
static void hookNative() {
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
        pub::hooked = true; void **vt = *(void ***)n;
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
    wchar_t exe[MAX_PATH], cmd[MAX_PATH + 32]; swprintf(exe, MAX_PATH, L"%ls\\GoTVRHost.exe", g_dir);
    if (GetFileAttributesW(exe) == INVALID_FILE_ATTRIBUTES) {   // also accept it next to the game exe
        wchar_t alt[MAX_PATH]; wcscpy(alt, g_dir); if (wchar_t *sl = wcsrchr(alt, L'\\')) wcscpy(sl, L"\\GoTVRHost.exe");
        logf("host: %ls not found (err %lu); trying %ls", exe, GetLastError(), alt);
        if (GetFileAttributesW(alt) != INVALID_FILE_ATTRIBUTES) wcscpy(exe, alt);
    }
    swprintf(cmd, MAX_PATH + 32, L"\"%ls\" %lu", exe, GetCurrentProcessId());
    STARTUPINFOW si{}; si.cb = sizeof si; PROCESS_INFORMATION pi{};
    if (!CreateProcessW(exe, cmd, nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, g_dir, &si, &pi)) { logf("host: cannot start %ls (err %lu) -> headset off, game flat", exe, GetLastError()); return false; }
    CloseHandle(pi.hThread); g_hostProc = pi.hProcess;
    logf("host: started GoTVRHost.exe pid %lu (mode %d); its log is GoTVR\\host.log", pi.dwProcessId, g_cfg.xr);
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
    if (MH_Initialize() != MH_OK) { logf("MinHook init failed"); return 0; }
    struct H { u32 rva; void *det; void **orig; const char *name; } hooks[] = {
        {RVA_GAMETICK, (void *)hGameTick, (void **)&oGameTick, "GameTick"}, {RVA_PRESENT, (void *)hPresent, (void **)&oPresent, "Present"},
        {RVA_SETLOCAL, (void *)hSetLocal, (void **)&oSetLocal, "SetLocalMatrix"}, {RVA_SLOT43, (void *)hSlot43, (void **)&oSlot43, "vUpdateFrustum43"},
        {RVA_SETPROJRECT, (void *)hSetProjRect, (void **)&oSetProjRect, "SetProjRect"}, {RVA_FRAMEVIEW, (void *)hFrameView, (void **)&oFrameView, "FrameView"},
        {RVA_RVSUBMIT, (void *)hRVSubmit, (void **)&oRVSubmit, "RV_Submit"},
        {RVA_FV_LIST, (void *)hFvList, (void **)&r1::oFvList, "FrameView_ViewList"}, {RVA_RV_POST, (void *)hRVPost, (void **)&r1::oRVPost, "RV_Post"},
        {RVA_SUBMITGRAPH, (void *)hSubmitGraph, (void **)&oSubmitGraph, "Jobs_SubmitGraph"},
        {RVA_VC_END, (void *)r1::hVcEnd, (void **)&r1::oVcEnd, "VC_FrameEnd"}, {RVA_PASSBODY, (void *)r1::hPassBody, (void **)&r1::oPassBody, "Pass_Body"}, {RVA_CLOCK_UPDATE, (void *)hClock, (void **)&oClock, "Clock_Update"}, {RVA_CAM_TEMPORAL, (void *)hCamT, (void **)&oCamT, "Cam_TemporalUpdate"}, {RVA_CAMS_STOREPREV, (void *)occ::hStorePrev, (void **)&occ::oSP, "Cam_StorePrev"}, {RVA_RV_OCC, (void *)occ::hOcc, (void **)&occ::oOcc, "RV_OcclusionSetup"}, {RVA_AA_RESOLVE, (void *)taa::hRes, (void **)&taa::oRes, "AA_Resolve"}, {RVA_SL_CONSTS, (void *)dlss::hSLC, (void **)&dlss::oSLC, "Cam_BuildSLConstants"}, {RVA_NGX_CREATE, (void *)dlss::hNgxCreate, (void **)&dlss::oNgxCreate, "NGX_CreateFeature"}, {RVA_NGX_EVAL, (void *)dlss::hNgxEval, (void **)&dlss::oNgxEval, "NGX_EvaluateFeature_C"}, {RVA_NGX_RELEASE, (void *)dlss::hNgxRelease, (void **)&dlss::oNgxRelease, "NGX_ReleaseFeature"}, {RVA_AA_VELOCITY, (void *)taa::hVel, (void **)&taa::oVel, "AA_Velocity"}, {RVA_TAA_DRAW, (void *)taa::hDraw, (void **)&taa::oDraw, "TAA_Draw"}, {RVA_RCTX_BUILD, (void *)r1::hRCtxBuild, (void **)&r1::oRCtxBuild, "RCtx_Build"}, {RVA_RCTX_FLUSH, (void *)r1::hRCtxFlush, (void **)&r1::oRCtxFlush, "RCtx_FlushDraws"}, {RVA_SLPASS, (void *)hSLPass, (void **)&oSLPass, "StreamlinePass"},
        {RVA_RT_BIND, (void *)ui::hRT, (void **)&ui::oRT, "RT_Bind"}, {RVA_CTX_SCISSOR, (void *)ui::hSC, (void **)&ui::oSC, "Ctx_SetScissor"}, {RVA_MOVIE_DRAW, (void *)ui::hMovie, (void **)&ui::oMovie, "Movie_Draw"},
    };
    for (auto &h : hooks) { MH_STATUS st = MH_CreateHook(g_base + h.rva, h.det, h.orig); logf("hook %s -> %s", h.name, MH_StatusToString(st)); if (st != MH_OK) { logf("hook failed -> aborting all hooks"); MH_Uninitialize(); return 0; } }
    // the shared block must exist before any hook can read it
    if (g_cfg.xr && !startHost()) { logf("xr disabled for this run"); if (!S) g_cfg.xr = 0; }
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
    MH_STATUS st = MH_EnableHook(MH_ALL_HOOKS); logf("hooks enabled: %s", MH_StatusToString(st));
    if (g_cfg.xr == 4) {
        dlss::mode = g_cfg.dlssMode; if (g_cfg.dlssMode) dlss::install();
        logf("per-eye DLSS mode %d (0 = off: recommended, use the game's TAA; 1/2 experimental)", g_cfg.dlssMode);
        ui::hud = g_ui.hud; ui::hudDeg = g_ui.hudDeg; ui::crop = g_ui.thCrop; ui::pauseMask = g_ui.pauseMask;
        logf("UI: HUD panel %s (%.0f deg wide, converges at %.2f m, %.1f deg below eye level); theatre screen %s (%.1f m away, %.1f m wide, backdrop %d, crop %d, flat render %d, pause reasons 0x%x)",
             g_ui.hud ? "on" : "off", g_ui.hudDeg, g_ui.hudDist, g_ui.hudDown, g_ui.theater ? "auto" : "never (F10 forces it)", g_ui.thDist, g_ui.thWidth, g_ui.thBackdrop, ui::crop, g_ui.flatRender, g_ui.pauseMask);
        if (!g_ui.theater) ui::theaterMode = 2;
        occ::fastDeg = g_fastTurnDeg; occ::fastMode = g_fastTurnMode; logf("fast head-turn limit for occlusion history: %.1f deg per eye frame pair", occ::fastDeg);
        bool ok = cutp::apply(); applyPreset(ok ? g_cfg.histMode : 4);
        if (!ok) logf("cut overrides unavailable -> history preset 5 RESET (0.9.0 behaviour)");
        logf("history preset %s", kPresets[g_preset.load()].name);
    }
    logflush();
    LONG lastHostState = -1; bool hostExitLogged = false;
    bool prev[11] = {}; u64 lastRep = GetTickCount64(), lastCallers = lastRep;
    for (;;) {
        Sleep(50);
        if (g_cfg.hotkeys) {
            const int keys[11] = {VK_F7, VK_F8, VK_F9, VK_F10, VK_F11, VK_F6, VK_F5, VK_F4, VK_F3, VK_F2, VK_F1};
            for (int i = 0; i < 11; i++) {
                bool down = (GetAsyncKeyState(keys[i]) & 0x8000) != 0;
                if (down && !prev[i]) {
                    if (i == 0) {
                        if (g_cfg.xr == 4) { m_cullHead ^= 1; logf("HOTKEY F7 culling/shadow camera follows your head -> %d", m_cullHead.load()); }
                        else { m_armed ^= 1; logf("HOTKEY F7 armed zero-delta -> %d", m_armed.load()); }
                    }
                    if (i == 1) {
                        if (g_cfg.xr == 4) { int m = occ::fastMode.load() % 3 + 1; if (m == 3) m = 0; occ::fastMode = m; occ::setVoFast(occ::fast.load());
                                             logf("HOTKEY F8 fast head turns -> %s", m == 1 ? "1 culling memory off during the turn" : m == 2 ? "2 game's own culling memory during the turn" : "0 no special handling"); }
                        else { m_leftHalf ^= 1; logf("HOTKEY F8 projection left-half (EXP-03b) -> %d", m_leftHalf.load()); }
                    }
                    if (i == 2) {
                        if (g_cfg.xr == 4) {   // HUD panel size: 40 -> 55 -> 70 -> 85 -> off -> 40
                            static const float sz[4] = {40, 55, 70, 85}; float nx = 0;
                            if (!ui::hud.load()) nx = sz[0]; else for (int j = 0; j < 4; j++) if (sz[j] > ui::hudDeg.load() + 0.5f) { nx = sz[j]; break; }
                            if (nx > 0) { ui::hud = 1; ui::hudDeg = nx; logf("HOTKEY F9 HUD panel %.0f deg wide", nx); } else { ui::hud = 0; logf("HOTKEY F9 HUD panel OFF (UI stretched over the lens)"); } }
                        else { m_yaw ^= 1; logf("HOTKEY F9 camera yaw +5 deg, idempotent (EXP-02) -> %d", m_yaw.load()); }
                    }
                    if (i == 10) { m_offTest ^= 1; logf("HOTKEY F1 second render's camera 0.5 m to the right (flat stereo proof; needs F5 on) -> %d", m_offTest.load()); }
                    if (i == 9) { if (sweep::running.load()) sweep::end("cancelled with F2"); else sweep::begin(GetTickCount64()); }
                    if (i == 8) { static int k = 0; k = (k + 1) % 6; r1::skipFrom = r1::skipSteps[k]; r1::pjDump = 1;
                                  if (r1::skipFrom.load()) logf("HOTKEY F3 second run skips passes %d..68 (bisection; image should come from run 1)", r1::skipFrom.load()); else logf("HOTKEY F3 second run complete again (no skipped passes)"); }
                    if (i == 3) {
                        if (g_cfg.xr == 4) { int m = (ui::theaterMode.load() + 1) % 3; ui::theaterMode = m; logf("HOTKEY F10 theatre screen -> %s", m == 1 ? "ALWAYS (everything on the flat screen)" : m == 2 ? "never" : "auto (videos and menus without a 3D world)"); }
                        else { m_burst = 1; m_census = 1; slDump = 2; r1::pjDump = 1; logf("HOTKEY F10 snapshot + R1 view census"); } }
                    if (i == 7) { r1::dupOn ^= 1; r1::pjDump = 1; logf("HOTKEY F4 R1 main-only renderables duplicated into the second view (BUG-17 fix, A/B) -> %d", r1::dupOn.load()); }
                    if (i == 4) { m_recenter = 1; if (S) InterlockedIncrement(&S->theaterRecenter); logf("HOTKEY F11 recentre head tracking (and the theatre screen)"); }
                    if (i == 6) { r1::on ^= 1; slDump = 2; logf("HOTKEY F5 R1-A second render-graph run (same camera) -> %d", r1::on.load()); }
                    if (i == 5) {
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
              logf("   OCC/s: setup calls=%llu per-eye fixed=%llu | not steady-state=%llu dims differ=%llu no bank=%llu | eye=%d | fast-turn frames=%llu (last %.2f deg, limit %.1f)", (unsigned long long)(c - pc), (unsigned long long)(f - pf), (unsigned long long)(b - pb), (unsigned long long)(d - pd), (unsigned long long)(k - pk), g_camEye.load(), (unsigned long long)(ft - pft), occ::lastAngle, occ::fastDeg); pft = ft;
              pc = c; pf = f; pb = b; pd = d; pk = k; }
            { static u64 a0, a1, a2, a3, a4, a5, a6, a7, vs0, vw0; u64 b0 = taa::runs, b1 = taa::drawFix, b2 = taa::velFix, b3 = taa::restores, b4 = taa::saves, b5 = taa::resets, b6 = taa::stale, b7 = taa::texFail;
              { static u64 q0, q1, q2, q3, q4, q5, q6; u64 w0 = ui::binds, w1 = ui::mapped, w2 = ui::scissors, w3 = ui::movieDraws, w4 = ui::flatPosts, w5 = ui::skipNoView, w6 = ui::posts;
                static u64 q7; u64 w7 = pub::flatPub;
                logf("   UI/s: frames=%llu UI binds=%llu mapped=%llu scissors mapped=%llu | video draws=%llu flat frames=%llu (flat-rendered published %llu, paused=%d, world mode %u) | no eye view=%llu | panel %s %.0f deg, eye %d, theatre frames shown by host=%lld",
                     (unsigned long long)(w6 - q6), (unsigned long long)(w0 - q0), (unsigned long long)(w1 - q1), (unsigned long long)(w2 - q2), (unsigned long long)(w3 - q3), (unsigned long long)(w4 - q4), (unsigned long long)(w7 - q7),
                     (int)ui::pausedTick.load(), ui::worldMode.load(), (unsigned long long)(w5 - q5), ui::hud.load() ? "on" : "off", ui::hudDeg.load(), ui::cur.eye, S ? (long long)S->hostTheater : -1LL);
                q0 = w0; q1 = w1; q2 = w2; q3 = w3; q4 = w4; q5 = w5; q6 = w6; q7 = w7; }
              logf("   TAA/s: same-eye matrices=%llu draw=%llu velocity=%llu (private %llu, swapped %llu) | history restored=%llu saved=%llu | resets=%llu stale=%llu bank failures=%llu source switches=%llu recreations=%llu",
                   (unsigned long long)(b0 - a0), (unsigned long long)(b1 - a1), (unsigned long long)(b2 - a2), (unsigned long long)(taa::velSplit - vs0), (unsigned long long)(taa::velSwaps - vw0), (unsigned long long)(b3 - a3), (unsigned long long)(b4 - a4), (unsigned long long)(b5 - a5), (unsigned long long)(b6 - a6), (unsigned long long)(b7 - a7), (unsigned long long)taa::srcChanges.load(), (unsigned long long)taa::recreations.load());
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
        if (S && S->hostState != lastHostState) { lastHostState = S->hostState; logf("host state -> %ld (%s)", lastHostState, S->hostMsg); }
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
        {RVA_RT_BIND, (void *)ui::hRT, (void **)&ui::oRT, "RT_Bind"}, {RVA_CTX_SCISSOR, (void *)ui::hSC, (void **)&ui::oSC, "Ctx_SetScissor"}, {RVA_MOVIE_DRAW, (void *)ui::hMovie, (void **)&ui::oMovie, "Movie_Draw"},
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
        *par = sp; *en = se; dims[0] = sd0; *(u32 *)((u8 *)dims + 0x34) = sd1; g_cfg.xr = sx; occ::on = 0; occ::bank[1].ok = false; occ::fixed = 0;
        logf("per-eye occlusion test: %s", occOk ? "PASS" : "FAIL");
    }
    bool fastOk = false;
    {   // BUG-36: fast-turn detection between this eye's previous world and the current one
        static u8 cam[0x900]; memset(cam, 0, sizeof cam); occ::Bank &bk = occ::bank[0]; memset(&bk, 0, sizeof bk); bk.ok = true;
        float *pw = (float *)(bk.w180 + 0x40); pw[0] = 1; pw[5] = 1; pw[10] = 1;
        auto setYaw = [&](float deg) { float a = deg / 57.2957795f, c = cosf(a), sn = sinf(a); float *w = (float *)(cam + 0x140); memset(w, 0, 64); w[0] = c; w[1] = sn; w[4] = -sn; w[5] = c; w[10] = 1; };
        const int sx = g_cfg.xr; g_cfg.xr = 4; occ::on = 1; g_pairPhase = 0; occ::fastDeg = 2.0f;
        setYaw(1.0f); occ::fastCheck(cam); bool slow = !occ::fast.load() && fabsf(occ::lastAngle - 1.0f) < 0.05f;
        setYaw(5.0f); occ::fastCheck(cam); bool quick = occ::fast.load() && fabsf(occ::lastAngle - 5.0f) < 0.05f;
        setYaw(0.5f); occ::fastCheck(cam); bool back = !occ::fast.load();
        fastOk = slow && quick && back;
        g_cfg.xr = sx; occ::on = 0; bk.ok = false; occ::fastTicks = 0;
        logf("fast-turn detection test: 1 deg slow=%d, 5 deg fast=%d, back=%d -> %s", slow, quick, back, fastOk ? "PASS" : "FAIL");
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
    bool uiOk = false;
    {   // 0.12.0 HUD panel + theatre: panel geometry, convergence, and the real Ctx_SetViewport code driven by the RT_Bind hook
        const int sx = g_cfg.xr; g_cfg.xr = 4; g_ui.hudDist = 2.0f; g_ui.hudDown = 0.0f; g_ui.theater = 1;
        float p[4]; ui::View v{true, 0, -1, 1, 1, -1, 0};
        bool geo = ui::panel(v, 1000, 625, 90, p) && fabsf(p[0]) < 1e-3f && fabsf(p[2] - 1000) < 1e-3f && fabsf(p[1] - 117.1875f) < 1e-2f && fabsf(p[3] - 507.8125f) < 1e-2f;
        ui::View vl{true, 0, -1, 1, 1, -1, -0.032f}, vr{true, 1, -1, 1, 1, -1, 0.032f}; float pl[4], pr[4];
        geo &= ui::panel(vl, 1000, 625, 60, pl) && ui::panel(vr, 1000, 625, 60, pr) && fabsf((pl[0] + pl[2]) * 0.5f - 508.0f) < 0.05f && fabsf((pr[0] + pr[2]) * 0.5f - 492.0f) < 0.05f && fabsf(pl[1] - pr[1]) < 1e-3f;
        static u8 ctx[0x3000]; memset(ctx, 0, sizeof ctx);
        static float vpRec[6]; static int vpN = 0; static u32 scRec[4]; static int scN = 0;
        typedef void (*Vt)(void *, UINT, const void *);
        static void *vt[32]; vt[21] = (void *)(Vt)[](void *, UINT, const void *q) { memcpy(vpRec, q, sizeof vpRec); vpN++; };
        static u64 L[1], A[1]; L[0] = (u64)vt; A[0] = (u64)L; *(u64 *)ctx = (u64)A;
        ui::oRT = [](u8 *c, u8 *t, int *, int) { int *r = (int *)(c + 0x2948); r[0] = 0; r[1] = 0; r[2] = 2560; r[3] = 1600; u32 *q = (u32 *)(c + 0x328); q[0] = 0; q[1] = 0; q[2] = 2560; q[3] = 1600; *(u8 **)(c + 0x2940) = t; };
        ui::oSC = [](u8 *, u32 l, u32 t, u32 r, u32 b) { scRec[0] = l; scRec[1] = t; scRec[2] = r; scRec[3] = b; scN++; };
        ui::oMovie = [](void *, void *, void *, void *) {};
        ui::hud = 1; ui::hudDeg = 90; ui::theaterMode = 2; ui::cur = v; ui::pauseMask = 0; uiDecide();
        ui::hRT(ctx, g_base + RVA_UI_TARGET, nullptr, -1);
        bool bind = vpN == 1 && vpRec[0] == 0 && vpRec[1] == 300 && vpRec[2] == 2560 && vpRec[3] == 1000 && vpRec[4] == 0 && vpRec[5] == 1 &&
                    scN == 1 && scRec[0] == 0 && scRec[1] == 300 && scRec[2] == 2560 && scRec[3] == 1300;
        ui::hSC(ctx, 1280, 800, 2560, 1600); bind &= scN == 2 && scRec[0] == 1280 && scRec[1] == 800 && scRec[2] == 2560 && scRec[3] == 1300;
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
        uiOk = geo && bind && mv;
        logf("UI test: panel geometry=%d convergence (left %.1f / right %.1f px) bind+scissor mapping=%d video/world/pause flat detection + flat render=%d -> %s",
             (int)geo, (pl[0] + pl[2]) * 0.5f, (pr[0] + pr[2]) * 0.5f, (int)bind, (int)mv, uiOk ? "PASS" : "FAIL");
        ui::theaterMode = 0; ui::hudDeg = 55; ui::pauseMask = 0x80; ui::flatNow = false; g_flatRender = false; g_flatCut = 0; g_cfg.xr = sx; g_ui = UiCfg{};
    }
    MH_Uninitialize(); logflush(); CloseHandle(g_log);
    return bad || en != MH_OK || dis != MH_OK || !restored || !tabTest || !simOk || !sweepOk || !cutOk || !occOk || !taaOk || !fastOk || !dlssOk || !uiOk ? 3 : 0;
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
        logflush();
    }
    return TRUE;
}
