# Stereo route — DRAFT v2 (RE-09 not entered; nothing selected until EXP-02/03 pass)

Owner goal: native stereo (R1), 6DoF roomscale (Quest 3 via VDXR; SteamVR also), motion controls.
Prior art `[AUTHOR]`: Luke Ross released a GoT VR mod (June 2024) with stereo 3D and 6DoF, gamepad-only
(mixed-news.com). His mods are normally alternate-eye (R3). That shows camera injection with AFR is viable
on this engine family. It says nothing about R1 or motion controls on this build.

## What session 2 changed

1. **The per-eye seam is cheap.** Rendering is camera-relative: viewProj (+0x430) is rotation-only and
   position (+0x550/+0x170) is separate. An eye = (rotation of the HMD view, translation of the eye) written
   into the world matrix (+0x140..+0x17F) followed by vslot 43, which rebuilds viewProj, frustum planes and
   the off-center cull frustum consistently. `[STATIC]`
2. **One choke point for all camera modes** (`Cam_SetLocalMatrix` 0x806E50), so HMD composition is one hook,
   not one per mode. `[STATIC]`
3. **Projection convention known** (update 0.5: the SetProjRect seam is dead at runtime, NEG-010; use +0x200 in slot 43): reversed-Z, row-vector, LH; symmetric base × projection sub-rect
   (+0x694). The engine already has a **native asymmetric-projection setter** (`Cam_SetProjRect` 0x8068E0,
   used every frame for TAA jitter). OpenXR tangents → eye rect composed with the jitter offset in a hook of
   that setter; culling via the separate off-center rect (+0x6A4, `SetOffCenter`). `[STATIC]`
4. **HUD-less scene, UI RGBA, depth and motion vectors** are delivered to Present (desc +0x28 / +0x38 /
   +0x18 / +0x08) → UI can be a separate compositor layer at any rung, and depth can be submitted for
   runtime reprojection. `[STATIC]+[SPEC]`, populated-when-FG-off `[OPEN]`.
5. **Submit is a separate thread**; the pipeline is ≥1 frame deep. R1 must duplicate the render-graph
   work for frame N before submission, not at Present. `[STATIC]`

## Rungs on this target

| Rung | How on GoT | Static evidence | Main risk | Fast test |
|---|---|---|---|---|
| R1 native re-entry | Execute the frame's render-graph build twice (eye L/R) with CAM-A world set per eye between runs, same simulation state; both eyes' HUD-less outputs copied to XR images | camera seam ✔; render-graph entry ✗ (`[OPEN]`: who walks the pass tables at rdata 0x1210E00..0x1211170 and builds the frame) | transient aliasing, async compute, Streamline/FSR state, TAA/upscaler history per eye, particles/queries side effects; ~2× GPU cost | three-capture gate A/B/C once the frame-build entry is found |
| R1′ dormant Nixxes stereo | settings stereo mode 1 (SBS) / 3 (VR) | strings are legacy 3D Vision/SBS; **no OpenXR/OpenVR/LibOVR code in the binary** | very likely framework residue | EXP-04 (cheap, keep) |
| R2 per-draw replay | replay command lists for eye 2 | — | playbook ch.19: D3D12 aliasing makes replay infeasible | not planned |
| R3 alternate-eye (AFR) | per-frame alternate eye pose via the SetLocalMatrix hook + per-eye history banking | everything needed is found statically | temporal disparity; TAA/upscaler history smear (bank history per eye or disable TAA → DLAA per eye) | EXP-02 + EXP-03 then alternation |

**Recommended order:** ship R3 first as the bring-up/fallback (it validates pose, transport, UI layer,
input and comfort end-to-end), while R1 work targets the frame-build entry (next static job: walk callers
of the pass tables and of `StreamlinePass_cb` 0x60F9E0's table entry 0x1211090 up to the per-frame graph
executor).

## Per-eye companions

