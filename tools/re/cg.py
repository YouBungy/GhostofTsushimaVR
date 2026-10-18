"""direct call graph over pdata functions (+ jmp tail calls to function starts)."""
import sys, os, re, pickle, bisect, collections
sys.path.insert(0, os.path.dirname(__file__)); import idx
CG=os.path.join(idx.ROOT,'out','cg.pkl')
def build():
    I=idx.I(); fst=I['fstarts']; funcs=I['funcs']
    starts=set(fst)
    callees=collections.defaultdict(set)
    r=re.compile(r'^\s*([0-9a-f]+):\s+(call|jmp)\s+0x([0-9a-f]+)')
    for l in open(idx.ASM,'rb'):
        m=r.match(l.decode('latin1'))
        if not m: continue
        a=int(m.group(1),16); t=int(m.group(3),16)
        if m.group(2)=='jmp' and t not in starts: continue
        k=bisect.bisect_right(fst,a)-1
        if k<0: continue
        callees[fst[k]].add(t)
    pickle.dump(dict(callees),open(CG,'wb'))
def load(): return pickle.load(open(CG,'rb'))
def path(src, dsts, depth=8, cg=None):
    cg=cg or load(); dsts=set(dsts)
    prev={src:None}; q=collections.deque([(src,0)])
    while q:
        n,d=q.popleft()
        if n in dsts:
            p=[];
            while n is not None: p.append(n); n=prev[n]
            return p[::-1]
        if d>=depth: continue
        for c in cg.get(n,()):
            # map chunk to function start
            f=idx.func_of(c); c2=f[0] if f else c
            if c2 not in prev: prev[c2]=n; q.append((c2,d+1))
    return None
if __name__=='__main__':
    if sys.argv[1]=='build': build()
