/*
 * Segment 0c66 - PAK resource loader.
 *
 * PAK file: long table at offset 0; entry #i offset stored at (i+1)*4.  Entry layout:
 *   long R (size of the relocation table incl. this long) + R-4 bytes more of table (longs),
 *   12-byte header {long packed, long unpacked, u8 method, u8 implode flags, u16 skip},
 *   `skip` bytes, then the data (method 0 = stored, 1 = PKWARE implode).
 * Relocation: every table entry points (relative to table[0]) to a sub-block that starts with
 * a table of long offsets (first long = table size); those offsets become far pointers.
 */
#include "../core/mem.h"
#include "../platform/platform.h"
#include "protos.h"

/* 0c66:0003 - load entry #index of "<name>.PAK" into a new block, relocate its pointer tables */
uint8_t *sub_0c66_0003(uint8_t *name, int16_t index)
{
    uint8_t path[0x42];
    uint8_t tbl[0x190];        /* relocation table (max 100 longs in the original stack frame) */
    uint8_t hdr[12];
    uint8_t ent[4];
    int32_t h, packed, unpacked, rsize;
    uint8_t *buf, *src, *q;
    int16_t cnt, n, i, j;

    sub_0f32_000d(path, name, DSADDR(0x23ba));          /* ".PAK" */
    h = sub_0e66_0031(path, 0x3ed);
    if (h == 0) {
        /* TODO(port): the original falls through to the relocation loop with an uninitialised
           table and returns an uninitialised pointer; we return NULL (callers retry / abort). */
        return NULL;
    }

    sub_0e66_0006(h, (int32_t)(uint16_t)((index + 1) << 2), -1);
    sub_0e66_0191(h, ent, 4);
    sub_0e66_0006(h, P32(ent), -1);
    sub_0e66_0191(h, tbl, 4);
    rsize = P32(tbl);
    if (rsize != 0) {
        int32_t more = rsize - 4;
        if (more > (int32_t)sizeof(tbl) - 4)
            more = (int32_t)sizeof(tbl) - 4;          /* port: original would overflow its stack frame */
        sub_0e66_0191(h, tbl + 4, more);
    }
    sub_0e66_0191(h, hdr, 12);
    packed = P32(hdr + 0);
    unpacked = P32(hdr + 4);
    sub_0e66_0006(h, (int32_t)PU16(hdr + 10), 0);         /* SEEK_CUR skip */

    /* original: farmalloc(farcoreleft()) if farcoreleft() >= unpacked + 0x10cc */
    buf = sub_0f27_0004(unpacked + 0x10cc);
    if (buf == NULL) {
        sub_0e66_022b(h);
        return NULL;
    }

    if (hdr[8] == 0) {
        sub_0e66_0191(h, buf, packed);
        sub_0f27_0079(buf, unpacked);
        sub_0e66_022b(h);
    } else if (hdr[8] == 1) {
        src = buf + (unpacked + 0x12c - packed);
        sub_0e66_0191(h, src, packed);
        sub_0e66_022b(h);
        sub_0f48_0314(hdr[9], src, buf, unpacked, buf + unpacked + 0x12c);
        sub_0f27_0079(buf, unpacked);
        /* port: the original closes the (already closed) handle a second time here */
    } else {
        sub_0e66_022b(h);
    }

    if (rsize != 0) {
        cnt = (int16_t)((uint32_t)rsize >> 2);
        if (cnt > (int16_t)(sizeof(tbl) / 4))
            cnt = (int16_t)(sizeof(tbl) / 4);                /* port: stay inside tbl */
        for (i = 0; i < cnt; i++) {
            q = buf + (P32(tbl + i * 4) - P32(tbl));
            n = (int16_t)((uint32_t)P32(q) >> 2);
            for (j = 0; j < n; j++)
                FP_SET(q + j * 4, q + P32(q + j * 4));
        }
    }
    return buf;
}
