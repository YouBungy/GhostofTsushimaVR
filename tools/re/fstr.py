"""strings referenced from a function (or VA range): python3 -I fstr.py <va> [<va_end>]"""
import sys, os, gzip, re
sys.path.insert(0, os.path.dirname(__file__)); import idx
S={}
for l in gzip.open(os.path.join(idx.ROOT,'out','strings.tsv.gz'),'rt',errors='replace'):
    p=l.rstrip('\n').split('\t')
    if len(p)>=4 and p[1] in ('.rdata','.data'):
        S[int(p[0],16)+idx.IB]=p[3]
def strs_in(a,b):
    out=[]
    for line in idx.dis(a,b):
        m=re.search(r'# 0x([0-9a-f]+)',line)
        if m:
            t=int(m.group(1),16)
            if t in S: out.append((line.split(':')[0], S[t]))
    return out
if __name__=='__main__':
    a=int(sys.argv[1],16)
    if len(sys.argv)>2: b=int(sys.argv[2],16)
    else:
        f=idx.func_of(a); a,b=f
    for va,s in strs_in(a,b): print(va, repr(s[:120]))
