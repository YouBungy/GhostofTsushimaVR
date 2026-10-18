# D3D12 transport and frame ownership (PORT-08 groundwork, static)

Target runtime: OpenXR with `XR_KHR_D3D12_enable`. Owner hardware: Quest 3 over **Virtual Desktop
(VDXR OpenXR runtime)**, roomscale; **SteamVR OpenXR** must also work. Both runtimes support the D3D12
binding, so one code path serves both. Static only; every row needs a `[LIVE]` receipt (probe v0.2 logs
the first four rows).

## The objects OpenXR needs, reachable without hooks

```
*(base+0x1998300)            Nx service table (0x120 bytes)
  +0x50 -> NxD3DImpl         vtable 0x10FAC50 ; +0x160 apiMode (3 = D3D12) ; +0x168 driver
             +0x168 -> NxD3D12Driver   vtable 0x10FAF58, 0x118 bytes (alloc in NxD3DImpl::Init 0x43BDB0)
                         +0x30  queue records, 0x40 bytes each, one per GPU node; record+0 = ID3D12CommandQueue*
                         +0x38  ID3D12Device*
                         +0x80  node/record count
                         +0x88  HMODULE d3d12.dll ; +0x90 PFN D3D12CreateDevice ; +0x98 PFN D3D12SerializeRootSignature
```

| Item | Evidence | Grade |
|---|---|---|
| Driver allocated only for apiMode 3, else a D3D11 driver (vtable 0x10FAEB8, 0xE8 bytes) | 0x43BDB0 | `[STATIC]` |
| `D3D12CreateDevice` resolved with `GetProcAddress` into `+0x90`; called with FL 11_0 (0xB000), IID_ID3D12Device `{189819f1-1db6-4b57-be54-1821339b85f7}`, out = `+0x38` | 0x43BCD0, 0x43801A | `[STATIC]+[SPEC]` |
| `slSetD3DDevice([driver+0x38])` | 0x4381E6 | `[STATIC]` |
| `device->CreateCommandQueue(&desc, IID_ID3D12CommandQueue {0ec870a6-…}, &records[i])`, desc = {Type 0 DIRECT, Priority 0, Flags 0, NodeMask 1<<i}, loop i < `+0x80` | 0x43846F..0x438498 (r12d zeroed at 0x43814F) | `[STATIC]+[SPEC]`, verifier C8 |
| NxD3DImpl slot 43 forwards to `device->CreateCommandQueue` (so `+0x168` is the driver) | 0x4387D0 | `[STATIC]` |
| Swapchain: `IDXGIFactory2::CreateSwapChainForHwnd` (vtable +0x78) in `NxDXGI_CreateSwapchain` 0x4335E0; FSR3 frame-interpolation swapchain variant exists | 0x433AA9 + strings | `[STATIC]` |

**OpenXR binding:** `XrGraphicsBindingD3D12KHR{ device = *(drv+0x38), queue = *(u64*)*(drv+0x30) }`.
Required check: `device->GetAdapterLuid()` == `xrGetD3D12GraphicsRequirementsKHR().adapterLuid`
(probe v0.2 logs the LUID; VDXR/SteamVR on a single-GPU PC should match).

Hazard: Streamline (`sl.interposer.dll`) is statically imported. The game creates the device through the
system `d3d12.dll` it loaded itself, so the pointer should be native, but confirm `[LIVE]` that no SL proxy
object is in the chain (compare the device vtable module with `d3d12.dll`/`D3D12Core.dll`).

## Frame pipeline (threads)

| Stage | Function | Thread | Reflex marker | Grade |
|---|---|---|---|---|
| Game tick (input sample, simulation) | 0x4C49E0 (called only from 0x4C49C0) | game thread | INPUT_SAMPLE, SIMULATION_START/END | `[STATIC]` |
| Camera update (driver → `Cam_SetLocalMatrix`) | 0xA3xxxx / 0x673610 via virtual dispatch | game thread `[INFERENCE]` | — | `[STATIC]` path, thread `[OPEN]` |
| Render-command build | render graph (pass callback tables in .rdata 0x1210E00..) | render/job threads `[INFERENCE]` | — | `[OPEN]` |
| **GPU submission** | `RenderSubmitThread` 0x509FB0: waits on `[+0x328]`, runs a command list `[+0x230]` (count `[+0x238]`, stride 0x80): 0 = ExecuteCommandLists, 1 = Signal, 2 = Wait, 3 = Present; then `SetEvent([+0x320])` | dedicated submit thread | RENDERSUBMIT_START/END | `[STATIC]` |
| Queue wrappers used by submission | table at 0x1CE01F0, indexed by command.queueIndex (vtable +0x50 Execute, +0x70 Signal, +0x78 Wait) | | | `[STATIC]` |
| Present | `NxDXGI_Present` 0x434CC0 (NxDXGIImpl slot 10) → `IDXGISwapChain::Present` at 0x435437 | submit thread `[INFERENCE]` | PRESENT_START/END | `[STATIC]` |

Consequence: the renderer is **pipelined** (simulation of frame N overlaps GPU submission of N−1). The
pose used to write the camera must travel with its frame to the submit thread. Design: one
`PosePacket` per `xrWaitFrame` (predicted display time), latched by the camera hook, keyed by the Reflex
frame token (`NxReflexImpl` slot 24) and consumed at the Present hook for `xrEndFrame`. Exactly one owner
for wait/begin/end (playbook PORT-08).

## Present descriptor: HUD-less scene and UI layer (big UI win)

`NxDXGI_Present(this, desc)` receives a descriptor holding four resources, each followed by its resource
state dword. It tags them for Streamline with `slSetTag` (4 tags, 0x435281, command buffer `desc+0x48`)
**only when** the depth resource is non-null and the mode `[obj+0x13C] == 2` (frame generation active):

| desc offset (state) | Tagged as | Streamline type `[SPEC]` | Meaning for VR |
|---|---|---|---|
| +0x08 (+0x10) | `kBufferTypeMotionVectors` | 1 | motion vectors → optional runtime reprojection hints |
| +0x18 (+0x20) | `kBufferTypeDepth` | 0 | scene depth (reversed-Z) → `XR_KHR_composition_layer_depth` for positional reprojection |
| +0x28 (+0x30) | `kBufferTypeHUDLessColor` | 2 | scene colour **without HUD** → eye image |
| +0x38 (+0x40) | `kBufferTypeUIColorAndAlpha` | 23 (0x17) | UI/HUD RGBA → compositor quad layer / wrist panel |

`[STATIC]` (independently re-derived, VERIFICATION.md C10; the session-2 first draft had the offsets wrong).
**Whether these are populated when frame generation is off is `[OPEN]`** (EXP-06). If they are not,
enabling DLSS-G/FSR-FG in the menu and then suppressing the actual interpolation may still produce the
buffers. That needs testing, not assuming.

## Stereo transport options (to be selected in RE-09)

| Option | Mechanism | Pros | Cons |
|---|---|---|---|
| T-A: copy per-eye HUD-less colour (desc+0x28) and depth (desc+0x18) at Present into XR swapchain images | `CopyTextureRegion` on the direct queue in the Present hook | simplest, mono UI handled separately | needs two renders (R1) or AFR (R3) |
| T-B: render directly into XR images | redirect the scene RT | no copy | requires render-graph RT ownership `[OPEN]` |

Frame generation (DLSS-G, FSR3-FG) must be **off** in VR (it interpolates a single view). Upscalers
(DLSS SR/FSR/XeSS) can stay but need per-eye history.
