# Tool preflight — 2026-10-17 session

| Tool | Intended question | Host live? | Correct target/program? | Bitness/API fit? | Known hazard | Fallback | Checked at |
|---|---|---:|---:|---:|---|---|---|
| llvm-objdump (Linux sandbox) | full .text disassembly | yes | yes, exe hash matches | x64 | symbolizes with nearest export names (misleading `<…+off>` tags; stripped in receipts) | — | 13:23 |
| Custom PE/RTTI/xref/sig scripts (`re/scripts`) | imports, RTTI→vtables, xrefs, unique AOBs | yes | yes | x64 | data refs via computed addresses are missed (xref = rip-relative and direct branches only) | Ghidra | 13:23 |
| Ghidra | decompile, cross-check | not used this session | — | — | — | — | — |
| Cheat Engine / ReGenny | live structs (CAM-A) | not used (no game launch) | — | — | — | — | — |
| Frida / x64dbg | EXP-01/02 | not used | — | — | Streamline interposer + debugger interplay unknown | — | — |
| RenderDoc | EXP-05 | not used | — | D3D12 | Streamline may block capture; try `-noStreamline` | PIX | — |

Reproduce: `python3 -I re/scripts/pe.py GhostOfTsushima.exe`, `rtti.py exe out/vtables.json`,
`llvm-objdump -d --no-show-raw-insn -M intel --section=.text exe > out/text.asm`, `idx.py build`,
`idx.py x|f|d|iat …`, `sigs.py '[["name","0xVA"],…]'`. Scripts expect `bin/GhostOfTsushima.exe` beside `re/`.

## Session 2 — 2026-10-17

| Tool | Intended question | Host live? | Correct target/program? | Bitness/API fit? | Known hazard | Fallback | Checked at |
|---|---|---:|---:|---:|---|---|---|
| Ghidra 11.4.2 headless (cloud sandbox, JDK 21) | decompile named functions | yes | yes, imported `GhostOfTsushima.exe` (hash matches) | x64 | full auto-analysis OOMs (TOOL-003); `-noanalysis` + `Decomp.java` works | llvm-objdump | 14:58 |
| llvm-objdump 18 + `re/scripts` (idx, cg, unwind, sigs, fstr, dptr, storescan) | xrefs, call graph, chain resolution, strings, data pointers, store scans, unique signatures | yes | yes | x64 | xrefs miss computed addresses | Ghidra | 14:45 |
| clang 18 + lld-link (freestanding) | build probe v0.2 | yes | — | x64 PE DLL | no CRT; COM calls hand-typed | — | 15:02 |
| Game runtime / Cheat Engine / RenderDoc / headset | EXP-00..08 | **not used** (static session) | — | — | — | — | — |
