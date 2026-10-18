"""Generate and verify unique AOB signatures for anchor functions.
Wildcards rip-relative disp32 and call/jmp rel32 using a tiny decoder derived from llvm-objdump instruction
boundaries (we re-disassemble with raw bytes for the window)."""
import subprocess, re, sys, os, struct, json
sys.path.insert(0, os.path.dirname(__file__))
from pe import PE

EXE = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'bin', 'GhostOfTsushima.exe')
pe = PE(EXE)
IB = 0x140000000
text = next(s for s in pe.sections if s['name'] == '.text')
TB = pe.d[text['rptr']:text['rptr'] + text['rsz']]


def insns(va, n_bytes=64):
    out = subprocess.run(['llvm-objdump', '-d', '-M', 'intel', '--start-address=%#x' % va,
                          '--stop-address=%#x' % (va + n_bytes), EXE], capture_output=True, text=True).stdout
    res = []
    for l in out.splitlines():
        m = re.match(r'\s*([0-9a-f]+):\s+((?:[0-9a-f]{2} )+)\s*(.*)', l)
        if m:
            res.append((int(m.group(1), 16), bytes.fromhex(m.group(2).replace(' ', '')), m.group(3)))
    return res


def pattern(va, n_bytes=48):
    pat = []
    for a, b, txt in insns(va, n_bytes):
        mask = [True] * len(b)
        if 'rip' in txt and len(b) >= 5:
            # disp32 sits before any immediate; find it by matching computed target
            m = re.search(r'# 0x([0-9a-f]+)', txt)
            if m:
                tgt = int(m.group(1), 16)
                for k in range(len(b) - 3):
                    d = struct.unpack_from('<i', b, k)[0]
                    imm_len = len(b) - (k + 4)
                    if a + len(b) + d == tgt:
                        mask[k:k + 4] = [False] * 4
                        break
        if re.match(r'(call|jmp|j[a-z]+)\s+0x', txt) and len(b) >= 5:
            mask[-4:] = [False] * 4
        pat += [(x if mk else None) for x, mk in zip(b, mask)]
    return pat


def count(pat):
    first = next(i for i, x in enumerate(pat) if x is not None)
    anchor = bytes([pat[first]])
    hits = []
    i = 0
    while True:
        i = TB.find(anchor, i)
        if i < 0:
            break
        s = i - first
        if s >= 0 and all(x is None or TB[s + j] == x for j, x in enumerate(pat)):
            hits.append(text['va'] + s + IB)
            if len(hits) > 3:
                break
        i += 1
    return hits


def fmt(p):
    return ' '.join('??' if x is None else '%02X' % x for x in p)


def unique(va, start=16, step=8, maxlen=96):
    n = start
    while n <= maxlen:
        p = pattern(va, n)
        h = count(p)
        if h == [va]:
            return fmt(p), n
        n += step
    return None, None


if __name__ == '__main__':
    anchors = json.loads(sys.argv[1])
    out = []
    for name, va in anchors:
        sig, n = unique(int(va, 16))
        out.append(dict(name=name, va=va, rva=hex(int(va, 16) - IB), sig=sig, unique=bool(sig)))
        print('%-34s rva=%-9s unique=%s  %s' % (name, hex(int(va, 16) - IB), bool(sig), sig))
    json.dump(out, open(os.path.join(os.path.dirname(EXE), '..', 'out', os.environ.get('SIGOUT','signatures.json')), 'w'), indent=1)
