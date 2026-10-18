# GhostofTsushimaVR
(WIP) GOT VR mod with 6DGOF, native stereo rendering, and motion controls.

# Important Notes
- TAA Only; DLSS caused more performance loss and jitter. Will investigate later
- **Set the game to Windowed display mode.** The headset image is then a square render per eye, independent of the desktop window.

# Current version: GoTVR 0.16.5
- **xr=4 synchronized stereo:** true per-eye stereo with 6DoF. Both eyes are rendered from the same simulation tick and presented as complete pairs. The monitor shows only the left eye.
- **Per-eye temporal state:** each eye has its own occlusion/culling history and TAA history and reprojection, with fast-head-turn handling and a 3° render margin.
- **Square headset render:** each eye is rendered as a square image, 2560x2560 by default. **F2** cycles 1536 / 1792 / 2048 / 2304 / 2560 / the game's own size while playing (applied through the game's own display path on its main thread). `render_size` in `gotvr.ini` sets the start size.
- **16:9 UI on the square render:** the game is told 16:9 for its UI, menus, flat frames and videos, and the headset shows those images at 16:9 (`ui_aspect` in `gotvr.ini`; 0 = square).
- **Sharp HUD layer:** the game's UI passes are redirected into their own image and shown as a separate quad at the HUD's full resolution. The panel stays put in the room and eases back when you look away (F4: fixed to view, F9: size, F1: old in-picture panel).
- **Cutscenes:** in-engine cutscenes (letterbox bars, the cutscene camera) show the HUD over the whole view until the game hands the camera back; after the pause menu the HUD is large for 1 s.
- **Theatre screen** (a world-locked quad in a black void): pre-rendered videos, pausing menus (pause, map, gear), the start screen and loading screens. F10 cycles the mode: auto, always, never. F11 recentres (also automatic when VR starts).
- **Startup:** the headset session starts at the game's first frame, so the PlayStation launcher can be used in Virtual Desktop / SteamVR.
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
