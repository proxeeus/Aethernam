/*
 * Intro segment 05ae: builds a reduced copy of a sprite (used at factor 7 = half size).
 * Rows are kept when bit (row counter & 15) of the row mask table cs:0x176[factor] is set;
 * each 16-pixel group of a kept row keeps factor+1 pixels (the 15 unrolled copy loops of
 * the jump table cs:0x158, expressed below as bit masks of the kept source bytes).
 * The result is a one-sprite bank: dword 6, u16 0, u16 flags, sprite header, rows.
 *
 * Code-segment variables: cs:[0xa..] row work buffer, 0x14a write pointer, 0x14c source
 * pointer, 0x14e width/16, 0x14f factor, 0x151 new width in bytes, 0x153 kept rows,
 * 0x154 row mask, 0x156 sprite flags.
 */
#include "../core/mem.h"
#include "../platform/platform.h"
#include "intro_protos.h"
#include <string.h>

static const uint16_t s_rowmask[15] = {        /* cs:0x176 */
    0x0200, 0x0202, 0x0842, 0x2222, 0x4892, 0x9254, 0xcc54, 0xaaaa,
    0xaada, 0xd6da, 0x6eee, 0xeeee, 0xfeee, 0xfefe, 0xfeff,
};
static const uint16_t s_bit[16] = {            /* cs:0x194 */
    0x0001, 0x0002, 0x0004, 0x0008, 0x0010, 0x0020, 0x0040, 0x0080,
    0x0100, 0x0200, 0x0400, 0x0800, 0x1000, 0x2000, 0x4000, 0x8000,
};
/* kept source bytes of each 16-byte group, per factor (from the code at cs:0x3e8..0x4f7) */
static const uint16_t s_colmask[15] = {
    0x0040, 0x4040, 0x4210, 0x4444, 0x4912, 0x2a49, 0x2a55, 0x5555,
    0x5b55, 0x5b6b, 0x7776, 0x7777, 0x777f, 0x7f7f, 0xff7f,
};

static uint8_t s_work[0x10000];                 /* cs:0000.. (indices wrap like cs:di) */

/* byte moves with 16-bit wrapping destination offsets (es:di) */
static inline void put(uint8_t *dst, uint16_t *di, const uint8_t *src, uint16_t n)
{
    while (n--) dst[(*di)++] = *src++;
}
static inline void fill0(uint8_t *dst, uint16_t *di, uint16_t n)
{
    while (n--) dst[(*di)++] = 0;
}

/* repe/repne scasb over s_work: returns the new index, updates *cx */
static uint16_t scas(uint16_t t, uint16_t *cx, int while_zero)
{
    while (*cx) {
        uint8_t v = s_work[t++];
        (*cx)--;
        if (while_zero ? (v != 0) : (v == 0)) break;
    }
    return t;
}

/* 05ae:01ba  writes into dest a one-sprite bank holding sprite idx of `bank` reduced by
 * `factor` (0..14: keeps factor+1 of every 16 pixels and rows); returns the end pointer */
uint8_t *intro_05ae_01ba(uint16_t idx, int16_t factor, uint8_t *bank, uint8_t *dest)
{
    uint8_t *spr = bank + PU32(bank + (uint16_t)(idx << 2));
    uint8_t *si = spr;
    uint16_t flags;
    if (!arena_owns(spr)) return dest;  /* TODO(port): damaged bank (the original drew garbage) */
    flags = PU16(spr - 2);                      /* cs:0x156 */
    uint8_t w16, dh, dl, rows_kept = 0;
    uint16_t mask = s_rowmask[factor], neww, ax, wp, cx;
    w16 = si[0];
    dh = si[1];
    si += 2;

    PU16(dest) = 6; PU16(dest + 2) = 0; PU16(dest + 4) = 0;
    ax = (uint16_t)((uint8_t)(factor + 1) * w16);   /* inc al / mul dl */
    ax = (uint16_t)((uint16_t)(ax + 0xf) >> 4);
    PU16(dest + 6) = ax;
    neww = (uint16_t)((ax & 0xff) << 4);
    wp = 8;                                     /* cs:0x14a (offset in dest) */
    cx = 0;

    do {
        if (s_bit[dh & 0xf] & mask) {
            uint16_t t = 0xa, g, bx, start, rowstart;
            uint8_t runs;
            rows_kept++;
            /* unpack the row into the work buffer */
            runs = *si++;
            do {
                uint16_t n;
                fill0(s_work, &t, si[0]);
                n = (uint16_t)((uint8_t)(si[1] << 1)) * 2 + si[2];   /* shl cl,1 is 8-bit */
                si += 3;
                put(s_work, &t, si, n);
                si += n;
            } while (--runs);
            fill0(s_work, &t, *si);
            si++;
            /* horizontal reduction, in place */
            {
                uint16_t s = 0xa, d = 0xa;
                for (g = w16; g; g--) {
                    int b;
                    for (b = 0; b < 16; b++)
                        if (s_colmask[factor] & (1u << b)) s_work[d++] = s_work[(uint16_t)(s + b)];
                    s = (uint16_t)(s + 16);
                }
                fill0(s_work, &d, 16);          /* mov cx,8 / rep stosw */
            }
            /* re-encode the row */
            t = 0xa;
            rowstart = wp;
            wp++;
            dl = 0;
            cx = neww;
            bx = t;
            for (;;) {
                t = scas(t, &cx, 1);            /* repe scasb: skip zeros */
                bx = (uint16_t)(t - bx);
                if (cx == 0) {                  /* 05e8f: end of row */
                    if (dl == 0) {
                        dl = 1;
                        fill0(dest, &wp, 3);
                    }
                    dest[wp++] = (uint8_t)bx;
                    break;
                }
                bx--; t--; cx++;
                dest[wp++] = (uint8_t)bx;       /* skip */
                bx = t;
                t = scas(t, &cx, 0);            /* repne scasb: pixels */
                bx = (uint16_t)(t - bx);
                bx--; t--; cx++;
                start = (uint16_t)(t - bx);
                dest[wp++] = (uint8_t)(bx >> 2);
                dest[wp++] = (uint8_t)(bx & 3);
                while (bx--) dest[wp++] = s_work[start++];
                bx = t;
                dl++;
                if (cx == 0) break;             /* (not reached: cx was just incremented) */
            }
            dest[rowstart] = dl;
        } else {
            /* skip a row of the source */
            uint8_t runs = *si++;
            do {
                uint16_t c;
                si++;
                c = PU16(si);
                c = (uint16_t)((c & 0xff) << 2 | ((c >> 8) & 3));
                si += c + 2;
            } while (--runs);
            si++;
        }
    } while (--dh);

    PU16(dest + 4) = flags;
    dest[7] = rows_kept;
    return dest + wp;
}
