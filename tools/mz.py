import struct
class MZ:
    def __init__(self, path):
        d = open(path,'rb').read()
        h = struct.unpack_from('<14H', d, 0)
        self.nreloc, self.hdrpar = h[3], h[4]
        size = (h[2]-1)*512 + (h[1] or 512)
        self.image = bytearray(d[self.hdrpar*16:size])
        self.ss, self.sp, self.ip, self.cs = h[7], h[8], h[10], h[11]
        self.minalloc = h[5]
        self.relocs = []
        for i in range(self.nreloc):
            off, seg = struct.unpack_from('<HH', d, h[12]+4*i)
            self.relocs.append((seg, off))
        self.reloc_lin = set(seg*16+off for seg,off in self.relocs)
        self.segvals = sorted(set(struct.unpack_from('<H', self.image, a)[0] for a in self.reloc_lin))
    def w(self, lin): return struct.unpack_from('<H', self.image, lin)[0]
