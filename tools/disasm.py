"""Recursive-descent disassembler for Turbo C large-model MZ images."""
import sys, struct, json, re, capstone
from capstone import x86
from mz import MZ

class Dis:
    def __init__(self, path, dgroup):
        self.m = MZ(path); self.img = self.m.image; self.dg = dgroup
        self.md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_16); self.md.detail = True
        self.insns = {}      # lin -> insn
        self.funcs = {}      # lin -> (seg, off)
        self.labels = {}     # lin -> name
        self.jtables = {}    # lin of table -> count
        self.xrefs = {}      # func lin -> set of caller func lins
        self.indirect = []
        self.owner = {}      # lin -> func lin
    def lin(self, seg, off): return seg*16 + off
    def add_func(self, seg, off, caller=None):
        l = self.lin(seg, off)
        if caller is not None: self.xrefs.setdefault(l, set()).add(caller)
        if l not in self.funcs:
            self.funcs[l] = (seg, off); self.queue.append(l)
    def decode(self, l):
        if l in self.insns: return self.insns[l]
        if l >= len(self.img): return None
        seg, _ = self.cur
        off = l - seg*16
        for i in self.md.disasm(bytes(self.img[l:l+16]), off, count=1):
            self.insns[l] = i; return i
        return None
    def run(self, entries):
        self.queue = []
        for s,o in entries: self.add_func(s,o)
        while self.queue:
            f = self.queue.pop()
            self.cur = self.funcs[f]
            self.walk(f)
    def walk(self, f):
        seg, _ = self.cur
        work = [f]; seen = set()
        while work:
            l = work.pop()
            while True:
                if l in seen: break
                if l in self.owner and self.owner[l] != f and l in self.funcs: break
                seen.add(l)
                i = self.decode(l)
                if i is None: break
                self.owner.setdefault(l, f)
                nl = l + i.size
                m = i.mnemonic
                ops = i.operands
                if m in ('ret','retf','iret','hlt'): break
                if m == 'call':
                    if ops[0].type == x86.X86_OP_IMM:
                        self.add_func(seg, ops[0].imm & 0xffff, f)
                    else: self.indirect.append((l, i.op_str))
                    l = nl; continue
                if m == 'lcall':
                    if ops[0].type == x86.X86_OP_IMM and len(ops) == 2:
                        self.add_func(ops[0].imm, ops[1].imm, f)
                    else: self.indirect.append((l, i.op_str))
                    l = nl; continue
                if m == 'ljmp':
                    if ops[0].type == x86.X86_OP_IMM and len(ops) == 2:
                        self.add_func(ops[0].imm, ops[1].imm, f)
                    break
                if m == 'jmp':
                    if ops[0].type == x86.X86_OP_IMM:
                        t = seg*16 + (ops[0].imm & 0xffff)
                        self.labels.setdefault(t, 'loc_%05x' % t); work.append(t); break
                    # jump table: jmp word ptr cs:[bx + X]
                    mm = re.match(r'word ptr cs:\[bx \+ (0x[0-9a-f]+)\]', i.op_str)
                    if mm:
                        tbl = seg*16 + int(mm.group(1),16)
                        n = self.guess_table_len(l)
                        self.jtables[tbl] = n
                        for k in range(n):
                            t = seg*16 + struct.unpack_from('<H', self.img, tbl+2*k)[0]
                            self.labels.setdefault(t, 'loc_%05x' % t); work.append(t)
                    else: self.indirect.append((l, i.op_str))
                    break
                if m.startswith('j') or m in ('loop','loope','loopne','jcxz'):
                    t = seg*16 + (ops[0].imm & 0xffff)
                    self.labels.setdefault(t, 'loc_%05x' % t); work.append(t)
                l = nl
    def guess_table_len(self, l):
        # look back for 'cmp bx, N' / 'cmp ax, N' within the previous few instructions
        best = None
        for back in range(3, 30):
            for i in self.md.disasm(bytes(self.img[l-back:l]), 0):
                if i.mnemonic == 'cmp' and i.operands[1].type == x86.X86_OP_IMM:
                    best = i.operands[1].imm + 1
        return best if best and best < 256 else 1

if __name__ == '__main__':
    path, dgroup, out = sys.argv[1], int(sys.argv[2],16), sys.argv[3]
    d = Dis(path, dgroup)
    m = d.m
    entries = [(m.cs, m.ip)]
    # far code pointers stored in data: relocations inside DGROUP pointing to code segs
    codesegs = [s for s in m.segvals if s < dgroup]
    for seg, off in m.relocs:
        l = seg*16+off
        if l >= dgroup*16:
            sv = d.m.w(l); o = d.m.w(l-2)
            if sv in codesegs and sv*16+o < dgroup*16: entries.append((sv, o))
    d.run(entries)
    json.dump({'funcs': {hex(k): v for k,v in sorted(d.funcs.items())},
               'xrefs': {hex(k): sorted(hex(x) for x in v) for k,v in d.xrefs.items()},
               'indirect': d.indirect, 'jtables': {hex(k): v for k,v in d.jtables.items()}},
              open(out+'.json','w'), indent=1)
    print(len(d.funcs), 'functions', len(d.insns), 'insns', len(d.indirect), 'indirect')
    covered = sum(i.size for i in d.insns.values())
    print('coverage bytes', covered, 'of code', dgroup*16)
