import struct, collections
from minidump.minidumpfile import MinidumpFile
P='/mnt/user-data/uploads/CrashDumps/GhostOfTsushima.exe.27004.dmp'
mf=MinidumpFile.parse(P); r=mf.get_reader()
mods=sorted([(m.baseaddress,m.baseaddress+m.size,m.name.split('\\')[-1]) for m in mf.modules.modules])
def mod_of(a):
    for b,e,n in mods:
        if b<=a<e: return n,a-b
    return None,None
segs=[]
ml=mf.memory_segments_64 or mf.memory_segments
for s in ml.memory_segments:
    segs.append((s.start_virtual_address, s.size))
best=[]
for va,sz in segs:
    if sz<200000 or sz>4_000_000: continue
    try: buf=r.read(va,sz)
    except Exception: continue
    if not buf: continue
    cnt=collections.Counter()
    for off in range(0,len(buf)-7,8):
        v=struct.unpack_from('<Q',buf,off)[0]; n,o=mod_of(v)
        if n: cnt[(n,o)]+=1
    if cnt:
        top=cnt.most_common(1)[0]
        if top[1]>=40: best.append((top[1],va,sz,cnt.most_common(8)))
best.sort(reverse=True)
for c,va,sz,top in best[:6]:
    print('seg %#x size %d KB  topcount=%d'%(va,sz//1024,c))
    for (n,o),k in top:
        if k>10: print('    %6d x %s+%#x'%(k,n,o))
