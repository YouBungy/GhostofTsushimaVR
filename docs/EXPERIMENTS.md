# Next proofs (pre-registered). Write the decision rule before running; never after seeing C.

All of these need the game running with a small, read-mostly probe DLL (or Cheat Engine/Frida). Run them in
single-player, offline, never in Legends. Each experiment needs: vanilla control A, armed zero-delta B,
single-variable C, rollback.

## EXP-00 — RE-03 load proof (prerequisite for everything)

- Question: can our bytes run in this process deterministically?
- Method: proxy `xinput1_4.dll` in the game folder (the pad loader 0x497F30 tries it first with plain `LoadLibraryW` at NxInput init; corrected 2026-10-17, see NEG-009; no
  `SetDefaultDllDirectories` import), forwarding all exports to System32. First log line: build id, PID,
  exe SHA-256, module base, `d3d12.dll`/`dxgi.dll`/`sl.interposer.dll` loaded yes/no.
- Confirm: identity line present on cold launch with the proxy; absent with the proxy removed (control).
- Ambiguous: game reaches the menu but no log, because XInput may load late (only on first controller poll).
  Fall back to a `version.dll`/`winhttp.dll` proxy or an injector.
- Exit: `LOAD_PROOF.log` per the RE-03 checklist.

### Session-2 note: probe v0.2 covers EXP-00 plus the read-only part of EXP-02 and the D3D chain

Install `probe/xinput1_4.dll` (v0.3; v0.2 was named XInput9_1_0.dll and never loaded, NEG-009). Besides the v0.1 identity/signature
lines it logs: CAM-A **and** CAM-B (either camera class accepted), class id, parent and parent class,
`camA == camB`, cut state, local vs. world translation; and once, after d3d12.dll loads, the D3D12 chain
(`apiMode`, driver vtable, device, queue0, `GetNodeCount`, `GetAdapterLuid`, `queue0.GetDesc().Type`).
Decision rules:
- chain accepted when `apiMode=3`, `driver_vtable=NxD3D12Driver`, `queue0.Desc Type=0 (DIRECT)`, LUID non-zero;
- CAM-A identity accepted when its class is 0x11F or 0x11C and its pos550 tracks walking linearly;
- if `camA == camB` is never logged, record which one moves with the player and which with photo mode.

## EXP-01 — RE-04 frame boundary census

- Read-only detours on: game tick 0x4C49E0, render submit 0x509FDB, Present wrapper 0x434CC0, and
  `slSetConstants` (IAT 0x10EB8B8).
- Log per call: thread id, call index, QPC time, Reflex frame id (from the `+0xC0` token call), phase.
- Run menu → loading → gameplay → pause → photo mode → gameplay → cutscene.
- Decision: the boundary is accepted when Present fires once per displayed frame in gameplay, the tick and
  submit thread ids are stable, and the A/B cadence difference (probe vs. no probe) is inside the noise floor.
- Also record: does `slSetConstants` fire with DLSS off / FSR / XeSS / native AA? (Decides whether CAM-A's
  Streamline oracle is always available.)

## EXP-02 — RE-05 camera ownership (CAM-A)

- A: read `*(base+0x1EDBCC0)`; check `*(u64*)obj == base+0x1183C60` (vtable guard).
- Controlled motion: walk straight 5 s, then rotate right 90°, then pitch, with a static save.
- Metrics: +0x550 position tracks translation linearly; +0x140/+0x150/+0x160 rows stay orthonormal
  (|len−1|<1e-3, |dot|<1e-3, det≈+1); FOV +0x670 matches the in-game FOV setting; `2·atan(1/P11)` from
  +0x200 matches +0x670 within 1e-3 rad.
- Also: hardware write breakpoint on `base+0x1EDBCC0` from launch to find the **writer** (OPEN in static)
  and the object's lifetime across level load / fast travel / cutscene.
- Decision: CAM-A is the player render camera if all of the above hold *and* EXP-03 moves pixels.

## EXP-03 — RE-08 first reversible mutation: native off-center (decides rung 7)

