# Camera findings (RE-05/06, CPU-first, static)

Build: `GhostOfTsushima.exe` SHA-256 `ED0931B6…7109F63` (PE timestamp 1782230973). Addresses are RVAs
(VA − 0x140000000). Session 2 (2026-10-17), static only; nothing here is `[LIVE]` yet.
Raw receipts: `re/decomp/*.c` (Ghidra 11.4.2 on-demand decompiles), `re/receipts/*.asm`,
`re/out/signatures_v2.json`. Session-1 findings (Streamline oracle, field layout) are kept and extended.

## Summary: one camera pipeline, one choke point

The render camera is a **scene-graph node**. Every camera mode the game has (follow, fixed, interpolated,
attached/animated, manual) delivers its pose through one non-virtual setter. The world matrix,
view-projection, frustum and culling planes are all derived from that one matrix.

```
camera driver (per mode)             CamDrv_*  (0xA32DF0 attached, 0xA33360 follow, 0xA332D0 fixed,
   pose = pos + quaternion (+FOV)               0xA386B0 interpolated, 0xA33720 manual/pad, 0xA34A80 path)
        |                                        + 0x641C20, 0x673610, 0x916740, 0x5A0460 (other users)
        v
Cam_SetLocalMatrix  0x806E50   local 4x4  -> +0x100..+0x13F   (backup copy -> +0x890..+0x8CF)
        |  (tail jump)
        v
Cam_UpdateWorld     0x8081F0   world = M x local (row-vector). M = identity if parent(+0x730)+0x2AD != 0
                               or this+0x660 != 0; else M from the transform hierarchy 0xA17AA0(&g_0x1F3E138,
                               this), combined with joint matrix 0x66C760(parent) when parent class == 0xD6,
                               and 0xA181A0 (identity unless this == CAM-A: render-camera special case)
                               -> +0x140..+0x17F (rows: fwd, left, up, translation), copy -> +0x850
        |  vcall slot 43 (+0x158)
        v
Cam_vUpdateFrustum  0x807D10   view rotation from rows (fwd, -left, up) x projection(+0x200)
        |
        v
Cam_SetViewProjFrustum 0x806BA0  pos -> +0x550 ; rotation-only viewProj -> +0x430..+0x46F ; 0x977360 -> +0x4B0
                                 (inverse) ; viewProj WITH translation -> +0x240..+0x27F ; then, only if
                                 byte 0x1434C2DD0 == 0 (the flag 0x5A0460 toggles), off-center rect (+0x6A4) ->
                                 Cam_BuildCullFrustum 0x803BD0 -> +0x4F0
Cam_SetFov 0x806F50 -> +0x670/+0x680/+0x684 -> vslot 42 Cam_vUpdateProjection 0x807F30 -> +0x200
Cam_SetProjRect 0x8068E0 -> +0x694 rect (l,b,r,t) -> vslot 42 (P = P_sym x RectToMatrix(rect)) -> vslot 43
Cam_ResetProjRect 0x806200 -> rect (-1,-1,1,1) -> vslot 42 -> vslot 43
```

Grade: `[STATIC]` for every arrow (instruction-level, Ghidra decompile cross-checked against llvm-objdump).
That this is the **player** render camera is `[INFERENCE]` until EXP-02 runs.

## Key facts for VR

