# GoTVR architecture (design doc; revision 3 after the GoTVR 0.5 live run, 2026-10-17)

Revision 3 changes (evidence: BUGLOG.md BUG-5/BUG-6): (1) **the OpenXR session lives in `GoTVRHost.exe`**,
not in the game process; the game shares eye images through D3D12 shared textures + a shared fence and
receives poses through a shared-memory block (`src/gotvr_shared.h`); (2) **one projection layer every
frame**, never zero; (3) frames are tagged by the engine's **Reflex frame id** (ADDR-061 at the camera hook,
ADDR-062 at Present), so each image is declared with the exact eye pose and frustum it was rendered with;
(4) the per-eye projection seam (+0x200) is `[LIVE]` (F8 PASS); (5) first stereo = alternate-eye (R3), the
bring-up rung before R1.

## Process layout (revision 3)

```
GhostOfTsushima.exe                                   GoTVRHost.exe (started by the DLL, exits with the game)
  xinput1_4.dll (GoTVR)                                 own D3D12 device on the runtime's adapter (LUID)
    camera hook (slot 43, FrameView pre-snapshot)       OpenXR session, LOCAL + VIEW spaces
      packet[frame id] = eye, pose, fov  <───────────── pose block: head + 2 eye poses + fovs (predicted display time)
    Present hook (native present queue)                 per eye: CPU-check shared fence ≥ slot.fence,
      copy back buffer → shared tex[eye][3 bufs]  ────>   copy tex → swapchain image, declare slot pose/fov
      signal shared fence, publish slot[eye]            always 1 projection layer (black until images arrive)
```
No GPU wait crosses the process boundary in either direction.

Revision 2 changes (evidence: BUGLOG.md, LIVE_RESULTS.md): (1) all OpenXR work moves to a **dedicated XR
thread with its own D3D12 queue** (0.4 crashed doing it on the Present thread); (2) the per-eye projection
seam is **+0x200 in slot 43**, not `Cam_SetProjRect` (never called for the render camera, `projrect(camA)=0`);
(3) every camera/projection write is **idempotent** (recomposed from a cached game value; slot 43 runs ~5–6×
per frame); (4) the render pose seam is `[LIVE]` (F9 turned the view, not Jin/HUD/aim); (5) motion blur and
frame generation are required off; the Present-descriptor HUD/UI/depth resources are null with FG off.

