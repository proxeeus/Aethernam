/* Segment 0dc5: screen scroll / copy / zoom / palette effects, dashed line, time of day. */
#include "../core/mem.h"
#include "../platform/platform.h"
#include "protos.h"

/* 0dc5:0000  scrolls the play-view rows of the draw buffer left by n pixels */
void sub_0dc5_0000(int16_t n)
{
    uint16_t top = DSU16(0x303c);
    uint8_t *dst = DSPTR(0x2d38) + SSU16((uint16_t)(top * 2));
    uint8_t *src = dst + n;
    uint16_t bytes = (uint16_t)(((uint16_t)(320 - n) >> 1) * 2);
    uint16_t rows = (uint16_t)(0xac - top);

    do {
        memmove(dst, src, bytes);
        dst += bytes + n;
        src += bytes + n;
    } while (--rows);
}

/* 0dc5:0040  scrolls the play-view rows of the draw buffer right by n pixels (bottom row first) */
void sub_0dc5_0040(int16_t n)
{
    uint8_t *base = DSPTR(0x2d38);
    int32_t di = 0xd6fe;                    /* last word of row 171 */
    int32_t si = di - n;
    uint16_t bytes = (uint16_t)(((uint16_t)(320 - n) >> 1) * 2);
    uint16_t rows = (uint16_t)(0xac - DSU16(0x303c));

    do {
        /* backward word copy ending at di+1 == memmove of the whole span */
        memmove(base + di + 2 - bytes, base + si + 2 - bytes, bytes);
        di -= bytes + n;
        si -= bytes + n;
    } while (--rows);
}

/* 0dc5:007f  after a left scroll: copies an n-px strip of the background buffer (column srcx) to the right edge of the view */
void sub_0dc5_007f(int16_t n, int16_t srcx)
{
    uint16_t top = DSU16(0x303c);
    uint16_t row = SSU16((uint16_t)(top * 2));
    uint8_t *src = DSPTR(0x3056) + row + srcx;
    uint8_t *dst = DSPTR(0x2d38) + row + 320 - n;
    uint16_t bytes = (uint16_t)(((uint16_t)n >> 1) * 2);
    int16_t skip = (int16_t)(320 - n);
    uint16_t rows = (uint16_t)(0xac - top);

    do {
        memmove(dst, src, bytes);
        src += bytes + skip;
        dst += bytes + skip;
    } while (--rows);
}

/* 0dc5:00ca  after a right scroll: copies an n-px strip of the background buffer (column 320-srcx-n) to the left edge of the view */
void sub_0dc5_00ca(int16_t n, int16_t srcx)
{
    uint16_t top = DSU16(0x303c);
    uint16_t row = SSU16((uint16_t)(top * 2));
    uint8_t *dst = DSPTR(0x2d38) + row;
    uint8_t *src = DSPTR(0x3056) + row + 320 - (int16_t)(srcx + n);
    uint16_t bytes = (uint16_t)(((uint16_t)n >> 1) * 2);
    int16_t skip = (int16_t)(320 - n);
    uint16_t rows = (uint16_t)(0xac - top);

    do {
        memmove(dst, src, bytes);
        src += bytes + skip;
        dst += bytes + skip;
    } while (--rows);
}

/* 0dc5:0117  restores the play-view rows [0x303c]..[0x3054) of the draw buffer from the background buffer */
void sub_0dc5_0117(void)
{
    uint16_t top = DSU16(0x303c);
    uint16_t off = SSU16((uint16_t)(top * 2));
    uint16_t len = (uint16_t)((SSU16((uint16_t)((DSU16(0x3054) - top) * 2)) >> 1) * 2);

    memmove(DSPTR(0x2d38) + off, DSPTR(0x3056) + off, len);
}

