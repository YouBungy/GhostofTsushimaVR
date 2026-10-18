"""find absolute qword pointers to target RVAs anywhere in the image: python3 -I dptr.py rva [rva..]"""
import sys, os, struct
sys.path.insert(0, os.path.dirname(__file__)); from pe import PE
pe=PE(os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),'bin','GhostOfTsushima.exe'))
IB=0x140000000
def find(rva):
    pat=struct.pack('<Q',IB+rva); out=[]; d=pe.d; i=d.find(pat)
    while i!=-1:
        r=pe.rva_of_off(i)
        if r is not None: out.append(r)
        i=d.find(pat,i+1)
    return out
if __name__=='__main__':
    for a in sys.argv[1:]:
        r=int(a,16); print(a, [hex(x) for x in find(r)][:20])
