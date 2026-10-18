# Address registry

## Supported builds

| Build/store | Module | SHA-256 | Image base convention | State |
|---|---|---|---|---|
| Steam, linked 2026-06-23 | GhostOfTsushima.exe | ED0931B6F6D3CA3874BAF39EBC16D95FF0E93296891182F96C3A790DA7109F63 | RVA = VA − 0x140000000 (preferred base; ASLR on) | static-only, no anchor yet LIVE |

Every signature below was generated with rip-relative disp32 and rel32 call/jmp targets wildcarded, then
checked to match **exactly once** in `.text` of this build (`re/scripts/sigs.py`, output
`re/out/signatures.json`). Uniqueness is a byte fact; semantic placement is the evidence column.
Shipping code must also check the module SHA-256 (or a guard-byte set) and fail closed.

## Anchors

| ID | Semantic owner | RVA | Derivation | Evidence | Call phase / consumers | Invalidation | Fallback |
|---|---|---|---|---|---|---|---|
| ADDR-001 | Render-view/camera object pointer (CAM-A) | data 0x1EDBCC0 | rip-relative load at 0x60FA11 (`48 8B 35 ?? ?? ?? ?? 4C 8D 45 50 0F 57 C0 66 C7 45 70 00 00`) | `[STATIC]` | read by ≥215 functions; writer OPEN | patch; lifetime OPEN (level load?) | none yet |
| ADDR-002 | Camera vtable (44 slots) | rdata 0x1183C60 | unique table whose slots 42/43 land in the camera cluster | `[STATIC]` | validates a CAM-A pointer (`*(u64*)obj == base+0x1183C60`) | patch | — |
| ADDR-003 | Camera::SetFov(float rad) | 0x806F50 | `40 53 48 83 EC 20 0F 2E 89 70 06 00 00 48 8B D9` | `[STATIC]` | 7 callers | patch | — |
| ADDR-004 | Camera::SetFar(float) | 0x807020 | `40 53 48 83 EC 20 48 8B 01 48 8B D9 F3 0F 11 89 68 06 00 00` | `[STATIC]` | | | |
| ADDR-005 | Camera::SetNear(float) | 0x807050 | `40 53 48 83 EC 20 48 8B 01 48 8B D9 F3 0F 11 89 64 06 00 00` | `[STATIC]` | 6 callers | | |
| ADDR-006 | Camera::SetOffCenter(const float4& lbrt) | 0x807080 | `40 53 48 83 EC 20 0F 10 02 C6 81 B7 06 00 00 01` | `[STATIC]` | caller 0x59C620 | | |
| ADDR-007 | Camera::ResetOffCenter() | 0x806240 | `C7 81 A4 06 00 00 00 00 80 BF C7 81 A8 06 00 00 00 00 80 BF` | `[STATIC]` | callers 0x59AD5C, 0x59AE3B, 0x59C620 | | |
| ADDR-008 | Camera vslot 42 UpdateProjection | 0x807F30 | see signatures.json | `[STATIC]` | | | |
| ADDR-009 | Camera vslot 43 UpdateFrustum | 0x807D10 | see signatures.json | `[STATIC]` | calls 0x806BA0 (reads off-center rect) | | |
| ADDR-010 | Camera::BuildSLConstants (→ slSetConstants) | 0x807140 | `48 89 5C 24 10 55 48 8D AC 24 00 FD FF FF 48 81 EC 00 04 00 00` | `[STATIC]` | render-pass callback 0x60F9E0 | Streamline disabled? | FSR/XeSS/NRD oracles |
| ADDR-011 | Matrix4 inverse (SSE cofactor) | 0x7D8360 | see signatures.json | `[STATIC]` | utility | | |
| ADDR-012 | Streamline/upscaler render-pass callback | 0x60F9E0 | `40 55 56 57 41 55 41 56 48 8D AC 24 00 FF FF FF` | `[STATIC]` | pass table entry at rdata 0x1211090 | | |
| ADDR-013 | NxDXGIImpl vslot 10, the Present wrapper | 0x434CC0 | `40 55 53 56 57 41 54 41 56 41 57 48 8D AC 24 80 FD FF FF`; also vtable 0x10F9B68 slot 10 | `[STATIC]` | logs "Present failed…" / "Window occluded…" | | DXGI vtable hook |
| ADDR-014 | IDXGISwapChain::Present call site | 0x435428 (call at 0x435437) | `48 8B 4E 30 41 8B D7 44 8B 44 24 24 48 8B 01 FF 50 40` | `[STATIC]` | swapchain = `[rsi+0x30]`; Reflex PRESENT_START before, PRESENT_END after | | |
| ADDR-015 | Game tick (Reflex INPUT_SAMPLE, SIMULATION_START/END) | 0x4C49E0 | see signatures.json | `[STATIC]` | once per simulation frame? (`[INFERENCE]`) | | |
| ADDR-016 | Render submit (Reflex RENDERSUBMIT_START/END) | 0x509FDB (chunk) | see signatures.json | `[STATIC]` | render thread? OPEN | | |
| ADDR-017 | Nx service table pointer | data 0x1998300 | getters at 0x4093A0..0x409540 (`mov rax,[rip+X]; mov rax,[rax+off]; ret`) | `[STATIC]` | see NX_SERVICE_MAP.md | | |
| ADDR-018 | NxSettings getter | 0x409530 | `48 8B 05 ?? ?? ?? ?? 48 8B 40 30 C3` | `[STATIC]` | 98 callers | | |
| ADDR-019 | NxSettings::Validate (stereo/FOV clamp) | 0x428420 | see signatures.json | `[STATIC]` | | | |
| ADDR-020 | On-demand XInput loader (function table 0x1110698; order 9_1_0 → 1_4 → 1_3, prefers already-loaded). **Not** the pad loader; see ADDR-053 | 0x4AEAA0 | see signatures.json; order XInput9_1_0 → XInput1_4 → XInput1_3, plain `LoadLibraryW` | `[STATIC]` | | | |
| ADDR-021 | Reflex jitter table | rdata 0x338F520 | `lea rcx,[rip+X]` in 0x807140 | `[STATIC]` | indexed by frame & 7 | | |
| ADDR-022 | slSetConstants IAT slot | 0x10EB8B8 | import | `[SPEC]`/`[STATIC]` | 1 caller | | |

