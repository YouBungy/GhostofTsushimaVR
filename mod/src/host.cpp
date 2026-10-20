// GoTVRHost.exe — owns the OpenXR session OUTSIDE the game process (GoTVR 0.6).
// Why: GoTVR 0.4/0.5 ran the runtime in-process and crashed twice inside Virtual Desktop's in-process
// path (stack overflow on an NVIDIA 64 KB compile worker; use-after-free 0xDEDEDEDE in its D3D11
// compositor), in a process that also hosts the Steam overlay, Streamline and DLSS-G (docs/BUGLOG.md).
// Playbook ch09 (out-of-process helper) + ch07 (never submit zero layers; census the API layers).
//
// Usage: GoTVRHost.exe <gamePid>   (started by the game DLL; exits with the game)
#define WIN32_LEAN_AND_MEAN
#define WIDL_EXPLICIT_AGGREGATE_RETURNS
#include <windows.h>
#include <shellapi.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <vector>
#include <string>
#include <atomic>
#define XR_NO_PROTOTYPES
#define XR_USE_GRAPHICS_API_D3D12
#define XR_USE_PLATFORM_WIN32
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include "gotvr_shared.h"

typedef uint64_t u64; typedef uint32_t u32;
#define HOST_VERSION "GoTVRHost 0.14.3 (thumb / trigger touch for the VR fingers, five-option item wheel, shared block v14, item wheel + gesture indicator overlay, VR controllers: canonical controller per hand for Touch / Touch Plus / Index / Vive / WMR / HP / Pico / simple, haptics, pose prediction lead = measured pipeline lag, high GPU priority, pair-atomic xr=4, theatre screen, sharp UI layer: room-fixed HUD panel from the game, full-view cinematic UI, anamorphic 16:9 UI and flat frames of the square render)"

static FILE *g_log; static wchar_t g_dir[MAX_PATH];
static void logf(const char *fmt, ...) {
    if (!g_log) return;
    fprintf(g_log, "[%llu] ", (unsigned long long)GetTickCount64());
    va_list ap; va_start(ap, fmt); vfprintf(g_log, fmt, ap); va_end(ap); fputc('\n', g_log); fflush(g_log);
}

static GvShared *S; static HANDLE g_game; static u32 g_pid;
static void setState(GvHostState st, const char *msg) {
    S->hostState = st; if (msg) { strncpy(S->hostMsg, msg, sizeof S->hostMsg - 1); S->hostMsg[sizeof S->hostMsg - 1] = 0; }
    logf("state %d %s", (int)st, msg ? msg : "");
}
static bool gameAlive() { return WaitForSingleObject(g_game, 0) == WAIT_TIMEOUT; }

// ---------------- OpenXR ----------------
static HMODULE lib; static PFN_xrGetInstanceProcAddr gipa;
static XrInstance inst; static XrSystemId sys; static XrSession sess; static XrSpace local, viewSp;
#define XRFN(n) static PFN_##n n##_
XRFN(xrCreateInstance); XRFN(xrEnumerateApiLayerProperties); XRFN(xrDestroyInstance);
XRFN(xrGetSystem); XRFN(xrGetSystemProperties); XRFN(xrCreateSession); XRFN(xrDestroySession); XRFN(xrPollEvent); XRFN(xrBeginSession); XRFN(xrEndSession);
XRFN(xrWaitFrame); XRFN(xrBeginFrame); XRFN(xrEndFrame); XRFN(xrCreateReferenceSpace); XRFN(xrEnumerateSwapchainFormats); XRFN(xrCreateSwapchain);
XRFN(xrEnumerateSwapchainImages); XRFN(xrAcquireSwapchainImage); XRFN(xrWaitSwapchainImage); XRFN(xrReleaseSwapchainImage); XRFN(xrLocateViews);
XRFN(xrEnumerateViewConfigurationViews); XRFN(xrGetD3D12GraphicsRequirementsKHR); XRFN(xrResultToString); XRFN(xrGetInstanceProperties); XRFN(xrLocateSpace);
#define LOADFN(n) gipa(inst, #n, (PFN_xrVoidFunction *)&n##_)
static bool ok(XrResult r, const char *what) {
    if (XR_SUCCEEDED(r)) return true;
    char s[XR_MAX_RESULT_STRING_SIZE] = "?"; if (xrResultToString_ && inst) xrResultToString_(inst, r, s);
    // 0.7.1: rate-limited (a lost device made xrBeginFrame fail in a tight loop: 46 MB log); back off a little
    static const char *lastWhat = nullptr; static int lastR = 0; static u64 rep = 0;
    if (what == lastWhat && (int)r == lastR) { rep++; if (rep == 10 || rep % 10000 == 0) logf("XR FAIL %s -> %d %s (repeated %llu times)", what, (int)r, s, (unsigned long long)rep); Sleep(rep > 100 ? 10 : 0); return false; }
    lastWhat = what; lastR = (int)r; rep = 0;
    logf("XR FAIL %s -> %d %s", what, (int)r, s); return false;
}

#include "xrinput.h"

// ---------------- D3D12 ----------------
static ID3D12Device *dev; static ID3D12CommandQueue *queue; static LUID hostLuid;
static ID3D12CommandAllocator *alloc[3]; static ID3D12GraphicsCommandList *cl; static ID3D12Fence *ownFence; static HANDLE ownEv;
static u64 ownVal[3], ownNext = 1; static int ring;
static ID3D12Resource *tex[GV_TEXCOUNT]; static ID3D12Fence *gameFence; static LONG texGen; static u32 texW, texH; static DXGI_FORMAT texFmt;

struct Chain { XrSwapchain sc = XR_NULL_HANDLE; u32 w = 0, h = 0; ID3D12Resource *img[8] = {}; u32 n = 0; ID3D12DescriptorHeap *rtv = nullptr; u32 step = 0; DXGI_FORMAT fmt = DXGI_FORMAT_UNKNOWN; bool everReleased = false; bool broken = false; };
static Chain eyeSc[2]; static Chain uiSc;   // v7: the game UI on its own quad layer
static ID3D12Resource *uiTex[GV_UITEX]; static LONG uiGenOpen = 0, uiGenFailed = 0;
struct Shown { bool valid = false; u32 tex = 0; u32 frame = 0; u64 fence = 0; GvPose pose{}; float fov[4]{}; u32 flags = 0; };
static Shown shown[2];   // what each eye's swapchain currently shows (declared pose/fov)

static ID3D12GraphicsCommandList *beginCl() {
    ring = (ring + 1) % 3;
    if (ownFence->GetCompletedValue() < ownVal[ring]) { ownFence->SetEventOnCompletion(ownVal[ring], ownEv); WaitForSingleObject(ownEv, 500); }
    alloc[ring]->Reset(); cl->Reset(alloc[ring], nullptr); return cl;
}
static void submitCl() { cl->Close(); ID3D12CommandList *l[] = {cl}; queue->ExecuteCommandLists(1, l); ownVal[ring] = ownNext++; queue->Signal(ownFence, ownVal[ring]); }
static D3D12_RESOURCE_BARRIER barrier(ID3D12Resource *r, D3D12_RESOURCE_STATES a, D3D12_RESOURCE_STATES b) {
    D3D12_RESOURCE_BARRIER x{}; x.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; x.Transition.pResource = r;
    x.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES; x.Transition.StateBefore = a; x.Transition.StateAfter = b; return x;
}
static DXGI_FORMAT typeless(DXGI_FORMAT f) {
    switch (f) {
    case DXGI_FORMAT_R8G8B8A8_UNORM: case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: case DXGI_FORMAT_R8G8B8A8_TYPELESS: return DXGI_FORMAT_R8G8B8A8_TYPELESS;
    case DXGI_FORMAT_B8G8R8A8_UNORM: case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: case DXGI_FORMAT_B8G8R8A8_TYPELESS: return DXGI_FORMAT_B8G8R8A8_TYPELESS;
    case DXGI_FORMAT_R10G10B10A2_UNORM: case DXGI_FORMAT_R10G10B10A2_TYPELESS: return DXGI_FORMAT_R10G10B10A2_TYPELESS;
    case DXGI_FORMAT_R16G16B16A16_FLOAT: case DXGI_FORMAT_R16G16B16A16_TYPELESS: return DXGI_FORMAT_R16G16B16A16_TYPELESS;
    default: return f;
    }
}
static DXGI_FORMAT srgbOf(DXGI_FORMAT f) {
    if (f == DXGI_FORMAT_R8G8B8A8_UNORM) return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    if (f == DXGI_FORMAT_B8G8R8A8_UNORM) return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    return f;
}

