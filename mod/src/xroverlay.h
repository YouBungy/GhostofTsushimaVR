// GoTVRHost 0.14.1 overlay: the item wheel at the hand and the head-gesture test indicator (shared block v13).
// Drawn on the CPU with GDI into one 1024x512 image (left half = wheel, right half = indicator), copied into an XR swapchain only
// when it changes, shown as quad layers. The real wheel art (the game's own icons and font) replaces this later (ROADMAP phase 1,
// the bloodborne-vr approach); this is the working placeholder.
#pragma once

namespace ov {
static const u32 W = 1024, H = 512, HALF = 512;
static Chain sc; static bool tried = false, swapRB = false;
static ID3D12Resource *up[3] = {}; static D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{}; static u64 upSize = 0;
static HDC dc = nullptr; static HBITMAP bmp = nullptr; static u32 *px = nullptr; static HFONT fBig = nullptr, fMid = nullptr, fSmall = nullptr;
struct St { u32 wheelOpen; int32_t sel; u32 n; int32_t hand; char names[8][24]; u32 dbg, atHead, gseq[2]; char gname[2][24]; };
static St cur{}; static u64 flashUntil[2] = {0, 0}; static u32 seenSeq[2] = {0, 0}; static char flashName[2][24] = {};
static u64 sigDrawn = ~0ull; static bool haveImage = false;
static std::atomic<u64> nDraws{0};

static bool init() {
    if (tried) return sc.sc != XR_NULL_HANDLE; tried = true;
    if (!makeChain(sc, W, H, DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)) { logf("overlay: no swapchain -> no item wheel / gesture indicator"); return false; }
    swapRB = typeless(sc.fmt) != DXGI_FORMAT_B8G8R8A8_TYPELESS;
    D3D12_RESOURCE_DESC td = sc.img[0]->GetDesc(); u32 rows = 0; u64 rowSize = 0;
    dev->GetCopyableFootprints(&td, 0, 1, 0, &fp, &rows, &rowSize, &upSize);
    D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC bd{}; bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; bd.Width = upSize; bd.Height = 1; bd.DepthOrArraySize = 1; bd.MipLevels = 1; bd.SampleDesc.Count = 1; bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    for (auto &u : up) if (FAILED(dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&u)))) { logf("overlay: upload buffer failed"); return false; }
    BITMAPINFO bi{}; bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = (LONG)W; bi.bmiHeader.biHeight = -(LONG)H; bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    dc = CreateCompatibleDC(nullptr); bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, (void **)&px, nullptr, 0); if (!dc || !bmp || !px) { logf("overlay: GDI surface failed"); return false; }
    SelectObject(dc, bmp);
    fBig = CreateFontW(54, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, L"Segoe UI");
    fMid = CreateFontW(40, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, L"Segoe UI");
    fSmall = CreateFontW(32, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, L"Segoe UI");
    logf("overlay: %ux%u swapchain format %d%s", W, H, (int)sc.fmt, swapRB ? " (RGBA: channels swapped on upload)" : "");
    return true;
}
static void text(HFONT f, int cx, int cy, const char *s, COLORREF c) {
    SelectObject(dc, f); SetTextColor(dc, c); SetBkMode(dc, TRANSPARENT);
    wchar_t w[64]; MultiByteToWideChar(CP_UTF8, 0, s, -1, w, 64); SIZE z{}; GetTextExtentPoint32W(dc, w, (int)wcslen(w), &z);
    TextOutW(dc, cx - z.cx / 2, cy - z.cy / 2, w, (int)wcslen(w));
}
// draw both halves; premultiplied alpha: text alpha from its brightness, panels at least 190
static void draw(u64 now) {
    memset(px, 0, (size_t)W * H * 4);
    const int cx = 256, cy = 256, R = 240;
    if (cur.wheelOpen) {
        HBRUSH bg = CreateSolidBrush(RGB(24, 24, 28)); SelectObject(dc, bg); SelectObject(dc, GetStockObject(NULL_PEN)); Ellipse(dc, cx - R, cy - R, cx + R, cy + R); DeleteObject(bg);
        const int n = (int)(cur.n > 8 ? 8 : cur.n < 2 ? 2 : cur.n); const float step = 360.0f / (float)n;
        const int rt = n > 4 ? 155 : 140;   // label radius (0.14.2: 5 options - Heal, Photo mode, Call horse, Play flute, Recenter view)
        for (int i = 0; i < n; i++) {   // option i at 90 - i*360/n degrees (screen y down), clockwise from the top
            const float a = (90.0f - i * step) / 57.2957795f; const int ox = cx + (int)(cosf(a) * rt), oy = cy - (int)(sinf(a) * rt);
            if (i == cur.sel) {   // the picked sector: a lighter wedge
                const float a0 = a - step * 0.5f / 57.2957795f, a1 = a + step * 0.5f / 57.2957795f;
                HBRUSH hb = CreateSolidBrush(RGB(150, 40, 40)); SelectObject(dc, hb);
                Pie(dc, cx - R, cy - R, cx + R, cy + R, cx + (int)(cosf(a0) * R), cy - (int)(sinf(a0) * R), cx + (int)(cosf(a1) * R), cy - (int)(sinf(a1) * R));   // GDI arcs run counter-clockwise from start to end DeleteObject(hb);
            }
            text(n > 4 ? fSmall : fMid, ox, oy, cur.names[i], i == cur.sel ? RGB(255, 255, 255) : RGB(200, 200, 200));
        }
        HBRUSH hub = CreateSolidBrush(RGB(60, 60, 66)); SelectObject(dc, hub); Ellipse(dc, cx - 50, cy - 50, cx + 50, cy + 50); DeleteObject(hub);
    }
    const int ix = HALF;
    if (cur.dbg) {   // the gesture indicator: two hand rows and the last gesture
        HBRUSH bg = CreateSolidBrush(RGB(20, 20, 24)); RECT r{(LONG)ix + 16, 16, (LONG)W - 16, (LONG)H - 16}; FillRect(dc, &r, bg); DeleteObject(bg);
        text(fMid, ix + 256, 60, "HEAD GESTURES (test)", RGB(170, 170, 170));
        for (int h = 0; h < 2; h++) {
            const bool on = (cur.atHead >> h) & 1; RECT b{(LONG)ix + 40, (LONG)(120 + h * 100), (LONG)W - 40, (LONG)(200 + h * 100)};
            HBRUSH hb = CreateSolidBrush(on ? RGB(30, 120, 50) : RGB(45, 45, 50)); FillRect(dc, &b, hb); DeleteObject(hb);
            text(fMid, ix + 256, 160 + h * 100, h ? (on ? "RIGHT hand at head" : "right hand") : (on ? "LEFT hand at head" : "left hand"), on ? RGB(255, 255, 255) : RGB(150, 150, 150));
        }
        for (int h = 0; h < 2; h++) if (now < flashUntil[h]) { text(fBig, ix + 256, 400, flashName[h], RGB(255, 210, 90)); break; }
    }
    for (u32 y = 0; y < H; y++) for (u32 x = 0; x < W; x++) {   // alpha: panels 190 (inside the wheel disc / the indicator box), text by brightness
        u32 &p = px[y * W + x]; const u32 b = p & 255, g = (p >> 8) & 255, r = (p >> 16) & 255; u32 a = r > g ? (r > b ? r : b) : (g > b ? g : b);
        bool panel = false;
        if (x < HALF) { if (cur.wheelOpen) { const int dx = (int)x - cx, dy = (int)y - cy; panel = dx * dx + dy * dy <= R * R; } }
        else if (cur.dbg) panel = x >= ix + 16 && x < W - 16 && y >= 16 && y < H - 16;
        if (panel && a < 215) a = 215;
        p = swapRB ? (a << 24) | (b << 16) | (g << 8) | r : (a << 24) | (r << 16) | (g << 8) | b;
    }
    nDraws++;
}
// once per host frame, before the frame's command list: read the state; true when the image must be redrawn this frame
static bool wantDraw(u64 now) {
    St s{}; bool got = gvRead(&S->ovSeq, [&] { s.wheelOpen = S->ovWheelOpen; s.sel = S->ovWheelSel; s.n = S->ovWheelN; s.hand = S->ovWheelHand; memcpy(s.names, S->ovWheelName, sizeof s.names);
        s.dbg = S->ovDebug; s.atHead = S->ovAtHead; s.gseq[0] = S->ovGestSeq[0]; s.gseq[1] = S->ovGestSeq[1]; memcpy(s.gname, S->ovGestName, sizeof s.gname); });
    if (!got) return false;
    for (int h = 0; h < 2; h++) if (s.gseq[h] != seenSeq[h]) { seenSeq[h] = s.gseq[h]; flashUntil[h] = now + 1500; memcpy(flashName[h], s.gname[h], 24); flashName[h][23] = 0; }
    cur = s;
    const bool flash = now < flashUntil[0] || now < flashUntil[1];
    const u64 sig = (u64)s.wheelOpen | ((u64)(s.sel + 1) << 1) | ((u64)s.dbg << 4) | ((u64)s.atHead << 5) | ((u64)(flash ? 1 : 0) << 7) | ((u64)(now < flashUntil[0]) << 8) | ((u64)(now < flashUntil[1]) << 9);
    if (!s.wheelOpen && !s.dbg) return false;
    if (!init()) return false;
    return sig != sigDrawn || !haveImage;
}
static u64 pendingSig = 0;
static void record(ID3D12GraphicsCommandList *c, ID3D12Resource *img, int ring, u64 now) {
    draw(now);
    void *m = nullptr; D3D12_RANGE rr{0, 0}; if (FAILED(up[ring]->Map(0, &rr, &m))) return;
    for (u32 y = 0; y < H; y++) memcpy((uint8_t *)m + fp.Offset + (u64)y * fp.Footprint.RowPitch, px + (size_t)y * W, (size_t)W * 4);
    up[ring]->Unmap(0, nullptr);
    D3D12_RESOURCE_BARRIER br = barrier(img, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST); c->ResourceBarrier(1, &br);
    D3D12_TEXTURE_COPY_LOCATION dst{img, D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, {}}, src{up[ring], D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT, {}}; src.PlacedFootprint = fp;
    c->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    br = barrier(img, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET); c->ResourceBarrier(1, &br);
}
static u64 sigOf(u64 now) { const bool flash = now < flashUntil[0] || now < flashUntil[1];
    return (u64)cur.wheelOpen | ((u64)(cur.sel + 1) << 1) | ((u64)cur.dbg << 4) | ((u64)cur.atHead << 5) | ((u64)(flash ? 1 : 0) << 7) | ((u64)(now < flashUntil[0]) << 8) | ((u64)(now < flashUntil[1]) << 9); }
