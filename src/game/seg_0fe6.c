/* Segment 0fe6: clip-rectangle setters and the RLE sprite blitter. */
#include "../core/mem.h"
#include "../platform/platform.h"
#include "protos.h"

/* Code-segment data of 0fe6 (cs:0x000a scratch row, cs:0x019a.. blitter state). */
static uint8_t  s_cs[0x1100];  /* indexed by the original cs offset; 0x0a..0x199 = 400-byte scratch (enlarged for safety) */
static uint16_t s_id;          /* cs:0x19a */
static int16_t  s_x;           /* cs:0x19c */
static int16_t  s_y;           /* cs:0x19e top row */
static int16_t  s_vis;         /* cs:0x1a2 visible width */
static int16_t  s_left;        /* cs:0x1a4 pixels clipped on the left */
static uint16_t s_clip;        /* cs:0x1aa 1 = horizontally clipped */

/* 0fe6:01ac  sets the horizontal clip limits (SS 0x196 xmin, 0x198 xmax) */
void sub_0fe6_01ac(int16_t xmin, int16_t xmax)
{
    SS16(0x196) = xmin;
    SS16(0x198) = xmax;
}

/* 0fe6:01bf  sets the vertical clip limits (SS 0x192 ymin, 0x194 ymax) */
void sub_0fe6_01bf(int16_t ymin, int16_t ymax)
{
    SS16(0x192) = ymin;
    SS16(0x194) = ymax;
}

/* 0fe6:01d2  draws RLE sprite id&0xfff of bank at left x / bottom row y into dst, clipped; 0x8000 mirror, 0x4000 opaque */
void sub_0fe6_01d2(uint16_t id, int16_t x, int16_t y, uint8_t *dst, uint8_t *bank)
{
    uint8_t *spr, *s, *d;
    int16_t w, stride, h, ax, bx;
    int32_t off;
    uint8_t rows;

    s_x = x;
    s_y = y;
    s_id = id;
    spr = bank + PU32(bank + (uint16_t)((id & 0xfff) * 4));
    w = (int16_t)(spr[0] << 4);
    stride = (int16_t)(0x140 - w);          /* bp */
    h = spr[1];
    s = spr + 2;
    s_y = (int16_t)(s_y - h + 1);
    off = (int32_t)s_y * 320;
    bx = w;
    s_clip = 0;

    if ((id ^ PU16(spr - 2)) & 0x8000) {
        /* mirror the sprite rows in place, using the scratch area backwards from cs:0x198 */
        int di = 0x198;
        uint16_t lastw = (uint16_t)w;       /* bx register: stale value used for (0,0) runs */
        uint8_t *r = s;
        uint8_t dl = (uint8_t)h;

        PU16(spr - 2) ^= 0x8000;
        do {
            uint8_t dh = *r++;
            uint16_t len = 1;
            s_cs[di] = *r++; di++;          /* first skip becomes the trailing skip */
            do {
                uint16_t ax2;
                di -= 2;
                ax2 = PU16(r); r += 2;
                if (ax2 != 0) {
                    uint16_t cnt;
                    lastw = ax2;
                    cnt = (uint16_t)((ax2 & 0xff) * 4 + (ax2 >> 8));
                    len = (uint16_t)(len + cnt + 3);
                    do {
                        s_cs[di] = *r++;
                        di -= 1;
                    } while (--cnt);
                    di -= 2;
                }
                s_cs[di + 1] = (uint8_t)lastw;
                s_cs[di + 2] = (uint8_t)(lastw >> 8);
                s_cs[di] = *r++; di++;      /* next skip / trailing skip */
            } while (--dh);
            di--;
            memcpy(r - len, s_cs + di, len);
            di = di + len - 1;
        } while (--dl);
    }

    /* vertical clipping */
    ax = (int16_t)(s_y + h - SS16(0x194) - 1);
    if (ax > 0) {
        h = (int16_t)(h - ax);
        if (h <= 0) return;
    }
    ax = (int16_t)(s_y - SS16(0x192));
    if (ax < 0) {
        uint8_t k;
        h = (int16_t)(h + ax);
        if (h <= 0) return;
        k = (uint8_t)-ax;                   /* 8-bit row counter in the original */
        do {
            uint8_t dh = *s++;
            do {
                s += s[2] + s[1] * 4 + 3;
            } while (--dh);
            s++;
            off += 0x140;
        } while (--k);
    }

    /* horizontal clipping */
    s_left = 0;
    ax = (int16_t)(SS16(0x196) - s_x);
    if (ax > 0) {
        s_clip = 1;
        bx = (int16_t)(bx - ax);
        s_x = (int16_t)(s_x + ax);
        stride = (int16_t)(stride + ax);
        s_left = ax;
    }
    ax = (int16_t)(s_x + bx - SS16(0x198) - 1);
    if (ax > 0) {
        s_clip = 1;
        bx = (int16_t)(bx - ax);
        stride = (int16_t)(stride + ax);
    }
    if (bx <= 0)
        return;
    s_vis = bx;
    d = dst + off + s_x;
    rows = (uint8_t)h;

    if (!(s_id & 0x4000)) {
        if (!(s_clip & 1)) {
            /* transparent, unclipped: skips advance, run pixels copied verbatim */
            do {
                uint8_t dh = *s++;
                do {
                    uint16_t nb;
                    d += s[0];
                    nb = (uint16_t)(s[1] * 4 + s[2]);
                    s += 3;
                    memcpy(d, s, nb);
                    d += nb; s += nb;
                } while (--dh);
                d += *s++;
                d += stride;
            } while (--rows);
        } else {
            /* transparent, clipped: decode the row into scratch, copy the visible part with 0 transparent */
            do {
                uint8_t *b = s_cs + 0x0a;
                uint8_t *q;
                uint16_t k;
                uint8_t dh = *s++;
                do {
                    uint16_t nb;
                    memset(b, 0, s[0]); b += s[0];
                    nb = (uint16_t)(s[1] * 4 + s[2]);
                    s += 3;
                    memcpy(b, s, nb);
                    b += nb; s += nb;
                } while (--dh);
                memset(b, 0, *s); s++;
                q = s_cs + 0x0a + s_left;
                k = (uint16_t)s_vis;
                do {
                    if (*q != 0) *d = *q;
                    q++; d++;
                } while (--k);
                d += stride;
            } while (--rows);
        }
    } else {
        if (s_clip != 1) {
            /* opaque, unclipped: skips are written as colour 0 */
            do {
                uint8_t dh = *s++;
                do {
                    uint16_t nb;
                    memset(d, 0, s[0]); d += s[0];
                    nb = (uint16_t)(s[1] * 4 + s[2]);
                    s += 3;
                    memcpy(d, s, nb);
                    d += nb; s += nb;
                } while (--dh);
                memset(d, 0, *s); d += *s; s++;
                d += stride;
            } while (--rows);
        } else {
            /* opaque, clipped: decode into scratch, copy the visible part verbatim */
            do {
                uint8_t *b = s_cs + 0x0a;
                uint8_t dh = *s++;
                do {
                    uint16_t nb;
                    memset(b, 0, s[0]); b += s[0];
                    nb = (uint16_t)(s[1] * 4 + s[2]);
                    s += 3;
                    memcpy(b, s, nb);
                    b += nb; s += nb;
                } while (--dh);
                memset(b, 0, *s); s++;
                memcpy(d, s_cs + 0x0a + s_left, (uint16_t)s_vis);
                d += s_vis;
                d += stride;
            } while (--rows);
        }
    }
}
