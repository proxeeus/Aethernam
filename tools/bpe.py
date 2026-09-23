"""Infogrames Tatou BPE unpacker (reversed from TATOU.COM 0x3ef)."""
import struct, sys

def bpe_unpack(src, pos=0):
    out = bytearray()
    while True:
        npairs, more, length = src[pos], src[pos+1], struct.unpack_from('<H', src, pos+2)[0]
        pos += 4
        if npairs == 0:
            out += src[pos:pos+length]; pos += length
        else:
            code = [0]*256; left=[0]*256; right=[0]*256
            code[1:npairs+1] = src[pos:pos+npairs]; pos += npairs
            left[1:npairs+1] = src[pos:pos+npairs]; pos += npairs
            right[1:npairs+1] = src[pos:pos+npairs]; pos += npairs
            head = [0]*256; nxt = [0]*256
            for i in range(1, npairs+1):
                c = code[i]; nxt[i] = head[c]; head[c] = i
            def find(c, limit):
                k = head[c]
                while k and k >= limit: k = nxt[k]
                return k
            for _ in range(length):
                b = src[pos]; pos += 1
                stack = [(b, 256)]
                while stack:
                    c, lim = stack.pop()
                    k = find(c, lim)
                    if k == 0: out.append(c)
                    else:
                        stack.append((right[k], k)); stack.append((left[k], k))
        if not more: break
    return bytes(out), pos

def cc_entries(data):
    """Multi-entry container: BE word count, BE dword offsets, then per entry BE packed/unpacked sizes."""
    n = struct.unpack_from('>H', data, 0)[0]
    base = 2 + 4*n
    res = []
    for i in range(n):
        off = struct.unpack_from('>I', data, 2+4*i)[0] + base
        packed, unpacked = struct.unpack_from('>II', data, off)
        res.append((off+8, packed, unpacked))
    return res

if __name__ == '__main__':
    import os
    f = sys.argv[1]; outdir = sys.argv[2]
    data = open(f,'rb').read()
    for i,(off,p,u) in enumerate(cc_entries(data)):
        buf, end = bpe_unpack(data, off)
        print(f"{os.path.basename(f)}[{i}] packed={p} unpacked={u} got={len(buf)} consumed={end-off} head={buf[:2]}")
        open(os.path.join(outdir, f"{os.path.basename(f)}.{i}.bin"),'wb').write(buf)