- Variable: call `Camera::SetOffCenter(obj, {-1,-1,0,1})` (left half) for exactly 60 frames from the Present
  hook, then `ResetOffCenter`.
- A: stock. B: armed, call `SetOffCenter(obj,{-1,-1,1,1})` (identity rect = zero delta). C: left-half rect.
- Confirm: C shows the left half of the view stretched to full screen with correct geometry, shadows,
  SSR, AO and fog (no seams), and culling does not drop right-edge geometry; B is identical to A.
- Refute: C changes nothing (rect only used by a capture path), or geometry moves but screen-space passes
  stay full-frame (rect consumed by only some passes; record which).
- Then repeat with an asymmetric HMD-like rect such as (−1.1, −1.0, 0.9, 1.05) and compare against A3's
  projection residual tests.

## EXP-04 — dormant Nixxes stereo (decides whether a "free" stereo path exists)

- Variable: write `1` (side-by-side) to the live settings block `+0xBC` (block = NxSettingsImpl+0x48+idx·0x148,
  idx from vslot 19), then trigger the game's settings-apply path (any graphics option change).
- Confirm: output becomes SBS with **different** per-eye geometry (disparity grows with depth). Inverting
  separation `+0xC0` reverses it.
- Refute: no visual change, or both halves identical (mono duplicate).
- Note: also try mode 3 after NOPing the validator's `mode==3 → 0` reset at 0x428688 (reversible patch).
  Watch for "VR change caused swapchain recreate" in the Nixxes log.
- Either outcome is valuable. Refuted means the stereo fields are framework residue and we build stereo
  ourselves (STEREO_ROUTE.md).

## EXP-05 — GPU-first cross-check (independent discovery method)

- RenderDoc capture (if Streamline allows; else launch with `-noStreamline` and DLSS off), gameplay frame
  with a known FOV. Find the world-geometry constant buffer holding view/projection. Search those exact
  floats in memory and confirm they derive from CAM-A (+0x200 projection, basis rows).
- Also yields the first `RENDER_PASS_CENSUS.md` rows (pass count, queues, aliasing).

## EXP-06 — Present descriptor contents (UI-layer viability)

- Question: are desc+0x28 (HUD-less colour), desc+0x38 (UI colour+alpha), desc+0x18 (depth) and desc+0x08
  (motion vectors) valid D3D12 resources every frame, with frame generation **off**?
- Method: read-only detour on `NxDXGI_Present` 0x434CC0 (ADDR-013); per second log `GetDesc()` of both
  resources (width/height/format), and whether pointers are null, in menu / gameplay / photo mode, for
  FG off, DLSS-G on, FSR3-FG on.
- Confirm: non-null and swapchain-sized with FG off. Ambiguous: only with FG on. Refute: never populated.
- Follow-up capture (RenderDoc) to check that +0x38 has HUD-only content with alpha.

## EXP-07 — camera write census (RE-06)

- Read-only detour on `Cam_SetLocalMatrix` 0x806E50 and vslot 43 0x807D10: log per frame
  `(caller RVA, this, this==CAM-A, this==CAM-B)` counts in gameplay, horse, combat lock-on, cutscene,
  photo mode, map. Gives the active driver per context and proves "once per frame per camera".
- Decision: the vslot-43 hook (VR_ARCHITECTURE.md) is accepted when CAM-A gets exactly one slot-43 call
  per rendered frame in every context, or the extra calls are understood.

## EXP-08 — units, up axis, handedness

- From probe v0.2 logs while walking 5 s forward, strafing right, then turning right 90° and looking up:
  derive engine up axis, units per metre (walk speed vs. known animation speed; better: horse gallop or a
  measured in-game distance) and whether (fwd, left, up) is right-handed (det of the 3×3 = +1).

## EXP-03b — native asymmetric projection (supersedes the "P20/P21 write" option of EXP-03)

- Variable: in a detour of `Cam_SetProjRect` 0x8068E0 for CAM-A only, replace the incoming rect
  (l,b,r,t) by (l·0.5−0.5, b, r·0.5−0.5, t), i.e. the left half of the view, for 60 frames.
