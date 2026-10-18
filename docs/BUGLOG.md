# Bug log — GoTVR 0.4 run (2026-10-17, owner's PC)

Logs: `receipts/gotvr04_runA.log` (xr=0, hotkeys), `receipts/gotvr04_xr1_crash.log` (xr=1 crash),
`receipts/GhostOfTsushima.exe.27004.dmp` analysis below. Build hash MATCH, 8/8 signatures, 6/6 hooks `MH_OK`.

## BUG-1 — F7 "does nothing" → NOT A BUG (it is the control)

F7 arms the zero-delta control: the hook rewrites the camera with the **same** values. By design nothing
changes on screen; the census confirms the flag toggles (`mut armed=1`). This is the expected PASS result
— it proves the hook path has no side effect of its own. `[LIVE]`

## BUG-2 — F8 "does nothing" → seam not exercised at runtime

F8 routed through a detour of `Cam_SetProjRect` (0x8068E0). The census shows **`projrect(camA)=0` for the
entire session** (153/153 samples), i.e. the game never calls 0x8068E0 for the render camera. The probe
0.3 log agrees: `projrect694` stayed at the default (−1,−1,1,1) and `en=0`. So the TAA-jitter projection
sub-rect is applied by a different path (jitter is folded in elsewhere, or disabled at this setting), and
F8's chosen seam is dead. `[LIVE]`

Fix (0.5): inject the projection in the **working** seam instead. `Cam_vUpdateFrustum43` (slot 43,
0x807D10) is called ~5×/frame for the render camera (census `slot43(camA)` ≈ 5 × present) and **reads the
projection matrix at +0x200** before baking it into the view-projection at +0x430. So rewriting +0x200
just before the original slot-43 runs puts an asymmetric (per-eye) projection into the real pipeline. It is
rebuilt from the camera's own half-tangent scalars (+0x680 horizontal, +0x684 vertical; confirmed live:
P00=1/[+0x680], P11=1/[+0x684], aspect 1.6), so it never compounds across the ~5 calls per frame.

## BUG-3 — F9 rotates but smears → cumulative rotation (my bug) + motion blur

F9 proved the seam: rewriting the render-camera world matrix (+0x140) in the slot-43 hook turns the view
while the body, aim and HUD are unaffected. `[LIVE]`
But 0.4 applied +5° **in place on every slot-43 call**. Slot 43 runs ~5–6×/frame for the render camera and
the game often does not rewrite the camera for many frames (`setlocal(camA)=0` while `slot43(camA)≈600/s`),
so the rotation accumulated: the logged forward vector was ≈35° off (7×5°), not 5°, and kept moving.
Motion blur turned that continuous spin into the distant smear (owner: disabling Motion Blur removes it).
Fix (0.5): the hook keeps a copy of the game's world matrix and recomposes from it every call
(idempotent): the game pose is re-captured whenever the matrix differs from what GoTVR last wrote.
Motion blur must still be off in VR (it is computed for one flat view).

## BUG-4 — xr=1 crash → stack overflow in the NVIDIA pipeline compiler on the game's Present thread

Symptom: with `xr=1`, the game reached the headset (log: `XR runtime: VirtualDesktopXR 1.1.0`,
`adapter LUID runtime==game 00000000:00013ad2`, `XR swapchain 2560x1440 format 28, 3 images`,
`XR frame 1 submitted`, session states → 5/FOCUSED), then crashed about one second in; `present` fell to 0.

