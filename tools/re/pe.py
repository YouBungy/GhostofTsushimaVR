"""Minimal PE32+ parser (no third-party deps). Usage: python3 -I pe.py <exe>"""
import struct, sys, json


class PE:
    def __init__(self, path):
        self.d = d = open(path, 'rb').read()
        e = struct.unpack_from('<I', d, 0x3C)[0]
        assert d[e:e + 4] == b'PE\0\0'
        self.machine, self.nsec, self.timestamp = struct.unpack_from('<HHI', d, e + 4)
        optsz = struct.unpack_from('<H', d, e + 20)[0]
        self.characteristics = struct.unpack_from('<H', d, e + 22)[0]
        o = e + 24
        self.magic = struct.unpack_from('<H', d, o)[0]
        assert self.magic == 0x20B, 'not PE32+'
        self.entry = struct.unpack_from('<I', d, o + 16)[0]
        self.imagebase = struct.unpack_from('<Q', d, o + 24)[0]
        self.sizeofimage = struct.unpack_from('<I', d, o + 56)[0]
        self.subsystem, self.dllchar = struct.unpack_from('<HH', d, o + 68)
        nrva = struct.unpack_from('<I', d, o + 108)[0]
        self.dirs = [struct.unpack_from('<II', d, o + 112 + 8 * i) for i in range(nrva)]
        s = o + optsz
        self.sections = []
        for i in range(self.nsec):
            name = d[s:s + 8].rstrip(b'\0').decode('latin1')
            vsz, va, rsz, rptr = struct.unpack_from('<IIII', d, s + 8)
            ch = struct.unpack_from('<I', d, s + 36)[0]
            self.sections.append(dict(name=name, va=va, vsz=vsz, rptr=rptr, rsz=rsz, ch=ch))
            s += 40

    def off(self, rva):
        for s in self.sections:
            if s['va'] <= rva < s['va'] + max(s['vsz'], s['rsz']):
                return rva - s['va'] + s['rptr']
        return None

    def rva_of_off(self, off):
        for s in self.sections:
            if s['rptr'] <= off < s['rptr'] + s['rsz']:
                return off - s['rptr'] + s['va']
        return None

    def section_of(self, rva):
        for s in self.sections:
            if s['va'] <= rva < s['va'] + max(s['vsz'], s['rsz']):
                return s['name']
        return None

    def cstr(self, off):
        end = self.d.index(b'\0', off)
        return self.d[off:end].decode('latin1')

    def imports(self):
        rva, sz = self.dirs[1]
        out = {}
        if not rva:
            return out
        p = self.off(rva)
        while True:
            ilt, ts, fwd, name, iat = struct.unpack_from('<IIIII', self.d, p)
            if not (ilt or name):
                break
            dll = self.cstr(self.off(name))
            fns = []
            t = self.off(ilt or iat)
            k = 0
            while True:
                v = struct.unpack_from('<Q', self.d, t)[0]
                if not v:
                    break
                slot = iat + 8 * k
                if v >> 63:
                    fns.append(('#%d' % (v & 0xFFFF), slot))
                else:
                    fns.append((self.cstr(self.off(v & 0x7FFFFFFF) + 2), slot))
                t += 8
                k += 1
            out[dll] = fns
            p += 20
        return out

    def delay_imports(self):
        rva, sz = self.dirs[13] if len(self.dirs) > 13 else (0, 0)
        out = {}
        if not rva:
            return out
        p = self.off(rva)
        while True:
            attr, name, hmod, iat, int_, *_ = struct.unpack_from('<IIIIIIII', self.d, p)
            if not name:
                break
            dll = self.cstr(self.off(name))
            fns = []
            t = self.off(int_)
            k = 0
            while True:
                v = struct.unpack_from('<Q', self.d, t)[0]
                if not v:
                    break
                fns.append(('#%d' % (v & 0xFFFF), iat + 8 * k) if v >> 63 else (self.cstr(self.off(v & 0x7FFFFFFF) + 2), iat + 8 * k))
                t += 8
                k += 1
            out[dll] = fns
            p += 32
        return out

    def exports(self):
        rva, sz = self.dirs[0]
        if not rva:
            return []
        p = self.off(rva)
        (_, _, _, _, name, base, nfun, nnames, afun, anames, aords) = struct.unpack_from('<IIHHIIIIIII', self.d, p)
        funcs = [struct.unpack_from('<I', self.d, self.off(afun) + 4 * i)[0] for i in range(nfun)]
        res = []
        for i in range(nnames):
            n = struct.unpack_from('<I', self.d, self.off(anames) + 4 * i)[0]
            o = struct.unpack_from('<H', self.d, self.off(aords) + 2 * i)[0]
            res.append((self.cstr(self.off(n)), funcs[o]))
        return res


if __name__ == '__main__':
    pe = PE(sys.argv[1])
    print('machine=%#x timestamp=%d entry=%#x imagebase=%#x sizeofimage=%#x subsystem=%d dllchar=%#x' % (
        pe.machine, pe.timestamp, pe.entry, pe.imagebase, pe.sizeofimage, pe.subsystem, pe.dllchar))
    for s in pe.sections:
        print('  %-8s va=%#010x vsz=%#010x raw=%#010x rsz=%#010x ch=%#010x' % (s['name'], s['va'], s['vsz'], s['rptr'], s['rsz'], s['ch']))
    print('entry section:', pe.section_of(pe.entry))
    for i, (r, z) in enumerate(pe.dirs):
        if r:
            print('  dir[%d] rva=%#x size=%#x' % (i, r, z))
