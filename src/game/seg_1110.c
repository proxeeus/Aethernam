/* Segment 1110: builds a shrunk (decimated) copy of an RLE sprite. */
#include "../core/mem.h"
#include "../platform/platform.h"
#include "protos.h"

/* Code-segment data of 1110 */
static uint8_t  s_buf[0x1000 + 0x20];  /* cs:0xc 320-byte row scratch (enlarged for safety) */
static uint8_t *s_out;                 /* cs:0x14c output pointer (start of current output row) */
static uint8_t *s_src;                 /* cs:0x14e source pointer */
static uint8_t  s_w16;                 /* cs:0x150 source width / 16 */
static uint16_t s_level;               /* cs:0x151 shrink level 0..14 */
static uint16_t s_neww;                /* cs:0x153 new width in pixels */
static uint8_t  s_rows;                /* cs:0x155 rows written */
static uint16_t s_mask;                /* cs:0x156 row keep mask (CS table 0x178) */
static uint16_t s_flagw;               /* cs:0x158 sprite flag word (mirror state) */

/* Column pickers (code at cs:0x3ea.. selected through the table at cs:0x15a): for level L, the
   indices kept out of each group of 16 source pixels. */
static const uint8_t s_ncols[15] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
static const uint8_t s_cols[15][15] = {
    { 6 },
    { 6, 14 },
    { 4, 9, 14 },
    { 2, 6, 10, 14 },
    { 1, 4, 8, 11, 14 },
    { 0, 3, 6, 9, 11, 13 },
    { 0, 2, 4, 6, 9, 11, 13 },
    { 0, 2, 4, 6, 8, 10, 12, 14 },
    { 0, 2, 4, 6, 8, 9, 11, 12, 14 },
    { 0, 1, 3, 5, 6, 8, 9, 11, 12, 14 },
    { 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14 },
    { 0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14 },
    { 0, 1, 2, 3, 4, 5, 6, 8, 9, 10, 12, 13, 14 },
    { 0, 1, 2, 3, 4, 5, 6, 8, 9, 10, 11, 12, 13, 14 },
    { 0, 1, 2, 3, 4, 5, 6, 8, 9, 10, 11, 12, 13, 14, 15 },
};

/* 1110:01bc  writes a 1-sprite bank at dst holding sprite idx of bank shrunk to level (0..14); returns end of written data */
uint8_t *sub_1110_01bc(uint16_t idx, uint16_t level, uint8_t *bank, uint8_t *dst)
{
    uint8_t *o = dst, *spr;
    uint8_t dh;
    uint16_t ax;

    PU16(o) = 6; PU16(o + 2) = 0; PU16(o + 4) = 0;
    o += 6;
    spr = bank + PU32(bank + (uint16_t)(idx * 4));
    s_flagw = PU16(spr - 2);
    s_src = spr + 2;
    s_level = level;
    s_mask = (uint16_t)CS16(0x1110, 0x178 + level * 2);
    s_w16 = spr[0];
    ax = (uint16_t)((uint8_t)(level + 1) * s_w16);
    ax = (uint16_t)((uint16_t)(ax + 0xf) >> 4);
    PU16(o) = ax;                   /* new width/16, height patched at the end */
    o += 2;
    s_neww = (uint16_t)((ax & 0xff) << 4);
    s_rows = 0;
    s_out = o;
    dh = spr[1];

    do {
        uint8_t *s = s_src;
        if ((uint16_t)CS16(0x1110, 0x196 + (dh & 0xf) * 2) & s_mask) {
            uint8_t *b = s_buf, *op;
            uint8_t nr, dl;
            uint16_t g, j, cx, di, start, bx, len;

            s_rows++;
            /* decode the RLE row (skips -> 0) */
            nr = *s++;
            do {
                uint16_t nb;
                memset(b, 0, s[0]); b += s[0];
                nb = (uint16_t)((uint8_t)(s[1] << 1) * 2);   /* shl cl,1 is 8-bit */
                memcpy(b, s + 3, nb); b += nb;
                memcpy(b, s + 3 + nb, s[2]); b += s[2];
                s += 3 + nb + s[2];
            } while (--nr);
            memset(b, 0, *s); s++;
            s_src = s;

            /* horizontal decimation in place, 16-pixel groups */
            b = s_buf;
            g = s_w16;
            do {
                const uint8_t *grp = s_buf + (uint16_t)(s_w16 - g) * 16;
                for (j = 0; j < s_ncols[s_level]; j++)
                    *b++ = grp[s_cols[s_level][j]];
            } while (--g);
            memset(b, 0, 0x10);

            /* re-encode (emulating repe/repne scasb exactly) */
            op = s_out + 1;
            dl = 0;
            cx = s_neww;
            di = 0;
            for (;;) {
                int eq = 1;
                start = di;
                while (cx != 0) {           /* repe scasb: skip zeros */
                    cx--;
                    eq = (s_buf[di++] == 0);
                    if (!eq) break;
                }
                bx = (uint16_t)(di - start);
                if (cx == 0) {
                    /* end of row: trailing skip */
                    if (dl == 0) {
                        dl = 1;
                        op[0] = 0; op[1] = 0; op[2] = 0;
                        op += 3;
                    }
                    *op++ = (uint8_t)bx;
                    break;
                }
                bx--; di--; cx++;
                *op++ = (uint8_t)bx;        /* skip */
                start = di;
                while (cx != 0) {           /* repne scasb: find next zero */
                    cx--;
                    eq = (s_buf[di++] == 0);
                    if (eq) break;
                }
                bx = (uint16_t)(di - start);
                bx--; di--; cx++;
                len = bx;
                op[0] = (uint8_t)(len >> 2);
                op[1] = (uint8_t)(len & 3);
                op += 2;
                memcpy(op, s_buf + di - len, len);
                op += len;
                dl++;
                if (cx == 0)                /* never true (cx was just incremented) */
                    break;
            }
            *s_out = dl;                    /* run count of this row */
            s_out = op;
        } else {
            /* row dropped: skip its RLE data */
            uint8_t nr = *s++;
            do {
                uint16_t cx;
                s++;
                cx = PU16(s);
                cx = (uint16_t)((cx & 0x00ff) | ((uint16_t)(uint8_t)((cx >> 8) << 6) << 8));
                cx = (uint16_t)((cx << 2) | (cx >> 14));
                s += cx + 2;
            } while (--nr);
            s++;
            s_src = s;
        }
    } while (--dh);

    PU16(dst + 4) = s_flagw;
    dst[7] = s_rows;
    return s_out;
}
