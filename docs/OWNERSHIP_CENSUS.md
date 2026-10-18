# Ownership census — CAM-A (render camera global 0x1EDBCC0), static lower bound

Method `[STATIC]`: every `mov reg, [rip→0x141EDBCC0]` (241 sites), primary function resolved through the
unwind chain, then the camera fields read through the same register within ~40 instructions. Lower bound:
it misses reads after the pointer is passed to a callee (93 functions only pass the pointer on).
Full table: `re/receipts/camA_consumer_census.tsv` (208 functions).

| Field group read | Functions | Meaning for the VR hook |
|---|---:|---|
| (pointer only / passed on) | 93 | callee decides; census incomplete here |
| local | 42 | **game pose** (+0x100..+0x13F). The vslot-43 hook does not modify it, so these keep seeing the game camera |
| POS | 27 | +0x550 = render position: sees the eye pose (audio listener, LOD, fog, …: check per consumer) |
| cut | 12 | +0x7D8 cut/reset: unaffected |
| WORLD | 12 | +0x140..+0x17F = render pose: sees the eye pose |
| offcenter | 4 | off-center rect: per-eye value in stereo |
| fov/near/far | 4 | projection params |
| planes | 1 | eye frustum |
| cullfrustum | 1 | eye/union cull frustum |
| VIEWPROJ | 1 | eye viewProj |

Other offsets read (pointer chases / sub-objects): +0x730 parent (7), +0x808/+0x820 sub-objects (5 each),
+0x880/+0x850..+0x870 world copy, +0x8C0 local-backup translation.

## Design consequence

The engine already separates a **game pose** (local, read by ~42 gameplay-side functions) from a **render
pose** (world/position/viewProj, read by render-side functions). Writing the HMD eye pose only into world
(+0x140..+0x17F) and letting slot 43 derive the rest gives a natural split: gameplay keeps the game camera,
pixels follow the head. `[STATIC]` lower bound; EXP-07 must confirm no gameplay consumer of WORLD/POS needs
the game pose (candidates: audio listener, aim/reticle).

## Consumers reading WORLD or POS (first to audit)

| Primary fn RVA | Fields | Strings (hint) |
|---|---|---|
| 579900 | POS |  |
| 57b190 | POS |  |
| 57d140 | POS |  |
| 57df20 | POS |  |
| 59eb30 | POS |  |
| 59eef0 | POS |  |
| 59f720 | POS |  |
| 0 | POS,cut,local,other:182,other:7fc,planes |  |
| 6319c0 | WORLD,cut,other:8c0 |  |
| 646f50 | POS |  |
| 6660d0 | POS |  |
| 693710 | WORLD |  |
| 6f9d40 | WORLD |  |
| 744cf0 | POS |  |
| 771a10 | POS | Clouds::NoisePerlin%d; Clouds::NoiseWorley%d |
| 773610 | POS,other:8 |  |
| 775370 | POS | Microsoft Software Key Storage; RSAPRIVATEBLOB |
| 7790f0 | POS |  |
| 779b80 | POS |  |
| 781a70 | WORLD | diffuse; normal |
| 783770 | WORLD,other:840 |  |
| 79ada0 | POS | Blade Count; Grass Indirect Args |
| 7af1b0 | POS | 3D Tex Shhalf Grid |
| 7b1db0 | POS | SH Sky Intermediate; SH Sky Result |
| 7c2750 | POS,fov/near/far |  |
| 7f0af0 | WORLD |  |
| 873070 | WORLD |  |
| 9a2610 | POS |  |
| 9a37a0 | WORLD |  |
| 9ae8b0 | POS,other:80 | Sky::S1; Sky::S2 |
| 9af920 | POS,other:80 | Sky::E; Sky::DeltaE |
| 9b5c20 | POS |  |
| 9db8b0 | WORLD | ImpactRecords |
| a36b00 | WORLD,other:8 |  |
| a76160 | WORLD |  |
| aad450 | POS |  |
| aaf540 | POS |  |
| ab9b50 | POS,other:50 |  |
| b13ab0 | WORLD |  |
