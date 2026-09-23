/*
 * Segment 0c9b - 3D outdoor engine (hand-written assembly, self-modifying).
 *
 * The asm addresses DGROUP through CS: cs:X (X >= 0xe30) == DS:(X - 0x57e0).
 * World: 0x4000 x 0x4000, 32x32 cells of 0x200 units; map DSPTR(0x34a2) = 2 bytes per cell
 * (type, height), row stride 0x40.  DSPTR(0x34aa) = 4 terrain tiles x 0x400 bytes:
 *   +0x000 vertices, +0x320 fptr object def, +0x324 count, +0x328/+0x32a origin x,y,
 *   +0x32c slope-class word, +0x334 three 65-byte height lines (top, bottom @0x41, diagonal @0x82).
 * Vertex (8 bytes): s16 x, s16 y (depth after rotation), s16 z, u16 attr.
 * Vertex buffers: A = DSPTR(0x34a6), B = DSPTR(0x34ae), count DS16(0x2284) (asm cs:0x7a64).
 * Object list: DSPTR(0x349e), count DS16(0x3496).  Heading DS8(0x1fcd), sin/cos DS16(0x1fbc/0x1fbe)
 * (Q14 table DS 0x2d8c).  Player x,y DS16(0x1fc2/0x1fc4), camera height DS16(0x1fc6).
 */
#include "../core/mem.h"
#include "../platform/platform.h"
#include "protos.h"

/* code-segment variables of the original */
static int16_t  s_cs_000e;     /* cs:0x0e  - temp of sub_0c9b_04f4 */
static uint16_t s_cs_000c;     /* cs:0x0c  - "current vertex inside" flag of sub_0c9b_077c (write only) */
static uint16_t s_cs_023c;     /* cs:0x23c - self-modified immediate in sub_0c9b_01cd */
static uint16_t s_cs_03df;     /* cs:0x3df - self-modified immediate in sub_0c9b_0323 */
static uint8_t  s_cs_053f;     /* cs:0x53f - patched jcc opcode in sub_0c9b_04f4: 0 jge, 1 jl, 2 never */

/* 16-bit idiv (quotient truncated to 16 bits) */
static int16_t idiv16(int32_t num, int16_t den)
{
    if (den == 0)
        return 0;              /* port: DOS would raise a divide error */
    return (int16_t)((int64_t)num / den);
}

/* 0c9b:0022 - palette fade: dst[i] = src[i]*level >> 4 for 0x240 or 0x2a0 bytes */
void sub_0c9b_0022(uint8_t *src_pal, uint8_t *dst_pal, uint16_t level)
{
    uint16_t cx = 0x240, i;

    if (DS16(0xa0) != -1 && DS16(0xa0) != 0x63)
        cx = 0x2a0;
    for (i = 0; i < cx; i++)
        dst_pal[i] = (uint8_t)((uint16_t)(src_pal[i] * level) >> 4);
}

/* 0c9b:005c - build 4 x 16-step RGB gradients (colours 192..255) between 5 src triples */
void sub_0c9b_005c(uint8_t *src_rgb, uint8_t *dst_pal)
{
    uint8_t *si = src_rgb;
    uint8_t *di = dst_pal + 0x240;
    int16_t k, c, ch;
    uint16_t a, b, prod;

    for (k = 0; k < 4; k++) {
        for (c = 0; c < 16; c++) {
            for (ch = 0; ch < 3; ch++) {
                a = si[ch];
                b = si[ch + 3];
                prod = (uint16_t)((int16_t)(b - a) * c);
                *di++ = (uint8_t)((prod >> 4) + a);
            }
        }
        si += 3;
    }
}

/* 0c9b:00d8 - linear interpolation a + (b-a)*num/den */
int16_t sub_0c9b_00d8(int16_t a, int16_t b, int16_t num, int16_t den)
{
    int16_t d = (int16_t)(b - a);
    return (int16_t)(idiv16((int32_t)d * num, den) + a);
}

