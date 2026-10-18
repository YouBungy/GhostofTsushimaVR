// GoTVR probe v0.3 - EXP-00 load proof + passive EXP-02 camera read + D3D12 chain check (READ-ONLY, no hooks, no writes)
// Built as xinput1_4.dll proxy. Freestanding (no CRT). Target: GhostOfTsushima.exe
// SHA-256 ED0931B6F6D3CA3874BAF39EBC16D95FF0E93296891182F96C3A790DA7109F63 (PE timestamp 1782230973).
typedef unsigned long long u64; typedef unsigned int u32; typedef unsigned short u16; typedef unsigned char u8;
typedef void *HANDLE; typedef void *HMODULE; typedef int BOOL; typedef unsigned long DWORD; typedef u64 SIZE_T;
typedef unsigned short WCHAR; typedef long LONG; typedef void *FARPROC;
#define WINAPI __stdcall
#define DECL __declspec(dllimport)
#define INVALID_HANDLE_VALUE ((HANDLE)(long long)-1)

typedef struct { void *BaseAddress, *AllocationBase; DWORD AllocationProtect; u16 PartitionId; SIZE_T RegionSize; DWORD State, Protect, Type; } MBI;

DECL HMODULE WINAPI LoadLibraryW(const WCHAR *);
DECL FARPROC WINAPI GetProcAddress(HMODULE, const char *);
DECL HMODULE WINAPI GetModuleHandleW(const WCHAR *);
DECL DWORD WINAPI GetModuleFileNameW(HMODULE, WCHAR *, DWORD);
DECL DWORD WINAPI GetSystemDirectoryW(WCHAR *, DWORD);
DECL HANDLE WINAPI CreateThread(void *, SIZE_T, DWORD(WINAPI *)(void *), void *, DWORD, DWORD *);
DECL BOOL WINAPI CloseHandle(HANDLE);
DECL HANDLE WINAPI CreateFileW(const WCHAR *, DWORD, DWORD, void *, DWORD, DWORD, HANDLE);
DECL BOOL WINAPI WriteFile(HANDLE, const void *, DWORD, DWORD *, void *);
DECL BOOL WINAPI ReadFile(HANDLE, void *, DWORD, DWORD *, void *);
DECL BOOL WINAPI FlushFileBuffers(HANDLE);
DECL BOOL WINAPI MoveFileExW(const WCHAR *, const WCHAR *, DWORD);
DECL BOOL WINAPI CreateDirectoryW(const WCHAR *, void *);
DECL void WINAPI Sleep(DWORD);
DECL DWORD WINAPI GetCurrentProcessId(void);
DECL DWORD WINAPI GetCurrentThreadId(void);
DECL u64 WINAPI GetTickCount64(void);
DECL SIZE_T WINAPI VirtualQuery(const void *, MBI *, SIZE_T);
DECL BOOL WINAPI DisableThreadLibraryCalls(HMODULE);
DECL void WINAPI GetLocalTime(u16 *);
// bcrypt
DECL LONG WINAPI BCryptOpenAlgorithmProvider(void **, const WCHAR *, const WCHAR *, DWORD);
DECL LONG WINAPI BCryptCreateHash(void *, void **, u8 *, DWORD, u8 *, DWORD, DWORD);
DECL LONG WINAPI BCryptHashData(void *, u8 *, DWORD, DWORD);
DECL LONG WINAPI BCryptFinishHash(void *, u8 *, DWORD, DWORD);
DECL LONG WINAPI BCryptDestroyHash(void *);
DECL LONG WINAPI BCryptCloseAlgorithmProvider(void *, DWORD);

int _fltused = 1;
void *memset(void *d, int c, SIZE_T n) { u8 *p = d; while (n--) *p++ = (u8)c; return d; }
void *memcpy(void *d, const void *s, SIZE_T n) { u8 *a = d; const u8 *b = s; while (n--) *a++ = *b++; return d; }

