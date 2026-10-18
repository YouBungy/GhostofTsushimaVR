# GhostofTsushimaVR
(WIP) GOT VR mod with 6DGOF, native stereo rendering, and motion controls.

# Important Notes
- TAA Only; DLSS caused more performance loss and jitter. Will investigate later
- Set to windowed for increased performance, it downsizes the window to a smaller resolution to save overhead.

# Current version: GoTVR 0.14.5 (WIP: towards proper UI)
- **xr=4 synchronized stereo:** true per-eye stereo with 6DoF. Both eyes are rendered from the same simulation tick and presented as complete pairs. The monitor shows only the left eye.
- **Per-eye temporal state:** each eye has its own occlusion/culling history and TAA history and reprojection, with fast-head-turn handling and a 3° render margin.
- **HUD:** drawn on a converged panel (55° wide, 1.6 m away) that stays put in the room and eases back in front of you when you turn away (F4 toggles follow, F9 cycles the size). **Press F3 once** to turn off the letterbox crop; with it on the world stretches when you look up or down (the next build makes this the default).
- **Theatre screen** (a world-locked OpenXR quad in a black void): pre-rendered videos, pausing menus (pause, map, gear), the start screen, loading screens and fades to black. F10 cycles the mode: auto, always, never. F11 recentres.
- **Known WIP issues:** menus and HUD are blurry (not yet upscaled); in-engine cutscene fades and cinematic bars are drawn on the HUD panel instead of across the whole view.
- **Headset side:** runs out of process (`GoTVRHost.exe`, OpenXR, D3D12 shared textures). Tested on Quest 3 via Virtual Desktop (VDXR); SteamVR is supported through OpenXR.

**Single-player only.** Never enter Legends (online) with the mod installed. The mod refuses to run on unknown game builds; it checks the exe hash and byte signatures.

## Install
1. Copy `mod/install/xinput1_4.dll` into the game folder.
2. Copy `mod/install/GoTVR/` (GoTVRHost.exe and openxr_loader.dll) into the game folder.
3. Set `xr=4` in `GoTVR\gotvr.ini`. This file is created on first run.
4. Start VDXR or SteamVR, then start the game from Steam.

See `mod/README.md` for every version's notes and test steps.

## Repository layout
- `mod/src`: the game-side DLL (`gotvr.cpp`), the OpenXR host (`host.cpp`) and the shared protocol.
- `mod/build.sh`: MinGW-w64 cross build. It needs MinHook and the OpenXR SDK headers.
- `docs/`: findings, address registry, bug log, stereo route.
- `tools/`: RE helper scripts and Ghidra scripts.
- `test/`: self-test and host-test harnesses.
- `probe/`: the early census probe.

This repository contains no game files, disassembly or decompiled game code. Licenses for MinHook and the OpenXR loader are in `mod/licenses`.