/* 0c9b:00fe - build tile height lines + slope class from 4 corner heights of the map */
void sub_0c9b_00fe(uint8_t *hmap, uint8_t *tile)
{
    uint8_t *si = tile + 0x334;
    uint8_t *e = hmap;
    uint16_t ax, dx;
    uint8_t ah, cl, ch, dh, dl, bl;
    int16_t i;

    /* the caller leaves ax = 2*cell index: AL is the initial fraction */
    ax = (uint8_t)(hmap - DSPTR(0x34a2) - 1);

    ah = e[0];
    ax = (uint16_t)((ah << 8) | (ax & 0xff));
    si[0] = ah;
    si[0x82] = ah;
    dh = e[2];
    si[0x40] = dh;
    cl = e[0x40];
    si[0x41] = cl;
    dl = e[0x42];
    si[0x81] = dl;
    si[0xc2] = dl;

    bl = 0;
    if (ah == cl)
        bl = 4 | 2;                     /* flags of "or bl,4": jl not taken */
    else if (!((int8_t)ah < (int8_t)cl))
        bl = 2;
    if (dl == cl)
        bl |= 1;
    cl = CS8(0x0c9b, 0xf6 + bl);
    bl = 0;
    if (dh == dl)
        bl = 4 | 2;
    else if (!((int8_t)dh < (int8_t)dl))
        bl = 2;
    if (dh == ah)
        bl |= 1;
    ch = CS8(0x0c9b, 0xf6 + bl);
    PU16(si - 8) = (uint16_t)(cl | (ch << 8));

    /* top line c00 -> c01 */
    si++;
    dh = (uint8_t)(dh - ah);
    dx = (uint16_t)((int16_t)((dh << 8) | dl) >> 6);
    for (i = 0; i < 0x3f; i++) {
        ax = (uint16_t)(ax + dx);
        *si++ = (uint8_t)(ax >> 8);
    }

    /* bottom line c10 -> c11 */
    ah = e[0x40];
    ax = (uint16_t)((ah << 8) | (ax & 0xff));
    dh = (uint8_t)(e[0x42] - ah);
    dx = (uint16_t)((int16_t)((dh << 8) | (dx & 0xff)) >> 6);
    si += 2;
    for (i = 0; i < 0x3f; i++) {
        ax = (uint16_t)(ax + dx);
        *si++ = (uint8_t)(ax >> 8);
    }

    /* diagonal c00 -> c11 */
    ah = e[0];
    ax = (uint16_t)((ah << 8) | (ax & 0xff));
    dh = (uint8_t)(e[0x42] - ah);
    dx = (uint16_t)((int16_t)((dh << 8) | (dx & 0xff)) >> 6);
    si += 2;
    for (i = 0; i < 0x3f; i++) {
        ax = (uint16_t)(ax + dx);
        *si++ = (uint8_t)(ax >> 8);
    }
}

/* 0c9b:01cd - set z = -(terrain height) for each vertex from the tile height lines */
void sub_0c9b_01cd(uint8_t *verts, uint8_t *edges)
{
    uint16_t cx = DSU16(0x2284);
    uint8_t *v = verts;
    int16_t x, y;
    uint16_t bx, ax;

    if (cx == 0)
        return;
    do {
        x = P16(v);
        y = P16(v + 2);
        bx = (uint16_t)(x >> 3);
        if (x > y || x == 0x200) {
            ax = edges[(uint16_t)(bx + 0x82)];
            bx = (uint16_t)((bx & 0xff00) | edges[bx]);
            ax = (uint16_t)(ax - bx);
            ax = (uint16_t)idiv16((int32_t)(int16_t)ax * y, x);
            ax = (uint16_t)(ax + bx);
        } else {
            ax = edges[(uint16_t)(bx + 0x41)];
            bx = (uint16_t)((bx & 0xff00) | edges[(uint16_t)(bx + 0x82)]);
            s_cs_023c = ax;
            ax = (uint16_t)(ax - bx);
            ax = (uint16_t)idiv16((int32_t)(int16_t)ax * (int16_t)(y - 0x200), (int16_t)(0x200 - x));
            ax = (uint16_t)(ax + s_cs_023c);
        }
        P16(v + 4) = (int16_t)-(int16_t)ax;
        v += 8;
    } while (--cx);
}

