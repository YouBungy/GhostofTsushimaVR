# GoTVRMod: RE session 2 handoff (static)

Target: Ghost of Tsushima DIRECTOR'S CUT (Steam), `GhostOfTsushima.exe` SHA-256
`ED0931B6F6D3CA3874BAF39EBC16D95FF0E93296891182F96C3A790DA7109F63` (re-hashed this session; unchanged from
session 1, so all session-1 anchors still apply).
Goal (owner): 6DoF roomscale, motion controls, native stereo. Quest 3 via VDXR (Virtual Desktop), plus
SteamVR. Route: RE-owned (closed retail binary), per the VR modding playbook (`start-new-port.md` →
`reverse-engineered-route.md`). Its RE goal is the smallest set of proven seams, not a full decompile.
State (2026-10-17): BASELINE / LOAD `[LIVE]`; GoTVR 0.4 live run: hooks stable, **render-pose seam
`[LIVE]`** (F9), multi-view frame `[LIVE]` (5 RenderViews in gameplay), two bugs and one crash diagnosed
(docs/BUGLOG.md). **GoTVR 0.5** (`mod/`) fixes them (projection at +0x200 in slot 43, idempotent camera
writes, OpenXR on its own thread + queue) and adds the first head-tracked mode (xr=3).
Next: 0.5 runs A–D (docs/EXPERIMENTS.md) → R1 second RenderView → first-person head anchor.
Owner decision: **first-person**, **native stereo (R1)**.

## Seams the playbook asks for, and where they stand

| # | Seam (reverse-engineered-route.md) | Session-2 result | Doc |
|---|---|---|---|
| 1 | Gameplay frame boundary | tick 0x4C49E0 (game thread) → dedicated **submit thread** 0x509FB0 → Present 0x434CC0; Reflex markers on each. Pipelined ≥1 frame | D3D12_TRANSPORT.md |
| 2 | Render camera + projection | full pipeline decoded: `Cam_SetLocalMatrix` 0x806E50 → `UpdateWorld` 0x8081F0 → vslot 43 → viewProj/frustum. Camera-relative rendering, reversed-Z; **native asymmetric projection** via `Cam_SetProjRect` 0x8068E0 (the game's TAA-jitter path) | CAMERA_FINDINGS.md |
| 3 | Culling | same object: off-center cull frustum +0x4F0 built from the +0x6A4 rect (`SetOffCenter`); skipped while flag 0x1434C2DD0 (photo-mode toggle) is set | CAMERA_FINDINGS.md |
| 4 | Player/body + native aim | camera drivers (follow/fixed/interp/attached) found; cameras attach to **skeleton joints** (joint array format known). Aim ray: `[OPEN]` (leads listed) | CAMERA_FINDINGS.md, INPUT_OWNERSHIP.md |
| 5 | Two views | per-eye = world matrix + slot 43; R3 (AFR) fully specified, R1 needs the frame-build entry (`[OPEN]`) | STEREO_ROUTE.md |
| 6 | Input / UI / interaction | XInput poll decoded (bit → Nx id 0x190..0x19D, axes 0..3, rumble); the XInput proxy is a complete T1 input + haptics seam. **HUD-less scene, UI RGBA, depth and motion vectors are handed to Present** (desc +0x28 / +0x38 / +0x18 / +0x08) | INPUT_OWNERSHIP.md, D3D12_TRANSPORT.md |
| — | OpenXR D3D12 binding | device and direct queue reachable by a guarded pointer chain, no hooks | D3D12_TRANSPORT.md |

30 new anchors (ADDR-023..052), each with a byte signature verified unique in this build (ADDRESS_REGISTRY.md).
An independent agent re-derived the 12 central claims from the bytes: 9 confirmed, 3 partial; the
corrections are applied in the docs (`docs/VERIFICATION.md`).

## Key design consequence

The engine already separates a **game pose** (camera local matrix, read by ~42 gameplay-side functions) from
a **render pose** (world/position/viewProj). Writing the HMD eye pose only into world at vslot 43 gives 6DoF
pixels without moving the game camera (OWNERSHIP_CENSUS.md, VR_ARCHITECTURE.md).

## Next proofs (EXPERIMENTS.md), in order (superseded by "GoTVR 0.4 run plan")

1. **EXP-00 + EXP-02 (passive) + D3D chain**: install `probe/xinput1_4.dll` (v0.3; v0.2 used the wrong name, NEG-009), play the scripted
   60 s in `probe/README.md`, send `GoTVR\load.log`.
2. EXP-08 units/up-axis/handedness from the same log.
3. EXP-01 frame census + EXP-07 camera write census (first build with read-only detours).
4. EXP-06 Present descriptor (UI layer with frame generation off).
5. EXP-03b native asymmetric projection (Cam_SetProjRect), then the first AFR head-tracked image on VDXR.

## Native stereo plan (R1) — groundwork found in session 3

The frame setup `FrameView_5A07F0` builds a **list of RenderViews** per frame, each with its own camera,
constants and cull set (STEREO_ROUTE.md, "R1 groundwork"). The R1 route is a second RenderView bound to a
second camera carrying the other eye's pose, in the same simulation frame. What remains open is how the
back end assigns targets/passes to a list view; the GoTVR 0.4 view census and a RenderDoc capture answer it.
First-person: camera composed onto the hero head joint (camera-driver joint array format known); the
head-joint index and the body-hiding policy are the next static targets.

## Folder layout

`docs/` evidence set (playbook templates) · `mod/` GoTVR 0.5 (install files, source, build) · `receipts/` live logs + self-test · `probe/` probe 0.3 ·
`re/decomp/` Ghidra decompiles of 30 functions · `re/receipts/` instruction windows + consumer census ·
`re/out/` signatures (v1, v2), vtables, strings, PE summary · `re/scripts/` reproducible analysis
(`idx.py build` needs `re/bin/GhostOfTsushima.exe` and `re/out/text.asm` from
`llvm-objdump -d --no-show-raw-insn -M intel --section=.text`; Ghidra: import with `-noanalysis`, then
`-postScript Decomp.java <list> <outdir>`).
