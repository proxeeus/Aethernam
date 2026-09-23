"""Disassembler for Eternam room scripts (R??.CC4), per the decompiled interpreter (seg 03dd)."""
import struct, sys
sys.path.insert(0, 'tools')
from texts import bank_entries, text

def rooms(path):
    d = open(path, 'rb').read()
    first = struct.unpack_from('<I', d, 0)[0]
    offs = list(struct.unpack_from('<%dI' % (first // 4), d, 0)) + [len(d)]
    return {i: d[offs[i]:offs[i + 1]] for i in range(len(offs) - 1)}

NAMES = {0:'ANIM',1:'FACE',2:'STOP',3:'GOTO',4:'WALKX',5:'WALKY',6:'LOOP',7:'PLACE',8:'SYNC',9:'WAIT',0x0a:'IF',
 0x0e:'IF_HERO_IN',0x0f:'WALK_HERO_X',0x10:'WALK_HERO_Y',0x11:'WALK_NEAR_HERO',0x12:'LOCK',0x13:'UNLOCK',0x14:'SAY',
 0x15:'FACE_HERO',0x16:'BIND',0x17:'FLOOR',0x18:'DOORS',0x19:'TAB_BC0',0x1a:'OBJ8',0x1b:'COPYPROT',0x1c:'START',
 0x1d:'KILL',0x1e:'START_LIST',0x1f:'ANIMPAK',0x20:'SET',0x21:'ADD',0x22:'SUB',0x23:'IGNORE_COLL',0x24:'WALKTO',
 0x25:'FX25',0x26:'LIGHT',0x27:'WAIT_HERO_OUT',0x28:'WAIT_HERO_IN',0x29:'RELEASE',0x2a:'XRANGE',0x2b:'YRANGE',
 0x2c:'SET_D0',0x2d:'SETANIM',0x2e:'SFX',0x2f:'SPRBANK',0x30:'TO_HERO',0x32:'NOP3',0x33:'EFFECT',0x34:'SET_177C',
 0x35:'MENU',0x36:'SHOW',0x37:'FLAG173E',0x38:'OR',0x39:'AND',0x3a:'BACKDROP',0x3b:'SET_C4',0x3c:'OBST',
 0x3d:'IF_TALKED',0x3e:'RECT',0x3f:'OBST_SET',0x40:'COLOR',0x41:'BUBBLE_Y',0x42:'ACTION',0x43:'RESTART_GOTO',
 0x44:'RECT3020',0x45:'SET14',0x46:'SET_AC',0x47:'SCENE8',0x48:'MUSIC',0x49:'VOLUME',0x4a:'RANDOM',0x4b:'ROOM',
 0x4c:'RECT_DE',0x4d:'SAY_OTHER',0x4e:'END_GAME',0x4f:'OBJ16',0x50:'SETFLAG',0x51:'WAIT_KEY'}
CMP = ['==', '!=', '>', '>=', '<', '<=', '&', '|']

def dis(code, start, end, texts=None):
    out = []; p = start
    u8 = lambda: code[p]
    while p < end:
        a = p; op = code[p]; p += 1; args = []
        def b():
            nonlocal p; v = code[p]; p += 1; return v
        def w():
            nonlocal p; v = struct.unpack_from('<h', code, p)[0]; p += 2; return v
        def rel():
            nonlocal p; v = struct.unpack_from('<h', code, p)[0]; t = p + v; p += 2; return 'L%04x' % t
        def val():
            nonlocal p
            if code[p] == 0x0b: p += 1; return 'v%d' % w()
            tag = b()
            return str(b()) if tag == 0x0c else str(w())
        n = NAMES.get(op, 'OP%02x' % op)
        if op in (0, 1, 8, 9, 0x12, 0x1c, 0x1d, 0x26, 0x2c, 0x2e, 0x2f, 0x34, 0x3b, 0x40, 0x41, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x50): args = [b()]
        elif op in (4, 5): args = [b() * 2 if op == 4 else b(), 'anim%d' % b()]
        elif op in (0x0f, 0x10): args = ['anim%d' % b()]
        elif op in (3, 0x43, 0x3d): args = [rel()]
        elif op == 6: args = [rel(), 'x%d' % b()]
        elif op in (7, 0x24): args = [b() * 2, b()]
        elif op == 0x0a: args = ['v%d' % w(), CMP[b() & 7], val(), '->else ' + rel()]
        elif op == 0x0e: args = [b() * 2, b(), b() * 2, b(), '->else ' + rel()]
        elif op in (0x11, 0x16, 0x2b, 0x2d, 0x3a, 0x3f): args = [b(), b()]
        elif op in (0x14, 0x36):
            t = w(); args = ['text%d' % t, b()]
            if texts: args.append(repr(text(texts, t)[:70].decode('latin1')))
        elif op == 0x4d:
            ac = b(); t = w(); args = ['actor%d' % ac, 'text%d' % t, b()]
            if texts: args.append(repr(text(texts, t)[:70].decode('latin1')))
        elif op == 0x17: n2 = b(); args = [(b() * 2, b()) for _ in range(n2)]
        elif op == 0x18: n2 = b(); args = [(b(), b(), b() * 2, b() * 2) for _ in range(n2)]
        elif op == 0x19:
            m = b(); k = 2 if m == 2 else 1; args = [m] + [[b(), b(), b(), b() * 2, b()] for _ in range(k)]
        elif op in (0x1a, 0x4f): args = [b() if op == 0x1a else w(), b() * 2, b()]
        elif op == 0x1e:
            while True:
                v = b()
                if v == 0xff: break
                args.append(v)
        elif op == 0x1f:
            args = [b(), b(), w(), b()]; nc = b(); p += nc * 3; args.append('%d cues' % nc)
        elif op in (0x20, 0x21, 0x22, 0x38, 0x39): args = ['v%d' % w(), val()]
        elif op == 0x28 or op in (0x3e, 0x44, 0x4c): args = [b() * 2, b(), b() * 2, b()]
        elif op == 0x2a: args = [b() * 2, b() * 2]
        elif op == 0x32: args = [b(), b(), b()]
        elif op == 0x33: args = [b(), b() * 2, b()]
        elif op == 0x35:
            q = w(); x = b() * 2; y = b(); nn = b(); args = ['q=text%d' % q]
            if texts and q >= 0: args.append(repr(text(texts, q)[:50].decode('latin1')))
            for _ in range(nn):
                t = w(); args.append('text%d->%s' % (t, rel()))
                if texts: args.append(repr(text(texts, t)[:40].decode('latin1')))
        elif op == 0x3c: args = [b(), b(), b(), b()]
        out.append((a, '%04x  %-14s %s' % (a, n, ' '.join(str(x) for x in args))))
        if op in (2, 3, 0x43, 0x4e): pass
    return out

if __name__ == '__main__':
    level, room = int(sys.argv[1]), int(sys.argv[2])
    lang = sys.argv[3] if len(sys.argv) > 3 else 'E'
    code = rooms('../Eternam/R%02d.CC4' % level)[room]
    texts = dict(bank_entries('../Eternam/%s.CC4' % lang)).get(level)
    n = struct.unpack_from('<H', code, 0)[0] // 2
    starts = [struct.unpack_from('<H', code, 2 * i)[0] for i in range(n)] + [len(code)]
    for s in range(n):
        print('--- script %d @%04x' % (s, starts[s]))
        for a, line in dis(code, starts[s], starts[s + 1], texts): print('  ' + line)