/* 0c9b:024e - recompute view flags 0x1fcb; rebuild terrain tiles when they change */
void sub_0c9b_024e(void)
{
    uint8_t h = DS8(0x1fcd);
    uint8_t al, ah;

    al = (uint8_t)(h + 0x10) >> 5;
    if (DSU16(0x1fc2) & 0x100)
        al |= 8;
    if (DSU16(0x1fc4) & 0x100)
        al |= 0x10;
    ah = (uint8_t)(h - 0x20) & 0xc0;
    al |= ah;
    DS8(0x1fcb) = al;
    if (DS8(0x1fcc) != al) {
        DS8(0x1fcc) = al;
        sub_0a86_0517();
    }
}

/* 0c9b:0293 - horizon row DS 0x34b8 from camera height DS 0x1fc6 */
void sub_0c9b_0293(void)
{
    int16_t ax = DS16(0x1fc6);
    int16_t bx = 0x62;
    int cl = 4;

    if (ax > 0x40) {
        cl = 5;
        ax -= 0x40;
        bx = 0x76;
    }
    DS16(0x34b8) = (int16_t)(((int16_t)(ax * 5) >> cl) + bx);
}

/* 0c9b:02b6 - DS 0x34b6 = 1 if all four terrain tiles are flat (slope class 0x303) */
void sub_0c9b_02b6(void)
{
    uint8_t *t = DSPTR(0x34aa);
    uint16_t v = PU16(t + 0x32c) & PU16(t + 0x72c) & PU16(t + 0xb2c) & PU16(t + 0xf2c);

    DS16(0x34b6) = (v == 0x303) ? 1 : 0;
}

/* 0c9b:0323 - terrain height at world x,y in tile #tile (triangle interpolation of corners) */
int16_t sub_0c9b_0323(int16_t x, int16_t y, int16_t tile)
{
    uint8_t *e = DSPTR(0x34aa) + (uint16_t)((uint8_t)tile << 10) + 0x334;
    uint16_t bx, cx, dy, ax;
    int32_t prod;

    if (x <= 0)
        return 0;
    bx = (uint16_t)(x & 0x1ff) >> 1;
    if (y <= 0)
        return 0;
    dy = (uint16_t)(y & 0x1ff) >> 1;
    cx = bx;

    if ((int16_t)bx > (int16_t)dy) {
        /* upper triangle: between top line and diagonal */
        bx = (uint16_t)((cx & 0xff00) | e[0]);
        ax = (uint16_t)(e[0x40] - bx);
        prod = (int32_t)(int16_t)ax * (int16_t)cx;
        ax = (uint16_t)(prod >> 8);
        bx = (uint16_t)(bx + ax);
        ax = (uint16_t)(e[0x81] - e[0]);
        prod = (int32_t)(int16_t)ax * (int16_t)cx;
        ax = (uint16_t)((uint16_t)(prod >> 8) + e[0]);
        ax = (uint16_t)(ax - bx);
        ax = (uint16_t)idiv16((int32_t)(int16_t)ax * (int16_t)dy, (int16_t)cx);
        return (int16_t)(ax + bx);
    }

    /* lower triangle: between diagonal and bottom line */
    bx = (uint16_t)((cx & 0xff00) | e[0]);
    ax = (uint16_t)(e[0x81] - bx);
    prod = (int32_t)(int16_t)ax * (int16_t)cx;
    ax = (uint16_t)(prod >> 8);
    bx = (uint16_t)(bx + ax);
    ax = (uint16_t)(e[0x81] - e[0x41]);
    prod = (int32_t)(int16_t)ax * (int16_t)cx;
    ax = (uint16_t)((uint16_t)(prod >> 8) + e[0x41]);
    s_cs_03df = ax;
    ax = (uint16_t)(ax - bx);
    ax = (uint16_t)idiv16((int32_t)(int16_t)ax * (int16_t)(dy - 0x100), (int16_t)(0x100 - cx));
    return (int16_t)(ax + s_cs_03df);
}

/* 0c9b:03e5 - camera height = terrain height under the player + eye offset */
void sub_0c9b_03e5(void)
{
    DS16(0x1fc6) = (int16_t)(sub_0c9b_0323(DS16(0x1fc2), DS16(0x1fc4), 0) + DS16(0x1fc8));
}