- A: stock. B: detour armed, rect passed through unchanged (zero delta). C: left-half rect.
- Confirm: C shows the left half stretched with jitter still present (TAA stable), shadows/SSR/AO/fog
  consistent, no culling loss on the left edge; B identical to A.
- Note: if the alternate RectToMatrix branch is active (global flag 0x1419F2400), only r−1/t−1 are used;
  log which branch runs.

## GoTVR 0.4 run plan (EXP-01 + EXP-07 + EXP-06 + view census + EXP-02/03b + PORT-08 transport)

Static pre-check (cloud sandbox, Wine 9): fail-closed path PASS on a non-game host; all six hooks build valid
trampolines on the real image and restore all signatures on disable (`receipts/gotvr04_selftest.log`).
That is not a runtime result: the next receipts must come from the owner's PC.

Run A (no headset, `xr=0`): census per second plus the hotkey mutations below, each with a stock control.
| Key | Variable | Expected if the owner is right | Refuted if |
|---|---|---|---|
| F7 | armed zero-delta (B control) | no visible change | any change → hook side effect |
| F8 | projection rect left half (EXP-03b C) | left half of the view stretched to full screen, TAA stable, no culling loss | no change, or HUD/UI also stretched |
| F9 | +5° yaw on the render camera world matrix (EXP-02) | the 3D view turns 5° left; the hero, aim and controls are unaffected; HUD unchanged | gameplay direction or HUD changes too |
Run B (headset, `xr=1`): virtual screen quad in VDXR/SteamVR (transport, LUID, lifecycle).
Run C (headset, `xr=2`): known-colour stereo projection layer (left red, right blue, white centre square).

## GoTVR 0.5 run plan (pre-registered decision rules)

| Run | Setting | Pass | Fail → next action |
|---|---|---|---|
| A1 | xr=0, F9 on/off ×3 | view turns **exactly once** by a small amount (≈5°) and stays still; off restores it; log fwd vector differs by ≈5° | drifts → cache not catching game rewrites; send log |
| A2 | xr=0, F8 on/off | the **left half** of the 3D view fills the screen (looks zoomed ×2 horizontally, objects at the screen centre move to the right edge); HUD unchanged; no missing objects | no change → slot 43 does not read +0x200 (falls back to the +0x430 path); mirrored half → depth sign flips (logged P23) |
| B | xr=1 | game image on a fixed screen in the headset, no crash for 5 min incl. one load | crash → send log + dump; grey screen with white square → log line "virtual screen source" names why |
| C | xr=2 | red left eye / blue right eye, squares fuse | — |
| D | xr=3 | game camera follows the head (turn, look up/down, tilt, lean); Jin/aim/HUD do not; F11 recentres; leaning 10 cm moves the view a small step (units check) | wrong axis/sign → log, single-line basis fix; huge/no translation → units_per_m |

Decision: A2 + D pass → the per-eye seam (pose + asymmetric projection) is complete → start R1 (second
RenderView for the other eye, STEREO_ROUTE.md) and the first-person head-joint anchor.

## GoTVR 0.6 run plan (pre-registered)

| Run | Setting | Pass | Fail → next action |
|---|---|---|---|
| H0 | xr=9 | red left / blue right with white squares, 5 min, **no crash** (host process only touches the runtime) | crash → host.log + dump: the runtime fails even out of process → SteamVR cross-check |
| H1 | xr=1 | game image world-stable when turning the head (compositor reprojection), camera follows head; 10 min incl. a load; no crash | black image → gotvr.log `pub:` lines say which step failed |
| H2 | xr=2 | depth: near objects (Jin, grass) pop out vs far hills; no eye swap (closing one eye shows that eye's view); horizon level | eyes swapped/shimmer → log `frameIdMismatch`; wrong depth scale → units_per_m |
| Log | any | `frame counter PASS`, `pub: present queue ... (vtable in dxgi.dll/d3d12…)`, `frameIdMismatch≈0`, host `copied` grows | — |

Decision: H2 PASS → stereo pipeline proven → next build targets R1 (second RenderView) and the first-person
head-joint anchor.