| Fact | Consequence for the mod | Evidence |
|---|---|---|
| **Camera-relative rendering**: viewProj at +0x430 is built from the rotation rows only; translation is carried separately (+0x550 and +0x170) | An eye offset is a pure translation of +0x170 (then re-run slot 43); no matrix surgery on shader constants | 0x807D10 builds rows 0..2 from +0x140/+0x150/+0x160, row 3 = (0,0,0,1); passes +0x170 as position `[STATIC]` |
| **Reversed-Z, D3D [0,1], row-vector**: `P = diag(1/tanH, 1/tanV) ; z' = z·(−n/(f−n)) + n·f/(f−n) ; w' = z` → near→1, far→0 | OpenXR projection must be reversed-Z with the same near/far; use the engine's own near/far | 0x807F30 decompile; Streamline `depthInverted = true` (byte 0x01 at the sl::Constants Boolean block in 0x807140) `[STATIC]+[SPEC]` |
| **Native asymmetric projection**: slot 42 multiplies the symmetric base by `RectToMatrix(+0x694 rect)` (0x8049E0: maps NDC window [l,r]×[b,t] onto [−1,1]). The game writes this rect every frame **for TAA jitter** (callers 0x618D30, 0xAF9850 build (−1,−1,1,1) shifted by jitter/resolution) through `Cam_SetProjRect` 0x8068E0, which then rebuilds projection and frustum | Per-eye OpenXR FOV = hook `Cam_SetProjRect` for CAM-A: `rect_eye = (tanL/tanH, tanD/tanV, tanR/tanH, tanU/tanV)` relative to the current symmetric tangents (+0x680/+0x684), shifted by the game's jitter offset. Culling (+0x6A4 off-center rect) is separate: widen it to the eye union. Caveat: a global mode flag (0x1419F2400 helpers 0x4D5060/0x4D5070) selects an alternate branch that only uses r−1/t−1 `[OPEN]` | 0x807F30, 0x8049E0, 0x8068E0 decompiles `[STATIC]`, verifier C4 |
| **Single write choke point**: 11 call sites of 0x806E50 (7 in the 0xA3xxxx driver cluster, 4 elsewhere, all camera-mode code) | Hook `Cam_SetLocalMatrix` (or post-`Cam_UpdateWorld`) **filtered to the render camera** = 6DoF for every mode (gameplay, cutscene, horse, photo) | caller census below `[STATIC]` |
| Cameras can be **parented to skinned objects' joints** (`+0x730`; joint matrix = `(joint+1)*0x40 + obj[+0x278]`) | Native first-person route: attach/compose the render camera onto the hero's head joint; same joint array gives hand/wrist transforms for motion controls | 0xA32DF0 (`iVar3 - 10U < 0xd2` class-range test then joint array read) `[STATIC]` |
| Jitter enters as the +0x694 projection-rect offset; TAA history via clipToPrevClip (+0x2C0) | Per-eye temporal history is mandatory (or AFR history banking) | 0x618D30/0xAF9850 + 0x807F30 `[STATIC]` |
| Camera-cut / reset flag `+0x7D8` feeds `sl::Constants::reset` | Reset per-eye TAA/upscaler history on cuts; also a cutscene/teleport signal for comfort | 0x807140, 0x673610 `[STATIC]` |
| Typed object model: vtable slot 3 = class descriptor, slot 4 = class id (`mov eax, imm; ret`) | Cheap, robust runtime guards without calling game code (decode the immediate) | 0x812BA0 → 0x11F, 0x815D60 → 0x11C `[STATIC]` |

## Camera classes

| Class id | vtable RVA | Size | Constructed by | Notes |
|---|---|---|---|---|
| 0x11C | 0x1143850 (44 slots; the next qwords start the +0xF0 sub-object vtable 0x11439B0) | — | `Cam_ctor_base` 0x801320 (factory 0x824FC0) | base render camera; slots 0..41 are generic scene-node methods |
| 0x11F (derives 0x11C) | 0x1183C60 (44 slots) | 0x920 | factory 0x825100 → 0x801320, then vtable overwrite, `+0x6B6 = 0` | session-1 "CAM-A vtable"; **probe v0.1 only accepted this one** |

Class registry: `0x17B73C0 + id*0xA0` (descriptors filled by a large static initialiser at 0x5EC40..).
Descriptor for 0x11F at 0x17C2720 (parent desc = entry 0x11C, factory 0x825100) `[STATIC]`.

## Field layout additions (session 2)

| Offset | Meaning | Evidence |
|---|---|---|
| +0x100..+0x13F | **local** 4x4 (rows fwd, left, up, translation) | 0x806E50 writes, 0x8081F0 reads |
| +0x140..+0x17F | **world** 4x4 = local × parent | 0x8081F0 |
| +0x240..+0x27F | view-projection **including** translation | 0x806BA0 (verifier C5) |
| +0x430..+0x46F | view-projection (rotation-only view × P) | 0x806BA0 |
| +0x4B0.. | inverse view-projection | 0x806BA0 (`0x977360` inverse) |
| +0x4F0.. | culling frustum built with off-center rect | 0x803BD0 |
| +0x550 | world position (= +0x170 xyz, w=1) | 0x806BA0 |
| +0x660 | non-zero → world ignores the parent (identity M) | 0x808245 |
| +0x66C | 1 + sqrt(1+aspect²)·tanV·near (written by slot 42) | 0x807F30 |
| +0x694..+0x6A3 | **projection sub-rect** (l,b,r,t), default (−1,−1,1,1); carries TAA jitter (session 1 called it "jitter") | 0x801320, 0x8068E0, 0x807F30 |
| +0x730 | parent object (attachment); camera drivers read joint matrices when the parent class is in 10..0xDB; UpdateWorld special-cases class 0xD6 | 0x8081F0, 0xA32DF0 |
| +0x770/+0x771 | flag computed from viewProj sign test (mirror/handedness?) | 0x806BA0 `[INFERENCE]` |
| +0x7D8 | camera-cut/reset state (1, 2 seen) | 0x807140, 0x673610 |
| +0x850..+0x88F | copy of world | 0x8081F0 |
| +0x890..+0x8CF | **backup of last local** (restored by 0x5A0460 on a mode toggle) | 0x806E50, 0x5A0460 |

Session-1 rows (+0x200 projection, +0x2C0 clipToPrevClip, +0x664 near, +0x668 far, +0x670 FOV rad,
+0x67C aspect, +0x680/+0x684 half-tangents, +0x6A4..+0x6B3 off-center, +0x6B7 off-center enable) are
re-confirmed by the decompiles. **Correction:** session 1's "+0x694 jitter input" is the projection
sub-rect; jitter is only its per-frame use.