/* 0c9b:0404 - read direction input: turn heading, set move direction, then move */
void sub_0c9b_0404(void)
{
    uint8_t cl = 0, dl = 0;
    uint8_t al = DS8(0x2d4a);

    if (al & 4)
        dl = (uint8_t)-DS8(0x1fce);
    if (al & 8)
        dl = DS8(0x1fce);
    if (al & 1)
        cl = 1;
    if (al & 2) {
        cl = 0xff;
        dl = (uint8_t)-dl;
    }
    DS8(0x1fca) = cl;
    if (dl != 0) {
        dl = (uint8_t)(dl + DS8(0x1fcd));
        DS8(0x1fcd) = dl;
        DS16(0x1fbc) = DS16(0x2d8c + dl * 2);
        DS16(0x1fbe) = DS16(0x2d8c + (uint8_t)(dl + 0x40) * 2);
    }
    sub_0c9b_0464();
}

/* 0c9b:0464 - move the player along the heading, then view flags, camera height, horizon */
void sub_0c9b_0464(void)
{
    int8_t d = DSS8(0x1fca);
    uint8_t cl;
    int16_t ax, bx;

    if (d != 0) {
        cl = DS8(0x1fd0) & 31;
        ax = (int16_t)(DS16(0x1fbe) >> cl);
        bx = (int16_t)(DS16(0x1fbc) >> cl);
        if (d <= 0) {
            ax = (int16_t)-ax;
            bx = (int16_t)-bx;
        }
        DS16(0x1fc4) += ax;
        DS16(0x1fc2) -= bx;
    }
    sub_0c9b_024e();
    sub_0c9b_03e5();
    sub_0c9b_0293();
}

/* 0c9b:04a1 - recompute sin/cos from the heading and update the view flags */
void sub_0c9b_04a1(void)
{
    uint8_t h = DS8(0x1fcd);

    DS16(0x1fbc) = DS16(0x2d8c + h * 2);
    DS16(0x1fbe) = DS16(0x2d8c + (uint8_t)(h + 0x40) * 2);
    sub_0c9b_024e();
}

/* 0c9b:04cc - recompute sin/cos from the heading */
void sub_0c9b_04cc(void)
{
    uint8_t h = DS8(0x1fcd);

    DS16(0x1fbc) = DS16(0x2d8c + h * 2);
    DS16(0x1fbe) = DS16(0x2d8c + (uint8_t)(h + 0x40) * 2);
}

/* 0c9b:04f4 - translate/rotate vertices into camera space (optional half-cell cull), close polygon */
void sub_0c9b_04f4(uint8_t *src, uint8_t *dst, int16_t camx, int16_t camy, int16_t mode)
{
    /* self-modified immediates of the original */
    int16_t cosv = DS16(0x1fbe), sinv = DS16(0x1fbc);
    uint16_t cx = DSU16(0x2284);
    uint8_t *si = src, *di = dst;
    int16_t x, y, ay;
    int32_t p;
    int skip;

    s_cs_053f = (uint8_t)mode;     /* opcode table cs:0x4f1 = {jge, jl, mov dl (never)} */
    do {
        x = P16(si);
        y = P16(si + 2);
        switch (s_cs_053f) {
        case 0:  skip = (x >= y); break;
        case 1:  skip = (x < y);  break;
        default: skip = 0;        break;
        }
        if (skip) {
            si += 8;
            DS16(0x2284)--;
            continue;
        }
        s_cs_000e = (int16_t)-(int16_t)(x - camx);
        ay = (int16_t)(y - camy);
        p = (int32_t)((uint32_t)((int32_t)s_cs_000e * cosv) - (uint32_t)((int32_t)ay * sinv));
        P16(di) = (int16_t)(p >> 14);
        p = (int32_t)((uint32_t)((int32_t)ay * cosv) + (uint32_t)((int32_t)s_cs_000e * sinv));
        P16(di + 2) = (int16_t)(p >> 14);
        P16(di + 4) = (int16_t)(P16(si + 4) + DS16(0x1fc6));
        P16(di + 6) = P16(si + 6);
        si += 8;
        di += 8;
    } while (--cx);
    memmove(di, dst, 8);           /* duplicate first vertex to close the polygon */
}

