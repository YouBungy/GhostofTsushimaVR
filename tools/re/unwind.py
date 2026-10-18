"""Resolve pdata chunk -> primary function via UNW_FLAG_CHAININFO. python3 -I unwind.py <va>..."""
import sys, os, struct, bisect
sys.path.insert(0, os.path.dirname(__file__)); from pe import PE; import idx
pe=PE(os.path.join(idx.ROOT,'bin','GhostOfTsushima.exe')); IB=idx.IB
r,sz=pe.dirs[3]; o=pe.off(r)
ents=[struct.unpack_from('<III',pe.d,o+12*i) for i in range(sz//12)]
ents=[e for e in ents if e[0]]; ents.sort(); starts=[e[0] for e in ents]
def entry(rva):
    k=bisect.bisect_right(starts,rva)-1
    if k>=0 and ents[k][0]<=rva<ents[k][1]: return ents[k]
def primary(rva):
    e=entry(rva); seen=0
    while e and seen<16:
        u=e[2]
        if u&1: u=struct.unpack_from('<I',pe.d,pe.off(u&~1)+8)[0]  # indirect
        uo=pe.off(u); vf=pe.d[uo]; flags=vf>>3; cnt=pe.d[uo+2]
        if flags&4:
            co=uo+4+2*((cnt+1)&~1)
            b,en,ui=struct.unpack_from('<III',pe.d,co); e=(b,en,ui); seen+=1
        else: return e
    return e
if __name__=='__main__':
    for a in sys.argv[1:]:
        v=int(a,16); rva=v-IB if v>IB else v
        p=primary(rva); print(a, '-> primary %x..%x'%(p[0],p[1]) if p else None)
