import ghidra.app.script.GhidraScript;
import java.util.Map;
public class SetOpts extends GhidraScript {
  public void run() throws Exception {
    String[] off = {"Decompiler Switch Analysis","Decompiler Parameter ID","Call Convention ID","Stack","Aggressive Instruction Finder","Non-Returning Functions - Discovered","Shared Return Calls","Variadic Function Signature Override","Condense Filler Bytes","Embedded Media","Apply Data Archives","Demangler GNU","PDB Universal","DWARF","Function ID","Library Identification","Scalar Operand References","Windows x86 Thunk","GCC Exception Handlers","Create Address Tables"};
    Map<String,String> opts = getCurrentAnalysisOptionsAndValues(currentProgram);
    for (String k : off) {
      if (opts.containsKey(k)) { setAnalysisOption(currentProgram, k, "false"); println("off: "+k); }
      else println("missing: "+k);
    }
    for (String k : opts.keySet()) if (!k.contains(".")) println("opt: "+k+" = "+opts.get(k));
  }
}