/* 0c9b:05b2 - expand a tile object definition into 8-byte vertices (order from view flags) */
void sub_0c9b_05b2(uint8_t *def, uint8_t *dst)
{
    uint16_t cx = def[0];
    uint8_t *si, *di = dst;
    uint8_t f;
    uint16_t ax, bx, bp;

    DS16(0x2284) = (int16_t)cx;
    if (cx == 0)
        return;
    si = def + 2;
    f = DS8(0x1fcb);

    if (!(f & 0x40)) {
        if (f & 0x80) {
            /* forward order */
            do {
                PU16(di) = (uint16_t)(si[0] << 3);
                ax = (uint16_t)(si[1] << 3);
                PU16(di + 2) = ax;
                PU16(di + 4) = ax;
                PU16(di + 6) = PU16(si + 2);
                si += 4;
                di += 8;
            } while (--cx);
        } else {
            /* reverse order */
            si += (uint16_t)((cx - 1) << 2);
            do {
                PU16(di) = (uint16_t)(si[0] << 3);
                ax = (uint16_t)(si[1] << 3);
                PU16(di + 2) = ax;
                PU16(di + 4) = ax;
                PU16(di + 6) = PU16(si + 2);
                si -= 4;
                di += 8;
            } while (--cx);
        }
        return;
    }

    /* indexed order: byte 3 of each entry = index of the entry holding the point */
    if (f & 0x80) {
        bp = 3;
        do {
            bx = (uint16_t)(si[bp] << 2);
            bp += 4;
            PU16(di) = (uint16_t)(si[bx] << 3);
            ax = (uint16_t)(si[bx + 1] << 3);
            PU16(di + 2) = ax;
            PU16(di + 4) = ax;
            ax = (uint16_t)((ax & 0xff00) | si[bx + 2]);  /* AH keeps high byte of y*8 */
            PU16(di + 6) = ax;
            di += 8;
        } while (--cx);
    } else {
        bp = (uint16_t)(((cx - 1) << 2) + 3);
        do {
            bx = (uint16_t)(si[bp] << 2);
            bp -= 4;
            PU16(di) = (uint16_t)(si[bx] << 3);
            ax = (uint16_t)(si[bx + 1] << 3);
            PU16(di + 2) = ax;
            PU16(di + 4) = ax;
            ax = (uint16_t)((ax & 0xff00) | si[bx + 2]);
            PU16(di + 6) = ax;
            di += 8;
        } while (--cx);
    }
}

/* 0c9b:06a4 - frustum cull buffer A in place (10 < depth < 600, |x| < 256, |x| < depth) */
void sub_0c9b_06a4(void)
{
    uint8_t *si = DSPTR(0x34a6), *di = si;
    uint16_t cx = DSU16(0x2284), dx = 0;
    int16_t ax, bx;

    do {
        ax = P16(si);
        bx = P16(si + 2);
        if (bx > 10 && bx < 0x258 && ax > -0x100 && ax < 0x100) {
            if (ax < 0)
                ax = (int16_t)-ax;
            if (ax < bx) {
                dx++;
                memmove(di, si, 8);
                di += 8;
                si += 8;
                continue;
            }
        }
        si += 8;
    } while (--cx);
    DS16(0x2284) = (int16_t)dx;
}

/* 0c9b:06fe - perspective projection of buffer A: x*256/(d+10)+160, z*256/(d+10)+100 */
void sub_0c9b_06fe(void)
{
    uint16_t cx = DSU16(0x2284);
    uint8_t *si = DSPTR(0x34a6);
    int16_t d;

    do {
        d = (int16_t)(P16(si + 2) + 10);
        if (d == 0)
            d = 1;
        P16(si) = (int16_t)(idiv16((int32_t)P16(si) * 256, d) + 0xa0);
        P16(si + 4) = (int16_t)(idiv16((int32_t)P16(si + 4) * 256, d) + 0x64);
        si += 8;
    } while (--cx);
}

/* 0c9b:0744 - expand polygon byte pairs (count at src[-2]) into vertices in buffer A */
void sub_0c9b_0744(uint8_t *src)
{
    uint8_t *di = DSPTR(0x34a6);
    uint16_t cx = src[-2];
    uint16_t x, y;

    DS16(0x2284) = (int16_t)cx;
    if (cx == 0)
        return;                    /* TODO(port): original `loop` would run 65536 times */
    do {
        x = (uint16_t)(src[0] << 3);
        y = (uint16_t)(src[1] << 3);
        src += 2;
        PU16(di) = x;
        PU16(di + 2) = y;
        PU16(di + 4) = y;
        PU16(di + 6) = y;
        di += 8;
    } while (--cx);
}

