import struct, collections
from minidump.minidumpfile import MinidumpFile
P='/mnt/user-data/uploads/CrashDumps/GhostOfTsushima.exe.27004.dmp'
mf=MinidumpFile.parse(P); r=mf.get_reader()
mods=sorted([(m.baseaddress,m.baseaddress+m.size,m.name.split('\\')[-1]) for m in mf.modules.modules])
def mod_of(a):
    for b,e,n in mods:
        if b<=a<e: return '%s+0x%x'%(n,a-b)
    return None
print('my module present?:', [m.name for m in mf.modules.modules if 'xinput' in m.name.lower() or 'gotvr' in m.name.lower()])
va=0x7ffee1a59000  # check if nvgpucomp seg is stack or heap: look for return-address diversity
# The real stack: 0xef472fc000
va=0xef472fc000; sz=1040*1024
buf=r.read(va,sz)
frames=[]
for off in range(0,len(buf)-7,8):
    v=struct.unpack_from('<Q',buf,off)[0]; m=mod_of(v)
    if m: frames.append(m)
# find the repeating cycle: distinct module+off seen between consecutive opengl32+0x45bf0
import itertools
print('total module frames on stack:', len(frames))
c=collections.Counter(frames)
print('\nmost common frames (the recursion cycle):')
for m,k in c.most_common(14): print('  %6d x %s'%(k,m))
# print one contiguous window to see the repeating unit
print('\ncontiguous sample (60 frames):')
seen=0; prev=None
for off in range(0,len(buf)-7,8):
    v=struct.unpack_from('<Q',buf,off)[0]; m=mod_of(v)
    if m:
        print('  %s'%m); seen+=1
        if seen>=60: break
