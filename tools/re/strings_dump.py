"""Dump ASCII + UTF-16LE strings with RVAs/sections. Output TSV: rva<TAB>section<TAB>enc<TAB>string"""
import re, sys
sys.path.insert(0, __import__('os').path.dirname(__file__))
from pe import PE

pe = PE(sys.argv[1])
out = open(sys.argv[2], 'w', encoding='utf-8')
d = pe.d
for m in re.finditer(rb'[\x20-\x7e]{5,}', d):
    rva = pe.rva_of_off(m.start())
    if rva is None:
        continue
    out.write('%08x\t%s\ta\t%s\n' % (rva, pe.section_of(rva), m.group().decode('latin1')))
for m in re.finditer(rb'(?:[\x20-\x7e]\x00){5,}', d):
    rva = pe.rva_of_off(m.start())
    if rva is None:
        continue
    out.write('%08x\t%s\tw\t%s\n' % (rva, pe.section_of(rva), m.group().decode('utf-16le')))
out.close()