/* 0c9b:077c - clip closed polygon B against the near plane (depth > 0) into buffer A */
void sub_0c9b_077c(void)
{
    uint8_t *si = DSPTR(0x34ae), *di = DSPTR(0x34a6);
    uint16_t cx = DSU16(0x2284);
    uint16_t prev_in, cur_in;
    int16_t bp, cy, bx;

    DS16(0x2284) = 0;
    prev_in = (P16(si + 2) > 0);
    si += 8;
    do {
        cur_in = (P16(si + 2) > 0);
        si += 8;
        s_cs_000c = cur_in;
        if (prev_in) {
            memcpy(di, si - 0x10, 6);
            di += 8;
            DS16(0x2284)++;
        }
        if (prev_in != cur_in) {
            bp = (int16_t)(P16(si - 4) - P16(si - 0xc));
            cy = (int16_t)(P16(si - 6) - P16(si - 0xe));
            bx = (int16_t)(P16(si - 8) - P16(si - 0x10));
            if (bx > 0) {
                P16(di) = (int16_t)(P16(si - 0x10) - idiv16((int32_t)P16(si - 0xe) * bx, cy));
                P16(di + 2) = 0;
                P16(di + 4) = (int16_t)(P16(si - 0xc) - idiv16((int32_t)P16(si - 0xe) * bp, cy));
            } else {
                P16(di) = (int16_t)(P16(si - 8) - idiv16((int32_t)P16(si - 6) * bx, cy));
                P16(di + 2) = 0;
                P16(di + 4) = (int16_t)(P16(si - 4) - idiv16((int32_t)P16(si - 6) * bp, cy));
            }
            di += 8;
            DS16(0x2284)++;
        }
        prev_in = cur_in;
    } while (--cx);
}

/* 0c9b:083f - compact buffer A vertices to 2D points (sx, sy) for the polygon filler */
void sub_0c9b_083f(void)
{
    uint16_t cx = DSU16(0x2284);
    uint8_t *si = DSPTR(0x34a6), *di = si;

    do {
        PU16(di) = PU16(si);
        PU16(di + 2) = PU16(si + 4);
        di += 4;
        si += 8;
    } while (--cx);
}

/* 0c9b:0861 - append object (x,y) with id to the 3D object list, tagged with its tile */
void sub_0c9b_0861(int16_t x, int16_t y, int16_t id)
{
    uint8_t *di = DSPTR(0x349e);
    uint16_t n = DSU16(0x3496);
    uint8_t *t;
    uint16_t ax = 0;
    int16_t tx, ty;

    DS16(0x3496)++;
    di += (uint16_t)(n << 3);
    PU16(di) = (uint16_t)x;
    PU16(di + 2) = (uint16_t)y;
    PU16(di + 4) = (uint16_t)y;
    di += 6;

    t = DSPTR(0x34aa);
    do {
        tx = P16(t + 0x328);
        ty = P16(t + 0x32a);
        if (!(tx > x) && !((int16_t)(tx + 0x1ff) < x) && !(ty > y) && !((int16_t)(ty + 0x1ff) < y)) {
            if ((int16_t)(x & 0x1ff) > (int16_t)(y & 0x1ff))
                ax = (uint16_t)((ax & 0xff00) | (uint8_t)(ax + 0x80));
            ax = (uint16_t)(((ax & 0xff) << 8) | (uint8_t)id);
            PU16(di) = ax;
            return;
        }
        t += 0x400;
        ax++;
    } while (ax != 4);
    DS16(0x3496)--;
}

/* 0c9b:08e8 - place objects on the terrain, transform, cull, project, copy back to the list */
void sub_0c9b_08e8(void)
{
    uint16_t cx = DSU16(0x3496);
    uint8_t *di = DSPTR(0x349e);
    int16_t h;

    DS16(0x2284) = (int16_t)cx;
    do {
        h = sub_0c9b_0323(P16(di), P16(di + 2), P8(di + 7) & 3);
        P16(di + 4) = (int16_t)-h;
        di += 8;
    } while (--cx);
    sub_0c9b_04f4(DSPTR(0x349e), DSPTR(0x34a6), DS16(0x1fc2), DS16(0x1fc4), 2);
    sub_0c9b_06a4();
    if (DS16(0x2284) != 0)
        sub_0c9b_06fe();
    cx = DSU16(0x2284);
    DS16(0x3496) = (int16_t)cx;
    memmove(DSPTR(0x349e), DSPTR(0x34a6), (size_t)(uint16_t)(cx << 2) * 2);
}

