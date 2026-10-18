import sys, struct, collections
from minidump.minidumpfile import MinidumpFile
mf = MinidumpFile.parse(sys.argv[1]); r = mf.get_reader()
mods = sorted([(m.baseaddress, m.baseaddress + m.size, m.name.split('\\')[-1]) for m in mf.modules.modules])
def mod_of(a):
    for b,e,n in mods:
        if b<=a<e: return n,a-b
    return None,None
# stack memory ranges from Memory64List if present
rows=[]
for t in mf.threads.threads:
    lo=t.Stack.StartOfMemoryRange; sz=t.Stack.MemoryLocation.DataSize
    rows.append((sz,t.ThreadId,lo))
rows.sort(reverse=True)
print("biggest stacks (size, tid, base):")
for sz,tid,lo in rows[:6]: print('  %8d  tid=%d base=%#x'%(sz,tid,lo))
# for the biggest, count self-recursion: most frequent game/module return addresses
sz,tid,lo=rows[0]
data=r.read(lo,min(sz,6_000_000)) or b''
cnt=collections.Counter()
for off in range(0,len(data)-7,8):
    v=struct.unpack_from('<Q',data,off)[0]; n,o=mod_of(v)
    if n: cnt[(n,o)]+=1
print("\ntop repeated return addresses on biggest stack tid=%d (recursion signature):"%tid)
for (n,o),c in cnt.most_common(18):
    if c>5: print('  %5d x  %s+%#x'%(c,n,o))
