"""Resolve MSVC x64 RTTI: TypeDescriptor -> CompleteObjectLocator(s) -> vtable(s).
Output: class, offset-in-complete-object, vtable RVA, slot count, slot RVAs."""
import struct, sys, re, json, os
sys.path.insert(0, os.path.dirname(__file__))
from pe import PE

pe = PE(sys.argv[1])
d = pe.d
IB = pe.imagebase
text = next(s for s in pe.sections if s['name'] == '.text')
rdata_secs = [s for s in pe.sections if s['name'] in ('.rdata', '.data', '_RDATA')]


def in_text(rva):
    return text['va'] <= rva < text['va'] + text['vsz']


# 1. type descriptors: name string at TD+0x10, TD lives in .data
tds = {}
for m in re.finditer(rb'\.\?AV[\x21-\x7e]+?@@\x00', d):
    off = m.start() - 0x10
    rva = pe.rva_of_off(off)
    if rva is None:
        continue
    tds[rva] = m.group()[:-1].decode('latin1')

# 2. COLs: signature=1, offset, cdOffset, pTypeDescriptor(rva), pClassHierarchy(rva), pSelf(rva)
cols = {}
for s in rdata_secs:
    if s['name'] != '.rdata':
        continue
    base = s['rptr']
    blob = d[base:base + s['rsz']]
    for i in range(0, len(blob) - 24, 4):
        sig, off_, cd, ptd, pch, pself = struct.unpack_from('<IIIIII', blob, i)
        if sig == 1 and ptd in tds and pself == s['va'] + i:
            cols[s['va'] + i] = (tds[ptd], off_)

# 3. vtables: a qword equal to IB+colRVA, followed by qwords pointing into .text
col_va = {IB + r: r for r in cols}
res = []
for s in rdata_secs:
    if s['name'] != '.rdata':
        continue
    base = s['rptr']
    blob = d[base:base + s['rsz']]
    for i in range(0, len(blob) - 16, 8):
        q = struct.unpack_from('<Q', blob, i)[0]
        if q in col_va:
            vt = s['va'] + i + 8
            slots = []
            j = i + 8
            while j < len(blob):
                f = struct.unpack_from('<Q', blob, j)[0]
                fr = f - IB
                if not (0 <= fr < 0x10000000) or not in_text(fr):
                    break
                if j != i + 8 and (s['va'] + j) - IB + IB in col_va:
                    break
                slots.append(fr)
                j += 8
            name, off_ = cols[col_va[q]]
            res.append(dict(cls=name, off=off_, vtable=vt, n=len(slots), slots=slots))

res.sort(key=lambda r: r['cls'])
json.dump(res, open(sys.argv[2], 'w'), indent=0)
for r in res:
    print('%-70s off=%-4d vtable=%#010x slots=%d' % (r['cls'][:70], r['off'], r['vtable'], r['n']))
