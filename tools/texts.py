"""Decrypt the language text banks (T/E/D/I.CC4): int32 offset table, entries XOR-free
subtractive cipher buf[i] -= 0x5f*(i+1) (seg 0053:0895). Texts are '*'-separated."""
import struct, sys
def bank_entries(path):
    d = open(path, 'rb').read()
    first = struct.unpack_from('<I', d, 0)[0]
    offs = list(struct.unpack_from('<%dI' % (first // 4), d, 0)) + [len(d)]
    out = []
    for i in range(len(offs) - 1):
        a, b = offs[i], offs[i + 1]
        raw = d[a:b]
        out.append((i, bytes((c - 0x5f * (k + 1)) & 0xff for k, c in enumerate(raw))))
    return out
def text(buf, idx):
    """sub_03dd_165d: skip idx+1 '*' separators (its counter starts at -1), then past the next NUL"""
    p = 0
    for _ in range(idx + 1):
        p = buf.index(b'*', p) + 1
    p = buf.index(b'\0', p) + 1
    e = buf.find(b'*', p)
    return buf[p:e if e >= 0 else len(buf)]
if __name__ == '__main__':
    for i, b in bank_entries(sys.argv[1]):
        print(i, len(b), b[:80])
