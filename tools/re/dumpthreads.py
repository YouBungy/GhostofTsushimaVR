# minidump: every thread's RIP and the module return addresses above RSP: python3 dumpthreads.py <dmp> [tid ...]
import sys, struct
from minidump.minidumpfile import MinidumpFile
mf = MinidumpFile.parse(sys.argv[1]); r = mf.get_reader()
mods = sorted([(m.baseaddress, m.baseaddress + m.size, m.name.split('\\')[-1]) for m in mf.modules.modules])
def mod_of(a):
    for b, e, n in mods:
        if b <= a < e: return n, a - b
    return None, None
f = open(sys.argv[1],'rb')
want = [int(x) for x in sys.argv[2:]]
for t in mf.threads.threads:
    if want and t.ThreadId not in want: continue
    f.seek(t.ThreadContext.Rva); ctx = f.read(t.ThreadContext.DataSize)
    rip = struct.unpack_from('<Q', ctx, 0xF8)[0]; rsp = struct.unpack_from('<Q', ctx, 0x98)[0]
    lo = t.Stack.StartOfMemoryRange; sz = t.Stack.MemoryLocation.DataSize
    data = r.read(lo, sz) or b''
    n,o = mod_of(rip)
    out=[]
    start = max(0, rsp-lo)
    for off in range(start - start%8, len(data)-7, 8):
        v = struct.unpack_from('<Q', data, off)[0]; mn, mo = mod_of(v)
        if mn and mn.lower() not in ('ntdll.dll',) and mo > 0x1000: out.append('%s+%x' % (mn.replace('GhostOfTsushima.exe','exe').replace('xinput1_4.dll','MOD'), mo))
        if len(out) >= 14: break
    print('tid %6d rip %s+%x | %s' % (t.ThreadId, n, o or 0, ' '.join(out)))