/* 0c9b:0974 - insert 8-byte entry at pos, shifting the n following entries up */
void sub_0c9b_0974(uint8_t *entry, uint8_t *pos, int16_t n)
{
    memmove(pos + 8, pos, (size_t)(uint16_t)(n << 2) * 2);
    memmove(pos, entry, 8);
}

/* 0c9b:09ae - depth-sorted insert of the objects of (tile, side) into the polygon stream A */
void sub_0c9b_09ae(int16_t tile, int16_t side)
{
    uint16_t cx = DSU16(0x3496);
    uint8_t *si = DSPTR(0x349e);
    uint8_t *di;
    uint16_t w, c2;
    int16_t d;

    do {
        w = PU16(si + 6);
        if ((uint8_t)((w >> 8) & 0x7f) == (uint8_t)tile &&
            (side == 2 || (uint8_t)((w >> 15) & 1) == (uint8_t)side)) {
            di = DSPTR(0x34a6);
            c2 = DSU16(0x2284);
            if (c2 != 0) {
                d = P16(si + 2);
                do {
                    if (d >= P16(di + 2))
                        break;
                    di += 8;
                } while (--c2);
            }
            sub_0c9b_0974(si, di, (int16_t)c2);
            DS16(0x2284)++;
        }
        si += 8;
    } while (--cx);
}

/* 0c9b:0a15 - (386) paint the sky: rows of colour 0xc0 then 2-row bands up to colour 0xff */
void sub_0c9b_0a15(int16_t horizon)
{
    uint8_t *di = DSPTR(0x2d38);
    int16_t dx = horizon;
    uint16_t bx = (uint16_t)horizon;
    uint8_t al;

    dx = (int16_t)(dx - 0x80);
    if (dx > 0) {
        bx = 0x80;
        do {
            memset(di, 0xc0, 320);
            di += 320;
        } while (--dx);
    }
    al = (uint8_t)((uint16_t)(0x200 - bx) >> 1);
    do {
        memset(di, al, 640);
        di += 640;
        al++;
    } while (al != 0);
}

/* 0c9b:0a66 - (8086) same as sub_0c9b_0a15 with word stores */
void sub_0c9b_0a66(int16_t horizon)
{
    uint8_t *di = DSPTR(0x2d38);
    int16_t dx = horizon;
    uint16_t bx = (uint16_t)horizon;
    uint8_t al;

    dx = (int16_t)(dx - 0x80);
    if (dx > 0) {
        bx = 0x80;
        do {
            memset(di, 0xc0, 320);
            di += 320;
        } while (--dx);
    }
    al = (uint8_t)((uint16_t)(0x200 - bx) >> 1);
    do {
        memset(di, al, 640);
        di += 640;
        al++;
    } while (al != 0);
}

/* 0c9b:0ae4 - (register args ax/si/bx + patched imm) plot one star layer: 6 columns x 4 points */
void sub_0c9b_0ae4(int16_t color, uint16_t src, int16_t x0, int16_t y0)
{
    int16_t col, k;
    uint16_t dx, ax;

    sub_1100_00c0((uint8_t)color);
    for (col = 0; col < 6; col++) {
        for (k = 0; k < 4; k++) {
            dx = DS8(src);
            ax = (uint16_t)(DS8((uint16_t)(src + 1)) + y0);
            src += 2;
            sub_1100_000b((int16_t)(dx + x0), (int16_t)ax);
        }
        src += 0x18;
        x0 += 0x40;
    }
}

