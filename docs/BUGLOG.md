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

## GoTVR 0.14.5 → 0.15.0 — blurry HUD, fades and cinematic bars on the HUD panel (BUG-56b..58)
- `[HEADSET]` 0.14.5: loading screens and the start screen on the theatre screen are right. **F3 (crop off) fixes the
  world stretching** and the HUD stays put in the room. The HUD is blurry; fades to/from black and the cinematic black bars
  are drawn on the HUD rectangle instead of across the whole view. User saved 0.14.5 as `gotvr-0.14.5` (WIP) on GitHub.
- BUG-56b `[LIVE]`: the "letterbox check" said "bars" because it measured video frames (pre-rendered videos have their own
  bars). Gameplay frames fill the window: the engine's 2560x1440 picture is scaled to 2560x1600, so the eye frustum spans
  the whole window image. letterbox_crop now defaults to 0 (F3 still toggles).
- BUG-57 `[STATIC]`: the "Letterbox" display mode (ADDR-117) is a SETTINGS toggle (setting callback 0x140D160C0 → push/pop
  0x996740), not a cinematic signal - so 0.14's "cinema" mode never triggered. The cinematic bars are a UI widget
  (`CUiLetterbox`, script `PushLetterbox`/`PopLetterbox`, tunables TRIGGER/UNTRIGGER_LETTERBOX_SPEED) drawn in the UI
  passes; `HUDScaling` (0x6E3210) is the ultrawide HUD-width setting. Fix: measured on the UI-only image (BUG-58): rows at
  3 % and 97 % of the height opaque black across the width (bars), or all three sampled rows covered and dark (a UI fade)
  → GV_UI_FULL: the UI image covers each eye's whole frustum (one quad per eye on the frustum its image used), so bars sit
  at the top/bottom of the view and fades cover everything. Enter after 2 hits, leave after 24 misses. While the game's
  own pass-65 fade (0x14149AF80 alpha) is above 0.5 the UI layer is hidden (on the monitor that fade covers the UI too).
- BUG-58 (HUD blur): the HUD was rasterised into the eye image (2560x1600 for a ~1:1 lens frustum; VDXR recommends
  2496x2688) - a 1440-row UI squeezed into ~450 rows of the panel, then resampled again by the compositor. Fix (playbook
  ch04 "Redirect a whole category of draws by rebinding the target once", HUD-004): UI passes 60-65 (ctx+0x2A50) that bind
  the UI target are armed in RT_Bind; right after the engine's `Ctx_FlushRT` 0x4FEA80 (one OMSetRenderTargets per flush,
  fresh RTV each time - why 0.14.1's RTV-identity lock-in failed, BUG-50) the same list gets OMSetRenderTargets(our shared
  image's RTV, the engine's own depth view ctx+0x33C*[0x141CDFBD8]+[0x141CDFB58]) when colour attachment 0 is the UI
  texture. Pass 59 (the 3D picture) is never redirected. One shared image per frame (chosen at RV_Post), cleared to
  transparent in pass 60's pre-callback, un-redirected at pass 65 (dirty bit → the engine re-binds its own target for the
  fade), published to the host → a quad at 2560x1440, sampled once by the runtime. The 3D picture is now HUD-less. Arms
  are cleared every frame (pooled contexts). Kill switch: hud_layer=0 or F1 (the 0.14 panel mapping).
  Known limit (HUD-004 trip hazard): the engine's blend squares the alpha of translucent UI; the log's UICINE line prints
  the alpha histogram of the middle row so this can be measured before a blend fix.
- Self-test: 54/54 (Ctx_FlushRT + its depth-handle code); redirect test (arm / clear / re-bind + depth / clean flush /
  other texture / pass 59 / pass 65 / flags / F1) and cinematic detector test (bars / empty / fade / partial / hysteresis)
  PASS; host test OK.

## GoTVR 0.15.0 → 0.15.1 — "the HUD is always too large" (BUG-58b)
- `[HEADSET]` 0.15.0: the in-game HUD was always huge (not only during bars/fades).
- `[LIVE]` 0.15.0 log: mode HUD on the UI layer, UI-pass binds armed ~290/s, clears and publishes every frame, but
  **redirected flushes = 0**; the published UI image is fully transparent (mid-row alpha: all zero). So the UI was never
  redirected: with the layer on the panel mapping is off, and the UI was drawn full-size into the 3D picture.
- Cause `[STATIC]`: RT_Bind 0x7E34E0 calls Ctx_FlushRT ITSELF at its end (eager flush, then drops stale slots). 0.15.0
  armed the context after RT_Bind returned, so the one flush that binds the UI target was missed; later draws found the
  context clean. Fix: arm before calling the engine's RT_Bind (the pass index is known from ctx+0x2A50 beforehand); the
  re-bind no longer requires attachment slot 1 to be empty (stale slots are removed after the flush; we bind 1 RTV).
- Fail-closed safety net: if UI binds are armed but none is redirected for 30 frames, the layer refuses itself and the
  HUD goes back to the 0.14 panel (log line; F1 retries). Self-test reproduces the bind-then-flush order: PASS 54/54.

## GoTVR 0.15.1 → 0.15.2 — fades not over the whole view; thin bars full but the HUD panel stays; thick bars undetected (BUG-57b)
- `[HEADSET]` 0.15.1: the HUD is stationary and sharp (redirect works: ~130 redirected flushes/s, the HUD image has
  zero/partial/full alpha populations). Fades to/from black do not cover the whole view; THIN black bars do cover the whole
  view but the small HUD panel stays; THICK bars are not detected (no full-view HUD).
- `[LIVE]` 0.15.1 log: the detector never fired (hits = 0 all session; one sample bottom 24/24 + middle covered 24/24).
  Causes: (1) one row at 3 %/97 % with "a >= 230, rgb <= 10" - the UI's squared alpha (HUD-004) and translucent fades
  never pass; (2) the thin bars are not UI: they are in the 3D picture (the HUD image never contains them).
- Fix: 13 rows edge to edge (0.5 %…14 %, 50 %, 86 %…99.5 %), 32 samples each, measured as BANDS. HUD image: bar rows
  (a >= 150, rgb <= 20) counted from each edge inwards, both edges -> bars; all 13 rows covered by a dark veil (a >= 20,
  rgb <= a/2) -> a fade. 3D picture (published left-eye image, same rows): symmetric black bands (rgb <= 4) at both edges
  with the picture between -> bars. Either -> GV_UI_FULL (the HUD over the whole view, no small panel). The game's own
  pass-65 fade now hides the HUD layer above alpha 0.3 (was 0.5). Log: UICINE/s prints the max band rows per second.
  Separating the bars from the rest of the HUD was not done: the full-view HUD keeps subtitles where the game puts them.
- Self-test 54/54; band detector test (HUD: none/thick/squared/veil/partial; 3D: thin/sky/black; hysteresis) PASS.

## GoTVR 0.15.2 → 0.15.3 — thick bars flicker back to small; cutscene detection; jittering interact prompt (BUG-57c, BUG-59)
- `[HEADSET]` 0.15.2: thin bars → full-view HUD worked; thick (32:9) bars worked momentarily, then back to the small panel.
  Interact prompts ("E: mount horse") jitter back and forth on the still HUD panel. Owner asks for a cutscene detector
  instead of fade detection.
- `[LIVE]` 0.15.2 log: every in-engine cutscene showed bars only in the 3D picture (bar rows 5/5 of 6; the HUD image never
  had any). The latch toggled every 0.5-2 s: dark shots/cuts break "symmetric bars with picture between" for 20 frames.
  Census for a game-state signal: the cutscene camera is written by the same gameplay camera system (SetLocalMatrix caller
  0x673FC1 in gameplay AND cutscenes; 0xA38841 = title camera) -> not a discriminator. Strings: an input-owner enum
  (Zero, Debug, Skill, Gadget, GadgetExtra, Hud, HeroScript, Camera, POIVol, Hwacha, SneakAttack, SkillPeek, GadgetPeek,
  WhistleRadialMenu, RadialMenu, HandsOff, Movie, CutScene, ManualCamera, TestRigging, PhotoMode, PauseMenu, Menu, ...) at
  name table 0x1417B5300 - its storage is not located yet (`[OPEN]`).
- BUG-57c fix: the 3D-picture bar detector is now a CUTSCENE latch (playbook ch13: cinematic classifier with hysteresis):
  enter on 3 strict hits; while in a cutscene, any frame with both edges black holds it (dark shots, cuts to black); leave
  after 90 frames (~1.5 s) without. CAMCENSUS/s logs camera fov, rect +0x6A4 and cut flag each second (to find a state flag).
- BUG-59: world-anchored prompts are placed with the eye view of the frame that drew them; xr=4 alternates eyes per frame,
  so consecutive HUD images differed by the eye offset and the host showed them alternately -> jitter. Only left-eye frames
  publish the HUD image now (60 Hz HUD updates, consistent positions).
- Self-test 54/54; cutscene latch (enter/hold through black/leave after 90) and right-eye skip PASS.

## GoTVR 0.15.3 → 0.15.4 — thick-bar cutscenes still drop to the small HUD; black headset during the launcher (BUG-57d, BUG-60)
- `[HEADSET]` 0.15.3: with the thick 32:9 bars the HUD stays large longer but still shrinks back mid-scene. Jitter fixed,
  HUD sharp. While the PlayStation launcher is up the headset is black. (No cutscenes in this session's log.)
- BUG-57d `[LIVE]` (0.15.2 log, inference): during letterboxed scenes the strict bar test passed on only 4-63 of ~60 reads/s
  -> the bars are not pure black (grain/dither around 4/255). Thresholds: enter <= 12/255 (rows need 28 of 32 samples),
  hold <= 24/255, picture between >= 32/255; leave after 180 frames (~3 s) without bars. CAMCENSUS/s logs the brightness
  (avg/max) of the outermost 3D-picture rows, so the bar colour is measured next time.
- BUG-60 `[LIVE]`: the host (OpenXR session) started with the game process; the game's first frame came 8 s later (more
  while the launcher waits) - black headset meanwhile. host_start=1 (default): the session starts at the game's first
  presented frame, so Virtual Desktop / SteamVR keeps showing its own home (desktop, launcher) until then. host_start=0:
  old behaviour.
- Self-test 54/54; latch test extended (grainy 14/255 bars held; leave after 180) PASS.

