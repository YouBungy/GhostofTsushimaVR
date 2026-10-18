"""Find functions that store xmmword to all of a set of displacement offsets on one base register."""
import re, sys, os, collections
sys.path.insert(0, os.path.dirname(__file__))
import idx
ASM = idx.ASM
offs = [int(x,16) for x in sys.argv[1].split(',')]
re_st = re.compile(r'^\s*([0-9a-f]+):\s+(v?mov(?:aps|ups|dqa|dqu|ss|q|d)?)\s+(?:xmmword|qword|dword)\s+ptr\s+\[(r[a-z0-9]+)\s*\+\s*0x([0-9a-f]+)\],\s*(\S+)')
hits = collections.defaultdict(lambda: collections.defaultdict(set))
with open(ASM,'rb') as f:
    for raw in f:
        m = re_st.match(raw.decode('latin1'))
        if not m: continue
        o = int(m.group(4),16)
        if o in offs:
            va=int(m.group(1),16)
            fn = idx.func_of(va)
            if fn: hits[fn[0]][m.group(3)].add(o)
for fn, regs in sorted(hits.items()):
    for r, s in regs.items():
        if len(s)==len(offs):
            print(hex(fn), hex(fn-idx.IB), r)
