import sys, struct, collections
from minidump.minidumpfile import MinidumpFile
mf = MinidumpFile.parse(sys.argv[1]); r = mf.get_reader()
mods = sorted([(m.baseaddress, m.baseaddress+m.size, m.name.split('\\')[-1]) for m in mf.modules.modules])
def mod_of(a):
    for b,e,n in mods:
        if b<=a<e: return n,a-b
    return None,None
tid=32404
th=[t for t in mf.threads.threads if t.ThreadId==tid][0]
ctx=th.ContextObject
rsp=getattr(ctx,'Rsp',None); rip=getattr(ctx,'Rip',None)
print('tid=%d rip=%#x (%s) rsp=%#x'%(tid, rip, '%s+%#x'%mod_of(rip) if mod_of(rip)[0] else '?', rsp))
# read a large window upward from rsp across the full dumped memory
buf=r.read(rsp, 1_500_000)
if not buf:
    # try progressively
    for n in (1_000_000,500_000,200_000,64_000):
        buf=r.read(rsp,n)
        if buf: break
print('read %d bytes from rsp'%(len(buf or b'')))
cnt=collections.Counter()
seq=[]
for off in range(0,len(buf)-7,8):
    v=struct.unpack_from('<Q',buf,off)[0]; n,o=mod_of(v)
    if n: cnt[(n,o)]+=1; 
    if n and len(seq)<400: seq.append((n,o))
print('\nrepeated return addresses (recursion signature):')
for (n,o),c in cnt.most_common(16):
    if c>3: print('  %6d x  %s+%#x'%(c,n,o))
print('\nfirst ~60 module frames from rsp upward:')
for n,o in seq[:60]: print('  %s+%#x'%(n,o))
