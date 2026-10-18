import struct
from minidump.minidumpfile import MinidumpFile
P='/mnt/user-data/uploads/CrashDumps/GhostOfTsushima.exe.27004.dmp'
mf=MinidumpFile.parse(P); r=mf.get_reader()
target=0xef472fc000+0x80000
exc_tid=32404
for t in mf.threads.threads:
    try: tib=r.read(t.Teb,0x18)
    except Exception: continue
    if not tib: continue
    base,limit=struct.unpack_from('<QQ',tib,8)
    hit = limit<=target<base
    if hit or t.ThreadId==exc_tid:
        print('tid=%d teb=%#x stack limit=%#x base=%#x size=%dKB %s'%(t.ThreadId,t.Teb,limit,base,(base-limit)//1024,'<-- owns the recursion stack' if hit else '(exception thread)'))