Crash dump (`GhostOfTsushima.exe.27004.dmp`, Windows WER): `EXCEPTION_STACK_OVERFLOW` (0xC00000FD) with
the faulting address inside **`nvgpucomp64.dll`** (NVIDIA's pipeline/shader compiler). The thread that
holds the exception record (tid 32404) is the game's own crash-handler thread (fn 0x43EF70, "Unhandled
exception at address…"); the overflowed stack itself is not preserved in the dump. A 1 MB region full of
one repeated `opengl32.dll` return address belongs to **no live thread** (no TEB stack range contains it):
stale memory, **not** evidence about this crash. `[LIVE]` for the facts, root cause below is `[INFERENCE]`.

Most likely cause (`[INFERENCE]`, consistent with all evidence: only with xr on, ~1 s after the session
started submitting, in the driver's pipeline compiler):
1. **All OpenXR work ran on the game's Present/submit thread.** That thread is created by the game's own
   thread wrapper with a stack size from a parameter block (`_beginthreadex` at 0xEEDA91), not the 1 MB
   default. VDXR is an in-process runtime: during the first frames it builds D3D12 pipelines on the game's
   device *inside* `xrCreateSwapchain`/`xrEndFrame`, which runs NVIDIA's compiler on whatever thread made
   the call. A deep compiler call on a small game stack overflows.
2. **Queue hazard (independent):** 0.4 submitted copies and fences on the nx-layer queue0, while the
   Sucker Punch renderer creates its own queues (`m_pQueue[QUEUEK_*]`, 0x509920) — so the copy of the back
   buffer was not synchronised with the queue that actually renders and presents.

Not established: the exact faulting thread and call chain (needs a dump that preserves it or a debugger).

Fix (0.5):
- **Dedicated XR thread with a 16 MB stack** owns every OpenXR call (instance, session, swapchains,
  wait/begin/end frame). The game thread never calls OpenXR. This also stops `xrWaitFrame` from throttling
  the game's own frame rate, and keeps the headset alive during loads (playbook PORT-08).
- **Dedicated D3D12 command queue** for XR; the OpenXR session binds to it.
- **Virtual screen:** the Present hook only copies the back buffer into a GoTVR-owned texture on the
  GoTVR queue, synchronised with the **swapchain's own queue** (`IDXGISwapChain::GetDevice(IID_ID3D12CommandQueue)`)
  by two fences (game queue signals → GoTVR copies → game queue waits before presenting). No barriers on
  game resources (implicit COMMON promotion/decay). If that queue cannot be obtained, the screen falls back
  to a test pattern and logs why.

Open follow-ups: confirm whether DLSS-G/frame-gen must be forced off for VR (very likely yes — it
interpolates a single flat view); the present-descriptor HUD-less/UI/depth tags were null in this run
because frame-gen tagging only runs when `[obj+0x13C]==2` (EXP-06 still open).

---

# Bug log — GoTVR 0.5 runs (2026-10-17 evening, owner's PC)

Logs: `receipts/gotvr05_xr1.log`, `receipts/gotvr05_xr3.log`; dumps `GhostOfTsushima.exe.33212.dmp` (full, xr=1)
and `GhostOfTsushima.exe(1).45916.dmp` (faulting thread only, xr=3). Analysis: `receipts/gotvr05_crash_analysis.md`.

| Test | Result |
|---|---|
| F8 (projection at +0x200) | **PASS** — left half stretches correctly → the per-eye projection seam is live `[LIVE]` |
| F9 (idempotent yaw) | **PASS** `[LIVE]` |
| xr=1 | grey screen + white square in the headset (link OK, game-image queue unavailable), then crash |
| xr=3 | same, the screen followed the head (head pose path OK), then crash |

## BUG-5 — `GetDevice(ID3D12CommandQueue)` fails on the game swapchain
The object at `NxDXGI+0x30` is not the native DXGI swapchain (the game creates DXGI/D3D12 objects through
`sl.interposer.dll`, which exports `slGetNativeInterface`). 0.6 unwraps the swapchain, queue and device with
`slGetNativeInterface`, logs which module each vtable lives in, and falls back to the nx queue0 only with a
loud "present queue NOT verified" log line (`queue_fallback=1`).

## BUG-6 — both crashes are inside Virtual Desktop's in-process runtime, not in GoTVR code `[LIVE]` (dumps)
- **xr=1 (full dump):** `EXCEPTION_STACK_OVERFLOW` in `nvgpucomp64.dll` on thread 13696, an **NVIDIA driver
  shader-compile worker whose whole stack reservation is 64 KB** (TEB DeallocationStack→StackBase =
  0x10000; 51 such driver workers in the process; the exe default is 1 MB). GoTVR's XR thread (16 MB stack)
  was idle inside `virtualdesktop-openxr.dll` (frame wait). So 0.5's big-stack thread was not enough: the
  overflow is on the driver's own worker, compiling something the in-process runtime triggered.
- **xr=3 (thread dump):** `0xC0000409` / `FAST_FAIL_GUARD_ICALL_CHECK_FAILURE` (CFG). The original access
  violation's CONTEXT on the stack shows `RAX = 0xDEDEDEDEDEDEDEDE` (a vtable read from freed memory) called
  from `d3d11.dll+0x22249` on Virtual Desktop's compositor thread (`virtualdesktop-openxr.dll` →
  `VirtualDesktop.LibOVRRT64_1.dll` → `d3d11.dll` → `nvwgf2umx.dll`). Use-after-free inside the runtime.
- Playbook match: ch07 records BioshockVR's identical `0xC0000409` / freed `0xDEDEDEDE` fail-fast under a
  streaming runtime, correlated with **zero-layer `xrEndFrame` submissions**. GoTVR 0.4/0.5 submitted zero
  layers whenever `shouldRender` was false or an acquire failed (session state transitions).
- Also resident in the game process: Steam overlay (`gameoverlayrenderer64.dll`), Streamline + DLSS/DLSS-G,
  `openxr-oculus-compatibility.dll`.

Fix (0.6), playbook ch09 "out-of-process helper" + ch07 rules:
1. **OpenXR moves to `GoTVRHost.exe`**, a separate process with its own D3D12 device on the runtime's
   adapter. The game process never loads the runtime. The game shares 6 eye textures + 1 fence (NT handles);
   the host copies them into swapchain images. Neither side ever GPU-waits on the other (CPU fence checks only),
   so a dead host can never hang the game and vice versa.
2. **Exactly one projection layer on every `xrEndFrame`**, from the first frame (black placeholder images
   until the game publishes, black when the game is stale).
3. **API-layer census** at host start (loader enumeration + implicit-layer registry keys), logged.

## GoTVR 0.6 first launch (2026-10-17 18:45)
- BUG-7: `host: cannot start ...\GoTVR\GoTVRHost.exe (err 2)` → xr disabled for the run, so changing `xr=` had no
  visible effect. The helper was not in `GoTVR\` at launch time. Fix 0.6.1: also look next to the game exe, log both
  paths; running the helper by hand now shows a message explaining it is started by the mod.
- BUG-8: false frame-generation refusal: 1.99 native Presents per game Present (478/240). DXGI (or an overlay)
  chains Present→Present1, and both are hooked. Fix 0.6.1: thread-local nesting guard, only the outermost call
  counts; nested calls logged separately.
- Observed `[LIVE]`: native swapchain `GetDevice(ID3D12CommandQueue)` → E_NOINTERFACE (D3D12 swapchains do not
  expose their queue); fallback = nx queue0 (D3D12Core vtable, same device as the swapchain's). Streamline returns
  the same object (swapchain vtable already in dxgi.dll): no proxy on this build. FrameView runs after the engine's
  own slot-43 calls in every frame (before/after = N/1).

## GoTVR 0.6.1 runs (2026-10-17 19:25) — xr=9 PASS, xr=1/2 black, no crashes
- **xr=9 PASS** `[LIVE]`: out-of-process host stable on VDXR (Quest 3, RTX 5090 Laptop, LUID match), test pattern
  correct per eye, no crash → BUG-6/NEG-013 fix confirmed. API layer census: one implicit layer,
  `XR_APILAYER_VIRTUALDESKTOP_oculus_compatibility` (Virtual Desktop's own).
- **BUG-9 (black in xr=1/2):** the game stopped publishing after 4 s with the false "frame generation" refusal
  (still 1.99 presents per frame, 0 nested). Cause: the engine Present wrapper calls `Present(sync,
  DXGI_PRESENT_TEST)` first (occlusion check, decompile 0x434CC0 line 184, result compared with
  0x087A0001 = DXGI_STATUS_OCCLUDED), then the real Present (line 430). GoTVR counted the TEST call as a frame
  and also consumed the pending copy there (too early). Frame generation was really off (Present-descriptor tags
  null). Fix 0.6.2: calls with `DXGI_PRESENT_TEST` are passed through untouched and counted separately.

## GoTVR 0.6.2 runs (19:40) — xr=1/2 working; xr=2 flicker + ghosting (BUG-10, analysed)
Cause `[STATIC]` + `[INFERENCE]`: the engine has one global TAA/DLSS history (StreamlinePass_cb reads global
target slots; STEREO_ROUTE session 6). In AFR every frame's temporal accumulation blends with the previous frame =
the other eye, 6.3 cm away → ghost doubles and shimmer at edges. Not a frame-tagging bug (mismatches ≈0).
0.6.3 test: **F6** (or `history_reset=1`) sets the engine's camera-cut flag every VR frame so the history is
discarded: if ghosting disappears (aliasing increases), the cause is confirmed. The real fix is R1 with per-eye
history (STEREO_ROUTE R1-A…D), not an AFR patch.

## GoTVR 0.6.3 run (19:52) — F6 removes the AFR flicker (R1-0 PASS); no new bugs in the log
Frame tagging clean (mismatch ≈0), host stable, presents 1.00/frame.

## GoTVR 0.7.0 F5 (R1-A) — immediate crash in gameplay (BUG-11) + host crash at every game exit (BUG-12)
**BUG-11** `[LIVE]` (dump GhostOfTsushima.exe.21768): AV write at GhostOfTsushima+0x77ED49 inside the second
`RV_Post` (stack: GoTVR hRVPost → RV_Post 0x9B535E → pass 13 callback 0x60977C → 0x60E685 → 0x77E810).
`mov [rsi+8*rbx+0x1B8], rax` with rsi = 0x1434A26F0 (frame view-constants object): 4 block pointers at
+0x1B8..+0x1D7, counter at +0x1D8; 0x77E810 fills all 4 per call (loop r15<4) and the engine resets the counter
only at frame end (0x77F340, which also releases the frame constant ring). The second run wrote slot #5 over the
counter (counter = low dword of a pointer, 0x52CFD3C0) and the next write went wild. The log agrees: second run
entered (contexts after main = 32), never returned; V2+0x18 stayed 0 (main 1) before its kick.
Playbook ch17 arena-slot class ("per-frame state globally slotted"). Fix 0.7.1: reset the counter (+0x1D8 only,
not the ring) between the runs; a vectored handler logs any further fault during the second run (RVA + game
stack) so each remaining single-use structure is named by the log. Note for R1-B: 0x77E810 builds the global
view constants from CAM-A (0x141EDBCC0), not from the job's view → the second run must borrow CAM-A (pose swap
and restore around the second RV_Post).
**BUG-12** `[LIVE]` (4 host dumps, one per game exit): CFG fail-fast in Virtual Desktop's D3D11 compositor inside
GoTVRHost at exit (same signature as BUG-6): the host returned from main with the session alive. Fix 0.7.1:
xrRequestExitSession → wait STOPPING (frame loop kept alive ≤2 s) → xrEndSession → GPU idle → destroy swapchains,
spaces, session, instance → TerminateProcess.

## GoTVR 0.7.1 F5 — game froze (BUG-13); "GoTVRHost crash" checked: not GoTVRHost
- Log `[LIVE]`: the second graph run **completed once** (runs=1; pass contexts 32 after the main run → 122 after
  the second; no R1 FAULT), then the frame loop stopped (tick=0, present=0 every second) = CPU deadlock, not a GPU
  hang (that would TDR). `view+0x18`: main 1, V2 0.
- Cause `[STATIC]`: the per-view activation `RV_9E5DE0(frame+0x1EAB0, view)` (sets view+0x18 = 1, visibility
  parameters +0x790..+0x820, per-worker visibility jobs FUN_140B004D0 counted into the global wait groups
  0x141EDD9B8/0x14148A570/0x1434A24B8/0x1434A2730, renderer registration 0x787D50) runs only for the main view at
  the end of FrameView_5A07F0. V2 was never activated → RV_Kick skipped it (no job list queued) → its recorded graph
  waits on work that never comes → frame end blocks forever. Fix 0.7.2: call RV_9E5DE0 for V2 after FrameView
  (same order as main: gather, then activate). `RV_9E5A00` stays main-only (it resets shared occlusion state).
- Hang diagnostics: a stall watchdog now suspends the game and submit threads after 3 s without a tick (F5 on)
  and logs their RIP and game return addresses (`R1 STALL` lines).
- Owner suspected GoTVRHost crashed: **not the case** — xr=0 does not start the host (host.log unchanged since
  19:51, no new GoTVRHost dump). The only new dump is `PsPcSdkUI.exe` (Sony's PlayStation PC SDK overlay), written
  at the moment the frozen game was killed (upload of that dump failed, so its stack is unverified).

## GoTVR 0.7.2 F5 — game froze again, music kept playing (BUG-14)
- Log `[LIVE]`: `R1 STALL` — game thread parked in ntdll, game return addresses `0x4DBD09` (inside
  `Sched_EndFrameWait_4DBCA0`) ← `0x4C5200` (GameTick); submit thread idle at `0x46E3C9`/`0x42470E`. Second run
  entered and returned once (runs=1, activations=1), V2 activated (`view+0x18` main=1 v2=1), pass contexts 33 after
  the main run → 67 after the second (the second run allocates the same graph again, as expected).
- Cause `[STATIC]`: the pass-context pool (scheduler 0x141A0AB50, contexts at +0x9290, stride 0x2B20, allocated
  count +0x2B5C60, completed count +0x2B5C64) is submitted by `Jobs_SubmitGraph_4D7EB0`, whose only caller is
  RV_Post. It walks the **whole pool from index 0**: a context with work (+0x2A6C) that is already done (+0x2A6D) is
  retired (`0x4DA510`: completed += 1, wake waiters when completed == allocated), otherwise queued as a job
  (`0x4DADF0`). Called a second time in the same frame it retires/queues the first run's 33 contexts again, so the
  completed count passes the allocated count. `Sched_EndFrameWait` waits for **completed == allocated** (`!=` loop
  on a condition variable), which then never becomes true → deadlock. Audio runs on its own threads, hence the
  music. Playbook ch17 again: a per-frame engine structure that assumes one graph per frame.
- Fix 0.7.3: `Jobs_SubmitGraph` hooked (signature-guarded). During the second run only, a re-implementation with
  the same steps, the same engine functions and the same job-group layout (frame allocator 0x141EBAE48 via
  0x576E80, header 0x28 bytes, entries 0x18 = {0, 0, job fn, context}; queue 0x4DAC80, then wait-list push
  0x5764B0) processes only the contexts allocated after the first run (index ≥ the count saved after run 1). Every
  other call goes to the original. The per-second line now also logs the second-run submit counts and the
  scheduler's completed/allocated pair, so a remaining mismatch shows up as a number, not a freeze.

## GoTVR 0.7.3 F5 — no freeze; 122 → ~90 fps; picture darker and bluer at night (BUG-15)
- Log `[LIVE]` (receipts/gotvr073_live.log): ~100 second runs per second for minutes, two F5 on/off cycles, no
  stall, scheduler completed/allocated balanced; per run 33 contexts, second-run submit ≈ 12.7 queued + 1 retired.
  BUG-14 fix confirmed.
- Cause of the colour shift `[STATIC]`: the view-constants object 0x1434A26F0 (BUG-11) also carries the frame's
  **tiled light lists**. Pass 13 (`0x77E810`) allocates a constant block (+0x1B0) and a 4-entry GPU argument buffer
  (buffer object at +0x1E0/+0x200 by frame parity; slots +0x1B8.. point into its staging memory), with default
  "no list" descriptors. Only caller of `0x77F340` is GameTick 0x4C5213, right after `Sched_EndFrameWait`: it bins the
  lights into 32-px screen tiles, patches the light-list descriptors (block +0x00, +0x10..+0x40, +0x50..+0x80) into
  the **latest** block, the per-tile counts into the latest slots, and uploads the latest buffer (`0x7E2D70`). With
  two runs, run 2's pass 13 released run 1's buffer (`0x7E1F30`) and replaced the block; run 1 rendered with no local
  lights (torches/lanterns gone → darker, bluer at night) and the shared TAA/DLSS history mixed that into the
  output. Playbook ch14 class "computed per view, finalised once".
- Fix 0.7.4: between the runs, run 1's block/buffer/slots are banked and run 2 gets its own buffer object (same
  2-frame parity lifetime). At frame end, after the engine finalises run 2, run 2's light lists are lent to run 1
  (descriptors + tile counts copied, run 1's buffer uploaded with the engine's own function). One binning, two
  consumers (ch14 "one view is the authority, the other reuses"). **F4** toggles the lend for an A/B check.
  R1-B note: the binning uses CAM-A's screen tiles; the other eye reuses them (edge tiles may miss a light by a
  few pixels: watch for light popping at tile edges in the right eye).
- fps: a second full scene render normally costs close to ×2 GPU time. 122 → ~90–100 means the GPU was not the
  limit at 122 fps (CPU-bound) or part of the second run is not rendered. 0.7.4 logs pass 59 (upscaler) calls per
  run to tell which.
- R1-B census added: pass 59's pre-callback `StreamlinePass_cb` (0x60F9E0) records the upscaler evaluate inline on
  the game thread into the pass context's native command list (`**ctx`, via 0x501A00) and writes the global output
  slot 0x141EBA130 (resource via 0x501A10 → +8). 0.7.4 logs that list and the output's size/format per run on F5
  and F10: if the output is display-referred and full size, each eye is copied there, in GPU order, inside the
  engine's own command list.

## GoTVR 0.7.4 F5 — colour shift unchanged, F4 no visible effect (BUG-15 open; light-list hypothesis falsified)
- Log `[LIVE]` (receipts/gotvr074_live.log): lend active ~103/s, F4 toggled many times, no visible change. So the
  missing light lists in run 1 are **not** what darkens the image (NEG-018). The lend stays on (it is correct by the
  static reading and harmless) but is unproven.
- Census `[LIVE]`: owner uses **TAA** (no upscaler). The upscaler output slot 0x141EBA130 and input 0x141EBA148 are
  1×1 placeholders (fmt 28 = R8G8B8A8_UNORM): with TAA the evaluate struct is not built, and pass 59 takes the TAA
  branch (`FUN_140638280(ctx, …, 0x141A0B8D0)`). Both runs record pass 59 into **different native command lists**
  (D3D12Core), as expected for separate pass contexts.
- Pass jobs `[STATIC]`: `Pass_Job_9B55B0` (25-byte thunk to 0x9B55D0) reads the pass's draw range from the view
  (`view+0x13C4 + pass*8` = begin/end), calls the pass's p1 setup (table +8) only if the range is non-empty, then
  splits the draws over worker jobs. The job wrapper 0x4DADF0 counts completion (0x141CC07B4) whatever the job does.
  Pass 0x3FCBD0 = `xor eax,eax; ret` is the engine's own no-op callback (used by many passes).
- 0.7.5 (playbook ch06 bisection, pre-registered): **F3** makes run 2 skip passes [N, 69), N = 14, 30, 45, 59, 65
  (inline callbacks swapped for 0x3FCBD0 during run 2 only, V2's pass jobs return early). For N ≤ 59 the visible
  image is run 1's (run 2 executes later on the GPU and no longer writes the output). Readings:
  normal at N=14 → the cause is in run 2's passes ≥ 14; dark already at N=14 → run 2's passes 0..13, V2's
  activation/visibility, or the second RV_Post itself; then the first dark N brackets the pass. **F10** logs a
  per-pass census (draws/jobs, main vs V2) of the last doubled frame.

## GoTVR 0.7.5 F3 — scene went black (UI still drawn), menus all black (BUG-16, caused by GoTVR)
- Log `[LIVE]` (receipts/gotvr075_live_f3.log): from the first F3 press, **run 1** also lost its later passes
  (pass-59 calls run 1 = 0, run-1 pass contexts 33 → 13), and it stayed that way after F5 off and through menus.
- Cause: race between the hotkey thread and the game thread. `maskSecondRun` read `skipFrom` separately for the
  patch (before run 2) and for the restore (after run 2). An F3 press in between (likely: run 2 takes several ms
  of each ~10 ms frame) patched nothing and then "restored" passes 14..68 from a never-filled buffer = zeros, into
  the engine's pass table, permanently, for every render including menus (passes 59..68). The game process
  keeps the broken table until restarted.
- Fix 0.7.6: pristine table copy captured at install (verified: pass 0 pre 0x60EDC0, pass 0 main = no-op, pass 13
  main 0x609760, base = image base); one skip value per frame used for patch, restore and that frame's pass jobs;
  restore always from the pristine copy; self-check before every main render repairs and logs any difference.
  Regression test in the self-test (mask, restore, the exact 0.7.5 race order, self-repair): PASS.
- Census `[LIVE]` (receipts/gotvr075_live.log, before any F3): per-pass draws main vs V2 identical except pass 17
  (main 30 draws via job, V2 no job) and noise on 33/40. Explained `[STATIC]`: the "debug" flag 0x141803B60 is **1
  in the shipping exe**, so RV_Post runs any pass whose bit is set in the view's bitset (view+0x15F0, built by
  RV_Submit) inline on the game thread via `0x9B55D0` and marks it done (= the 1 retired context per run). The
  0.7.5 census hooked only the job thunk 0x9B55B0. 0.7.6 hooks the body 0x9B55D0 and labels the route
  (return address 0x9B5431 = inline).

## GoTVR 0.7.6 — F3 safe; darkness gone with run 2 stopped from pass 14 (BUG-17 found)
- Owner `[LIVE]`: "the darkness is fixed after pressing F3 once" (skip 14..68 → visible image = run 1's). Table
  self-check silent (no repairs), no stall. So run 1 is correct and **run 2's output** is the darker one.
- Census `[LIVE]` (receipts/gotvr076_live.log, doubled frame, no skipping): per-pass draws main | V2 equal except
  pass 0 (1|0), **17 (30|0)**, **18 (8|0)**, 29 (1|0), 51 (1|0), 52 (1|0), 57 (1|0), 61 (6|0). Both views run every
  pass; V2 simply has no draws there.
- Cause `[STATIC]`: draws live per view: sorted keys view+0x13B8 (low 16 bits = renderer slot in view+0x13B0,
  stride 0x80), pass ranges view+0x13C4, executed by Pass_Draw_9B5B40. `Frame_59FA60` walks the frame's renderable
  lists (frame+0x1EA18 / +0x1EA30) calling `obj->Render(ctx)` with `RCtx_Build_9B2DD0(ctx, mainView)` (calls at
  0x59FB3F, 0x59FD43); `RCtx_FlushDraws_9B49E0` copies the context's 0x80-byte records (+0x98, pass ids +0xA0, count
  +0xA8) into the frame's global pools bucketed per pass and links the batch into ctx+0x20 = view+0x840 (lock
  ctx+0x18 = view+0x850). Those renderables (pass 17/18 etc.: likely light volumes / additive effects) reach the
  main view only. Playbook ch17 "render the world twice" was violated for this producer.
- Fix 0.7.7: hook RCtx_Build (tags each context with its builder, per thread) and RCtx_FlushDraws: a flush of a
  main-view context from Frame_59FA60's builders is first flushed again from a copy aimed at V2's batch list
  (records by value; lights +0x1C0 registered once in RCtx_Finish; FlushB's global items once). The per-worker
  visibility builder (0xB01776) already builds V2's own contexts and is not duplicated. **F4** = A/B of the
  duplication. F10 logs a per-builder census (records into main / V2 / duplicated) to confirm no other builder
  feeds the main view only.

## GoTVR 0.7.7 — still darker/bluer with F5; fps 122 → 90 (BUG-17 fixed but not the cause; BUG-18 open)
- Log `[LIVE]` (receipts/gotvr077_live.log): duplication active (Frame_59FA60 builder 0x59FD48: 49 records/frame
  into V2); with it V2's per-pass draws equal main's on **every** pass (17: 30|30, 18: 8|8, 61: 6|6). Image unchanged
  (owner) → the draw-list gap was real but is not the cause (NEG-019). Builder census: 0x7840BC (240) and the
  visibility jobs 0xB0177B (~1070) feed both views equally; 0x59FB44/0x62EB00/0x651C34/0x65265C idle here.
- What is established: run 1's image is correct (F3 skip 14..68); run 2 draws the same content; so the difference
  is **state that a second execution of the graph changes**. Lead `[STATIC]`: the engine has auto exposure with a
  histogram + adaptation (`ExposureHistogramMin/Max`, `ExposureAdaptationSpeed`, `ExposureDamping`) and a
  **Purkinje** low-light shift (`PurkinjeExposure`, `PurkinjeExposureMax`): a lower exposure in run 2 gives
  exactly "darker and bluer at night". Playbook ch14 (Elite Dangerous): any adaptive/feedback process must be driven
  by one shared value across eyes.
- 0.7.8: instead of more manual A/B rounds, a measured, automatic bisection (playbook ch06 + A5 flat harness):
  a brightness meter at the native Present (back buffer → READBACK, 1/64 pixel grid, R/G/B/luma), and **F2** runs
  A (F5 off) / B (F5 on) controls twice per phase, then skips one block of run-2 passes at a time (rest of run 2
  still runs; skipped passes leave run 1's results in the global targets), then the single passes of any block
  that restores the brightness (score (L−B)/(A−B) > 0.6), applies the culprit set as the run-2 skip (run 1 = the
  authority) and verifies it (A, B, fixed, twice). Gate: |A−B| ≥ max(1, 3×noise). fps is logged per state.
  Offline simulation of the whole state machine in the self-test: PASS (finds the planted pass 50, verify 1.00).
- fps: per the census both renders execute every pass with identical draw counts, so the second render is real.
  122 → 90 fps = 8.2 → 11.1 ms: the second render adds ~2.9 ms, i.e. the frame was not GPU-limited at 122 fps (the
  GPU had headroom; a second render costs GPU time, which is only "half the fps" when the GPU is the limit). F2
  measures the fps of each state, which shows where run 2's cost is.

## GoTVR 0.7.8 F2 — first measurements; native stereo built (0.8.0)
- Meter `[LIVE]` (receipts/gotvr078_live_f2.log), 2560x1600 R8G8B8A8: A (F5 off) luma 21.1 rgb (15.5 22.2 26.5),
  B (F5 on) luma 25.4 rgb (16.5 27.0 36.9). **The second render is BRIGHTER and bluer**, by ≈ (+1, +5, +10): an
  additive blue-cyan contribution applied twice (fog in-scatter / sky light / ambient accumulating on a target the
  second render does not start fresh on), not an exposure drop. Skipping run-2 blocks 14–16, 17–19, 20–25 did not
  remove it (25.6 / 27.5 / 26.8). States 6–7 measured the same static near-white image (menu/map opened) and the
  run was then cancelled by the safety check (F5 toggled by hand) → no result for blocks ≥ 26 yet.
- 0.8.0 adds **xr=3 native stereo** (STEREO_ROUTE R1-B): left = first render, right = second render, same game
  frame. Second render: drain the first render's pass jobs (completed ≥ contexts after run 1, ≤ 40 ms), compose
  CAM-A for the right eye via the slot-43 hook (pose2/fov2), refresh V2's camera snapshot (`RV_Snapshot_9B6AE0`),
  arm the left capture. Left capture inside the second render's pass-0 context (table entry swapped for that run
  only): engine transition of the back buffer (via the swapchain wrapper `0x4093C0()->vt+0x118`, record
  `0x501A10`) to COPY_SOURCE (`0x4FDD50`, per-list tracking with first-use fix-up at submit), engine barrier flush
  (`0x4FE780`), CopyTextureRegion into the shared left-eye texture. Right = back buffer at the native Present.
  Both published for the same frame with their own poses/frusta; frames without a second render (menus) are shown
  to both eyes. CAM-A returns to the left eye after the end-of-frame wait. TAA history reset every frame by default
  in xr=3 (one shared history until R1-C).
- **F1** (flat proof, xr=0 + F5): the second render's camera 0.5 m to the right. Pass = the whole picture shifts
  consistently (near things more than far ones). Fail = some objects stay put → camera-relative data baked during
  gathering (then those producers need a per-eye pass).

## GoTVR 0.8.0 — F1: nothing moved, Jin transparent; xr=3: left eye black, right eye doubled (BUG-20, BUG-21)
- **BUG-20** `[LIVE]`+`[STATIC]`: the engine renders **camera-relative**. `Cam_vUpdateFrustum43` builds a
  rotation-only view-projection from the world rows (+0x140/+0x150/+0x160) and passes the position (+0x170) to
  `Cam_SetViewProjFrustum_806BA0(cam, pos, VPrel)`, which stores pos (+0x550), VPrel (+0x430), the absolute VP
  T(−pos)·VPrel (+0x240..+0x27C), and the frustum planes. Everything the frame gathered is relative to the
  position the camera had then. 0.8.0 moved the second render's camera position → the gathered geometry kept its
  relative positions (nothing moved), while consumers of the position (Jin's near-camera fade) saw the new one
  (Jin transparent). In the headset the right eye mixed both (double image, parts of models moving).
  Fix 0.8.1: right eye = left-eye POSITION + right-eye rotation and frustum, eye offset D = C_R − C_L folded into
  the relative VP as a translation row (VPrel2 = T(−D)·VP_R), set through the engine's own SetViewProjFrustum ⇒
  absolute VP = T(−C_R)·VP_R exactly; inverse/planes derived by the engine. Residual: camera-position shading terms
  use a point 3 cm (one IPD) away.
- **BUG-21** `[LIVE]`: left-eye capture failed every frame (`left-capture fails` = fps): the swapchain wrapper's
  vt+0x118 returns the **display-settings block** (HDR mode +0x13C …), not a render target. Fix 0.8.1: the engine's
  per-buffer resource object is found by the native back-buffer pointer (resource table [0x141487D38], 0x18-byte
  entries → per-buffer objects, object+8 = ID3D12Resource), cached; transition through the tracking core
  `0x4FDED0(ctx, object, COPY_SOURCE)` + flush `0x4FE780`. Back buffer of frame F predicted from the last presented
  frame and **verified at Present** (mismatch → that left image dropped, counted).
- Blue tint (owner): "F3 once fixed it" = run 2 skipping passes 14..68, i.e. the second render no longer
  contributing anything (the picture is then the first render's; as a stereo fix the right eye would have no image
  of its own). It does prove the extra blue comes from the second render's passes ≥ 14; 0.7.8 F2 excluded 14..25.
  0.8.1 F2 tests only 26..58 (59.. would merely show the first render's finished image = a false fix), with a
  2.5 s recovery after each skip (exposure-type feedback) and an early stop if a skip breaks the picture
  (0.7.8 states 6–7 measured a static near-white screen).

## GoTVR 0.8.1 — F1: double image (one moved, one not), black flicker, parts not moving; F2 not completed
- F1 double image = **TAA**, not geometry: in xr=0 the history reset is off, so the shifted second render was
  blended with the TAA history = the unshifted first render of the same frame (ghost of the other camera; black
  flicker = TAA rejection; "parts not moved" = neighbourhood clamping). Fix 0.8.2: F1 forces the camera-cut flag
  (history reset) in both renders while it is on. (Clears checked `[STATIC]`: render-target/depth clears are explicit
  calls inside pass callbacks — e.g. pass 6 pre clears six targets — executed by both renders; not a frame-once
  clear. ClearRT wrapper 0x4FA890 via 0x4DD5C0/0x4DD530; ClearDS 0x4FAA10 via 0x4DD6B0, used by UI pass 60.)
- F2 `[LIVE]` (receipts/gotvr081_live.log): started and cancelled three times within the first two states (F1/F5
  pressed meanwhile); A=18.9..22.4, B=26.0 rgb (16.2 27.8 37.6). A time-based A/B test needs minutes of stillness
  → wrong tool for this owner/scene.
- 0.8.2 **same-frame meter**: L = the back buffer at the start of the second render (= the first render's finished
  image, copied into a READBACK buffer inside the engine's pass-0 list), R = the same frame's back buffer at Present
  (back-buffer index verified). With the same camera the two must be identical: their difference IS the artifact,
  immune to motion. **F2 sweep**: skip one second-render pass at a time (14..68, 0.65 s each, baselines every 11
  passes); the EARLIEST pass whose skip removes the difference (share < 25 %) is the cause (later passes that
  rebuild the image also remove it — excluded by "earliest", and ≥ 59 is never applied). Applied as "second render
  takes that pass from the first" (playbook ch14 authority), verified (≥ 3 pairs, < 35 % left) and saved as
  `right_eye_reuse=` in gotvr.ini (loaded at start). Self-test simulation: finds the planted pass 33 (not the false
  fixes ≥ 59), verify 0 %, saved. Shared bbmap (back buffer → engine object, predict + verify) used by both the
  stereo capture and the meter.

## GoTVR 0.8.2 — F1 still a double image (TAA reset on), F2 no effect → route change: xr=4 synchronized-sequential (0.9.0)
- Log `[LIVE]` (receipts/gotvr082_live.log): `bbmap: … engine resource objects found for 0` — the native back buffers
  are not in the resource table under their native pointers, so the 0.8.1/0.8.2 left-eye capture and the same-frame
  meter never produced a single pair (F2 had nothing to measure); the failed scan also repeated every frame
  (CPU cost; throttled in 0.9.0). xr=3's left eye therefore still cannot work as built.
- F1 with the TAA history reset still shows both renders' geometry at once (moved + unmoved, black flicker, partial
  objects) → the second render draws into targets that still hold the first render's frame (depth/G-buffer/other
  per-frame state the second execution does not reset), consistent with the blue tint being additive work piling up.
  Each such state can be chased (BUG-11, -14, -15, -16, -17, -20, -21 so far), but the playbook's Cyberpunk teardown
  documents exactly this wall ("ONE main view per frame at every accessible level … per-frame state non-idempotent
  and globally arena-slotted"; replay/double route closed as "decisive, do not retry") and gives the route that
  meets the actual requirement — both eyes from ONE simulation tick — without fighting it: **Synchronized
  Sequential** (ch17, "a different thing from AFR").
- 0.9.0 **xr=4**: tick A = left eye, normal simulation; tick B = right eye, simulation and time frozen:
  `Clock_Update_56ED80` (only caller GameTick 0x4C4B9A) skipped, so its QPC timestamps stay and tick A+2's step
  covers both ticks (normal game speed, one simulation step per pair); game steps clock+0x08 (= the GameTick gate
  0x141489DF8), +0x18, +0x38, +0x48 = 0, real step 1e-4 s (GameTick's paused branch, as in photo mode; no division
  by zero); saved/restored around tick B so tick A's early code sees normal steps. Tick B's camera composes from
  tick A's game camera (cache lock); both eyes use ONE head-pose sample; the XInput proxy returns tick A's pad state
  during tick B (no lost presses). Every tick is a full, normal engine frame → none of the second-render hazards.
  TAA history reset per frame by default (shared history alternates eyes) until per-eye history (R1-C), which is now
  far simpler: each eye is a complete frame of its own.

## GoTVR 0.9.0 → 0.9.1 — xr=4 works (depth, fps, controls) but objects flicker + random black streaks (BUG-22)
- User `[HEADSET]`: "working with real depth and good frame rate. Controls feel good but there is weird flickering of
  certain objects and random black streaks". Log `[LIVE]`: SYNC/s simulated = frozen ticks (≈38–41 pairs/s in the
  last receipt), published = packets, no faults → the pair mechanics are healthy; the defect is in rendering.
- Cause: 0.9.0 forced the camera-cut flag cam+0x7D8 on every VR frame (`history_reset` default 1 for xr 3 and 4).
  Every temporal consumer therefore ran in its first-frame state permanently: TAA output = one jittered frame
  (sub-pixel shimmer, dithered LOD/fade/hair/foliage flicker), temporal effects with history (SSR/SSAO/volumetric)
  never converge (dark streaks). Playbook ch14 `#velocity-not-temporal` `[HEADSET]`: under alternate frames TAA is
  fine; velocity-buffer effects (motion blur) are the blocker — "measure first".
- RE `[STATIC]` of the engine's prev-frame chain (all inside GameTick, every tick, game thread):
  `Cams_StorePrev` loop 0x59C710 (call 0x4C4F61, gated by clock+0x28 ≠ 0, true on frozen ticks too) →
  `Cam_StorePrev` 0x8085D0: +0x1C0 ← world +0x140, +0x180 ← +0x100, +0x380 ← VPrel +0x430, +0x3C0 ← unjittered VPrel
  +0x470, +0x560 ← pos +0x550, +0x674 ← time +0x670. Then `Cam_TemporalUpdate` 0x808EF0 (loop 0x59CB80, call
  0x4C502C): on cut (+0x7D8) seeds prev = current; builds reprojection +0x300..+0x370 / +0x2C0..+0x2FC from the
  prev slots, dt +0x650 = +0x670 − +0x674, then slot 42/43 unjittered (+0x694 jitter cleared) → +0x280 ← +0x240,
  +0x470 ← +0x430, then slot 42/43 jittered. Our slot-43 cut write lands after the 0x808EF0 entry check, i.e. it
  reached the render-side history reset (Streamline/TAA read the flag) and was cleared by Frame_59F2B0.
- Consequence for xr=4: the prev camera is always the LAST RENDERED frame (the other eye), and the history textures
  hold that same frame. Prev matrices and prev images describe the same image, so the engine's reprojection is
  correct across the 6 cm baseline without any reset. (A per-eye matrix bank alone would be WRONG here: it would
  pair this eye's old camera with the other eye's history image → the half-IPD ghost the playbook describes.
  A per-eye bank needs both halves — matrices and textures — and is the follow-up only if the cross-eye
  reprojection shows ghosting.)
- Fix 0.9.1: `history_reset` default 0 for xr=4 (still 1 for xr=3); F6 = A/B in one session; SYNC/s shows the mode
  and the cuts the engine raises itself (`Cam_TemporalUpdate` hooked at entry, count only). README: motion blur,
  depth of field, film grain off.
- Open risk to watch: frozen-tick per-object velocity (if object prev transforms advance only on simulated ticks,
  moving objects get velocity over-shoot in the right eye) → would show as ghosting on moving NPCs/grass in one eye.
- Self-test: 33/33 signatures (new: Cam_TemporalUpdate, Cam_StorePrev), all hooks create/enable/disable, harness and
  sweep simulations PASS (receipts/gotvr091_selftest.log). Host unchanged (fail-closed test: state 4, exit 2).

## GoTVR 0.9.1 → 0.9.2 — history KEPT: one-eye objects/shadows + faint double edge; history RESET: light flashes (BUG-23)
- User `[HEADSET]` + `[MONITOR]` A/B (log receipts/gotvr091_live.log: 7 F6 presses, SYNC/s 56–66 pairs/s; cuts/s ≈
  ticks in RESET, 0 in KEPT): KEPT → "faint double edge to close objects … some objects and shadows were only rendered
  in one eye, and kept flickering"; RESET → "both eyes rendered the correct image" (and no blue hue) but "random
  flickering of lights across the screen … objects randomly turned very bright for a split second", monitor too.
- So neither global setting is right: the cut flag cam+0x7D8 has consumers with opposite needs. Static census of the
  readers (23 sites, decompiled with true function starts from .pdata: out/re/decomp/cut):
  - visibility: 0x9BB4F0 (per-view flag +0xE1 = cut || camera moved > 500 → occlusion reset; reads previous-slot
    data at +0x508 + (1−sel)·0x18), RV_9E5350 (occlusion setup, previous slot (1 − p) at 0x1433891A4/0x143389240),
    List2_Build 0x6319C0 (round-robin light-list update, full rebuild on cut). Previous frame = other eye's
    viewpoint → culls what this eye sees (playbook ch14 #cpu-visibility-history, "results deferred a frame").
  - TAA/AA: 0x618D30 (history valid iff AA+0x1C010 == frame−1 and no cut; jitter = frame & 7 or ±0.25 by p),
    0x614970 (AA history pyramid ping-pong at AA+0x11618 + p·0x628 / (1−p), cleared on reset), 0x610CC0
    (reprojection pass skipped on cut), 0x609B90, 0x60F9E0 (Streamline), 0xAF9850.
  - lighting/temporal accumulation: 0x63C130 (blend 0.95 → 0 on cut or frame gap), 0x63D4C0 (previous-slot
    exposure exp(−ln2·E[1−p])), 0x63D960, 0x60C7F0.
  - gameplay camera users (0x672F20, CamUser_673610, 0x678000, 0x66E870, 0xA46100, 0xA57D10, 0xA73CB0) and the
    scheduler 0x4DB3A0: left native.
- Double-buffer index p = 0x14148A3F8 (and +0xC), toggled once per tick by 0x577820 at GameTick end → under xr=4 the
  left eye always runs with p = a and the right with 1 − a; every "(1 − p)" read is the other eye's frame.
- Fix 0.9.2: 13 reader instructions redirected to stubs near the image (`push rax; lock inc hits; mov eax,[reg+0x7D8];
  or eax,[force_group]; cmp eax,0|r32; pop rax; jmp back`, fail closed on byte mismatch, other threads suspended
  and stepped out of the sites while patching). Groups V / T1 / T2 forced independently; presets on F6:
  1 HYBRID (V forced; T1, T2 keep) default, 2 HYBRID-B (+T1 reset), 3 HYBRID-C (+T2 reset), 4 KEPT, 5 RESET.
  SYNC/s logs the preset and reader hits per group per second (proves the sites run).
- Self-test: 13/13 site byte checks; every stub EXECUTED with a fake camera for cut ∈ {0,1} × force ∈ {0,1}
  (52 cases, result = cut | force); patch + restore on the mapped image; 33/33 signatures; harness/sweep PASS
  (receipts/gotvr092_selftest.log).
- Next: per-eye TAA history (STR-015: bank the AA ping-pong per eye, GPU copies at the frame boundary) if the
  double edge remains the main defect in preset 1.

## GoTVR 0.9.2 → 0.9.3 — preset results, AFR-like pair mixing in the host (BUG-24, BUG-25)
- User `[HEADSET]` per preset: 1 (V forced) light flicker; 2 (V + T1 forced) "gets rid of certain shadows, lighting,
  and objects on each eye" + flicker; 3 (V + T2) like 1; 4 KEPT "totally fixed the bug, proper lighting, but some
  objects flickering in and out of existence per eye when I moved my head (culling)". → BUG-24: the light flicker is
  in group V (it was also present in RESET); T1 contains the depth pyramid the GPU occlusion test reads (0x614970
  clears both ping-pong slots on reset → everything tests as occluded); KEPT's pop-in = occlusion from the other
  eye's previous frame. 0.9.3 splits V into VV (0x9BB95A view flag), VO (0x9E5393 RV occlusion setup), L (0x631B92
  light lists) and offers 1 VV+VO, 2 VV, 3 VO, 4 L, 5 KEPT, 6 RESET; the preset number is drawn as squares into the
  shared eye textures for 3 s (ClearRenderTargetView rects; shared textures RT-capable for typed formats only).
- BUG-25 (found by review while answering "is this true stereo, not AFR?"): the host chose the newest completed image
  PER EYE, so between pair k+1's left publish and its right publish the headset showed L(k+1) with R(k) — one
  simulation tick apart, i.e. an AFR pair, at every pair boundary. Fix: protocol v4; in xr=4 both eyes of a pair carry
  the left eye's frame id (right = left + 1) and the host shows only the newest pair present in both eyes
  (`hostPairs` counts pair switches; game logs pairs published/s).
- Self-test: 33/33 signatures, cut-override stubs executed 52/52, harness/sweep PASS; host fail-closed test state 4.

## GoTVR 0.9.3 → 0.9.4 — the culling owner is RV_OcclusionSetup; per-eye occlusion history (BUG-26)
- User `[HEADSET]`: setting 1 (VV+VO forced) light flicker, no object flicker; 2 (VV only) no light flicker, object
  and shadow flicker; 3 (VO only) = 1. → the occlusion setup RV_9E5350 (0x9E5393 reader) owns both: its history is
  the cross-eye pop-in, and invalidating it (view+0x34 = 0) is the light flicker. VV and light lists are innocent.
- RE `[STATIC]` RV_9E5350 (GameTick → 0x5A07F0 → 0x5A08B0, game thread, CAM-A): steady state (DAT_141804010 ≠ 0,
  history valid) = branch with view+0x120 = 0: source HiZ record view+0x110 = slot p (0x143389240 + p·0x20), slot
  (1 − p) = this frame's destination (+0x40 descriptor, +0xF0 dims), reprojection with the PREVIOUS camera
  (+0x380 VPrel rows → +0x60..+0x9C scaled by (w/2, −h/2, h/2) from dims 0x1433891A4 + p·0x34, raw rows →
  +0xB0..+0xEC, +0x560 pos → +0xA0). First frame after a cut = +0x120 = 1 (current camera).
- Fix (playbook STR-015, both halves, for this consumer only): Cam_StorePrev hooked to bank the camera it copies
  (= last tick's eye) per eye; RV_9E5350 post-hook, steady state only, dims equal: source := slot (1 − p) (this eye's
  HiZ from two frames ago, rebuilt later in the frame), matrices/pos := this eye's banked camera. Engine cuts,
  dims changes and missing banks fall through untouched. Presets: 1 PER-EYE OCCLUSION (default), 2 VO reset,
  3 KEPT, 4 RESET. `OCC/s` logs calls/fixed/skips.
- Self-test: 34/34 signatures; per-eye occlusion unit test on the mapped image (slot swap, scaled rows, pos, cut
  frame untouched) PASS; cut stubs 52/52; harness/sweep PASS.
- Risk to watch: if the HiZ for frame N is rebuilt BEFORE the occlusion test runs, slot (1 − p) holds the current
  frame — then setting 1 shows mild pop-in only while moving (matrices two ticks old).

## GoTVR 0.9.4 → 0.9.5 — per-eye TAA history (BUG-27), monitor showed both eyes (BUG-28)
- User `[HEADSET]` 0.9.4 setting 1: no light flicker, no pop-in (BUG-26 confirmed fixed); "very close objects have a
  faint double edge"; monitor "displays two frames overlapped".
- BUG-27 RE `[STATIC]` (re/decomp/taa): AA_Resolve 0x614EB0(AA 0x143376280, ctx) — unless reset (AA+0x1D40E):
  TAA_Draw 0x7E7D10(shader, ctx, CAM-A, scene [frame+0x18]+0x88, history AA+0x193A8 (= view of record AA+0x19320),
  velocity AA+0xEA90, exposure) → AA+0x8788 → copied to AA+0x640 (output); then the current scene is copied into
  record AA+0x19320. Carried history = previous scene colour only. Reprojection: TAA_Draw → FxBindCamera 0x56DF80 →
  RV_9A2610 reads cam +0x240.., +0x340..+0x37C (prev-relative VP from Cam_TemporalUpdate), +0x430.., +0x550, +0x694,
  +0x828..; velocity pass AA_Velocity 0x614720 (only caller 0x610D15) reads cam +0x400..+0x42C, +0x6A4.., +0x6B7.
  TAA_Draw's only caller is AA_Resolve (0x615069).
- Fix (STR-015 both halves): (1) matrices — Cam_TemporalUpdate runs once more per tick on CAM-A with this eye's banked
  prev slots (+0x180..+0x1FF, +0x380..+0x3FF, +0x560, +0x674 from Cam_StorePrev), the reprojection outputs
  (+0x2C0..+0x37F, +0x400..+0x42F) are kept, the whole camera object (0x8D0) is restored, then the normal run (all
  other consumers keep the engine chain); (2) TAA_Draw and AA_Velocity receive a thread-local copy of CAM-A carrying
  those outputs, only when the live camera position equals the run's (identity check, else live camera);
  (3) texture — inside AA_Resolve's own command list: record AA+0x19320 ← bank[eye] before (engine transition to
  COPY_DEST + flush, CopyResource), bank[eye] ← record after (COPY_SOURCE); banks are committed simultaneous-access
  copies of the record's desc (recreated on any desc change); engine reset (+0x1D40E) → no restore, banks reseeded.
