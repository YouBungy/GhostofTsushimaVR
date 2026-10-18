# GoTVR probe 0.3 — EXP-00 load proof + passive camera / D3D12 read (read-only)

`xinput1_4.dll` is a pass-through XInput proxy (every xinput1_4 export, including the ordinal-only ones,
forwards to `C:\Windows\System32\xinput1_4.dll` by full path). It installs **no hooks and writes nothing
into the game**. On load it starts one background thread that writes `<game folder>\GoTVR\load.log`
(the previous run is kept as `load.prev.log`):

1. identity: probe version, PID, exe path, module base, PE timestamp, exe SHA-256 (MATCH/MISMATCH)
2. 17 byte-signature guards (v0.1's 8 plus 9 new anchors), PASS/FAIL. Camera and D3D reads are disabled
   unless hash + timestamp + all signatures match
3. once per second for 30 min: module loads (d3d12, dxgi, Streamline, FSR, XeSS); changes of the two camera
   globals CAM-A (0x1EDBCC0) and CAM-B (0x1EDBCC8), with class (0x11F / 0x11C), parent and parent class;
   every 5 s for each camera: FOV, near/far, aspect, cut state, pos550, world vs. local translation, basis
   rows, projection terms, off-center rect, projection rect (+0x694, carries TAA jitter)
4. once, 15 s after d3d12.dll is loaded: the D3D12 chain NxD3DImpl → NxD3D12Driver → device / direct queue,
   plus three const COM queries: `GetNodeCount`, `GetAdapterLuid`, `queue->GetDesc()`

SHA-256 of this build: 1bebbc2aa152c9e81e9885a0d3a2015f47f7198dafc9a5001485a4b05e7e440b
Built with clang 18 + lld-link, freestanding (`build.sh`).

## Use
1. **Delete `XInput9_1_0.dll`** from the game folder if it is there (it is never loaded). Copy `xinput1_4.dll`
   next to `GhostOfTsushima.exe`. The log folder `GoTVR\` should appear within a few seconds of launch,
   before the title menu (the game loads XInput during input initialisation at startup).
2. Launch from Steam. Single-player only; do not enter Legends with it installed.
3. Load a save outdoors. Then, slowly, with a few seconds between each: walk straight ~5 s, strafe right
   ~3 s, turn right ~90°, look up, look down. Then open photo mode for a few seconds and close it. Change
   the FOV option once (if available). Ride the horse briefly. Quit.
4. Send back `GoTVR\load.log`. Tell me roughly when you did each step (the log has a seconds counter).

Remove: delete `xinput1_4.dll` (and the `GoTVR` folder). Steam "Verify integrity" does not remove it, but
nothing else in the game folder is touched.

Why 0.3: probe 0.1/0.2 were named `XInput9_1_0.dll`, but the game's pad loader (0x497F30) tries
`xinput1_4.dll` first, which always exists in System32. The 9_1_0 name was never requested (EXP-00 run 1,
2026-10-17: no log). If `load.log` still never appears, note whether a `GoTVR` folder exists at all and
whether the game started normally.