| Companion | Location | Policy |
|---|---|---|
| View rotation + eye translation | world +0x140..+0x17F → slot 43 | per eye |
| Projection tangents | +0x680/+0x684 (symmetric base); per-eye matrix written at +0x200 in the slot-43 hook (NEG-010: SetProjRect is not called for the render camera) | per eye |
| Cull rect | +0x6A4 off-center (`SetOffCenter`) | union of both eyes |
| Jitter | offset of the +0x694 rect (writers 0x618D30/0xAF9850; Halton table 0x338F520, frame&7) | compose with the eye rect; same jitter both eyes in R1, per-eye sequences in R3 |
| clipToPrevClip | +0x2C0 | per eye (banked per eye in R3) |
| Camera cut/reset | +0x7D8 → sl reset | broadcast to both eyes' histories |
| Culling | +0x4F0 off-center cull frustum (+0x6A4 rect) | union of both eye frusta (widen rect) for a single cull |
| Frame generation (DLSS-G, FSR3-FG) | settings | **off** in VR |

## Side-effect ledger (to fill in RE-09)

| Operation | Once simulation | Once frame | Per eye | Banked | Snapshot/restore | Assertion |
|---|---:|---:|---:|---:|---:|---|
| Game tick 0x4C49E0 | ✔ | | | | | must not re-run per eye |
| Camera driver update (→ 0x806E50) | ✔ (gameplay pose) | | | | | game pose saved before HMD composition |
| Render camera world/viewProj (CAM-A) | | | ✔ | | restore game pose after eye 2 | aim/audio read the game pose, not the eye pose |
| TAA/upscaler history | | | ✔ | ✔ | | |
| Streamline constants | | ✔ | (✔ in R1) | | | FG disabled |
| Submit thread command list | | ✔ | | | | single xrEndFrame owner |

## R1 groundwork (session 3, 2026-10-17, static) — the engine is already multi-view

Path from the main loop: `0x4C5380` main loop → `0x4C49B0` → GameTick `0x4C49E0` → **`FrameView_5A07F0(frame)`**
→ `0x5A0540` → per-view snapshot `RV_9B6FB0` → `RV_Snapshot_9B6AE0` (per-view constants: projection +0x200,
viewProj with translation +0x240, rotation-only viewProj +0x430, inverse +0x470, position +0x550, camera
delta from +0x880, forward +0x140, near/far, time).

| Object / field | Meaning | Evidence |
|---|---|---|
| `frame+0x1ED70` | **main RenderView**; `view[0]` = camera pointer (bound by `RV_BindCamera` 0x9B6AD0 from `frame+0x1EA08`) | 0x5A07F0 |
| `frame+0x1EA90 / +0x1EA98 / +0x1EAA4` | **list of RenderViews rendered this frame** (pointer array, count, capacity, max 0x200) | 0x5A0540 |
| registry `0x339E498` (count `0x339E5B0`), entry`+0xE8` = view, `+0xE0→+0x19B4` enabled | secondary camera views gathered into the list when `DAT_141F3E1C9 > 1` | 0x5A0540, 0x64CA50 |
| list `0x339BE38` (count `0x339BE98`) | second view list, snapshotted and submitted with mode 3 | 0x5A0540, 0x5A07F0 |
| `RV_Submit 0x9B6FE0(view, mode)` | per-view culling/gather (mode bit 1 → 0xB04D20, bit 2 → 0xB04B20 on view+0x170) | decompile |
| `frame+0x80F09` | multi-view mode byte (≥2 enables secondary-view submission) | 0x5A07F0 |

**Consequence:** each RenderView carries its own camera, constants and cull set, and the frame already
loops over a view list. R1 candidate: **append a second RenderView bound to a second camera object
(clone of CAM-A carrying the right-eye pose)**, so both eyes are culled and snapshotted in the same
simulation frame. Still `[OPEN]`: how the back end assigns colour/depth targets and the full deferred pass
set to a list view (main view vs. secondary views may get reduced pass sets); measured by GoTVR 0.4's
view census plus a RenderDoc capture.

