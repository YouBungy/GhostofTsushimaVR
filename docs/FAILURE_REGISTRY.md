# Failure registry / negative results (dated 2026-10-17)

| ID | Attempt | Result | Why it matters | Reopen when |
|---|---|---|---|---|
| NEG-001 | Find the writer of the render-view global 0x1EDBCC0 by direct-store search | No `mov [rip+X], reg`, no `lea` of it, no struct-base alias found within ±0xC0 | Writer is indirect (registry/array/memcpy). Static search is insufficient | EXP-02 write breakpoint from launch |
| NEG-002 | Find persisted stereo settings (registry names, CLI switch) | None. Stereo fields exist only in memory; validator forces mode 3 → off | No zero-code way to try Nixxes stereo | EXP-04 runtime write |
| NEG-003 | Map stereo fields +0xBC/+0xC0 consumers via the Settings getter | Only a 3D Vision check (`+0xBC == 2` at 0x490DEF) and a copy into a display-mode descriptor; no renderer consumer seen | Leans toward framework residue (`[INFERENCE]`), not refuted | EXP-04 |
| NEG-004 | Find who calls the Streamline pass callback 0x60F9E0 by direct calls | Called only through a pass table in .rdata (0x1211090) | Render passes are data-driven; use the table, not call graphs | — |
| TOOL-001 | Stage GhostOfTsushima.exe through the file bridge | Bridge refused `.exe` and an 11 MB DLL; worked when split into 4 MB `.bin` chunks (hash verified) | Reuse the chunk recipe for other big modules | — |
| TOOL-002 | pip install pefile/capstone | PyPI blocked in the analysis sandbox | Used a self-written PE parser + llvm-objdump | — |
| NEG-005 | (2026-10-17, session 2) Re-run NEG-001 with a ±4 KB window around 0x1EDBCC0 and an absolute-pointer scan of the whole image | The two `[rip+X]` destination-operand hits (0x61991B, 0x9FECB5) are `cmp`; no absolute pointer to the global exists | Confirms the global is assigned through a computed pointer. **No longer blocking**: the camera object's writer is known (`Cam_SetLocalMatrix` 0x806E50) | EXP-02 write breakpoint, only if the global's lifetime matters |
| NEG-006 | Find a direct call path from the game tick 0x4C49E0 to the camera setters (direct call graph, depth 7) | None | Camera drivers run through virtual dispatch (component update). Use the setter, not the tick, as the seam | — |
| NEG-007 | Static render-pass census from the pass tables at rdata 0x1210E00..0x1211170 | Tables are small per-pass method tables with no names | A static census is not cost-effective. Use RenderDoc (EXP-05) | names found via another path |
| NEG-008 | Look for shipped VR runtime code (OpenVR/OpenXR/LibOVR) behind the Nixxes "VR" stereo mode | None; "Oculus Touch" strings are Steam Input controller-type names; stereo code is 3D Vision/SBS | Stereo mode 3 is very likely framework residue (EXP-04 still cheap to run) | EXP-04 shows SBS rendering |
| TOOL-003 | Ghidra 11.4.2 headless full auto-analysis on the 7 GB sandbox | Killed (OOM) at 6 GB heap; X86Analyzer OutOfMemoryError at 4 GB with heavy analyzers disabled | Use `-noanalysis` import + on-demand `Decomp.java` (30 functions in ~30 s). Full analysis needs a ≥16 GB machine (the user's PC) | bigger machine |
| TOOL-004 | Function bounds from .pdata for chained (CHAININFO) fragments | `func_of` returns the fragment, not the primary function | `re/scripts/unwind.py` resolves the primary function through the unwind chain | — |
| NEG-009 | (2026-10-17, EXP-00 run 1, owner's PC) probe v0.2 named `XInput9_1_0.dll` in the game folder; played the intro | No `GoTVR\load.log`, no folder: the DLL was never loaded | Session 1's "XInput9_1_0 first" order came from 0x4AEAA0, an on-demand loader behind a function table. The pad loader 0x497F30 (called from NxInput init) tries **xinput1_4.dll first**, which always resolves in System32. Fix: probe v0.3 is named `xinput1_4.dll` and forwards to System32 by full path | v0.3 also absent → check AV / load-time errors |

## NEG-010 — `Cam_SetProjRect` (0x8068E0) is not a live seam for the render camera
Evidence: GoTVR 0.4, `projrect(camA)=0` for the whole session; F8 had no effect. Do not route per-eye
projection through it. Replacement: write the projection at camera +0x200 inside the slot-43 hook (0.5).

## NEG-011 — Do not call OpenXR on the game's Present/submit thread
Evidence: GoTVR 0.4 xr=1 crashed ~1 s after the session reached FOCUSED with `EXCEPTION_STACK_OVERFLOW` in
`nvgpucomp64.dll` (driver pipeline compiler). Cause `[INFERENCE]`: in-process runtime (VDXR) compiles D3D12
pipelines on the calling thread; the game thread's stack is small. Also: never submit on the nx queue0 and
never barrier game back buffers from a queue that is not fenced against the swapchain queue.
Replacement: XR thread with a 16 MB stack + GoTVR-owned queue (0.5).

## NEG-012 — Never mutate camera state cumulatively in slot 43
Evidence: 0.4 F9 applied +5° per call; slot 43 runs ~5–6× per frame on a matrix the game does not always
rewrite → ≈35° and drifting. All camera/projection writes recompose from a cached game value (0.5).

## NEG-013 — Do not host the OpenXR runtime inside the game process (this target)
Evidence: GoTVR 0.5 (XR on its own 16 MB thread and queue) still crashed in both modes inside Virtual
Desktop's in-process code: stack overflow on a 64 KB NVIDIA compile worker, and a use-after-free
(`0xDEDEDEDE`) in its D3D11 compositor (BUG-6). Replacement: GoTVRHost.exe (0.6).

## NEG-014 — Never submit `xrEndFrame` with zero layers
Evidence: playbook ch07 (BioshockVR, same fail-fast signature); GoTVR 0.4/0.5 did it on every
`shouldRender=false` frame. Replacement: one projection layer every frame (black placeholder).

## NEG-015 — The Nixxes stereoscopic 3D mode is framework residue, not a renderer path (R1′ closed)
Evidence (static, session 5): settings validator 0x429570.. holds stereo mode +0xBC (1 = side-by-side, forced
off when windowed), separation +0xC0, popout +0xC4, near-separation multiplier +0xC8; every reader of +0xBC
after the settings getter (0x4093C0 → vt+0x118) is in the Nx framework range 0x43xxxx–0x46xxxx (present
composition, letterboxing, NVIDIA 3D Vision messages: 0x43B035, 0x43B407, 0x446405, 0x46D93F, 0x46DA18); no
code reads separation+popout together outside the validator/compare/NVIDIA handler. The Sucker Punch renderer
never offsets a camera per eye. Strings: "Stereoscopic 3D has been disabled because exclusive fullscreen was
enabled (DX12)". Do not spend time unlocking it.

## NEG-016 — Ignore DXGI_PRESENT_TEST when hooking Present on this engine
Evidence: BUG-9. The wrapper issues an occlusion-test Present before every real Present.

## NEG-017 — Do not call the engine's whole-pool graph submit twice per frame
Evidence: BUG-14. `Jobs_SubmitGraph_4D7EB0` re-walks every pass context from index 0, so a second render-graph
run must submit only its own contexts (index ≥ count after run 1), or the end-of-frame counter wait deadlocks.

## NEG-018 — The F5 darkening is not run 1's missing tiled light lists
Evidence: 0.7.4 live log; lending run 2's finished light lists to run 1 (F4 A/B, ~103 lends/s) changed nothing
visible. Bisect the second run's passes instead (0.7.5 F3).

## NEG-019 — The F5 darkening is not the main-only renderables missing from V2
Evidence: 0.7.7 live log; with the duplication V2's draws equal main's on every pass, image unchanged.

## NEG-020 — Moving the camera position for a second render of a camera-relative engine
Evidence: BUG-20. Keep the gathered position; put the eye offset into the relative view-projection.

## NEG-021 — Using the swapchain wrapper's vt+0x118 as the back buffer
Evidence: BUG-21. It is the display-settings block. Find the engine object by the native resource pointer.