## Two camera globals

| Global | Readers | Writer | Observed role |
|---|---|---|---|
| `0x1EDBCC0` (CAM-A) | ≥241 loads | none static (NEG-001, re-run with ±4 KB window and absolute-pointer scan: still none) | Streamline/SL constants source, render passes |
| `0x1EDBCC8` (CAM-B) | 32 loads | none static | 0x5A0460 restores its local from +0x890 on a toggle (photo-mode style); 0x641C20 reads its backup translation |

Whether A and B are the same object, and which is the gameplay vs. render camera, is the first thing
probe v0.2 logs (`camA == camB`, class id, parent).

## Camera-driver census (writers of `Cam_SetLocalMatrix`)

| Caller (primary fn) | Mode (string/structure evidence) | Camera reached via | Also sets FOV |
|---|---|---|---|
| 0xA32DF0 | attached / animated pose: evaluates pose via 0xA312B0, composes with joint matrix of attachment | `[this+0x10]`, guarded by `cam+0x730 == this-0x290` | yes (`Cam_SetFov`) |
| 0xA33360 | **follow**: references "Camera following object" / "Camera following <NOTHING>" (refcounted string objects at 0x12167D8/0x12167F8; text at +4); calls SetLocalMatrix only when a followed object is set (`[this+0x20] != 0`) | `[this+0x10]` | no |
| 0xA332D0 | fixed: copies stored matrix (this−0x140) and FOV (this+0x20) | `[this+0x10]` | yes |
| 0xA386B0 | interpolated: lerps position/rotation, guard `cam+0x730 == this` | `[this+0x2A0]` | yes |
| 0xA33720 | manual/pad camera: reads input axes 9/10 and action ids 0xA3..0xAE | via 0xA34A80 | — |
| 0xA34A80 | path / spline step loop | `[this+0x10]` | — |
| 0x673610 | heavy gameplay camera (many sub-solvers 0x677210..0x679310; reads cut state +0x7D8) — **best candidate for the third-person hero camera** `[INFERENCE]` | `[this+0x10]` | — |
| 0x641C20 | reads CAM-B backup (+0x8C0) | CAM-B | — |
| 0x916740 | object follow with `0x141EDDCC0` system | — | — |
| 0x5A0460 | toggle restore: `SetLocalMatrix(camB, camB+0x890)` | CAM-B | — |

Type-name tables (`.data` 0x17A30A8, 193 entries) name the modes: CAMERA, MANUAL_CAMERA, ANIMATED_CAMERA,
INTERPOLATED_CAMERA, FIXED_CAMERA, INTEGRATED_CAMERA, MOUSE_CAMERA, DIRECT_CAMERA, FOLLOW_CAMERA,
CAMERA_DRIVER (index 44), plus CameraCut, Shake/ProtoShake, CombatLockOn, UseLockedOnCamera. Mapping each
table index to a driver function is `[OPEN]`.

## Camera roles

| Role | Owner | Authored when | Consumers | Safe per-eye seam | Evidence |
|---|---|---|---|---|---|
| Game/body camera | camera driver of the active mode (0x673610 candidate for gameplay) | game tick (driver update) | aim/reticle (`[OPEN]`), audio listener (Wwise `SetPosition`), culling | **read** the driver pose before HMD composition (keep as gameplay basis) | `[STATIC]` |
| Player render camera | CAM-A (class 0x11F/0x11C) | each frame, via 0x806E50 → 0x8081F0 | ≥215 fns; Streamline; render passes | compose HMD in `Cam_SetLocalMatrix` hook (render camera only), per-eye offset on +0x170 then re-run slot 43 | `[STATIC]` |
| Cull camera/frustum | same object: +0x4F0 off-center cull frustum (from +0x6A4 rect); +0x240 viewProj with translation | slot 43 | culling | union frustum for both eyes via `SetOffCenter` widened rect | `[STATIC]` |
| Viewmodel/foreground | none (third person; hero body is world geometry) | | | | `[INFERENCE]` |
| Scripted/cutscene | ANIMATED/INTERPOLATED/FIXED drivers + CameraCut | cutscene system | same path | same choke point; comfort policy decides 3DoF vs 6DoF | `[STATIC]` |
| Secondary views | photo mode (0x59C620 tiled capture, 0x5A0460 toggle), map, shadows/reflections `[OPEN]` | | | exclude by camera identity (CAM-A only) | `[STATIC]`/`[OPEN]` |

## Rejected / open

- NEG-001 reopened and re-confirmed (see FAILURE_REGISTRY NEG-005): the global is written through a
  computed pointer. Not needed any more: the camera **object's** writer is known (0x806E50).
- GPU-first cross-check (RenderDoc) still not run (EXP-05). It remains the independent second method.
- Aim ray producer (bow/throwables) still `[OPEN]`; leads: `CAnimRelativeAim`, RangedAim press/hold/release,
  BowAimed/LongbowAimed states, `ReticleBow`, `aim_assist`.