**0.4 live view census:** gameplay frames render **5 RenderViews with 5 distinct cameras** (`mode80f09=2`,
secondary registry 4); menus 1. So the list mechanism is active every gameplay frame — the R1 second eye is
an extra entry of the same kind, not a new engine feature. `[LIVE]`

## Session 4 (2026-10-17): R3 implemented as the bring-up rung (GoTVR 0.6, `xr=2`)

- Eye = frame id parity (ADDR-061). The packet for frame N is built the first time frame N touches the render
  camera; FrameView re-runs the slot-43 composition before its view snapshot so the snapshot carries frame
  N's eye even if the engine's own slot-43 calls come later (logged: `slot43 before/after FrameView`).
- Present looks the packet up by `desc[0]` (ADDR-062); a mismatch is counted, never guessed.
- Per-eye projection = the runtime's asymmetric eye frustum written at +0x200 with the game's own P20/P21
  (TAA jitter) added back; the image is declared with exactly that frustum and eye pose (playbook ch09:
  "declare the frustum you actually rendered").
- Known R3 costs to measure, not fix yet: each eye updates at half the game frame rate; TAA/DLSS history
  sees a ±3 cm eye offset every frame (ghosting at depth edges); HUD/menus appear at infinite depth.
- R1 path unchanged (second RenderView per frame, the 0.4 census shows 5 views already). R3 validates
  everything R1 needs: pose packet, per-eye camera + projection, transport, declared frusta.

## Session 5 (2026-10-17): R1 — what is now known `[STATIC]`, and the next decision

**R1′ closed** (NEG-015): the shipped stereo 3D settings are Nixxes-framework residue; the renderer has no
per-eye camera. The second eye must be produced by the engine's view machinery.

**Per-view pipeline inside `FrameView_5A07F0` (game thread, once per frame):**
1. `RV_BindCamera(frame+0x1ED70, frame+0x1EA08)` → main RenderView bound to the frame camera list head
   (`frame+0x1EA08` is a linked list of cameras, next at `cam+0x6F0`; `cam+0x7D8` cut flag reset per frame by
   `Frame_59F2B0`).
2. `RV_9B6FB0(view)` → `RV_Snapshot_9B6AE0` per-view constants (projection +0x200, viewProj, inverse, position…).
3. `FrameView_5A0540`: when `DAT_141F3E1C9 > 1`, four camera-user systems run (`0x14339E180`, `0x1433B3670`,
   `0x14339DF50`, `0x1433B3880` via `0x653D50`) and `CamUser_641C20(0x14339E380)` builds per-object cameras
   (Cam_SetLocalMatrix + view build) and **registers them** into the secondary registry
   (`0x14339E498[count 0x14339E5B0]`, entry+0xE8 = RenderView); active entries are appended to the frame view list.
4. `RV_Submit(view, mode)`: main view and list2 (`0x14339BE38`) with mode 3; each secondary list view with the
   mode selected by its camera kind `cam+0x660` (2 → 3, 1 → 0, 0 → skipped). Mode bit 1 → gather 0xB04D20, bit 2
   → gather 0xB04B20 (on view+0x170).
5. `Rnd_AddView_787AE0(0x1434A2BC0, frame+0x1EAB0, view)` for every list view (main included): fans the view out
   to worker gather jobs (job fn 0x140783F20) and records it in the renderer's per-frame view array
   (`0x1434A2BC0+0x3068`, count +0x3070).

**Consequence:** a RenderView is a self-contained unit (camera, constants, cull/gather set) that the engine
already processes N times per frame. A second *main-kind* view (camera kind 2, mode 3) bound to a second camera
is structurally the same operation the engine performs for its secondary views. What is still `[OPEN]` and
decides the route: whether the later render-graph stage gives a non-main view the full deferred + post pass set
and its own colour target (playbook ch17: engine repeat renders are usually *reduced*).

