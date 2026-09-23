/*
 * Intro segment 0488: clip rectangle setter and the RLE sprite blitter.
 *
 * Sprite bank: dword offsets per sprite (index & 0xfff).  Sprite: u16 flags at -2 (bit 15 =
 * the data is currently mirrored), u8 width/16, u8 height, then `height` rows:
 *   u8 nruns, nruns x { u8 skip, u8 n4, u8 n1, data[4*n4 + n1] }, u8 trailing skip.
 * Draw index bits: 0x8000 = mirrored (the rows are reversed in place when the stored
 * orientation differs), 0x4000 = opaque (skips drawn with colour 0).  (x, y) = bottom left.
 *
 * Code-segment variables: cs:[8..] row work buffer, 0x198 index, 0x19a x, 0x19c top y,
 * 0x19e dest segment, 0x1a0 visible width, 0x1a2 left clip, 0x1a4 dest save, 0x1a6 source
 * save, 0x1a8 "clipped horizontally" flag.
 */
#include "../core/mem.h"
#include "../platform/platform.h"
#include "intro_protos.h"
#include <string.h>

static uint8_t s_work[0x10000];                 /* cs:0000.. (indices wrap at 64K like cs:di) */

/* es:di stores: di is a 16-bit offset inside the destination segment */
static inline void put(uint8_t *dest, uint16_t *di, const uint8_t *src, uint16_t n)
{
    while (n--) dest[(*di)++] = *src++;
}
static inline void fill0(uint8_t *dest, uint16_t *di, uint16_t n)
{
    while (n--) dest[(*di)++] = 0;
}

/* 0488:01bd  sets the vertical clip limits SS:0x192 (ymin) and SS:0x194 (ymax) */
void intro_0488_01bd(int16_t ymin, int16_t ymax)
{
    ISS16(0x192) = ymin;
    ISS16(0x194) = ymax;
}

/* mirrors the rows of a sprite in place (04b05..04b71) */
static void mirror_rows(uint8_t *p, uint8_t rows, uint16_t bxv)
{
    uint16_t di = 8 + 0x18e, bp, cx, wb;
    uint8_t dh;
    do {                                        /* dec dl / jne */
        dh = *p++;
        bp = 1;
        s_work[di++] = *p++;                    /* first skip */
        do {                                    /* dec dh / jne */
            di -= 2;
            wb = PU16(p);
            p += 2;
            if (wb != 0) {
                bxv = wb;
                cx = (uint16_t)((wb & 0xff) * 4 + (wb >> 8));
                bp += cx + 3;
                do {
                    s_work[di] = *p++;
                    di -= 1;                    /* movsb; sub di,2 */
                } while (--cx);
                di -= 2;
            }
            s_work[(uint16_t)(di + 1)] = (uint8_t)bxv;
            s_work[(uint16_t)(di + 2)] = (uint8_t)(bxv >> 8);
            s_work[di++] = *p++;                /* next skip */
        } while (--dh);
        p -= bp;                                /* back to the first skip of the row */
        di--;
        for (cx = 0; cx < bp; cx++)             /* rep movsb from the work buffer */
            p[cx] = s_work[(uint16_t)(di + cx)];
        p += bp;
        di = (uint16_t)(di + bp - 1);
    } while (--rows);
}

/* 0488:01d0  draws sprite idx of `bank` with its bottom-left corner at (x, y) into `dest`
 * (320 wide), clipped to the SS clip rectangle; see the format above */
