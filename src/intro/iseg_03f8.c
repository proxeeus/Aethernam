/*
 * Intro segment 03f8: PAK loader.
 * PAK: dword table of entry offsets at 4*(idx+1); entry = dword R (reloc table size, 0 =
 * none) [+ R-4 bytes of table], 12-byte header {long packed, long unpacked, u8 method,
 * u8 implode flags, u16 name length}, name, data.
 */
#include "../core/mem.h"
#include "../platform/platform.h"
#include "intro_protos.h"
#include <string.h>

/* 03f8:000c  loads entry idx of <name>.PAK into a new block and returns it (NULL if the
 * block cannot be allocated); method 0 = stored, 1 = imploded (decompressed in place
 * from the end of the block); then relocates the offset tables of the sub-blocks listed
 * in the reloc table into far pointers */
uint8_t *intro_03f8_000c(uint8_t *name, int16_t idx)
{
    char path[0x40];                            /* [bp-0x1ea] */
    uint8_t tab[0x194];                         /* [bp-0x194]: R then R-4 bytes of table */
    uint8_t off4[4];                            /* [bp-0x1b0] */
    uint8_t hdr[12];                            /* [bp-0x1a8] */
    int32_t packed, unpacked, x, n, m, i, j;
    uint8_t method, info, *buf, *q;
    uint16_t skip;
    FILE *fh;

    memset(tab, 0, sizeof tab);
    intro_03f1_0005(path, (const char *)name, DSSTR(0x142));   /* ".PAK" */
    fh = intro_032c_0035(path, 0x3ed);
    if (!fh) return NULL;   /* TODO(port): the original goes on with uninitialised locals */

    intro_032c_000a(fh, (uint16_t)((idx + 1) << 2), -1);
    intro_032c_0195(fh, off4, 4);
    intro_032c_000a(fh, P32(off4), -1);
    intro_032c_0195(fh, tab, 4);
    x = P32(tab);
    if (x != 0)   /* TODO(port): a table > 0x194 bytes overflowed the original's locals */
        intro_032c_0195(fh, tab + 4, (x > (int32_t)sizeof tab ? (int32_t)sizeof tab : x) - 4);
    intro_032c_0195(fh, hdr, 12);
    packed = P32(hdr);
    unpacked = P32(hdr + 4);
    method = hdr[8];
    info = hdr[9];
    skip = PU16(hdr + 10);
    intro_032c_000a(fh, skip, 0);

    /* the original allocates the largest free block (farmalloc(farcoreleft())) and
     * shrinks it to `unpacked` afterwards; allocate what is needed instead */
    buf = intro_03e5_000c((packed > unpacked ? packed : unpacked) + 0x12c);
    if (!buf) {
        intro_032c_022f(fh);
        return NULL;
    }
    switch (method) {
    case 1: {
        uint8_t *src = buf + (unpacked + 0x12c - packed);
        intro_032c_0195(fh, src, packed);
        intro_032c_022f(fh);                    /* the original closes the handle twice */
        fh = NULL;
        intro_042e_0314(info, src, buf, unpacked, buf + unpacked + 0x12c);
        intro_03e5_0081(buf, unpacked);
        break;
    }
    case 0:
        intro_032c_0195(fh, buf, packed);
        intro_03e5_0081(buf, unpacked);
        break;
    default:
        break;
    }
    if (fh) intro_032c_022f(fh);

    if (x != 0) {
        n = (int32_t)((uint32_t)x >> 2);
        if (n > (int32_t)(sizeof tab / 4)) n = (int32_t)(sizeof tab / 4);
        for (i = 0; i < n; i++) {
            q = buf + (P32(tab + i * 4) - x);
            m = (int32_t)((uint32_t)P32(q) >> 2);
            for (j = 0; j < m; j++)
                FP_SET(q + j * 4, q + P32(q + j * 4));
        }
    }
    return buf;
}
