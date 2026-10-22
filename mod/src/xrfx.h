// GoTVRHost v17 (0.15.5): combat feedback in the headset (owner, session 20: "there are no sfx / vfx or damage indicators - make a custom
// red damage vignette for when the player takes damage, like other VR games"). A head-locked quad in front of the eyes, painted on the
// CPU (like the wrist gauge), premultiplied, transparent in the middle:
//  - DAMAGE: Jin's health drops -> red at the edges of the view, rising in 60 ms and fading over fx_hit_fade_ms (450); stronger for a
//    bigger loss (fx_hit_strength). Below wrist_hud_low_hp (30 %) a dim red heartbeat pulse stays at the edges (fx_low_health).
//    The usual VR-game damage cue (Half-Life: Alyx, Blade & Sorcery, Boneworks): peripheral only, never across the centre, no camera shake.
//  - BLOCK: a short cool-white rim flash; PARRY: a brighter warm-gold one (game DLL events, shared fxSeq / fxKind / fxAmp).
// The DLL adds a rumble and a synthesized clang (fx_sound) for block / parry. fx_vignette = 0 turns the visual part off.
#pragma once
namespace fx {
static const u32 W = 256, H = 256;
static Chain sc; static bool tried = false, swapRB = false, on = true, lowOn = true; static ID3D12Resource *up[3] = {}; static D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{}; static u64 upSize = 0;
static u32 px[W * H]; static float edge[W * H];
static float strength = 1.0f, fadeMs = 450.0f, lowHp = 0.30f, dist = 0.5f, sizeM = 2.6f;
static float lastHp = -1.0f; static u64 hitAt = 0; static float hitAmp = 0.0f; static LONG lastFx = 0; static u64 fxAt = 0; static u32 fxKind = 0; static float fxAmp = 0.0f;
static bool active = false, haveImage = false; static float lastDrawSig = -1.0f; static u64 nHits = 0, nFlashes = 0;
static void load(const wchar_t *dir) {
    wchar_t ini[MAX_PATH]; swprintf(ini, MAX_PATH, L"%ls\\gotvr.ini", dir);
    auto f = [&](const wchar_t *k, float d) { wchar_t v[64]; GetPrivateProfileStringW(L"gotvr", k, L"", v, 64, ini); return v[0] ? (float)_wtof(v) : d; };
    auto def = [&](const wchar_t *k, const wchar_t *v) { wchar_t t[8]; if (!GetPrivateProfileStringW(L"gotvr", k, L"", t, 8, ini)) WritePrivateProfileStringW(L"gotvr", k, v, ini); };
    def(L"fx_vignette", L"1"); def(L"fx_hit_strength", L"1.0"); def(L"fx_hit_fade_ms", L"450"); def(L"fx_low_health", L"1");
    on = f(L"fx_vignette", 1) != 0; strength = fmaxf(0.0f, fminf(2.0f, f(L"fx_hit_strength", 1.0f))); fadeMs = fmaxf(100.0f, fminf(3000.0f, f(L"fx_hit_fade_ms", 450.0f)));
    lowOn = f(L"fx_low_health", 1) != 0; lowHp = f(L"wrist_hud_low_hp", 0.30f);
    for (u32 y = 0; y < H; y++) for (u32 x = 0; x < W; x++) {   // 0 in the middle -> 1 at the rim (an ellipse touching the image's sides)
        const float u = ((float)x + 0.5f) / W * 2.0f - 1.0f, v = ((float)y + 0.5f) / H * 2.0f - 1.0f; const float r = sqrtf(u * u + v * v);
        float t = (r - 0.42f) / (1.0f - 0.42f); t = t < 0 ? 0 : t > 1 ? 1 : t; edge[y * W + x] = t * t * (3.0f - 2.0f * t); }
    logf("combat feedback: vignette %s (hit strength %.2f, fade %.0f ms, low-health pulse %s below %.0f %%)", on ? "ON" : "off (fx_vignette = 0)", strength, fadeMs, lowOn ? "on" : "off", lowHp * 100.0f);
}
static bool init() {
    if (tried) return sc.sc != XR_NULL_HANDLE; tried = true;
    if (!makeChain(sc, W, H, DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)) { logf("combat feedback: no swapchain -> no vignette"); return false; }
    swapRB = typeless(sc.fmt) != DXGI_FORMAT_B8G8R8A8_TYPELESS;
    D3D12_RESOURCE_DESC td = sc.img[0]->GetDesc(); u32 rows = 0; u64 rowSize = 0; dev->GetCopyableFootprints(&td, 0, 1, 0, &fp, &rows, &rowSize, &upSize);
    D3D12_HEAP_PROPERTIES hpp{}; hpp.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC bd{}; bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; bd.Width = upSize; bd.Height = 1; bd.DepthOrArraySize = 1; bd.MipLevels = 1; bd.SampleDesc.Count = 1; bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    for (auto &u : up) if (FAILED(dev->CreateCommittedResource(&hpp, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&u)))) { logf("combat feedback: upload buffer failed"); return false; }
    logf("combat feedback: %ux%u swapchain format %d", W, H, (int)sc.fmt); return true;
}
// intensities now: damage (red), flash (block / parry), low-health pulse
static float hitI = 0, flashI = 0, lowI = 0;
static bool want(u64 now, bool gameplay) {
    if (!on) return false;
    float h = 0, m = 0; u32 vok = 0; gvRead(&S->vitSeq, [&] { h = S->vitHp; m = S->vitHpMax; vok = S->vitOk; });
    if (vok && m > 0) { if (lastHp >= 0 && h < lastHp - 0.5f) { const float d = (lastHp - h) / m; hitAmp = fminf(1.0f, (0.45f + 3.0f * d) * strength); hitAt = now; nHits++; } lastHp = h; }
    else lastHp = -1.0f;
    const LONG fs = S->fxSeq; if (fs != lastFx) { lastFx = fs; fxKind = S->fxKind; fxAmp = fminf(1.0f, fmaxf(0.0f, S->fxAmp)); fxAt = now; nFlashes++; }
    hitI = 0; if (hitAt) { const float t = (float)(now - hitAt); hitI = t < 60.0f ? hitAmp * t / 60.0f : hitAmp * expf(-(t - 60.0f) / fadeMs); if (hitI < 0.01f) hitI = 0; }
    flashI = 0; if (fxAt) { const float t = (float)(now - fxAt), dur = fxKind == 2 ? 240.0f : 160.0f; flashI = t < 30.0f ? fxAmp * t / 30.0f : fxAmp * expf(-(t - 30.0f) / dur); if (flashI < 0.01f) flashI = 0; }
    lowI = 0; if (lowOn && vok && m > 0 && h > 0 && h / m < lowHp) { const float ph = (float)(now % 1100) / 1100.0f; const float beat = ph < 0.12f ? ph / 0.12f : ph < 0.3f ? 1.0f - (ph - 0.12f) / 0.18f : 0.0f; lowI = 0.18f + 0.22f * beat; }
    active = gameplay && (hitI > 0 || flashI > 0 || lowI > 0);
    if (!active) return false;
    if (!init()) return false;
    const float sig = hitI * 1000.0f + flashI * 37.0f + lowI * 3.0f + (float)fxKind;
    if (haveImage && fabsf(sig - lastDrawSig) < 0.5f) return false;   // nothing changed visibly
    lastDrawSig = sig; return true;
}
static void draw() {
    const float pr = fxKind == 2 ? 1.0f : 0.80f, pg = fxKind == 2 ? 0.80f : 0.88f, pb = fxKind == 2 ? 0.45f : 1.0f;   // parry warm gold, block cool white
    const float red = fmaxf(hitI, lowI);
    for (u32 i = 0; i < W * H; i++) {
        const float e = edge[i]; if (e <= 0.0f) { px[i] = 0; continue; }
        const float aR = fminf(1.0f, red * e * (0.6f + 0.4f * e) * 0.9f), aF = fminf(1.0f, flashI * e * 0.55f);
        const float a = 1.0f - (1.0f - aR) * (1.0f - aF);
        float r = aR * 0.70f + aF * pr, g = aR * 0.02f + aF * pg, b = aR * 0.02f + aF * pb;   // premultiplied (the colour already times its alpha)
        r = fminf(r, a); g = fminf(g, a); b = fminf(b, a);
        const u32 A = (u32)(a * 255.0f + 0.5f), R = (u32)(r * 255.0f + 0.5f), G = (u32)(g * 255.0f + 0.5f), B = (u32)(b * 255.0f + 0.5f);
        px[i] = swapRB ? (A << 24) | (B << 16) | (G << 8) | R : (A << 24) | (R << 16) | (G << 8) | B;
    }
}
static void record(ID3D12GraphicsCommandList *c, ID3D12Resource *img, int ring) {
    draw();
    void *m = nullptr; D3D12_RANGE rr{0, 0}; if (FAILED(up[ring]->Map(0, &rr, &m))) return;
    for (u32 y = 0; y < H; y++) memcpy((uint8_t *)m + fp.Offset + (u64)y * fp.Footprint.RowPitch, px + (size_t)y * W, (size_t)W * 4);
    up[ring]->Unmap(0, nullptr);
    D3D12_RESOURCE_BARRIER br = barrier(img, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST); c->ResourceBarrier(1, &br);
    D3D12_TEXTURE_COPY_LOCATION dst{img, D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, {}}, src{up[ring], D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT, {}}; src.PlacedFootprint = fp;
    c->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    br = barrier(img, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET); c->ResourceBarrier(1, &br);
}
} // namespace fx