- BUG-28: under xr=4 consecutive presents alternate eyes, so the window showed L and R alternately at the monitor
  refresh. On a right-eye present the back buffer now receives this pair's left image after the right image was
  captured (COPY_SOURCE → COPY_DEST → PRESENT barriers in the transport list); `monitor_left_eye` ini key.
- Presets: 1 occlusion + TAA per eye (default), 2 occlusion per eye (0.9.4), 3 + TAA reset, 4 KEPT, 5 RESET.
- Self-test: 37/37 signatures; per-eye occlusion test PASS; per-eye TAA camera test (velocity + draw get the copy with
  this eye's reprojection, stale identity falls back to the live camera) PASS; cut stubs 52/52; harness/sweep PASS.
- Risks to watch: moving objects in the right eye (per-object motion vectors are written by geometry passes from the
  engine's cross-eye chain); only the TAA path (not DLSS/FSR/XeSS) is banked.

## GoTVR 0.9.5 → 0.9.6 — "shadows moving as I move my head" (BUG-29, BUG-30)
- User `[HEADSET]`: monitor fixed (BUG-28 confirmed). Setting 1: shadows move with head motion.
- Log `[LIVE]` (receipts/gotvr095_live.log): `TAA/s: same-eye matrices≈130 draw≈130 velocity≈130 | history restored=0
  saved=0`, no "taa: … textures" line → the texture half never ran.
- BUG-30: AA+0x19320 / AA+0xEA90 are render-target HANDLES; Res_Transition takes the resource record
  *(handle+0x10) (RT_ResOf 0x7E10F0: `mov eax,[rcx+14h]; mov ecx,[rcx+10h]; shl rax,20h; or rax,rcx`, used by
  RT_ToX 0x7E3E50). nativeOf() resolved the handle itself → null → no bank. So 0.9.5 reprojected the OTHER eye's
  history with THIS eye's motion: everything drifted by the eye offset as the head moved (shadows most visible).
  Fix: resolve through +0x10 (self-test: handle → record → object → native chain).
- BUG-29 (design): the velocity buffer AA+0xEA90 feeds other temporal passes with their own cross-eye history, so a
  per-eye velocity must stay private to the TAA: AA_Velocity now runs twice (this eye's camera → copied to a private
  texture; then the normal camera for everyone else) and AA_Resolve swaps the private one in around the TAA only
  (parked/restored in the same command list). Preset 2 keeps the 0.9.5 shared variant for comparison.
- Self-test: 37/37 signatures, per-eye occlusion PASS, per-eye TAA camera + RT→resource chain PASS, cut stubs 52/52,
  harness/sweep PASS.
- DLSS (user question): Streamline keeps DLSS history per viewport (sl::ViewportHandle). The route for DLSS is one
  viewport per eye (slSetTag / slSetConstants / slEvaluateFeature / slDLSSSetOptions: right eye → viewport 1) with
  per-eye constants (clipToPrevClip from the same per-eye camera bank) and the per-eye motion vectors built here.
  sl.interposer imports in the game: slSetConstants, slSetTag, slEvaluateFeature, slGetFeatureFunction, … → planned
  after the TAA path is confirmed in the headset.

## GoTVR 0.9.6 → 0.9.7 — ghosts still moving with the head in setting 1; setting 3 "effectively perfect" (BUG-31)
- User `[HEADSET]`: setting 1 (per-eye TAA) "almost ghost-like objects moved with my head"; setting 3 (per-eye
  occlusion, engine TAA) effectively perfect.
- Log `[LIVE]` (receipts/gotvr096_live.log): `taa: history RT …f5a0 -> resource record bf80000000000001 -> native 0`;
  velocity RT AA+0xEA90 resolved fine; `TAA/s … velocity (private 416, swapped 416) | history restored=0 saved=0`.
  So again: same-eye reprojection + velocity on the cross-eye history.
- BUG-31: AA+0x19320 is the 0xA8-byte target RECORD (SetRenderTarget scope 0x4DBE20, dims at +0); its texture handle
  is record+0x88 = AA+0x193A8 — exactly what AA_Resolve passes to TAA_Draw as the history (and the scene input is
  likewise [frame+0x18]+0x88). Fix: bank the handle AA+0x193A8 (resource = *(+0x193B8)).
- Hardening (STR-015 "one bank"): the per-eye reprojection (TAA_Draw camera copy) and the private velocity swap are
  applied ONLY when this eye's history was restored in the same AA_Resolve call (thread-local flag around the
  original); otherwise the engine chain runs untouched = setting 2 behaviour. The shared-velocity diagnostic preset
  is gone. Presets: 1 per-eye occlusion + TAA, 2 per-eye occlusion (perfect), 3 + TAA reset, 4 KEPT, 5 RESET.
- Self-test: TAA camera test now also checks that no camera copy is used without a restored history; all PASS.

## GoTVR 0.9.7 → 0.9.8 — crash at the main menu (BUG-32)
- User: "the game crashes once reaching the main menu screen". Log `[LIVE]` (receipts/gotvr097_live.log): the
  history now resolves (`history RT …f628 -> record …1ce0 -> native …3280`, 2560x1440 fmt 10) and saves run
  (menu = TAA reset every frame, saves = resets), but "taa: history textures …" is logged again in pairs every few
  seconds, then ticks/presents stop ~35 s in; host.log: 721,596 × `xrBeginFrame -> XR_ERROR_RUNTIME_FAILURE`
  (46 MB) = the D3D12 device was lost.
- Cause: the history handle's native resource changes now and then (multi-buffered/re-bound by the engine);
  ensureSet() treated an identity change as "recreate" and Released the two bank textures immediately while command
  lists that copy into/out of them were still queued or executing → GPU use-after-free → device removed.
- Fix: same desc → keep the banks, only note the new source (`source switches` in TAA/s); desc change → old banks to
  a graveyard released ≥ 3 s later (or leaked if full, never freed in flight). Device-removed reason is logged once if
  it ever happens. Host 0.7.1: identical XR failures rate-limited (first, 10th, every 10,000th) with a 10 ms back-off.
- Playbook ch07/ch19 rule applied: never release a GPU object the queue may still reference; tie lifetime to fences
  or to a conservative delay when no fence of that queue is available.

## GoTVR 0.9.8 → 0.9.9 — per-eye TAA runs but leaves outlines in place on head motion (BUG-33, open)
- User `[HEADSET]`: no crash (BUG-32 fixed); setting 2 (engine TAA) great; setting 1 "like an outline of the objects
  themselves are still in place when I move my head".
- Log `[LIVE]` (receipts/gotvr098_live.log): `history restored=107 saved=107 draw=107 velocity swapped=107
  source switches=0` — all three per-eye pieces execute every frame. The artifact = this eye's history reprojected
  with too little motion, so one of the pieces carries the wrong motion (candidates: the extra Cam_TemporalUpdate
  run's outputs are not what the shader uses, TAA_Draw's constants come from more than the camera copy, or the
  velocity texture also carries per-object/other-pass data from the cross-eye chain).
- 0.9.9: default back to the headset-approved preset (per-eye occlusion, engine TAA). Per-eye TAA split into bits
  (1 history, 2 camera constants, 4 velocity) as presets 2–5 for a per-piece headset bisection (STR-015 recipe), plus
  a per-second `TAA camera:` line: mean |current − previous| eye position for the same-eye and cross-eye previous
  cameras, and mean max |same-eye +0x340 rows − engine rows|.

## GoTVR 0.9.9 → 0.10.0 — bisection result: the private velocity was the broken piece (BUG-33 closed)
- User `[HEADSET]`: presets 1 (engine TAA), 3 (history + reprojection), 5 (history only) — no flickering or ghosting;
  4 (history + velocity) ghosts. Log `[LIVE]` (receipts/gotvr099_live.log): `TAA camera: |current − previous|
  same-eye 0.01 cm (still) / 6.75 cm (moving), cross-eye 6.31 cm` = the bank holds this eye's camera and the
  cross-eye previous is one IPD away, as designed; same-eye reprojection differs from the engine's by up to ~8 units.
- Interpretation: AA_Velocity 0x614720 does not just re-derive camera motion; it reads a ring entry
  [frame+0x18]+0x88+(+0x620)·0xA8 (most likely the previous frame's depth = the other eye's) and reprojects it, so
  feeding it this eye's matrices pairs them with the other eye's depth. The TAA's camera reprojection comes from the
  constants (+0x340 rows) — the piece that works.
- 0.10.0: preset 1 = per-eye occlusion + per-eye TAA (bits history + constants); velocity left to the engine.
  Presets: 1 per-eye TAA (default), 2 engine TAA, 3 TAA reset, 4 KEPT, 5 RESET.

## GoTVR 0.10.0 → 0.10.1 — preset 1 clean but 20 fps (BUG-34)
- User `[HEADSET]`: setting 1 no ghosting (per-eye TAA confirmed), frame rate "tanked all the way down to 20".
- Log `[LIVE]` (receipts/gotvr0100_live.log): 529 × "taa: history textures 2560x1440 format 10" (same printed shape),
  ticks/s falling 190 → 6–28. Cause: the history handle alternates between two native resources whose
  D3D12_RESOURCE_DESC differs in fields irrelevant to a copy (flags/alignment/layout); ensureSet() compared the whole
  struct and recreated two full-resolution textures on every switch; the 32-entry graveyard overflowed and leaked →
  VRAM exhaustion (plus per-frame CreateCommittedResource).
- Fix: compare only the copy-relevant shape (dimension, size, array, mips, format, samples); hard cap of 64 creations
  per session (beyond: per-eye TAA disabled → engine TAA); creation log now shows flags/alignment/layout and a counter;
  TAA/s shows `recreations=`.

## GoTVR 0.10.1 → 0.10.2 — distant shadows/objects invisible in some head directions (BUG-35)
- User `[HEADSET]`: setting 1 great, frame rate normal, no ghosting (BUG-34 confirmed). "Some shadows in the
  distance and objects go invisible when I turn my head"; asked: they "stay gone" while looking there and come back
  only when looking forward again → not latency: a culling owner that does not follow the head (playbook ch01
  "you cannot render your way out of missing geometry", CAM-002).
- RE `[STATIC]`: Cam_UpdateWorld 0x8081F0 writes the world rows +0x140..+0x17F and copies them to +0x850..+0x88F
  (0x80854E..0x808578) immediately before calling slot 43 (our eye-pose rewrite of +0x140 only). CAM-A's
  +0x850/+0x880 copy is read by 0x611E00 (with +0x550), 0x649ED0 (forward row), 0x728260, 0x74F1F0 (via the camera
  list head 0x141EDBCC8), 0x5A4740 … — consumers that kept culling/fitting from the game camera's direction.
- Fix: in xr=4 VR frames the slot-43 hook also writes the eye-composed world into +0x850..+0x88F. F7 / ini
  `cull_follows_head` toggles it; MONITOR/s logs the state and writes/s. Watch item: any gameplay reader of the copy
  (movement relative to camera) would now follow the head.

## GoTVR 0.10.2 → 0.10.3 — culling misses only during FAST head turns (BUG-36)
- User `[HEADSET]`: 0.10.2 changed nothing; "the culling ONLY happens when I move my head fast, even if I look at the
  object"; slow is fine; movement does NOT follow the head (so the +0x850 copy has no gameplay reader worth worrying
  about; kept on).