#define PROBE_VERSION "GoTVR-probe 0.3 xinput1_4 proxy (EXP-00 load proof + passive CAM-A/CAM-B read + D3D12 chain; read-only)"
static const u32 EXPECTED_TIMESTAMP = 1782230973u;
static const char EXPECTED_SHA[] = "ed0931b6f6d3ca3874baf39ebc16d95ff0e93296891182f96c3a790da7109f63";
#define RVA_RENDERVIEW_PTR 0x1EDBCC0u
#define RVA_CAMB_PTR       0x1EDBCC8u   /* second camera global (writer of 0x5A0460 restore toggle) */
#define RVA_CAMERA_VTABLE  0x1183C60u   /* class 0x11F (factory 0x825100) */
#define RVA_CAMERA_VTABLE_BASE 0x1143850u /* class 0x11C (ctor 0x801320) */
#define RVA_NX_SERVICES    0x1998300u
#define RVA_VT_NXD3DIMPL   0x10FAC50u
#define RVA_VT_NXD3D12DRV  0x10FAF58u

// ---------------- XInput forwarding (lazy; never LoadLibrary inside DllMain) ----------------
// v0.3: built as xinput1_4.dll. The game's pad loader (0x497F30) tries xinput1_4.dll first, then
// xinput9_1_0.dll, so only this name is picked up from the game folder. The real DLL is loaded by full
// System32 path so we never resolve to ourselves.
static HMODULE g_real;
static FARPROC real(const char *name) {
    if (!g_real) {
        WCHAR p[300]; DWORD n = GetSystemDirectoryW(p, 260);
        const WCHAR *f = L"\\xinput1_4.dll"; while (*f) p[n++] = *f++; p[n] = 0;
        g_real = LoadLibraryW(p);
    }
    return g_real ? GetProcAddress(g_real, name) : 0;
}
typedef u64(WINAPI *FG)(u64, u64, u64, u64);
#define FWD(name, err) u64 WINAPI name(u64 a, u64 b, u64 c, u64 d) { FG f = (FG)real(#name); return f ? f(a, b, c, d) : (err); }
#define FWDORD(fn, ord, err) u64 WINAPI fn(u64 a, u64 b, u64 c, u64 d) { FG f = (FG)real((const char *)(u64)(ord)); return f ? f(a, b, c, d) : (err); }
FWD(XInputGetState, 1167) FWD(XInputSetState, 1167) FWD(XInputGetCapabilities, 1167) FWD(XInputEnable, 0)
FWD(XInputGetAudioDeviceIds, 1167) FWD(XInputGetBatteryInformation, 1167) FWD(XInputGetKeystroke, 1167)
FWDORD(Ord100, 100, 1167) FWDORD(Ord101, 101, 1167) FWDORD(Ord102, 102, 1167) FWDORD(Ord103, 103, 1167) FWDORD(Ord104, 104, 1167) FWDORD(Ord108, 108, 1167)

// ---------------- tiny logger ----------------
static HANDLE g_log = INVALID_HANDLE_VALUE; static int g_lines;
static char buf[2048]; static int bl;
static void put(const char *s) { while (*s && bl < 2000) buf[bl++] = *s++; }
static void putw(const WCHAR *s) { while (*s && bl < 2000) { buf[bl++] = (*s < 128) ? (char)*s : '?'; s++; } }
static void putu(u64 v) { char t[24]; int n = 0; do { t[n++] = '0' + v % 10; v /= 10; } while (v); while (n) buf[bl++] = t[--n]; }
static void puth(u64 v) { const char *h = "0123456789abcdef"; put("0x"); int s = 60; while (s > 0 && !((v >> s) & 15)) s -= 4; for (; s >= 0; s -= 4) buf[bl++] = h[(v >> s) & 15]; }
static void putf(float f) {
    if (f != f) { put("nan"); return; }
    if (f < 0) { put("-"); f = -f; }
    if (f > 1e9f) { put("big"); return; }
    u64 ip = (u64)f; putu(ip); put(".");
    float fr = f - (float)ip; for (int i = 0; i < 5; i++) { fr *= 10; int d = (int)fr; buf[bl++] = '0' + d; fr -= d; }
}
static void endl(void) {
    if (g_log == INVALID_HANDLE_VALUE || g_lines > 4000) { bl = 0; return; }
    char t[64]; int tn = 0; u64 ms = GetTickCount64(); u64 v = ms; char d[24]; int n = 0; do { d[n++] = '0' + v % 10; v /= 10; } while (v);
    t[tn++] = '['; while (n) t[tn++] = d[--n]; t[tn++] = ']'; t[tn++] = ' ';
    DWORD w; WriteFile(g_log, t, tn, &w, 0);
    buf[bl++] = '\r'; buf[bl++] = '\n'; WriteFile(g_log, buf, bl, &w, 0); FlushFileBuffers(g_log); bl = 0; g_lines++;
}