/* 0dc5:0148  bumps the frame counter and copies the play-view rows of the draw buffer to the screen */
void sub_0dc5_0148(void)
{
    uint16_t top, off, len;

    DSU16(0x2d66)++;
    top = DSU16(0x303c);
    off = SSU16((uint16_t)(top * 2));
    len = (uint16_t)((SSU16((uint16_t)((DSU16(0x3054) - top) * 2)) >> 1) * 2);
    memmove(DSPTR(0x2d3c) + off, DSPTR(0x2d38) + off, len);
    plat_present();
    plat_yield();
}

/* 0dc5:017d  7-pass interleaved dissolve of draw-buffer rows 0..171 onto the screen */
void sub_0dc5_017d(void)
{
    /* original uses only the segments of 0x2d38/0x2d3c (offset 0) */
    uint8_t *src = DSPTR(0x2d38), *dst = DSPTR(0x2d3c);
    uint16_t pass, k, i;

    DSU16(0x17e2) = 0;
    for (pass = 0; pass < 7; pass++) {
        sub_118f_00de();
        sub_118f_00de();
        i = pass;
        k = 0x1eb6;
        do {
            dst[i] = src[i];
            i += 7;
        } while (--k);
    }
    plat_present();
    plat_yield();
}

/* 0dc5:01c5  7-pass interleaved dissolve of draw-buffer rows 0..198 onto the screen */
void sub_0dc5_01c5(void)
{
    uint8_t *src = DSPTR(0x2d38), *dst = DSPTR(0x2d3c);
    uint16_t pass, k, i;

    DSU16(0x17e2) = 0;
    for (pass = 0; pass < 7; pass++) {
        sub_118f_00de();
        sub_118f_00de();
        i = pass;
        k = 0x2389;
        do {
            dst[i] = src[i];
            i += 7;
        } while (--k);
    }
    plat_present();
    plat_yield();
}

/* 0dc5:020d  copies a full 64000-byte image onto dst, colour 0 transparent */
void sub_0dc5_020d(uint8_t *src, uint8_t *dst)
{
    uint16_t i;
    for (i = 0; i < 0xfa00; i++)
        if (src[i] != 0)
            dst[i] = src[i];
}

/* 0dc5:022e  sky gradient in the background buffer: colour 15 pixels become 0xFF..0xE0 per 2-row band, bottom-up */
void sub_0dc5_022e(void)
{
    uint8_t *p = DSPTR(0x3056);
    int32_t si = 0xd6ff;                    /* last byte of row 171 */
    uint16_t bands = 0x56, cx;
    uint8_t dl = 0xff;
    int started = 0;

    do {
        cx = 0x280;
        do {
            uint8_t al = p[si--];
            if (al == 0x0f) {
                started = 1;
                p[si + 1] = dl;
            }
        } while (--cx);
        if (started && dl != 0xe0)
            dl--;
    } while (--bands);
}

/* 0dc5:026b  4x zoom of an 80x50 draw-buffer window around (x,y) onto the whole screen */
void sub_0dc5_026b(int16_t x, int16_t y)
{
    uint8_t *dst = DSPTR(0x2d3c);
    uint8_t *src = DSPTR(0x2d38);
    int16_t ax = (int16_t)(x - 0x28), bx = (int16_t)(y - 0x2c);
    uint8_t r, c, v;

    if (ax < 0) ax = 0;
    if (ax > 0xef) ax = 0xef;
    if (bx < 0) bx = 0;
    if (bx > 0x7a) bx = 0x7a;
    src += (uint16_t)(SSU16((uint16_t)(bx * 2)) + ax);
    for (r = 0; r < 0x32; r++) {
        for (c = 0; c < 0x50; c++) {
            v = *src++;
            memset(dst, v, 4);
            memset(dst + 320, v, 4);
            memset(dst + 640, v, 4);
            memset(dst + 960, v, 4);
            dst += 4;
        }
        dst += 0x3c0;
        src += 0xf0;
    }
    plat_present();
    plat_yield();
}

