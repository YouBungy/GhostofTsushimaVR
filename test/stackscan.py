import sys, struct, collections
from minidump.minidumpfile import MinidumpFile
mf = MinidumpFile.parse(sys.argv[1]); r = mf.get_reader(); tid = int(sys.argv[2])
mods = sorted([(m.baseaddress, m.baseaddress + m.size, m.name.split('\\')[-1]) for m in mf.modules.modules])
def mod_of(a):
    for b, e, n in mods:
        if b <= a < e: return n, a - b
    return None, None
th = [t for t in mf.threads.threads if t.ThreadId == tid][0]
lo = th.Stack.StartOfMemoryRange; sz = th.Stack.MemoryLocation.DataSize
print('thread %d stack %#x..%#x (%d KB) teb=%#x' % (tid, lo, lo + sz, sz // 1024, th.Teb))
data = r.read(lo, sz)
counts = collections.Counter(); first = []
for off in range(0, sz - 7, 8):
    v = struct.unpack_from('<Q', data, off)[0]
    n, o = mod_of(v)
    if n and n.lower() not in ('',):
        counts[n] += 1
        if len(first) < 4000: first.append((lo + off, n, o))
print('modules referenced on stack:', counts.most_common(20))
# print top-of-stack (highest addresses = oldest frames) chain
for a, n, o in first[-60:]: print('%#x %s+%#x' % (a, n, o))
print('--- lowest (newest) ---')
for a, n, o in first[:25]: print('%#x %s+%#x' % (a, n, o))