- Cause (reasoned, consistent with the symptom): the per-eye occlusion history is two ticks old (twice the engine's
  designed latency, and at pair rate); a fast turn produces a large rotation between the HiZ's camera and the current
  one, and objects entering from outside the old view are tested against clamped edge depth → culled while the turn
  lasts. Playbook ch14: "treat a pose gap of several frames as a camera cut and reseed"; STR-015: detect your own pose
  gaps in addition to the engine's.
- Fix: occ::fastCheck at Cam_TemporalUpdate (before the occlusion setup in the same tick): angle between this eye's
  current world rows (+0x140) and its banked previous world (+0x1C0 of two ticks ago); above fast_turn_deg (2.0):
  mode 1 = force the occlusion-setup cut (VO) for that tick (history invalid → everything drawn), mode 2 = leave the
  engine's cross-eye 1-frame history for that tick, mode 0 = off. F8 cycles; OCC/s logs fast frames + last angle.
- Self-test: fast-turn detection (1° slow, 5° fast, back to slow) PASS; all other tests PASS.

## GoTVR 0.10.3 → 0.11.0 — residual culling at very high turn speed (BUG-38); per-eye DLSS (BUG-37, first version)
- User `[HEADSET]`: fast-turn fix works "unless I turn at really high speeds"; asked for DLSS.
- BUG-38: the eye frustum (= cull frustum) had no margin; at very high angular speed the compositor's reprojection
  reveals area outside what was rendered/culled. Playbook ch01 BuildCullFrustum: union of the eyes plus margin "for
  reprojection and the pose moving between cull time and render time". 0.11.0 widens each eye's tangents by
  fov_margin_deg (3.0) on all sides for rendering; the host already declares the rendered frustum per slot, so the
  compositor crops correctly.