/* 0c9b:0b20 - draw 4 layers of stars/skyline from DS 0x23c0, scrolled by heading, above horizon */
void sub_0c9b_0b20(void)
{
    int16_t y0 = (int16_t)(DS16(0x34b8) - 0x73);      /* patched imm cs:0xaf8 */
    uint8_t h = DS8(0x1fcd);
    int16_t bx, dx;
    uint16_t si;

    bx = (int16_t)(0 - ((h & 7) << 3));
    dx = (int16_t)((h >> 3) - 2);
    if (dx < 0)
        dx += 0x20;
    si = (uint16_t)(0x23c0 + (dx << 5));
    sub_0c9b_0ae4(0x9f, si, bx, y0);
    sub_0c9b_0ae4(0x9d, (uint16_t)(si + 8), bx, y0);
    sub_0c9b_0ae4(0x9b, (uint16_t)(si + 16), bx, y0);
    sub_0c9b_0ae4(0x99, (uint16_t)(si + 24), bx, y0);
}

/* 0c9b:0b85 - (386) fill nrows full rows of the draw buffer from row y with color */
void sub_0c9b_0b85(int16_t y, int16_t nrows, int16_t color)
{
    uint8_t *di = DSPTR(0x2d38) + SSU16((uint16_t)(y * 2));
    uint16_t dx = (uint16_t)nrows;

    do {
        memset(di, (uint8_t)color, 320);
        di += 320;
    } while (--dx);
}

/* 0c9b:0bba - (8086) same as sub_0c9b_0b85 with word stores */
void sub_0c9b_0bba(int16_t y, int16_t nrows, int16_t color)
{
    uint8_t *di = DSPTR(0x2d38) + SSU16((uint16_t)(y * 2));
    uint16_t dx = (uint16_t)nrows;

    do {
        memset(di, (uint8_t)color, 320);
        di += 320;
    } while (--dx);
}

/* 0c9b:0c0b - (386) copy view rows y..171 from src buffer to dst (screen) */
void sub_0c9b_0c0b(uint8_t *src, uint8_t *dst, int16_t y)
{
    /* the asm replaces both pointer offsets by the row offset (buffers start at offset 0) */
    uint16_t off = SSU16((uint16_t)(y * 2));
    uint16_t end = SSU16(0x158);                       /* row 172 */
    uint16_t cnt = (uint16_t)(end - off) >> 2;

    DS16(0x2d66)++;
    memmove(dst + off, src + off, (size_t)cnt * 4);
    plat_yield();   /* port: let the platform present/pump events */
}

/* 0c9b:0c40 - (8086) copy view rows y..171 from src buffer to dst (screen) */
void sub_0c9b_0c40(uint8_t *src, uint8_t *dst, int16_t y)
{
    uint16_t off = SSU16((uint16_t)(y * 2));
    uint16_t end = SSU16(0x158);
    uint16_t cnt = (uint16_t)(end - off) >> 1;

    DS16(0x2d66)++;
    memmove(dst + off, src + off, (size_t)cnt * 2);
    plat_yield();   /* port: let the platform present/pump events */
}

/* 0c9b:0c70 - blit the 320x172 view scaled 3/4 (240x129) to dst at (40,21) */
void sub_0c9b_0c70(uint8_t *src, uint8_t *dst, int16_t y_unused)
{
    uint8_t *si = src, *di = dst + 0x1a68;
    int16_t dx, bx, cx;

    (void)y_unused;
    DS16(0x2d66)++;
    for (dx = 0; dx < 0x2b; dx++) {
        for (bx = 0; bx < 3; bx++) {
            for (cx = 0; cx < 0x50; cx++) {
                di[0] = si[0];
                di[1] = si[1];
                di[2] = si[2];
                di += 3;
                si += 4;
            }
            di += 0x50;
        }
        si += 0x140;
    }
    plat_yield();   /* port: let the platform present/pump events */
}

/* 0c9b:0ca6 - blit the 320x172 view scaled 1/2 (160x86) to dst at (80,43) */
void sub_0c9b_0ca6(uint8_t *src, uint8_t *dst, int16_t y_unused)
{
    uint8_t *si = src, *di = dst + 0x3610;
    int16_t dx, cx;

    (void)y_unused;
    DS16(0x2d66)++;
    for (dx = 0; dx < 0x56; dx++) {
        for (cx = 0; cx < 0xa0; cx++) {
            *di++ = si[0];
            si += 2;
        }
        si += 0x140;
        di += 0xa0;
    }
    plat_yield();   /* port: let the platform present/pump events */
}

/* 0c9b:0e15 - CPU probe (FLAGS bits 12-14 writable => 386+); the port always reports a 386 */
int16_t sub_0c9b_0e15(void)
{
    return 1;
}
