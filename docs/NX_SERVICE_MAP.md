# Nixxes `nx::` service layer map

The Nixxes PC layer keeps MSVC RTTI. The Sucker Punch core does not. Services hang off one table pointed to
by the global at **RVA 0x1998300** (allocated 0x120 bytes in ctor 0x408BC0, freed in 0x408EE0).
Each service has a one-instruction getter, `mov rax,[rip→0x1998300]; mov rax,[rax+off]; ret`.

Slot → implementation was resolved by following each constructor to the vtable it installs `[STATIC]`.

| Table off | Service impl | Getter VA | Impl vtable RVA | Slots |
|---|---|---|---|---|
| 0x000 | NxAppImpl | 0x1404093C0 | 0x10F8110 | 103 |
| 0x008 | NxResourceLoaderImpl | 0x140409510 | 0x1101E50 | 8 |
| 0x010 | NxAssertImpl | | 0x10F4088 | 3 |
| 0x018 | NxExceptionsImpl | 0x140409420 | 0x10FB9E0 | 7 |
| 0x028 | NxLogImpl (vslot 9 `+0x48` = log(category, fmt, …)) | 0x1404094A0 | 0x10FFD60 | 13 |
| 0x030 | **NxSettingsImpl** | 0x140409530 | 0x1102660 | 28 |
| 0x038 | NxLocalizationImpl | 0x140409490 | 0x10FF8E0 | 8 |
| 0x040 | NxMonitorImpl | 0x1404094D0 | 0x1100A70 | 11 |
| 0x048 | NxDXGIImpl (Present wrapper = vslot 10) | 0x140409400 | 0x10F9B68 | 26 |
| 0x050 | NxD3DImpl | 0x1404093F0 | 0x10FAC50 | 56 |
| 0x058 | NxADLImpl | | 0x10F4C18 | 23 |
| 0x060 | NxNvidiaImpl | 0x1404094F0 | 0x1101818 | 37 |
| 0x068 | NxBCryptImpl | | 0x1101968 | 2 |
| 0x070 | (unnamed; 0x1AD0-byte object, ctor 0x4141A0) | 0x1404094B0 | — | — |
| 0x078 | **NxSwapChainImpl** | | 0x11054B8 | 23 |
| 0x080 | NxSystemInformationImpl | | 0x11055E0 | 7 |
| 0x088 | NxSteamImpl | | 0x1103358 | 53 |
| 0x098 | NxMiniDumpImpl | 0x1404094C0 | 0x10FFF08 | 2 |
| 0x0A0 | NxFailDialogImpl | 0x140409430 | 0x10FBDB8 | 4 |
| 0x0A8 | NxGameWindowImpl | 0x140409460 | 0x10FC4E0 | 20 |
| 0x0B0 | **NxInputImpl** | 0x140409470 | 0x10FD5A8 | 85 |
| 0x0B8 | NxMovieImpl | 0x1404094E0 | 0x10FD978 | 2 |
| 0x0C0 | NxActionsImpl | 0x1404093B0 | 0x10F3F88 | 16 |
| 0x0C8 | **NxGameStateImpl** | 0x140409450 | 0x10FC078 | 19 |
| 0x0D0 | NxLauncherImpl | 0x140409480 | 0x10FDF80 | 14 |
| 0x0D8 | NxBenchmarkImpl | 0x1404093D0 | 0x10F8BE8 | 50 |
| 0x0E0 | NxConfigDialogImpl | 0x1404093E0 | 0x10F92F0 | 3 |
| 0x0E8 | **NxUpscalerImpl** | | 0x1104DA0 | 9 |
| 0x0F0 | **NxReflexImpl** (vslot 5 `+0x28` = SetMarker(marker, frame); vslot 24 `+0xC0` = frame token) | 0x140409500 | 0x1105A80 | 25 |
| 0x0F8 | NxModulePrefetchImpl | | 0x1105C00 | 3 |
| 0x100 | NxNRDImpl | | 0x1105C58 | 5 |
| 0x108 | NxSaveImpl | | 0x1106A78 | 16 |
| 0x110 | NxVoiceImpl | | 0x1107678 | 12 |
| 0x118 | NxFfxImpl | 0x140409440 | 0x1108250 | 23 |

Other RTTI vtables of interest (`re/out/vtables.json` lists all of them):
`NxD3D12Driver` 0x10FAF58 (20), `GamepadXInput` 0x110ED40, `GamepadSce` 0x110F508, `GamePadSteam` 0x1109168,
`GamepadWindowsInput` 0x10FD528 (each 15 slots, base `NxGamepad` 0x10FD478), `NxKeyboardDevice` 0x110E190,
`NxMouseDevice` 0x110E3C8.

## Reflex markers = an engine-annotated frame timeline `[STATIC]`

| Marker | Site | Function |
|---|---|---|
| INPUT_SAMPLE (6) | 0x4C4A4B | 0x4C49E0, the game tick |
| SIMULATION_START (0) | 0x4C4AB8 | 0x4C49E0 |
| SIMULATION_END (1) | 0x4C4FB1 | 0x4C49E0 |
| RENDERSUBMIT_START (2) | 0x50A016 | 0x509FDB |
| RENDERSUBMIT_END (3) | 0x50A0EA | 0x509FDB |
| PRESENT_START (4) | 0x43540E | 0x434CC0, the Present wrapper |
| PRESENT_END (5) | 0x43543D | 0x434CC0 |

Marker numbering is NVIDIA Reflex's public `NV_LATENCY_MARKER_TYPE` order `[AUTHOR]`. It is consistent
with placement: 4 immediately before and 5 immediately after `IDXGISwapChain::Present`. This gives RE-04 its
candidate boundaries without guesswork: one simulation frame = SIM_START..SIM_END in 0x4C49E0, and the
render frame ends at Present. Thread identity and cadence remain `[LIVE]` OPEN.

## Dormant stereo / VR code in the Nixxes layer `[STATIC]`

Settings block (two copies, stride 0x148, at NxSettingsImpl+0x48):

| Offset | Meaning | Evidence |
|---|---|---|
| +0x040 | FOV setting (clamped) | "Changed FOV." |
| +0x0BC | stereo mode: 0 off, 1 side-by-side (disabled in windowed), 2 NVIDIA 3D Vision (needs exclusive fullscreen), **3 = VR** | "VR change caused swapchain recreate" is logged when `(old==3) != (new==3)` in NxSwapChainImpl vslot 7 |
| +0x0C0 | stereo separation | "Changed stereo separation." |
| +0x0C4 | stereo popout | "Changed stereo popout." |
| +0x0C8 | near separation multiplier | "Changed stereo near separation multiplier." |

The validator 0x428420 resets mode 3 at its first stereo check (`if mode==3: mode = r13d`), so VR mode is
forced off in this build. None of the stereo fields appear in the persisted registry value list, and no
command-line switch enables them. Treat this as shared Nixxes framework code that may or may not have a
renderer consumer in this game; that is EXP-04.