static int readable(const void *p, SIZE_T n) {
    MBI m; if (!VirtualQuery(p, &m, sizeof m)) return 0;
    if (m.State != 0x1000) return 0;                       // MEM_COMMIT
    DWORD pr = m.Protect & 0xff; if (m.Protect & 0x100) return 0; // PAGE_GUARD
    if (!(pr == 0x02 || pr == 0x04 || pr == 0x08 || pr == 0x20 || pr == 0x40 || pr == 0x80)) return 0;
    return (u8 *)p + n <= (u8 *)m.BaseAddress + m.RegionSize;
}

static int executable(const void *p) {
    MBI m; if (!VirtualQuery(p, &m, sizeof m)) return 0;
    DWORD pr = m.Protect & 0xff; return m.State == 0x1000 && (pr == 0x10 || pr == 0x20 || pr == 0x40 || pr == 0x80);
}
// class id of an engine object = immediate of vtable slot 4 ("mov eax, imm32; ret"); read-only decode, never called
static long classid(u64 obj) {
    if (!obj || !readable((void *)obj, 8)) return -1;
    u64 vt = *(u64 *)obj; if (!readable((void *)vt, 0x28)) return -2;
    u8 *f = *(u8 **)(vt + 0x20); if (!readable(f, 6)) return -3;
    return (f[0] == 0xB8 && f[5] == 0xC3) ? (long)*(u32 *)(f + 1) : -4;
}
// a COM method pointer is only used if the vtable and target are readable/executable
static void *com_slot(void *obj, int slot) {
    if (!obj || !readable(obj, 8)) return 0; u64 vt = *(u64 *)obj;
    if (!readable((void *)(vt + 8 * slot), 8)) return 0; void *f = *(void **)(vt + 8 * slot);
    return executable(f) ? f : 0;
}
static void log_cam(const char *tag, u8 *base, u64 obj, int t, int full) {
    u64 vt = *(u64 *)obj; u8 *o = (u8 *)obj;
    if (!full) {
        put(tag); put(" ptr="); puth(obj); put(" vtable_rva="); puth(vt - (u64)base);
        put(vt == (u64)(base + RVA_CAMERA_VTABLE) ? " (class 0x11F)" : vt == (u64)(base + RVA_CAMERA_VTABLE_BASE) ? " (class 0x11C)" : " (OTHER)");
        put(" classid="); puth((u64)classid(obj)); u64 par = *(u64 *)(o + 0x730);
        put(" parent="); puth(par); put(" parent_classid="); puth((u64)classid(par)); put(" t="); putu(t); put("s"); endl(); return;
    }
    float fov = *(float *)(o + 0x670), nr = *(float *)(o + 0x664), fr = *(float *)(o + 0x668), asp = *(float *)(o + 0x67C);
    float *pos = (float *)(o + 0x550), *fw = (float *)(o + 0x140), *lf = (float *)(o + 0x150), *up = (float *)(o + 0x160);
    float *wt = (float *)(o + 0x170), *lt = (float *)(o + 0x130), *P = (float *)(o + 0x200);
    put(tag); put(" t="); putu(t); put(" fov_rad="); putf(fov); put(" fov_deg="); putf(fov * 57.29578f);
    put(" near="); putf(nr); put(" far="); putf(fr); put(" aspect="); putf(asp); put(" cut7d8="); putu(*(u32 *)(o + 0x7D8));
    put(" pos550=("); putf(pos[0]); put(","); putf(pos[1]); put(","); putf(pos[2]); put(")"); endl();
    put("   world170=("); putf(wt[0]); put(","); putf(wt[1]); put(","); putf(wt[2]); put(") local130=("); putf(lt[0]); put(","); putf(lt[1]); put(","); putf(lt[2]); put(")"); endl();
    put("   fwd=("); putf(fw[0]); put(","); putf(fw[1]); put(","); putf(fw[2]); put(") left=("); putf(lf[0]); put(","); putf(lf[1]); put(","); putf(lf[2]);
    put(") up=("); putf(up[0]); put(","); putf(up[1]); put(","); putf(up[2]); put(")"); endl();
    put("   P00="); putf(P[0]); put(" P11="); putf(P[5]); put(" P20="); putf(P[8]); put(" P21="); putf(P[9]); put(" P22="); putf(P[10]); put(" P23="); putf(P[11]); put(" P32="); putf(P[14]); put(" P33="); putf(P[15]);
    put(" offcenter=("); putf(*(float *)(o + 0x6A4)); put(","); putf(*(float *)(o + 0x6A8)); put(","); putf(*(float *)(o + 0x6AC)); put(","); putf(*(float *)(o + 0x6B0)); put(") en="); putu(o[0x6B7]);
    put(" projrect694=("); putf(*(float *)(o + 0x694)); put(","); putf(*(float *)(o + 0x698)); put(","); putf(*(float *)(o + 0x69C)); put(","); putf(*(float *)(o + 0x6A0)); put(")"); endl();
}
static int is_cam_vt(u8 *base, u64 obj) {
    if (!obj || !readable((void *)obj, 0x7E0)) return 0; u64 vt = *(u64 *)obj;
    return vt == (u64)(base + RVA_CAMERA_VTABLE) || vt == (u64)(base + RVA_CAMERA_VTABLE_BASE);
}
// D3D12 chain: services(0x1998300)+0x50 NxD3DImpl -> +0x168 NxD3D12Driver -> +0x38 ID3D12Device*, [+0x30]+0 direct queue
static int d3d_chain(u8 *base, int t) {
    u64 *ps = (u64 *)(base + RVA_NX_SERVICES); if (!readable(ps, 8) || !*ps || !readable((void *)*ps, 0x120)) return 0;
    u64 d3d = *(u64 *)(*ps + 0x50); if (!d3d || !readable((void *)d3d, 0x170)) return 0;
    if (*(u64 *)d3d != (u64)(base + RVA_VT_NXD3DIMPL)) { put("d3d NxD3DImpl vtable MISMATCH"); endl(); return 1; }
    u32 mode = *(u32 *)(d3d + 0x160); u64 drv = *(u64 *)(d3d + 0x168);
    put("d3d t="); putu(t); put("s NxD3DImpl="); puth(d3d); put(" apiMode="); putu(mode); put(mode == 3 ? " (D3D12)" : " (not D3D12)"); put(" driver="); puth(drv);
    if (!drv || !readable((void *)drv, 0x118)) { put(" driver unreadable"); endl(); return 0; }
    int dvt = *(u64 *)drv == (u64)(base + RVA_VT_NXD3D12DRV); put(dvt ? " driver_vtable=NxD3D12Driver" : " driver_vtable=OTHER"); endl();
    if (!dvt) return 1;
    u64 dev = *(u64 *)(drv + 0x38), qarr = *(u64 *)(drv + 0x30); u32 nodes = *(u32 *)(drv + 0x80);
    u64 q0 = (qarr && readable((void *)qarr, 8)) ? *(u64 *)qarr : 0;
    put("d3d device="); puth(dev); put(" queueRecords="); puth(qarr); put(" nodeRecords="); putu(nodes); put(" queue0="); puth(q0); endl();
    if (!dev || !q0) return 0;
    typedef u32 (*NodeCount_t)(void *); typedef void *(*Luid_t)(void *, u32 *); typedef void *(*QDesc_t)(void *, u32 *);
    NodeCount_t gn = (NodeCount_t)com_slot((void *)dev, 7);      // ID3D12Device::GetNodeCount
    Luid_t gl = (Luid_t)com_slot((void *)dev, 43);               // ID3D12Device::GetAdapterLuid (hidden return ptr)
    QDesc_t gd = (QDesc_t)com_slot((void *)q0, 18);              // ID3D12CommandQueue::GetDesc (hidden return ptr)
    if (gn) { put("d3d device.GetNodeCount="); putu(gn((void *)dev)); endl(); }
    if (gl) { u32 l[2] = {0, 0}; gl((void *)dev, l); put("d3d device.AdapterLuid hi="); puth(l[1]); put(" lo="); puth(l[0]); put(" (OpenXR adapterLuid must match)"); endl(); }
    if (gd) { u32 dsc[4] = {9, 9, 9, 9}; gd((void *)q0, dsc); put("d3d queue0.Desc Type="); putu(dsc[0]); put(dsc[0] == 0 ? " (DIRECT)" : " (NOT DIRECT)"); put(" Priority="); putu(dsc[1]); put(" Flags="); putu(dsc[2]); put(" NodeMask="); putu(dsc[3]); endl(); }
    return 1;
}

