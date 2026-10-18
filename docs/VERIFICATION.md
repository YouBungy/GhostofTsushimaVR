# Independent verification — session 2 (2026-10-17)

A separate agent that had not seen the analysis re-derived 12 central claims from the shipping bytes
(llvm-objdump, capstone, raw scans; exe SHA-256 re-checked: match). Grades below are `[STATIC]`.

| # | Claim | Verdict | Correction applied |
|---|---|---|---|
| C1 | `Cam_SetLocalMatrix` 0x806E50 copies the matrix to +0x100 and +0x890, tail-jumps to 0x8081F0 | CONFIRMED | — |
| C2 | `Cam_UpdateWorld` 0x8081F0: world = local × parent(+0x730), copy to +0x850, vslot 43 | PARTIAL | World = **M × local**; M comes from the transform hierarchy (0xA17AA0 on global 0x141F3E138; 0x66C760 if parent class 0xD6; 0xA181A0 special-cases CAM-A); identity if parent+0x2AD or this+0x660 set |
| C3 | Two camera vtables, class ids 0x11F/0x11C, factory 0x825100 | CONFIRMED | both 44 slots; neither class has RTTI |
| C4 | Reversed-Z projection in slot 42 | CONFIRMED | base projection is multiplied by `RectToMatrix(+0x694)` → native asymmetric projection (led to ADDR-049..052) |
| C5 | Slot 43 → 0x806BA0 stores pos/viewProj, reads off-center rect, calls 0x803BD0 | CONFIRMED | +0x240..+0x27F is viewProj with translation (not planes); 0x803BD0 only runs when byte 0x1434C2DD0 == 0 |
| C6 | No direct writer of global 0x1EDBCC0; no absolute pointer to it | CONFIRMED | 243 references: 239 loads, 4 cmp |
| C7 | Callers of 0x806E50; follow-camera strings | CONFIRMED | string text at 0x12167DC / 0x12167FC (refcount header +0); 4 of 11 callers outside 0xA3xxxx |
| C8 | Queue creation: device+0x38, IID_ID3D12CommandQueue, records at +0x30 stride 0x40, Type | CONFIRMED | Type = 0 (DIRECT): r12d zeroed at 0x43814F; loop over node count +0x80 |
| C9 | NxD3DImpl::Init allocates NxD3D12Driver (vtable 0x10FAF58) into +0x168 when apiMode 3 | CONFIRMED | RTTI: `.?AVNxD3D12Driver@nx@@`, `.?AVNxD3DImpl@nx@@`, else `.?AVNxD3D11Driver@nx@@` |
| C10 | Present tags HUDLess (+0x08) and UI (+0x18) | **PARTIAL (offsets wrong)** | four tags: MotionVectors +0x08, Depth +0x18, HUDLessColor +0x28, UIColorAndAlpha +0x38 (states at +8 each); tagged only if depth non-null and mode == 2 |
| C11 | XInput poll bit → id 0x190..0x19D, axes 0..3 | CONFIRMED | runs only when the packet number changes; bits 0x400/0x800 skipped |
| C12 | Submit thread loop | CONFIRMED | outer loop `while (![+0x330])`; SetEvent once per batch; target `table_0x1CE01F0[cmd+4]` |

Reviewer working files were in the cloud sandbox (`/home/claude/got/verify/`) and are not part of this folder.
The verifier's limits: static only, same binary, no runtime. It checks bytes, not behaviour.

## GoTVR 0.6 independent code review (2026-10-17)

A separate agent that had not seen the code written reviewed `gotvr.cpp`, `host.cpp`, `gotvr_shared.h` twice.
Pass 1: 4 confirmed bugs (swapchain images released before their copy was submitted; XR_TIMEOUT_EXPIRED
treated as success / release of an unwaited image; null copy source after a failed shared open; layers
referencing never-released swapchains), 9 hazards/risks (latest-only slot starvation, copy before the
wrapper's final GPU waits, camera cache not keyed by camera, reused shared names, no LUID check, half-tangents,
10-bit formats, seqlock hygiene, no retry). Pass 2 verified 9 fixes and found gaps in 4 (retry path skipped the
native hook; unnamed fence; fallback counted unmatched presents; pendFrame/pending ordering → possible eye
swap) plus 2 new bugs (texture rebuild racing a copy on another thread; slot reset). All fixed in the shipped
build: protocol v3, `imagesOk`/`gameRefuse` flags, one critical section for init/rebuild/copy, frame
generation detected (≈2 native presents per game frame) and refused. Pose/projection/level math confirmed.
Still open: vertical projection sign (not live-tested), consumers of +0x680/+0x684 under an asymmetric
frustum (`widen_tangents` toggle for A/B on hardware).