**Decision rule for the 0.6.2 R1 census (pre-registered):**
- If any live secondary view has camera kind 2 / submit mode 3 and a perspective frustum comparable to the main
  view → those are full scene views: next build clones that registration path for the right eye (route "get
  the engine to register a second view").
- If all secondary views are shadow-like (kind 1 / mode 0, small frusta at light positions, or orthographic) →
  the render graph only renders one full view: next step is a static walk of the render-graph consumer of
  `0x1434A2BC0+0x3068` (which views get the deferred/post passes), then decide between a cloned main view and
  "synchronized sequential" (playbook ch17: both eyes from one simulation tick, sim frozen for the second eye,
  not AFR) as the interim.

## Session 6 (2026-10-17): the render graph is found — R1 design `[STATIC]` + census `[LIVE]`

**Frame structure (game thread, inside GameTick 0x4C49E0, frame object = 0x141EBD2C0):**
`FrameView_5A07F0` (views, snapshots, submits, gather jobs) → **`Frame_59FA60`**: renderable lists
(frame+0x1EA18/+0x1EA30, `obj->Render(ctx)` with a context from `RCtx_Build_9B2DD0`) → `RV_Kick_9B77C0(view)` for
**every** list view and list2 view → `RVSet_9E62B0(frame+0x1EAB0)` → **`RV_Post_9B5180(mainView, dt)` once**.

**`RV_Post_9B5180(view, dt)` is the frame-build entry (closes the session-2 `[OPEN]`):** it walks the pass table
at **0x141210930** (32-byte entries: pre-callback, 2 parameters, main callback), passes 0..0x44 (69 passes; only
0x3B..0x44 when the frame mode byte is 0, i.e. menus/UI), creating one job per pass (job+0x2A60 = view, job
callback 0x1409B55B0, dispatch 0x9B5C20/0x9B5B40), and submits the job graph `FUN_1404D7EB0(0x141A0AB50,
view+0x108)`. Pass 59 = `StreamlinePass_cb 0x60F9E0` (DLSS/upscale), 60–64 UI. A per-view pass bitset at
view+0x15F0 is honoured only when the debug flag `0x141803B60` is set.

**Arena-slot answer (playbook ch17): globally slotted.** `StreamlinePass_cb` takes colour/depth/MV/output from
global render-target slots (0x141EBA148, 0x141A0B8D0, 0x143384D10, 0x141EBA130, 0x141CDB130), jitter from the
global frame counter + Halton table 0x14338F520, reset from the camera cut flag `cam+0x7D8`. One set of targets
and one temporal history per frame. Consequences:
- AFR ghosting/flicker = the single TAA/DLSS history blending each frame with the other eye's image (confirm with
  F6 history-reset test in 0.6.3).
- R1 = **render the 69-pass graph twice in sequence per frame** (left view → capture → right view → capture) with
  **per-eye temporal history** (banked render-target slots / a second DLSS viewport), and run once-per-frame work
  (UI passes 60–64, Present) only once (playbook ch17 "render the world twice; everything else once").

**R1 implementation ladder (pre-registered gates):**
| Gate | Change | Pass | Fail → |
|---|---|---|---|
| R1-0 | 0.6.3 F6: cut flag every frame in AFR | ghosting gone (aliasing up) → shared history confirmed | history lives elsewhere → find TAA history pass |
| R1-A | second `RV_Post` per frame on a **clone of the main RenderView** bound to a clone of CAM-A at the same pose, passes 0..58 only; log job-graph completion | no crash/hang for 10 min; GPU time ≈ ×2; image unchanged | read the job-graph submit 0x4D7EB0 / transient allocator for single-use assumptions |
| R1-B | clone gets the right-eye pose; copy the scene colour after pass 59 of each run into the eye textures (both eyes from one sim tick) | stereo from one frame; disparity correct | per-pass target ownership map |
| R1-C | per-eye history: swap the global history slots (or a second Streamline viewport) between runs | no ghosting, full AA | — |
| R1-D | drop AFR; HUD once (passes 60–64 only on the second run) → quad layer | true native stereo | — |

