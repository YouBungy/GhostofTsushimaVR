# Port brief

Target game/build/store: Ghost of Tsushima DIRECTOR'S CUT, Steam, exe linked 2026-06-23
Executable + hash: GhostOfTsushima.exe, SHA-256 ED0931B6F6D3CA3874BAF39EBC16D95FF0E93296891182F96C3A790DA7109F63
Intended runtime/devices: OpenXR (D3D12 binding). Headset: **Meta Quest 3** (Touch Plus) over **Virtual Desktop / VDXR** OpenXR runtime (owner's primary); **SteamVR OpenXR** must also be supported (decided 2026-10-17)
Seated / standing / roomscale: **roomscale**, 6DoF (owner, 2026-10-17); seated option desirable
Target completeness tier: owner request = full VR with motion controls (T3–T4); first milestone T1 (6DoF head, flat-style controls)
Initially acceptable stereo rung: owner wants R1 native stereo; R3 (AFR) acceptable only as bring-up and fallback
Single-player / multiplayer / anti-cheat position: single-player story only. Legends (online) explicitly excluded; never test injected builds online. No anti-cheat found statically
Required mechanics/content (first milestone): walking, horse riding, camera, menus readable
Explicit exclusions (first milestone): Legends mode; DLSS-G/FSR frame generation; photo mode; pre-rendered webm movies (show on a flat screen panel)
Fundamentally hard for VR: third-person camera (**decision OPEN**: native first-person via head joint vs. third-person comfort follow camera; both supported by the camera seam, see VR_ARCHITECTURE.md), cinematic cutscene cameras, horse camera, combat lock-on
Clean vanilla launch recipe: OPEN (RE-03). Remove the proxy DLL; `-nolauncher` exists
Deterministic test scene/save: OPEN. Pick a static outdoor save with near/mid/far geometry