// ---------------- signature guards (from re/out/signatures.json, ?? = -1) ----------------
typedef struct { const char *name; u32 rva; const char *sig; } Sig;
static const Sig sigs[] = {
    {"Camera_SetFov", 0x806F50, "40 53 48 83 EC 20 0F 2E 89 70 06 00 00 48 8B D9"},
    {"Camera_SetOffCenter", 0x807080, "40 53 48 83 EC 20 0F 10 02 C6 81 B7 06 00 00 01"},
    {"Camera_ResetOffCenter", 0x806240, "C7 81 A4 06 00 00 00 00 80 BF C7 81 A8 06 00 00 00 00 80 BF"},
    {"Camera_BuildSLConstants", 0x807140, "48 89 5C 24 10 55 48 8D AC 24 00 FD FF FF 48 81 EC 00 04 00 00"},
    {"RenderView_global_load", 0x60FA11, "48 8B 35 ?? ?? ?? ?? 4C 8D 45 50 0F 57 C0 66 C7 45 70 00 00"},
    {"DXGI_Present_callsite", 0x435428, "48 8B 4E 30 41 8B D7 44 8B 44 24 24 48 8B 01 FF 50 40"},
    {"NxDXGI_PresentWrapper", 0x434CC0, "40 55 53 56 57 41 54 41 56 41 57 48 8D AC 24 80 FD FF FF"},
    {"XInput_loader", 0x4AEAA0, "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 41 54 41 55 41 56 41 57 48 83 EC 50 48 8B 05 ?? ?? ?? ??"},
    // v0.2 additions (re/out/signatures_v2.json, all verified unique in .text)
    {"Cam_SetLocalMatrix", 0x806E50, "0F 10 02 0F 11 81 00 01 00 00 0F 10 4A 10 0F 11 89 10 01 00 00"},
    {"Cam_SetViewProjFrustum", 0x806BA0, "48 8B C4 48 89 58 10 55 56 57 48 81 EC D0 00 00 00"},
    {"Cam_factory_11F", 0x825100, "40 53 48 83 EC 30 48 85 C9 74 12 BA 20 09 00 00"},
    {"CamDrv_AttachedPose", 0xA32DF0, "4C 8B DC 55 53 56 49 8D AB 68 FE FF FF 48 81 EC 80 02 00 00"},
    {"GamepadXInput_Poll", 0x498030, "40 53 48 83 EC 50 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 44 24 40 80 3D ?? ?? ?? ?? 00"},
    {"D3D12_CreateQueue_site", 0x43846F, "41 8B C7 44 89 65 A0 D3 E0 4C 8D 05 ?? ?? ?? ??"},
    {"slSetD3DDevice_site", 0x4381E6, "49 8B 4E 38 E8 ?? ?? ?? ?? 48 63 D8 85 C0 0F 84 ?? ?? ?? ??"},
    {"RenderSubmitThread", 0x509FB0, "40 56 48 81 EC 80 00 00 00 48 8B F1 BA FF FF FF FF"},
    {"Cam_SetProjRect", 0x8068E0, "40 53 48 83 EC 20 0F 10 02 48 8B 01 48 8B D9 0F 11 81 94 06 00 00"},
};
static int hexv(char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'A' && c <= 'F' ? c - 'A' + 10 : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1; }
static int check_sig(u8 *base, const Sig *s) {
    u8 *p = base + s->rva; const char *q = s->sig; int i = 0;
    if (!readable(p, 64)) return -1;
    while (*q) {
        while (*q == ' ') q++; if (!*q) break;
        if (q[0] == '?') { q += 2; i++; continue; }
        int b = hexv(q[0]) * 16 + hexv(q[1]); q += 2;
        if (p[i] != (u8)b) return 0; i++;
    }
    return 1;
}