static bool makeChain(Chain &c, u32 w, u32 h, DXGI_FORMAT want) {
    u32 nf = 0; if (!ok(xrEnumerateSwapchainFormats_(sess, 0, &nf, nullptr), "xrEnumerateSwapchainFormats")) return false;
    int64_t fm[64]; nf = nf > 64 ? 64 : nf; xrEnumerateSwapchainFormats_(sess, nf, &nf, fm);
    DXGI_FORMAT pick = DXGI_FORMAT_UNKNOWN;
    for (u32 i = 0; i < nf && pick == DXGI_FORMAT_UNKNOWN; i++) if ((DXGI_FORMAT)fm[i] == want) pick = want;
    for (u32 i = 0; i < nf && pick == DXGI_FORMAT_UNKNOWN; i++) if (typeless((DXGI_FORMAT)fm[i]) == typeless(want)) pick = (DXGI_FORMAT)fm[i];
    if (pick == DXGI_FORMAT_UNKNOWN) { logf("no swapchain format compatible with %d", (int)want); return false; }
    XrSwapchainCreateInfo ci{XR_TYPE_SWAPCHAIN_CREATE_INFO};
    ci.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
    ci.format = pick; ci.sampleCount = 1; ci.width = w; ci.height = h; ci.faceCount = 1; ci.arraySize = 1; ci.mipCount = 1;
    if (!ok(xrCreateSwapchain_(sess, &ci, &c.sc), "xrCreateSwapchain")) return false;
    c.w = w; c.h = h; c.fmt = pick;
    XrSwapchainImageD3D12KHR im[8]; for (auto &i : im) i = {XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR};
    u32 n = 0; xrEnumerateSwapchainImages_(c.sc, 0, &n, nullptr); n = n > 8 ? 8 : n;
    if (!ok(xrEnumerateSwapchainImages_(c.sc, n, &n, (XrSwapchainImageBaseHeader *)im), "xrEnumerateSwapchainImages")) return false;
    c.n = n; for (u32 i = 0; i < n; i++) c.img[i] = im[i].texture;
    D3D12_DESCRIPTOR_HEAP_DESC hd{}; hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; hd.NumDescriptors = n;
    if (FAILED(dev->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&c.rtv)))) return false;
    c.step = dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_CPU_DESCRIPTOR_HANDLE hh = c.rtv->GetCPUDescriptorHandleForHeapStart();
    for (u32 i = 0; i < n; i++) { D3D12_RENDER_TARGET_VIEW_DESC rv{}; rv.Format = pick; rv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D; dev->CreateRenderTargetView(c.img[i], &rv, {hh.ptr + (SIZE_T)i * c.step}); }
    logf("swapchain %ux%u format %d (wanted %d), %u images", w, h, (int)pick, (int)want, n);
    return true;
}
static void destroyChains() {
    if (uiSc.sc) { PFN_xrDestroySwapchain d = nullptr; gipa(inst, "xrDestroySwapchain", (PFN_xrVoidFunction *)&d); if (d) d(uiSc.sc); } if (uiSc.rtv) uiSc.rtv->Release(); uiSc = Chain{}; S->uiOk = 0;
    for (auto &t : uiTex) if (t) { t->Release(); t = nullptr; } uiGenOpen = 0;
    for (auto &c : eyeSc) { if (c.sc) { PFN_xrDestroySwapchain d = nullptr; gipa(inst, "xrDestroySwapchain", (PFN_xrVoidFunction *)&d); if (d) d(c.sc); } if (c.rtv) c.rtv->Release(); c = Chain{}; shown[&c - eyeSc] = Shown{}; } }

// Open the game's shared textures when it publishes a new generation. texGen is only committed on success
// (review #3); on any failure nothing from the old generation stays usable.
static LONG failedGen = 0;
static void dropShared() {
    S->imagesOk = 0;
    destroyChains();
    for (auto &t : tex) if (t) { t->Release(); t = nullptr; }
    texGen = 0;
}
static void openShared() {
    LONG g = S->texGen; if (g == texGen || g <= 0 || g == failedGen) return;
    dropShared();
    if (memcmp(&S->gameLuid, &hostLuid, sizeof(LUID)) != 0) {
        logf("game GPU %08lx:%08lx differs from the headset runtime's GPU %08lx:%08lx -> cannot share images", S->gameLuid.HighPart, S->gameLuid.LowPart, hostLuid.HighPart, hostLuid.LowPart);
        setState(GV_HOST_RUNNING, "game runs on a different GPU than the headset runtime: set the game to the same GPU"); failedGen = g; return;
    }
    bool good = true; wchar_t nm[96], sfx[32];
    for (int i = 0; i < GV_TEXCOUNT && good; i++) {
        _snwprintf(sfx, 32, L"_g%ld_tex%d", (long)g, i); sfx[31] = 0; gvName(nm, 96, g_pid, sfx); HANDLE h = nullptr;
        HRESULT hr = dev->OpenSharedHandleByName(nm, GENERIC_ALL, &h);
        if (SUCCEEDED(hr) && h) hr = dev->OpenSharedHandle(h, IID_PPV_ARGS(&tex[i]));
        if (FAILED(hr) || !tex[i]) { logf("open %ls failed 0x%08lx", nm, (unsigned long)hr); good = false; }
        if (h) CloseHandle(h);
    }
    if (good && !gameFence) {
        gvName(nm, 96, g_pid, L"_fence"); HANDLE h = nullptr;
        HRESULT hr = dev->OpenSharedHandleByName(nm, GENERIC_ALL, &h);
        if (SUCCEEDED(hr) && h) hr = dev->OpenSharedHandle(h, IID_PPV_ARGS(&gameFence));
        if (FAILED(hr) || !gameFence) { logf("open fence failed 0x%08lx", (unsigned long)hr); good = false; }
        if (h) CloseHandle(h);
    }
    if (good) {
        texW = S->texW; texH = S->texH; texFmt = (DXGI_FORMAT)S->texFmt;
        if (typeless(texFmt) != DXGI_FORMAT_R8G8B8A8_TYPELESS && typeless(texFmt) != DXGI_FORMAT_B8G8R8A8_TYPELESS)
            logf("note: game image format %d is not 8-bit; colours may look washed out (turn HDR off)", (int)texFmt);
        // swapchains follow the game's image size/format (sRGB view of 8-bit UNORM: the game writes gamma-encoded values)
        for (int e = 0; e < 2 && good; e++) good = makeChain(eyeSc[e], texW, texH, srgbOf(texFmt));
    }
    if (!good) { logf("shared generation %ld unusable -> black until the game publishes a new one", g); dropShared(); failedGen = g; return; }
    texGen = g; S->imagesOk = 1;
    logf("opened shared eye textures gen %ld: %ux%u format %d", g, texW, texH, (int)texFmt);
}

// v8: the UI images (own size and generation; the game builds them once it knows its UI texture)
static void openUi() {
    LONG g = S->uiGen; if (texGen <= 0 || g <= 0 || g == uiGenOpen || g == uiGenFailed) return;
    if (uiSc.sc) { PFN_xrDestroySwapchain d = nullptr; gipa(inst, "xrDestroySwapchain", (PFN_xrVoidFunction *)&d); if (d) d(uiSc.sc); } if (uiSc.rtv) uiSc.rtv->Release(); uiSc = Chain{}; S->uiOk = 0;
    for (auto &t : uiTex) if (t) { t->Release(); t = nullptr; }
    bool good = true; wchar_t nm[96], sfx[32];
    for (int i = 0; i < GV_UITEX && good; i++) {
        _snwprintf(sfx, 32, L"_u%ld_ui%d", (long)g, i); sfx[31] = 0; gvName(nm, 96, g_pid, sfx); HANDLE h = nullptr;
        HRESULT hr = dev->OpenSharedHandleByName(nm, GENERIC_ALL, &h);
        if (SUCCEEDED(hr) && h) hr = dev->OpenSharedHandle(h, IID_PPV_ARGS(&uiTex[i]));
        if (FAILED(hr) || !uiTex[i]) { logf("open %ls failed 0x%08lx", nm, (unsigned long)hr); good = false; }
        if (h) CloseHandle(h);
    }
    if (good) good = makeChain(uiSc, S->uiW, S->uiH, srgbOf((DXGI_FORMAT)S->uiFmt));
    if (!good) { logf("UI layer generation %ld unusable -> the game keeps the UI in the 3D image", g); for (auto &t : uiTex) if (t) { t->Release(); t = nullptr; } uiSc = Chain{}; uiGenFailed = g; return; }
    uiGenOpen = g; S->uiOk = 1; logf("UI layer: opened UI images gen %ld: %ux%u format %u", g, S->uiW, S->uiH, S->uiFmt);
}