## Session-2 anchors (2026-10-17, static; signatures in `re/out/signatures_v2.json`, all unique in .text)

| ID | Semantic owner | RVA | Derivation / guard | Evidence | Call phase / consumers | Invalidation | Fallback |
|---|---|---|---|---|---|---|---|
| ADDR-023 | **Cam_SetLocalMatrix(cam, const float4x4*)**: the single write choke point for every camera mode | 0x806E50 | `0F 10 02 0F 11 81 00 01 00 00 0F 10 4A 10 0F 11 89 10 01 00 00`; guard: `rcx` vtable ∈ {0x1183C60, 0x1143850} | `[STATIC]` | 11 callers (camera drivers); tail-jumps to ADDR-024 | patch | ADDR-024 |
| ADDR-024 | Cam_UpdateWorld: world(+0x140) = M × local(+0x100), M from the transform hierarchy (0xA17AA0 / 0x66C760 / 0xA181A0) → vslot 43 | 0x8081F0 | see json (64-byte sig) | `[STATIC]` | 0x804690 (LookAt), 0x806E50 | patch | — |
| ADDR-025 | Cam_LookAt(cam, eye, target, yawOfs, pitchOfs, roll) | 0x804690 | json | `[STATIC]` | vslots 35/36 (init) | | |
| ADDR-026 | Cam_SetViewProjFrustum(cam, pos, viewProj) → +0x550, +0x430, planes, cull frustum | 0x806BA0 | `48 8B C4 48 89 58 10 55 56 57 48 81 EC D0 00 00 00` | `[STATIC]` | vslot 43, 0x64A035, 0x64B740 | | |
| ADDR-027 | Cam_ctor_base (class 0x11C, vtable 0x1143850) | 0x801320 | json | `[STATIC]` | factories 0x824FC0 / 0x825100, 0x63F9B0, 0x801AC0/0x801B00 | | |
| ADDR-028 | Cam_factory_11F (0x920 bytes, vtable 0x1183C60) | 0x825100 | `40 53 48 83 EC 30 48 85 C9 74 12 BA 20 09 00 00` | `[STATIC]` | class registry slot | | |
| ADDR-029 | Base camera vtable (class 0x11C) | rdata 0x1143850 | slot 42 = 0x807F30, 43 = 0x807D10, slot 4 → `mov eax,0x11C` | `[STATIC]` | guard alongside ADDR-002 | | |
| ADDR-030 | CamDrv_AttachedPose (pose ⊗ joint matrix → SetLocalMatrix + SetFov) | 0xA32DF0 | `4C 8B DC 55 53 56 49 8D AB 68 FE FF FF 48 81 EC 80 02 00 00` | `[STATIC]` | camera driver update | | |
| ADDR-031 | CamDrv_Follow ("Camera following object", string text at 0x12167DC) | 0xA33360 | json | `[STATIC]` | | | |
| ADDR-032 | CamDrv_Interp | 0xA386B0 | `48 8B C4 48 89 58 18 55 56 57 48 81 EC 40 01 00 00` | `[STATIC]` | | | |
| ADDR-033 | CamDrv_Fixed | 0xA332D0 | json | `[STATIC]` | | | |
| ADDR-034 | Camera-B local restore on mode toggle | 0x5A0460 | `48 83 EC 28 38 15 ?? ?? ?? ?? 0F 84 ?? ?? ?? ??` | `[STATIC]` | photo-mode style toggle `[INFERENCE]` | | |
| ADDR-035 | Second camera global (CAM-B) | data 0x1EDBCC8 | read by 0x5A0460, 0x641C20 (32 loads) | `[STATIC]` | | | |
| ADDR-036 | GamepadXInput::Poll (slot 5) | 0x498030 | json | `[STATIC]` | per-frame input | | XInput proxy |
| ADDR-037 | GamepadXInput::Rumble (slot 3) | 0x4983E0 | json | `[STATIC]` | → XInputSetState | | |
| ADDR-038 | NxGamepad::SetButton(this, down, state, id 0x190..0x19D) | 0x4973D0 | json | `[STATIC]` | | | |
| ADDR-039 | NxGamepad::SetAxis(this, axis 0..3, x, y) | 0x497A90 | json | `[STATIC]` | | | |
| ADDR-040 | D3D12 queue creation site (desc, IID_ID3D12CommandQueue, &driver->queues[i]) | 0x43846F | `41 8B C7 44 89 65 A0 D3 E0 4C 8D 05 ?? ?? ?? ??` | `[STATIC]+[SPEC]` | device init | | ExecuteCommandLists hook |
| ADDR-041 | slSetD3DDevice(driver->device) site | 0x4381E6 | `49 8B 4E 38 E8 ?? ?? ?? ?? 48 63 D8 85 C0 0F 84 ?? ?? ?? ??` | `[STATIC]` | device init | | |
| ADDR-042 | NxD3D12Driver vtable; device `+0x38`, queue records `+0x30` (stride 0x40), count `+0x80` | rdata 0x10FAF58 | `NxD3DImpl(+0x50 svc)+0x168`, guard apiMode `+0x160 == 3` | `[STATIC]` | OpenXR binding | | |
| ADDR-043 | RenderSubmitThread (Execute/Signal/Wait/Present command loop) | 0x509FB0 | `40 56 48 81 EC 80 00 00 00 48 8B F1 BA FF FF FF FF` | `[STATIC]` | submit thread | | |
| ADDR-044 | Present: slSetTag(4 tags: MotionVectors desc+0x08, Depth +0x18, HUDLessColor +0x28, UIColorAndAlpha +0x38) | 0x435268 (call 0x435281) | `4C 89 A5 10 01 00 00 48 C7 85 28 01 00 00 01 00 00 00` | `[STATIC]+[SPEC]` | inside ADDR-013 | FG off → no tags | read desc in ADDR-013 hook |
| ADDR-045 | NxD3D12Driver::LoadD3D12 (`+0x88` hmodule, `+0x90` D3D12CreateDevice) | 0x43BCD0 | json | `[STATIC]` | | | |
| ADDR-046 | Queue-wrapper table used by submission | data 0x1CE01F0 | 0x509FB0 | `[STATIC]` | | | |
| ADDR-047 | XInput function-pointer table (flag, GetCaps, GetState, SetState) | data 0x19B2678..0x19B2698 | 0x497EC0/0x498030/0x4983E0 | `[STATIC]` | | | |
| ADDR-048 | Class registry base (0xA0-byte entries by class id) | data 0x17B73C0 | static init 0x5EC40.. | `[STATIC]` | type guards | | vtable compare |
| ADDR-049 | **Cam_SetProjRect(cam, const float4* lbrt)** → +0x694, vslot 42, vslot 43 (native asymmetric projection; TAA jitter path) | 0x8068E0 | `40 53 48 83 EC 20 0F 10 02 48 8B 01 48 8B D9 0F 11 81 94 06 00 00` | `[STATIC]` | jitter writers ADDR-051 | patch | P20/P21 write after slot 42 |
| ADDR-050 | Cam_ResetProjRect → (−1,−1,1,1) | 0x806200 | json | `[STATIC]` | | | |
| ADDR-051 | Jitter writers (build jittered rect, call ADDR-049/050) | 0x618D30, 0xAF9850 | json | `[STATIC]` | per frame `[INFERENCE]` | | |
| ADDR-053 | **Pad XInput loader**: LoadLibraryW("xinput1_4.dll") then "xinput9_1_0.dll"; fills ADDR-047; called from NxInput init 0x44A090 | 0x497F30 | `[STATIC]`, EXP-00 run 1 `[LIVE]` negative for the 9_1_0 name | startup | | xinput1_4 proxy |
| ADDR-052 | Cam_RectToMatrix (NDC window → crop matrix; alternate branch under flag 0x1419F2400) | 0x8049E0 | json | `[STATIC]` | slot 42 | | |

