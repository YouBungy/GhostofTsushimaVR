# Target profile — Ghost of Tsushima DIRECTOR'S CUT (Steam, PC)

Session: 2026-10-17, static-only (no game launch). Gate: RE-01.
Grades: `[SPEC]` `[STATIC]` `[LIVE]` `[HEADSET]` `[AUTHOR]` `[INFERENCE]` per playbook.

| Field | Value | Evidence | Invalidation |
|---|---|---|---|
| Game/store/build | Steam, `E:\SteamLibrary\steamapps\common\Ghost of Tsushima DIRECTOR'S CUT` | install tree listing | Steam update |
| Main executable | `GhostOfTsushima.exe`, 29,324,752 bytes | `[STATIC]` | any patch |
| Main executable SHA-256 | `ED0931B6F6D3CA3874BAF39EBC16D95FF0E93296891182F96C3A790DA7109F63` | `[STATIC]` hashed on the PC (Get-FileHash) **and** on the reassembled copy; identical | any patch |
| PE link timestamp | 1782230973 = 2026-06-23 16:09:33 UTC | `[STATIC]` | rebuild |
| Logic module | The EXE itself; no separate engine DLL. Sucker Punch core engine + Nixxes `nx::` porting layer + Wwise + NGX + LZ4 all statically linked | `[STATIC]` exports/RTTI | — |
| Architecture | x64 (machine 0x8664), PE32+, GUI subsystem, image base 0x140000000, ASLR (DllCharacteristics 0x8160) | `[STATIC]` | — |
| Packer / DRM wrapper | None detected: 7 standard sections (.text .rdata .data .pdata _RDATA .rsrc .reloc), .text entropy 6.51, entry point in .text | `[STATIC]` | — |
| Engine/generation | Sucker Punch proprietary engine (no RTTI in core) under a Nixxes PC layer (`nx::INx*` interfaces with RTTI) | `[STATIC]` RTTI names, `-nixxes`, `NIXXES log` strings | — |
| Active graphics API | D3D12 (Agility SDK: exports `D3D12SDKVersion`/`D3D12SDKPath`, ships `D3D12\D3D12Core.dll`). D3D11 code path also present (`NxD3D11Driver`) | `[STATIC]` only. `d3d12.dll`/`dxgi.dll` are **not** in the import table; they are loaded dynamically (`Failed to load d3d12.dll`) | Must be confirmed `[LIVE]` from loaded modules: the playbook warns imports lie |
| Upscalers / frame gen | DLSS + DLSS-G via Streamline 2.4.0 (`sl.interposer.dll` statically imported), NGX statically linked, FSR3 (`amd_fidelityfx_dx12.dll`), XeSS (`libxess.dll`), HBAO+ (`GFSDK_SSAO_D3D12`), NRD | `[STATIC]` | settings |
| Swapchain proxy | `sl.interposer.dll` is in the import table; Streamline can proxy DXGI. `-noStreamline` switch exists | `[STATIC]` | launch args |
| Active runtime | planned: VDXR (Virtual Desktop) primary, SteamVR OpenXR secondary | owner | user runtime switch |
| Game adapter LUID | OPEN; probe v0.2 logs `device.GetAdapterLuid()` | needs `[LIVE]` | GPU change |
| Script/managed loader | None found. No Lua/managed layer identified | `[STATIC]` negative, string-search only | |
| Anti-cheat | No EAC/BattlEye/anti-cheat strings or modules. Online "Legends" mode exists (WinHTTP, websockets, PSN SDK). **Single-player only for VR**; do not test injected builds online | `[STATIC]` negative (string search; not a proof of absence) | |
| PSN SDK | PlayStation PC SDK runtime 2.50 (`PsPcSdk.dll`, libScePad for DualSense) | `[STATIC]` | |
| Audio | Wwise, exported by name (`AK::SoundEngine::*`, incl. `AddListener`, `SetPosition`) + XAudio2 | `[STATIC]` exports | |
| Input | Nixxes `INxInput` with `GamepadXInput`, `GamepadSce` (libScePad), `GamePadSteam` (SteamInput006), `GamepadWindowsInput` (Windows.Gaming.Input), keyboard/mouse/raw input, HID | `[STATIC]` RTTI + strings | |
| Settings storage | Registry `HKCU\Software\Sucker Punch Productions\Ghost of Tsushima DIRECTOR'S CUT` | `[STATIC]` string | |

Known launch switches (from strings, `[STATIC]`): `-nolauncher`, `-benchmark`, `-d3ddebug`,
`-noStreamline`, `-allowDRSVsync`, `-fsrDebugDraw`, `-pixelAspectRatio`, `-enablesteamoverlay`,
`-disablesteamoverlay`, `-unlockReflexOptions`, `-disallowDLSSGOffInMenu`.

Known launch preconditions: PlayStation PC SDK runtime installed by Steam install script. OPEN otherwise.
Known capture/debugger conflicts: OPEN (Streamline interposer + RenderDoc compatibility untested).
Vanilla control receipt: OPEN — RE-03 has not started.

## Companion module hashes (staged, SHA-256)

| Module | SHA-256 | Version |
|---|---|---|
| sl.interposer.dll | aa20b751432b1ba19a06379117a4ada8a8ba37c882e06a6203dd52fc8246bb1f | NVIDIA Streamline 2.4.0.0 |
| steam_api64.dll | a44e5537939ae4eebc69000589aa9b2437a667813a1657cc779198bae9b815a9 | |
| dstoragecore.dll | 77c7b3acf5b5a9acec6ad3ed7aa3c33e1ad43e73116b437adfcd26965a079b2b | |
| amd_ags_x64.dll | 311328f5e06202b83880b6f4917dddca9fc2157c960933d17857a119d7c8169f | |
| default.pso | 195d447a21b1bf1fea2869178c1be283a0e6a396558ea789c54048cf46e87c6f | proprietary/compressed PSO cache |

## Prior art

No playbook chapter, source or project covers this game, Sucker Punch's engine or a Nixxes D3D12 port
(`grep` of the full docs corpus for Tsushima / Sucker Punch / Nixxes: zero hits). `tools/prior_art.py`
was not run (Python tooling on the PC not used this session); run `python tools\prior_art.py "nixxes" d3d12`
once the D3D12 API is confirmed live.
