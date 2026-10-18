import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.app.cmd.function.CreateFunctionCmd;
import java.io.*;
import java.nio.file.*;
// args: <listfile with hex VAs, one per line, optional name after space> <outdir>
public class Decomp extends GhidraScript {
  public void run() throws Exception {
    String[] a = getScriptArgs();
    java.util.List<String> lines = Files.readAllLines(Paths.get(a[0]));
    File out = new File(a[1]); out.mkdirs();
    DecompInterface d = new DecompInterface();
    DecompileOptions o = new DecompileOptions(); d.setOptions(o);
    d.openProgram(currentProgram);
    for (String ln : lines) {
      ln = ln.trim(); if (ln.isEmpty() || ln.startsWith("#")) continue;
      String[] p = ln.split("\\s+");
      Address ad = toAddr(Long.parseLong(p[0].replace("0x",""),16));
      if (getInstructionAt(ad)==null) disassemble(ad);
      Function f = getFunctionAt(ad); if (f==null) f = getFunctionContaining(ad);
      if (f == null) { new CreateFunctionCmd(ad).applyTo(currentProgram); f = getFunctionAt(ad); }
      if (f == null) { println("nofunc "+p[0]); continue; }
      if (p.length > 1 && f.getEntryPoint().equals(ad)) { try { f.setName(p[1], ghidra.program.model.symbol.SourceType.USER_DEFINED);} catch(Exception e){} }
      DecompileResults r = d.decompileFunction(f, 120, monitor);
      String c = r.decompileCompleted() ? r.getDecompiledFunction().getC() : ("// FAILED: "+r.getErrorMessage());
      String fn = String.format("%x_%s.c", f.getEntryPoint().getOffset()-0x140000000L, p.length>1?p[1]:f.getName());
      try (PrintWriter w = new PrintWriter(new File(out, fn))) { w.println("// "+f.getName()+" @ "+f.getEntryPoint()); w.print(c); }
      println("ok "+fn);
    }
  }
}