Note on ADDR-001/002 (session 1): the render-view object can be **either** camera class; a guard must
accept both vtables (0x1183C60 and 0x1143850). Probe v0.1 accepted only the first; v0.2 accepts both and
logs which.

## Full derivation packets

Raw instruction windows for ADDR-003..010 are in `re/out/camera_api.asm` and `re/out/slconst_func.asm`,
for ADDR-014 in `re/out/present_callsite.asm`, and for ADDR-017 in `re/out/service_table_ctor.asm`.

Restart/level-transition validation: **none yet**. Every row needs a `[LIVE]` receipt before use.

## Session-3 anchors (frame / render-view layer)

| ID | Semantic owner | RVA | Guard / signature | Evidence |
|---|---|---|---|---|
| ADDR-054 | FrameView_5A07F0(frame): per-frame view setup inside the game tick | 0x5A07F0 | `40 53 55 56 57 41 54 48 83 EC 40 48 8B 05 ?? ?? ?? ??` | `[STATIC]` |
| ADDR-055 | View list gather 0x5A0540 (frame+0x1EA90 list) | 0x5A0540 | — | `[STATIC]` |
| ADDR-056 | RV_BindCamera (view[0] = cam) | 0x9B6AD0 | — | `[STATIC]` |
| ADDR-057 | RV_Prepare 0x9B6FB0 → RV_Snapshot 0x9B6AE0 (per-view constants) | 0x9B6FB0 / 0x9B6AE0 | — | `[STATIC]` |
| ADDR-058 | RV_Submit(view, mode) (cull/gather per view) | 0x9B6FE0 | — | `[STATIC]` |
| ADDR-059 | Secondary view registry (array 0x339E498, count 0x339E5B0) / list 2 (0x339BE38, count 0x339BE98) | data | — | `[STATIC]` |
| ADDR-060 | GameTick (Reflex SIM markers), hook target in GoTVR 0.4 | 0x4C49E0 | `48 89 5C 24 08 57 48 81 EC C0 00 00 00 FF 05 ?? ?? ?? ??` | `[STATIC]`; trampoline self-test PASS |