// Acquire + wait. XR_TIMEOUT_EXPIRED is a success code, so loop on it; an image that was acquired but could
// not be waited must not be released (spec) -> the chain is marked broken (review #2).
static bool acquire(Chain &c, u32 &idx) {
    if (!c.sc || c.broken) return false;
    XrSwapchainImageAcquireInfo ai{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
    if (!ok(xrAcquireSwapchainImage_(c.sc, &ai, &idx), "xrAcquireSwapchainImage")) return false;
    XrSwapchainImageWaitInfo wi{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO}; wi.timeout = 100000000;
    for (int i = 0; i < 20; i++) {
        XrResult r = xrWaitSwapchainImage_(c.sc, &wi);
        if (r == XR_SUCCESS) return true;
        if (r != XR_TIMEOUT_EXPIRED) { ok(r, "xrWaitSwapchainImage"); break; }
    }
    logf("swapchain image wait failed -> chain disabled"); c.broken = true; return false;
}
static void release(Chain &c) { XrSwapchainImageReleaseInfo ri{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO}; if (ok(xrReleaseSwapchainImage_(c.sc, &ri), "xrReleaseSwapchainImage")) c.everReleased = true; }

// A placeholder chain so a projection layer can be submitted before the game publishes images.
static Chain blank[2];
static XrViewConfigurationView vcv[2];

static void layerCensus() {
    if (xrEnumerateApiLayerProperties_) {
        u32 n = 0; xrEnumerateApiLayerProperties_(0, &n, nullptr);
        XrApiLayerProperties p[32]; for (auto &x : p) x = {XR_TYPE_API_LAYER_PROPERTIES}; n = n > 32 ? 32 : n;
        if (XR_SUCCEEDED(xrEnumerateApiLayerProperties_(n, &n, p))) { logf("OpenXR API layers available: %u", n); for (u32 i = 0; i < n; i++) logf("   layer %s (%s)", p[i].layerName, p[i].description); }
    }
    const wchar_t *keys[] = {L"SOFTWARE\\Khronos\\OpenXR\\1\\ApiLayers\\Implicit"};
    HKEY roots[] = {HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER};
    for (HKEY root : roots) for (auto k : keys) {
        HKEY h; if (RegOpenKeyExW(root, k, 0, KEY_READ | KEY_WOW64_64KEY, &h) != ERROR_SUCCESS) continue;
        for (DWORD i = 0;; i++) {
            wchar_t name[512]; DWORD nl = 512, type, val = 0, vl = 4;
            if (RegEnumValueW(h, i, name, &nl, nullptr, &type, (BYTE *)&val, &vl) != ERROR_SUCCESS) break;
            logf("   implicit layer manifest (%s) %ls -> %s", root == HKEY_LOCAL_MACHINE ? "HKLM" : "HKCU", name, val == 0 ? "ENABLED" : "disabled");
        }
        RegCloseKey(h);
    }
}

static bool initXr() {
    wchar_t p[MAX_PATH]; swprintf(p, MAX_PATH, L"%ls\\openxr_loader.dll", g_dir);
    lib = LoadLibraryW(p); if (!lib) { setState(GV_HOST_FAILED, "openxr_loader.dll missing"); return false; }
    gipa = (PFN_xrGetInstanceProcAddr)GetProcAddress(lib, "xrGetInstanceProcAddr"); if (!gipa) return false;
    gipa(XR_NULL_HANDLE, "xrCreateInstance", (PFN_xrVoidFunction *)&xrCreateInstance_);
    gipa(XR_NULL_HANDLE, "xrEnumerateApiLayerProperties", (PFN_xrVoidFunction *)&xrEnumerateApiLayerProperties_);
    layerCensus();
    xin::pickExtensions(gipa);
    std::vector<const char *> ext{XR_KHR_D3D12_ENABLE_EXTENSION_NAME}; for (auto *e : xin::enabledExt) ext.push_back(e);
    XrInstanceCreateInfo ici{XR_TYPE_INSTANCE_CREATE_INFO};
    strcpy(ici.applicationInfo.applicationName, "GoTVR"); ici.applicationInfo.applicationVersion = 6;
    strcpy(ici.applicationInfo.engineName, "GoTVRHost"); ici.applicationInfo.apiVersion = XR_MAKE_VERSION(1, 0, 0);
    ici.enabledExtensionCount = (u32)ext.size(); ici.enabledExtensionNames = ext.data();
    for (int i = 0;; i++) {   // the runtime (Virtual Desktop / SteamVR) may be started after the game
        XrResult r = xrCreateInstance_(&ici, &inst);
        if (XR_SUCCEEDED(r)) break;
        if (i == 0) { ok(r, "xrCreateInstance"); setState(GV_HOST_NO_RUNTIME, "no OpenXR runtime yet: start Virtual Desktop (VDXR) or SteamVR; retrying every 3 s"); }
        if (!gameAlive()) return false;
        Sleep(3000);
    }
    LOADFN(xrDestroyInstance); LOADFN(xrGetSystem); LOADFN(xrGetSystemProperties); LOADFN(xrCreateSession); LOADFN(xrDestroySession); LOADFN(xrPollEvent);
    LOADFN(xrBeginSession); LOADFN(xrEndSession); LOADFN(xrWaitFrame); LOADFN(xrBeginFrame); LOADFN(xrEndFrame); LOADFN(xrCreateReferenceSpace);
    LOADFN(xrEnumerateSwapchainFormats); LOADFN(xrCreateSwapchain); LOADFN(xrEnumerateSwapchainImages); LOADFN(xrAcquireSwapchainImage);
    LOADFN(xrWaitSwapchainImage); LOADFN(xrReleaseSwapchainImage); LOADFN(xrLocateViews); LOADFN(xrEnumerateViewConfigurationViews);
    LOADFN(xrGetD3D12GraphicsRequirementsKHR); LOADFN(xrResultToString); LOADFN(xrGetInstanceProperties); LOADFN(xrLocateSpace);
    XrInstanceProperties ip{XR_TYPE_INSTANCE_PROPERTIES}; xrGetInstanceProperties_(inst, &ip);
    logf("runtime: %s %u.%u.%u", ip.runtimeName, XR_VERSION_MAJOR(ip.runtimeVersion), XR_VERSION_MINOR(ip.runtimeVersion), XR_VERSION_PATCH(ip.runtimeVersion));
    XrSystemGetInfo sgi{XR_TYPE_SYSTEM_GET_INFO}; sgi.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    for (int i = 0; !XR_SUCCEEDED(xrGetSystem_(inst, &sgi, &sys)); i++) {   // headset may connect later
        if (i == 0) setState(GV_HOST_NO_RUNTIME, "waiting for headset (xrGetSystem)");
        if (!gameAlive()) return false; Sleep(1000);
    }
    XrSystemProperties sp{XR_TYPE_SYSTEM_PROPERTIES}; xrGetSystemProperties_(inst, sys, &sp); logf("system: %s", sp.systemName);
    XrGraphicsRequirementsD3D12KHR req{XR_TYPE_GRAPHICS_REQUIREMENTS_D3D12_KHR};
    if (!ok(xrGetD3D12GraphicsRequirementsKHR_(inst, sys, &req), "xrGetD3D12GraphicsRequirementsKHR")) return false;
    // own device on the runtime's adapter (no game hooks/interposers in this process)
    IDXGIFactory4 *f = nullptr; if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&f)))) return false;
    IDXGIAdapter1 *ad = nullptr; if (FAILED(f->EnumAdapterByLuid(req.adapterLuid, IID_PPV_ARGS(&ad)))) { setState(GV_HOST_FAILED, "runtime adapter not found"); return false; }
    DXGI_ADAPTER_DESC1 add{}; ad->GetDesc1(&add); logf("adapter %ls LUID %08lx:%08lx", add.Description, req.adapterLuid.HighPart, req.adapterLuid.LowPart);
    hostLuid = req.adapterLuid;
    if (FAILED(D3D12CreateDevice(ad, req.minFeatureLevel < D3D_FEATURE_LEVEL_11_0 ? D3D_FEATURE_LEVEL_11_0 : req.minFeatureLevel, IID_PPV_ARGS(&dev)))) { setState(GV_HOST_FAILED, "D3D12CreateDevice failed"); return false; }
    // 0.13.1 (BUG-66): the game keeps the GPU busy; the per-frame copies into the XR swapchains must not wait behind it, or
    // xrEndFrame is late, the runtime halves the rate and turning the head judders with black edges (playbook ch19)
    D3D12_COMMAND_QUEUE_DESC qd{}; qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT; qd.Priority = D3D12_COMMAND_QUEUE_PRIORITY_HIGH;
    if (FAILED(dev->CreateCommandQueue(&qd, IID_PPV_ARGS(&queue)))) return false;
    for (auto &a : alloc) if (FAILED(dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&a)))) return false;
    if (FAILED(dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc[0], nullptr, IID_PPV_ARGS(&cl)))) return false; cl->Close();
    if (FAILED(dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&ownFence)))) return false; ownEv = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    XrGraphicsBindingD3D12KHR gb{XR_TYPE_GRAPHICS_BINDING_D3D12_KHR}; gb.device = dev; gb.queue = queue;
    XrSessionCreateInfo sci{XR_TYPE_SESSION_CREATE_INFO}; sci.next = &gb; sci.systemId = sys;
    if (!ok(xrCreateSession_(inst, &sci, &sess), "xrCreateSession")) { setState(GV_HOST_FAILED, "xrCreateSession failed"); return false; }
    XrReferenceSpaceCreateInfo rs{XR_TYPE_REFERENCE_SPACE_CREATE_INFO}; rs.poseInReferenceSpace.orientation.w = 1;
    rs.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL; if (!ok(xrCreateReferenceSpace_(sess, &rs, &local), "LOCAL space")) return false;
    rs.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW; if (!ok(xrCreateReferenceSpace_(sess, &rs, &viewSp), "VIEW space")) return false;
    u32 n = 0; for (auto &v : vcv) v = {XR_TYPE_VIEW_CONFIGURATION_VIEW};
    xrEnumerateViewConfigurationViews_(inst, sys, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &n, nullptr);
    if (n != 2 || !ok(xrEnumerateViewConfigurationViews_(inst, sys, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 2, &n, vcv), "views")) return false;
    logf("recommended eye %ux%u", vcv[0].recommendedImageRectWidth, vcv[0].recommendedImageRectHeight);
    // small placeholder chains (black / test pattern) so a projection layer exists from the first frame
    u32 bw = vcv[0].recommendedImageRectWidth / 2, bh = vcv[0].recommendedImageRectHeight / 2;
    for (int e = 0; e < 2; e++) if (!makeChain(blank[e], bw, bh, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)) return false;
    if (!xin::init()) logf("controllers: unavailable this session (the game keeps keyboard / gamepad)");
    setState(GV_HOST_SESSION, "session created, waiting for READY");
    return true;
}