- BUG-37 RE `[STATIC]`: StreamlinePass_cb 0x60F9E0 = upscaler pass; inputs include AA+0xEA90 (= 0x143384D10) as
  motion vectors, the depth ring entry, exposure; Cam_BuildSLConstants 0x807140 builds sl::Constants from CAM-A
  (+0x200, +0x2C0..+0x2FC reprojection, +0x550, +0x694 jitter from table AA+0x192A0 by frame & 7, +0x7D8 reset;
  depthInverted = cameraMotionIncluded = 1) and calls slSetConstants with ViewportHandle
  {171b6435-9b3c-4fc8-9994-fbe52569aaa4} value 0 (SL headers: NVIDIAGameWorks/Streamline include/, MIT).
- Implementation (playbook ch19 #dlss-in-vr: separate history per eye is the requirement; per-eye inputs; fail
  closed): sl.interposer slSetTag / slSetConstants / slEvaluateFeature / slGetFeatureFunction hooked (MinHook on the
  exports), slDLSSSetOptions hooked through slGetFeatureFunction; right-eye frames (g_camEye == 1) are remapped to
  viewport value + 0x40; every SetOptions is also applied to that viewport; nothing is remapped (and no per-eye
  constants / motion vectors are used) until that succeeded once. Cam_BuildSLConstants receives the camera copy with
  this eye's reprojection; AA_Velocity's this-eye run is swapped into AA+0xEA90 around StreamlinePass_cb only.
  F9 / ini dlss_per_eye: 1 all three, 2 history + constants, 0 off. DLSS/s log line.