## GoTVR 0.15.4 → 0.15.5 — thick-bar cutscenes still shrink; recentre at start; large HUD after pause (BUG-57e, BUG-61)
- `[HEADSET]` 0.15.4: launcher fixed (visible in Virtual Desktop until the game's first frame). Thick 32:9-bar cutscenes:
  large HUD for longer but still shrinks; never with the thin bars. Requests: recentre once VR starts; large HUD for 3 s
  after closing the pause menu.
- `[LIVE]` 0.15.4 CAMCENSUS: during the thick-bar shots the 3D picture's outermost rows are BRIGHT (avg ~127) and the bar
  detector sees bars only ~0.5 s per shot (5/5 rows) -> the latch left after 3 s each time. The camera keeps the
  cutscene's field of view for the whole sequence: fov 1.028 rad from ~4 s before the first bars until the end, gameplay
  0.935 (0.934 the moment it ends). The thin-bar shots set camera rect +0x6A4 to (-1, -0.758, 1, 0.758) = the engine's own
  2.35:1 letterbox (outermost rows 0/255 then). Where the thick bars are drawn is still `[OPEN]` (not in the sampled rows of
  the 3D picture or the HUD image most of the time).
- BUG-57e fix (game thread, uiDecide): any evidence (3D-picture bars, HUD-image bars/fade, or rect top < 0.99) STARTS a
  cutscene; it HOLDS while the camera keeps the fov it had at the start; ENDS when the fov changes with no evidence for
  0.5 s, after 15 s without evidence, or on a flat context. Pausing-menu close -> GV_UI_FULL for 3 s.
- BUG-61: recentre automatically 1.5 s after the headset session starts (host state RUNNING) and at the first 3D gameplay
  after start (F11 as before).
- Self-test 54/54 PASS.

## GoTVR 0.15.5 → 0.15.6 — pause hold 1 s
- `[HEADSET]` 0.15.5: the large HUD now lasts whole cutscenes (thin and thick bars). Owner: after the pause menu the HUD
  should go back to small after 1 s (was 3 s). Changed; self-test 54/54 PASS.

## GoTVR 0.15.6 → 0.16.0 — headset render size (square), independent of the desktop window (BUG-62)
- Owner request (design doc): square render aspect and a resolution scaler for the headset; should the desktop window be
  720x720 to save resources?
- `[STATIC]` RE (ADDR-128..131): in windowed mode the game renders at its window client size from settings +0x10/+0x14,
  clamped by "Forced aspect ratio" +0x28 (0 auto, 3 = 16:9 -> the 2560x1440 picture in the 2560x1600 window).
  SC_ComputeSizes 0x46C840(SC, settings) builds client/back-buffer (SC+0x1C0), viewport (+0x1E8, aspect +0x208), output
  (+0x160) and render (+0x190) records; a change makes the engine recreate every target (RTPool_OnResize 0x4DF360) and
  resize the swapchain at the client record. PostSCCmd 0x4277D0(NxApp = **0x141998300, 2) runs the apply synchronously on
  the swapchain thread (the settings menu path). No render-scale setting exists (only dynamic resolution / upscaler).
- Fix: hook SC_ComputeSizes; while windowed and render_size > 0 the settings it reads say W x W and aspect auto for the
  duration of the call (restored right after: nothing is saved), so the engine itself builds a consistent square frame,
  back buffer included; the desktop window keeps its size (DXGI scales the mirror). F2 cycles 1536/1792/2048/2304/2560/
  game's own and applies through PostSCCmd on the game thread. Fail-closed: > 30 recomputes in 2 s -> off. Videos inside
  the square image are cropped to their 16:9 band on the theatre screen. Detector readback rings are rebuilt on resize.
- 720x720 window: not needed - with the override the headset size no longer depends on the window; a smaller window only
  saves the desktop's scaled copy (negligible), so it is left to the user.
- Self-test 56/56; render size hook test PASS.

## GoTVR 0.16.0 → 0.16.1 — F2 froze the game (BUG-63)
- `[HEADSET]` 0.16.0: pressing F2 turned the headset black and froze the game (sound kept playing).
- `[LIVE]` log: the game is in FULLSCREEN mode (display settings byte0 = 1), so the square override never applied ("the game's
  own size", output 2560x1440, back buffer 2560x1600). On F2 the game thread stopped for good (tick=0 from then on; host:
  gameFresh=0) and "applied ..." was never logged: PostSCCmd(NxApp, 2) blocks until the swapchain thread finishes, which
  needs the frame the game thread was in the middle of -> called from the GameTick hook it deadlocked.
- `[STATIC]` the game applies display changes from its window-message handler (Alt+Enter, 0x445970 -> vt+0x118/vt+0x120;
  vt+0x120 returns early when the settings are unchanged - every caller passes "only if changed"); SC::Apply 0x46BB20 always
  runs SC_ComputeSizes before deciding whether to rebuild.
- Fix: F2 posts a private message to the game window (subclassed on first use); its handler calls PostSCCmd(app, 2) in
  the same context as Alt+Enter, never from inside a frame. In fullscreen F2 only logs that Windowed mode is needed.
- Self-test 56/56 PASS.

## GoTVR 0.16.1 → 0.16.2 — F2 crashed the game (BUG-64b); UI, menus and pause screen square (BUG-64)
- `[HEADSET]` 0.16.1 (windowed): F2 applied 2304x2304 ("applied ... on the window thread (547 ms, result 1)"), then the game
  stopped (tick=0, present=0) and crashed. Also: with the square render the HUD, the menus and the pause screen became squares,
  stretched in height.
- BUG-64b `[STATIC]` the engine resizes the swapchain with ResizeBuffers (SC_Resize 0x435510 -> vt+0x68). The mod's back-buffer
  map (bbmap::nat / obj, the native back buffers and the engine's objects for them) and the UI-texture probe were raw pointers
  cached at startup and never refreshed: the first left-eye capture after the resize called GetDesc on a freed back buffer.
  The crash dump could not be uploaded (148 MB, "upload failed" three times); this is the one use of freed memory that the
  first frame after a resize always reaches.
- Fix: the native ResizeBuffers / ResizeBuffers1 are hooked; under the publisher lock the back-buffer map, the UI probe and a
  pending copy are dropped BEFORE the swapchain frees the buffers. The map is rescanned 4x/s for 5 s after a resize (the engine
  builds its objects for the new buffers right after). The cinematic-detector ring is made once with room for 4096-pixel rows,
  so a new size only changes the row size used in it, and every UI row copy checks that the image and the ring agree (no copy
  box outside an image while the present thread rebuilds the UI images).
- BUG-64 `[STATIC]` the UI canvas comes from the viewport aspect SC+0x208: SetRenderDims 0x4DBB60 (vt+0xA8 of the swapchain
  object = SC+0x1E8, +0x20 = aspect) -> 0x141A0AB24/28 = 3840 x 2160 at 16:9, 3840 x 3840 at 1:1 (read all over the UI
  module 0x6E....-0x6F....). The flat-frame camera takes the same value (RTPool_OnResize 0x4DF3D1 -> Cam_SetAspect 0x806F00:
  cam+0x67C = aspect, tanH = aspect x tanV; fov +0x670 is vertical).
- Fix (anamorphic): the image stays square, but SC_ComputeSizes (hooked) leaves aspect 16:9 (ui_aspect) in SC+0x208 -> the
  UI is laid out on a 3840x2160 canvas and flat frames use a 16:9 projection, both drawn into the square image (stretched
  vertically by 16:9). GvShared v10 dispAspect: the host shows the UI image (head-locked / room-fixed) and flat theatre frames
  at 16:9; videos fill the image and are no longer cropped. Stereo frames are unaffected (the eye projection is written whole:
  P00/P11 from the eye tangents, half-tangents widened). The full-view cinematic UI still covers each eye's whole view.
- Self-test PASS (anamorphic aspect: windowed override, fullscreen / ui_aspect=0 / size 0 untouched, panel 16:9 in the square
  eye; resize: map + probe dropped before ResizeBuffers, other swapchains untouched).

## GoTVR 0.16.2 → 0.16.3 — F2 still crashed (BUG-64c); pre-rendered cutscenes squashed (BUG-64d)
- `[HEADSET]` 0.16.2: menus and the pause screen at 16:9 again (BUG-64 fixed), but videos looked squashed, and F2 still crashed.
- BUG-64c `[DUMP]` GhostOfTsushima.exe.66416.dmp (uploaded by hand): AV reading null+0x10 at exe+0x437B11 on the TICK thread,
  stack hGameTick -> GameTick 0x4C49E0 -> 0x577820 -> ... -> Comp_Draw 0x4FE620 (object 0x141CE0450) -> NxD3D vt+0x138
  0x43CC60 -> 0x437AF0: back-buffer entry 0 of the D3D12 driver (NxD3D+0x168 -> +0x30, 0x40-byte entries: +0 back buffer,
  +0x10 its descriptor range) was empty. The swapchain thread was idle (0x42470E), the window thread in GetMessage.
  The 0.16.2 ResizeBuffers hook worked ("swapchain ResizeBuffers 2304x2304 -> 0"); the crash is the game's own.
- `[STATIC]` cause: 0.16.1/0.16.2 applied from the WINDOW thread, so the main thread went on ticking while the swapchain thread
  rebuilt the back buffers. The game applies display changes on its MAIN thread: the window thread only queues messages
  (NxWnd_Queue 0x444F80); GameWindow_Pump 0x446530 (GameWindow vt+0x48, the first call of NxApp::Update 0x4257B0, early in
  every tick) hands them to 0x445970, whose WM_SIZE / Alt+Enter cases call NxApp vt+0x120 -> ApplyDisplay 0x421ED0 ->
  PostSCCmd(2): the main thread is blocked for the whole rebuild.
- Fix: F2 only sets a request; a hook on GameWindow_Pump applies it right after the game's own pump, on the main thread, through
  ApplyDisplay with the current settings (the game's flags and window/Reflex follow-ups included). During the apply the
  camera slot hook passes through (RTPool_OnResize calls it from the swapchain thread via Cam_SetAspect).
- BUG-64d `[STATIC]` Movie_Draw 0x7EAF80 letterboxes from the RENDER aspect SC+0x1A8 (swapchain vt+0xB0 = SC+0x188, +0x20),
  not SC+0x208: the video was a 16:9 band in the square image, shown at 16:9 by the host = squashed. Fix: during Movie_Draw
  the render aspect is the layout aspect too, so the video fills the anamorphic image like the UI and its subtitles.
- Self-test PASS (58 signatures; main-thread size change: after the pump, ApplyDisplay with the current settings, once, never
  in fullscreen).

## GoTVR 0.16.3 → 0.16.4 — F2: "fatal error making a swap chain" (BUG-64e)
- `[HEADSET]` 0.16.3: videos fixed (BUG-64d); F2 ended with the game's fatal error "creating a swap chain".
- `[DUMP]` GhostOfTsushima.exe.56848: main thread in ApplyDisplay 0x422077 -> PostSCCmd 0x42782E (as designed, BUG-64c fix
  works); the swapchain thread crashed at exe+0x4359C5 (NxDXGI vt+0x40 0x435990: `mov rax,[rcx]` with the swapchain pointer
  this+0x30 = NULL) after the game took the RE-CREATE path (SC_Recreate 0x46E3D0: destroy vt+0x28, create vt+0x18) and the
  create failed. Log: no ResizeBuffers this time.
- Cause: the mod's publisher held a COM reference to the game's swapchain (QueryInterface for IDXGISwapChain3 at init, never
  released). DXGI allows one swapchain per window, so the old one stayed alive and the new one could not be created.
- Fix: the reference is released right after the QueryInterface (the mod never owns the swapchain); SC_Recreate is hooked and
  drops every pointer to the old swapchain and its buffers before the game destroys it; the publisher starts again on the
  new swapchain at the next Present (new eye-texture generation, the host follows). Fallback: if the engine presents on a
  different swapchain than the publisher's, the publisher starts again too.
- Self-test PASS (59 signatures; re-creation: swapchain + buffers dropped before the game's destroy).

## GoTVR 0.16.4 → 0.16.5 — default headset size 2560
- `[HEADSET]` 0.16.4: F2 works (resize and re-creation, no crash); the user preferred the highest size, 2560 x 2560.
- Change: render_size default 2048 -> 2560 (the ini key still overrides it; F2 cycle unchanged). Self-test PASS.

## GoTVR 0.16.5 → 0.17.0 — first person (FP-1)
- Request: first person, head hidden, as the first step towards motion controls and full-body IK (design doc). Playbook
  check: camera ownership first (PORT-06), input before visible hands (PORT-10 -> PORT-11); ch01 "don't double-count height",
  ch02/ch12 "hang the arms off the body", 18 "third-person games: default to first person, keep third person".
- References read (methods only): bldbrne-vr anchors the eye on the physics root + eye height + forward offset (root moves
  smoothly, animations never bob the view) and hides the head per draw in colour passes only; BetterVR anchors on the actor
  position + height with the game camera's yaw; FRIK hides the head geometry or moves the head bone back/up.
- `[STATIC]` hero = *(0x1434C6F20) (627 readers); scene node (class id vt[4] in [10,220)): world +0x150, joint world matrices
  *(+0x278) + (j+1)*0x40, character top/bottom joints +0x7FC/+0x7F8 (UI screen bounds 0x6E9433 / 0x6EFB90).
- 0.17.0: F5 first person (default on), Shift+F5 hero skeleton into the log. Eye = hero root + low-passed head-joint height
  (0.35 s) + slowly followed lean (0.3 s) + offsets (8 cm up, 10 cm forward); the HMD pose on top; the game camera keeps yaw.
  Kept third person: flat/theatre frames, cutscenes (M_CINEMA), the start screen, no valid hero, game camera > 8 m from the
  head (scripted cameras). Head hiding v1: joints within 22 cm of the head joint in the first pose are collapsed onto it at
  every render-camera update (no head shadow; a joint that strays > 35 cm is dropped from the set). FP/s census line.
- Self-test PASS (first person: anchor, head set, stray hand dropped, cutscene / far camera keep the game camera).

## GoTVR 0.17.0 → 0.17.1 — first person at hip level (BUG-65); judder with black edges when turning (BUG-66)
- `[HEADSET]` 0.17.0: the view faced forward but sat inside Jin's chest at hip level; turning the head juddered with black
  screen edges in the headset (not on the monitor, no FPS drop); 2560 felt too high as the default.
- BUG-65 `[LIVE]` log: *(0x1434C6F20) is a class-15 hero ENTITY whose +0x278 array has 8 entries: three at the root, three at
  98..105 cm, one 17 cm behind at 94 cm, the highest at 116 cm - sockets, not the character skeleton (+0x7F8/+0x7FC = -1).
  The eye was placed at that 116 cm socket, and 0.17.0 collapsed sockets 3/4/5/7 (no visible effect; no longer touched).
- Fix: the hero object (+0x0..+0x4100) and the game objects it points to (one level down) are searched once for the skinned
  model: a scene node whose root is within 60 cm of the hero's, the most joints (>= 20), and a highest joint near the body axis
  1.0..2.3 m up (a standing human; rules out the horse and held weapons). The head is that highest joint (the top of the
  head): eyes 11 cm below it and 9 cm forward (fp_eye_up_cm -11, fp_eye_forward_cm 9). A model whose joint buffer pointer
  changes keeps its indices (multi-buffered arrays are counted, not re-searched).
- BUG-66 `[LIVE]` the host's own frame rate (GoTVRHost xrWaitFrame cadence, from "frames=" in the census) fell from 72 to
  27..60 Hz whenever 3D gameplay ran at 2560 (both 0.17.0 sessions; 72 in menus): the game kept the GPU saturated (60 stereo
  pairs/s), the host's swapchain copies queued behind it, xrEndFrame was late and the runtime halved its rate - the compositor
  then reprojected old frames (judder, black edges), which the monitor never shows.
- Fix: default render_size 2304 (user request), and the host's copy queue is HIGH priority (GoTVRHost 0.13.1).
- Self-test PASS.

## GoTVR 0.17.1 → 0.17.2 — no first person at all (BUG-65b); left-eye pop-in on fast head turns (BUG-67)
- `[HEADSET]` 0.17.1: always third person, F5 had no visible effect; when moving the head fast, objects popped in and out
  in the LEFT eye only (this was the "glitching" of the 0.17.0 report).
- BUG-65b `[LIVE]` log: every model search ended "no skinned model ... (best 8 joints)" (278..340 objects probed), so first
  person never engaged (F5 toggled a mode that was refused each frame). The 8 entries were the strict counter stopping at the
  first ZERO-scale joint: the game keeps gear it does not show as zero-scale joints inside the skeleton. Fix: such joints
  count (finite, rows < 50 incl. zero, within 10 m; the end = last good entry before 8 bad ones); the head pick and the head
  set use only shown joints. The hero's own array is then expected to be the skeleton (the model search still runs).
- BUG-67 `[STATIC]`+`[LIVE]` reasoning: occ::fastCheck runs at Cam_TemporalUpdate, before this tick's head pose is composed.
  On the simulated (left) tick the game has just rewritten its camera, so a head-only turn was invisible to the detector there
  and the left eye kept using its 2-tick-old occlusion history through fast turns (objects culled at the leading edge); the
  frozen (right) tick still carries the composed head and was detected. Fix: the HMD rotation since this eye's previous
  check (pose packet) is measured too; the larger angle decides (OCC/s logs both).
- Self-test PASS (first person with a zero-scale joint inside the skeleton).

## GoTVR 0.17.2 → 0.17.3 — head still visible (BUG-68); left-eye foliage pop-in (BUG-67b); black edges / stutter on fast pitch (BUG-69)
- `[HEADSET]` 0.17.2: first person at eye level and facing forward, but the head is still there; the left eye still pops
  objects in and out, worst when looking side to side somewhat fast at grass / flowers / foliage (right eye fine); looking
  down then quickly up stutters and shows the black screen edge although the game's frame rate holds.
- BUG-68 `[LIVE]`+`[STATIC]`: (a) the 0.17.2 head set was capped at 96 joints, filled by low-index face joints (14..109), and
  never contained the head joint 123 itself (the skeleton dump: 487 of 500 counted joints are within 30 cm of the head), and
  the count stopped at the 500 cap (the node's own count is node+0x280 - 1, ADDR-143). (b) Decisive: the collapse edited the
  joint WORLD array (+0x278), which the GPU never reads. RE: Anim_Setup 0x9BB4F0 queues Anim_Job 0xB2DBC0 with the node at
  job+0xE8 and the node's current RENDER POSE (node+0x508[node+0x2DA], model space) at job+0xA8; the skin palette job
  0x59EB30 builds the GPU palette from that render pose x inverse bind (ADDR-140..142). Every write to the world array was
  overwritten by the next animation and was invisible in between.
- Fix: Anim_Job is hooked; after the hero's job writes its render pose (before the palette job, which waits for the job's ready
  flag) the head set's rows are zeroed - the engine's own hidden-gear state - and moved onto the neck joint (the highest shown
  joint below the head, outside the set). The set is every shown joint within fp_hide_radius_cm (22) of the top-of-head joint
  in the first pose (cap 2048, joints up to 4096); a joint that later strays more than 45 cm from the neck (a hand that was by
  the face) is drawn and leaves the set. The world array is left alone (game logic, attachments and the eye anchor read it).
  Nothing is hidden when first person is off or blocked (cutscenes, menus, far cameras: the head is back within 2 frames). A
  changed joint buffer or count (outfit/gear change) recalibrates the set. FP/s: hero animation jobs, jobs with the head hidden,
  joints hidden per job.
- BUG-67b `[STATIC]`: besides RV_OcclusionSetup (banked per eye since 0.9.4), six GPU culling passes - including the grass /
  foliage culling 0x62A530 and the tile culling compute 0x79ADA0 - test against the PREVIOUS frame's HiZ (table B,
  ADDR-144/145). Under synchronized-sequential stereo the right eye's previous frame is the left eye of the same pair (same
  head pose), the left eye's is the right eye of the previous pair - a whole pair of head motion earlier (playbook ch14
  #cpu-visibility-history: "eye zero reads the previous pair's eye-one set"). The 2.0 deg/pair limit (120 deg/s) let moderate
  turns through. Fix: the left eye has its own limit fast_turn_deg_left = 0.6 deg/pair (36 deg/s): above it the left frame
  declares the history invalid (the engine's cut path: those passes test nothing and draw everything for that frame). The
  right eye keeps 2.0. OCC/s logs fast frames per eye.
- BUG-69 `[LIVE]`: the 0.17.2 census shows host frames/s dropping to 44..60 together with the stereo pairs (GPU-bound
  moments) - the headset streamer and GoTVRHost wait behind the game on the GPU, and every pair reaches the display at least a
  pair after its pose was predicted, so fast head motion warps the image past its 3 deg margin (black edge). Fixes: (1) pose
  prediction lead: the host measures, for each new pair, how late it reaches the display relative to the time its pose was
  predicted for, and predicts later by that amount (pose_lead_ms = auto, 0..45 ms; the declared pose stays the rendered pose);
  (2) WDDM GPU scheduling: GoTVRHost HIGH (else ABOVE_NORMAL), the game BELOW_NORMAL (game_gpu_priority_below_normal = 1) so
  the streamer's encoder and the host's copies go first. Host log: lead and measured lag every 900 frames; game POSE/s line.
  Shared block v11.
- Self-test PASS (first person: render-pose hiding after the hero's job, onto the neck, world array untouched, other characters
  untouched, stray hand drawn and dropped, head back when FP stops; fast-turn per-eye limits). 65/65 signatures.

## GoTVR 0.17.3 → 0.17.4 — left-eye pop-in and left-eye stutter (BUG-70)
- `[HEADSET]` 0.17.3: head gone, view correct. Pop-in still in the LEFT eye only; the stutter on fast look down/up is also
  left-eye only.
- `[LIVE]` 0.17.3 log: the left-eye history cut fired on ~50 of ~60 left frames per second during turns (OCC/s "left 55,
  right 42") and the pop-in stayed, so the culling those frames used was not set up by their own tick. Pose lead converged
  (lag -1..+1 ms, lead ~35 ms), so the remaining stutter is not prediction.
- Cause `[STATIC]`+`[LIVE]`: the engine presents on its own render thread (census tid tick 54860 / present 70992), so frame N
  is recorded while the game thread already runs tick N+1. Render passes read game-thread globals at record time: the
  occlusion view 0x141EDBD70 (through Occ_View 0x9E5960: grass/foliage/tile culling, ADDR-144), CAM-A, the eye id. Under xr=4
  the tick after the LEFT frame is the short frozen right tick, which re-composes CAM-A for the right eye and rewrites the
  occlusion view while the left frame is still being recorded; the right frame is recorded during the long simulated tick,
  before those globals change. Hence left-only. Second, the first-person anchor was computed at the first camera update of a
  tick - on the simulated tick that is the camera controller (SetLocal -> slot 43), before the hero's animation - while the
  frozen tick used the animated hero: two body poses inside one stereo pair.
- Fix: (1) render sync (playbook STR-012): the right-eye tick starts only after the left frame's Present (event from the
  render thread; 40 ms timeout; 3 timeouts in a row pause it for 300 ticks; render_sync = 1, 2 = every stereo tick, 0 = off);
  (2) the left tick recomputes the anchor at every camera update with the filters advanced once per tick (idempotent), and the
  right tick reuses the left tick's final anchor. POSE/s logs sync waits / cost / timeouts and how far apart the two anchors
  would have been.
- Self-test PASS (render sync: wait, no-wait, timeout, pause; first person: recompute within the left tick, right-tick reuse).

## GoTVR 0.17.4 → 0.18.0 — left-eye pop-in (BUG-71, third attempt); VR controllers (FEAT-01)
- `[HEADSET]` 0.17.4: left-eye pop-in unchanged. Owner: no performance-costing measure that does not fix it.
- `[LIVE]` 0.17.4: render sync waited ~0.1 ms per pair (the left frame was nearly always already presented: there was
  almost no overlap to remove) and the two eye anchors were never apart (0.00 cm) -> neither 0.17.4 hypothesis was the
  cause. Reverted to defaults off: render_sync = 0, fast_turn_deg_left = 2.0 (the 0.17.3 left-eye history cut made the
  left frames draw unculled during turns - a cost without a fix). The one-body-pose-per-pair anchor stays (free).
- BUG-71 `[STATIC]`: RV_OcclusionSetup's object IS the occlusion view 0x141EDBD70 (frame 0x141EBD2C0 + 0x1EAB0, ADDR-146)
  that the grass / foliage / terrain-tile culling passes read through Occ_View 0x9E5960: HiZ record +0xF0 (table B, slot
  1-parity = previous frame = the other eye) reprojected with +0xA0 / +0xB0..+0xEC. Since 0.9.4 hOcc overwrites those with
  THIS eye's camera of two ticks ago (correct for RV's own table-A source it swaps, wrong for table B): the foliage passes
  reprojected the other eye's depth with this eye's old camera - a mismatch that first person (grass at arm's length) makes
  large. Fix: Occ_View is hooked and the foliage passes get the ENGINE's own view of the tick (saved right after RV, before
  hOcc rewrites it): previous-frame depth with the previous-frame camera, as the engine designed. Zero cost.
  foliage_occlusion = 1 (default) / 0 (0.17.4) / 2 (no view, diagnostic); Shift+F8 cycles in game; OCCVIEW/s logs it.
  `[OPEN]` not yet confirmed in the headset.
- FEAT-01 VR controllers (playbook ch03, INPUT-001, XR-002): GoTVRHost 0.14.0 binds one CANONICAL controller per hand
  (stick, stick click, trigger, grip, lower/upper face button, menu, thumb rest, grip and aim poses, vibration) for Touch,
  Touch Plus, Touch Pro, Index, Vive, WMR, HP Reverb, Pico 4 and KHR simple (xrinput.h: one table entry per controller;
  optional profile extensions enabled when offered), syncs while focused, logs the profile the runtime chose, publishes
  neutral on focus loss (shared block v12). The game DLL (vrinput.h) gives them meaning through the game's own funnel - a
  virtual Xbox pad on user 0 (the mod is xinput1_4.dll), merged with a real pad, connected for the process lifetime after
  the first VR activation, neutral after 250 ms without a fresh sample. [controls] in gotvr.ini maps every control to pad
  outputs. Gameplay extras: snap turn closed on the game camera's own yaw (the stick drives the game's turn, the view is
  held and lands at once; or smooth), the item wheel on the left trigger (right stick picks Heal / Photo mode / Call
  horse, stick back to centre confirms, haptic ticks; no visuals yet), head gestures (right hand at the head + trigger =
  guiding wind = D-pad right + right stick up; left hand = show/hide the HUD), the game's rumble on the controllers
  (low motor left, high right). Menus/theatre/cutscenes: plain pad semantics.
- Self-test PASS (VR controllers: deadzone, mapping, snap loop and view hold, wheel pick/tap/close, gestures, menus,
  staleness, rumble). 67/67 signatures.

## GoTVR 0.18.0 → 0.18.1 — pop-in fixed (mode 2 default), seamless snap turn (BUG-73), wheel / gesture visuals, third person after menus (BUG-72)
- `[HEADSET]` 0.18.0: left-eye pop-in present; Shift+F8 once (foliage_occlusion 1 -> 2: the foliage / grass / tile culling
  passes get NO previous-frame occlusion view) removed it completely. Controls mostly worked; snap turn "turns a little, waits a
  second, turns more"; head gestures could not be confirmed; the left trigger showed no wheel; after the map / pause menu the
  view was third person for a while.
- BUG-71 closed `[HEADSET]`: foliage_occlusion = 2 is the default. Those passes then cull without the previous frame's HiZ
  (frustum and their own tests only) - their occlusion data was never per-eye correct under synchronized-sequential stereo.
- BUG-73 `[LIVE]`: 25 snaps in the log; 0.18.0 drove the GAME's camera turn with the right stick and held the view until it
  landed - the game's camera accelerates (most of a second for 30 deg) and the hold lagged a frame, so the view crept, paused,
  then jumped. Now the turn is VR-owned (playbook ch03): the view's yaw offset over the game camera changes at once (snap) or
  continuously (smooth, 150 deg/s at full stick), it is applied before the head pose (head motion turns with it), the left stick
  is rotated by it (forward = where you look), and the right stick turns the game camera after it in the background while each
  degree it turns comes off the offset - the view never moves during the catch-up, and aiming / targets follow your facing.
- `[LIVE]` wheel: opened 12 times, never used - it worked but had no picture. GoTVRHost 0.14.1 draws it (shared block v13): a
  disc above the hand holding the trigger, three labelled sectors, the picked one highlighted; plus a head-gesture TEST
  indicator low in the view while a hand is at the head (which hand, green) and the gesture's name for 1.5 s when it fires
  ([controls] gesture_indicator = 0 hides it). CPU-drawn placeholder (GDI, Segoe UI) until the game's own art (roadmap phase 1).
- BUG-72 `[LIVE]`: closing a pausing menu fades the HUD (veil, no bars) -> the cutscene latch started on it and held 15 s (the
  camera fov never changed), and the 1 s "HUD large after a menu" window also counted as cinematic -> third person. First
  person now yields only to a CAMERA cutscene (bars in the 3D picture, the engine letterbox rect, or bars on the HUD image);
  a fade-only latch ends 0.5 s after the fade. The UI may still cover the view meanwhile.
- Self-test PASS (102 checks; VR controllers: VR-owned snap with catch-up, view-relative stick, game turns pass through).

## GoTVR 0.18.1 → 0.18.2 — left-eye pop-in is not occlusion (BUG-74), pause menu off the screen after F10, HUD gesture, eye too far forward
- `[HEADSET]` 0.18.1 (the DLL still reported "0.18.0" in IDENTITY; host 0.14.1): the left-eye pop-in is back; the pause menu
  is no longer on the theatre screen; the wheel works; the left-hand gesture fired (INPUT/s "head gestures 1 (HUD 1)") but
  nothing appeared in front; the view sits a little in front of Jin's body (right height, right direction).
- BUG-71 re-opened, then re-scoped `[LIVE]`: OCCVIEW/s shows mode 2 on EVERY caller of the occlusion-view getter - grass,
  terrain tiles and the other six consumers (e.g. "calls 93222, engine view 0, none 93222"), and the owner cycled Shift+F8
  through all three modes. The pop-in survives all three, so it is not the HiZ occlusion view of those passes - the
  earlier "fixed" was coincidence. Mode 2 stays the default only because it measured no cost; if a later test shows a
  cost it goes back to 1 (owner rule: no cost without a measured fix).
- BUG-74 `[OPEN]` new hypothesis (playbook ch14 render-pass hazards, a2 pose pipeline): under synchronized-sequential stereo
  the LEFT eye is the simulated tick, and the camera for it was composed inside the tick (slot 43), i.e. AFTER the systems
  that cull / pick LODs / stream at the start of the tick. They decided with the previous pair's RIGHT camera - wrong eye
  and one pair old - so objects at the edge of what that camera saw pop in the left eye only, worse on fast head turns,
  matching the left-eye-only stutter. The right tick is frozen and re-uses the left decisions with its own camera set
  before it runs, so it is unaffected. Fix: pre_compose = 1 (default) composes the left-eye camera at the very start of
  the simulated tick (same packet, same frame counter, so slot 43 later re-uses it); Shift+F7 toggles it for an A/B in the
  headset; POSE/s counts "pre-composed" ticks. Cost: one extra compose per pair (microseconds).
- Pause menu `[LIVE]`: the log has "HOTKEY F10 theatre screen -> ALWAYS" then "-> never": the F10 cycle reached NEVER,
  which takes every menu off the screen for the rest of the session - not the BUG-72 change. F10 now toggles only
  auto <-> always; never is only reachable from the ini (theater = 0).
- HUD gesture `[LIVE]`: it toggled "HUD hidden", and the HUD was already hidden / room-fixed where it was not visible. The
  left hand at the head + trigger now (a) shows the HUD head-locked straight in front, 70 deg wide, for 6 s, and (b) presses
  the game's touchpad swipe up (PC: hold D-pad right + flick the right stick, confirmed by the PC port's own scheme), which
  shows the current objective with the guiding wind - default left_hand_at_head_trigger = hud+dpad_right+rs_up (set it
  to just "hud" for the HUD alone). The forearm HUD replaces this in roadmap phase 4.