void intro_0488_01d0(uint16_t idx, int16_t x, int16_t y, uint8_t *dest, uint8_t *bank)
{
    uint8_t *spr = bank + PU32(bank + (uint16_t)((idx & 0xfff) << 2));
    uint8_t *p = spr + 2;
    int16_t bx, bp, dx, ytop, ax, cx, xx = x;
    uint16_t di, visw, lskip;
    uint8_t dl, dh, al;
    int clipped = 0;

    if (!arena_owns(spr)) return;   /* TODO(port): damaged bank (the original drew garbage) */
    bx = (int16_t)(spr[0] << 4);                /* width */
    bp = (int16_t)(0x140 - bx);
    dx = spr[1];                                /* height */
    ytop = (int16_t)(y - dx + 1);
    di = (uint16_t)(ytop * 320);                /* 16-bit offset, like the original */
    if ((idx ^ PU16(spr - 2)) & 0x8000) {
        PU16(spr - 2) ^= 0x8000;
        mirror_rows(p, (uint8_t)dx, (uint16_t)bx);
    }

    ax = (int16_t)(ytop + dx - ISS16(0x194) - 1);
    if (ax > 0) {
        dx = (int16_t)(dx - ax);
        if (dx <= 0) return;
    }
    ax = (int16_t)(ytop - ISS16(0x192));
    if (ax < 0) {
        dx = (int16_t)(dx + ax);
        if (dx <= 0) return;
        al = (uint8_t)-ax;
        do {                                    /* skip the rows above the clip */
            dh = *p++;
            do {
                uint16_t n = p[2];
                n += (uint16_t)(p[1] << 2);
                p += n + 3;
            } while (--dh);
            p++;
            di += 0x140;
        } while (--al);
    }

    cx = 0;
    ax = (int16_t)(ISS16(0x196) - xx);
    if (ax > 0) {
        clipped = 1;
        bx = (int16_t)(bx - ax);
        xx = (int16_t)(xx + ax);
        bp = (int16_t)(bp + ax);
        cx = ax;
    }
    ax = (int16_t)(xx + bx - ISS16(0x198) - 1);
    if (ax > 0) {
        clipped = 1;
        bx = (int16_t)(bx - ax);
        bp = (int16_t)(bp + ax);
    }
    if (bx <= 0) return;
    di = (uint16_t)(di + xx);
    dl = (uint8_t)dx;
    visw = (uint16_t)bx;
    lskip = (uint16_t)cx;

    if (!(idx & 0x4000)) {
        if (!clipped) {                         /* transparent skips, no clipping */
            do {
                dh = *p++;
                do {
                    uint16_t n;
                    di += p[0];
                    n = (uint16_t)(p[1] * 4 + p[2]);
                    p += 3;
                    put(dest, &di, p, n);
                    p += n;
                } while (--dh);
                di += *p++;
                di += bp;
            } while (--dl);
        } else {                                /* unpack the row, copy non-zero pixels */
            do {
                uint16_t t = 8, k;
                dh = *p++;
                do {
                    uint16_t n;
                    fill0(s_work, &t, p[0]);
                    n = (uint16_t)(p[1] * 4 + p[2]);
                    p += 3;
                    put(s_work, &t, p, n);
                    p += n;
                } while (--dh);
                fill0(s_work, &t, *p);
                p++;
                for (k = 0; k < visw; k++) {
                    uint8_t c = s_work[(uint16_t)(8 + lskip + k)];
                    if (c) dest[di] = c;
                    di++;                       /* uint16_t: wraps like di */
                }
                di += bp;
            } while (--dl);
        }
    } else {
        if (!clipped) {                         /* opaque: skips drawn as colour 0 */
            do {
                dh = *p++;
                do {
                    uint16_t n;
                    fill0(dest, &di, p[0]);
                    n = (uint16_t)(p[1] * 4 + p[2]);
                    p += 3;
                    put(dest, &di, p, n);
                    p += n;
                } while (--dh);
                fill0(dest, &di, *p++);
                di += bp;
            } while (--dl);
        } else {
            do {
                uint16_t t = 8;
                dh = *p++;
                do {
                    uint16_t n;
                    fill0(s_work, &t, p[0]);
                    n = (uint16_t)(p[1] * 4 + p[2]);
                    p += 3;
                    put(s_work, &t, p, n);
                    p += n;
                } while (--dh);
                fill0(s_work, &t, *p);
                p++;
                {
                    uint16_t k;
                    for (k = 0; k < visw; k++) dest[di++] = s_work[(uint16_t)(8 + lskip + k)];
                }
                di += bp;
            } while (--dl);
        }
    }
}