- Self-test: viewport remap (right → 64, left untouched, original struct untouched, fail-closed without options)
  PASS; 39/39 signatures; all earlier tests PASS.
- Known limits: jitter phase = frame & 7, so each eye sees 4 of the 8 phases (playbook: same phase for both eyes of a
  pair, ≥ 8 phases per eye) — next step if DLSS looks soft; the motion-vector half reuses the AA_Velocity this-eye
  run, which the TAA bisection showed is NOT what the TAA wants — DLSS may differ (mode 2 is the comparison).

## GoTVR 0.11.0 → 0.11.1 — DLSS Quality "jittery, including the headset position"; F9 no effect (BUG-39)
- Log `[LIVE]` (receipts/gotvr0110_live.log): sl.interposer hooks installed, but no DLSS/s line ever (zero
  slSetConstants/slEvaluateFeature calls) → DLSS does not go through Streamline here.
- RE `[STATIC]`: the exe exports a statically linked NGX shim (NVSDK_NGX_D3D12_* at 0x4B9150..0x4B9710, forwarding
  through function pointers at 0x1419E0208/…210/…218); the engine's DLSS wrapper calls CreateFeature (0x469EA8),
  EvaluateFeature_C (tail jump 0x46AE54), ReleaseFeature (0x46AED8). One NGX feature handle = one DLSS history, so
  under xr=4 both eyes accumulated into one history → output wobbling by the eye offset ("headset position
  jittering").
- Fix: hook the three shims. CreateFeature(feature 1 = SuperSampling) records the game's handle H0; the first
  right-eye EvaluateFeature_C(H0) creates H1 with the same command list and parameter object (fail closed: on error
  both eyes keep sharing, logged once); right-eye evaluates of H0 go to H1; ReleaseFeature(H0) releases H1 too; a new
  H0 (resolution/quality change) drops the old H1. perEye() (gates this-eye constants and the motion-vector swap)
  is now true when H1 exists. F9 / dlss_per_eye: 1 own feature + this-eye motion vectors, 2 own feature, 0 shared.
- Self-test: NGX routing (left keeps H0, right gets a second feature created once, F9 off → H0, release clears)
  PASS; 41/41 signatures.

## GoTVR 0.11.1 → 0.12.0 — DLSS scrapped; UI and videos "huge in headset", wrong aspect (BUG-40, BUG-41)
- `[LIVE]` (user, 0.11.1): DLSS per-eye mode 1 ghosting, modes 1–3 all jittery, and slower than TAA.
  Why it is slower: xr=4 renders two complete engine frames per stereo pair, so the frame is CPU/simulation-bound;
  DLSS only reduces GPU shading cost, while the second NGX feature + per-eye inputs add work. Why it jitters: DLSS
  assumes consecutive evaluates come from one camera stepping through one sub-pixel jitter sequence; with eyes
  alternating, each eye sees every other jitter phase (frame & 7 → 4 of 8 phases) and the shared parts of the
  inputs carry the other eye's motion. Decision: **dlss_per_eye defaults to 0 and the Streamline hooks are not
  installed** (NGX shim hooks pass through); use the game's TAA. F9 is now the HUD size.
- BUG-40 `[STATIC]` cause: the eye image is the game's 2560×1600 (16:10) back buffer declared to the runtime with
  the eye frustum (~100°×100°): the 3D projection is correct (rendered with that frustum), but the UI is laid out
  for a 16:10 screen and therefore covers the whole field of view, vertically stretched.
  RE: UI passes 60/61/64 render into their own target 0x143377510 (pass 60 pre 0x6E3790, setup 0x6F0860,
  Ctx_InheritState 0x7E5170 re-binds it for continued contexts) through RT_Bind 0x7E34E0, which is the only caller of
  Ctx_SetViewport 0x50ECA0 and sets the scissor via Ctx_SetScissor 0x50EC60 (also per draw via Ctx_ScissorNorm
  0x7E52C0). Both act on the bound rect ctx+0x2948; the engine composites the UI target over the frame later.
  Fix (playbook ch04 HUD-004 adapted, one layer): while the UI target is bound, viewport + scissor are mapped into
  the rect where a flat panel (hud_width_deg wide, the UI's aspect, hud_distance in front, hud_down_deg below eye
  level) projects in THIS eye's rendered frustum, with the half-IPD offset → both eyes converge at hud_distance.
  Layout untouched (ctx+0x2948 stays full size). F9 cycles 40/55/70/85°/off.
- BUG-41 `[STATIC]` cause: pre-rendered videos are drawn full-screen into the back buffer by Movie_Draw 0x7EAF80
  (from Movie_Render 0x7A1040 at the end of StreamlinePass_cb, letterboxed to 16:9 inside the screen aspect) → the
  video filled the whole lens. Fix (HUD-002/003): frames with a video draw (8-frame hysteresis), or without a 3D world
  (frame flag 0x141EBD2C0+0x80F09 == 0, RV_Post starts at pass 59), are flat contexts: HUD mapping off (subtitles
  stay on the video), slot flag GV_SLOT_THEATER (protocol v5); GoTVRHost 0.8.0 shows the left image on a
  world-locked, gravity-aligned quad layer (3.2 m wide at 3 m, image aspect preserved, black room by default),
  placed where you look when it appears (re-placed after 3 s without flat frames or F11). F10: auto/always/never.
- Self-test: 47/47 signatures; UI test (panel geometry, ±16 px convergence for a 64 mm IPD at 2 m, RT_Bind →
  real Ctx_SetViewport code → mapped viewport and scissor, per-draw scissor mapping, other targets untouched,
  flat context leaves the UI full-screen, video/world detection, slot flags) PASS; all earlier tests PASS.
- Known limits / to verify in the headset: the HUD panel is head-locked (world-locked only in theatre); full-screen
  UI effects (pause-menu dimming, letterbox bars drawn by the UI) shrink with the panel; if the UI target is cleared
  by a viewport-limited draw instead of a full clear, old UI could remain outside the panel (F9 off is the check).

## GoTVR 0.12.0 → 0.13.0 — pause menu head-locked; menus low-res; start screen follows the head (BUG-42..44)
- `[HEADSET]` 0.12.0: pre-rendered videos on the theatre quad PASS; HUD single (no double image) PASS; HUD and pause
  screen follow the head (by design in 0.12: head-locked panel). Owner: menus too low resolution; pause menu on a
  stationary screen in a black void like the videos; start screen anchored in space.
- BUG-42 pause `[STATIC]`: the game clock 0x141489DF0 combines 28 time-scale reasons at +0x84 (Clock_SetScale
  0x56F340(clock, reason, scale)). Reason 7 = refcount of open UI screens flagged "pauses the game" (screen+0x7F8;
  0x7D6E10 first opener sets 0, 0x7D7240 last closer sets 1) → pause menu, map, gear, options. Reason 8 = video
  (Movie_Tick 0x7A1190), 9 = world load/reset (0x59AAC0). Fix: a flat-context decision once per tick at GameTick
  start (pause reason in pause_channels, default 0x80 | video | no 3D world | F10); in a flat tick the game renders
  its OWN flat camera/projection (no eye pose, no right-eye tick, every tick simulated), the frame is published to both
  slots tagged GV_SLOT_THEATER|GV_SLOT_FLAT (protocol v6), the host shows it on the world-locked screen in a black
  void (never as a projection: no pose), and 2 camera cuts are forced at each flat↔stereo switch. theater_flat_render=0
  keeps the stereo image on the screen instead.
- BUG-43 resolution (analysis, fix next): the eye image is the 2560×1440 back buffer over a ~106°×100° frustum →
  ~24 px/° horizontally and only ~14 px/° vertically; the HUD panel maps the UI into ~half of that. The playbook fix
  is the UI on its own compositor quad at the UI target's own resolution (04 "Get UI off the backbuffer";
  HUD-004). RE for it: Ctx_BindColor 0x50E430 stores the colour attachment at ctx+0x2C8, attachment+0x18 = resource
  record (Res_Transition target) → native via the TAA chain; RT binding is lazy (ctx+0x340 dirty bit). 0.13.0 logs a
  one-time "UI probe": whether the UI target colour is the back buffer or a separate texture, its format and size.
