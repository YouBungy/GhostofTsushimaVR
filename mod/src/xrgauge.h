// GoTVRHost v16: the wrist HUD, stage W2 (docs/WRIST_HUD.md) - our own health / resolve gauge, painted from the game's values (shared
// block vitHp / vitHpMax / vitRes / vitResMax), so it is there whenever you look at your forearm (the game's own HUD fades its health bar
// out of combat - owner, 2026-10-21: "is there any way to make the wrist HUD always show when the player looks at the forearm?").
// Painted with GDI into a 512x128 image like the item wheel (xroverlay.h), only when the values change; shown at the wrist HUD's place.
//  - a dark lacquer plate; the health bar (ivory, like the game's) with the health just lost in red, fading over 0.6 s;
//  - resolve as gold pips above it (the game shows resolve as pips) once the game DLL knows where resolve lives (WRIST_HUD.md W0);
//  - below wrist_hud_low_hp the bar turns red and the plate pulses with the heartbeat (the game DLL rumbles the left hand).
#pragma once
namespace gauge {
static const u32 W = 512, H = 128;
static Chain sc; static bool tried = false, swapRB = false, haveImage = false; static ID3D12Resource *up[3] = {}; static D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{}; static u64 upSize = 0;
static HDC dc = nullptr; static HBITMAP bmp = nullptr; static u32 *px = nullptr;
static float hp = 0, hpMax = 0, res = 0, resMax = 0, lostFrom = 0; static u64 lostAt = 0, sigDrawn = ~0ull; static bool ok = false;
static bool init() {
    if (tried) return sc.sc != XR_NULL_HANDLE; tried = true;
    if (!makeChain(sc, W, H, DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)) { logf("wrist gauge: no swapchain -> the game's own HUD art only"); return false; }
    swapRB = typeless(sc.fmt) != DXGI_FORMAT_B8G8R8A8_TYPELESS;
    D3D12_RESOURCE_DESC td = sc.img[0]->GetDesc(); u32 rows = 0; u64 rowSize = 0; dev->GetCopyableFootprints(&td, 0, 1, 0, &fp, &rows, &rowSize, &upSize);
    D3D12_HEAP_PROPERTIES hpp{}; hpp.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC bd{}; bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; bd.Width = upSize; bd.Height = 1; bd.DepthOrArraySize = 1; bd.MipLevels = 1; bd.SampleDesc.Count = 1; bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    for (auto &u : up) if (FAILED(dev->CreateCommittedResource(&hpp, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&u)))) { logf("wrist gauge: upload buffer failed"); return false; }
    BITMAPINFO bi{}; bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = (LONG)W; bi.bmiHeader.biHeight = -(LONG)H; bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    dc = CreateCompatibleDC(nullptr); bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, (void **)&px, nullptr, 0); if (!dc || !bmp || !px) { logf("wrist gauge: GDI surface failed"); return false; }
    SelectObject(dc, bmp); logf("wrist gauge: %ux%u swapchain format %d", W, H, (int)sc.fmt); return true;
}
static void box(int x0, int y0, int x1, int y1, COLORREF c) { HBRUSH b = CreateSolidBrush(c); RECT r{x0, y0, x1, y1}; FillRect(dc, &r, b); DeleteObject(b); }
static void disc(int cx, int cy, int r, COLORREF fill, COLORREF ring) { HBRUSH b = CreateSolidBrush(fill); HPEN p = CreatePen(PS_SOLID, 3, ring); SelectObject(dc, b); SelectObject(dc, p); Ellipse(dc, cx - r, cy - r, cx + r, cy + r); DeleteObject(b); DeleteObject(p); }
// the state from the game DLL; true when the image must be redrawn (acquire + record this frame)
static bool want(u64 now, float lowHp) {
    float h = 0, m = 0, r = 0, rm = 0; u32 vok = 0; gvRead(&S->vitSeq, [&] { h = S->vitHp; m = S->vitHpMax; r = S->vitRes; rm = S->vitResMax; vok = S->vitOk; });
    ok = vok && m > 0; if (!ok) return false;
    if (h < hp - 0.05f && hpMax > 0) { lostFrom = lostAt && now - lostAt < 600 ? lostFrom : hp; lostAt = now; }   // health just lost: shown red, fading
    hp = h; hpMax = m; res = r; resMax = rm;
    const bool fading = lostAt && now - lostAt < 600, low = hp / hpMax < lowHp && hp > 0;
    const u64 sig = (u64)(hp * 4) ^ ((u64)(hpMax * 4) << 16) ^ ((u64)(res * 16) << 32) ^ ((u64)(resMax * 16) << 44) ^ (fading ? (now - lostAt) / 50 + 1 : 0) * 0x9E3779B1ull ^ (low ? ((now / 100) & 15) + 77 : 0);
    if (!init()) return false;
    return sig != sigDrawn || !haveImage ? (sigDrawn = sig, true) : false;
}
static void draw(u64 now, float lowHp) {
    memset(px, 0, (size_t)W * H * 4);
    const float f = hpMax > 0 ? fminf(1.0f, fmaxf(0.0f, hp / hpMax)) : 0.0f; const bool low = f < lowHp && f > 0;
    const float pulse = low ? 0.5f + 0.5f * sinf((float)(now % 1000) / 1000.0f * 6.2831853f) : 0.0f;
    box(0, 0, W, H, RGB(18 + (int)(40 * pulse), 16, 16));   // the plate
    const int bx0 = 24, bx1 = (int)W - 24, by0 = resMax > 0 ? 78 : 50, by1 = by0 + 26;
    box(bx0 - 3, by0 - 3, bx1 + 3, by1 + 3, RGB(60, 52, 44)); box(bx0, by0, bx1, by1, RGB(40, 8, 8));
    const int xf = bx0 + (int)((bx1 - bx0) * f); box(bx0, by0, xf, by1, low ? RGB(210, 50, 40) : RGB(236, 228, 210));
    if (lostAt && now - lostAt < 600 && hpMax > 0) { const float a = 1.0f - (float)(now - lostAt) / 600.0f, fl = fminf(1.0f, lostFrom / hpMax); const int xl = bx0 + (int)((bx1 - bx0) * fl);
        if (xl > xf) box(xf, by0, xl, by1, RGB((int)(200 * a + 40 * (1 - a)), (int)(20 * a + 8 * (1 - a)), (int)(20 * a + 8 * (1 - a)))); }
    if (resMax > 0) {   // resolve pips (WRIST_HUD.md W0 fills vitRes / vitResMax)
        const int n = resMax > 12 ? 12 : (int)(resMax + 0.5f); const float rv = fmaxf(0.0f, fminf(res, resMax));
        for (int i = 0; i < n; i++) { const int cx = bx0 + 18 + i * 38, cy = 36; const float fill = fminf(1.0f, fmaxf(0.0f, rv - (float)i));
            disc(cx, cy, 14, fill >= 0.999f ? RGB(250, 200, 80) : fill > 0 ? RGB(130, 100, 40) : RGB(30, 26, 22), RGB(200, 160, 70)); }
    }
    for (u32 y = 0; y < H; y++) for (u32 x = 0; x < W; x++) {   // premultiplied: the plate 215 with rounded corners, transparent outside
        const int dx = (int)x < 16 ? 16 - (int)x : (int)x > (int)W - 17 ? (int)x - ((int)W - 17) : 0, dy = (int)y < 16 ? 16 - (int)y : (int)y > (int)H - 17 ? (int)y - ((int)H - 17) : 0;
        u32 &p = px[y * W + x]; if (dx * dx + dy * dy > 256) { p = 0; continue; }
        const u32 a = 215, b = (p & 255) * a / 255, g = ((p >> 8) & 255) * a / 255, r = ((p >> 16) & 255) * a / 255;
        p = swapRB ? (a << 24) | (b << 16) | (g << 8) | r : (a << 24) | (r << 16) | (g << 8) | b; }
}
static void record(ID3D12GraphicsCommandList *c, ID3D12Resource *img, int ring, u64 now, float lowHp) {
    draw(now, lowHp);
    void *m = nullptr; D3D12_RANGE rr{0, 0}; if (FAILED(up[ring]->Map(0, &rr, &m))) return;
    for (u32 y = 0; y < H; y++) memcpy((uint8_t *)m + fp.Offset + (u64)y * fp.Footprint.RowPitch, px + (size_t)y * W, (size_t)W * 4);
    up[ring]->Unmap(0, nullptr);
    D3D12_RESOURCE_BARRIER br = barrier(img, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST); c->ResourceBarrier(1, &br);
    D3D12_TEXTURE_COPY_LOCATION dst{img, D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, {}}, src{up[ring], D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT, {}}; src.PlacedFootprint = fp;
    c->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    br = barrier(img, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET); c->ResourceBarrier(1, &br);
}
} // namespace gauge