/* 0dc5:02dd  3x zoom of a 106x66 draw-buffer window around (x,y) onto the screen (last 2 rows cleared) */
void sub_0dc5_02dd(int16_t x, int16_t y)
{
    uint8_t *dst = DSPTR(0x2d3c);
    uint8_t *src = DSPTR(0x2d38);
    int16_t ax = (int16_t)(x - 0x35), bx = (int16_t)(y - 0x32);
    uint16_t r, rep, c;
    uint8_t v = 0;

    if (ax < 0) ax = 0;
    if (ax > 0xd5) ax = 0xd5;
    if (bx < 0) bx = 0;
    if (bx > 0x6a) bx = 0x6a;
    src += (uint16_t)(SSU16((uint16_t)(bx * 2)) + ax);
    for (r = 0; r < 0x42; r++) {
        for (rep = 0; rep < 3; rep++) {
            for (c = 0; c < 0x6a; c++) {
                v = src[c];
                dst[0] = v; dst[1] = v; dst[2] = v;
                dst += 3;
            }
            dst[0] = v; dst[1] = v;          /* stosw of the last pixel pads the row to 320 */
            dst += 2;
        }
        src += 320;
    }
    memset(dst, 0, 0x280);
    plat_present();
    plat_yield();
}

/* 0dc5:0355  2x zoom of a 160x100 draw-buffer window around (x,y) onto the screen */
void sub_0dc5_0355(int16_t x, int16_t y)
{
    uint8_t *dst = DSPTR(0x2d3c);
    uint8_t *src = DSPTR(0x2d38);
    int16_t ax = (int16_t)(x - 0x50), bx = (int16_t)(y - 0x50);
    uint16_t r, rep, c;

    if (ax < 0) ax = 0;
    if (ax > 0x9f) ax = 0x9f;
    if (bx < 0) bx = 0;
    if (bx > 0x48) bx = 0x48;
    src += (uint16_t)(SSU16((uint16_t)(bx * 2)) + ax);
    for (r = 0; r < 0x64; r++) {
        for (rep = 0; rep < 2; rep++) {
            for (c = 0; c < 0xa0; c++) {
                dst[0] = src[c];
                dst[1] = src[c];
                dst += 2;
            }
        }
        src += 320;
    }
    plat_present();
    plat_yield();
}

/* 0dc5:03b5  (dead code) scales 672 palette bytes: dst = (src*level) >> 4 */
static __attribute__((unused)) void sub_0dc5_03b5(uint8_t *src, uint8_t *dst, uint16_t level)
{
    uint16_t cx = 0x2a0;
    do {
        *dst++ = (uint8_t)((uint16_t)(*src++ * level) >> 4);
    } while (--cx);
}

/* 0dc5:03de  writes a 32-step colour gradient (r0,g0,b0)->(r1,g1,b1) into palette entries 224..255 */
void sub_0dc5_03de(uint8_t *pal, int16_t r0, int16_t g0, int16_t b0, int16_t r1, int16_t g1, int16_t b1)
{
    uint8_t *d = pal + 0x2a0;
    int16_t i;

    r1 = (int16_t)(r1 - r0);
    g1 = (int16_t)(g1 - g0);
    b1 = (int16_t)(b1 - b0);
    for (i = 0; i < 0x20; i++) {
        *d++ = (uint8_t)(((uint16_t)(r1 * i) >> 5) + r0);
        *d++ = (uint8_t)(((uint16_t)(g1 * i) >> 5) + g0);
        *d++ = (uint8_t)(((uint16_t)(b1 * i) >> 5) + b0);
    }
}