- BUG-44 start screen (analysis): needs a title-screen signal; 0.13.0 logs every time-scale reason change and the raw
  world mode byte (+0x80F09: 0 none, 1/2 seen by the pass-65 fade 0x728400) to find it, then the UI layer is placed
  world-locked there (HUD-002).
- Self-test: 47/47; UI test extended (pause reason 7 → flat, reason 9 ignored, flat render on/off, flags, cut count)
  PASS; all earlier tests PASS.

## GoTVR 0.13.0 → 0.14.0 — cinematic bars and fades confined to the HUD rectangle; UI layer; start screen (BUG-45..47)
- `[HEADSET]` 0.13.0: whole prologue stable; pause menu and map on the theatre screen exactly as intended (PASS).
  Probe `[LIVE]`: the UI passes draw into a SEPARATE 2560×1440 RGBA8 texture (flags 0x5), composited later. Census:
  pause = reason 7 (confirmed: 1→0 on open, 0→1 on close); reason 27 = 0 from boot through the start screen until the
  first world finishes loading (frame+0xA0 != 0), again briefly before loads; reason 8 = video; 9 = load.
- BUG-45 fades `[STATIC]`: pass 65's pre-callback 0x610460 → 0x728400 draws the full-screen fade into the target its
  context inherited from pass 64 (Ctx_InheritState → RT_Bind(UI target)) → mapped into the HUD panel. Fix: hook
  0x610460: full viewport/scissor, and (layer mode) the engine's own UI image for the fade draw (ctx+0x340 dirty bit
  re-issues the engine RTV).
