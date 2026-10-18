# Integration authority map

| Layer / subsystem | Artifact | Readable | Buildable | Modifiable | Shippable | Retail-compatible | Route | Evidence / caveat |
|---|---|---:|---:|---:|---:|---:|---|---|
| Frame/simulation loop | GhostOfTsushima.exe (SP core) | asm only | no | patch/hook | as a hook DLL | yes | RE | Reflex markers locate tick/submit/present |
| Camera/view construction | CAM-A (SP core) | asm only | no | hook/write | DLL | yes | RE | CAMERA_FINDINGS.md |
| Culling/visibility | CAM-A vslot 43 + SP core | asm | no | hook | DLL | yes | RE | off-center aware |
| Native renderer | D3D12 frame graph (SP core) + Nixxes D3D12 driver | asm, RTTI in nx layer | no | hook | DLL | yes | RE | pass-callback table in .rdata |
| Script/managed game layer | none found | — | — | — | — | — | — | negative, string search only |
| PC platform layer | Nixxes `nx::` (RTTI, service table) | asm + type names | no | vtable hooks | DLL | yes | RE (easiest layer) | NX_SERVICE_MAP.md |
| Input/action layer | NxInput / NxGamepad + XInput/Steam/libScePad | asm | no | proxy/hook | DLL | yes | RE | INPUT_OWNERSHIP.md |
| UI/HUD | proprietary (no Scaleform/Coherent/Noesis strings) | asm | no | OPEN | | | RE | OPEN |
| Audio | Wwise (exported API) | exported symbols | no | hook exports | DLL | yes | RE | listener: `AK::SoundEngine::SetPosition`/`SetListeners` exported |
| Assets/content | psarc archives | format OPEN | — | — | — | — | not needed | |

Primary delivery route: **RE-owned** (closed retail binary). The `nx::` layer is a hybrid-friendly island
with typed interfaces.
Closed boundaries requiring RE: all of the SP core (camera writer, render graph, gameplay, UI).
Source/SDK/oracle versions: Streamline 2.4.0 headers (public) for `sl::Constants`; NVIDIA Reflex marker enum
(public); Wwise SDK (public headers) for the audio exports.
License/redistribution constraints: ship only our own DLL; never redistribute game binaries.
Decision receipt: this session's static analysis (re/out/*).
