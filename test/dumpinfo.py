import sys
from minidump.minidumpfile import MinidumpFile
mf = MinidumpFile.parse(sys.argv[1])
r = mf.get_reader()
exc = mf.exception
print('EXCEPTION:', exc)
mods = sorted([(m.baseaddress, m.baseaddress + m.size, m.name.split('\\')[-1]) for m in mf.modules.modules])
def mod_of(a):
    for b, e, n in mods:
        if b <= a < e: return '%s+0x%x' % (n, a - b)
    return '?'
if exc:
    for e in exc.exception_records:
        rec = e.ExceptionRecord
        print('code=%#x addr=%#x (%s) tid=%d params=%s' % (rec.ExceptionCode_raw if hasattr(rec,'ExceptionCode_raw') else 0, rec.ExceptionAddress, mod_of(rec.ExceptionAddress), e.ThreadId, [hex(x) for x in rec.ExceptionInformation[:rec.NumberParameters]]))
