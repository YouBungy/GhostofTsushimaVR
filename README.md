# GhostofTsushimaVR
(WIP) GOT VR mod with 6DGOF, native stereo rendering, and motion controls.

# Important Notes
- TAA Only; DLSS caused more performance loss and jitter. Will investigate later
- Set to windowed for increased performance, it downsizes the window to a smaller resolution to save overhead.

# Current version: GoTVR 0.13.0
- **xr=4 synchronized stereo:** true per-eye stereo with 6DoF. Both eyes are rendered from the same simulation tick and presented as complete pairs. The monitor shows only the left eye.
- **Per-eye temporal state:** each eye has its own occlusion/culling history and TAA history and reprojection, with fast-head-turn handling and a 3° render margin.
- **HUD and menus:** the UI is drawn on a converged, correctly proportioned panel (55° wide, 1.6 m away). F9 cycles the size.
- **Pre-rendered videos, pausing menus (pause, map, gear) and world-less menus:** these are shown on a world-locked theatre screen (an OpenXR quad layer). F10 cycles the mode: auto, always, never. F11 recentres.
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
