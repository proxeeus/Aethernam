import sys, json, re, struct
sys.path.insert(0, 'tools')
from disasm import Dis
path, dgroup, out = sys.argv[1], int(sys.argv[2],16), sys.argv[3]
names = {}
if len(sys.argv) > 4:
    for line in open(sys.argv[4]):
        line=line.split('#')[0].split()
        if len(line)>=2: names[int(line[0],16)] = line[1]
d = Dis(path, dgroup); m = d.m
entries = [(m.cs, m.ip)]
codesegs = [s for s in m.segvals if s < dgroup]
for seg, off in m.relocs:
    l = seg*16+off
    if l >= dgroup*16:
        sv = m.w(l); o = m.w(l-2)
        if sv in codesegs and sv*16+o < dgroup*16: entries.append((sv, o))
extra = [(int(a,16),int(b,16)) for a,b in (x.split(':') for x in open('re/extra_entries.txt').read().split())] if len(sys.argv)>5 else []
d.run(entries + extra)
img = d.img; DS = dgroup*16
def fname(l):
    if l in names: return names[l]
    s,o = d.funcs.get(l, (l>>4, l&15))
    return 'sub_%04x_%04x' % (s, o)
def dstr(off):
    a = DS+off
    if a >= len(img): return None
    s = bytearray()
    while a < len(img) and 32 <= img[a] < 127 and len(s) < 40: s.append(img[a]); a+=1
    if len(s) >= 3 and (a>=len(img) or img[a]==0): return s.decode()
    return None
byfunc = {}
for l, f in d.owner.items(): byfunc.setdefault(f, []).append(l)
o = open(out, 'w')
for f in sorted(d.funcs):
    seg, off = d.funcs[f]
    ls = sorted(byfunc.get(f, []))
    callers = sorted(d.xrefs.get(f, []))
    o.write('\n;' + '='*70 + '\n; %s  (%04x:%04x lin %05x)  size~%d  callers: %s\n' % (fname(f), seg, off, f, sum(d.insns[x].size for x in ls), ', '.join(fname(c) for c in callers[:8])))
    for l in ls:
        i = d.insns[l]
        if l in d.labels and l != f: o.write('%s:\n' % d.labels[l])
        ops = i.op_str
        cmt = ''
        if i.mnemonic in ('call',) and i.operands[0].type == 2:
            ops = fname(seg*16 + (i.operands[0].imm & 0xffff))
        elif i.mnemonic == 'lcall' and len(i.operands) == 2 and i.operands[0].type == 2:
            ops = fname(i.operands[0].imm*16 + i.operands[1].imm)
        elif i.mnemonic.startswith('j') or i.mnemonic.startswith('loop'):
            if i.operands and i.operands[0].type == 2:
                t = seg*16 + (i.operands[0].imm & 0xffff); ops = d.labels.get(t, fname(t))
        for mm in re.finditer(r'0x([0-9a-f]{2,4})', i.op_str):
            v = int(mm.group(1),16); s = dstr(v)
            if s and v >= 0x80 and img[DS+v-1] == 0 and 'ptr' not in i.op_str: cmt += ' "%s"' % s
        if l+1 in m.reloc_lin or l+2 in m.reloc_lin or l+3 in m.reloc_lin: cmt += ' [seg]'
        o.write('  %05x  %-7s %s%s\n' % (l, {'cwde':'cbw','cdq':'cwd'}.get(i.mnemonic,i.mnemonic), ops, (' ;' + cmt) if cmt else ''))
    for t, n in d.jtables.items():
        if t in range(ls[0] if ls else 0, (ls[-1] if ls else 0) + 64):
            o.write('  ; jump table %05x: %s\n' % (t, ' '.join('%04x' % struct.unpack_from('<H', img, t+2*k)[0] for k in range(n))))
print(len(d.funcs))