## Session 7 (2026-10-17): R1-0 PASS `[LIVE]`, R1-A built

**R1-0 PASS:** owner, xr=2 + F6 (camera-cut flag every frame) → flicker/ghosting gone. The single engine
TAA/DLSS history is the AFR ghost source; R1 must carry per-eye history (gate R1-C).

**Pass-context pool (why a second graph run is structurally possible) `[STATIC]`:**
`Jobs_NewPassJob_4DA6A0` allocates pass contexts from the scheduler 0x141A0AB50 (`+0x9290`, stride 0x2B20,
counter `+0x2B5C60` locked-increment, capacity ≈254); `Jobs_SubmitGraph_4D7EB0(sched, view+0x108)` walks the
whole pool and submits only contexts not yet submitted (flag +0x2A6D), so successive `RV_Post` calls in one frame
each submit their own new contexts, queued in allocation order. RenderView = 0x1600 bytes, ctor `0x9B2770`,
dtor `0x9B3200` (frame object 0x141EBD2C0, main view at +0x1ED70); `RV_BindCamera` = `view[0] = cam`;
`RV_9B6FB0` = snapshot + `view+0x18 = 0`; `RV_Kick` skips views with `+0x18 == 0`; `RV_Post`'s second argument is
the starting render context (`[0x141A13DC0]`), reset to 0x141A11370 at the end of RV_Post.

**R1-A (GoTVR 0.7.0, F5):** a second RenderView built by the engine ctor, bound to the main camera, snapshotted,
appended to the frame view list after `FrameView_5A0540` (engine then runs its gather jobs, RV_Kick and the
end-of-frame reset), submitted with mode 3 next to the main view, and posted with the same starting context right
after the main `RV_Post`. Pre-registered result: **PASS** = no crash/hang for 10 min of play, image unchanged on the
monitor, frame rate ≈ halved (the world is rendered twice), log `pass contexts after second ≈ 2× after main`.
**FAIL modes and next action:** crash → dump (pass-context pool / transient allocator single-use assumption);
garbage/flicker → the two runs overlap on the GPU (need an explicit ordering fence between them); no fps change
→ second run did not render (log `view+0x18 v2`, `second graph runs`).
Known expected difference: DLSS/TAA is evaluated twice per frame with the same jitter, so temporal accumulation
runs twice (possibly a slightly softer/ghostier image) — irrelevant once R1-C banks history per eye.

