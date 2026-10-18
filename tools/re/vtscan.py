import sys,json,re,os;sys.path.insert(0,os.path.dirname(__file__));import idx
IB=0x140000000
vt={r['cls']:r for r in json.load(open(os.path.join(idx.ROOT,'out','vtables.json')))}
def body(va):
    f=idx.func_of(va); return idx.dis(f[0],f[1]) if f else idx.dis(va,va+0x100)
def scan(cls, pats, depth=1, show_all=True):
    r=vt[cls]
    for i,s in enumerate(r['slots']):
        va=s+IB; b=body(va); hits=[l for l in b if any(re.search(p,l) for p in pats)]
        sub=[]
        if depth>1:
            for l in b:
                m=re.search(r'call\s+0x([0-9a-f]+)',l)
                if m:
                    t=int(m.group(1),16); bb=body(t)
                    h2=[x for x in bb if any(re.search(p,x) for p in pats)]
                    if h2: sub.append('->%x: %s'%(t,h2[0].split(': ',1)[1][:60]))
        if show_all or hits or sub:
            print('slot %2d @%x n=%-4d %s %s'%(i,va,len(b),' | '.join(re.sub(r'<[^>]*>','',h.split(': ',1)[1])[:70] for h in hits[:3]),' '.join(sub[:3])))
if __name__=='__main__':
    scan(sys.argv[1], sys.argv[2:], depth=2)
