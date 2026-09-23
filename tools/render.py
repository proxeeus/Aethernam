"""Render RLE sprites from a resource block to PNG (for inspection / icon)."""
import sys, struct, zlib
sys.path.insert(0, 'tools')
from pak import pak_load

def png(path, w, h, rgb):
    raw = b''.join(b'\x00' + bytes(rgb[y*w*3:(y+1)*w*3]) for y in range(h))
    def chunk(t, d): return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    open(path, 'wb').write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))

def sprite(buf, off):
    w = buf[off] * 16; h = buf[off+1]; p = off + 2
    img = [[0]*w for _ in range(h)]
    for y in range(h):
        n = buf[p]; p += 1; x = 0
        for _ in range(n):
            skip, n4, n1 = buf[p], buf[p+1], buf[p+2]; p += 3
            x += skip
            cnt = 4*n4 + n1
            for k in range(cnt):
                if x+k < w: img[y][x+k] = buf[p+k]
            p += cnt; x += cnt
        p += 1
    return w, h, img

if __name__ == '__main__':
    pak, ent, sub = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
    d, _ = pak_load(pak, ent)
    pal_d, _ = pak_load(sys.argv[4], int(sys.argv[5])) if len(sys.argv) > 5 else (None, None)
    dirsz = struct.unpack_from('<I', d, 0)[0]
    dirs = struct.unpack_from('<%dI' % (dirsz//4), d, 0)
    base = dirs[sub]
    pal = pal_d if pal_d else d[dirs[4]:dirs[4]+768]
    n = struct.unpack_from('<I', d, base)[0] // 4
    offs = struct.unpack_from('<%dI' % n, d, base)
    sprites = []
    for o in offs:
        try: sprites.append(sprite(d, base + o))
        except Exception as e: pass
    W = 640; x = y = rowh = 0; placed = []
    for w, h, img in sprites:
        if x + w > W: x = 0; y += rowh + 2; rowh = 0
        placed.append((x, y, w, h, img)); x += w + 2; rowh = max(rowh, h)
    H = y + rowh
    rgb = bytearray(W*H*3)
    for px, py, w, h, img in placed:
        for yy in range(h):
            for xx in range(w):
                c = img[yy][xx]
                if c:
                    i = ((py+yy)*W + px+xx)*3
                    rgb[i:i+3] = bytes(pal[c*3:c*3+3])
    png(sys.argv[-1] if sys.argv[-1].endswith('.png') else 'out.png', W, H, rgb)
    print(len(sprites), 'sprites', W, H)