Target: Quest 3 (Touch Plus) via **VDXR** (Virtual Desktop's OpenXR runtime), **roomscale 6DoF**; must also
run on **SteamVR OpenXR**. One OpenXR D3D12 code path covers both; never call runtime-specific APIs.
Every component below names the RE anchor it stands on and the experiment that must pass first.

## Components

```
xinput1_4.dll proxy (loader + input)            ── ADDR-053 pad loader, ADDR-036..039 pad funnel
  ├─ XR thread (16 MB stack) owns ALL OpenXR    ── BUG-4; never call OpenXR on a game thread
  │   calls: instance, session, wait/begin/end
  │   frame, locate; keeps the headset alive
  │   during loads and stalls
  ├─ XR D3D12 queue (GoTVR-owned DIRECT queue)  ── ADDR-042 device chain, LUID check; session bound to it
  ├─ pose packet (seqlock, one per xrWaitFrame) ── playbook a2; head pose live in 0.5 (xr=3)
  ├─ camera hook: slot 43 (0x807D10), CAM-A     ── ADDR-023/024/026; render pose seam [LIVE]
  │   world +0x140 (pose) and proj +0x200 (FOV)
  ├─ eye images: copy on the XR queue, fenced   ── swapchain queue via GetDevice(ID3D12CommandQueue)
  │   both ways with the swapchain's own queue
  ├─ UI layer: UIColorAndAlpha → quad layer     ── ADDR-044 (desc +0x38) null with FG off → [OPEN]
  ├─ input: OpenXR actions → XINPUT_STATE       ── ADDR-036 poll semantics
  └─ haptics: XInputSetState → xrApplyHapticFeedback ── ADDR-037
```

## Camera / 6DoF composition

Hook point: **vslot 43 (`Cam_vUpdateFrustum` 0x807D10)**, a MinHook detour on the function (shared by both
camera vtables), acting only when `this == *(base+0x1EDBCC0)` (CAM-A). At that moment the
camera's world matrix (+0x140..+0x17F) holds the game's final pose for this frame (all driver modes, all
parents applied). Steps:

1. Save `G = world` (game camera pose) into the pose packet → used for locomotion basis, aim and audio.
2. Build the anchor frame `A`:
   - third-person "comfort" mode: `A = G` translation, yaw-only rotation of `G` (pitch/roll removed);
   - **first-person** mode: `A` = hero head-joint position (skeleton joint array, ADDR-030 format) +
     body yaw (VR-owned).
3. `C_eye = A · R_basis · (recenter⁻¹ · xrView[eye].pose) · units`. `R_basis` maps OpenXR (RH, Y-up,
   −Z forward, metres) onto the engine rows (fwd, left, up, translation). The engine's up axis and units/metre
   are `[OPEN]` until EXP-02 logs the basis while walking/turning.
4. Write `C_eye` into +0x140..+0x17F (rows fwd, left, up, pos) and call the original slot 43. The eye's
   four FOV tangents go into the **projection at +0x200** before the original runs (it bakes +0x200 into the
   view-projection): `P00 = 2/(r−l)`, `P20 = −s(r+l)/(r−l)`, `P11 = 2/(u−d)`, `P21 = −s(u+d)/(u−d)` with
   `s = P23` (±1, depth sign) — first exercised by F8 in 0.5. The cull rect is widened to the union of both eyes.
   **Idempotency rule:** slot 43 runs ~5–6× per frame and the game often does not rewrite the camera between
   calls, so the hook keeps the game's values (world, projection) and recomposes from them on every call;
   it re-captures them when the live value differs from what GoTVR last wrote, and restores them when off.
   `R_basis` (0.5, verified numerically): game local (f, l, u) ↔ OpenXR (−z, −x, +y); Z-up world `[LIVE]`;
   units 100/m assumed (`units_per_m`, confirmed or corrected by the xr=3 walk test).
5. AFR (R3): one eye per frame, alternating, with the pair generation in the pose packet. R1: both eyes per
   frame once the frame-build entry is found.

Bring-up ladder (0.5 → R1): xr=2 known-colour stereo → **xr=3 head-tracked mono** (head pose → render
camera, game image on a head-locked screen; proves basis, units, latency and the "game pose untouched"
rule) → R1 second RenderView with the other eye's pose (STEREO_ROUTE.md) → first-person head-joint anchor.

Restore: gameplay reads that happen after slot 43 and need the game pose (aim, interaction) must read `G`
from the packet. The consumer census (OWNERSHIP_CENSUS.md) decides which consumers that is.

## Locomotion and turning

- The game moves the hero relative to its own camera yaw. In 6DoF the player expects movement relative to
  the head (or the off-hand). The proxy rotates the left-stick vector by `(yaw(G) − yaw(head))` before
  writing `sThumbLX/LY`. This is a pure input-side fix, so the game state is not touched.
- Right stick X becomes VR snap/smooth turn and updates **body yaw** (anchor rotation). It is still forwarded
  to the game in menus and photo mode.
- Horse: same scheme; the anchor follows the horse-mounted hero.

## UI

- Gameplay HUD: the UIColorAndAlpha resource goes onto an `XrCompositionLayerQuad` 1.5–2 m ahead,
  body-locked (yaw only) with a recenter action. Fallback if the resource is absent with frame generation
  off: HUD-less versus final difference, or a flat virtual screen.
- Menus, map, loading, pre-rendered movies (`movies/*.webm`): full flat image on a world-locked quad;
  6DoF scene rendering is suspended (stereo off) while the game reports a menu or movie state (`[OPEN]`:
  game-state flag; NxGameStateImpl, service +0xC8, is the lead).

## Motion controls ladder

| Tier | Feature | Native owner needed | Status |
|---|---|---|---|
| T1 | 6DoF head, pad-equivalent Touch input, UI quad | camera seam, XInput funnel, UI layer | seams found `[STATIC]` |
| T2 | head-relative locomotion, snap turn, comfort vignette | stick rotation in proxy | design ready |
| T3 | bow aimed by hands (L hand = bow, R hand draw), throwables aimed by hand | aim ray producer `[OPEN]` | leads in INPUT_OWNERSHIP.md |
| T3 | visible hands / katana following the controller | hero skeleton joints (+0x278 array) + IK `[OPEN]` | joint array format known |
| T4 | gesture swings mapped to light/heavy/stance attacks | attack/stance endpoints `[OPEN]` | `StanceTrainer`, `CApJointWarp` leads |

## Comfort / safety

- Camera cuts (+0x7D8) and cutscenes: option for 3DoF (fixed position, head rotation only) or a "cinema"
  flat screen.
- Disable camera shake (`Shake`/`ProtoShake`), motion blur (setting `MotionBlur`) and depth of field in VR.
  Motion blur is confirmed harmful `[LIVE]` (it smeared the distance under a camera change, BUG-3); frame
  generation (DLSS-G/FSR-FG) must be off (it interpolates one flat view).
- Never run in Legends (online). The proxy refuses to arm if the online mode is detected (`[OPEN]` flag).
- Unknown build hash → proxy stays a pure XInput passthrough (fail closed). Implemented since probe 0.1 and re-tested
  for every build (receipts/gotvr05_failclosed.log).