- BUG-46 letterbox bars: the UI layout asks the display service (services+0xC8) vt+0x70("Letterbox") whether a
  cinematic letterbox is pushed (0x6E9544: layout falls back to a 1920 canvas then). 0.14.0 asks the same each tick;
  while true the UI mode is CINEMA: no mapping, no layer → bars, fades and subtitles are the whole frame (head-locked
  across the view, as on a monitor). To verify live: "UI mode -> CINEMATIC" lines during letterboxed scenes.
- BUG-43 resolution, implemented (playbook 04 "Get UI off the backbuffer", "rebind the target once"): after
  RT_Bind(UI target) the command list awaits its next OMSetRenderTargets (lazy bind), whose RTV is the engine's UI
  view (locked in after 3 identical captures; never seen after 900 binds → layer off, panel mapping); it is replaced
  by our own render target of the same desc. The engine image is cleared once per frame (its composite adds nothing);
  ours is copied at Present into shared UI images (GV_TEXCOUNT 9, protocol v7) and cleared. GoTVRHost 0.10.0 shows it
  as an alpha-blended quad (premultiplied by default) at 2560 px over the HUD width: ~2× the panel's pixel density.
  HUD: head-locked quad (VIEW space). Start screen (reason 27 == 0, world mode 2, not paused): world-locked,
  gravity-aligned panel placed on entry (HUD-002), 3D world behind in stereo. Flat/cinema: no layer.
- Known: the desktop mirror has no HUD while the layer is on; the HUD stays head-locked (forearm HUD planned with
  first person).
- Self-test: 50/50 signatures; UI layer test (lock-in, substitution, once-per-frame engine-image clear, other targets
  untouched, fade un-substitution, cinema = no layer, title = world flag) PASS; all earlier tests PASS.

## GoTVR 0.14.0 → 0.14.1 — frame rate halved; start screen not anchored; intro videos missed (BUG-47..49)
- `[LIVE]` 0.14.0 log: ~31 stereo pairs/s instead of ~72 (census present=54-65/s, 0.13.0: ~145). UI layer refused:
  "UI texture 2560x1440 does not match the shared images 2560x1600" (the game now letterboxes a 2560x1440 render
  inside a 2560x1600 window) → panel mapping, so the start screen stayed head-locked.
- BUG-48 frame rate `[STATIC]`: 0.14.0 called taa::listOf(ctx) (VirtualQuery via readable()) in the RT_Bind hook for
  EVERY bind (82 call sites, thousands per frame, all render job threads) - VirtualQuery is a syscall that takes the
  process address-space lock, so the render jobs serialised on it. Fix: non-UI binds touch nothing unless a list is
  awaiting/substituted (counter); the list is read with the same plain derefs the engine's Ctx_SetViewport does;
  letterbox query without VirtualQuery. No VirtualQuery left on any per-bind / per-draw path.
- BUG-47 layer refused: the UI images are now their own shared textures at the UI target's size/format (protocol v8,
  uiGen); GoTVRHost 0.11.0 opens them separately. The game also publishes where the picture sits inside the eye image
  (centred UI-target-sized rect when smaller); the host declares only that rect as the projection view (the
  letterbox bars no longer squash the 3D vertically) and as the theatre image.
- BUG-49 intro videos: CreateProcess(GoTVRHost) took 27-35 s at game start in every log and blocked enabling the hooks;
  now it runs on its own thread (log shows how long it took).
- Self-test: 50/50; UI + UI layer tests PASS (plus: a non-UI bind with nothing awaiting does not touch the list).

## GoTVR 0.14.1 → 0.14.2 — screen mostly black and flickering, HUD jumping in size (BUG-50)
- `[LIVE]` 0.14.1: frame rate restored (87/58 pairs-ticks, 156 in flat), host startup 26.6 s (antivirus, confirmed by
  the owner), UI layer active - but the engine's UI RTV "locked in" dozens of times (…aded0, …ad6d0, …adfd0, …ae5d0 …):
  the engine creates a NEW render-target view for every bind (transient descriptor ring). The RTV-substitution design
  therefore swapped other targets' views (scene → our UI texture, cleared → black frames) and missed the real UI
  binds (UI drawn full-screen into the engine image → the HUD alternating between full-screen and the quad).
- Fix (simpler, no descriptor games): the engine draws its UI unmodified into its own texture; at pass 65's
  pre-callback (after UI passes 60/61/64 in GPU order, before the full-screen fade) the texture - if bound to that
  context, i.e. in RENDER_TARGET per the engine's tracker - is transitioned with the engine's own Res_Transition/flush,
  copied into the next shared UI image, transitioned back and cleared through our own RTV, so the composite adds
  nothing while the fade still covers the frame. OMSetRenderTargets is no longer hooked.
- Self-test: 50/50; UI layer test (full size, copy + round robin, untouched when not bound, cinema/title/kill switch)
  PASS; all earlier tests PASS.

## GoTVR 0.14.2 → 0.14.3 — "the whole game on the UI quad, and it still follows the head" (BUG-51, BUG-52)
- `[HEADSET]` 0.14.2: flicker gone; the quad showed the whole game, not just the HUD; quad head-locked.
- BUG-51 `[LIVE]` analysis: the texture the UI passes draw into (0x143377510's colour, 2560x1440) is the game's FINAL
  picture: the 3D scene is already in it (upscaler output, pass 59) and the UI is drawn on top, then the frame is
  letterboxed into the 2560x1600 window. The 0.13.0 probe only proved "not the back buffer". So the UI cannot be
  separated by copying - the layer is OFF (forced); the HUD is drawn into the 3D image on the mapped panel again (0.13
  behaviour) with the 0.14 fixes kept: full-frame fades (pass 65), full-frame cinematic letterbox, letterbox crop,
  background host start, no VirtualQuery in hot paths. A UI-only seam (redirecting passes 60/61/64 into a cleared
  transparent target, with HUD-004 alpha handling) is future work.
- BUG-52 start screen detection `[LIVE]`: clock reason 27 = 0 marks "leaving the world / about to load" (0.5 s before
  each load), NOT the start screen (which ran with reason 27 = 1). Detection dropped (title_reason default -1).
  Instead (HUD-002 "lazily ease a recenter after sustained large deviation"): the HUD/menu panel is a rectangle fixed in
  the room (LOCAL space, gravity-aligned, yaw-locked, following head position), projected per eye into the mapped rect
  (billboarded; float viewport so it slides off the edge instead of squashing). After looking more than 35° away for
  0.5 s it eases back in front at 90°/s; F11 or a new context re-centres at once. hud_follow=0: old head-locked panel.
- Self-test: 50/50; UI test, world-locked geometry (30° yaw → 0.577 tangent, behind → hidden) PASS.

## GoTVR 0.14.3 → 0.14.4 — world things stretching with the HUD; start screen; loading screens (BUG-53..55)
- `[HEADSET]` 0.14.3: HUD world-locked OK; "when I look up and down the game world also stretches like the HUD";
  owner wants the main menu fixed (no turning to face the head, no following) and loading screens like pause/videos.
- BUG-53 `[STATIC]`: UI_WorldDraw 0x6E37D0 (renderer class with this draw at vtable slot 8; vtables 0x14112FBF0/
  0x14112FC58) projects each element with the view's view-projection (view+0x5C0) - world-anchored UI (markers, prompts,
  icons on things), already per-eye correct. Mapped into the panel it moved with the panel. Fix: hook Batch_Draw
  0x5681A0(renderer, range{ctx, view}) - for that class on a mapped context the full frame is restored around the call,
  then the panel mapping re-applied. A per-second census logs which other renderer draw functions run on the panel.
  F3 (xr=4) now toggles the letterbox crop for an A/B in case the crop contributes.
- BUG-54 start screen: the first world shown after boot is the title world; its camera position is remembered; a later
  world opening at that position (quit to title) is the title again; it ends at the next load. It is a flat context ->
  the theatre screen (fixed in the room, proper quad, no billboarding). menu_theater=0 disables.
- BUG-55 loading screens: world mode 1 or time-scale reason 9 = 0 → flat context (theatre). loading_theater=0 disables.
- Self-test: 52/52; world-anchored UI exemption, loading theatre, UI tests PASS.

## GoTVR 0.14.4 → 0.14.5 — "nothing changed": world stretch, start screen, loading screens (BUG-53b..56)
- `[LIVE]` 0.14.4 log: the world-anchored-UI exemption never fired (0 batches) - Batch_Draw calls vtable slot 8, and
  slot 8 of that class is 0x6E5780 (0x6E37D0 is slot 9). Renderers seen on the panel: 0x6E5780 (2/frame, also on the
  start screen), 0x5A7FE0 (1/frame), 0x7EFC20 (1/frame, gameplay only) - none is clearly world-anchored, so the
  exemption is left inactive (census kept). "Start screen" fired on a 0.5 s staging world (camera 2000,0,500), not the
  title (18 s at world mode 2 with 0x6E5780+0x5A7FE0 only). The post-load loading screen ran at world mode 2.
- BUG-56 (most likely cause of the stretch): the letterbox crop (0.14.1) assumed the game draws black bars around its
  16:9 picture in the 16:10 window. If it scales instead, cropping magnifies the world ~11% vertically - it swims when
  you look up/down. Now measured: 32x4 px readback boxes (middle of the top band / centre) on lit frames; crop only if
  the top band stays black (log: "letterbox check"). F3 still forces it off; F4 toggles the room-fixed HUD for A/B.
- BUG-54b start screen: fingerprint of the title world's fixed camera - (5907, 3490, 17158) ±300 with fov 0.789 rad,
  identical in the 0.14.0, 0.14.2 and 0.14.4 logs → theatre screen. World-mode changes now log camera + fov.
- BUG-55b loading screen at world mode 2: the game's own fade (0x14149AF80, alpha +0xC, drawn by pass 65) fully opaque
  for 150 ms → theatre; back to stereo when it starts fading in.
- Self-test: 52/52; loading test extended (black screen 150 ms → flat, fade-in → stereo) PASS; all UI tests PASS.