## Session 4 additions (2026-10-17)

| ID | What | RVA | Signature / guard | Grade | Used by |
|---|---|---|---|---|---|
| ADDR-061 | Engine frame id (Reflex frame token), u32, incremented as GameTick's first instruction | 0x19F1A50 | GameTick `FF 05 disp32` at +0x0D resolves to it (checked at runtime: `frame counter ... PASS`) | `[STATIC]` + runtime guard | 0.6 eye selection |
| ADDR-062 | Present descriptor `desc[0]` = frame id of the image being presented (Present Reflex markers 4/5 use it) | NxDXGI_Present param_2+0x00 | decompile 0x434CC0 lines 427–433 | `[STATIC]` | 0.6 frame tagging |
| ADDR-063 | `sl.interposer.dll!slGetNativeInterface` (Streamline proxy unwrap) | export | GetProcAddress | `[STATIC]` | 0.6 native swapchain/queue |
| ADDR-064 | `Jobs_SubmitGraph`: submits the pass-context pool (walks all contexts from 0; retire done, queue rest) — only caller RV_Post (0x9B5573) | 0x4D7EB0 | 32-byte signature `4C 8B DC 56 41 57 48 83 EC 58 8B 81 60 5C 2B 00 …` (unique) | `[STATIC]` | 0.7.3 R1 range-limited submit |
| ADDR-065 | Pass scheduler 0x141A0AB50: contexts +0x9290 (stride 0x2B20), allocated +0x2B5C60, completed +0x2B5C64; `Sched_Retire` 0x4DA510, `Sched_EndFrameWait` 0x4DBCA0 (waits completed == allocated), queue group 0x4DAC80, context job 0x4DADF0 | 0x1A0AB50 | via ADDR-064 code | `[STATIC]` + `[LIVE]` stall stack | 0.7.3 |
| ADDR-066 | VC/light-list frame end: bins lights into 32-px tiles, patches descriptors into the latest block (+0x1B0) and counts into slots, uploads the parity buffer; only caller GameTick 0x4C5213 (after Sched_EndFrameWait) | 0x77F340 | `40 55 53 41 54 48 8D 6C 24 D0 48 81 EC 30 01 00 00 48 8B 05 …` | `[STATIC]` | 0.7.4 light-list lend |
| ADDR-067 | Pass 13 view constants / light-list setup (block 0x81C from 0x141CDF6F0, parity buffer object at obj+(0xF+parity)*0x20, 4 slots) | 0x77E810 | via pass 13 stack (BUG-11) | `[STATIC]`+`[LIVE]` | 0.7.4 |
| ADDR-068 | GPU buffer object helpers: init 0x7E1E80, release 0x7E1F30, alloc 0x7E25B0, upload staging 0x7E2D70, descriptor 0x7E1FF0; resource record → native ID3D12Resource at +8: 0x501A10; context → native list: 0x501A00 (`**ctx`) | — | code | `[STATIC]` | 0.7.4 |
| ADDR-069 | Pass table 0x141210930 entries {pre, p1, p2, main}: pass 13 main 0x609760; pass 59 pre = StreamlinePass_cb 0x60F9E0, main 0x3FCBD0; upscaler output slot 0x141EBA130, input 0x141EBA148 | rdata | table dump | `[STATIC]` | 0.7.4 census |
| ADDR-070 | `Pass_Job` (thunk → 0x9B55D0): pass id ctx+0x2A50, view ctx+0x2A60, draw range view+0x13C4+pass*8 (begin,end); p1 setup = table+8; worker split fn 0x9B5B40/0x9B5C20 (pass 15) | 0x9B55B0 | `83 B9 50 2A 00 00 0F 48 8D 05 82 05 00 00 48 8D 15 5B 06 …` | `[STATIC]` | 0.7.5 census/bisection |
| ADDR-071 | Engine no-op pass callback `xor eax,eax; ret` | 0x3FCBD0 | `33 C0 C3` | `[STATIC]` | 0.7.5 F3 |
| ADDR-072 | Context job wrapper: calls ctx+0x2A58, then completed += 1 (0x141CC07B4 vs allocated 0x141CC07B0) | 0x4DADF0 | code | `[STATIC]` | 0.7.5 safety |
| ADDR-073 | Pass body (ctx, splitFn): reached from job thunk 0x9B55B0 and RV_Post inline route (call at 0x9B542C, return 0x9B5431) and 0x611587 | 0x9B55D0 | `4C 8B DC 49 89 53 10 49 89 4B 08 55 53 56 41 56 …` | `[STATIC]` | 0.7.6 census/skip |
| ADDR-074 | "Debug" inline-pass flag, **1 in the shipping .data**; per-view pass bitset view+0x15F0 (built in RV_Submit ~0x9B7074..0x9B72F3) selects passes run inline | 0x1803B60 | .data initial value | `[STATIC]` | 0.7.6 |
| ADDR-075 | `RCtx_Build(ctx, view)`: ctx[0]=view, ctx[2]=cam, +0x18 view+0x850 (lock), +0x20 view+0x840 (batch list), +0x68..0x90 global record/batch pools, +0x98 records (0x80 B), +0xA0 pass ids, +0xA8 count (cap 0x800), +0xB0.. FlushB items, +0x1B8/+0x1C0 lights | 0x9B2DD0 | `48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 …` | `[STATIC]` | 0.7.7 |
| ADDR-076 | `RCtx_FlushDraws(ctx)`: records → global pools by pass, batch → view list (also mid-way when full) | 0x9B49E0 | `4C 8B DC 55 56 57 48 81 EC D0 00 00 00 48 8B 05 …` | `[STATIC]` | 0.7.7 |
| ADDR-077 | `RCtx_Finish`: FlushDraws, FlushB (global 0x141EBC740), lights → VC 0x77D7D0 | 0x9B4970 | — | `[STATIC]` | 0.7.7 |
| ADDR-078 | Per-view draws: keys view+0x13B8 (u64, low16 = renderer slot), renderers view+0x13B0 (stride 0x80, +8 obj), pass ranges view+0x13C4; executor Pass_Draw 0x9B5B40 | — | — | `[STATIC]` | 0.7.7 |
| ADDR-079 | RCtx builders (return addresses): Frame_59FA60 0x59FB44 / 0x59FD48 (main only), 0x62EB00, 0x651C34, 0x65265C, 0x7840BC, visibility jobs 0xB0177B (every activated view) | — | — | `[STATIC]` | 0.7.7 census |
| ADDR-080 | Engine resource transition (ctx, record, D3D12 state): per-list tracked states at (*ctx)+0x28, pending barriers ctx+0x678 (count ctx+0xC78, flush at 0x20 or when 0x141CDF6C0) | 0x4FDD50 → 0x4FDED0 | `48 89 6C 24 10 48 89 74 24 18 57 48 83 EC 20 …` | `[STATIC]` | 0.8.0 left capture |
| ADDR-081 | Context barrier flush: list->ResourceBarrier(count, ctx+0x678) (vt+0xD0) + 0x4FE7D0 | 0x4FE780 | `40 53 48 83 EC 20 8B 91 78 0C 00 00 …` | `[STATIC]` | 0.8.0 |
| ADDR-082 | Swapchain wrapper getter ([0x141998300]); vt+0x118 = current back buffer target object (record split at +0x10/+0x14) | 0x4093C0 | `48 8B 05 39 EF 58 01 48 8B 00 C3` | `[STATIC]` | 0.8.0 |
| ADDR-083 | RV_Snapshot: camera matrices (+0x200.., +0x240.., +0x430.., +0x470..) → view+0x580.. (no +0x18 reset) | 0x9B6AE0 | `40 57 48 83 EC 40 48 8B F9 …` | `[STATIC]` | 0.8.0 right-eye refresh |
| ADDR-084 | Pass 0 pre-callback (first context of a render) | 0x60EDC0 | `48 89 5C 24 10 57 48 83 EC 30 …` | `[STATIC]` | 0.8.0 capture seam |
| ADDR-085 | `Cam_SetViewProjFrustum(cam, pos4, VPrel)`: +0x550 pos, +0x430 VPrel (camera-relative, row-vector), +0x4B0 derived, +0x240..+0x27C absolute VP = T(−pos)·VPrel, planes via 0x803BD0 | 0x806BA0 | `48 8B C4 48 89 58 10 55 56 57 48 81 EC D0 00 00 00 …` | `[STATIC]` | 0.8.1 right eye |
| ADDR-086 | Resource tracking core (ctx, per-buffer object, state) — key = object, object+8 = native resource | 0x4FDED0 | `48 89 5C 24 10 57 48 83 EC 40 48 8B D9 41 8B F8 …` | `[STATIC]` | 0.8.1 left capture |
| ADDR-087 | Engine resource table: [0x141487D38] → 0x18-byte entries (+0 → per-buffer object array); multi-buffer flag 0x141487EA8 (record +0x3C = sub index) | data | — | `[STATIC]` | 0.8.1 |
| ADDR-088 | Swapchain wrapper vt+0x118 = display settings block (+0x38, +0xE0, +0x13C HDR mode), NOT a render target | — | — | `[STATIC]` NEG | 0.8.1 |
| ADDR-089 | ClearRenderTargetView wrapper (ctx …) → list vt+0x180; callers 0x4DD5C0 (28 call sites in pass callbacks, e.g. pass 6 pre ×6) and 0x4DD530 | 0x4FA890 | — | `[STATIC]` | 0.8.2 analysis |
| ADDR-090 | ClearDepthStencilView wrapper (ctx, target+0x18 record, depth, stencil) → vt+0x178; via 0x4DD6B0 (UI pass 60 pre 0x6E3790, 0x650C00, 0x6F09A0) | 0x4FAA10 | — | `[STATIC]` | 0.8.2 analysis |
| ADDR-091 | `Clock_Update(clock=0x141489DF0)`: only caller GameTick 0x4C4B9A; +0x00 real dt, +0x08 game dt (GameTick gate), +0x18/+0x38/+0x48 scaled steps, +0x28 = +0x30·real dt (camera-update gate) | 0x56ED80 | `48 89 5C 24 18 57 48 83 EC 50 48 89 6C 24 60 …` | `[STATIC]` | 0.9.0 freeze |
| ADDR-092 | `Cams_StorePrev` loop: every camera of the list at 0x141EDBCC8 (+0x6F0 next) → 0x8085D0; GameTick 0x4C4F61 when clock+0x28 ≠ 0 | 0x59C710 | — | `[STATIC]` | 0.9.1 BUG-22 |
| ADDR-093 | `Cam_StorePrev(cam)`: +0x1C0←+0x140 (world), +0x180←+0x100, +0x380←+0x430 (VPrel), +0x3C0←+0x470 (unjittered VPrel), +0x560←+0x550 (pos), +0x674←+0x670 (time) | 0x8085D0 | `0F 10 81 40 01 00 00 8B 81 70 06 00 00 0F 11 81 C0 01 …` | `[STATIC]` | 0.9.1 |
| ADDR-094 | `Cam_TemporalUpdate(cam)` (loop 0x59CB80 ← GameTick 0x4C502C, with 0x808970): cut +0x7D8 → prev = current; reprojection +0x300..+0x370, +0x2C0..+0x2FC; dt +0x650; motion +0x640; slots 42/43 unjittered (+0x694 cleared; +0x280←+0x240, +0x470←+0x430) then jittered | 0x808EF0 | `48 8B C4 48 89 58 10 57 48 81 EC B0 01 00 00 …` | `[STATIC]` | 0.9.1 (hooked: cut count) |
| ADDR-095 | Frame packet [0x1433761E0] (set 0x606719, cleared 0x606EC7) +0x18 → render-target array +0x88 + i·0xA8, current index +0x620 (read by the Streamline/TAA callbacks 0x60A089, 0x60F8F3 …) — candidate history ring for a per-eye texture bank | data | — | `[STATIC]` lead | 0.9.1 |
| ADDR-096 | Double-buffer index p (game→render slots): 0x14148A3F8 (+0xC twin), toggled per tick by 0x577820 (GameTick end); previous slot = 1 − p | data / 0x577820 | — | `[STATIC]` | 0.9.2 BUG-23 |
| ADDR-097 | Camera-cut readers (cmp [reg+0x7D8]): V 0x9BB95A, 0x9E5393, 0x631B92; T1 0x618D9A, 0x6149FE, 0x610CD0, 0x609BDD, 0x60FD07, 0xAF98C0; T2 0x63D246, 0x63D8BF, 0x63E082, 0x60C922 (patched since 0.9.2); native: 0x4DB3DA, 0x67316F, 0x67363D, 0x678285, 0x66E958, 0xA461A2, 0xA57E3C, 0xA7432A, 0x612ED7 | code | site bytes in gotvr.cpp `cutp::kSites` | `[STATIC]` + self-test executed | 0.9.2 |
| ADDR-098 | AA/post object 0x143376280: +0x1C00C/+0x1C010 last frame ids (history valid iff == frame−1), +0x1D40D/E/F reset flags, +0x11618 + p·0x628 history ping-pong records, +0x192A·0x10 jitter table (frame & 7), +0x1A599 T2x mode | data | — | `[STATIC]` | 0.9.2 |
| ADDR-099 | RV_OcclusionSetup (main-view HiZ test setup): source record view+0x110 (slots 0x143389240 + i·0x20, dims 0x1433891A4 + i·0x34), prev camera → +0x60..+0xEC; enable flag 0x141804010 | 0x9E5350 | `48 8B C4 53 41 54 48 81 EC 88 00 00 00 …` | `[STATIC]` + headset (BUG-26) | 0.9.4 |
| ADDR-100 | AA_Resolve (TAA): history record AA+0x19320 (prev scene colour), output AA+0x8788 → AA+0x640, velocity AA+0xEA90 | 0x614EB0 | `40 56 57 48 81 EC C8 00 00 00 …` | `[STATIC]` | 0.9.5 |
| ADDR-101 | TAA_Draw (shader, ctx, cam, scene, history, velocity, exposure…), only caller AA_Resolve | 0x7E7D10 | `40 55 53 56 57 41 54 41 56 41 57 48 8D 6C 24 F9 …` | `[STATIC]` | 0.9.5 |
| ADDR-102 | AA_Velocity (AA, ctx, cam): camera reprojection rows cam+0x400..+0x42C → velocity AA+0xEA90; only caller 0x610D15 | 0x614720 | `48 89 5C 24 20 55 56 57 41 56 41 57 …` | `[STATIC]` | 0.9.5 |
| ADDR-103 | FxBindCamera → RV_9A2610: camera constants from +0x240, +0x340..+0x37C, +0x430..+0x46C, +0x550, +0x694, +0x828 | 0x56DF80 / 0x9A2610 | — | `[STATIC]` | 0.9.5 |
| ADDR-104 | Camera world copy +0x850..+0x88F (= +0x140 rows as computed by Cam_UpdateWorld, before slot 43); read by 0x611E00, 0x649ED0, 0x728260, 0x74F1F0, 0x5A4740 … | 0x80854E | — | `[STATIC]` + headset (BUG-35) | 0.10.2 |
| ADDR-105 | Cam_BuildSLConstants (cam): sl::Constants from +0x200/+0x2C0/+0x550/+0x694/+0x7D8 → slSetConstants(viewport 0) | 0x807140 | `48 89 5C 24 10 55 48 8D AC 24 00 FD FF FF 48 81 EC 00 04 00 00 48 8B 05` | `[STATIC]` | 0.11.0 |
| ADDR-106 | StreamlinePass_cb DLSS inputs: motion vectors AA+0xEA90 (0x143384D10), depth ring [frame+0x18]+0x88+(+0x620)·0xA8, output 0x141A0B8D0, exposure [0x141EBA130]+0x18 | 0x60F9E0 | — | `[STATIC]` | 0.11.0 |
| ADDR-107 | NGX shim exports (static NGX SDK in the exe): CreateFeature 0x4B91B0, EvaluateFeature_C 0x4B9300, ReleaseFeature 0x4B96B0 (pointers 0x1419E0208/210/218); engine callers 0x469EA8 / 0x46AE54 / 0x46AED8 | 0x4B91B0.. | see `kSigs` | `[STATIC]` | 0.11.1 |
| ADDR-108 | RT_Bind (ctx, target, rect\|null, slice): binds colour/depth, Ctx_SetViewport + scissor from rect, ctx+0x2940 = target, ctx+0x2948..54 = rect; back-buffer targets (target+0xC8 & 0x400000) via 0x4F9540 | 0x7E34E0 | `4C 8B DC 56 41 55 41 57 48 81 EC A0 00 00 00 …` | `[STATIC]` | 0.12.0 |
| ADDR-109 | Ctx_SetViewport (ctx, x0, y0, x1, y1) → ctx+0x310 D3D12_VIEWPORT, list vt+0xA8; only caller RT_Bind | 0x50ECA0 | `0F 57 C0 8B C2 4C 8D 91 10 03 00 00 …` | `[STATIC]` | 0.12.0 |
| ADDR-110 | Ctx_SetScissor (ctx, l, t, r, b) → ctx+0x328 RECT, list vt+0xB0; callers RT_Bind, Ctx_ScissorNorm 0x7E52C0 (NDC rect × bound rect) | 0x50EC60 | `8B 44 24 28 89 81 34 03 00 00 …` | `[STATIC]` | 0.12.0 |
| ADDR-111 | UI render target object (passes 60/61/64; bound by 0x6E3790 / 0x6F0860 / inherited via Ctx_InheritState 0x7E5170) | 0x143377510 | via `UI_Pass60_pre` sig (lea at 0x6E379C) | `[STATIC]` | 0.12.0 |
| ADDR-112 | Movie_Draw (fx, ctx, Y/CbCr textures…): full-screen video into the bound back buffer 0x141A0B848, 16:9 letterbox from the display aspect; only caller Movie_Render 0x7A1040 (movie object 0x1434B6FA0: +0x74 playing count, +0x1D8 state 1/2 = draw) | 0x7EAF80 | `48 8B C4 55 53 56 57 41 56 41 57 48 8D 68 98 …` | `[STATIC]` | 0.12.0 |
| ADDR-113 | World rendered flag: byte frame(0x141EBD2C0)+0x80F09 (reader 0x5A0450); 0 → RV_Post schedules passes 59..68 only (menus/loading without a 3D world) | 0x5A0450 | `0F B6 81 09 0F 08 00 C3` | `[STATIC]` | 0.12.0 |
| ADDR-114 | Clock_SetScale (clock 0x141489DF0, reason, scale): reasons at clock+0x84 (28 floats); 7 = pausing UI screens (refcount 0x1434C28F0; 0x7D6E10 / 0x7D7240), 8 = video (0x7A1190), 9 = world load (0x59AAC0) | 0x56F340 | — | `[STATIC]` | 0.13.0 |
| ADDR-115 | Ctx_BindColor (ctx, slot, attachment): ctx+0x2C8[slot] = attachment, attachment+0x18 = resource record → Res_Transition(…,4), ctx+0x340 \|= 1 (lazy bind) | 0x50E430 | — | `[STATIC]` | 0.13.0 |
| ADDR-116 | Pass 65 pre 0x610460 → 0x728400: full-screen fade colour (alpha > 1/255, world mode != 1) | 0x728400 | — | `[STATIC]` | 0.13.0 |
| ADDR-117 | Display service: services(0x141998300)+0xC8 (getter 0x409450); vt+0x70(name) = mode active, vt+0x78 push, vt+0x88 pop; "Letterbox" 0x141114870 (cinematics; UI layout 0x6E9544 reads it) | 0x409450 | `48 8B 05 A9 EE 58 01 48 8B 80 C8 00 00 00 C3` | `[STATIC]` | 0.14.0 |
| ADDR-118 | UI target colour (live): separate 2560×1440 R8G8B8A8_UNORM, flags 0x5, target flags 0x20000009, 1 attachment; composited after pass 65 | — | runtime probe | `[LIVE]` | 0.13.0 log |
| ADDR-119 | Clock reason 27 (Clock_Update: 0 while frame+0xA0 != 0): 0 from boot through the start screen until the first world is loaded, briefly before loads | 0x56EEFF | — | `[LIVE]` census | 0.13.0 log |
| ADDR-120 | Ctx_FlushRT(ctx): if ctx+0x340 & 1: CreateRenderTargetView per colour attachment (ctx+0x2C8[i] → +0x18 record → native obj+8) at ctx+0x338·[0x141CDFB28]+[0x141CDFAA8], DSV for ctx+0x308 at ctx+0x33C·[0x141CDFBD8]+[0x141CDFB58], then list(**ctx)->OMSetRenderTargets(n, &rtv, TRUE, dsv?) (vt+0x170); clears bit 1 | 0x4FEA80 | `4C 8B DC 41 57 48 83 EC 70 F6 81 40 03 00 00 01 4C 8B F9 0F 84 3C 03 00 00` | `[STATIC]` | 0.15.0 |
| ADDR-121 | DSV handle code in Ctx_FlushRT (globals verified by bytes) | 0x4FED6D | `41 8B 8F 3C 03 00 00 0F AF 0D 5D 0E 7E 01 48 03 0D D6 0D 7E 01 48 85 D2` | `[STATIC]` | 0.15.0 |
| ADDR-122 | Pass context: +0x2A50 pass index, +0x2A60 view (Pass_Body 0x9B55D0) - used to arm UI passes 60..65 only | — | — | `[STATIC]` | 0.15.0 |
| ADDR-123 | "Letterbox" display mode = a SETTINGS toggle: setting callback 0x140D160C0 → 0x996740 (push vt+0x78 / pop vt+0x88); NOT cinematics (corrects ADDR-117) | 0x996740 | — | `[STATIC]` NEG | 0.15.0 |
| ADDR-124 | Cinematic bars: UI widget `CUiLetterbox` (class name table 0x1417A8508), script `PushLetterbox`/`PopLetterbox` (0x1417A18B8), tunables TRIGGER/UNTRIGGER_LETTERBOX_SPEED; state not located - detected on the UI image instead | — | — | `[STATIC]` | 0.15.0 |
| ADDR-125 | HUDScaling setting (0x1411341A0) → UI layout width 0x6E3210: 1 = 1920 (16:9), 2 = 2520 (21:9), 3 = 3840 (32:9), else native | 0x6E3210 | — | `[STATIC]` | 0.15.0 |
| ADDR-126 | Camera fov camA+0x670: gameplay 0.935 rad; cutscene camera keeps its own (1.028 in the prologue sequence) for the whole scene; title 0.789 | camA+0x670 | — | `[LIVE]` | 0.15.4 log |
| ADDR-127 | Camera projection rect camA+0x6A4 (l, b, r, t): (-1,-1,1,1) normally; (-1,-0.758,1,0.758) during 2.35:1 letterboxed shots (engine letterbox, black rows in the 3D picture) | camA+0x6A4 | — | `[LIVE]` | 0.15.4 log |