static void sha256_file(const WCHAR *path, char *out) {
    out[0] = 0; void *alg = 0, *h = 0; static u8 data[1 << 16]; u8 dg[32];
    HANDLE f = CreateFileW(path, 0x80000000, 1 | 2 | 4, 0, 3, 0x80, 0);
    if (f == INVALID_HANDLE_VALUE) return;
    if (BCryptOpenAlgorithmProvider(&alg, L"SHA256", 0, 0) == 0 && BCryptCreateHash(alg, &h, 0, 0, 0, 0, 0) == 0) {
        DWORD r;
        while (ReadFile(f, data, sizeof data, &r, 0) && r) BCryptHashData(h, data, r, 0);
        if (BCryptFinishHash(h, dg, 32, 0) == 0) { const char *x = "0123456789abcdef"; for (int i = 0; i < 32; i++) { out[2 * i] = x[dg[i] >> 4]; out[2 * i + 1] = x[dg[i] & 15]; } out[64] = 0; }
        BCryptDestroyHash(h);
    }
    if (alg) BCryptCloseAlgorithmProvider(alg, 0);
    CloseHandle(f);
}

static HMODULE g_self;
static DWORD WINAPI worker(void *arg) {
    (void)arg;
    static WCHAR exe[520], dir[520], logp[560], prev[560];
    DWORD n = GetModuleFileNameW(0, exe, 512); exe[n] = 0;
    int k = (int)n; while (k > 0 && exe[k - 1] != '\\') k--;
    for (int i = 0; i < k; i++) dir[i] = exe[i]; dir[k] = 0;
    const WCHAR *sub = L"GoTVR"; int m = k; while (*sub) dir[m++] = *sub++; dir[m] = 0;
    CreateDirectoryW(dir, 0);
    int a = 0; for (; dir[a]; a++) { logp[a] = dir[a]; prev[a] = dir[a]; }
    const WCHAR *l1 = L"\\load.log", *l2 = L"\\load.prev.log"; int b = a, c = a;
    while (*l1) logp[b++] = *l1++; logp[b] = 0; while (*l2) prev[c++] = *l2++; prev[c] = 0;
    MoveFileExW(logp, prev, 1 /*REPLACE_EXISTING*/);
    g_log = CreateFileW(logp, 0x40000000, 1, 0, 2 /*CREATE_ALWAYS*/, 0x80, 0);

    static WCHAR self[520]; DWORD sn = GetModuleFileNameW(g_self, self, 512); self[sn] = 0;
    u16 st[8]; GetLocalTime(st);
    put("IDENTITY "); put(PROBE_VERSION); put(" built " __DATE__ " " __TIME__); endl();
    put("dll="); putw(self); endl();
    put("pid="); putu(GetCurrentProcessId()); put(" tid="); putu(GetCurrentThreadId()); put(" localtime=");
    putu(st[0]); put("-"); putu(st[1]); put("-"); putu(st[3]); put(" "); putu(st[4]); put(":"); putu(st[5]); put(":"); putu(st[6]); endl();
    put("exe="); putw(exe); endl();

    u8 *base = (u8 *)GetModuleHandleW(0);
    u32 e = *(u32 *)(base + 0x3C); u32 ts = *(u32 *)(base + e + 8); u32 soi = *(u32 *)(base + e + 24 + 56);
    put("module_base="); puth((u64)base); put(" size_of_image="); puth(soi); put(" pe_timestamp="); putu(ts);
    put(ts == EXPECTED_TIMESTAMP ? " (expected)" : " (UNEXPECTED BUILD)"); endl();

    char sha[65]; sha256_file(exe, sha);
    int shaok = 1; for (int i = 0; i < 64; i++) if (sha[i] != EXPECTED_SHA[i]) shaok = 0;
    put("exe_sha256="); put(sha[0] ? sha : "(failed)"); put(shaok ? " MATCH" : " MISMATCH"); endl();

    int pass = 0, total = (int)(sizeof sigs / sizeof sigs[0]);
    for (int i = 0; i < total; i++) {
        int r = check_sig(base, &sigs[i]); pass += (r == 1);
        put("sig "); put(sigs[i].name); put(" rva="); puth(sigs[i].rva); put(r == 1 ? " PASS" : r == 0 ? " FAIL" : " UNREADABLE"); endl();
    }
    put("sig_summary "); putu(pass); put("/"); putu(total); put(pass == total && shaok ? " -> build supported" : " -> UNSUPPORTED: camera reads disabled"); endl();
    int cam_enabled = (pass == total && shaok && ts == EXPECTED_TIMESTAMP);

    // Passive watch: modules + CAM-A. Logs on change, plus a camera sample every 5 s. Stops after 30 min.
    const WCHAR *mods[] = {L"d3d12.dll", L"D3D12Core.dll", L"dxgi.dll", L"sl.interposer.dll", L"xinput1_4.dll", L"XInput1_3.dll", L"amd_fidelityfx_dx12.dll", L"libxess.dll"};
    HMODULE seen[8] = {0};
    u64 lastptr = ~0ull, lastptrB = ~0ull; int lastvt = -1, lastvtB = -1, d3d_done = 0;
    for (int t = 0; t < 1800; t++) {
        for (int i = 0; i < 8; i++) {
            HMODULE h = GetModuleHandleW(mods[i]);
            if (h != seen[i]) { seen[i] = h; put("module "); putw(mods[i]); put(h ? " loaded at " : " not loaded"); if (h) puth((u64)h); put(" t="); putu(t); put("s"); endl(); }
        }
        if (cam_enabled) {
            u64 *pa = (u64 *)(base + RVA_RENDERVIEW_PTR), *pb = (u64 *)(base + RVA_CAMB_PTR);
            u64 a = readable(pa, 8) ? *pa : 0, b = readable(pb, 8) ? *pb : 0;
            int va = is_cam_vt(base, a), vb = is_cam_vt(base, b);
            if (a != lastptr || va != lastvt) { if (va) log_cam("camA", base, a, t, 0); else { put("camA ptr="); puth(a); put(" vtable=no-match/unreadable t="); putu(t); put("s"); endl(); } lastptr = a; lastvt = va; }
            if (b != lastptrB || vb != lastvtB) { if (vb) log_cam("camB", base, b, t, 0); else { put("camB ptr="); puth(b); put(" vtable=no-match/unreadable t="); putu(t); put("s"); endl(); } lastptrB = b; lastvtB = vb; }
            if (a && a == b && t % 60 == 0) { put("camA == camB (same object)"); endl(); }
            if (t % 5 == 0) { if (va) log_cam("camA", base, a, t, 1); if (vb && b != a) log_cam("camB", base, b, t, 1); }
            if (!d3d_done && t >= 15 && GetModuleHandleW(L"d3d12.dll")) d3d_done = d3d_chain(base, t);
        }
        Sleep(1000);
    }
    put("watch finished (30 min limit)"); endl();
    return 0;
}

BOOL WINAPI DllMain(HMODULE h, DWORD reason, void *r) {
    (void)r;
    if (reason == 1) {
        g_self = h; DisableThreadLibraryCalls(h);
        HANDLE t = CreateThread(0, 0, worker, 0, 0, 0); if (t) CloseHandle(t);
    }
    return 1;
}