static bool running = false, quit = false, focused = false;
static void invalidatePose() { gvWriteBegin(&S->poseSeq); S->poseValid = 0; gvWriteEnd(&S->poseSeq); }
static void pollEvents() {
    XrEventDataBuffer ev{XR_TYPE_EVENT_DATA_BUFFER};
    while (xrPollEvent_(inst, &ev) == XR_SUCCESS) {
        if (ev.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
            auto *s = (XrEventDataSessionStateChanged *)&ev; logf("session state -> %d", (int)s->state);
            focused = s->state == XR_SESSION_STATE_FOCUSED; if (!focused && xin::ready) xin::neutral();
            if (s->state == XR_SESSION_STATE_READY) {
                XrSessionBeginInfo bi{XR_TYPE_SESSION_BEGIN_INFO}; bi.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                if (ok(xrBeginSession_(sess, &bi), "xrBeginSession")) { running = true; setState(GV_HOST_RUNNING, "session running"); }
            } else if (s->state == XR_SESSION_STATE_STOPPING) { ok(xrEndSession_(sess), "xrEndSession"); running = false; invalidatePose(); setState(GV_HOST_SESSION, "session stopped"); }
            else if (s->state == XR_SESSION_STATE_EXITING || s->state == XR_SESSION_STATE_LOSS_PENDING) { running = false; quit = true; invalidatePose(); }
        } else if (ev.type == XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED) { xin::profDirty = true;
        } else if (ev.type == XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING) { running = false; quit = true; invalidatePose(); }
        ev = {XR_TYPE_EVENT_DATA_BUFFER};
    }
}

static void fillBlank(ID3D12GraphicsCommandList *c, Chain &ch, u32 idx, int e) {
    D3D12_CPU_DESCRIPTOR_HANDLE h = ch.rtv->GetCPUDescriptorHandleForHeapStart(); h.ptr += (SIZE_T)idx * ch.step;
    const float black[4] = {0, 0, 0, 1}, col[2][4] = {{0.6f, 0.05f, 0.05f, 1}, {0.05f, 0.05f, 0.6f, 1}}, white[4] = {1, 1, 1, 1};
    bool test = S->mode == GV_MODE_TEST;
    c->ClearRenderTargetView(h, test ? col[e] : black, 0, nullptr);
    if (test) { D3D12_RECT b{(LONG)ch.w / 2 - 20, (LONG)ch.h / 2 - 20, (LONG)ch.w / 2 + 20, (LONG)ch.h / 2 + 20}; c->ClearRenderTargetView(h, white, 1, &b); }
}

// ---------------- theatre screen (0.8.0, playbook ch04 HUD-002/HUD-003) ----------------
// Flat contexts (pre-rendered videos, menus without a 3D world, or forced with F10 in the game) are not stretched over
// the lens: the image goes on a world-locked, gravity-aligned 2D screen (quad layer) at its real aspect ratio, placed in
// front of where you look when it appears. F11 in the game re-places it.
static XrPosef qPose{}; static bool qPlaced = false; static u64 qLast = 0; static LONG qRecentre = 0;
static void placeScreen(const XrPosef &head) {
    const XrQuaternionf &q = head.orientation;
    float fx = -2.0f * (q.x * q.z + q.w * q.y), fz = -(1.0f - 2.0f * (q.x * q.x + q.y * q.y));   // head forward (-Z) projected on the floor
    float l = sqrtf(fx * fx + fz * fz); if (l < 1e-3f) { fx = 0; fz = -1; l = 1; } fx /= l; fz /= l;
    const float yaw = atan2f(-fx, -fz), d = S->theaterDist > 0.3f ? S->theaterDist : 3.0f;
    qPose.orientation = {0, sinf(yaw * 0.5f), 0, cosf(yaw * 0.5f)};
    qPose.position = {head.position.x + fx * d, head.position.y + S->theaterHeight, head.position.z + fz * d};
    qPlaced = true;
    logf("theatre screen placed %.2f m ahead (yaw %.1f deg), width %.2f m", d, yaw * 57.29578f, S->theaterWidth);
}
// v7 UI layer placement: head-locked HUD (VIEW space) or a world-locked, gravity-aligned panel (HUD-002) for the start screen
static XrPosef wPose{}; static bool wPlaced = false; static u64 wLast = 0; static LONG wRecentre = 0;
static void placeWorldPanel(const XrPosef &head, float d) {
    const XrQuaternionf &q = head.orientation;
    float fx = -2.0f * (q.x * q.z + q.w * q.y), fz = -(1.0f - 2.0f * (q.x * q.x + q.y * q.y));
    float l = sqrtf(fx * fx + fz * fz); if (l < 1e-3f) { fx = 0; fz = -1; l = 1; } fx /= l; fz /= l;
    const float yaw = atan2f(-fx, -fz), down = d * tanf(S->hudDown / 57.29578f);
    wPose.orientation = {0, sinf(yaw * 0.5f), 0, cosf(yaw * 0.5f)};
    wPose.position = {head.position.x + fx * d, head.position.y - down, head.position.z + fz * d};
    wPlaced = true; logf("UI layer: world-locked panel placed %.2f m ahead (yaw %.1f deg)", d, yaw * 57.29578f);
}
static XrRect2Di screenRect(u32 w, u32 h, bool crop) {   // the part of the game image that is the picture
    XrRect2Di r{{0, 0}, {(int32_t)w, (int32_t)h}};
    { const int32_t *k = S->content; if (k[2] > 0 && k[3] > 0 && k[0] + k[2] <= (int32_t)w && k[1] + k[3] <= (int32_t)h) { r = {{k[0], k[1]}, {k[2], k[3]}}; w = (u32)k[2]; h = (u32)k[3]; } }
    if (crop && w && h) {
        if (w * 9 < h * 16) { int32_t hh = (int32_t)(w * 9 / 16); r.offset.y += ((int32_t)h - hh) / 2; r.extent.height = hh; }       // letterboxed (16:10 frame)
        else if (w * 9 > h * 16) { int32_t ww = (int32_t)(h * 16 / 9); r.offset.x += ((int32_t)w - ww) / 2; r.extent.width = ww; }  // pillarboxed (ultrawide)
    }
    return r;
}

