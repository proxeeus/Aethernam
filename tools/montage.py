"""montage.py out.png cols files... - tile 320x200 RGB PNGs (from the debug dumper)"""
import sys, zlib, struct
sys.path.insert(0, 'tools')
from render import png
def load(f):
    d = open(f, 'rb').read(); i = d.index(b'IDAT'); n = struct.unpack('>I', d[i-4:i])[0]
    raw = zlib.decompress(d[i+4:i+4+n]); return [raw[y*961+1:(y+1)*961] for y in range(200)]
out, cols, files = sys.argv[1], int(sys.argv[2]), sys.argv[3:]
rows = (len(files) + cols - 1) // cols
W, H = cols * 322, rows * 202
img = bytearray(W * H * 3)
for k, f in enumerate(files):
    r = load(f); ox = (k % cols) * 322; oy = (k // cols) * 202
    for y in range(200):
        img[((oy+y)*W+ox)*3:((oy+y)*W+ox)*3+960] = r[y]
png(out, W, H, img)
