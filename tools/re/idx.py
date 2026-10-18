"""Cross-reference index over llvm-objdump output + .pdata function table.

build:   python3 -I idx.py build
query helpers (import as module): xrefs(va), func_of(va), dis(va_start, va_end), dis_func(va), iat_name(va)
All addresses here are VAs at preferred image base 0x140000000; RVA = VA - 0x140000000.
"""
import re, sys, os, pickle, struct, bisect, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from pe import PE

EXE = os.path.join(ROOT, 'bin', 'GhostOfTsushima.exe')
ASM = os.path.join(ROOT, 'out', 'text.asm')
IDX = os.path.join(ROOT, 'out', 'idx.pkl')
IB = 0x140000000

re_line = re.compile(r'^\s*([0-9a-f]+):\s+(.*)$')
re_hash = re.compile(r'# 0x([0-9a-f]+)')
re_direct = re.compile(r'^(call|j[a-z]+)\s+0x([0-9a-f]+)')


def build():
    pe = PE(EXE)
    # function table from .pdata
    r, sz = pe.dirs[3]
    o = pe.off(r)
    funcs = []
    for i in range(sz // 12):
        b, e, u = struct.unpack_from('<III', pe.d, o + 12 * i)
        if b:
            funcs.append((IB + b, IB + e))
    funcs.sort()
    iat = {}
    for dll, fns in pe.imports().items():
        for n, slot in fns:
            iat[IB + slot] = dll + '!' + n
    xref = {}
    lines_off = []  # (va, file offset) sparse every 64 lines
    with open(ASM, 'rb') as f:
        n = 0
        pos = f.tell()
        for raw in iter(f.readline, b''):
            line = raw.decode('latin1')
            m = re_line.match(line)
            if m:
                va = int(m.group(1), 16)
                ins = m.group(2).strip()
                if n % 64 == 0:
                    lines_off.append((va, pos))
                n += 1
                h = re_hash.search(ins)
                t = None
                if h:
                    t = int(h.group(1), 16)
                else:
                    dm = re_direct.match(ins)
                    if dm:
                        t = int(dm.group(2), 16)
                if t is not None:
                    xref.setdefault(t, []).append(va)
            pos = f.tell()
    pickle.dump(dict(funcs=funcs, iat=iat, xref=xref, lines=lines_off), open(IDX, 'wb'))
    print('funcs', len(funcs), 'xref targets', len(xref), 'iat', len(iat))


_I = None


def I():
    global _I
    if _I is None:
        _I = pickle.load(open(IDX, 'rb'))
        _I['fstarts'] = [a for a, _ in _I['funcs']]
        _I['lva'] = [a for a, _ in _I['lines']]
    return _I


def xrefs(va):
    return sorted(I()['xref'].get(va, []))


def func_of(va):
    idx = I()
    k = bisect.bisect_right(idx['fstarts'], va) - 1
    if k >= 0:
        b, e = idx['funcs'][k]
        if b <= va < e:
            return b, e
    return None


def iat_name(va):
    return I()['iat'].get(va)


def dis(start, end):
    idx = I()
    k = max(bisect.bisect_right(idx['lva'], start) - 1, 0)
    out = []
    with open(ASM, 'rb') as f:
        f.seek(idx['lines'][k][1])
        for raw in iter(f.readline, b''):
            line = raw.decode('latin1').rstrip('\n')
            m = re_line.match(line)
            if not m:
                continue
            va = int(m.group(1), 16)
            if va < start:
                continue
            if va >= end:
                break
            ins = m.group(2).strip()
            h = re_hash.search(ins)
            if h and iat_name(int(h.group(1), 16)):
                ins += '   ; ' + iat_name(int(h.group(1), 16))
            out.append('%x: %s' % (va, ins))
    return out


def dis_func(va, maxlines=400):
    f = func_of(va)
    if not f:
        return ['<no pdata function for %x>' % va]
    lines = dis(f[0], f[1])
    if len(lines) > maxlines:
        lines = lines[:maxlines] + ['... (%d more)' % (len(lines) - maxlines)]
    return ['; func %x-%x (rva %x)' % (f[0], f[1], f[0] - IB)] + lines


if __name__ == '__main__':
    if sys.argv[1] == 'build':
        build()
    elif sys.argv[1] == 'x':
        for a in sys.argv[2:]:
            v = int(a, 16)
            for r in xrefs(v):
                f = func_of(r)
                print('%x <- %x  (func %s)' % (v, r, hex(f[0]) if f else '?'))
    elif sys.argv[1] == 'f':
        print('\n'.join(dis_func(int(sys.argv[2], 16), int(sys.argv[3]) if len(sys.argv) > 3 else 400)))
    elif sys.argv[1] == 'd':
        print('\n'.join(dis(int(sys.argv[2], 16), int(sys.argv[3], 16))))
    elif sys.argv[1] == 'iat':
        pat = sys.argv[2].lower()
        for k, v in sorted(I()['iat'].items()):
            if pat in v.lower():
                print('%x %s xrefs=%d' % (k, v, len(xrefs(k))))
