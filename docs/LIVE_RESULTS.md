# Live results

## EXP-00 run 2 — 2026-10-17 16:56, owner's PC (probe v0.3, `xinput1_4.dll`, read-only)

Raw log: `receipts/EXP-00_run2_load.log` (80 s of samples). Observation environment: in-process passive
reads from a background thread (no hooks). Run validity: **VALID** (identity line, hash MATCH, 17/17
signatures).

| Claim | Verdict | Evidence (log) | Grade |
|---|---|---|---|
| Our bytes run in the game process at startup via `xinput1_4.dll` | **confirmed** | IDENTITY line, `dll=…\xinput1_4.dll`, module loads at t=0 | `[LIVE]` |
| Build identity | confirmed | SHA-256 MATCH, PE timestamp expected, 17/17 signatures PASS | `[LIVE]` |
| d3d12 / D3D12Core / dxgi / sl.interposer / FSR / XeSS loaded | confirmed | `module … loaded` lines | `[LIVE]` |
| CAM-A and CAM-B are **the same object**, class **0x11C** (base vtable 0x1143850), no parent | confirmed | `camA == camB`, `class 0x11C`, `parent=0x0` | `[LIVE]` |
| The camera object is **re-created** on world load (ptr changed at t≈41 s) | observed | `camA ptr=0x5c3a6013c00` → `0x5c3c7c16400` | `[LIVE]` (hooks must re-resolve the pointer every frame) |
| Basis rows orthonormal, right-handed (fwd × left = up), det = +1 | confirmed | 15/15 samples, max |dot| ≤ 1e-5 | `[LIVE]` |
| World up axis = **+Z** | confirmed in gameplay | up = (0, 0, 1.0) at t ≥ 45 | `[LIVE]` |
| +0x670 is the **vertical** FOV in radians; `2·atan(1/P11)` = +0x670; `P11/P00` = aspect (1.6) | confirmed | all samples | `[LIVE]` |
| Reversed-Z with near 10, far 1e7 (P22 ≈ −0, P32 = 10 = near) | consistent | P22=−0.0, P32=10 | `[LIVE]` |
| FOV changes with context (57.3° menu/default, 45.2°, 53.6° gameplay) | observed | t=40/45/50 | `[LIVE]` |
| Projection rect +0x694 and off-center rect +0x6A4 stay (−1,−1,1,1) in this sampling | observed | 5 s samples miss per-frame jitter writes, or TAA jitter is off at this setting `[OPEN]` | `[LIVE]` |
| D3D12 chain: apiMode 3, driver vtable NxD3D12Driver, 1 node, queue0 Type 0 DIRECT, device LUID 0x0:0x13AD2 | **confirmed** | `d3d …` lines | `[LIVE]` |
| Units | **open** | the player did not move during the sampled window (t=50..80 constant). Near = 10 and coordinates of ~10⁵ suggest **centimetres** | `[INFERENCE]` |

Consequences:
- Camera guard: accept vtable 0x1143850 (class 0x11C). The 0x1183C60 class is not the gameplay render
  camera on this run.
- Basis conversion OpenXR → engine (both right-handed): OpenXR (+X right, +Y up, −Z forward) maps to
  engine rows as `fwd = −Z_xr`, `left = −X_xr`, `up = +Y_xr`; translation scaled by units/metre (100 if cm).
- OpenXR binding: `XrGraphicsBindingD3D12KHR{device, queue0}` from the chain; the runtime's
  `adapterLuid` must equal 0x0:0x13AD2 on this PC (VDXR and SteamVR report the adapter they drive).

Bottlenecks: BASELINE/LOAD **cleared** (exit proof: identity line in the intended process). Next earliest:
OBSERVE FRAME (EXP-01), then CAMERA mutation (EXP-03b).

## GoTVR 0.4 runs — 2026-10-17, owner's PC (`receipts/gotvr04_runA.log`, `receipts/gotvr04_xr1_crash.log`)

Guards: SHA MATCH, 8/8 signatures, 6/6 hooks `MH_OK`, no hook-caused instability over the xr=0 run.

| Gate | Result | Status |
|---|---|---|
| EXP-01 frame census | tick, FrameView and Present on separate threads; slot43(camA) ≈ 5–6 × present | `[LIVE]` |
| EXP-07 camera writers | `setlocal(camA)` often 0 for long stretches while slot 43 keeps running → the camera matrix persists across frames; render-camera writers 0x673FC1 (hero cam 0x673610), 0xA38841, 0x916C4C | `[LIVE]` |
| View census (R1) | menu: 1 view; gameplay: **5 views, 5 distinct cameras**, `mode80f09=2`, secondary registry 4 → the frame already renders several RenderViews with their own cameras | `[LIVE]` |
| EXP-06 Present descriptor | all four tagged resources **null** with frame generation off | `[LIVE]` (UI layer needs another source) |
| EXP-08 basis | Z-up confirmed again (fwd z ≈ −0.08…−0.24 looking slightly down), world positions ~1e5 → centimetres likely | `[LIVE]` / units `[OPEN]` |
| F7 zero-delta control | no visible change (expected) | PASS |
| F8 SetProjRect left half | no visible change: `projrect(camA)=0` all session → seam dead | FAIL → NEG-010 |
| F9 render yaw | view turns, Jin/HUD/aim do not → **render pose seam confirmed**; but cumulative (≈35°) and smeared by motion blur | PASS with bug (BUG-3) |
| PORT-08 xr=1 | VDXR 1.1.0, LUID match, swapchain created, session FOCUSED, frames submitted, then crash ~1 s in: stack overflow in `nvgpucomp64.dll` | FAIL → NEG-011, BUG-4 |

## GoTVR 0.6.2 runs — 2026-10-17 19:40 (`receipts/gotvr062_xr1.log`, `receipts/gotvr062_xr2.log`)
- xr=1 mono 6DoF and xr=2 AFR stereo **work** in the headset `[LIVE]`. Presents 1.00/frame after ignoring
  DXGI_PRESENT_TEST; frame-id mismatches ≈0; game 110–360 fps on the owner's RTX 5090 Laptop.
- Owner: xr=2 shows "a lot of flickering and ghosting" → explained below (shared temporal history).
- **R1 census** (F10 in menu / outdoors / riding / cutscene / photo mode):

| View | Camera class (vt) | kind (+0x660) | RV_Submit mode | Frustum | Position | Verdict |
|---|---|---|---|---|---|---|
| main (frame+0x1ED70) | 0x1143850 (CAM-A) | 0 | 3 | eye frustum (GoTVR) | player camera | the only scene view |
| secondary registry (4–6) | 0x112B0F8 | 1 | **0** (no gather) | tan 0.683/0.546 | (0,0,0) or one ~38 m away | proxies, not scene renders |
| list2 (1) | 0x11439C0 | 2 | 3 | tan 0.683/0.546 | (0,0,0) | 3-level 6×6 tile-cache updater (`List2_Build_6319C0`) |
| menu | only the main view | | | | | |

→ **No full-quality secondary view exists** (playbook ch17 "proof not vehicle"). The second eye must be a second
run of the main view's render graph.