- Eye position `[HEADSET]`: fp_eye_forward_cm default 9 -> 2 (eyes 2 cm in front of the head's top joint instead of 9).
  Existing ini files keep working: the value was never written to them.
- Self-test PASS (selftest=0, 102 checks; UI layer + UI flat-detection tests needed the gesture boost cleared at test
  set-up). Receipt: receipts/gotvr0182_selftest.log.

## GoTVR 0.18.2 → 0.18.3 — eye position reverted
- `[HEADSET]` 0.18.2: fp_eye_forward_cm 2 made the view worse ("so broken" that nothing else could be tested). Reverted to
  9 (the 0.18.1 position). Everything else of 0.18.2 is unchanged (pre_compose / Shift+F7, F10 auto<->always, HUD gesture).
  The "a little in front of Jin" report stays `[OPEN]`: it needs a measured change, not a guess - next time adjust in small
  steps from 9 via the ini (fp_eye_forward_cm) and report which direction helps. Self-test PASS (selftest=0).

## GoTVR 0.18.3 → 0.18.4 — wrong head joint when the game starts on the horse (BUG-75)
- `[HEADSET]` 0.18.3, game loaded with Jin on the horse: the view "a bit above Jin's head", the head visible again; after
  jumping off, the view settled near the floor.
- `[LIVE]` cause: "head = the highest joint 644 (203 cm above the model root)", "head set = 1 joints ... collapsed onto joint
  123". In every earlier session (on foot at load) the head was joint 123 (169 cm) with a 480-joint head set and neck 552.
  The skeleton dump of this session: joints 644 and 651 sit 40 cm above the head with NO other joint within 22 cm (gear
  carried only while riding); on foot joint 644 is at the root (1 cm). So the eyes followed lone gear (above the head on
  the horse, the floor on foot) and only that joint was hidden. The 0.18.2 session also loaded on the horse with the same
  wrong joint - its eye-position test (fp_eye_forward_cm 2) was spoiled by this bug, not judged on its own.
- Fix: the head is the highest joint near the body axis that has a dense neighbourhood (at least half the densest
  near-axis joint's count, capped at 40, shown joints within fp_hide_radius); the model search uses the same rule. Checked
  offline on both skeleton dumps (horse: joint 22/123 at 163 cm with 480 neighbours; standing: joint 123) and in the
  self-test (a lone gear joint 37 cm above the head is skipped). Cost: one O(n^2) pass over ~650 joints at hero
  (re)validation only. Self-test PASS (selftest=0).

## GoTVR 0.18.4 → 0.18.5 — pre-compose removed, eye-order swap diagnostic (BUG-74), HUD gesture panel, body drawn facing forward when running back (FEAT-02)
- `[HEADSET]` 0.18.4: head correct and hidden on the horse (BUG-75 closed). Left-eye pop-in unchanged with Shift+F7 on or off
  (`[LIVE]` 0.18.4 log: ~60 pre-composed left ticks/s while on, 7 Shift+F7 toggles). HUD gesture: "too big to see" - it must
  be the smaller, world-locked panel. Running backwards: GoT has no backpedal, Jin turns round and runs at the camera, so his
  body is in front of you.
- BUG-74: pre_compose removed (no cost without a fix). What is ruled out so far: the HiZ occlusion view of every consumer
  (0.18.1 mode 2, all eight Occ_View callers incl. terrain), the left-eye history cut on fast turns (0.17.3: ~50 of 60 left
  frames/s ran with occlusion history invalid - pop-in stayed), render-thread overlap (0.17.4 render sync), the first-person
  anchor within a pair (0.00 cm apart), the camera seen by culling inside the left tick (0.18.2 pre-compose). Remaining
  family (playbook ch14 #cpu-visibility-history, "results deferred a frame"): a visibility / LOD / fade state that a frame
  takes from the PREVIOUS frame - under synchronized-sequential stereo the left eye's previous frame is the right eye of
  the previous pair, the right eye's is the left eye of its own pair - or something that is genuinely about the left eye.
  0.18.5 adds the discriminator: eye_order_swap / Shift+F9 makes the simulated tick render the RIGHT eye (packet eye from
  the latched swap; pairing tag, pair count and monitor mirror now keyed by pair phase; a camera cut on the switch). If the
  pop-in moves to the right eye -> "eye after the simulation step" (bank that state per eye / find its owner); if it stays
  in the left eye -> left-eye-specific (pose / projection / transport). POSE/s logs which eye is simulated.
- HUD gesture: no enlargement, no head lock. The left hand at the head + trigger places the normal room-fixed panel straight
  in front of you (ui::placeReq -> follow(): "UI: panel placed in the room in front of you ... - HUD gesture") and sends the
  touchpad swipe-up (objective).
- FEAT-02 body follow (render only, playbook ch12: body heading from the game, never the head's yaw): when Jin faces more than
  80 deg away from your forward direction (game camera yaw + VR turn), his whole render pose is turned about the vertical
  through the head (fully at 130 deg, 540 deg/s), in the Anim_Job write point after the head hiding; the eye offset turns with
  it. His run animation therefore plays facing forward while he moves back. World array / gameplay untouched. Off while riding
  (riding markers = lone gear joints in the skeleton's tail more than 150 cm above the root: 644 / 651 at 203 / 201 cm on the
  horse and 1 / 3 cm on foot in the two skeleton dumps), above 8 m/s, and fail-closed when the model space is not upright.
  A job that copies the previous pose gets only the difference. BODY/s logs it. Known limit: anything drawn from the world
  array instead of the skin (a separate prop) is not turned.
- Self-test PASS (selftest=0): body follow (turned 180 deg about the head axis, head still hidden, eyes in front of the turned
  face; not while riding; not at 29 deg), HUD gesture places the panel; a mutation that skips the rotation fails it.

## GoTVR 0.18.5 → 0.18.6 — the "pop-in" is motion blur on the first eye of each pair (BUG-74), gesture chord, carried items turn with the body
- `[HEADSET]` 0.18.5: Shift+F9 moved the artifact to the RIGHT eye. The owner's description: "less pop-in, more like objects
  (terrain, trees) across the screen extremely fast and extremely distorted, only when I move my head very fast". HUD gesture:
  nothing visible. Backwards running good, but the sword does not turn with the body. Owner: the body should follow the head
  for full-body IK (next).
- BUG-74 diagnosis `[HEADSET]`+`[STATIC]`: the artifact belongs to the eye drawn right after the simulation step. Its previous
  frame is the OTHER eye of the PREVIOUS pair, so the engine's velocity buffer (camera motion from the previous frame) holds a
  whole pair of head motion plus the eye separation; the second eye's previous frame is the same pose sample (velocity = eye
  separation). Motion blur turns that into streaks across the screen on fast head turns - playbook ch14
  #velocity-not-temporal: "disabling motion blur is a requirement of alternate-eye rendering" (TAA keeps its per-eye history
  and velocity since 0.9.5 and is not the culprit). Fix: the PC settings block (app vt+0x118, reflection table 0x1404600F0:
  MotionBlurScale float +0x64, FilmGrainStrength +0x68, DOFQuality +0x5C, Vignette +0x71) gets MotionBlurScale = 0 while
  xr=4 runs (main thread, message pump, every 0.5 s; motion_blur_off = 1 default), the user's value is put back at exit; the
  log says what the setting was. `[OPEN]` whether the renderer reads the block live: if the streaks remain, set Motion Blur to
  0 in the game's graphics menu (the log line "motion blur: the game's setting ... is X" tells which case).
- Gesture chord `[LIVE]` 0.18.5: the HUD gesture fired twice and placed the panel ("UI: panel placed ... - HUD gesture"), the
  HUD layer was published (62/s) but every mid-row alpha sample was 0 before and after: the game drew no HUD - out of combat
  GoT shows almost nothing, and the swipe-up that brings up the objective never registered. The PC port's touchpad substitute
  is HOLD D-pad right, then FLICK the right stick; both arrived in the same poll. Now a gesture that mixes buttons and a stick
  plays it like a player: buttons at once, the stick 120 ms later, released 30 ms before the buttons (450 ms in all).
- FEAT-02b carried items `[STATIC]`: RE 0x57E0A0 (render gather of attachments) builds each attached item's instance from the
  attachment's local matrix x the hero's joint WORLD matrix (node+0x278, entry joint+1) - not from the skin. So 0.18.5's turn
  of the render pose could not reach the sword. Now, when the body is turned, the joint world array is turned too, right
  after the hero's animation job (Rodrigues about the up axis through the head joint, entries 0..n); the next job rewrites the
  whole array (no accumulation), the head joint's position (eye anchor) is unchanged, riding markers keep their height.
  fp_body_follow_items = 1 (default). Game code that reads Jin's joints in that tick sees the turned (VR) body. BODY/s counts
  "items" turns.
- Self-test PASS (selftest=0, 103): motion blur held / restored / untouched when off or not in VR; the gesture chord (D-pad
  first, flick 120 ms later, flick ends first); the joint world matrices turned on the head axis.

## GoTVR 0.18.6 → 0.18.7 — the streaks are the game's upscaler (BUG-74), five-option wheel, eyes centred, FBIK stage 1
- `[HEADSET]` 0.18.6: HUD gesture works; sword turns with the body; the first-eye streaks persist "even with motion blur set to
  zero"; the view should be a little higher and is a bit toward Jin's left shoulder. Owner: add Play flute and Recenter view to
  the wheel (CONTROL_SCHEME.txt), move on to full-body IK (playbook + bldbrne-vr and its credits).
- BUG-74 `[LIVE]`: the 0.18.6 log says the game's MotionBlurScale was already 0.00 - motion blur was never the cause (0.18.6's
  conclusion withdrawn; the hold stays, it is free). The receipts show the game's TAA restoring its per-eye history only in
  0.9.8-0.10.0 (restored 29..107/s, resets 0); from 0.11.0 - when DLSS was tried (owner: "that anti-aliasing change might have
  been when we tried DLSS, remember we abandoned that") - every session has resets = every frame, restored = 0: the game's TAA is
  not running, an upscaler does the anti-aliasing with ONE history for both eyes. Under synchronized-sequential stereo the eye
  drawn first in each pair blends the other eye's view from a pair earlier, reprojected with cross-eye motion: streaks on fast
  head turns, in that eye only, following Shift+F9 - matching every observation. Fix: the game's Upscaling setting back to
  Off / TAA (the mod's per-eye TAA bank then works again). 0.18.7 logs "AA/s" (TAA resets, upscaler pass runs, the setting's
  raw method / quality from the settings block +0xB0 / +0xB4) and an "AA WARNING" line when TAA is reset every frame.
- Wheel: five options (wheel_count, up to 8; 90 - i*360/n degrees): Heal, Photo mode, Call horse, Play flute (touchpad swipe
  LEFT = hold D-pad right + flick the right stick left, played in order like the gestures), Recenter view (= F11). Chords on
  the wheel are staged like gestures; the snap-turn catch-up leaves the stick alone while a chord plays. Shared block v14
  (8 wheel names); GoTVRHost 0.14.2 draws n sectors.
- Eyes `[HEADSET]`: fp_eye_up_cm -11 -> -8 (3 cm higher). Sideways the eyes now sit on the centre of the head cluster: the head
  joint (123, the top of the head) is 3.3 cm (standing) / 4.0 cm (riding) to Jin's LEFT of the cluster centre in both
  skeleton dumps - the "toward his left shoulder". New fp_eye_left_cm for fine tuning; FP/s logs the correction.
- FEAT-03 FBIK stage 1 (docs/FBIK.md): standing, the body follows the head's yaw past 35 deg (tau 0.25 s, settle 2 deg); VR
  arms: parent table found at run time, arm chains found by shape, FRIK clavicle reach, two-bone IK with the BetterVR pole,
  grip-frame hands, held items follow (world matrices of the arm subtrees), 1-cos blend, arm joints out of the head set;
  Shift+F10 / fp_arms; ARMS/s and "arms:" log lines. Not yet: arm ownership during attacks / climbing (the controllers win).
- Self-test PASS (selftest=0, 104): FBIK arm test (synthetic skeleton: chains by shape, reach, lengths, elbow down/out, rigid
  fingers, grip frame, clavicle shrug for far targets), five-option wheel (flute chord staged, recenter), all earlier tests.

## GoTVR 0.18.7 → 0.18.8 — no VR arms (chain search), TAA now on: per-eye TAA working, left-eye artifact still open
- `[HEADSET]` 0.18.7: no VR arms with or without Shift+F10. With TAA turned on the pop-in "seemed worse", and the left eye did not
  draw far-away objects like the right eye until the pause menu (which "seemed to reset it").
- Arms `[LIVE]`: "the skeleton's parent table found at <rig>+0x48 (650 of 654 bone lengths under 80 cm)", rig bone count 655 =
  pose joints - the table is right; but fbik::findArms found no chain: it demanded a compact hand subtree (<= 25 cm), and items
  parented under Jin's hands reach much further. 0.18.8 searches by the bone chain only: every joint's elbow (first ancestor
  >= 15 cm away, past twist joints) and shoulder (the next >= 15 cm), forearm 15-45 / upper arm 15-50 cm, shoulder high and off
  the axis; per side the (elbow, shoulder) pair shared by the most joints wins (fingers, hand, twist joints of one arm), the hand
  = the member nearest the elbow with members below it. If it still fails, the whole parent table + pose is written to the log
  once. Self-test skeleton now has a held item 60 cm out under each hand and a forearm twist joint (0.18.7's search fails it).
- AA `[LIVE]`: after the owner switched the game to TAA the per-eye TAA bank works as designed (restored = draw = 113-161/s,
  resets 0; same-eye camera gap ~1 cm vs cross-eye ~6 cm) - for the first time since 0.10.0. The 0.18.7 "upscaler pass runs"
  counter counts pass 59 on every frame (not an upscaler signal) - noted. The left-eye artifact is still `[OPEN]`; the counters
  show no difference before / after the pause that "fixed" the far objects (OCC/s per-eye fixed, no bank failures).
- To bisect it in one session: F6 history presets (1 per-eye occlusion + per-eye TAA, 2 per-eye occlusion + engine TAA, 3 TAA
  reset every frame, 4 everything cross-eye, 5 global cut every frame); F11 (and the wheel's Recenter view) now also reset all
  per-eye histories (a camera cut + occlusion banks re-seeded), like the pause menu does.

## GoTVR 0.18.8 → 0.18.9 — per-eye occlusion by tracked slot ownership (BUG-74), hands, near plane, stand tall, arm ownership
- `[HEADSET]` 0.18.8: F6 preset 4 (everything cross-eye) removed the left-eye pop-in but distant objects went missing in one eye or
  the other; presets 1-3 (they differ only in the TAA) did not -> the pop-in is in the per-eye OCCLUSION path. VR arms worked, hands
  "totally upside down", arms see-through very close to the eyes, view at neck height, the sword stance crouch bends the knees
  (owner wants attacks fully player-driven), flute / climbing / takedowns should be the game's animation.
- BUG-74 analysis: under preset 1 and preset 4 the left eye's source HiZ comes from the same previous head-pose sample, so the
  difference is not freshness but CONSISTENCY: the 0.9.4 bank pairs "slot (1-p)" with "this eye's camera of two ticks ago" on the
  assumption that the parity toggles once per rendered frame; the engine pairing (preset 4) is consistent but cross-eye. Fix (occ
  mode 2, default preset 1): every frame records which eye wrote which HiZ slot (its destination 1-p) and the camera that drew it
  (+0x430 view-projection, +0x550 position - Cam_StorePrev 0x8085D0 copies exactly these into the prev slots); an eye tests against
  a slot only when its OWN last frame (<= 4 frames) wrote it, reprojected with that frame's camera, else the engine pairing. No
  parity assumption. OCCTRK/s logs own-slot tests vs engine pairings and the parity each eye sees. The 0.9.4 bank stays as F6
  preset 6. Self-test: own slot + own camera, the right eye when the parity does not toggle, engine fallback when overwritten.
- Hands `[HEADSET]`: wrist -> knuckles is grip +Y (0.18.7 used -Y: 180 deg about the thumb axis = upside down); the wrist sits below
  the grip. Near plane: first person sets the camera's near (+0x664, CAMERA_FINDINGS) to fp_near_cm (4) and patches the reversed-Z
  projection terms only when they match the engine's formula for its own near/far (else logged, left alone); restored when first
  person ends. View: fp_eye_up_cm -8 -> -3.
- FEAT-03b stand tall: the standing head height is tracked; a shallow drop (< 25 cm, nearly still, not riding, game not owning the
  arms) is given back: the whole render pose is raised and both legs re-planted by two-bone leg IK (knees forward, foot keeps its
  animated orientation, never past 98% of a straight leg); the eyes rise by the applied amount. Legs found by the chain search (hip
  0.25-0.75 of head height, foot low; the ankle = nearest the knee with toes below).
- FEAT-03c arm ownership (bldbrne-vr gates arms by animation class with eased hand-over): the game owns Jin's arms - 0.3 s 1-cos
  blend - while (a) an input says so: new output tokens anim (until you move or dodge; default on the wheel's Play flute) and
  anim_short (2.5 s; default on quick attack / assassinate), or (b) the ANIMATION lifts a hand to within 15 cm of head height
  (climbing, ledges, grappling, the flute at the mouth), held 0.4 s. Stand tall is off while the game owns the arms.
- World matrices: everything the mod moved (turn, stand tall, arms) is written back from the final render pose (world = pose x
  node), except the head set (the eye anchor reads the animated head; the raise is added there).
- Self-test PASS (selftest=0, 104): tracked occlusion, legs / stand tall (feet planted, lengths, knee straighter, foot flat, capped).

## GoTVR 0.18.9 → 0.18.10 — cross-eye occlusion default (owner), the see-through is the near stipple fade, hands, grip unbound
- `[HEADSET]` 0.18.9: pop-in worse with the tracked occlusion; shadows in one eye only; hands right only with the controller pointing
  straight down; Jin still see-through; still a small crouch before attacks. Owner: setting 4 as the default; the leg solver may
  not be needed (own attack system); unbind the grip from attacking (physical sword next).
- Occlusion: default history preset = 4 (KEPT, everything cross-eye: the engine's own pairing) as the owner asked - the only
  setting without the left-eye pop-in in the headset. Known cost: objects that only one eye can see past a near occluder can be
  culled in that eye. Tracked ownership stays as F6 setting 1, the 0.9.4 bank as 6. `[OPEN]` the one-eye culling under 4.
- See-through `[STATIC]`: the 0.18.9 near plane DID apply ("near clip plane 10.0 -> 4.0", the game rebuilt its projection with
  B = 4.0) and Jin stayed see-through -> not clipping. The game's environment parameters (353 per set, 32-byte entries: value,
  +0x10 index, +0x14 type, +0x1C set flag; ctor 0x619E10, entry init 0xAFF100, defaults 0x14180A980 + i*0x18) hold
  NearStippleDist 200 cm, NearStippleDistClose 100 cm and DisableNearStippleFade (index 106, bool): the third-person dither that
  fades the hero when the camera is within 1-2 m. In first person every active set (blend list 0x1433938A8, count 0x1433939A8,
  Env_SetA 0x61A9C0) and the two global sets get DisableNearStippleFade = 1 (entry checked: index 106, type 3); the game's values
  come back when first person ends. Near plane override reverted (fp_near_cm = 0): it was not the cause and was applied
  inconsistently between eyes ("projection does not match" lines) - a suspect for the one-eye shadows.
- Hands `[HEADSET]`: 0.18.9's frame was right only pitched 90 deg down -> fingers along grip -Z, thumb along -Y (0.18.9 pitched up
  90 deg). Trims still apply.
- Stand tall off by default (fp_stand_tall = 0; the owner's own attack system replaces the game's stance). Right grip unbound
  (right_grip = none): the physical sword gets the grip.
- Self-test PASS (selftest=0, 105): near stipple fade (checked sets only, restored), all earlier tests.

## GoTVR 0.18.10 → 0.18.11 — per-eye frustum culling (BUG-76), near plane made consistent (BUG-77), hand trim keys (FEAT-04), eyes 5 cm back
- `[HEADSET]` 0.18.10: pop-in fixed with setting 4; objects in one eye's view are culled in the other ("each eye needs its own
  object culling"); view too far forward; hands still not right; Jin's arms transparent very close up.
- BUG-76 `[STATIC]`: setting 4 = the engine pairing: every Occ_View 0x9E5960 caller (about 60 per frame: grass, foliage, tiles
  and the rest) tests against the previous frame's HiZ, which under xr=4 is the other eye's. Anything that eye could not see
  (6.4 cm of parallax behind near occluders: Jin's arm, grass clumps, posts) is culled in this eye. The per-eye presets 1-3 and 6
  already returned no view to all of them (OCCVIEW/s "none" ~3400/s in their logs), so their left-eye artifact was never the HiZ
  test. What 4 also turns off is the fast-head-turn rule (an occlusion cut forced during fast turns; the cut path tests the
  CURRENT camera against an old pyramid and hands the CPU occlusion buffer to the five Occ_ViewCut 0x9E5970 callers) and the
  hOcc rewrites. New occ mode 3 = preset 7 (default, `history_mode=7`): Occ_View returns null for every caller, hOcc leaves the
  setup alone, no fast-turn cut, engine TAA. It is 4 minus the cross-eye test, so each eye is culled only by its own frustum
  (playbook ch14: "reporting visible can never wrongly delete geometry, only fail to save work"). Cost: no HiZ culling (GPU).
  `[OPEN]` headset: one-eye culling gone? left-eye artifact still gone? (If it returns, the null view itself is implicated.)
- BUG-77 `[STATIC]`: the log showed the stipple fade off in 7 environment sets. What's left ("very close up") is the game's 10 cm
  near clip. In 0.18.9 the override left alone ticks where the game had rebuilt the projection with our near but reset +0x664
  to its own, so the camera said 10 cm under a 4 cm projection on those ticks (wrong depth linearisation, a suspect for the
  one-eye shadows). Now `nearp::apply` accepts a projection built with either near and always sets both the projection and
  +0x664. Default `fp_near_cm` 2. NEAR/s counts the applications per eye and the unrecognised projections.
- FEAT-04: F1/F2/F3 = hand pitch/yaw/roll +5° (Shift -5°), wrap at ±180, written to gotvr.ini on every press. The old
  F1/F2/F3 functions moved to Ctrl+F1/F2/F3.
- Eyes: `fp_eye_forward_cm` 9 -> 4 (owner: 5 cm back).
- Self-test PASS (selftest=0, 107): preset 7 (setup untouched, null view even with the engine-view setting, never a fast-turn
  cut), near plane (game-built / ours / unknown / restore), hand trims (steps, wrap, ini round trip), all earlier tests.

## GoTVR 0.18.11 → 0.18.12 — snap-turn wobble (BUG-78), wrist pivot (BUG-79), near clip in the environment (BUG-77b), eyes 3 cm back
- `[HEADSET]` 0.18.11: pop-in, shadows and one-eye culling all fixed (owner: leave them alone). New: the camera fights back and
  forth a little after a snap turn; arms still "transparent-ish" close up, "like the texture behind gets written on top of the
  arm"; hands pivot about a point near the wrist, and palms down the controllers are a few cm higher than the hands; view still too
  far forward. NEAR/s: near 2 cm applied in both eyes (~420/s each), none unrecognised. Owner's trims: pitch 5, yaw 15, roll -15.
- BUG-78: vrinput subtracted the game camera's catch-up turn from the view offset at the next controller poll (input thread), but
  hSlot43 composes the view from THIS tick's game camera: for a frame or two the view had the new camera yaw plus the old offset.
  Now viewHold is the single authority (atomic, CAS-add from both threads) and `vrin::gameTick` (called where hSlot43 reads the
  game camera) subtracts the turn in the same update. Catch-up hysteresis: starts above 2 deg, stops below 0.5 deg.
- BUG-79: the OpenXR grip origin is the centre of the fist around the handle (palm side of the forearm axis). Wrist target =
  grip - fingers*7 cm only -> hand low, pivot below the wrist. Now + back-of-hand*`fp_hand_up_cm` (3) + thumb*`fp_hand_side_cm` (0);
  back of hand = grip -X on the left, +X on the right (+X points to the user's right for both hands per the spec); the vectors
  turn with the trims. Ctrl+F1/F2/F3 = back/up/side ±1 cm, saved; old Ctrl+F1-F3 -> Ctrl+Alt+F1-F3.
- BUG-77b `[STATIC]`: the environment name table 0x141809260 (char* per index) has NEAR_CLIP at index 5 (10 cm default),
  STIPPLE_NEAR_RADIUS_HORIZONTAL/VERTICAL at 312/313 (the "stipple what is between the camera and the focus object" feature). The
  camera's near comes from NEAR_CLIP each tick. 0.18.11 overrode only the projection / +0x664 in slot 43, so whatever the engine
  derives from the environment's near beforehand (the TAA reprojection that fetches history, depth linearisation) still used 10 cm
  against a 2 cm depth buffer: near pixels were reprojected as if 5x farther, so between the eyes TAA fetched the background behind
  the arm. `[OPEN]` headset confirmation. The environment entry (float, type checked against NEAR_STIPPLE_DIST's) is now set to
  fp_near_cm in every active set while first person is on and restored after. NEAR/s shows the camera's near and the entries set.
- Eyes: fp_eye_forward_cm 4 -> 1.
- Self-test PASS (selftest=0, 107): environment (stipple + near clip, typed entries only, restored), snap turn with the game-thread
  catch-up, all earlier tests.

## GoTVR 0.18.12 → 0.18.13 — focus-object stipple (BUG-77c), idle hold (FEAT-05)
- `[HEADSET]` 0.18.12: snap turn fixed. Arms "sometimes transparent - it depends on where the headset looks: straight on both hands
  mostly transparent, down and to the left at a specific angle fully solid wherever the hands are". Owner: stop Jin's idle
  animations in first person.
- BUG-77c `[STATIC]`: a view-dependent, hand-independent mask = a screen-space region anchored to something that moves with the head.
  Environment entries 312 / 313 STIPPLE_NEAR_RADIUS_HORIZONTAL / _VERTICAL (1.0 / 0.35; "screen-space radius of sphere around focus
  object that should be stippled out") drive the third-person feature that dithers what is between the camera and Jin. In first
  person the focus object sits at the camera, so the dithered circle follows the view. Both radii = 0 in every active environment
  set while first person is on (`fp_focus_stipple_off`, default 1), restored after; float type checked like NEAR_CLIP. `[OPEN]` headset.
  (The near-clip change of 0.18.12 stays: it makes the depth the anti-aliasing uses agree with the 2 cm projection.)
- FEAT-05 idle hold (playbook ch02: weight, rate and position are separate levers - we own none of the game's graph, so the hold is
  on its output): after `fp_idle_settle_s` (0.5) of standing still, the fresh render pose is captured right after the hero's
  animation job and replaces every new pose while idle (model space; the world array is rebuilt from it, head set included, so the
  eye anchor does not bob). Arms IK, head hiding and body follow run on top. Let go (0.2 s 1-cos blend, then recapture next time) on
  any pad output (vrin::lastAct), body speed > 0.3 m/s, game-owned arms, riding, or neck / hips / feet moving > 50 cm/s in model
  space (hit reactions, scripted motion). IDLE/s logs state, weight, held poses, releases.
- Self-test PASS (selftest=0, 108): idle hold (capture, hold through a slow fidget, world matrices, release on input / walking),
  environment (+ focus radius), all earlier tests.

## GoTVR 0.18.13 → 0.18.14 — arm meshes culled by animation bounds (BUG-80), stance squared (FEAT-05b)
- `[HEADSET]` 0.18.13: focus-stipple radius 0 changed nothing (log: 28 entries set, near 2.0 everywhere); idle hold works (69 s
  holding) but the stance is turned ~15 deg right (left foot forward). Owner reminder: no attack hit-reaction animations in first
  person (later).
- BUG-80 `[STATIC]`, hypothesis: "solid when looking down-left at a specific angle, wherever the hands are" = the arm / hand meshes
  are frustum-culled with bounds where the ANIMATION has the arms (left hand on the katana at the left hip), not where FBIK draws
  them; at the frustum edge only one eye culls them (the half-transparent look). Cam_SetVPF 0x806BA0 -> 0x803BD0(cam, rectMin,
  rectMax, 6, cam+0x4F0) builds 6 camera-relative planes from VPrel (+0x430) and the NDC rect +0x6A4 (-1,-1,1,1). In first person
  the four side planes (|n.f| < 0.97; else the layout is left alone) are shifted so the apex is fp_cull_back_cm (100) behind the
  eye along the view: d' = d + D (n.f). Near/far untouched. Applied after both slot-43 paths (normal, and the right eye's SetVPF).
  Shift+F7 A/B; CULLBACK/s counts frames / unknown layouts; the first application logs the planes. `[OPEN]` headset + A/B.
- FEAT-05b: at idle capture the hip line and shoulder line (left - right, model space) give the stance yaw; the held pose is turned
  back about the head's vertical (|yaw| <= 40 deg, else untouched). IDLE/s shows the correction. fp_stance_square (1).
- Self-test PASS (selftest=0, 110): culling apex, stance squared, all earlier tests.

## GoTVR 0.18.14 → 0.18.15 — removals, hip squaring (BUG-81), snap-turn body (FEAT-06), TAA A/B (BUG-80 reopened)
- `[HEADSET]` 0.18.14: the culling apex change did nothing for the arms and caused black screen edges ("the opposite side has too
  much culling near the edges") -> REMOVED, together with 0.18.13's focus-stipple radius and 0.18.12's environment NEAR_CLIP (owner:
  "scrap those fixes"). Kept: 0.18.10 DisableNearStippleFade, 0.18.11 projection near 2 cm. The owner's description, restated:
  the arms are visible, but "whatever is behind the arm is displayed on top of the arm, almost like an added texture" - not culling.
  0.18.14's culling-bounds diagnosis was wrong.
- BUG-81: the IDLE/s log measured the standing stance at 31.8 deg to the right. The standing body follow centred its 35 deg dead
  zone on the node + bodyYaw, but the hips drawn are another 32 deg right: look 3 deg right of the hips and the body turns, look left
  and it needs 67 deg. Now hideInPose measures the fresh pose's hip line (after the idle hold), smooths it (0.15 s) and adds its
  negative to the body turn while standing (faded out between 0.3 and 0.6 m/s, off when riding / unmeasurable / > 46 deg). The drawn
  hips = node + bodyYaw: dead zone symmetric. BODY/s shows the stance hips.
- FEAT-06: vrin::turnSeq counts snap / smooth turns; the standing head follow starts a turn on a new one (|e| > 2 deg), so the hips
  come square to the view after every snap.
- BUG-80 (reopened as a TAA question): Shift+F7 cycles taa::diag: 0 the preset's TAA, 1 per-eye TAA (taa::on = 3: this eye's
  history bank + reprojection), 2 the engine's TAA reset (AA+0x1D40E) every frame. F6 resets it to 0. TAADIAG/s logs it.
- Self-test PASS (selftest=0, 110): snap-turn body, environment (only DisableNearStippleFade changed), all earlier tests.

## GoTVR 0.18.15 → 0.18.16 — world-frame body heading (BUG-82), move-facing (FEAT-07), body under HMD (FEAT-08), anchor jitter (BUG-83)
- `[HEADSET]` 0.18.15: Shift+F7 TAA A/B changed nothing on the arm overlay (-> removed; not TAA). Body "turns too much at the slightest
  turn either direction". Owner asks: keep a 35 deg editable threshold, even both ways; body directly under the HMD; movement
  animations face the headset; "stuttering and jittering when I look down, not FPS" (log: ~58 stereo pairs/s steady).
- BUG-82: bodyYawCur was RELATIVE to the node, which the game turns itself (run direction, the snap catch-up, turn-in-place), so the
  drawn body moved with every node turn and the dead zone was centred wherever the node pointed (log: "now" jumping 12 -> 37 -> -23 ->
  109 deg while standing). Now Hw = world yaw of the drawn hips: standing, Hw only moves when the head is more than
  fp_body_head_deadzone_deg from Hw (either side), toward the head (tau fp_body_head_follow_s), until within 2 deg; snap/smooth turns
  start the turn; riding / > bodyMaxSpeed: Hw = node. bodyYaw = wrap(Hw - node yaw), rate-limited as before.
- FEAT-07: moving (>= 0.6 m/s), Hw follows the head's yaw continuously (fp_body_move_follow_s 0.15) - strafing / backing up face the
  view. The refF start/full ramp (bodyStartDeg/FullDeg) is no longer used.
- FEAT-08: hSlot43 (left tick) composes the head-centre pose and publishes its horizontal offset from the eye anchor (world). hideInPose
  converts it to model space (node rows), clamps to fp_body_under_max_cm (60) and translates the render pose after the body turn
  (copied poses get the delta), before the arm IK; worldFromPose then moves every non-head joint's world matrix (items follow). The head
  set's world entries are untouched, so the eye anchor has no feedback. Off while riding. UNDER/s logs it.
- BUG-83: the eye anchor's sideways head-cluster correction was unsmoothed and the head offset used 0.3 s: every animated head twitch
  moved the camera against the body you look down at. Both now use fp_lean_smoothing_s = 1.0 s (committed once per tick like the rest).
- Shift+F7 = near-plane A/B (2 cm <-> the game's 10 cm) for the arm overlay.
- Self-test PASS (selftest=0, 110): body follow rewritten for world heading (moving faces the head, standing ±29 deg no turn both ways,
  riding as animated), body under the headset (pose + world shifted, clamped), snap-turn body, all earlier tests.

## GoTVR 0.18.16 → 0.18.17 / 0.18.18 — movement forward, menu direction, threshold 45, smoothed body-under, VR fingers (FEAT-09), arm see-through = SSW?
- `[HEADSET]` 0.18.16: arm fixes did nothing. The owner's screenshot (monitor) shows solid arms; "it clearly only shows in headset:
  the grass (in black and white) can be seen through the arm (the arm is still there, a little see-through)". Asks: threshold 45;
  body follows the headset smoothly; "movement directions are screwed up with this animation change - forward = the headset's forward
  at every snap turn (and once at the start / exiting menus)"; "when I exit a menu I don't have the same direction"; then the next
  task: hands fully detached from animation, custom finger poses, grip = fist (refs bldbrne-vr, God-of-War-VR-Mod).
- BUG-80 (arm see-through) `[INFERENCE]`: not in the game's image (the mirror is the game's left-eye frame) -> added after the game:
  the headset runtime. Virtual Desktop's Synchronous Spacewarp synthesises frames at ~58 fps per eye; the host submits no depth
  (no XR_KHR_composition_layer_depth in host.cpp), so SSW estimates motion from colour alone - near objects over textured background
  show the background as grey ghosts and wobble (also the "jitter when looking down"). Owner test: SSW off. Proper fix later: submit
  the game's depth (and motion vectors where the runtime takes them).
- 0.18.17: vrin::moveYaw (world) = the headset forward (hSlot43 headF) at each snap / smooth turn (30 ms after), the first live poll,
  menu exit (60 ms) and Recenter (150 ms); the left stick is rotated by moveYaw - game camera yaw (falls back to viewHold before the
  first capture). Moving, the body's Hw follows moveYaw. Menus: the view's world yaw (game yaw + viewHold) is saved on entering a flat
  context and restored on leaving (viewHold = saved - game yaw, the catch-up turns the game camera); 0.18.16 zeroed viewHold in menus.
  fp_body_head_deadzone_deg 35 -> 45; the body-under offset low-passed (fp_body_under_follow_s 0.5).
- FEAT-09 (0.18.18) VR fingers (fingers.h): bind pose from the palette job's inverse-bind array (0x59EB30: node+0x2F8 -> +0x660 ->
  +0xA8, 3x4 [R|t] rows per joint, 0x30 apart, same joint order as the render pose); analysed with the arms: finger chains = the
  hand's children followed while single-child, links 0.5..8 cm in the bind AND within 15 % of the animated lengths (validates both the
  pointer chain and inverse-vs-direct, tried in that order; fail closed -> animated fingers); thumb = base most along the hand's thumb
  direction, index = the long finger nearest the thumb. Per frame after the arm IK: finger joints = hand x (bind hand^-1 x bind finger),
  other hand-subtree joints keep their animated offset from their parent (sockets, helpers), then each link bends toward the
  controller's palm normal (axis = bone x palm): relaxed 5 + (max - 5) * curl, long 80/95/65, thumb 20/30/40 deg (God of War VR's
  maxima, bldbrne-vr's per-finger/per-joint tuning shape); a first link > 5.5 cm (metacarpal) does not bend. Curl = (squeeze - 0.05) /
  (fp_grip_fist_full 0.35 - 0.05), index = max(grip, trigger), smoothed 60 ms; blended with the arm weight / game-owned gate.
  FINGERS/s logs it; the analysis logs one "fingers:" line.
- Self-test PASS (selftest=0, 111): VR fingers (synthetic hand: chains, thumb/index, bind fingers on a turned hand, socket kept, fist
  curls toward the palm with lengths kept), menu direction + movement forward, all earlier tests.

## GoTVR 0.18.18 → 0.18.19 — riding false positive in jumps (BUG-84), stop = new forward, finger chains from leaf paths (FEAT-09b), HUD-layer test
- `[HEADSET]` 0.18.18: SSW off did not fix the arm see-through; VR fingers not working; the body turns before 45 deg; "at the beginning of
  the jump animation, VR IK arms stop working"; wants the movement forward set when the player stops.
- Log 0.18.18: `fingers: ... not usable | left: 3 finger chains (1 with a bind pose that does not match)` (both readings) - single-
  child runs from the hand do not reach Jin's fingers (helpers / shared palm joints). BODY/s "held off: riding 27 / 5" ON FOOT, with
  the drawn body snapping 57 deg -> 1 deg.
- BUG-84: riding() = any lone tail joint > 150 cm above the root; a jump lifts carried gear (one marker) past it -> ridingNow -> solveArms
  skipped (arms back to the animation at the jump's start) and Hw = node (the "turn before 45 deg"). Now >= 2 high markers (or the
  only one), debounced 0.25 s on/off; rideMarkersHigh logged. The raised-hand gate also ignores 1.2 s after a jump press (A).
- Stop -> vrin::moveCapture when the speed falls below 0.3 m/s for 0.3 s after having been above 0.6. BODY/s now counts dead-zone
  turns, snap turns and the forward captures.
- FEAT-09b: chains = leaf paths of the hand subtree (>= 3 joints, > 2 cm bind length); a leaf branching off a joint whose other child
  goes on is a helper (dropped); sibling leaves keep the longer path; joints on one path bend, shared ones (palm) take the bind pose
  unbent. Both readings of the palette matrices are analysed and the one with fewer finger-length mismatches is used (fail if > 1/3
  mismatch). The hands' structure (joint, parent, children, hand-local animated / bind position, link lengths) is logged once.
- Arm see-through, new hypothesis `[INFERENCE]`: the HUD quad layer (room-fixed panel 1.67 x 0.94 m at 1.6 m, drawn by the runtime
  over the projection layer, premultiplied blend) - headset-only, view-dependent, "an added texture". Shift+F4 cycles premultiplied ->
  straight alpha (XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT, stray colour at alpha 0 not added) -> HUD hidden. Owner: test, and
  report one eye or both.
- Self-test PASS (selftest=0, 111): fingers with a shared palm joint and helper leaves (5 chains, index 3 joints), all earlier tests.

## GoTVR 0.18.60 → 0.18.61 / host 0.15.2 — first VR combat test: parry, frame rate, left hand, controls, HUD
- Frame rate: SYNC/s 55-60 sheathed vs 21 / 28 / 37 with the katana drawn (lower with more characters near). readable()
  = VirtualQuery per call (cost grows with the region's size); vrBlade checks every registry entry every frame (alive,
  hostile, position: ~8 calls each). Fix: per-thread cache of the last 32 regions for 200 ms (PERF/5s counts).
- Left hand off the hilt while moving: solveArms side 0 aimed at sp.kat* (game thread, a tick old) while swordApply
  carries the katana by the solved right fist. Side 0's P0 / Am now get the same correction (drawnTgtP / drawnTgtA →
  fistGm[1] / fistDm[1]).
- Sheathing animation still seen: the auto-sheathe came at 30.3 s (learned 16.3 s). sword_mask_while_held: maskUntil held
  +400 ms every frame while S_DRAWN (legs stay free while walking).
- Death fade → cutscene START → whole-view HUD: a fade alone neither starts nor holds a cutscene while Jin is down
  (≤ 1.5 health, 8 s); g_jinDownAt / g_jinHurtAt from vitalsTick. Hit flash: M_CINEMA for ui_hit_fullscreen_ms after a
  health loss.
- Controls: right_trigger "rt+up", left_trigger "back" (migrated once, controls_v=2); blockMask + XB_BACK | XB_UP;
  wheel tap (< wheel_hold_ms) = wheel_tap ("down" = heal), hold = wheel (self-test updated, fresh fake samples during
  holds); heavy swing rumble 1.0 for sword_heavy_swing_rumble_ms.
- Block / parry (parry.h, self-tested): hHitProc → parryTry for an enemy's record on Jin while the katana is out (vrBlade
  feeds a 32-sample blade history on its own QPC clock); timing mode: the run of blade-in-line samples ending in the
  latest one; entered ≤ parryMs × perfectMult → PARRY (LB fresh, replay after 50 ms), else BLOCK (LB held, replay
  after 300 ms); the record (0x120 + tags) is copied and replayed with the original HitProc (parryReplay), checked
  first (attacker alive, components / holder readable, pool, target still Jin); noteHit reports the composed flags.
- Host 0.15.2: wrist_hud_mode 1 (migrated once, wrist_hud_v=2); wrist_hud_lock top: face = world up square to the
  forearm line (grip +Z), right = toward the hand; head direction when the forearm is near vertical.
- Self-test PASS (selftest=0).

## GoTVR 0.18.59 → 0.18.60 — dog record slots; plainest blow per kind
- recs[4] (light / heavy × people / dogs), ini keys cbt_rec_light / _heavy / _light_dog / _heavy_dog; hHitProc candidates
  for class 14 and 16 targets; recJudge slot by the target's class; a same-reaction record is replaced only by a blow of
  no more damage (session 16: 150 / 170 / 225 "light" blows). strike(): dog → its slot, else the people's; heavy → light
  x1.6 when missing; any saved slot as the last resort.
- Self-test PASS (selftest=0).

## GoTVR 0.18.58 → 0.18.59 — the record carries its definition; records load at start
- Session 15: 3-step search from both roots found no definition (separate asset array, 0xB0 per definition).
- RecT.def[0xB0] copied at recLearn; recResolve: no holder / path → R.cell = &R.def (the composer and HitSend read only
  **(rec+0x50) +0x88 / +0x9C / +0xA4). Save format v5 appends ";" + hex(def); load checks def+0x88 against the saved hash.
- installStrike loads cbt_rec_light / cbt_rec_heavy at start (cbt_mode 2) so recJudge keeps a saved reacting record.
- Self-test PASS (selftest=0).

## GoTVR 0.18.57 → 0.18.58 — definition path (3 steps), VR contact / velocity in the record, katana weight model
- Session 14: definition not found one step from the moveset / per-attack entries. defSearch: roots moveset entry (0xE0) and
  per-attack entry (0x58); 1, 2 (≤ 0x200) or 3 (≤ 0x100) pointer steps; mode = root * 10 + steps; defHash (def+0x88)
  saved and checked on rebuild. Save format v4. recResolve no longer requires k50 when a definition path exists; the
  `//` comment that swallowed the rest of the rebuild line during the edit was caught at compile.
- VR: Req carries the melee hit point and the tip velocity (vrBlade's own clock); strike(ctc) writes +0x90 / +0xA0 / +0xB0.
- Katana weight (attack.h Weight): rotational spring-damper (period weight * 45 ms * (1 + (twoHandFactor - 1) * twoW),
  damping 0.85), tangent velocity, gravity sag (sagDeg * weight * (1 - 0.8 twoW) * level), soft lag limit (16 → 28 deg),
  non-finite guard; the filter runs on the blade's forward (tip side) so gravity lowers the tip. gotvr.cpp: reset list
  (hilt jump > 40 cm, turnSeq change, g_flatRender, bad dt) with a SWORD log line. ini: sword_weight 1.0 (an old 0 moved
  to 1.0 once, marker sword_weight_model=2), sword_weight_sag_deg 3, sword_weight_two_hand 0.33.
- Self-test PASS (selftest=0) with new weight tests: 90 deg snap trails ≤ 28 deg, level sag 3.0 deg one hand / 0.6 two,
  none pointing up, settles.

## GoTVR 0.18.56 → 0.18.57 — Ctrl+Shift+M crash (reaction bit on the message path); definition search
- Session 13: Ctrl+Shift+M froze the game. The 0.18.56 message template had 0x11C01041 (reaction bit) without HitSend's
  composition. Fix: capture() refuses 0x10000000 (and 0x74000000 overall), tplLoad drops such a template from the ini,
  the message path clears 0x10000000 defensively.
- strike(): recLoad before the template check; records usable without a message template; the message path refuses
  cleanly when it has no template.
- recLearn: defSearch through the moveset entry (mode 1 / 2) and the component's per-attack array (+0x5B0, 0x58 per
  attack, the moveset index; mode 3 / 4), one indirection each (≤ 0x400). Save format v3 adds defMode / defB. Flat
  dumps of both entries when not found.
- Self-test PASS (selftest=0).

## GoTVR 0.18.55 → 0.18.56 — records findable from Jin (two-level search, own definition cell); reacting message templates
- Session 12: recLearn found no component offset in hero (direct) and no holder in the moveset entry → nothing saved; after a
  restart the first real blow (a combo opener, no 0x10000000) became the record → no flinch.
- recLearn: component at hero+off or (hero+off)->+off2 (2nd level ≤ 0x800, heap pointers only); defJ = the definition
  pointer's offset in the moveset entry (HitSend 0x850880 and the composer 0x84B1A0 only read **(rec+0x50), so a rebuilt
  record gets its own cell R.cell = definition); save format v2 ("2;compOff;compOff2;o68Off;inComp;k50;defJ;vtRva;
  flags;damage;nTags;tags;hex"). Flat: `REC near` lines (heap pointers in component / owner / +0x68 object within 16 KB
  below the holder; holder / definition offsets in hero, component, owner).
- capture(): any ordinary blow ((fl & 0x41) == 0x41, no 0x64000000, damage in (-5000, -0.5)); a template with 0x10000000
  is never replaced by one without; the plainest-blow rule only within the same reaction class.
- Ctrl+Shift+M (hkReq 8): one strike with forceMsg (message path).
- Self-test PASS (selftest=0).

## GoTVR 0.18.54 → 0.18.55 — record library (flinch through attack data; insta-kill fix; saved for VR)
- Session 11: flinch with no animation from the record of attack 0x9561... (composed flags 0x11C01041); the 0.18.53 record
  (0x74bf..., 0x03C00041) never flinched. Insta-kills: a finisher record (0x03C00001, -FLT_MAX) became the template.
- hHitProc copies Jin's real record (0x120 bytes + its tag list) to a candidate before calling the game; noteHit records the
  flags / damage the game composed for it (candSeen); recJudge keeps it if (fl & 0x41) == 0x41, no 0x64000000 bits, damage
  finite in (-5000, -0.5); kind = fl & 0x100; a record with 0x10000000 is never replaced by one without.
- recLearn: comp found at a hero offset (scan 32 KB), +0x68 object in hero or comp, holder = moveset entry (owner +0x350,
  0xE0 each, by hash) + k, vtable rva; recSave writes `cbt_rec_light` / `cbt_rec_heavy` ("1;compOff;o68Off;inComp;k50;
  vtRva;flags;damage;nTags;tags;hex(0x120)"); recLoad at the first strike; recResolve uses the live pointers only while
  Jin is the same object, else re-derives them (class 24 / moveset entry checks) or refuses to the message path.
- Strike: rec+0xE0 (base damage) x the multiplier (speed, two hands, heavy fallback 1.6); kill = 1e7 (the game composes
  the lethal blow); +0x38 = 2; contact rebased (live) or the enemy's position + 1.2 m (saved record).
- Self-test PASS (selftest=0).

## GoTVR 0.18.53 → 0.18.54 — no attack animations; MSGDIFF + swing test; cbt_assist kept off; resolve max = hero+0x4644 + 3
- Session 10: cbt_mode 2 strikes identical to the real blow in message +0x00..+0xAF, DLVDIFF and events; no flinch.
- A first 0.18.54 build pressed the attack button and waited for "attacks in progress" (owner +0x350 / +0x358). Wrong on two
  counts: the owner wants no attack animations in VR, and that list is Jin's moveset (always non-empty - the blow would
  have struck at once carrying the first move's hash). Replaced before testing.
- Now: `cbt_assist` (default 0) = the animated method for comparison only: vrin::forcePress(X / Y, 90 ms) OR-ed after
  blockMask (padImpl / capsImpl answer in flat once pressed), the blow lands after cbt_assist_delay_ms, Jin's own HitProc
  records held back in the window.
- Diagnostics (flat): hDeliver1 dumps the whole delivered message for the first 8 real / 8 own blows and MSGDIFF (every
  qword of ours that differs from the latest real one). Ctrl+Shift+G swing test: the hotkey thread watches left mouse /
  pad X after arming; the blow follows by cbt_swing_test_ms with Jin's own blows held back.
- vitalsTick runs in flat too (logging only without the shared block). vitRes = hero+0x43A4 clamped to the maximum;
  vitResMax = hero+0x4644 + 3 (1..30 valid, else wrist_resolve_max). VITALS on change or every 10 s.
- rscan off by default (`cbt_resolve_scan`).
- Self-test PASS (selftest=0).

## GoTVR 0.18.52 → 0.18.53 — strikes through the game's HitProc (cbt_mode 2); resolve published
- Session 9 (0.18.52 flat): HITPROC + EVENT show the victim gets the same events from real and injected blows; the real
  blow additionally runs the attack-side callbacks (0x846290 / 0x872F00 / 0x8466B0 / 0x870FF0 → events 0x11 / 0x12 /
  0x14 / 0x15 and the target's +0x9C8 hit-ring flag). Still no flinch on ours.
- Fix: `cbt::strike` in mode 2 copies the captured hit record (recTpl), sets +0x48 = enemy, +0x58 = hero, rebases the
  contact point, pins the reference count, and calls the original HitProc with `injecting` set. Falls back to the message
  path until a record is captured. The HitProc hook is installed whenever its signature passes (flat and VR).
- Safety (from the HitProc / HitSend decompiles): HitProc skips a target whose +0xE2 bit 4 is clear (logged, refused);
  it pops the hit-message pool with no empty check (the mod checks 0x1434C34F8 first and takes the message path when it is
  empty); the descriptor's tag list (+0xF0 / +0xF8, read by HitSend, copied by the composer) is heap memory of the real
  record, so it is copied into the mod's own array at capture. +0x78's vtable is checked too.
- The EVENT window (evUntil) is now opened before the injection, so our own blow's events are logged.
- vitalsTick publishes vitRes = hero+0x43A4 (0..30, finite) and vitResMax = `wrist_resolve_max` (10), read before the
  change tracker so resolve gain / spend edges fire; `VITALS` log line every 5 s with hero+0x4380..0x43BF.
- Self-test PASS (selftest=0).

## GoTVR 0.18.49 → 0.18.50 — message parity, delivery diff, data watchpoints, quieter resolve scan
- Session 7 (0.18.49 flat): strikes through Deliver2 (13-15 dwords changed, including +0x1688 / +0x168C / +0x1698), still no
  flinch. MSG ours vs real (light):
  - +0x168 = 0 vs Jin (flag 0x800000 cleared by the 0.18.47 rule when the template had no weak handle);
  - +0x1A0 = 0 vs 1 / 2;
  - +0x50 null (ini template).

  Jin's heavy blow also carries a +0x1C8 entry (count 1).
- Fixes:
  - +0x168 = Jin whenever the template's kind has the flag;
  - +0x1A0 ≥ 1 with flag 0x02000000;
  - +0x50 from wkAny50 (any real blow of Jin's, class 24).
- Diagnostics (flat):
  - MinHook on Deliver 0x85A770 (signature-checked) → DLVDIFF for the first 8 real and 8 own blows;
  - dbw:: hardware watchpoints (DR0-DR2 = enemy +0x1694 / +0x1698 / +0x16A4, write, 4 bytes) set on every game thread from
    the hotkey thread (suspend → set context → resume); a VEH logs RIP + RtlCaptureStackBackTrace; off after 30 writes /
    90 s / the hotkey.
- RSCAN: a per-field decaying busy counter (+64 per change between 250 ms snapshots, −⅛ per snapshot, busy ≥ 40 is left
  out); Ctrl+Shift+R mark.
- Self-test PASS (selftest=0).

## GoTVR 0.18.48 → 0.18.49 — the game's delivery; resolve finder
- **Session 6:** HHDIFF real and ours change the same 11-12 dwords (health, the dedupe ring, the history), and no reaction
  follows. A real blow arrives via 0x59E670 → 0x85AE40 → HitProc 0x8504D0 (composes with 0x84B1A0 + 0x850880, notifies the
  attack components at hitRecord+0x60 / +0x78) → Deliver2 0x85A4F0 (the +0x1F8 chain, replication when target+0xB8) →
  Deliver 0x85A770. Deliver:
  - sets +0x28 = 3 and keeps the message in the latest-hits ring (64, 0x1434C3540, with a reference);
  - fires event 0x16 on the target and 0x17 on the attacker (0x7E05F0(obj, id, 2, 1));
  - flag 0x08000000 → 0x9A8B90 (a shake / slow-down);
  - calls the target's vtable+0x1D8 = HandleHit (classes 14 / 15 / 16 / 17 all point to 0x694080);
  - flag 0x400 → impact effects (0x6B2790 with +0x170 / +0x190 / +0x1A8 / +0x1B0 / +0x1B8).
- The game's own simple sender (the resolve heal, 0x68CE70) does pool → 0x84B390 → 0x858F20(amount) → 0x859800 → 0x85A4F0
  → release. That is the mod's path, so the mod now calls 0x85A4F0, signature-checked (`cbt_deliver`).
- hasHandleHit checks vtable+0x1D8 exactly. An `injecting` flag keeps our own blow out of the template capture and the
  diagnostics.
- **Resolve:** the heal is a script native (0x711FE0 is the script VM's native-call dispatcher), so the spend is scripted
  too. The runtime finder instead: RSCAN snapshots the hero (32 KB) and the first 4 KB of every object it points to
  (≤ 640) every 250 ms in flat play, and diffs them at resolve heals (flags 0x40000001), revives (0x04000001) and Jin's
  kills, at once and 0.6 s later.
- Self-test PASS (selftest=0).

## GoTVR 0.18.47 → 0.18.48 / GoTVRHost 0.14.4 → 0.15.0 — session 5 fixes; attack parameters; wrist HUD W1
- Session 5 (docs/COMBAT_FINDINGS.md):
  - Damage and kills are exact; the pool is clean; everything runs on the game thread.
  - No reaction: the attack side is missing.
  - Too strict a hostility gate.
- **Hostility:** the attitude query 0x9552F0 (table 0x17F7560, faction at character+0x600, 2 = hostile; the threat test
  0x950C40 uses it), signature-checked; without it, the 0.18.47 rule. Hotkey refusals are always logged.
- **Templates:** only flags 0x03C00041 (light) / 0x03C00141 (heavy). Session 5's first capture was a 0x11C01041 chip.
  The plainest blow wins: least damage, then least posture.
- **Hit reaction:**
  - `cbt_msg_state` (+0x28 = the template's 3 during delivery, 0 restored before the release so the id is still freed).
  - Flat diagnostics: HHDIFF (16 KB of the enemy before / after HandleHit, real vs ours), MSG dumps, the real blow's
    caller chain.
- **attack.h** (self-tested):
  - Swing recognition: body-relative tip speed + travel over 150 ms, re-arm under 0.8 m/s.
  - Heavy charge: two hands + trigger ≥ 0.6 held still `chargeMs` within `chargeMoveCm`, kept while held, lost 250 ms
    after release, spent by a blow.
  - Lunge planning: base × (1 + mod × speed fraction), cooldown, stop short of the enemy.
  - Katana weight: a critically damped direction lag, weight × 60 ms one-handed, ⅓ two-handed.
- **Lunge = a timed stick push** toward the enemy's world yaw (vrin::lunge, same mechanism as the room-scale drive).
- **Vitals → host** (shared v16: vitSeq, hp, max, res, changed bits); the heartbeat runs game-side on the left controller.
- **Host wrist (xrwrist.h):**
  - A second UI-size swapchain; the HUD rectangle is copied from the UI texture each frame it is shown and cut from the
    room panel's copy with ClearRenderTargetView(rect).
  - A quad on the left grip: +X out of the back of the hand, 0.11 m toward the elbow, 0.045 m out; picture right toward
    the hand, aspect corrected for the anamorphic UI.
  - vit::Glance, vit::HandAtHead.
  - The host now logs every runtime extension.
- **Self-test PASS** (selftest=0): + the attack core, + the vitals core.

## GoTVR 0.18.46 → 0.18.47 — combat stage 1: katana blows as the game's own hit messages
- Read from the binary (Ghidra; this build only, every function signature-checked):
  - **The hit message:** a 0x210-byte ref-counted object (+0x08) from its own pool. 0x7686D0 allocates one; the deleting
    destructor (vtable +0x10, 0x84BD70 → 0x84B800) returns it.
  - **Constructor:** 0x84B390(msg, target) = base + vtable 0x11F3B70 + a fresh id (NextId 0x9321F0) + the target handle.
  - **Weak handles:** +0x30 / +0x48 / +0x50 / +0x168. 0x84BB50 sets one, registering the slot in object+0x10; the object nulls the
    slot when it dies.
  - **Corrections to 0.18.43's layout:** +0x58 and +0x78 are tag lists (ptr, count +0x60 / +0x80, cap at +0x14); 0x519DB0 copies
    one. So +0x60 / +0x80 are list counts, not "level" or "category".
  - **Other lists:** +0x1C8 (stride 0x18, count +0x1D0) and +0x1E0 (stride 8, count +0x1E8).
  - **Flag payloads:** flag 0x01000000 → 48 bytes at +0x170; 0x02000000 → +0x1A0; 0x800000 → the handle at +0x168.
  - HandleHit is slot 0 of a secondary vtable. The message composer is 0x850880.
- **Blow injection** (cbt::strike), run on the game thread after the tick:
  - Build: pool → constructor → our reference → attacker = Jin → +0x50 / +0x168 from the template's weak handles → tags copied
    with the game's own copy → template numbers and payload. Payload positions are re-based on the target.
  - Deliver: damage × speed factor, with a killing blow sent as 0x40 cleared and −FLT_MAX. Then HandleHit (the trampoline), then
    release.
  - **Fails closed when:** a signature misses, any HandleHit runs off the tick thread, there is no template, or a character
    doesn't use this HandleHit.
- **Template:** Jin's plainest live light hit this session; saved to gotvr.ini as hex.
- **Enemy registry:** 48 slots held as the game's weak handles, filled from HandleHit (attackers of Jin and Jin's targets =
  hostile; spawns = known only).
- **Decision core** (melee.h, after HAND-004):
  - Closing speed at the contact along the target normal, relative to the target, minus Jin's own velocity.
  - A swept tip; hysteresis per target (re-arm after leaving) plus a global cooldown; sample gaps over 100 ms restart the
    history; a 150 ms velocity window.
  - Counters: evaluated, too slow, too short, cooling.
- **Research:** docs/MELEE_REFERENCES.md (Bloodborne VR, God of War VR, BotW BetterVR).
- **Self-test PASS** (selftest=0, 52 lines PASS) including the decision core: one hit per fast pass; no hit when held inside,
  pushing slowly (1.5 m/s) or walking the blade in (body velocity); a hit when the enemy runs onto the blade, after re-arm, and
  on a thrust; no hit across a teleport; a full table reuses the stalest entry.

## GoTVR 0.18.45 → 0.18.46 — session 3 (parry / standoff / traversal); enemy watcher on real hits only
- Session 3:
  - Parry: Q +64 ms -> 0x1 (+12.5); perfect: Q +22 ms -> 0x1, then a one-blow kill.
  - Standoff: +0x1070 = 1, +0x1084 = 305, +0x106C ref, +0x107C = 8.
  - Traversal plane: +0xB60..0xB98 (13 of 14 changes in the tag, 0.5 - 0.9 s after step-on).
  - The dog = class 16.
- Bug: the enemy watcher followed the spawn messages (0x60000002 set max health on every spawning enemy). It now needs a real hit:
  flags without 0x60000000, with 0x41, and an attacker / target other than Jin.
- Self-test PASS (selftest=0, 128).

## GoTVR 0.18.44 → 0.18.45 — session 2 decoded, enemy watcher, hero watch 32 KB
- Owner session (0.18.44): 31 tags, 180 marks, 310 hit messages. Caps Lock on (no effect: GetAsyncKeyState). Dogs present, not
  fought (class 16 = health inits only).
- Decoded (docs/COMBAT_FINDINGS.md "Session 2"):
  - The hitInfo layout (+0x38 damage kind, +0x40 move id, +0x48 attacker, +0x60 level, +0x80 category, +0x90 flags, +0x94 health
    change, +0x98 posture).
  - Flag kinds: parry = 0x1 (+12.5 on Jin), stagger 0x200, perfect-parry kill 0x401001, crawl 0x2000.
  - hero+0x3918 / +0x39A0 corrected: paired actions, not incoming attacks (7 events: finisher click +12 ms, horse mount E, ...).
- obs: NB 0x4100 -> 0x8000 (the test buffer to match).
  - Enemy watcher (enemyPtr from cbt::hHandleHit: the target, or hitInfo+0x48 for hits on Jin; 16 KB, the same filters, ENEMY/s).
  - The COMBAT/s label fixed.
- Self-test PASS (selftest=0, 128). gnu_printf check clean.

## GoTVR 0.18.43 → 0.18.44 — capture tags for the design doc, keyboard / mouse input logging
- Owner: the updated design doc (combat rework, archery / gadgets, specials, assassinations, traversal, UI). "I can continue acquiring
  flat screen data - let me know what you need." Ledges / ropes / logs: first person, Jin's own animation takes over.
- obs: 23 tags (kTag / kTagKey; Ctrl+numpad for the second bank; numpad / * - as extras).
  - keysTick samples 27 keys plus WASD every tick and logs edges with ms (INPUT/s), because the 0.18.42 session's pad capture read
    0x0000 (keyboard / mouse play).
- docs/DATA_CAPTURE_PLAN.md: what to capture per design item, the priorities, and what needs no flat data.
- Self-test PASS (selftest=0, 128). gnu_printf check clean.

## GoTVR 0.18.42 → 0.18.43 — combat observer (SetHealth / HandleHit hooks), right trigger = Focus
- Owner: "Parse this log data to figure out the different states and how we can hook into it" (attacking logs: special ability,
  perfect parry, regular parry, attacking, getting hurt; no standoff). "Set right trigger to be Focus when not holding a kunai."
- Analysis -> docs/COMBAT_FINDINGS.md.
  - Health is hero+0x1974 (float). Its sole runtime writer is 0x69F3D0 (SetHealth), found by scanning for disp32 0x1974 writes (one
    besides the constructor's 100.0).
  - Callers include 0x694080 (HandleHit; no direct calls, 4 vtables).
  - The incoming-attack list is +0x3918 (count +0x39A0).
- cbt::install: separate signature checks (fail closed for these two only) and MinHook detours.
  - SetHealth(u8*, float) and HandleHit(u8*, u8*) -> u64 passthrough, logged at 40 lines/s at most, with a hit-record hex dump.
  - obs: COMBAT/s.
  - A gnu_printf check of every logf is clean.
- vrinput: right_trigger default "back" (Focus).
- Self-test PASS (selftest=0, 128).

## GoTVR 0.18.41 → 0.18.42 — catch at the left hand, wider catch, auto-sheathe timing persisted, draw mask to the end, play_mode key, observer floats
- Owner:
  - "The sheathing is working, but where it registers the sheath mouth still isn't quite right. Increase the radius and put it more where
    my left hand sits on the sheath."
  - "The animation to sheathe still plays occasionally."
  - "I can't find play_mode = flat in the ini."
- Log 0.18.41:
  - 7 catches, all at 29 - 32 deg. The misses were tips at +1..+8 cm along / 2 - 10 cm off but 37 - 46 deg -> the angle was the blocker.
  - The game's put-away was at 16.3 s every time. The first one in a session was not predicted (only detected).
  - The background redraw mask was a fixed 1.8 s from the request (the draw lands late).
- Catch:
  - mAx = the axial of the tracked left hand while onSheath (clamped +-10 cm of sheathG() + sword_sheathe_shift_cm), else that place.
  - The defaults in the ini: tip 12 cm, 55 deg, sheathOutCm 12. tipLen still comes from the real mouth (guard - 4.5).
- Mask:
  - holdMask(200) while gd for its first 1.8 s (any draw's own animation). After put-away 1.6 s (was 1.2). Draw request maskUntil +3 s.
  - autoAfterMs is saved to sword_game_autosheathe_ms and loaded at start.
- play_mode is written as "vr" when missing.
- obs: decimal values (finite, 0.01..1e5) that change on quiet fields are logged as floats beside the small-int flags.
- Self-test PASS (selftest=0, 128).

## GoTVR 0.18.40 → 0.18.41 — tip length, the game's auto-sheathe masked, the thumb spring restored, numpad tags
- Owner:
  - "The sheathing did not work at all - tip at the sheath mouth, nothing happened, no rumble."
  - "Sometimes the sheathing animation still played."
  - "The thumb pose is not responding to my thumb on the sticks."
  - "Numpad 1 sword combat, 2 archery, 3 climbing, 4 grappling, 6 cutscene" (flat-play logging).
- Log 0.18.40:
  - No catch ever. tipAx -45..-48 / 6..12 off / 15..20 deg on the aligned tries: the computed tip (guard - 72 along the blade) was 45 cm
    deeper than the real one. Draws free at s = 43..48 -> the blade's out-of-mouth travel is about 45.
  - The draw was re-requested at 525234 -> 543531 -> 561828 -> 580109 (18.3 s): the game's own out-of-combat auto-sheathe (confirmed:
    Push Square, "Jin sheathes his sword automatically after combat, but only after he has kept it out for a while").
- Tip:
  - cfg.tipLenCm = sword_blade_out_cm (45) - mouthAx. tipW = guard - dir * tipLenCm. The sheathing catch s = tipLenCm + axial(tip).
  - SWORD/s logs the per-second closest approach (score = perp + window overshoot + 0.3 * angle over).
- Auto-sheathe:
  - gameSwordScan: gd, more than 3 s since gd rose, and 637 (the game's pose) within 35 cm of 592 = the game putting it away -> maskUntil
    held +0.9 s each frame. autoAfterMs is learned (the first / shortest).
  - The predicted window [drawnSince + autoAfter - 1.5 s, + 4 s] is masked. On put-away the mask holds 1.2 s, then the background redraw
    (hold) masks itself.
- Gap: homeOk, park found and not gameDrawn while the katana is out -> the sheath is not moved (up to 3 s, then the old fallback).
  nGap logged.
- Mask: walking (not still) leaves the hip subtrees (legsD) to the game - the upper body is held.
- Thumb: the 0.18.31 index-tween comment ran to the end of the line and swallowed `fing::spring(thS, thV, tht, ...)` (the build warned:
  unused tht / thV). Restored. A gnu_printf format check of every logf is clean.
- obs: setTag (numpad 1-4, 6) with BEGIN / END banners, and the tag in every STATE/s line.
- Self-test PASS (selftest=0, 128).

## GoTVR 0.18.39 → 0.18.40 — no game sheathe (player-driven sheathing), "not animated" = put away, tip at the real mouth, sticky drawn grip
- Owner:
  - "I want sheathing to be player driven, not an animation - once the sword tip reaches the sheath mouth (around where the left hand
    default sheath placement is (4.5 cm)), the player must slide it back in."
  - "An animation plays, and the end of the animation is affecting Jin's pose."
  - "It keeps playing the sheathing animation multiple times, which stops me from moving properly."
  - "The unsheathing works well."
- Log 0.18.39:
  - Parked at 0 0 0 (the game never writes 637 while its sword is away). homeX learned (0.1 cm from 592).
  - No "tip is in the sheath mouth" event ever: every sheathe was "let go of the drawn katana - back in the sheath" (instant return),
    then the game's sheathe 1.5 s later.
  - Then "put away" every 2.5 s (444921 / 447437 / 449953 / 452468 / 454984). Each time 637 went 24.9 -> 0.4 -> 0.0 cm from 592 and then
    back to the stance hand (57 - 61 cm).
  - Cause: once put away, 637 was not animated and held the MOD's last write (home, not the park) -> read as drawn -> another toggle.
- Fix:
  - Scan: 637 bit-identical to what the mod left in that pose buffer (wroteM / wroteBuf, recorded at the end of every job) for 3 poses
    = not animated = put away.
  - The game-thread sheathe chord is removed. Draw chord: (hold) and not drawn, once per 2.5 s, park found.
  - maskStance needs inputQuiet and the head within 20 cm of maskRefStand (crouch / aim show through).
- sword.h:
  - mouthAx (guard - sword_mouth_below_guard_cm). Tip window: perp < 8, -25 < ta < 8 (relative to mouthAx), < 35 deg.
  - The catch keeps s = bladeCm + axial(tip) (up to bladeCm + 40, no clamp jump). Pull-out = hand 6 cm back from the catch.
  - releaseHome 0 (sticky drawn grip; the SHEATHING release only on tracking loss). autoHome 0 (a released blade stays at s).
  - The DRAWING slide top extends to s0 when it started above bladeCm.
- Self-test PASS (selftest=0, 128). The draw test now covers the sticky grip, the no-home release, auto-home as an option and the
  real-mouth window (catch at 20 cm inside, none at 10 cm outside).

## GoTVR 0.18.38 → 0.18.39 — game sword (637) with the draw / sheathe animation masked, toggle-safe requests, two-hand steer halved
- Owner:
  - "Push to git first as (WIP) sword grabbing" -> f488e55 on main.
  - "The game is back to grabbing sword + sheath. I want to just grab the sword and have the sheath stay put ... I'm not saying don't
    draw the sword, but I don't want the animation to play (or at least have any visual effect / effect on Jin)."
  - "When dual handing the sword angle changes too much - make it half."
  - "Lag / jitter in the headset when I look down; the performance monitor shows no dropped frames."
- 0.18.37 log evidence:
  - d637 (game pose, 637 to 591): about 107 cm while sheathed (parked), about 58 cm in the drawn stance, and 24-25 cm in the frames
    where the game draws / sheathes. That is |592 - 591| = 24.9, so 592 is the sheathed sword's pivot and 637 passes it when home.
  - The "asked again" resends toggled the game back (draw -> sheathe at 557151 / 557154 / 557166). The swipe is a toggle.
- Job thread: gameSwordScan (the raw pose, right after snapTake).
  - park = 637 still (< 0.3 cm per pose) and more than 80 cm from 591 for 30 poses. gameDrawn = more than 15 cm from park (back
    within 5 cm = sheathed). Before the park is found: the old 90 cm rule.
  - homeX = inv(P591) * P637 from the frame where 637 is within 4 cm of 592 (the best kept), saved as sword_game_home_* / _cm.
- Job thread: maskApply, before the arm handover gate and the idle hold.
  - Replaces every joint except the 637 subtree with maskRef (the last fresh pose with the game's sword parked; maskRefStand when still),
    eased 0.15 s in and 0.3 s out, and the world matrices are written too.
  - Active while now < maskUntil (the game thread: draw request + 1.8 s, sheathe request + 3.5 s, extended while the game is still
    drawn) or (maskStance and gameDrawn and still).
- swordApply gbMode (gameDrawn and homeOk):
  - SHEATHED: 637 = P591 * homeX (home in the scabbard until the game's sheathe is through).
  - Otherwise: 637 = (Mt * inv(Pb) * P591) * homeX, from the same Mt as before (rail / drawn K / blend). 591 is not moved.
- Game thread: hold = onHilt or not SHEATHED. want = hold or released less than 1.5 s ago.
  - The chord is sent only with park found, at most once per 2.5 s: draw when want and not drawn; sheathe when not want, drawn and we
    drew it.
  - maskStance = weDrew and gameDrawn.
  - SWORD/s: requests, masked frames, the closest 637 came to the scabbard, park found.
- sword_two_hand_steer (0.5) scales the two-hand turn (fbik::scaled(arc(a1, tg), k)).
- Look-down judder, diagnosis only (no change):
  - Host about 72 Hz (VDXR) against about 55 new stereo pairs/s (game about 110 fps, alternate eye), so about 17 frames/s are reused
    pairs.
  - The runtime's reprojection corrects rotation only, which fails most on near geometry (the body, arms and sword fill the view looking
    down). The compositor never drops a frame, so the monitor shows none.
  - Suggested: an even cadence (90 Hz with the game capped at 90 fps = each pair exactly twice, plus VD's SSW) or more game fps.
- Self-test PASS (selftest=0, 128).

## GoTVR 0.18.37 → 0.18.38 — no game draw / sheathe, TAA change reverted, katana frame lag while moving, guard-based hand placement, body under the headset
- `[HEADSET]` owner, photo plus notes: "STOP MAKING JIN'S SHEATHING ANIMATIONS PLAY ... IT IS PLAYER DRIVEN".
  - Jin no longer under the headset; not facing the headset direction while moving.
  - The TAA fix did not fix the ghost and looked worse.
  - The sheath vanished on the draw, and the grip was wrong (photo: the game's grip).
  - The blade was not angled with the sheath while drawing.
  - The grip placement ini settings did nothing (the right hand stays too far up the hilt).
- Log 0.18.37:
  - Draw / sheathe requested 3 / 4 times ("the game did not sheathe - asked again" repeatedly).
  - BODY/s while walking at 0.7 m/s: "Jin faces -5 deg" fixed (the game's drawn-sword strafe stance).
  - TAA/s: history restored 0 / saved 0 (the per-eye bank never ran on top of preset 7).
  - UNDER/s: held 55 frames/s with the hands near the hip.
- Removed: the 0.18.37 chords (draw / sheathe / resend / re-draw), the Ctrl+F9 chord, sword_game_draw (forced 0), sword_use_game_blade
  (forced 0) and the relHand load (ini keys deleted), taa_per_eye and Ctrl+F11's toggle (F11 recentres again).
- Frame lag: swd::frame read kGameW, the katana joint's WORLD matrix written by the previous animation job. Now the job also publishes
  the joint's MODEL matrix (kGameM) and kGameRead() rebuilds W with the game thread's current node. The DRAWN blade's Sfw.t (the
  mouth) comes from this job's W o mouthL (published as SwordPub.mouthL).
  - Moving, the 0.18.36 fist carry kept the lag: it preserved Mt.t - P0.
- Hand placement:
  - Evidence that the guard is not the rail origin (joint 591): the captured left (thumb on the guard) sits about 12 cm out along the
    rail. The right fist clamp max was hiltCm - 4.5 = 20.4 cm from 591, about 4 cm under the guard, whatever sword_grip_lower_cm said
    (200).
  - Now: guard = sword_guard_cm or auto (capAxial(left) + 4.5, else 16.5). hiltT = rightT() = guard + sword_right_hand_cm, clamped to
    guard + 4.5 .. guard + hiltCm - 4.5, applied every held frame. sheathG = guard - sword_left_hand_cm.
  - Captured grips are slid along the rail to those places (capShifted). Capturing writes the settings.
  - sword.h: Cfg.guardCm / rightCm / leftAx. dHilt over guard .. guard + hiltCm, dSheath from mouth + axis * leftAx.
  - The 0.18.35 grab radius widening is removed.
  - Live: Ctrl+PageDown / PageUp / End / Home (adjReq), which shift the active locks and drawLock by the change.
- underHold: only (not DRAWN and a hand on the hilt / sheath) or (DRAWN and the tip near the mouth). It was also within 25 cm, and the
  sheath held while drawn.
- Kept from 0.18.37: the T clamp (70 cm), the crouch / stand height follow, the two-hand release (squeeze < 0.35 or 90 cm), and no arm
  handover to the game while the katana is held.
- Self-test PASS (selftest=0, 128). The draw test adds the guard offset (the hilt grab zone, fist clamp, sheath grab centre).

## GoTVR 0.18.36 → 0.18.37 — the game's drawn sword (637) in the VR fist, per-eye TAA on preset 7, T clamp, crouch height, two hands
- `[HEADSET]` owner, photo plus notes:
  - Body teleports behind when moving back / right / left (the arms and sword with it).
  - The katana not anchored in the grip while moving.
  - Ghost copy of the arm and katana: up-left in the left eye, down-right in the right.
  - Confirmed the drawn-sword joint.
  - Stand-up camera too slow.
  - The two-hand hold breaks in swings.
- Log 0.18.36: Ctrl+F8 with the game's sword drawn -> 637 (sword_drawn_blade_joint). sword_grip_lower_cm = 200 (the owner's).
  UNDER/s small (< 9 cm). BODY/s while running backward: the drawn body 148..-163 deg from the node.
- Teleport: hideInPose's rigid-view term T = R(b) hzS - H0 failed closed to 0 past 40 cm. Running backward with the drawn body
  turned about 180 deg and the run lean about 20 cm, |T| is about 40 cm, so it flipped every frame. Now a continuous clamp at 70 cm
  (nTClamped).
- Ghost: the playbook ch14 temporal trap (alternate-eye TAA blends the other eye). taa_per_eye (default 1) sets taa::on = 3 after the
  preset is applied (the bank is filled by hStorePrev regardless of occ::on). Ctrl+F11 toggles it. F6 presets still set their own.
- Stand-up: the vertical anchor smoothing (0.35 s) blends toward a 0.06 s follow for height errors of 4..10 cm.
- Two hands: release only at squeeze < 0.35 or controllers > 90 cm apart.
- Game blade, job side:
  - gameDrawn = the game's own pose (g_animPose) has 637 within 90 cm of 591.
  - relHand = animHand^-1 * anim637, learned after 30 frames that agree (< 0.02 rotation, < 0.3 cm), saved as sword_game_grip_* by the
    game thread.
  - While gameDrawn && relHand: 637 = VR hand * relHand when the katana is out (sheathX = Sm^-1 * 637 recorded each frame), and
    Sm * sheathX after our sheathe until the game swaps back. 591 is left as the game draws it.
- Game blade, game thread: the draw chord on SHEATHED -> out, the sheathe chord on out -> SHEATHED (when gameDrawn), one resend after
  2.5 s, and a re-draw if the game sheathes by itself while DRAWN (4 s cooldown).
- Arms: the animation-raised-hand handover is skipped while the katana is out or a hand is on the hilt.
- Self-test PASS (selftest=0, 128).

## GoTVR 0.18.35 → 0.18.36 — katana carried by the solved fist, captured-left turn, two-hand hold, triggers blocked, drawn-state probe
- `[HEADSET]` owner, photo plus notes:
  - Left hand in the right place but inverted (the thumb should point up the sheath to the hilt).
  - The katana goes beyond Jin's hand when the controller is beyond his reach, and strays around the hand.
  - Two hands fall off too easily.
  - Probe: "the sword lifted alone on the Ctrl+F7 one AFTER the one that lifted the sheath, only with the sword already drawn by an
    attack animation".
  - Idle animations play while holding the sword.
- Log 0.18.35:
  - drawn-grip captures repeated (pitch -15..14 deg)
  - two-hand releases with the grip at 1.00 (geometry)
  - IDLE/s mostly "pad input" (498 of 559)
  - d637 went from 107 to 58-61 cm at about 512773 s (the game drew on an attack) and back to 107 at about 512791 s. The probe cycles
    during that window ran 637, 644, 651, 645 (cycle 2) and 591-with-592-held, 592, 637, 644, 651, 645 (cycle 3).
  - `[INFERENCE]` 591 carries the sheath (plus the sheathed blade); the game's drawn sword is a separate model on a joint the game
    animates. 592 (591's child) is the favourite: probe 1 lifted the sheath, "the next" lifted the sword. It is not confirmed yet.
- Fist-carried katana: solveArms stores the right hand's line target (P0, tD) when it snaps to the drawn katana. swordApply then turns
  the katana by arc(tD, the measured fist D) about P0 and moves it so P0 -> the measured fist G (nKatFistFix, katFistErrCm).
- Captured left: no placement snap; a rotation-only snap about G with index -> little = -axis (index toward the hilt); the thumb aimed
  straight along +axis.
- Two hands: release only at squeeze < 0.35, perp > 30 cm (beyond 8 cm along), or outside -12..55 cm.
- blockMask adds OX_RT / OX_LT (vrin zeroes the triggers): no R2 / L2 and no lastAct from the trigger while the katana is held.
- Probe lines report the game's sword state (d637 < 90 = drawn). Ctrl+F8 while drawn saves sword_drawn_blade_joint (not yet used).
- Self-test PASS (selftest=0, 128).

## GoTVR 0.18.34 → 0.18.35 — drawn-grip capture, WYSIWYG captures, swing filter, the 0.18.26 candidates in the blade probe
- `[HEADSET]` owner:
  - Photo: holding the controller forward, the katana points left. Asks for 90 deg about x / 90 deg about y.
  - Captured the left grip (Ctrl+F6) and wants it used.
  - The katana lags the hand in swings.
  - Ctrl+F7 never lifted the sword alone ("when I first found the sword bone it was raised alone").
- Log 0.18.34:
  - forward latched as row 2 x row 0
  - all 25 blade probes run 3 times, no Ctrl+F8
  - left capture at 13.0 cm out along the axis, 4.3-4.4 cm off the line
  - sword_grip_lower_cm set to 20 by the owner
  - no Ctrl+F9 census this run
- Axis guessing abandoned: 0.18.31 / 32 / 33 each mapped "forward" to a different row combination and none matched in the headset (the
  fist-channel sign flips made the photos ambiguous). Instead:
  - **Drawn capture:** Ctrl+F5 while DRAWN. The target blade frame is pommel = -view forward (headF + pitch) and edge = -view up
    (with the sheath frame's local world-up as the edge, worn edge-up; sword_edge_flip).
  - drawRot = C.R^T * frameTo(...) is saved (sword_drawrot_*). With it, K1.R = C.R * drawRot, the hilt goes through the grip point
    at hiltT, and it blends in with fwdT from the as-taken pose.
- WYSIWYG captures: a captured hand is not fist-snapped on the rail (sp.capNoSnap). The rigid lock reproduces the hand exactly as it
  was drawn at the capture. The sheath grab radius is max(12, |capture| + 6) cm.
- Swing lag: stab.h's derivative low-pass (dHz 1) and rotBeta 3 lag fast rotations, and the blade multiplies them by its length.
  While DRAWN / SHEATHING (right) or two-handed (left) the filter uses dHz 10, rotBeta 25, beta 30, rotMinHz 2 (nSwingStab).
- Blade probe: + 645, 646, 647, 653, 654, 652, 593, 594, 586-590 after 637 / 644 / 651. 645 (the parent of 646 / 647) was never in the
  0.18.29+ lists.
- Self-test PASS (selftest=0, 128).

## GoTVR 0.18.33 → 0.18.34 — the drawn-sword bone probe and a 50 ms draw census
- 0.18.32 census, which I first misread: Ctrl+F9 (the game's draw) moved only root children 637 (99 cm), 644 (65), 651 (53), and
  652-654 (22-27, the right-hip gear). The katana joint 591 did not move. No joint changed scale (a model swap or attachment change, not
  bone hiding). The right hand's subtree (561) is only the fingers: no weapon socket under the hand.
- Hypotheses:
  - (a) the sheathed katana is one model on 591, and the drawn one a second model on 637;
  - (b) the sword is a separate attachment that sits on 591 when sheathed and is re-attached to 637 on the draw.
- Probe: the blade candidates now start with 591's children, then 637 / 644 / 651, then the nearest others (up to 24).
- Census: 2.5 s window, 50 ms samples of 591 / 592 / 637 / 644 / 651 / the right hand from the game's OWN pose (snapTake's copy,
  g_animPose). Each sample has the position from the root, the movement since the press, 637 in the animated hand's frame and 637 in
  591's frame. Before the press, 637 / 644 / 651 / 592 are logged in 591's frame; at the end, the full diff. SWORD/s logs 637's distance
  from the katana joint.

## GoTVR 0.18.32 → 0.18.33 — grip captures, the drawn katana from the controller, two-hand gate without feedback, ini defaults
- `[HEADSET]` owner, two photos plus notes:
  - Right hand on the sheathed hilt: knuckles on the wrong side, still too high, "can't find the setting".
  - Left: the grip is wrong and too low; asks for a hotkey to capture the hand in place.
  - Two hands: the left kept grabbing above the right and jittered out.
  - Photo 2: controller held forward, the katana across it to the right.
- Log 0.18.32: "left hand took the hilt too" / "let go" alternating every 50-150 ms with the grip at 1.0. The gate used the published
  drawn katana, which the steering itself moves: a closed loop (playbook ch02, "the pose you draw and the pose you steer with are
  different variables"). No Ctrl+F9 census was run.
- Axes, from the photos: 0.18.31's blade along row 0 pointed up; 0.18.32's along -row 2 pointed right. So the controller's forward is
  row 2 x row 0 (left x up), with its sign latched the first time the blade comes free against the head's facing (sword_blade_sign
  overrides).
- The drawn katana is now the game thread's:
  - Kt = grip o drawLock^-1 (the sheathed grab: the fist's tube along the hilt, as in 0.18.30, or the capture).
  - K1 = Kt turned about the held point (guard + axis * hiltT) so the blade runs along the controller forward, eased in over 0.25 s
    after DRAWN.
  - K = K1 turned toward the raw hand line (twoSteer) while two-handed.
  - K is published (sp.kat*). The job sets the blade joint to N^-1 K Sf^-1 N Pb (the joint as it sits in the sheath, carried by K)
    and eases from the rail pose over 0.18 s at DRAWN.
  - The 0.18.30 "katana through the measured fist", the 0.18.31 / 0.18.32 wrist rotation and the job's drawn-pose publish are removed.
- Fist snapping: one rule per hand. The right hand goes on the drawn hilt at hiltT (sp.dSnapW) when drawn, else on the rail at
  s + hiltT. The left hand goes on the rail at sp.sheathG (capture or sword_sheath_hand_cm), or on the drawn hilt at hiltT + twoGap.
- Two-hand gate on K1 (unsteered, the right controller only): the left grip within 12 cm of the line, 4..30 cm behind the held point,
  squeeze 0.65 / 0.45; leaves at 20 cm off the line or outside -3..40 cm.
- Captures (Ctrl+F5 right / Ctrl+F6 left, 3 s countdown with ticks, Ctrl+Shift clears): the controller pose relative to the sheath
  frame (Sf: the katana joint's axes at the mouth), saved as sword_cap_{r,l}_0..11 + _ok.
  - A grab with a capture locks to it; the right capture also gives hiltT (its axial position) and drawLock.
  - The left capture's axial position is the left fist's place on the sheath.
- ini: missing katana keys are written with their defaults at load (sword_grip_lower_cm now 10, sword_sheath_hand_cm now -3).
- Self-test PASS (selftest=0, 128).

## GoTVR 0.18.31 → 0.18.32 — blade along grip -Y, lower grip, sheath hand fixed, two-handed hold (HAND-017), drawn pose published, draw census
- `[HEADSET]` owner, two photos plus notes:
  - Corrects 0.18.31: wants the knuckles on the side, so the katana faces forward when the controller is held forward. Photo 1: the
    drawn katana points up and the right fist sits right under the guard.
  - The left hand is too far down the sheath.
  - Ctrl+F7 with 592 held: the sword never moved without the sheath.
  - Wants a two-handed grip.
- Log 0.18.31: rail from the two joints taken (592 at 24.9 cm, 11 deg from the calibrated line; calibrated mouth 8.3 cm from 591).
  Wrist-forward ran; the fists snapped.
- Blade direction: the grip pose's -Z is the fist's tube (a rod in a neutral fist points up), so 0.18.31 put the blade up.
  - Now the blade is grip -Y (out of the front of the fist, OpenXR grip axes), pitched by sword_blade_pitch_deg about grip X. Published
    as sp.bladeW.
  - The wrist target puts the fist channel (index -> little = the pommel side) on -bladeW.
  - The sheathed-grab lock aligns bladeW (not grip -Z) to -+ the rail axis.
- Grip: hiltT += sword_grip_lower_cm (6) at the grab. The joint rail's mouth is 591's origin, 8.3 cm above the calibrated one.
- Left on the sheath: G at sword_sheath_hand_cm (-6) below the mouth. The thumb-length placement (CMC to the extrapolated tip, about
  12 cm) is dropped; the thumb is still aimed at the guard.
- Two hands (swd::frame):
  - Gate: DRAWN, the left grip within 10 cm of the drawn hilt at t in (hiltT + 3, hilt + 6) from the guard, squeeze 0.65 / 0.45; leave at
    17 cm or out of the region. Pulses on hover / take.
  - sp.twoW (0.15 s ease), twoGap = clamp(t - hiltT, 8, 16), raw grips.
  - Job: the arms are solved right first. The left target is the snap onto the right fist's channel line at G1 + D1 * gap, with the
    hilt finger table.
  - The right wrist target (pommel side) is turned toward the RAW hand line (left - right) by twoSteer: 1:1 to 55 deg, asymptotic to
    90 deg, faded below 6 cm apart.
- Drawn pose: swordApply publishes the drawn katana's guard and pommel direction in world (seqlock, 200 ms freshness). Sheathing and
  the two-hand gate use it; the fallback is grip o drawLock^-1.
- Census (Ctrl+F9): sends the game's draw toggle and logs the right hand's subtree (joint, parent, scale). 1.5 s later it logs every
  joint whose scale changed > 0.3 or that moved > 20 cm.
- Self-test PASS (selftest=0, 128) + twoSteer (30 deg followed, 120 deg compressed below 90, fade 0); grip-lower in the reach test.

## GoTVR 0.18.30 → 0.18.31 — rail from the katana joints, blade-forward wrist, sheath-hold probe, body held near the katana, index tween
- `[HEADSET]` owner, two photos plus notes:
  - The right grip angle is right, but the fist is a little to the side of / off the hilt.
  - Wants the right wrist at 90 deg so the knuckles are on top and the drawn sword points straight forward.
  - The left grip is right, but the hand is not on the sheath.
  - Blade probe: nothing lifted the sword alone.
  - New: "Jin's body sometimes moves behind me to the right".
  - Draw "a tiny bit longer"; the index tween too slow.
- Log 0.18.30:
  - Blade candidates: 591 (parent 5) has one child, 592 (25 cm); then 593 (parent 5, 9 cm), 509-516, 534/535, 541/542, 646/647. The owner
    cycled all 16.
  - UNDER/s jumped to 52 cm ("your head 57 cm from Jin's", speed 0) while both hands were 7-20 cm from the katana: the body-under shift
    followed a lean to the sword.
  - DRAWN at 36-37 cm out with 0.5 / 0.9. Fists snapped (finger circle radius 2.2-2.4 cm), left thumb aimed, katana through the fist.
  - The draw state flickered at the 1 cm threshold.
- Rail from joints `[INFERENCE]`:
  - The calibrated line (mouth (-3.6, -3.3, -6.7), axis (0.005, 0.188, -0.982), hilt 29 cm, in 591's frame) passes about 5 cm beside
    591's origin: the controller grips are not the drawn hilt.
  - The job publishes 591's child (592) in 591's frame (seqlock). railFromJoints uses the 591-592 line when the child is 15-40 cm away
    and within 35 deg of the calibrated axis: mouth = the end nearer the calibrated mouth, hilt = their distance. Logged once with the
    verdict; sword_rail_from_joints.
- Blade forward: solveArms, right side, before the sword snap, weight wristW (0.15 s ease while onHilt / DRAWN / SHEATHING): the
  predicted fist channel D is turned onto -controller fwd (grip -Z, the real fist's axis) about G.
  - Jin's hand mapping (0.18.10) has his fingers along grip -Z, so his fist channel was perpendicular to the real fist.
  - The drawn katana (along D) now points along the controller. sword_blade_forward.
- Probe: blade mode lists "the katana lifted with descendant X held still" first (probeHold; swordApply saves / restores X's subtree
  around the lift). Ctrl+F8 there saves sword_blade_joint = the katana joint and sword_keep_joint = X.
  - The draw restores sword_keep_joint's subtree after moving the blade (generalising the 0.18.29 keep-sheath).
- Body held: fp::underHold while not DRAWN with a hand on / within 25 cm of the sheath or hilt, or DRAWN with the left hand on the
  sheath / the tip near the mouth. The body-under target is frozen (sword_hold_body_still).
  - UNDER/s adds the offset in the drawn body's frame, the largest move in a second, and the held frames.
- Draw: freeMinFrac 0.6, freeExt 0.93 (sword_free_min_frac / sword_free_arm_reach). Out / in hysteresis 1.5 / 0.5 cm.
- Fingers: the index curl has its own spring, fp_index_tween_s 0.07 (the others keep fp_finger_tween_s 0.15).
- Self-test PASS (selftest=0, 128) + rail from joints (along: used, nearer end = mouth; across: refused; too close: refused). Updated
  the reach test to the 0.6 rule.

## GoTVR 0.18.29 → 0.18.30 — fist-channel snapping, left thumb on the guard, katana through the fist, one-hand draw, start-up turn kick
- `[HEADSET]` owner, three photos plus notes:
  1. The right fist sits beside / above the hilt.
  2. The left hand is hidden under the belt armour, below the mouth. Wanted at the mouth, the thumb outstretched along the sheath's
     ridge with its nail on the guard.
  3. The drawn katana (joint 591, with its sheath: the blade joint is not picked yet) floats about 25 cm from the fist.
  - Also wants a right-hand-only draw.
  - Movement forward "not set properly / keeps moving until I do a snap turn"; asks for two unnoticeable start-up snap turns.
- Cause of 1-3 `[INFERENCE]`: the lock placed the controller grip point on the line. Jin's fist centre is offset from it by the hand
  trims (back 6 / up 5 / side -3 cm, pitch 20 / yaw 10 / roll -10). In 3, Trel was captured while the hand target rode ahead of the
  real hand by the draw gain and may not have been reached.
- Fix, fist channel (job thread):
  - After fing::apply each frame (hold weight > 0.5): the circumcentre of each long finger's three bending joints (a finger wrapped
    round a cylinder has them on a circle about its axis). Their mean = G; index -> little = D. Kept in the hand joint's frame too.
  - Before the solve (snap weight from the game thread's lock ease x hold): Rh = frameTo(A.fl, A.tl, T.fing, T.thumb) predicts the
    hand frame. Rc = arc(D, +-axis) turns the target, and the wrist moves so that G lands on mouth + axis * gAx.
    - right: gAx = s + hiltT (hiltT clamped 4.5 cm from the guard / pommel)
    - left: gAx so that the thumb, laid along the surface (radius sword_sheath_radius_cm), reaches the guard
      (-sword_thumb_gap_cm)
  - After the fingers, the left thumb chain turns at its first bending joint toward that surface point at the guard.
- Fix, katana in hand: at the first DRAWN frame the hilt line and the held point (s + hiltT) are stored in the blade's local frame.
  Each DRAWN frame, the blade (Ph * Trel for the roll) is turned so the hilt lies along D and moved so the held point is at G.
- One-hand draw: needSheathHand default 0. The blade is freed at s >= 0.5 blade with the right arm's reach >= 0.9 (|target - shoulder|
  / (upper + lower), published by solveArms), or at s >= 0.92 blade.
- Start-up kick (vrin): 1.2 s after the first gameplay poll, and after flat periods >= 8 s, the right stick goes -0.6 for 0.2 s and then
  +0.6 for 0.2 s.
  - bleedUntil keeps the view still (the game camera's turn comes off viewHold, as in a snap's catch-up).
  - turnSeq++ and moveCapture(30) at the start and end, as a snap does.
  - Skipped if the right stick is in use; counted as nKicks.
  - Root cause still open `[INFERENCE]`: likely the game camera's automatic recentering behind Jin until the right stick is first
    used.
- Self-test PASS (selftest=0, 128) + right hand alone draws, freed at 90 % reach past half the blade, hiltT clamp, circumC
  (points on circles).

## GoTVR 0.18.28 → 0.18.29 — physical katana: hand lock, blade joint, draw gain, sheathing, game attacks held off
- `[HEADSET]` owner's report:
  - The left hand is not at the sheath mouth (too low, hard to see).
  - The right hand sits a little above the hilt and rotates freely ("needs to be locked / gripped onto the hilt").
  - Drawing moves the sheath and the sword together.
  - The draw is too long: it should free "until the arm is mostly extended".
  - The owner wants free-flowing PHYSICAL combat, not the game's: unsheathing must not start any attack animation, and all attack
    animations are disabled while the katana is physically held.
  - Wants sheathing: insert the tip into the mouth and guide it home along rails.
  - Log 0.18.28: the game draw chord fired at DRAWN ("asked the game to DRAW").
- Hand lock (swd::frame, game thread, before ArmPub is published):
  - At the grab, the controller's grip frame (rows of oh, now kept as swd::gripM) is turned with fbik::arc so that the grip -Z axis
    (the line through the fist) lies on +-rail axis. The sign follows the grab: right default toward the guard, left default toward
    the mouth.
  - The origin is placed on the line: right at mouth + axis (s + hiltT); left at mouth + axis * sword_sheath_hand_cm (-3).
  - The result is stored relative to the blade frame (sheath joint axes at the mouth, slid s) or the sheath frame.
  - Each frame Q = L.R * C.R^T (partial by the ease weight, through a quaternion: rotPart) rotates wrist, fingers, thumb and palm about
    the grip point; the grip point goes to L.t, and the hand goes + palm * sword_hand_into_grip_cm (2).
  - Ease in 0.1 s, out 0.25 s.
- Blade joint:
  - fp::findBladeCands, once on the job thread: the descendants of the katana joint, then joints within 35 cm that are not ancestors,
    with no hand / clavicle in their subtree and at most 24 joints, nearest first. Logged with parent, distance and subtree size.
  - Ctrl+F7 cycles them once calibrated; Ctrl+F8 saves sword_blade_joint. Ctrl+Shift+F7 / F8 pick the katana joint.
  - swordApply moves the blade joint's subtree. If that subtree holds the katana (sheath) joint, the sheath subtree's matrices are put
    back. Without a pick it falls back to the katana joint.
  - The rail frame stays the katana joint (kGameW).
- Draw gain:
  - s = s0 + gain * (hand axial - a0), with gain = drawnFrac * blade / sword_draw_pull_cm (66.2 / 40 = 1.66). Real draws pull the
    sheath back too (saya-biki, Akban wiki "Seitei gata - Mae").
  - Let go mid-draw: home at 220 cm/s.
  - The left hand releasing mid-draw keeps s and re-bases the pull.
- Sheathing (sword.h S_SHEATHING):
  - In DRAWN the blade frame is grip o drawLock^-1. The tip is the guard - dir * blade. It is taken when the tip is within 7 cm of the
    rail line, between 6 cm outside and 4 cm inside the mouth, and the blade is within 30 deg of the rail.
  - s = blade + tip axial; the same gain drives s while pushing. s <= 1 -> SHEATHED with the hand still on the hilt.
  - Pulling the hand 6 cm back past full length frees the blade (DRAWN). Letting go slides it home.
  - The job thread eases the blade from its last in-hand pose onto the rail (0.12 s, matrix lerp + Gram-Schmidt).
  - The right hand re-locks with the draw's lock (the hand where it holds the blade).
- Game input: vrin::blockMask clears XB_X / XB_Y from the virtual pad while the hand is on the hilt or the blade is out
  (sword_block_game_attacks 1; counted). sword_game_draw default 0.
- Self-test PASS (selftest=0, 128). The katana test now also covers:
  - gain: 20 cm of pull -> 33 cm out; 40.5 cm -> free
  - sheathing: refused at 45 deg; taken lined up at the mouth; 20 cm push; home with the hand still on the hilt
  - draw again; let go -> slides home; pulled back out -> DRAWN
  - rotPart: 0 / 1 / half twice
  The earlier 1:1 assertions run with drawCm = 66.2 (gain 1).

## GoTVR 0.18.27 → 0.18.28 — katana hold finger poses, hands pinned to the rail, the game's draw / sheathe toggle, square-on-stop
- `[HEADSET]` owner: performance and the 591 calibration good ("Perfect"). Wants the left trigger unbound from calibration, finger
  poses for the right hand on the hilt and the left on the sheath, the actual draw "similar to the game's animation", and the body
  turned to the headset whenever the player stops moving.
- Left-trigger calibration block removed from swd::frame (Ctrl+F10 kept; saved calibration unchanged).
- Hilt grab: distance to the hilt SEGMENT (mouth + axis * (s + t), t in [0, hiltCm]) with sword_hilt_grab_cm 10 (was a point at the
  hilt middle, 12 cm). grab0 is unchanged, so there is no jump at the grab.
- Hold fingers: SwordPub carries holdW[2] (1 - cos easing over 0.12 s; right = on the hilt or DRAWN, left = on the sheath).
  solveArms -> holdBlend blends fing::Tune / Curl toward the hold tables:
  - right: long 65 / 85 / 45, thumb 15 / 40 / 55, all curls 1
  - left: long 60 / 80 / 45, thumb 5 / 10 / 8 (nearly straight)
  thumbAcrossDeg is analysis-time only, so it is unchanged.
- Hands pinned (game thread, before ArmPub is published, so swd::frame(ap) now runs before apubIdx.store): each holding hand's wrist
  target moves by (pin - grip) * w.
  - right: the pin is the grip projected onto the rail line, t clamped [s, s + hiltCm]
  - left: the pin is the projection clamped [-20, +2] cm around the mouth
  - w: 1 - cos easing over 0.1 s. Not while DRAWN (Trel hand attach).
- Game draw: on SHEATHED -> DRAWN the mod plays vrin::requestChord(XB_RIGHT | OX_RSR) (touchpad swipe right = draw when sheathed, sheathe
  when drawn, PC key 5, per Shacknews / GosuNoob), and the same again on DRAWN -> SHEATHED while gameDrawn.
  - New ext chord slot in vrin: 450 ms, played with applyStaged (D-pad first, the stick after 120 ms), counted in chordBusy.
  - `sword_game_draw` (1).
  - Risk `[INFERENCE]`: the game may show its own in-hand katana as well as joint 591; the owner checks.
- Square-on-stop: bodyStep head branch; stopWasMoving is set at > 0.6 m/s. Below 0.3 m/s for 0.2 s it forces headTurning when
  |e| > 0.5 deg (the eased spring).
  - Reset with the root history (lastRootOk false or not active).
  - `fp_body_square_on_stop` (1); BODY/s "on stopping".
- Self-test PASS (selftest=0, 128): + hilt segment grab (pommel end, no jump, 12 cm aside refused); hold finger blend (left full,
  right half, none) + chord queued; stop squares the body (12 deg look inside the 20 deg zone: 0.4 deg moving, 11.5 deg stopped, 0 with
  the switch off).
- Test hygiene: the queued chord is marked seen so later controller tests are not disturbed (they failed once without it).

## GoTVR 0.18.26 → 0.18.27 — performance regression (audio emitter hook), calibration on the left trigger
- `[HEADSET]` owner: "performance much worse"; wants the rail calibration on the left trigger; Ctrl+F7 / Ctrl+F8 chose joint 591.
- Log 0.18.26: census ticks median 55 / s (0.18.25: 121, 0.18.24: 117) = ~27 stereo pairs / s. AUDIO/s: ~2100 emitter updates / s;
  0.18.26's hEmitter ran readable() (VirtualQuery) five times per emitter on the audio update before any filter. Fix: owner pointer
  compare against fp::node first (the game has just used e / source / owner), no VirtualQuery anywhere on that path.
- Rail calibration: left trigger edge with both grips > 0.5, blade in the sheath, both hands within 60 cm of the katana joint (the
  left trigger is unbound in gameplay); Ctrl+F10 kept. The draw state resets after a calibration.
- Lesson (playbook ch07 hot-path rule): no memory-validation system calls in per-object hooks; validate once at discovery.

## GoTVR 0.18.25 → 0.18.26 — katana draw stage 1, own-sound centring, room-scale off, crouched start, deadband, look-down 30, arm stretch
- Owner: remove physical dodging (keep code); started in third person; improve hand smoothing; finish audio (gear / voice); push to
  main as VR IK mostly complete + 8D audio; shadows / ghosting around the hands; look-down threshold 30; katana draw with sheath /
  hilt grips, rails, haptics, input logging; arms stretch (editable).
- Room-scale: fp_roomscale default 0 (drive, reference shift, block detection kept). Body-under drawing unchanged.
- Third-person start: log - validate() skipped the node, "highest joint 99 cm" (loaded crouched); the head-height window 100-230 cm
  -> 70-230 cm.
- Hands: stab.h soft deadband after the One Euro (position 1.5 mm, rotation 0.25 deg: the output follows only the excess beyond it -
  continuous, no lag for real moves; playbook ch01 micro-deadzone; SFU study: pointing suffers at +-0.25 deg rotational jitter);
  rotation min cut-off 0.9 Hz. Self-test now measures frame-to-frame jitter (x0.01 still, 1.6 cm lag at 1 m/s).
- Audio own sounds (God of War VR D-162): Audio_EmitterUpdate 0x5BC780 (emitter: Wwise id +0x18, source +0x28 -> owner +0x38, joint
  +0x48, offset +0x50; position = owner world (node +0x150 or joint world) x offset, game's formula 0x5B5D90). After the game's
  update an emitter within 35 cm of the head is re-sent (exported AK::SoundEngine::SetPosition, resolved by name) 25 cm ahead of the
  head with the head's front / top (y mirrored), position through 0x1420A1CD0. AUDIO/s counts.
- Look-down: head pitch from the eye frame; below -35 deg (back above -25) the dead zone is fp_body_head_deadzone_down_deg (30).
- Arm stretch (fbik): a target beyond reach lengthens the upper arm by up to fp_arm_stretch x arm length (0.10); the elbow subtree
  moves out along the upper arm, the bicep helper joints (within 6 cm of the shoulder -> elbow line) by their fraction.
- Katana draw stage 1 (sword.h + fp::swordApply + swd): state machine (sheathed / drawing / drawn) with grab hysteresis (12 cm in,
  x1.75 out; grip 0.5 / 0.3), hover ticks, grab pulses, rail s = hand travel along the draw axis minus the grab offset, rumble
  0.06-0.35 by speed (pulses every 40 ms), free at 92 % of the blade (the katana keeps its hand-relative pose from that moment,
  following the drawn right hand joint). Visual: the katana joint's subtree in the render pose (and so its world matrices) moved
  rigidly. The katana joint is not identified yet: candidates 645-647 / 652-654 (lone gear at the hips, skeleton dump) + 586-594;
  Ctrl+F7 probe (lift 15 cm), Ctrl+F8 keep, Ctrl+F10 calibrate the rail (left grip = mouth, right grip = hilt end, stored in the
  joint's frame). Logs: katana: events with both hands' inputs, SWORD/s with holds and the inputs while held.
- Hand shadows / ghosting: not reproduced; candidates - right-eye object motion vectors (the frozen tick's previous palette = this
  tick's, its history = the previous right frame), or screen-space shadows (now forced on) on near geometry. Needs a photo.
- Self-test PASS (selftest=0, 126): + own-sound centring, arm stretch, katana draw.

## GoTVR 0.18.24 → 0.18.25 — 3D audio (Wwise listener = head), frozen-frame pose restore, view on the drawn body, room-scale speed, body turn spring
- `[HEADSET]` owner: see-through arms fixed. Room-scale: Jin moves but "the headset slowly follows after"; "the camera kind of rotates
  around the body"; the body snaps to the head past the threshold (wants a tween); Jin's head missing when not in first person
  (cutscenes etc.); start 8D audio (God of War VR as reference).
- Headless outside first person: log 0.18.24 - FLAT / paused contexts run 0 hero animation jobs, so the last mod-written pose (head
  collapsed) stays in the pose buffer and world array. Fix: every pose the mod is about to change is snapshotted (pose buffer by
  pointer, two alternating, + the joint world array); in flat-render ticks (hGameTick) the snapshot is written back once.
- Orbit / slow follow: the eye anchor used the animated head offset from the root smoothed 1 s in WORLD axes; the room-scale drive
  makes the game turn the node to the walking direction, the 5-15 cm head offset swings with it and the view drifted after it. Now
  the offset is smoothed in the node's own axes (hideInPose, fresh poses) and placed along the drawn heading; the drawn pose is
  shifted by T = R(bodyYaw) hzS - hzRaw so its head is where the eyes are (body and view rigid; no T while riding). Also: the
  fp_follow_lean and fp_max_camera_distance_m reads sat inside a // comment since 0.18.16 (defaults were used) - fixed.
- Room-scale: drive 0.5..1.0 stick (was 0.35..0.85), start 12 / stop 5 cm; fp_body_under_follow_s 0.5 -> 0.1.
- Body turn: critically damped spring on Hw toward the head (fp_body_turn_s 0.35; 0 = instant) instead of the 0.08 s exponential.
- 3D audio (playbook ch20, God of War VR D-159): GoT = Wwise (AK::SoundEngine exported). Boot 0x5B91A0 registers game objects 0-4,
  SetDefaultListeners(0, 1, 2). Audio update 0x5B3E60 -> Audio_ListenerFromCamera 0x5BAC80 reads CAM-B (*0x141EDBCC8) +0x890
  (camera matrix rows fwd / left / up / pos), lerps the position toward the audio focus (manager +0x4338 list), smooths 0.8, writes
  the listener matrix + inverse into the manager (+0x90 / +0xD0); 0x5BA730 sends listeners 0-2 (front / top y mirrored, position via
  0x1420A1CD0) and object 3 (fixed orientation). Hook 0x5BAC80: CAM-B +0x890 = the head (compose(S->head), the eyes' composition,
  seqlock-published from hSlot43) for the call, restored after. First person in VR only; audio_head ini. Sig added.
- Self-test PASS (selftest=0, 122): + body turn tween, 3D audio listener swap, frozen-frame restore.

## GoTVR 0.18.23 → 0.18.24 — BUG-80 for real (apply path, AO off + SSS on), hand stabilisation, room-scale, flat observer, controls
- `[HEADSET]` owner: forearm / view / jump / recenter / body turn good. "At load AO was off (menu) but the hands were still see-through;
  screen-space shadows off: not solved; back on: fixed". Hands jitter (FEAR VR has "micro tremor stabilization"). Threshold 7.5 -> 20.
  Wants flat play with logging (climbing, takedowns flags), Jin's real object to follow physical movement (dodge), new control scheme.
- BUG-80: (1) a write into the settings block (app+0x7A8) is not applied: ApplyDisplay 0x421ED0 copies the new block to +0x7A8, posts
  swapchain command 2, hands (old copy, new) to the render service (0x409460 object, vt+0x60) and saves (FUN_14045F140 compare ->
  Settings_Dump 0x424AE0). The menu showed our 0 (it reads the block); the renderer kept AO. (2) After the owner's apply (AO 0, SSS 0)
  the arms were still see-through; with SSS 1 fixed -> the artefact shows with AO on OR SSS off. Fix: in first-person gameplay the
  mod calls ApplyDisplay(app, copy with AO 0, SSS >= 1) on the main thread after the pump (rsz path), once (<= 3 tries, 5 s apart);
  F5 / Shift+F6 apply the player's values back. Self-test with a stand-in apply.
- Hand stabilisation: FEAR VR's is closed source (repo = docs only: "fine-motion controller stability", RC3). Own One Euro filter
  (stab.h) on the grip pose in LOCAL space before it becomes the wrist target: position min 1.2 Hz + 10 Hz per m/s, rotation 1.2 Hz +
  3 Hz per rad/s, derivative 1 Hz; resets (playbook ch02 list): first pose, tracking lost, recenter (theaterRecenter bump), > 25 cm
  jump, dt <= 0.5 ms or > 100 ms, non-finite. FINGERS/s logs filtered samples, hand speed and resets by reason. Self-test: still
  jitter x0.29, lag 1.4 cm at 1 m/s.
- Body follow threshold 20 deg (fp_body_head_deadzone_deg).
- Controls: left_primary (Quest X) = wheel (any control bound to "wheel" now holds it: buttons, grips, triggers), left_trigger = none,
  left_menu = start. Self-test drives the wheel with X.
- Flat observer (play_mode=flat): xr forced 0 (no host, no stereo, game camera / pad), obs::tick in GameTick: Jin's world position from
  hero+0x150, ground / vertical speed, pad buttons; per-tick diff of the hero's first 16 KB as dwords, changes between small ints (<
  0x10000) logged with old / new / ms, fields that change > 6 ticks a second set aside as busy; F8 = MARK; other hotkeys off.
- Room-scale: vrin drives the left stick toward the head (world offset fp::underW, published in cm) when > 15 cm, until < 6 cm, only
  with the player's stick at rest, in gameplay, not riding / climbing (gateW) / jumping / airborne; magnitude 0.35..0.85. bodyStep
  attributes Jin's step along the drive direction (clamped to the offset left) to a running total; hSlot43 shifts g_ref by the same
  world distance in the game camera's level frame (rsShiftRef: delta = Rr kC g, g = level rows . dw / units), so anchor + offset stays
  on your head. A drive that covers < 5 cm in 0.6 s pauses 1.5 s (wall). ROOMSCALE/s log. Self-tests: drive direction / hysteresis /
  stick priority / block; reference shift exact for any reference and view yaw.
- Self-test PASS (selftest=0, 118).
- Pushed before this round: (WIP) 0.18.23 on gotvr-0.18.22-vrik-wip (merged the owner's README update from main).

## GoTVR 0.18.22 → 0.18.23 — BUG-80 = ambient occlusion, landing arms, eye trims, body speed, runtime recenter, forearm twist (host 0.14.4)
- `[HEADSET]` owner: fingers good. Arms see-through only with SSAO Quality on, normal with it off; screen-space shadows no effect
  (log: the menu wrote AmbientOcclusionQuality +0x60 = 0..3, ScreenSpaceShadows +0x98 = 0/1). VR arms go back to the animation at the
  END of a jump; eyes ~3 cm left of the head; view 2 cm too low; on big turns the movement lags the head and the body shows; the Meta
  recenter button should recenter the game; turning the wrist should turn the forearm too (the wrist pinches).
- BUG-80 resolved (workaround): AO held at 0 while first person runs in VR (shade namespace, main thread, 0.5 s; the player's menu
  value remembered and restored on F5 / exit / Shift+F6; values outside 0..8 left alone). `[INFERENCE]` the AO pass (likely temporal
  / reprojected) prints the background onto near geometry; a proper fix = find the AO pass and reject near pixels.
- Arms at landing: the 0.18.19 jump window (1.2 s from the A press) ended before longer falls landed; the landing animation lifts the
  hands above head - 15 cm -> raisedUntil -> the game owned the arms. bodyStep now tracks the root's vertical speed (|vz| > 0.8 m/s =
  airborne); the window = 1.2 s, or while airborne up to 4 s after the press and 0.6 s after touching down. Riding cannot START inside
  the window (ARMS/s counts "jump windows held").
- fp_eye_left_cm 0 -> -3 (Jin's right), fp_eye_up_cm -3 -> -1. Body follow fp_body_head_follow_s 0.25 -> 0.08, fp_body_turn_speed 540
  -> 1440 deg/s.
- Movement lag: vrin turns the stick by headYawNow - gameYaw every poll (no smoothing); the residual lag is the game's locomotion turn
  rate (BODY/s "Jin faces N deg from your forward" swings 40-100 deg while running). Real fix = lock-on / stance strafing (bldbrne-vr
  bloodborne_facing.h), roadmap.
- Runtime recenter: host 0.14.4 handles XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING (LOCAL / STAGE) -> shared v15
  hostRecenterSeq; the game runs vrin::recenterView() (= F11) 300 ms later, after the runtime moved the space.
- Forearm twist (fbik): twist joints = elbow subtree minus hand subtree within 3 cm of the elbow -> wrist line, 15-105 % along (Jin
  508 / 509 / 510 at 25 / 50 / 75 %, 511 / 512 at the wrist). Before the solve: roll of the hand about the forearm, from the elbow's own
  frame (projected axes); after: the same; the difference (clamped +-150 deg) times t times fp_forearm_twist turns each twist joint about
  the forearm axis through itself. The arms log lists the twist joints; ARMS/s the live roll.
- Self-test PASS (selftest=0, 115): + ambient occlusion hold, forearm twist (synthetic: half way down takes half the roll).
- Pushed before this round: branch gotvr-0.18.22-vrik-wip ("(WIP) GoTVR 0.18.22: VR IK").

## GoTVR 0.18.21 → 0.18.22 — owner change: movement always headset-oriented, body follows the head past 7.5 deg
- Owner (before testing 0.18.21): "always rotate Jin to match the headset rotation after a small threshold (5-10 deg); movement totally
  headset oriented at all times".
- vrin: `[controls] move_follows_head` (default 1): the stick is turned by headYawNow - gameYaw on every poll (fallback to the captured
  moveYaw / viewHold when the head yaw or game yaw is unknown). The capture code stays for move_follows_head=0.
- bodyStep: with move_follows_head the standing head-follow branch also runs while moving (dead zone around the drawn body, follow
  until within 0.5 deg - was 2, which left a visible offset with a small threshold). fp_body_head_deadzone_deg default 45 -> 7.5
  (min 1). Fixed: fp_body_head_follow_s / fp_body_move_follow_s were never read (the reads sat inside a // comment since 0.18.17).
- Self-test PASS (selftest=0, 114): + stick follows the headset while moving; moving body in head mode (5 deg look: no turn, 12 deg:
  square to the view); the 45 deg tests pin their own threshold.

## GoTVR 0.18.20 → 0.18.21 — forward at move start only, finger hinge axes (FEAT-09d), thumb fist curl, finger tween, BUG-80 screen-space shading test
- `[HEADSET]` 0.18.20: "the forward vector is being updated every frame I move - I only want it once I start moving"; arms still
  see-through (so BUG-80 is NOT the near clip: 10 cm did not fix it); grip photo = fingers bent sideways / one straight; thumb photo =
  the thumb only dips; owner wants a fist-like thumb on the stick and fingers that tween instead of snapping.
- Forward: 0.18.20's moving dead-zone turn wrote vrin::moveYaw = Hw every frame of the turn (and 0.18.19's stop capture was still on).
  Both removed. Moving: Hw follows moveYaw + off; the dead-zone turn changes `off` (body / sword to the view), the forward stays; a new
  moveYaw (start from rest, snap, menu, recenter) clears off. BODY/s logs the offset and the move-start count. Self-test hang found on
  the way: lastMv = 1e9 made wrapPi loop forever (float -1e9 + 2pi == -1e9) -> a seen flag, and wrapPi now fmods large / rejects
  non-finite input.
- FEAT-09d fingers, root cause of the sideways bends: each joint's axis = bone x palm computed AFTER the earlier joints had bent; after
  the ~80 deg knuckle the next bone is ~parallel to the palm normal, so the cross product is ~0 and its direction is noise. Now one
  hinge per finger (hand-local, analysed once from the straight bind finger knuckle -> end joint x palm normal), used for every joint
  of the finger; the chain's last joint (the end knuckle, a leaf) now bends too. Angles 80 / 100 / 60 (God of War VR 80 / 95 / 65).
  Thumb: hinge = thumb direction x (cos 15 * across-the-palm + sin 15 * palm normal), angles 15 / 40 / 60 -> the thumb crosses over the
  curled index / middle like a fist thumb (fit with a simulation on Jin's bind data: thumb end joint at (8.4, 4.9, 0.3) hand-local,
  above the index's middle phalanx). Self-test on Jin's own bind + animated joints (from the 0.18.20 log), both hands mirrored: all
  four end joints in front of the palm, each within 1.5 cm of its knuckle's side position; the thumb end joint across the fist.
- Tween: the curls (grip, index, thumb) are critically damped springs (exact step, no overshoot), ~90 % in fp_finger_tween_s (0.15 s),
  on a QueryPerformanceCounter clock per hand (GetTickCount64's 16 ms steps would have stalled every other frame).
- BUG-80 new lead `[INFERENCE]`: the owner's photo crop shows grass-shaped dark streaks ON the forearm skin, view-dependent (solid at
  one look angle), both eyes, in the game image, not fixed by the 10 cm near. A screen-space shadow / AO pass marching the depth
  buffer from a surface a few cm from the eye lands on the grass behind it and prints the grass as shade; the sun's on-screen direction
  (= where you look) changes it. Test: Shift+F6 holds ScreenSpaceShadows (+0x98) and then AmbientOcclusionQuality (+0x60) at 0 in the
  PC settings block (mblur pattern; values 0..8 only, restored on the way out / at exit), NEAR/s logs what the game holds. If the game
  only reads them on a menu apply, the owner toggles them in the menu instead.
- Self-test: + Jin's hands, moving body, arm shading test.

## GoTVR 0.18.19 → 0.18.20 — grass over the arms = near clip (BUG-80 resolved?), fingers (FEAT-09c), touch (host 0.14.3), move-start forward, moving dead zone
- `[HEADSET]` 0.18.19: see-through arms in BOTH eyes and visible in the owner's mirror photo (grass drawn over the palm / wrap); fingers
  detached but grip = odd pose, trigger = only the little finger; stop-forward not working; owner asks for thumb-on-stick detection
  and for the body to turn past the threshold while moving; remove the gesture text.
- Log 0.18.19 hand structure (left 513; mirrored right 561): long fingers = metacarpal (3.5 cm from the hand) -> knuckle (7 cm) -> 3.5
  -> 2.0 cm end, each metacarpal with a 7 cm helper leaf; thumb 534 -> 535 -> 536 (4.1 + 2.9 cm); 537 a 7.5 cm socket; 514 -> 515 a
  50 cm helper. Fingers (by hand-local z): thumb -2.3, index 516 (-1.8), middle 521, ring 530, little 525. The animation curls the
  fingers toward hand-local +y (left) / -y (right) = the palm. 0.18.19 took the ring as the thumb (the arm analysis' thumb direction
  points at the socket) -> index = little finger; bends used the controller palm.
- FEAT-09c: thumb = shortest chain (joint count, then length); the palm normal from the hand's own bind geometry (finger direction x
  index-to-farthest-base, signed toward the thumb tip), used in the hand's current frame. Angles 85 / 105 (fingers: metacarpal unbent,
  knuckle, middle joint), thumb 25 / 40. Curl: middle / ring / little = grip, index = trigger (0.35 on trigger touch), thumb = thumb
  touch. Host 0.14.3: thumb_touch (thumbstick / thumbrest / A B X Y touch) -> GV_BTN_THUMBTOUCH, trigger_touch -> GV_BTN_TRIGTOUCH
  (Touch and Index profiles).
- BUG-80 `[INFERENCE]`, now well supported: grass drawn over the arm in the game image (mirror photo), first reported right after
  0.18.11 lowered the near clip to 2 cm; a GPU grass path apparently projects with the game's 10 cm -> reversed-Z depth 5x larger ->
  grass within ~5x the hand's distance wins the depth test. fp_near_cm default 0 (game's own). Proper fix later: find the grass pass's
  projection and give it the same near.
- Movement forward: taken in vrin the moment the left stick leaves rest (idle >= 250 ms) = the headset yaw published by hSlot43 (the
  0.18.19 stop capture never fired in the log and took the head at the stop). Moving: |head - Hw| > dead zone -> the body turns to the
  head (tau fp_body_head_follow_s) and moveYaw follows Hw. gesture_indicator default 0.
- Self-test PASS (selftest=0, 111): + move-from-rest forward; FP body follow with the moving turn.

## GoTVR 0.18.61 → 0.18.62 / host 0.15.2 → 0.15.3 — VR session 18
- `[HEADSET]` black screen on every hit: the full-view hit flash (M_CINEMA path) - default off (ui_hit_fullscreen_ms 0, v=2).
- `[HEADSET]` two-hand flicker: explicit spring unstable at the two-hand stiffness -> implicit step (attack.h Weight::step).
- `[HEADSET]` heavy never fired: charge drift 10 cm -> 25 cm (sword_heavy_v=2), heavy window 300 ms into the swing.
- `[HEADSET]` parry never shown: 76 ms history; game sword auto-sheathed -> draw on guard + keep-drawn tap (the sheath blink /
  sound = the game's auto-sheathe, which the tap should prevent). parry_mode=blade added (enemy weapon segment).
- `[HEADSET]` VR arms lost on logs / ropes; flicked back during grapples -> hands above the head only, 1.2 s hold.
- `[HEADSET]` walking backwards showed Jin's body: sword_mask_while_held froze the upper body -> default 0 (sword_mask_v=2).
- `[HEADSET]` judder by direction and in menus without frame drops `[INFERENCE]`: host 72 -> 44 fps while the game ran
  uncapped (GPU contention) -> capPace (xr_game_fps_cap 58, xr_menu_fps_cap 60).
- `[HEADSET]` wrist HUD never seen: mode default 1 (wrist_hud_v=2), wider glance; the game hides its health art out of combat.
- OPEN: black square behind the pickup item picture (rations) on the HUD layer. `[INFERENCE]` the picture is an off-screen
  render with an opaque black background composited additively; additive on the flat screen, opaque on our transparent UI
  image. Next: log the PSO blend of UI draws (CreateGraphicsPipelineState) and keep dst alpha for additive UI blends.

## GoTVR 0.18.62 → 0.18.63 — code review of the 0.18.58-0.18.62 diff (no headset run yet)
- readable() cache: a heap region trusted 200 ms could be decommitted meanwhile -> 50 ms for non-image regions, 1 s for MEM_IMAGE.
- parryTry sent the draw toggle on every blow while the game's sword was away (double toggle = sheathe) -> only after a judged
  contact, shared 2.5 s limiter (g_drawChordAt) with the sword frame.
- forcePress single slot: a parry after a held block never re-pressed LB; the keep-drawn tap shortened a block hold ->
  forceFrom / forcePressFresh (50 ms release), a press never shortens an active one, tap skipped while ppend.on / LB forced.
- atk::Intent: a free left hand moving > 25 cm restarted a one-handed charge -> left hand counted only with two hands.
- Evidence for the 0.18.62 pacing (v18.log SYNC/s): menus 155-347 flat ticks/s uncapped, VR 56-70 pairs/s; host loop 44-72 fps.

## GoTVR 0.18.63 → 0.18.64 / host 0.15.3 → 0.15.4 — VR session 19
- `[HEADSET]` parry / block never: v19.log 3 contacts judged (PARRY, 168 / 21 / 108 ms) - the replay at +50 ms went through twice,
  "guarded" once (Jin already at 0). AND each held-back blow was followed 31-63 ms later by another record of the same swing (ids
  232/233, 578/579) that passed (ppend.on) -> double damage. Fix: parry_engine=mod (swallow + stagger via strike), swallow repeats
  (swAtt/swUntil) in both engines, game-engine delay 130 ms (parry_v=2).
- `[HEADSET]` stance: sword_mask_while_held 2 (standing only; sword_mask_v=3).
- `[HEADSET]` movement forward wrong until snap: BODY/s "Jin faces -121 deg from your forward, 4.3 m/s" at ~642066000; start-up kick
  had run at 642041. Root cause open. Guard: vrin::moveFix from the run direction vs the stick's world direction (>= 2.5 m/s,
  stick >= 0.8, 60+ deg steady 0.5 s), cleared on turnSeq; logs head / game camera / viewHold / stick / run yaws.
- `[HEADSET]` third person after respawn: cutHold from sceneBars (death camera, 642259250); the load at 642261250 did not end it
  (ev stayed true), sbar "hold" on dark edges for 6 s. Fix: loadingTick or a new hero pointer clears cutHold + sceneBars and resets
  sbar pos/neg.
- `[HEADSET]` grapple lag `[INFERENCE]`: host 55-67 fps stretches with the game at its 58 cap (GPU bound) -> adaptive cap (paceAdapt).
- `[HEADSET]` wrist HUD beside the wrist: grip +Z runs down the handle (across the forearm) -> aim pose +Z (wrist_hud_v=3: 0.15 / 0.05 m).

## GoTVR 0.18.64 → 0.18.65 / host 0.15.4 → 0.15.5 — VR session 20
- `[HEADSET]` parry worked but hard; v20: 11 blocks, 3 parries, 4 misses at 32-49 cm. Hitbox = guard - 30 cm .. tip + 12 cm, tolerance 40
  (parry_v=3), pushed before the strike gates.
- `[HEADSET]` no feedback: sfx::event (synth clang via PlaySound on a worker thread, fxSeq to the host), host fx layer (vignette on
  vitHp drops, low-health pulse, block / parry rim flash). Shared block v17.
- `[HEADSET]` arms: raised-hand gate = climbing only (slow < 1.2 m/s for 0.3 s); riding no longer skips the arm solve (riding fired
  643731-643757 with the sword sheathed - ropes / logs).
- `[HEADSET]` sheathe sound: the game's auto-sheathe (v20: 6.2-16.3 s after drawing) -> g_muteJinUntil (Jin's emitters to +10 km, before
  the self-radius test); our own requested sheathe is not learned as the auto time.
- `[HEADSET]` 30 deg turn after sheathing = the game's drawn stance -> game sheathe toggle once the katana is home and the hand is off
  the hilt 0.4 s.
- `[HEADSET]` "F1 changed the HUD": the log shows F1 = hand pitch +5 (20 -> 95 -> 45); the black square going away was not the HUD mode.
  Hand pitch reset (fp_hand_v=2). Alt+F1 is the HUD A/B; Ctrl+Shift+U dumps the UI image (uidump).
- `[HEADSET]` wrist HUD rolled / turned with the wrist (aim pose follows the wrist) -> forearm from an estimated elbow (positions only).

## GoTVR 0.18.65 → 0.18.66 / host 0.15.5 → 0.15.6 — VR session 21
- `[DUMP]` ui_dump (Ctrl+Shift+U): alpha 255 over x 2040-2303, y 534-1080 (pickup panel), colour 0 in 82 % of it -> host key shader
  (opaque & dark -> alpha = brightness / 0.20), compiled with d3dcompiler_47 at start (vs/ps 5.0, verified under wine).
- `[HEADSET]` game hit overlay: keyed out (see-through dark / red) for 1.5 s after an HP drop (host fx::hurtNow).
- `[HEADSET]` parry read as block: parry_mode=motion (parry.h mode 2, body-relative tip motion in the parry window), parry_v=4.
- `[HEADSET]` sheathe animation after 0.18.65's game sheathe request: request removed; hip squaring while walking with gd (hipM, 0.6 s).
- `[HEADSET]` wrist HUD not on the bone: Jin's left elbow / wrist (W array) mapped back through compose's inverse to tracking space,
  shared v18 relative to the grip; the host lays the panel on it.
- `[HEADSET]` ropes / logs: riding = markers AND a thigh near level (knee < 22 cm below the hip). v20 / v21 riding seconds: head ~140 cm
  above the root, 0-10 m/s (horse).
- `[HEADSET]` prompt jitter `[INFERENCE]`: adaptive cap swung 46-58 -> calmer hysteresis, floor 50.

## GoTVR 0.18.67 → 0.18.68 — VR session 23
- `[LIVE]` hybrid: every defended blow composed 0x03800041 (not guarded) with +0x14C already 0x03000105 in VR (bit 0 is not enough /
  not the guard alone) -> damage, then restored -> vignette + game damage HUD. Default back to mod (parry_v=6).
- `[LIVE]` 18 x "the light record's pointers could not be found" (game sword drawn) -> recResolve falls back to the saved definition copy
  when movesetEntry fails; failure step logged; parry stagger recOnly (no message path).
- Katana blows held 200 ms; dropped for an attacker defended within 600 ms (deflect touches).
- Snap / smooth turn: body tween 0.06 s (snapFast). Keep-drawn guard tap every 3 s, all engines (sword_keep_v=2).
- Movement direction wrong after a dismount (647097640: faces -112 -> 170 deg at 4.9 m/s): kick on riding off; guard 45 deg / 0.3 s / 1.5 m/s.
- UI image saved once after the first hit (ui_hit_*.bmp).

## GoTVR 0.18.68 → 0.18.69
- Correction: no horse in session 23 (owner). 647094-647097: jump window held 46-71 frames/s (in the air / sliding), 7.6-8.8 m/s, game camera
  4.2-4.6 m from the head; the "riding ON" at 647099843 (knee 20 cm below the hip, crouched landing) was a false positive. Kick after a
  fall / slide of >= 1 s.
- HIT SCAN (flat): hero 32 KB at each enemy blow on Jin, block held / time, composed flags; guarded vs unguarded value-set comparison.

## GoTVR 0.18.71 → 0.18.72 / host 0.15.6 → 0.15.7 — VR session 26
- `[LIVE]` crash: last lines a mod PARRY on dog 000004cbf53de000 (class 16) at 650835500, then tick=0 (game thread gone). No dog record
  exists in VR (REC: light / heavy people's records only) -> the parry stagger (recOnly) and the queued cut used the people's light record
  on a dog. Dogs: dog record or the message path; recOnly on a dog without one -> nothing.
- `[LIVE]` guard engine passed blows to the game twice; both "not guarded by the game" (0x01001041 spear, 0x03800041 dog) -> mod defends
  unless the record's damage (+0xE0/+0xE4/+0xEC) is already <= 0.
- `[LIVE]` 2 parries by the GAME: guard hold lapsed when sp >= 0.8 m/s and re-pressed; keep-drawn LB tap with foes near. Hold kept while in
  zone; tap skipped with a hostile within 4 m. xtMotion failure -> BLOCK.
- `[LIVE]` 0 clashes; enemyBlade never instrumented in the clash scan -> ebWhy[] reasons + WEAPON/total + per-touch weapon distance; two-hand
  weapon line (hands > 25 cm apart). Strike hold 300 ms (parry_hold_v=2).
- `[LIVE]` HITS: too slow at 1.8-2.4 m/s; too short at 4.9-9.4 m/s (history < 100 ms) -> 1.8 m/s / 14 cm (sword_hit_v=2), travel over
  >= 100 ms when the history is shorter.
- `[DUMP]` ui_hit (d26): alpha 156-255 over the WHOLE image, colour black / dark red / orange cross; HUD bright on top. Key shader: overlay
  = 4 probe points (50/30, 30/50, 70/50, 50/70 %) opaque & dark; then dark, red (r > 1.6 max(g,b), k < 0.8) and warm (r > 1.35 g, > 2 b) ->
  transparent, rims k < 0.35 fade. Verified on the dump (numpy) and compiled under wine. bandsOf: veil over all rows with >= 6 red samples is
  not a fade (the dump gave veil 13 / 13, reds 217).
- Swing step (sword_lunge_swing_*), sheath catch x speed (sword_sheathe_moving_mult), body lead (fp_body_lead_s), chordTurnAt (RSR in the
  draw chord).
- Review (independent agent) fixes: forcePress in the guard hold cancelled forcePressFresh's 40 ms release (hold skipped 300 ms after a
  fresh press; gaOn kept meanwhile); shader keyed full-screen fades (now needs >= 2 of 8 red edge probes and never with GV_UI_FULL);
  two-hand weapon line fired on idle arms (now the left hand within 15 cm of an axis of joint 637 held in the right hand); record choice
  never crosses dogs / people; bodyStep dt from QPC (GetTickCount64 gave dt 0 at 64+ pairs/s; < 2 ms = same frame; the tick clock when it
  disagrees by > 30 ms for the self-test); melee 100 ms stretch only when hn < HIST; melee evaluated / contacts counters were inside a
  comment; sword_hit_contact_cm / cooldown defs were inside a comment; WEAPON/total logged on its own counters; queued blows carry regGen;
  recZero hand-off needs gaOn; bandsOf red channel by rbFmt; swing step ignores enemies > 150 cm above / below; keep-drawn tap taken early
  right after a blow on Jin (60-400 ms) with a foe within 4 m instead of being skipped.
- Haptic signatures (hapx): block 45 ms knock, parry rising double knock (50 + 90 ms at +85 ms) both hands, hit 4 x 28 ms buzzes
  (fx_hit_rumble). Cut damage x(0.8 + 0.25 x position along the blade).
- `[DUMP]` GhostOfTsushima.exe.71452.dmp (14:26 local, session 26): ACCESS_VIOLATION reading 0x4cbf29006e0 at GhostOfTsushima.exe+0x586e76 on
  the game thread (tid 57568), rdi = r10 = the dog 0x4cbf53de000; the faulting stack holds the hit path (+0x856d8a, Deliver +0x85a8b8) and
  cbt::strike's static record buffer (rebuilt 0.18.71 map) -> the people's record processed against a dog, as diagnosed from the log.

## GoTVR 0.18.73 → 0.18.74
- Owner: triggers send their (no item held) inputs only with that hand's grip open. vrinput.h trigFist latch (grip > trigger_grip_level 0.3
  while pulled -> muted until the trigger is released) + fistWait (grip > 0.1 at the pull: 80 ms to become a fist); gestures, the wheel
  and the press edge (trigWas) honour it. Self-test "trigger fist test". Katana heavy charge reads hd.trigger directly (unaffected).

## GoTVR 0.18.74 → 0.18.75 / host 0.15.7 → 0.15.8
- Owner (screenshot): the wrist HUD sat right on the forearm; wanted beside it to the left. xrwrist.h: offset L * wrist_hud_side_m (0.07,
  wrist_hud_v=6), L = the panel's up axis (square to the forearm and the face), flipped to the head's left.
- Flat log session 27 -> COMBAT_FINDINGS; guard trace round 2 (HITREC at HitProc, interaction SetState hook, event-bus ring, glint
  event learning, guard_trace_dr off via guard_trace_v=2).

## GoTVR 0.18.75 → 0.18.76
- parry_engine=signal (default, parry_v=9): mod-defended blows rewritten (+0x34 = -1, +0xE0 = 0) and passed to HitProc; health put back if
  the game still took any. Interaction phase table (hook 0xB1F130 always on); katana touch on a striking / late-wind-up enemy = deflect.
- Flat: gtr_attacks.bin (interaction 0x2000 + params 0x200 at 0 -> 1 and at the end of the wind-up), Ctrl+Shift+N full guard signal.
- Fixed: parry_hilt_extend_cm / parry_tip_extend_cm defaults were inside a comment.

## GoTVR 0.18.76 → 0.18.77 — VR session 29
- `[LIVE]` parries: 18 "PARRY by the mod" each followed by "STRIKE parry stagger (... light record with the hit reaction, x0.15)" = a damaging
  blow (flinch + blood). parry_stagger = 0; parry assist: fresh LB press once per enemy attack when the blade moves in (sp >= 2 m/s, net >= 10
  cm in 80 ms, blade up in front) during the late wind-up (phase 1, 250-1500 ms in) of a hostile within 3.5 m.
- `[LIVE]` third person at low health: 666093906 "cinematic letterbox on the HUD image (bar rows 6/6, veil 12/13)" at 75 / 350 HP -> cutscene ->
  game camera. cineRead ignores the HUD image while HP < 40 % (> 1.5) and hurt < 30 s ago.
- Owner: invisible snap turn on draw / sheathe -> turn kick 0.7 s after S_DRAWN / S_SHEATHED and after the draw chord.
- `[LIVE]` 22 swing steps of 25 cm logged, not felt: 55 ms stick pushes; now closed loop on the hero position (stepStart / stepTick, 450 ms max).
