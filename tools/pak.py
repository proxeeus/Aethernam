"""Eternam .PAK reader + implode decompressor (reversed from AVE 0f48:0314 / 0fa2:0008)."""
import struct, sys, os

class Bits:
    def __init__(self, data, pos):
        self.d = data; self.pos = pos; self.buf = 0; self.n = 0
    def get(self, n):
        while self.n < n:
            b = self.d[self.pos] if self.pos < len(self.d) else 0
            self.buf |= b << self.n; self.n += 8; self.pos += 1
        v = self.buf & ((1 << n) - 1); self.buf >>= n; self.n -= n
        return v

def read_tree(data, pos, n):
    cnt = data[pos] + 1; pos += 1
    ent = []; val = 0
    for _ in range(cnt):
        b = data[pos]; pos += 1
        for _ in range((b >> 4) + 1):
            ent.append([0, val, (b & 15) + 1]); val += 1
    ent = ent[:n]
    ent.sort(key=lambda e: (e[2], e[1]))
    code = 0; inc = 0; last = 0
    for e in reversed(ent):
        code = (code + inc) & 0xffff
        if e[2] != last:
            last = e[2]; inc = 1 << (16 - last)
        e[0] = int('{:016b}'.format(code)[::-1], 2)
    # build tree: node = [c1, c0, leaf1, leaf0]
    nodes = [[0, 0, False, False]]
    for c, v, ln in ent:
        cur = 0; parent = 0; bit = 0
        for _ in range(ln):
            bit = c & 1; c >>= 1
            slot = 0 if bit else 1
            if nodes[cur][slot] == 0 or nodes[cur][slot+2]:
                nodes.append([0, 0, False, False])
                nodes[cur][slot] = len(nodes) - 1
            parent = cur; cur = nodes[cur][slot]
        slot = 0 if bit else 1
        nodes[parent][slot] = v; nodes[parent][slot+2] = True
    return nodes, pos

def decode(nodes, bits):
    cur = 0
    while True:
        slot = 0 if bits.get(1) else 1
        if nodes[cur][slot+2]: return nodes[cur][slot]
        cur = nodes[cur][slot]

def explode(data, pos, outsize, flags):
    minlen = 2; dbits = 6
    lit = None
    if flags & 2: dbits = 7
    if flags & 4:
        minlen = 3; lit, pos = read_tree(data, pos, 256)
    ln, pos = read_tree(data, pos, 64)
    ds, pos = read_tree(data, pos, 64)
    b = Bits(data, pos); out = bytearray()
    stats = {'d1': 0}
    while len(out) < outsize:
        if b.get(1):
            out.append(decode(lit, b) if lit else b.get(8))
        else:
            low = b.get(dbits)
            dist = ((decode(ds, b) << dbits) | low) + 1
            l = decode(ln, b) + minlen
            if l == 63 + minlen: l += b.get(8)
            if dist == 1 and l > 1: stats['d1'] += 1
            for _ in range(min(l, outsize - len(out))):
                p = len(out) - dist
                out.append(out[p] if p >= 0 else 0)
    return bytes(out), stats

def pak_entries(path):
    d = open(path, 'rb').read()
    first = struct.unpack_from('<I', d, 4)[0]
    n = first // 4 - 1
    res = []
    for i in range(n):
        off = struct.unpack_from('<I', d, 4 + 4*i)[0]
        x = struct.unpack_from('<I', d, off)[0]
        sub = list(struct.unpack_from('<%dI' % ((x-4)//4), d, off+4)) if x else []
        h = off + (x if x else 4)
        packed, unpacked, method, info, doff = struct.unpack_from('<IIBBH', d, h)
        name = d[h+12:h+12+doff]
        res.append(dict(idx=i, off=off, x=x, sub=sub, packed=packed, unpacked=unpacked,
                        method=method, info=info, data=h+12+doff, name=name))
    return d, res

def pak_load(path, i):
    d, ents = pak_entries(path)
    e = ents[i]
    if e['method'] == 0: return d[e['data']:e['data']+e['unpacked']], e
    out, st = explode(d, e['data'], e['unpacked'], e['info'])
    e['stats'] = st
    return out, e

if __name__ == '__main__':
    for p in sys.argv[1:]:
        d, ents = pak_entries(p)
        for e in ents:
            out, e2 = pak_load(p, e['idx'])
            print(os.path.basename(p), e['idx'], 'm%d i%d' % (e['method'], e['info']), e['packed'], e['unpacked'], 'x=%d' % e['x'], e['name'][:20], e2.get('stats'))
