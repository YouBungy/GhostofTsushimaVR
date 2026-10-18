import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.app.cmd.function.CreateFunctionCmd;
import java.io.*;
import java.nio.file.*;
// args: <listfile: bogusVA trueStartVA name> <outdir>; removes the bogus function, decompiles the true one
public class DecompFix extends GhidraScript {
  public void run() throws Exception {
    String[] a = getScriptArgs();
    java.util.List<String> lines = Files.readAllLines(Paths.get(a[0]));
    File out = new File(a[1]); out.mkdirs();
    DecompInterface d = new DecompInterface(); d.setOptions(new DecompileOptions()); d.openProgram(currentProgram);
    for (String ln : lines) {
      ln = ln.trim(); if (ln.isEmpty()) continue; String[] p = ln.split("\\s+");
      Address bog = toAddr(Long.parseLong(p[0].replace("0x",""),16)), st = toAddr(Long.parseLong(p[1].replace("0x",""),16));
      if (!bog.equals(st)) { Function fb = getFunctionAt(bog); if (fb != null) removeFunction(fb); }
      if (getInstructionAt(st)==null) disassemble(st);
      Function f = getFunctionAt(st);
      if (f == null) { Function c = getFunctionContaining(st); if (c != null && !c.getEntryPoint().equals(st)) removeFunction(c); new CreateFunctionCmd(st).applyTo(currentProgram); f = getFunctionAt(st); }
      if (f == null) { println("nofunc "+p[1]); continue; }
      try { f.setName(p[2], ghidra.program.model.symbol.SourceType.USER_DEFINED);} catch(Exception e){}
      DecompileResults r = d.decompileFunction(f, 180, monitor);
      String c = r.decompileCompleted() ? r.getDecompiledFunction().getC() : ("// FAILED: "+r.getErrorMessage());
      String fn = String.format("%x_%s.c", f.getEntryPoint().getOffset()-0x140000000L, p[2]);
      try (PrintWriter w = new PrintWriter(new File(out, fn))) { w.println("// "+f.getName()+" @ "+f.getEntryPoint()+" (cut-flag reader "+p[0]+")"); w.print(c); }
      println("ok "+fn);
    }
  }
}