// v11 (BUG-69) pose lead: every published pose is remembered with the time it was predicted for; when a new stereo pair is
// first shown, its pose is looked up and (this frame's display time - that time) is how late the pipeline delivered it. The
// lead integrates that lag, so the game renders for the moment its image is actually on the display (playbook ch09
// #runtime-owns-pacing / XR-005: one wait, one prediction, declare what you rendered; the declared pose stays the rendered one).
#include "xroverlay.h"

struct LeadRec { XrTime t; GvPose e0; };
static LeadRec leadRing[128]; static u32 leadPos = 0; static double leadNs = 0, lagEma = 0; static u64 lagN = 0; static u32 lastLeadFrame = 0xFFFFFFFFu;
static double leadLimitNs() { return 45e6; }
static void leadNote(XrTime now, const Shown &first) {   // a new pair is on the display this frame
    if (!first.valid || (first.flags & GV_SLOT_FLAT) || first.frame == lastLeadFrame) return;
    lastLeadFrame = first.frame;
    for (u32 k = 0; k < 128; k++) {
        const LeadRec &r = leadRing[(leadPos - 1 - k) & 127];
        if (r.t && !memcmp(&r.e0, &first.pose, sizeof(GvPose))) {
            const double lag = (double)(now - r.t); lagN++;
            lagEma = lagN == 1 ? lag : lagEma + (lag - lagEma) * 0.05;
            const float cfg = S->poseLeadCfgMs;
            if (cfg < 0) { leadNs += lag * 0.1; if (leadNs < 0) leadNs = 0; if (leadNs > leadLimitNs()) leadNs = leadLimitNs(); }
            else leadNs = (double)cfg * 1e6;
            S->poseLeadUs = (LONG)(leadNs / 1000.0); S->poseLagUs = (LONG)(lagEma / 1000.0);
            return;
        }
    }
}
static void frame() {
    XrFrameState fs{XR_TYPE_FRAME_STATE}; XrFrameWaitInfo fw{XR_TYPE_FRAME_WAIT_INFO};
    if (!ok(xrWaitFrame_(sess, &fw, &fs), "xrWaitFrame")) { Sleep(5); return; }
    // publish the pose packet for the game: predicted for this display time + the pipeline lead (v11)
    if (S->poseLeadCfgMs == 0.0f) leadNs = 0; else if (S->poseLeadCfgMs > 0) leadNs = (double)S->poseLeadCfgMs * 1e6;
    const XrTime target = fs.predictedDisplayTime + (XrTime)leadNs;
    XrViewLocateInfo li{XR_TYPE_VIEW_LOCATE_INFO}; li.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO; li.displayTime = target; li.space = local;
    XrViewState vs{XR_TYPE_VIEW_STATE}; XrView v[2] = {{XR_TYPE_VIEW}, {XR_TYPE_VIEW}}; u32 nv = 0;
    XrSpaceLocation hl{XR_TYPE_SPACE_LOCATION};
    bool views = XR_SUCCEEDED(xrLocateViews_(sess, &li, &vs, 2, &nv, v)) && nv == 2 && (vs.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT);
    bool headOk = XR_SUCCEEDED(xrLocateSpace_(viewSp, local, target, &hl)) && (hl.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT);
    if (views && headOk) {
        gvWriteBegin(&S->poseSeq);
        S->displayTime = target;
        auto put = [](GvPose &d, const XrPosef &p) { d.q[0] = p.orientation.x; d.q[1] = p.orientation.y; d.q[2] = p.orientation.z; d.q[3] = p.orientation.w; d.p[0] = p.position.x; d.p[1] = p.position.y; d.p[2] = p.position.z; };
        put(S->head, hl.pose);
        for (int e = 0; e < 2; e++) { put(S->eye[e], v[e].pose); S->fov[e][0] = v[e].fov.angleLeft; S->fov[e][1] = v[e].fov.angleRight; S->fov[e][2] = v[e].fov.angleUp; S->fov[e][3] = v[e].fov.angleDown; }
        S->poseValid = 1;
        gvWriteEnd(&S->poseSeq);
        LeadRec &r = leadRing[leadPos++ & 127]; r.t = target; r.e0 = S->eye[0];
    }
    xin::frame(target, focused && views && headOk);   // v12: controllers, located for the same time as the head
    S->hostBeat = (LONG64)GetTickCount64();

    XrFrameBeginInfo fb{XR_TYPE_FRAME_BEGIN_INFO}; if (!ok(xrBeginFrame_(sess, &fb), "xrBeginFrame")) return;
    // From here every path reaches xrEndFrame with one projection layer (playbook ch07: never zero layers).
    openShared(); openUi();
    bool gameFresh = (u64)GetTickCount64() - (u64)S->gameBeat < 1500;
    if (texGen > 0 && (eyeSc[0].broken || eyeSc[1].broken)) { logf("eye swapchain broken -> rebuilding"); dropShared(); failedGen = 0; openShared(); }
    bool useGame = texGen > 0 && eyeSc[0].sc && eyeSc[1].sc && !eyeSc[0].broken && !eyeSc[1].broken && S->mode != GV_MODE_TEST;
    Chain *ch[2]; u32 idx[2] = {0, 0}; bool acq[2];
    for (int e = 0; e < 2; e++) { ch[e] = useGame ? &eyeSc[e] : &blank[e]; acq[e] = acquire(*ch[e], idx[e]); }
    // Candidates per eye: completed history entries of this generation (CPU check: never GPU-wait on the game).
    Shown cand[2][3]; u64 done = gameFence ? gameFence->GetCompletedValue() : 0;
    for (int e = 0; e < 2; e++) for (int b = 0; b < 3; b++) {
        GvSlot sl{}; GvSlot &src = S->slot[e][b]; Shown &o = cand[e][b]; o = Shown{};
        bool got = gvRead(&src.seq, [&] { sl.tex = src.tex; sl.gen = src.gen; sl.fence = src.fence; sl.frame = src.frame; sl.pose = src.pose; memcpy(sl.fov, src.fov, 16); sl.flags = src.flags; });
        if (got && sl.fence && sl.fence <= done && (LONG)sl.gen == texGen && sl.tex < GV_TEXCOUNT && tex[sl.tex]) { o.valid = true; o.tex = sl.tex; o.frame = sl.frame; o.fence = sl.fence; o.pose = sl.pose; memcpy(o.fov, sl.fov, 16); o.flags = sl.flags; }
    }
    // xr=4 synchronized stereo (0.7, BUG-25): both eyes of a pair carry the same frame tag; show only the newest
    // COMPLETE pair, never pair k+1's left eye with pair k's right eye (that mix is what AFR looks like).
    Shown pick[2] = {shown[0], shown[1]};
    if (S->mode == 4) {
        int bl = -1, br = -1;
        for (int b = 0; b < 3; b++) for (int b2 = 0; b2 < 3; b2++)
            if (cand[0][b].valid && cand[1][b2].valid && cand[0][b].frame == cand[1][b2].frame && (bl < 0 || (int)(cand[0][b].frame - cand[0][bl].frame) > 0)) { bl = b; br = b2; }
        if (bl >= 0 && (!shown[0].valid || !shown[1].valid || shown[0].frame != shown[1].frame || (int)(cand[0][bl].frame - shown[0].frame) >= 0)) {
            if (!shown[0].valid || cand[0][bl].frame != shown[0].frame || cand[1][br].frame != shown[1].frame) S->hostPairs++;
            pick[0] = cand[0][bl]; pick[1] = cand[1][br];
            leadNote(fs.predictedDisplayTime, pick[0]);
        }
    } else for (int e = 0; e < 2; e++) for (int b = 0; b < 3; b++) if (cand[e][b].valid && (!pick[e].valid || cand[e][b].fence > pick[e].fence)) pick[e] = cand[e][b];
    // v7 UI layer: newest completed UI image of this generation, from (about) the frame the eyes show
    Shown uiPick{}; u32 uiFlags = 0;
    if (useGame && uiSc.sc && !uiSc.broken) for (int b = 0; b < GV_UITEX; b++) {
        GvSlot sl{}; GvSlot &src = S->ui[b];
        bool got = gvRead(&src.seq, [&] { sl.tex = src.tex; sl.gen = src.gen; sl.fence = src.fence; sl.frame = src.frame; sl.flags = src.flags; });
        if (got && sl.fence && sl.fence <= done && (LONG)sl.gen == uiGenOpen && sl.tex < GV_UITEX && uiTex[sl.tex] && (!uiPick.valid || (int)(sl.frame - uiPick.frame) > 0)) { uiPick.valid = true; uiPick.tex = sl.tex; uiPick.frame = sl.frame; uiPick.fence = sl.fence; uiFlags = sl.flags; }
    }
    const bool uiFresh = uiPick.valid && shown[0].valid && (int)(uiPick.frame - pick[0].frame) >= -3 && (uiFlags & (GV_UI_HEAD | GV_UI_WORLD | GV_UI_FULL));
    u32 uiIdx = 0; const bool uiAcq = uiFresh && acquire(uiSc, uiIdx);
    const u64 ovNow = GetTickCount64(); const bool ovNeed = useGame && ov::wantDraw(ovNow); u32 ovIdx = 0; const bool ovAcq = ovNeed && acquire(ov::sc, ovIdx);   // v13 overlay
    auto *c = beginCl();
    if (ovAcq) ov::record(c, ov::sc.img[ovIdx], ring, ovNow);
    if (uiAcq) {
        D3D12_RESOURCE_BARRIER br = barrier(uiSc.img[uiIdx], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST); c->ResourceBarrier(1, &br);
        D3D12_TEXTURE_COPY_LOCATION dst{uiSc.img[uiIdx], D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, {}}, src{uiTex[uiPick.tex], D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, {}};
        c->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
        br = barrier(uiSc.img[uiIdx], D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET); c->ResourceBarrier(1, &br);
    }
    for (int e = 0; e < 2; e++) {
        if (!acq[e]) continue;
        Chain &h = *ch[e];
        if (useGame) {
            Shown best = pick[e];
            if (best.valid && gameFresh && tex[best.tex]) {
                D3D12_RESOURCE_BARRIER br = barrier(h.img[idx[e]], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST); c->ResourceBarrier(1, &br);
                D3D12_TEXTURE_COPY_LOCATION dst{h.img[idx[e]], D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, {}}, src{tex[best.tex], D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, {}};
                c->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
                br = barrier(h.img[idx[e]], D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET); c->ResourceBarrier(1, &br);
                if (best.fence != shown[e].fence || !shown[e].valid) S->hostCopied++; else S->hostReused++;
                shown[e] = best;
            } else { fillBlank(c, h, idx[e], e); shown[e].valid = false; }
        } else fillBlank(c, h, idx[e], e);
    }
    submitCl();                                                       // work is on the queue BEFORE release (review #1)
    for (int e = 0; e < 2; e++) if (acq[e]) release(*ch[e]);
    if (uiAcq) release(uiSc);
    if (ovAcq) { release(ov::sc); ov::sigDrawn = ov::sigOf(ovNow); ov::haveImage = true; }
    XrCompositionLayerProjectionView pv[2]; bool layerOk = true;
    for (int e = 0; e < 2; e++) {
        pv[e] = {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
        Chain *use = ch[e];
        bool imageIsGame = useGame && acq[e] && shown[e].valid;
        if (!use->everReleased) { use = &blank[e]; imageIsGame = false; }        // never reference an unreleased chain (review #4)
        if (!use->everReleased) { layerOk = false; break; }
        if (!acq[e] && useGame && use == ch[e]) imageIsGame = shown[e].valid;   // last released game image is still what it shows
        if (imageIsGame && !(shown[e].flags & GV_SLOT_FLAT)) {
            const GvPose &p = shown[e].pose;   // declare exactly what the image was rendered with
            pv[e].pose.orientation = {p.q[0], p.q[1], p.q[2], p.q[3]}; pv[e].pose.position = {p.p[0], p.p[1], p.p[2]};
            pv[e].fov = {shown[e].fov[0], shown[e].fov[1], shown[e].fov[2], shown[e].fov[3]};
        } else if (views) { pv[e].pose = v[e].pose; pv[e].fov = v[e].fov; }
        else { pv[e].pose.orientation.w = 1; pv[e].fov = {-0.9f, 0.9f, 0.9f, -0.9f}; }
        pv[e].subImage.swapchain = use->sc; pv[e].subImage.imageRect = {{0, 0}, {(int32_t)use->w, (int32_t)use->h}};
        if (imageIsGame && use == &eyeSc[e]) {   // v8: only the picture, not the game's letterbox bars, spans the eye frustum
            const int32_t *k = S->content;
            if (k[2] > 0 && k[3] > 0 && k[0] >= 0 && k[1] >= 0 && k[0] + k[2] <= (int32_t)use->w && k[1] + k[3] <= (int32_t)use->h) pv[e].subImage.imageRect = {{k[0], k[1]}, {k[2], k[3]}};
        }
    }
    XrCompositionLayerProjection pl{XR_TYPE_COMPOSITION_LAYER_PROJECTION}; pl.space = local; pl.viewCount = 2; pl.views = pv;
    const XrCompositionLayerBaseHeader *layers[8] = {(XrCompositionLayerBaseHeader *)&pl, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr}; u32 nLayers = layerOk ? 1 : 0;
    // theatre: the left image of a flat-context pair goes on the screen; the projection layer is dropped (black room)
    // unless theaterBackdrop = 1. The eye-0 chain holds exactly that image (copied above this frame, or still shown).
    XrCompositionLayerQuad ql{XR_TYPE_COMPOSITION_LAYER_QUAD};
    const bool theatre = useGame && gameFresh && shown[0].valid && (shown[0].flags & GV_SLOT_THEATER) && eyeSc[0].everReleased && headOk;
    if (theatre) {
        const u64 now = GetTickCount64(); const LONG rc = S->theaterRecenter;
        if (!qPlaced || now - qLast > 3000 || rc != qRecentre) { qRecentre = rc; placeScreen(hl.pose); }
        qLast = now;
        XrRect2Di r = screenRect(eyeSc[0].w, eyeSc[0].h, (shown[0].flags & GV_SLOT_CROP169) != 0);
        const float wm = S->theaterWidth > 0.2f ? S->theaterWidth : 3.2f;
        ql.layerFlags = 0; ql.space = local; ql.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
        ql.subImage.swapchain = eyeSc[0].sc; ql.subImage.imageRect = r; ql.subImage.imageArrayIndex = 0;
        // v10 (BUG-64): a flat frame of the square render is anamorphic (the game drew it for dispAspect) -> shown at that aspect
        const float da = S->dispAspect; const bool ana = da > 0.5f && !(shown[0].flags & GV_SLOT_CROP169) && (shown[0].flags & GV_SLOT_FLAT);   // a stereo image has the eye's own square projection
        ql.pose = qPose; ql.size = {wm, ana ? wm / da : wm * (float)r.extent.height / (float)(r.extent.width ? r.extent.width : 1)};
        if (S->theaterBackdrop && layerOk && !(shown[0].flags & GV_SLOT_FLAT) && !(shown[1].flags & GV_SLOT_FLAT)) {   // a flat frame has no eye pose: never a projection
            layers[1] = (XrCompositionLayerBaseHeader *)&ql; nLayers = 2; }
        else { layers[0] = (XrCompositionLayerBaseHeader *)&ql; nLayers = 1; }
        S->hostTheater++;
    }
    XrCompositionLayerQuad uq{XR_TYPE_COMPOSITION_LAYER_QUAD}, uq2{XR_TYPE_COMPOSITION_LAYER_QUAD};
    if (!theatre && uiAcq && uiSc.everReleased && layerOk && headOk) {   // the HUD at the UI's own resolution (sharp: sampled once, by the runtime)
        const float d = S->hudDist > 0.3f ? S->hudDist : 1.6f, deg = S->hudDeg > 5 ? S->hudDeg : 55.0f;
        const float da = S->dispAspect, uiInvA = da > 0.5f ? 1.0f / da : (float)uiSc.h / (float)(uiSc.w ? uiSc.w : 1);   // v10: anamorphic UI image
        float w = 2.0f * d * tanf(deg * 0.5f / 57.29578f), h = w * uiInvA;
        uq.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT | (S->uiPremul ? 0 : XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT);
        uq.eyeVisibility = XR_EYE_VISIBILITY_BOTH; uq.subImage.swapchain = uiSc.sc; uq.subImage.imageRect = {{0, 0}, {(int32_t)uiSc.w, (int32_t)uiSc.h}}; uq.size = {w, h};
        const char *how = "head-locked";
        if (uiFlags & GV_UI_FULL) {
            // v9 cinematic: the UI image covers each eye's whole frustum, exactly where the monitor shows it (letterbox bars at
            // the top/bottom edge of the view, fades over everything). One quad per eye on the frustum its eye image used.
            how = "FULL VIEW (cinematic)"; bool ok2 = true; XrCompositionLayerQuad *qq[2] = {&uq, &uq2};
            for (int e = 0; e < 2 && ok2; e++) {
                const bool use = shown[e].valid && !(shown[e].flags & GV_SLOT_FLAT);
                const GvPose &p = shown[e].pose; const float *fv = shown[e].fov;
                XrPosef ep; float a[4];
                if (use) { ep.orientation = {p.q[0], p.q[1], p.q[2], p.q[3]}; ep.position = {p.p[0], p.p[1], p.p[2]}; for (int i = 0; i < 4; i++) a[i] = fv[i]; }
                else if (views) { ep = v[e].pose; a[0] = v[e].fov.angleLeft; a[1] = v[e].fov.angleRight; a[2] = v[e].fov.angleUp; a[3] = v[e].fov.angleDown; }
                else { ok2 = false; break; }
                const float tl = tanf(a[0]), tr = tanf(a[1]), tu = tanf(a[2]), td = tanf(a[3]), dd = 1.0f;
                const float cx = 0.5f * (tl + tr) * dd, cy = 0.5f * (tu + td) * dd;   // centre of the frustum rectangle at 1 m, eye space
                const XrQuaternionf &q = ep.orientation;   // eye-space offset -> LOCAL: q * (cx, cy, -dd)
                const float vx = cx, vy = cy, vz = -dd, tx = 2 * (q.y * vz - q.z * vy), ty = 2 * (q.z * vx - q.x * vz), tz = 2 * (q.x * vy - q.y * vx);
                XrCompositionLayerQuad &o = *qq[e]; o = uq;
                o.space = local; o.eyeVisibility = e == 0 ? XR_EYE_VISIBILITY_LEFT : XR_EYE_VISIBILITY_RIGHT;
                o.pose.orientation = q;
                o.pose.position = {ep.position.x + vx + q.w * tx + (q.y * tz - q.z * ty), ep.position.y + vy + q.w * ty + (q.z * tx - q.x * tz), ep.position.z + vz + q.w * tz + (q.x * ty - q.y * tx)};
                o.size = {(tr - tl) * dd, (tu - td) * dd};
                if (e == 0) { w = o.size.width; h = o.size.height; }
            }
            if (ok2) { layers[nLayers++] = (XrCompositionLayerBaseHeader *)&uq; layers[nLayers++] = (XrCompositionLayerBaseHeader *)&uq2; S->hostUi++; }
        } else {
            if (uiFlags & GV_UI_WORLD) {
                float pp[3] = {0, 0, 0}, yaw = 0, pw = 0; u32 pok = 0;
                gvRead(&S->uiPoseSeq, [&] { pp[0] = S->uiPos[0]; pp[1] = S->uiPos[1]; pp[2] = S->uiPos[2]; yaw = S->uiYaw; pw = S->uiWidth; pok = S->uiPoseOk; });
                if (pok && pw > 0.05f) {   // v9: the game's room-fixed lazy-follow panel (HUD-002)
                    how = "ROOM-FIXED (lazy follow)"; uq.space = local; uq.pose.orientation = {0, sinf(yaw * 0.5f), 0, cosf(yaw * 0.5f)}; uq.pose.position = {pp[0], pp[1], pp[2]};
                    w = pw; h = w * uiInvA; uq.size = {w, h};
                } else {
                    how = "WORLD-LOCKED"; const u64 now = GetTickCount64(); const LONG rc = S->theaterRecenter;
                    if (!wPlaced || now - wLast > 3000 || rc != wRecentre) { wRecentre = rc; placeWorldPanel(hl.pose, d); }
                    wLast = now; uq.space = local; uq.pose = wPose;
                }
            } else { uq.space = viewSp; uq.pose.orientation.w = 1; uq.pose.position = {0, -d * tanf(S->hudDown / 57.29578f), -d}; }
            layers[nLayers++] = (XrCompositionLayerBaseHeader *)&uq; S->hostUi++;
        }
        static u32 wasUi = 0; static float wasDa = -1; if (uiFlags != wasUi || da != wasDa) { logf("UI layer: %s quad %.2f x %.2f m (%ux%u%s)", how, w, h, uiSc.w, uiSc.h, da > 0.5f ? ", anamorphic: shown at the game's layout aspect" : ""); wasUi = uiFlags; wasDa = da; }
    }
    // v13 overlay: the item wheel above the hand that holds it open, the head-gesture indicator low in the view (both premultiplied)
    XrCompositionLayerQuad wq{XR_TYPE_COMPOSITION_LAYER_QUAD}, gq{XR_TYPE_COMPOSITION_LAYER_QUAD};
    if (!theatre && useGame && ov::haveImage && ov::sc.everReleased && headOk && layerOk && nLayers + 2 <= 8) {
        if (ov::cur.wheelOpen) {
            GvHand hd{}; u32 act = 0; gvRead(&S->inSeq, [&] { hd = S->hand[ov::cur.hand & 1]; act = S->inActive; });
            if (act && (hd.poseValid & 1)) {
                const float pos[3] = {hd.grip.p[0], hd.grip.p[1] + 0.13f, hd.grip.p[2]}, eye[3] = {hl.pose.position.x, hl.pose.position.y, hl.pose.position.z};
                wq.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT; wq.space = local; wq.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
                wq.subImage.swapchain = ov::sc.sc; wq.subImage.imageRect = {{0, 0}, {(int32_t)ov::HALF, (int32_t)ov::H}};
                wq.pose.position = {pos[0], pos[1], pos[2]}; wq.pose.orientation = ov::faceTo(pos, eye); wq.size = {0.26f, 0.26f};
                layers[nLayers++] = (XrCompositionLayerBaseHeader *)&wq;
            }
        }
        if (ov::cur.dbg && (ov::cur.atHead || ovNow < ov::flashUntil[0] || ovNow < ov::flashUntil[1])) {   // only while a hand is at the head or a gesture just fired
            gq.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT; gq.space = viewSp; gq.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
            gq.subImage.swapchain = ov::sc.sc; gq.subImage.imageRect = {{(int32_t)ov::HALF, 0}, {(int32_t)ov::HALF, (int32_t)ov::H}};
            gq.pose.orientation.w = 1; gq.pose.position = {0.0f, -0.24f, -0.8f}; gq.size = {0.22f, 0.22f};
            layers[nLayers++] = (XrCompositionLayerBaseHeader *)&gq;
        }
    }
    {   static bool was = false; static u32 wasFlags = 0;
        if (theatre != was || (theatre && shown[0].flags != wasFlags)) {
            if (theatre) logf("theatre: ON (%s%s, image %dx%d at %d,%d of %ux%u, %u layer(s), display aspect %.3f)", (shown[0].flags & GV_SLOT_CROP169) ? "video, cropped to 16:9" : "full frame", (shown[0].flags & GV_SLOT_FLAT) ? ", flat-rendered" : ", stereo image",
                              ql.subImage.imageRect.extent.width, ql.subImage.imageRect.extent.height, ql.subImage.imageRect.offset.x, ql.subImage.imageRect.offset.y, eyeSc[0].w, eyeSc[0].h, nLayers, ql.size.width / (ql.size.height > 1e-4f ? ql.size.height : 1.0f));
            else logf("theatre: off (stereo projection)");
            was = theatre; wasFlags = shown[0].flags;
        }
    }
    XrFrameEndInfo fe{XR_TYPE_FRAME_END_INFO}; fe.displayTime = fs.predictedDisplayTime; fe.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    fe.layerCount = nLayers; fe.layers = layers;   // 0 only if not even a placeholder was ever released (first-frame failure)
    ok(xrEndFrame_(sess, &fe), "xrEndFrame");
    u64 nf = ++S->hostFrames;
    if (nf % 900 == 1) logf("frame %llu: shouldRender=%d useGame=%d gameFresh=%d copied=%lld reused=%lld published=%lld mismatch=%lld eyeFrames=%u/%u | pose lead %.1f ms (new pairs reach the display %.1f ms after their prediction, %llu measured)",
                            (unsigned long long)nf, (int)fs.shouldRender, (int)useGame, (int)gameFresh, (long long)S->hostCopied, (long long)S->hostReused,
                            (long long)S->gamePublished, (long long)S->gameMismatch, shown[0].frame, shown[1].frame, leadNs / 1e6, lagEma / 1e6, (unsigned long long)lagN);
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR cmd, int) {
    int argc = 0; wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    GetModuleFileNameW(nullptr, g_dir, MAX_PATH); if (wchar_t *s = wcsrchr(g_dir, L'\\')) *s = 0;
    wchar_t lp[MAX_PATH], pp[MAX_PATH]; swprintf(lp, MAX_PATH, L"%ls\\host.log", g_dir); swprintf(pp, MAX_PATH, L"%ls\\host.prev.log", g_dir);
    MoveFileExW(lp, pp, MOVEFILE_REPLACE_EXISTING); g_log = _wfopen(lp, L"w");
    logf("%s built %s %s", HOST_VERSION, __DATE__, __TIME__); (void)cmd;
    if (argc < 2) {
        logf("usage: GoTVRHost.exe <gamePid> (started by the game)");
        MessageBoxW(nullptr, L"GoTVRHost.exe is started automatically by the GoTVR mod.\n\n"
                             L"Put it in the game's GoTVR folder (next to openxr_loader.dll), set xr= in GoTVR\\gotvr.ini, "
                             L"start Virtual Desktop (VDXR) or SteamVR, then start the game normally from Steam.",
                    L"GoTVR", MB_OK | MB_ICONINFORMATION);
        return 1;
    }
    g_pid = (u32)wcstoul(argv[1], nullptr, 10);
    g_game = OpenProcess(SYNCHRONIZE, FALSE, g_pid); if (!g_game) { logf("game pid %u not found", g_pid); return 1; }
    wchar_t nm[96]; gvName(nm, 96, g_pid, nullptr);
    HANDLE m = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, nm); if (!m) { logf("shared block %ls missing", nm); return 1; }
    S = (GvShared *)MapViewOfFile(m, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(GvShared));
    if (!S || S->magic != GV_MAGIC || S->version != GV_VERSION) { logf("shared block version mismatch (game DLL and host must be the same build)"); return 1; }
    S->hostPid = GetCurrentProcessId(); logf("attached to game pid %u, mode %u", g_pid, S->mode);
    {   // v11 (BUG-69): the host's copies and the runtime's submission must not queue behind a GPU-bound game (playbook ch19
        // host starvation). WDDM GPU scheduling class: HIGH (4), else ABOVE_NORMAL (3).
        typedef LONG(APIENTRY * FnPrio)(HANDLE, int); S->hostGpuPrio = -1;
        if (FnPrio f = (FnPrio)GetProcAddress(GetModuleHandleW(L"gdi32.dll") ? GetModuleHandleW(L"gdi32.dll") : LoadLibraryW(L"gdi32.dll"), "D3DKMTSetProcessSchedulingPriorityClass")) {
            LONG st = f(GetCurrentProcess(), 4); if (st == 0) S->hostGpuPrio = 4; else { LONG st2 = f(GetCurrentProcess(), 3); if (st2 == 0) S->hostGpuPrio = 3; logf("GPU priority HIGH refused (0x%08lX)", (unsigned long)st); }
        }
        logf("host GPU scheduling priority: %s", S->hostGpuPrio == 4 ? "HIGH" : S->hostGpuPrio == 3 ? "ABOVE NORMAL" : "unchanged (not available)");
        logf("pose prediction lead: %s", S->poseLeadCfgMs < 0 ? "automatic (measured pipeline lag)" : S->poseLeadCfgMs == 0 ? "off" : "fixed");
    }
    if (!initXr()) { if (S->hostState != GV_HOST_NO_RUNTIME && S->hostState != GV_HOST_FAILED) setState(GV_HOST_FAILED, "init failed"); logf("exit: init failed (%s)", S->hostMsg); return 2; }
    while (!quit && gameAlive()) {
        pollEvents();
        if (!running) { Sleep(20); S->hostBeat = (LONG64)GetTickCount64(); continue; }
        frame();
    }
    logf("exit (game alive=%d): orderly OpenXR teardown", (int)gameAlive());
    invalidatePose(); S->hostState = GV_HOST_EXITED;
    // BUG-12: returning from main with the session alive let Virtual Desktop's compositor thread run while its
    // DLLs unloaded (CFG fail-fast in d3d11 at every game exit). Request exit, wait for STOPPING, end, destroy.
    if (sess) {
        PFN_xrRequestExitSession reqExit = nullptr; gipa(inst, "xrRequestExitSession", (PFN_xrVoidFunction *)&reqExit);
        if (running && reqExit && XR_SUCCEEDED(reqExit(sess))) {
            for (int i = 0; i < 100 && running; i++) {   // up to ~2 s; keep the frame loop alive so the runtime can transition
                pollEvents(); if (!running) break;
                XrFrameState fs{XR_TYPE_FRAME_STATE}; XrFrameWaitInfo fw{XR_TYPE_FRAME_WAIT_INFO}; XrFrameBeginInfo fb{XR_TYPE_FRAME_BEGIN_INFO};
                if (XR_SUCCEEDED(xrWaitFrame_(sess, &fw, &fs)) && XR_SUCCEEDED(xrBeginFrame_(sess, &fb))) {
                    XrFrameEndInfo fe{XR_TYPE_FRAME_END_INFO}; fe.displayTime = fs.predictedDisplayTime; fe.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
                    xrEndFrame_(sess, &fe);
                } else Sleep(20);
            }
        }
        if (running) { xrEndSession_(sess); running = false; }
        if (queue && ownFence) { u64 v = ownNext++; queue->Signal(ownFence, v); if (ownFence->GetCompletedValue() < v) { ownFence->SetEventOnCompletion(v, ownEv); WaitForSingleObject(ownEv, 1000); } }
        destroyChains();
        PFN_xrDestroySwapchain dsc = nullptr; gipa(inst, "xrDestroySwapchain", (PFN_xrVoidFunction *)&dsc);
        for (auto &c : blank) if (c.sc && dsc) { dsc(c.sc); c.sc = XR_NULL_HANDLE; }
        PFN_xrDestroySpace dsp = nullptr; gipa(inst, "xrDestroySpace", (PFN_xrVoidFunction *)&dsp);
        if (dsp) { if (local) dsp(local); if (viewSp) dsp(viewSp); }
        xrDestroySession_(sess); sess = XR_NULL_HANDLE;
    }
    if (inst) { xrDestroyInstance_(inst); inst = XR_NULL_HANDLE; }
    logf("teardown complete");
    TerminateProcess(GetCurrentProcess(), 0);   // skip DLL-detach ordering of third-party runtime modules
    return 0;
}