**R1-A iteration log:** 0.7.0 crash (view-constants 4-slot table, BUG-11) → 0.7.1 second run completes once,
then deadlock (V2 never activated, BUG-13) → 0.7.2 activates V2 with RV_9E5DE0; second run completes once, then deadlock in Sched_EndFrameWait (the
graph submit re-walks the first run's pass contexts, completed overshoots allocated, BUG-14) → 0.7.3 submits only
the second run's contexts (range-limited Jobs_SubmitGraph). 0.7.3 PASS mechanically (no stall, ~100 doubled frames/s) but darker/bluer: run 1 lost
the frame's tiled light lists (BUG-15) → 0.7.4 banks the light-list state per run and lends run 2's finished lists
to run 1 at frame end; F4 A/B. Also 0.7.4: pass-59 census (upscaler output + command list per run) = R1-B capture
seam. Shared per-frame camera blocks to handle in R1-B (written per run from CAM-A, read by passes): VC block
0x77E810 (+0x1B0), Cam_FrameData 0x6336B0 (0x14339BCE0 +0x48/+0x50), light tiles (CAM-A screen tiles, lent). 0.7.4: lend had no visible effect (NEG-018); owner uses TAA (upscaler slots 1×1).
0.7.5: per-pass census (Pass_Job 0x9B55B0, view+0x13C4 draw ranges, main vs V2) + F3 suffix bisection of run 2's
passes. R1-B capture seam with TAA = pass 59's TAA branch output (to be identified from the census). 0.7.5 F3 race corrupted the engine pass
table (BUG-16) → 0.7.6 race-free masking from a pristine copy + self-check; census on the pass body 0x9B55D0
(job and inline routes). 0.7.6: run 2's output is the dark one; V2 had no draws on passes 0/17/18/29/51/52/57/61
because Frame_59FA60's renderable lists render into a main-view context only (BUG-17) → 0.7.7 duplicates those
flushes into V2 (F4 A/B). R1-A gate now: F5 image identical to F5 off. 0.7.7: draws now identical, image still dark (NEG-019) → the cause is state changed by
the second execution (exposure/Purkinje lead). 0.7.8: brightness meter + automatic F2 bisection that applies the
culprit run-2 passes as "run 1 is the authority" (playbook: one shared exposure across eyes = the R1-B design too). 0.7.8 F2: run 2 is brighter+bluer (additive, ≈ +1/+5/+10), not darker.
**R1-B built (0.8.0, xr=3)**: per-eye camera via drain + slot-43 recomposition + V2 snapshot refresh; left eye
captured in GPU order inside the second render's pass-0 list with the engine's own transition + flush; right eye at
Present; both eyes published per frame. Gates: F1 flat offset proof; in the headset: depth correct, eyes not swapped,
both eyes update every frame (STEREO/s line: published both-eye frames ≈ fps). 0.8.0 live: right eye mixed (camera-relative engine, BUG-20),
left eye black (wrong back-buffer object, BUG-21) → 0.8.1: camera-relative right eye (T(−D) in VPrel via the engine's
SetViewProjFrustum) + left capture by native back-buffer object with Present-time verification. 0.8.2: F1 made TAA-free (cut flag);
same-frame L/R pair meter + F2 single-pass sweep with saved result (right_eye_reuse). Main-view-only steps still not
mirrored for V2 (watch if the next run stalls): `RV_9E5A00` (occlusion setup, resets shared state — deliberately
skipped), `FUN_1407B5B20(0x1434BBFA0, mainView)` at the start of Frame_59FA60, list2/tile-cache coupling.


## Session 8 (2026-10-18): route decision — Synchronized Sequential (xr=4)
R1 (second render inside one frame) reached the playbook's documented wall for single-main-view engines: per-frame
state is globally slotted and non-idempotent (scheduler, view constants, light lists, main-only renderables,
camera-relative gathering, back-buffer objects, and finally depth/G-buffer/additive state: F1 double image with TAA
reset, blue tint). Following ch17 (Cyberpunk: "do not retry" families; Synchronized Sequential as the requirement-
meeting route), xr=4 renders each eye as a complete engine frame and freezes simulation + time for the right-eye
frame (Clock_Update skipped, game steps 0, camera + head sample + pad state held), so both eyes come from one
simulation tick. Cost: pair rate = half the frame rate (each eye ~fps/2), compensated per eye by the headset's
reprojection. R1/xr=3 code stays (off by default) for research. Next: R1-C per-eye TAA history in xr=4 (swap the
global history slot per eye), HUD once (quad layer), first-person anchor.

## Session 9 (2026-10-18): xr=4 confirmed in the headset; temporal history (BUG-22)
`[HEADSET]` xr=4: real depth, good frame rate, controls good. Remaining: object flicker + black streaks = the forced
per-frame history reset (every temporal pass stuck in its first frame). RE of the engine's prev-frame chain
(ADDR-092..094) shows that under synchronized-sequential the engine's "previous frame" camera and history images
are both the other eye, i.e. consistent, so 0.9.1 stops resetting (playbook ch14 #velocity-not-temporal: TAA fine,
motion blur off). Per-eye history bank (matrices + textures, ch14 #per-eye-history-bank) is kept as the next step
only if the headset shows cross-eye ghosting; the history ring lead is ADDR-095. Next after that: HUD once on a quad
layer, first-person anchor, motion controls.

## Session 10 (2026-10-18): history is per consumer, not global (BUG-23)
Headset A/B of 0.9.1 split the temporal consumers: visibility that reuses the previous frame's view must reset per
eye (else one-eye objects/shadows); lighting accumulation must keep history (else flashes); TAA keeps history at the
cost of a faint cross-eye ghost. 0.9.2 redirects the 13 render-side cut readers to per-group overrides (presets on
F6). Remaining step to full per-eye temporal correctness: bank the AA ping-pong (ADDR-098) per eye (STR-015).

## Session 11 (2026-10-18): "is this native stereo or AFR?"
Definition used here (playbook ch17): AFR = consecutive frames show different moments, one per eye. xr=4 =
synchronized sequential: each eye is a complete engine render with its own eye pose and asymmetric frustum (6DoF,
camera-relative T(−pos)·VPrel), both eyes from ONE head sample and ONE simulation tick (time/simulation/pad frozen for
the right-eye tick). What was still AFR-like: the host's per-eye newest selection (BUG-25) — fixed with pair-atomic
presentation in 0.9.3. What remains different from single-frame stereo: per-eye refresh = half the engine rate, and
the engine's frame-to-frame state alternates eyes (the temporal/visibility work of BUG-22..24).

## Session 12 (2026-10-18): per-eye history for the two consumers that showed it
Per-eye occlusion (0.9.4) confirmed in the headset: no pop-in, no light flicker. 0.9.5 banks the TAA (matrices via a
second Cam_TemporalUpdate run on this eye's prev slots, texture via GPU copies of the history record in the engine's
own command list) and mirrors only the left eye to the monitor. Each eye now has its own previous frame for culling
and anti-aliasing; lighting accumulation stays shared (it was correct in the headset). Next: HUD once on a quad
layer, first-person anchor, motion controls.

## Session 13 (2026-10-18): per-eye temporal state complete for the consumers that showed artifacts
Headset-verified: synchronized-sequential pairs, pair-atomic presentation, left-eye monitor mirror, per-eye occlusion
(BUG-26) and per-eye TAA history + reprojection (BUG-27/31/33). Lighting accumulation stays shared (correct in the
headset). Next: DLSS per eye (Streamline viewport per eye + per-eye constants; motion vectors need the same care as
AA_Velocity), HUD on a quad layer, first-person anchor, motion controls.

## Session 14 (2026-10-18): DLSS dropped; UI/HUD and videos (playbook ch04)
DLSS under xr=4 is a loss (CPU-bound pairs; alternate jitter phases per eye) → off by default, TAA recommended
(BUGLOG 0.12.0). UI: the 3D image is already true per-eye stereo; the UI target (0x143377510, passes 60/61/64) is
now drawn into a per-eye converged panel rect instead of the whole frustum (HUD-004 adapted without a second
layer; layout untouched). Flat contexts (pre-rendered video, no-3D-world frames, or F10) are published with
GV_SLOT_THEATER and shown by the host on a world-locked 16:9-correct quad (HUD-002/003). Next: world-locked / lazy-follow
HUD and pause menus (needs a pause signal), a real transparent HUD layer (copy the UI target into its own shared
texture and suppress the engine's composite), first-person anchor, motion controls.

## Session 15 (2026-10-18): flat contexts rendered flat; UI-layer RE
Videos on the theatre quad accepted in the headset. Pausing menus (clock time-scale reason 7) are now flat contexts:
the game renders a normal flat frame (own camera, every tick simulated) shown on the world-locked screen in a black
void. Next, in order: (1) UI on its own OpenXR quad at the UI target's resolution (from the 0.13.0 probe: separate
texture → copy + clear before the composite; back buffer → redirect the UI binds to a layer texture), (2) start screen
UI world-locked with the 3D world behind (title signal from the census), (3) square-aspect render resolution
(design doc) for 3D sharpness.