/* 0dc5:0448  palette tint: red pulled toward (R+G+B+100)/4 by level/16, green and blue scaled by (16-level)/16 */
void sub_0dc5_0448(uint8_t *src, uint8_t *dst, int16_t level)
{
    uint16_t cx = 0x100;
    uint16_t ax;
    int16_t bx;

    do {
        uint16_t r = src[0], g = src[1], b = src[2];
        ax = (uint16_t)((uint16_t)(b + r + g + 0x64) >> 2);
        ax = (uint16_t)(ax - r);
        ax = (uint16_t)((int16_t)ax * level);
        ax = (uint16_t)((int16_t)ax >> 4);
        ax = (uint16_t)(ax + r);
        *dst++ = (uint8_t)ax;
        bx = (int16_t)(0x10 - level);
        ax = (uint16_t)((ax & 0xff00) | src[1]);   /* ah carries over from the previous result */
        ax = (uint16_t)((int16_t)ax * bx);
        ax = (uint16_t)((int16_t)ax >> 4);
        *dst++ = (uint8_t)ax;
        ax = (uint16_t)((ax & 0xff00) | src[2]);
        ax = (uint16_t)((int16_t)ax * bx);
        ax = (uint16_t)((int16_t)ax >> 4);
        *dst++ = (uint8_t)ax;
        src += 3;
    } while (--cx);
}

/* 0dc5:04b1  builds a byte table by piecewise-linear interpolation of (x,value) break points (u16 n, n pairs) */
void sub_0dc5_04b1(uint8_t *pts, uint8_t *out)
{
    uint16_t cnt = (uint16_t)(PU16(pts) - 1);
    uint8_t *s = pts + 2;
    uint8_t al = 0;

    do {
        int16_t x0 = P16(s), v0 = P16(s + 2), x1 = P16(s + 4), v1 = P16(s + 6);
        int16_t len = (int16_t)(x1 - x0);
        s += 4;
        if (len != 0) {
            int16_t dv = (int16_t)(v1 - v0);
            int8_t step = 1;
            uint16_t k;
            int16_t acc;
            al = (uint8_t)v0;
            if (dv < 0) { dv = (int16_t)-dv; step = -1; }
            if (len > dv) {
                acc = (int16_t)((uint16_t)len >> 1);
                k = (uint16_t)len;
                do {
                    *out++ = al;
                    acc = (int16_t)(acc + dv);
                    if (acc >= len) {
                        acc = (int16_t)(acc - len);
                        al = (uint8_t)(al + step);
                    }
                } while (--k);
            } else {
                acc = (int16_t)((uint16_t)dv >> 1);
                k = (uint16_t)dv;           /* TODO(port): dv==0 here (len<0) loops 65536 times in the original */
                do {
                    al = (uint8_t)(al + step);
                    acc = (int16_t)(acc + len);
                    if (acc >= dv) {
                        acc = (int16_t)(acc - dv);
                        *out++ = al;
                    }
                } while (--k);
            }
        }
    } while (--cnt);
    *out = al;
}

/* 0dc5:051b  half-size transparent blit of the play view: src(2x,2y) -> dst(x,y), 160 px wide */
void sub_0dc5_051b(uint8_t *src, uint8_t *dst)
{
    uint16_t rows = (uint16_t)((uint16_t)(0xac - DSU16(0x303c)) >> 1);
    uint16_t c;

    do {
        for (c = 0; c < 0xa0; c++) {
            if (*src != 0)
                *dst = *src;
            dst++;
            src += 2;
        }
        src += 0x140;
        dst += 0xa0;
    } while (--rows);
}

/* 0dc5:0551  quarter-size transparent blit of the play view: src(4x,4y) -> dst(x,y), 80 px wide */
void sub_0dc5_0551(uint8_t *src, uint8_t *dst)
{
    uint16_t rows = (uint16_t)((uint16_t)(0xac - DSU16(0x303c)) >> 2);
    uint16_t c;

    do {
        for (c = 0; c < 0x50; c++) {
            if (*src != 0)
                *dst = *src;
            dst++;
            src += 4;
        }
        src += 0x3c0;
        dst += 0xf0;
    } while (--rows);
}

