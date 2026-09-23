"""Dev-only test oracle: run the ORIGINAL AdLib driver machine code (AVE.CC1 entry 0) under
Unicorn with the same call sequence as tests/test_adlib.c and dump its OPL writes.
Never used by the app - only to verify the C reimplementation byte for byte."""
import sys, struct
sys.path.insert(0, 'tools')
from unicorn import *
from unicorn.x86_const import *
from bpe import bpe_unpack, cc_entries
from pak import pak_load

data_dir, song_idx, ticks_total, out = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), sys.argv[4]
sfx_on = not (len(sys.argv) > 5 and sys.argv[5] == 'nosfx')
cc = open(data_dir + '/AVE.CC1', 'rb').read()
off, p, u = cc_entries(cc)[0]
drv, _ = bpe_unpack(cc, off)

DSEG, SONG, BANK, STK = 0x1000, 0x2000, 0x3000, 0x9000
mu = Uc(UC_ARCH_X86, UC_MODE_16)
mu.mem_map(0, 0x100000)
mu.mem_write(DSEG * 16 + 0x100, drv)
mu.mem_write(0x500, b'\xf4')                               # hlt: return stub
song, _ = pak_load(data_dir + '/MUS.PAK', song_idx)
bank, _ = pak_load(data_dir + '/MUS.PAK', 1)
mu.mem_write(SONG * 16, song + bytes(0x5208 - len(song)))
mu.mem_write(BANK * 16, bank + bytes(0x2968 - len(bank)))

writes = []; label = [0]; addr = [0]
def hook_out(uc, port, size, value, ud):
    if port == 0x388: addr[0] = value & 0xff
    elif port == 0x389: writes.append((label[0], addr[0], value & 0xff))
def hook_in(uc, port, size, ud): return 0
mu.hook_add(UC_HOOK_INSN, hook_out, None, 1, 0, UC_X86_INS_OUT)
mu.hook_add(UC_HOOK_INSN, hook_in, None, 1, 0, UC_X86_INS_IN)

def call(ax, bx=0, cx=0, dx=0, si=0, es=0):
    for r, v in ((UC_X86_REG_AX, ax), (UC_X86_REG_BX, bx), (UC_X86_REG_CX, cx), (UC_X86_REG_DX, dx),
                 (UC_X86_REG_SI, si), (UC_X86_REG_DI, 0), (UC_X86_REG_BP, 0), (UC_X86_REG_ES, es),
                 (UC_X86_REG_DS, 0), (UC_X86_REG_SS, STK), (UC_X86_REG_SP, 0xfff0 - 6)):
        mu.reg_write(r, v)
    mu.mem_write(STK * 16 + 0xfff0 - 6, struct.pack('<HHH', 0x500, 0, 0x202))   # ip, cs, flags for iret
    mu.reg_write(UC_X86_REG_CS, DSEG)
    mu.emu_start(DSEG * 16 + 0xf1f, 0x500, count=5_000_000)
    return mu.reg_read(UC_X86_REG_DX) << 16 | mu.reg_read(UC_X86_REG_AX)

def bank8(o): return bank[o] if o < len(bank) else 0
def bank16(o): return bank8(o) | bank8((o + 1) & 0xffff) << 8
def sfx(effect, cmd):
    si = bank16(0x0e)
    bx = (bank16((si + effect * 2) & 0xffff) + si) & 0xffff
    mask = bank16(bx); bx = (bx + 2) & 0xffff; bit = 1
    for n in range(11):
        if mask & bit:
            track = bank8(bx); bx = (bx + 1) & 0xffff
            call(0x0c00, bx, track, cmd, bit)
        bit <<= 1

call(0x0200); call(0x1000)                    # sub_0e9f_00c1
call(0x0800, dx=BANK, si=0, es=BANK)          # sub_0e9f_017e
call(0x0a00, cx=0, dx=0x40, si=0)             # stop
call(0x0600, dx=SONG, si=0, es=SONG); call(0x0400)   # sub_0e9f_01a8
call(0x0a00, cx=100, dx=0x2000)               # volume
call(0x0a00, cx=0, dx=0x80)                   # start
for t in range(1, ticks_total + 1):
    label[0] = t
    call(0x0000)
    if sfx_on:
        if t == 300:
            call(0x0a00, cx=0x32, dx=0x2000); call(0x0a00, cx=100, dx=0x8000)
            sfx(0, 0x40); sfx(3, 0x80)
        if t == 600: sfx(10, 0x80)
        if t == 720: sfx(120, 0x2000)
with open(out, 'w') as f:
    for t, r, v in writes: f.write('%d %02x %02x\n' % (t, r, v))
print(len(writes), 'writes')