// quad facing the viewer: +Z of the quad points from the quad to the head, +Y close to world up
static XrQuaternionf faceTo(const float *from, const float *to) {
    float z[3] = {to[0] - from[0], to[1] - from[1], to[2] - from[2]}; float l = sqrtf(z[0] * z[0] + z[1] * z[1] + z[2] * z[2]); if (l < 1e-4f) return {0, 0, 0, 1}; for (auto &v : z) v /= l;
    float x[3] = {z[2], 0, -z[0]}; l = sqrtf(x[0] * x[0] + x[2] * x[2]); if (l < 1e-4f) { x[0] = 1; x[2] = 0; l = 1; } x[0] /= l; x[2] /= l;   // up x z
    const float y[3] = {z[1] * x[2] - z[2] * x[1], z[2] * x[0] - z[0] * x[2], z[0] * x[1] - z[1] * x[0]};
    const float m00 = x[0], m11 = y[1], m22 = z[2], tr = m00 + m11 + m22; XrQuaternionf q;
    if (tr > 0) { const float s = sqrtf(tr + 1) * 2; q.w = 0.25f * s; q.x = (y[2] - z[1]) / s; q.y = (z[0] - x[2]) / s; q.z = (x[1] - y[0]) / s; }
    else if (m00 > m11 && m00 > m22) { const float s = sqrtf(1 + m00 - m11 - m22) * 2; q.w = (y[2] - z[1]) / s; q.x = 0.25f * s; q.y = (y[0] + x[1]) / s; q.z = (z[0] + x[2]) / s; }
    else if (m11 > m22) { const float s = sqrtf(1 + m11 - m00 - m22) * 2; q.w = (z[0] - x[2]) / s; q.x = (y[0] + x[1]) / s; q.y = 0.25f * s; q.z = (z[1] + y[2]) / s; }
    else { const float s = sqrtf(1 + m22 - m00 - m11) * 2; q.w = (x[1] - y[0]) / s; q.x = (z[0] + x[2]) / s; q.y = (z[1] + y[2]) / s; q.z = 0.25f * s; }
    return q;
}
} // namespace ov