/* Cohen-Sutherland outcode against the SS clip rect (1 left, 2 right, 4 above, 8 below) */
#define OUTCODE(x, y) ((uint16_t)(((x) < SS16(0x196) ? 1 : 0) | ((x) > SS16(0x198) ? 2 : 0) | \
                                  ((y) < SS16(0x192) ? 4 : 0) | ((y) > SS16(0x194) ? 8 : 0)))

/* 0dc5:058d  clipped dashed line (2 on / 2 off) into the draw buffer */
void sub_0dc5_058d(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color)
{
    int16_t ax = x0, bx = y0, cx = x1, dx = y1, t, e;
    uint16_t si, di, bp;

    if (ax > cx) { t = ax; ax = cx; cx = t; t = bx; bx = dx; dx = t; }
    si = OUTCODE(ax, bx);
    di = OUTCODE(cx, dx);
    while ((si | di) != 0) {
        int16_t ddx, ddy;
        if (si & di)
            return;
        bp = si ? si : di;
        DSU16(0x2d06) = 8;
        e = SS16(0x194);
        if (!(DSU16(0x2d06) & bp)) {
            e = SS16(0x192);
            DSU16(0x2d06) >>= 1;
            if (!(DSU16(0x2d06) & bp)) {
                e = SS16(0x198);
                DSU16(0x2d06) >>= 1;
                if (!(DSU16(0x2d06) & bp)) {
                    e = SS16(0x196);
                    DSU16(0x2d06) >>= 1;
                }
            }
        }
        ddx = (int16_t)(cx - ax);
        ddy = (int16_t)(dx - bx);
        if (DSU16(0x2d06) & 0xc) {
            DS16(0x2d0c) = e;
            DS16(0x2d0a) = (int16_t)((int16_t)((int32_t)(int16_t)(e - bx) * ddx / ddy) + ax);
        } else {
            DS16(0x2d0a) = e;
            DS16(0x2d0c) = (int16_t)((int16_t)((int32_t)(int16_t)(e - ax) * ddy / ddx) + bx);
        }
        DSU16(0x2d08) = OUTCODE(DS16(0x2d0a), DS16(0x2d0c));
        if (si == bp) {
            ax = DS16(0x2d0a); bx = DS16(0x2d0c); si = DSU16(0x2d08);
        } else {
            cx = DS16(0x2d0a); dx = DS16(0x2d0c); di = DSU16(0x2d08);
        }
    }

    {
        int16_t ddx = (int16_t)(cx - ax), ddy = (int16_t)(dx - bx), step = 0x140, acc;
        uint16_t n;
        uint8_t bh = 2;
        uint8_t *p;

        if (ddy == 0 && ddx == 0)
            return;
        p = DSPTR(0x2d38) + (uint16_t)(SSU16((uint16_t)(bx * 2)) + ax);
        *p = color;
        if (ddy < 0) { ddy = (int16_t)-ddy; step = -0x140; }
        if (ddx > ddy) {
            acc = (int16_t)((uint16_t)ddx >> 1);
            n = (uint16_t)ddx;
            do {
                p++;
                acc = (int16_t)(acc + ddy);
                if (acc >= ddx) { acc = (int16_t)(acc - ddx); p += step; }
                bh++;
                if (bh & 2) *p = color;
            } while (--n);
        } else {
            acc = (int16_t)((uint16_t)ddy >> 1);
            n = (uint16_t)ddy;
            do {
                p += step;
                acc = (int16_t)(acc + ddx);
                if (acc >= ddy) { acc = (int16_t)(acc - ddy); p++; }
                bh++;
                if (bh & 2) *p = color;
            } while (--n);
        }
    }
}

/* 0dc5:0780  reads the time of day into DS8 0x3040 (hour), 0x305e (minute), 0x3048 (second) */
void sub_0dc5_0780(void)
{
    uint8_t h, m, s;
    plat_get_time(&h, &m, &s);
    DS8(0x3040) = h;
    DS8(0x305e) = m;
    DS8(0x3048) = s;
}
