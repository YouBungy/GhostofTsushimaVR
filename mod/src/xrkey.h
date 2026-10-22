// GoTVRHost 0.15.6: the UI image's "black squares" (owner, sessions 18-21: a black square behind a picked-up item's picture). The UI dump
// (Ctrl+Shift+U, session 21) shows it: the pickup panel writes FULL alpha (255) over its whole rectangle (2040-2303 x 534-1080 of the
// 2304 image) while its colour there is black - on the monitor the game draws it over the scene in a way where black adds nothing and
// the back buffer's alpha is never used, but on the headset's separate UI layer (premultiplied) full alpha + black = an opaque black box.
// So instead of a plain copy, the UI image goes through one tiny pixel shader on its way into the layer: an OPAQUE pixel's alpha becomes
// its brightness when it is darker than hud_black_key_level (0.20) - black -> transparent, the dark glow round the icon fades out like
// the additive look on the monitor, and everything brighter (or already see-through) stays exactly as it was. hud_black_key = 0 = the
// old plain copy. Compiled at start with Windows' own d3dcompiler_47.dll; anything missing -> the plain copy, logged.
#pragma once
#include <d3dcompiler.h>
namespace key {
static bool on = true, tried = false, ready = false, hurtOff = true; static float level = 0.20f;
static ID3D12RootSignature *rs = nullptr; static ID3D12PipelineState *pso = nullptr; static ID3D12DescriptorHeap *srv = nullptr; static u32 srvStep = 0; static LONG srvGen = -1; static DXGI_FORMAT psoFmt = DXGI_FORMAT_UNKNOWN;
static u64 nDrawn = 0;
static void load(const wchar_t *dir) {
    wchar_t ini[MAX_PATH]; swprintf(ini, MAX_PATH, L"%ls\\gotvr.ini", dir); wchar_t t[32];
    if (!GetPrivateProfileStringW(L"gotvr", L"hud_black_key", L"", t, 32, ini)) WritePrivateProfileStringW(L"gotvr", L"hud_black_key", L"1", ini);
    if (!GetPrivateProfileStringW(L"gotvr", L"hud_black_key_level", L"", t, 32, ini)) WritePrivateProfileStringW(L"gotvr", L"hud_black_key_level", L"0.20", ini);
    on = GetPrivateProfileIntW(L"gotvr", L"hud_black_key", 1, ini) != 0;
    if (!GetPrivateProfileStringW(L"gotvr", L"hud_hide_game_hit", L"", t, 32, ini)) WritePrivateProfileStringW(L"gotvr", L"hud_hide_game_hit", L"1", ini); hurtOff = GetPrivateProfileIntW(L"gotvr", L"hud_hide_game_hit", 1, ini) != 0; GetPrivateProfileStringW(L"gotvr", L"hud_black_key_level", L"0.20", t, 32, ini); level = (float)_wtof(t); if (!(level > 0.005f && level < 0.9f)) level = 0.20f;
}
// 0.15.7 (session 26: "the in-game damage HUD keeps appearing"; ui_hit_*.bmp): the game's hit overlay is NOT see-through - for the
// moment Jin is hit the whole UI image is opaque (alpha 156-255 everywhere), black in the middle, dark red streaks at the edges and an
// orange cross at the centre, with the normal HUD on top. So the 0.15.6 key (see-through dark / red pixels only) never touched it. Now
// the shader sees the overlay itself: four points where the HUD never draws (30 % / 70 % across and down the middle) all opaque and dark
// AND red streaks at two or more of eight points near the edges (a fade to black has none) = the overlay is up (no timing needed - also when the mod puts the health back before the host saw it drop); then dark, red and the
// orange cross go (warm: r > 1.35 g and > 2 b; the HUD's gold / cream / pink stay), the HUD (bright, not red) stays, its dark rims fade by brightness. hud_hide_game_hit = 0 turns it off.
static const char kSrc[] =
    "Texture2D<float4> T : register(t0);\n"
    "cbuffer C : register(b0) { float lvl; float hurt; };\n"
    "float4 vs(uint id : SV_VertexID) : SV_Position { float2 p = float2((id << 1) & 2, id & 2); return float4(p * float2(2, -2) + float2(-1, 1), 0, 1); }\n"
    "float3 lin(float3 c) { return c <= 0.04045 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4); }\n"
    "float dk(float4 c) { return (c.a > 0.6 && max(c.r, max(c.g, c.b)) < 0.25) ? 1.0 : 0.0; }\n"
    "float rd(float4 c) { return (c.a > 0.6 && c.r > 0.06 && c.r > 3.0 * max(c.g, c.b)) ? 1.0 : 0.0; }\n"
    "float4 ps(float4 p : SV_Position) : SV_Target {\n"
    "  uint w, h; T.GetDimensions(w, h); \n"
    "  float4 c = T.Load(int3(p.xy, 0)); float k = max(c.r, max(c.g, c.b));\n"
    "  if (hurt > 0.0) {\n"
    "    float n = dk(T.Load(int3(w / 2, h * 3 / 10, 0))) + dk(T.Load(int3(w * 3 / 10, h / 2, 0))) + dk(T.Load(int3(w * 7 / 10, h / 2, 0))) + dk(T.Load(int3(w / 2, h * 7 / 10, 0)));\n"
    "    float e = rd(T.Load(int3(w * 15 / 100, h * 15 / 100, 0))) + rd(T.Load(int3(w * 85 / 100, h * 15 / 100, 0))) + rd(T.Load(int3(w * 15 / 100, h * 85 / 100, 0))) + rd(T.Load(int3(w * 85 / 100, h * 85 / 100, 0)))\n"
    "            + rd(T.Load(int3(w * 5 / 100, h * 5 / 100, 0))) + rd(T.Load(int3(w * 95 / 100, h * 5 / 100, 0))) + rd(T.Load(int3(w / 4, h * 8 / 100, 0))) + rd(T.Load(int3(w * 3 / 4, h * 8 / 100, 0)));\n"
    "    bool red = c.r > 1.6 * max(c.g, c.b);\n"
    "    if (n >= 3.0 && e >= 2.0) {\n"                                                    // the game's opaque hit overlay is up
    "      if (k < 0.12 || (red && k < 0.8) || (c.r > 1.35 * c.g && c.r > 2.0 * c.b)) c = 0;\n"
    "      else if (k < 0.35) { c.a = min(c.a, (k - 0.12) / 0.23); c.rgb *= c.a; }\n"   // the HUD's dark rim over it fades out
    "    } else if (hurt > 1.5 && c.a < 0.97 && (k < 0.12 || red)) c = 0;\n"   // just hit (health dropped): the see-through dark / red overlay
    "  }\n"
    "  if (c.a > 0.97 && k < lvl) c.a = k / lvl;\n"                            // an opaque dark pixel: as see-through as it is dark
    "  return float4(lin(c.rgb), c.a); }\n";                   // the target is sRGB: hand it linear so it stores the same bytes
typedef HRESULT (WINAPI *FnCompile)(LPCVOID, SIZE_T, LPCSTR, const D3D_SHADER_MACRO *, ID3DInclude *, LPCSTR, LPCSTR, UINT, UINT, ID3DBlob **, ID3DBlob **);
static bool init(DXGI_FORMAT rtFmt) {
    if (ready && psoFmt == rtFmt) return true; if (tried && psoFmt == rtFmt) return false; tried = true; psoFmt = rtFmt;
    if (pso) { pso->Release(); pso = nullptr; }
    HMODULE dc = LoadLibraryW(L"d3dcompiler_47.dll"); FnCompile comp = dc ? (FnCompile)(void *)GetProcAddress(dc, "D3DCompile") : nullptr;
    if (!comp) { logf("UI black key: d3dcompiler_47.dll not found -> plain UI copy"); return false; }
    ID3DBlob *vsb = nullptr, *psb = nullptr, *err = nullptr;
    if (FAILED(comp(kSrc, sizeof kSrc - 1, "key", nullptr, nullptr, "vs", "vs_5_0", 0, 0, &vsb, &err)) || FAILED(comp(kSrc, sizeof kSrc - 1, "key", nullptr, nullptr, "ps", "ps_5_0", 0, 0, &psb, &err))) {
        logf("UI black key: shader compile failed (%s) -> plain UI copy", err ? (const char *)err->GetBufferPointer() : "?"); if (err) err->Release(); if (vsb) vsb->Release(); return false; }
    if (!rs) {
        D3D12_DESCRIPTOR_RANGE rg{}; rg.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; rg.NumDescriptors = 1; rg.BaseShaderRegister = 0; rg.OffsetInDescriptorsFromTableStart = 0;
        D3D12_ROOT_PARAMETER rp[2]{}; rp[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; rp[0].DescriptorTable.NumDescriptorRanges = 1; rp[0].DescriptorTable.pDescriptorRanges = &rg; rp[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        rp[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS; rp[1].Constants.ShaderRegister = 0; rp[1].Constants.Num32BitValues = 2; rp[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        D3D12_ROOT_SIGNATURE_DESC rd{}; rd.NumParameters = 2; rd.pParameters = rp; ID3DBlob *sig = nullptr, *e2 = nullptr;
        if (FAILED(D3D12SerializeRootSignature(&rd, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &e2)) || FAILED(dev->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&rs)))) {
            logf("UI black key: root signature failed -> plain UI copy"); if (sig) sig->Release(); if (e2) e2->Release(); vsb->Release(); psb->Release(); rs = nullptr; return false; }
        sig->Release();
    }
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pd{}; pd.pRootSignature = rs; pd.VS = {vsb->GetBufferPointer(), vsb->GetBufferSize()}; pd.PS = {psb->GetBufferPointer(), psb->GetBufferSize()};
    pd.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL; pd.SampleMask = 0xFFFFFFFFu;
    pd.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID; pd.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; pd.RasterizerState.DepthClipEnable = TRUE;
    pd.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; pd.NumRenderTargets = 1; pd.RTVFormats[0] = rtFmt; pd.SampleDesc.Count = 1;
    const HRESULT hr = dev->CreateGraphicsPipelineState(&pd, IID_PPV_ARGS(&pso)); vsb->Release(); psb->Release();
    if (FAILED(hr)) { logf("UI black key: pipeline failed 0x%08lx -> plain UI copy", (unsigned long)hr); pso = nullptr; return false; }
    if (!srv) { D3D12_DESCRIPTOR_HEAP_DESC hd{}; hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; hd.NumDescriptors = GV_UITEX; hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        if (FAILED(dev->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&srv)))) { logf("UI black key: descriptor heap failed -> plain UI copy"); srv = nullptr; return false; }
        srvStep = dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV); }
    ready = true; logf("UI black key: ON (opaque UI pixels darker than %.2f become as see-through as they are dark - the pickup panel's black box)", level); return true;
}
// the UI image -> the layer's swapchain image (in RENDER_TARGET state); false = not done (the caller copies instead)
// hurtOff: hud_hide_game_hit (1): the game's hit overlay keyed out for 1.5 s after Jin is hit (our vignette replaces it)
static bool draw(ID3D12GraphicsCommandList *c, ID3D12Resource *src[], u32 srcIdx, LONG gen, D3D12_CPU_DESCRIPTOR_HANDLE rtv, u32 w, u32 h, DXGI_FORMAT rtFmt, bool hurt = false, bool full = false) {
    if ((!on && !hurtOff) || !init(rtFmt) || !src[srcIdx]) return false;
    if (srvGen != gen) { D3D12_CPU_DESCRIPTOR_HANDLE hh = srv->GetCPUDescriptorHandleForHeapStart();
        for (u32 i = 0; i < GV_UITEX; i++) if (src[i]) { D3D12_SHADER_RESOURCE_VIEW_DESC sd{}; { DXGI_FORMAT f0 = src[i]->GetDesc().Format; sd.Format = f0 == DXGI_FORMAT_R8G8B8A8_TYPELESS ? DXGI_FORMAT_R8G8B8A8_UNORM : f0 == DXGI_FORMAT_B8G8R8A8_TYPELESS ? DXGI_FORMAT_B8G8R8A8_UNORM : f0 == DXGI_FORMAT_R10G10B10A2_TYPELESS ? DXGI_FORMAT_R10G10B10A2_UNORM : f0; } sd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; sd.Texture2D.MipLevels = 1;
            sd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; dev->CreateShaderResourceView(src[i], &sd, {hh.ptr + (SIZE_T)i * srvStep}); }
        srvGen = gen; }
    c->SetGraphicsRootSignature(rs); c->SetPipelineState(pso); ID3D12DescriptorHeap *hp[] = {srv}; c->SetDescriptorHeaps(1, hp);
    D3D12_GPU_DESCRIPTOR_HANDLE g = srv->GetGPUDescriptorHandleForHeapStart(); g.ptr += (u64)srcIdx * srvStep; c->SetGraphicsRootDescriptorTable(0, g);
    const float cst[2] = {on ? level : 0.0001f, (hurtOff && !full) ? (hurt ? 2.0f : 1.0f) : 0.0f};   // 0.15.7 (review): never during a full-view fade / cinematic
    c->SetGraphicsRoot32BitConstants(1, 2, cst, 0);
    c->OMSetRenderTargets(1, &rtv, FALSE, nullptr); D3D12_VIEWPORT vp{0, 0, (float)w, (float)h, 0, 1}; D3D12_RECT sr{0, 0, (LONG)w, (LONG)h}; c->RSSetViewports(1, &vp); c->RSSetScissorRects(1, &sr);
    c->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); c->DrawInstanced(3, 1, 0, 0); nDrawn++;
    return true;
}
} // namespace key
