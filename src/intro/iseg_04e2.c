/*
 * Intro segment 04e2: clipped line drawing (Cohen-Sutherland + Bresenham) and horizontal
 * line fill, into the draw buffer DSPTR(0x148).
 * DGROUP temporaries: 0x18a boundary bit, 0x18c outcode, 0x18e / 0x190 clipped x / y.
 */
#include "../core/mem.h"
#include "../platform/platform.h"
#include "intro_protos.h"
#include <string.h>

static uint16_t outcode(int16_t x, int16_t y)
{
    uint16_t c = 0;
    if (x < ISS16(0x196)) c |= 1;
    if (x > ISS16(0x198)) c |= 2;
    if (y < ISS16(0x192)) c |= 4;
    if (y > ISS16(0x194)) c |= 8;
    return c;
}

/* 04e2:0004  draws a line (x0,y0)-(x1,y1) in colour `color`, clipped to the SS clip
 * rectangle; horizontal lines are filled directly; a zero-length line draws nothing */
void intro_04e2_0004(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t color)
{
    int16_t ax = x0, bx = y0, cx = x1, dx = y1, t, bnd, bp;
    uint16_t si, di, code;
    uint8_t *buf, c;
    int32_t off, step;

    if (ax > cx) { t = ax; ax = cx; cx = t; t = bx; bx = dx; dx = t; }
    if (dx == bx) {                             /* 0500a: horizontal */
        if (ax > ISS16(0x198)) return;
        if (cx < ISS16(0x196)) return;
        if (ax < ISS16(0x196)) ax = ISS16(0x196);
        if (cx > ISS16(0x198)) cx = ISS16(0x198);
        if (bx < ISS16(0x192)) return;
        if (bx > ISS16(0x194)) return;
        buf = DSPTR(0x148) + (uint16_t)(ISSU16((uint16_t)(bx << 1)) + ax);
        memset(buf, (uint8_t)color, (uint16_t)(cx - ax + 1));
        return;
    }
    si = outcode(ax, bx);
    di = outcode(cx, dx);
    while ((si | di) != 0) {
        if (si & di) return;                    /* trivially outside */
        code = si ? si : di;
        DS16(0x18a) = 8;
        bnd = ISS16(0x194);
        if (!(DS16(0x18a) & code)) {
            bnd = ISS16(0x192);
            DSU16(0x18a) >>= 1;
            if (!(DS16(0x18a) & code)) {
                bnd = ISS16(0x198);
                DSU16(0x18a) >>= 1;
                if (!(DS16(0x18a) & code)) {
                    bnd = ISS16(0x196);
                    DSU16(0x18a) >>= 1;
                }
            }
        }
        {
            int16_t ddx = (int16_t)(cx - ax), ddy = (int16_t)(dx - bx);
            if (DS16(0x18a) & 0xc) {            /* horizontal boundary y = bnd */
                DS16(0x190) = bnd;
                DS16(0x18e) = (int16_t)((int16_t)(((int32_t)(int16_t)(bnd - bx) * ddx) / ddy) + ax);
            } else {                            /* vertical boundary x = bnd */
                DS16(0x18e) = bnd;
                DS16(0x190) = (int16_t)((int16_t)(((int32_t)(int16_t)(bnd - ax) * ddy) / ddx) + bx);
            }
        }
        DS16(0x18c) = (int16_t)outcode(DS16(0x18e), DS16(0x190));
        if (si == code) {
            ax = DS16(0x18e); bx = DS16(0x190); si = DSU16(0x18c);
        } else {
            cx = DS16(0x18e); dx = DS16(0x190); di = DSU16(0x18c);
        }
    }
    /* 04fa1: draw */
    cx = (int16_t)(cx - ax);
    dx = (int16_t)(dx - bx);
    if (dx == 0 && cx == 0) return;
    c = (uint8_t)color;
    buf = DSPTR(0x148);
    off = (uint16_t)(ISSU16((uint16_t)(bx << 1)) + ax);
    buf[off] = c;
    step = 0x140;
    if (dx < 0) { dx = (int16_t)-dx; step = -0x140; }
    if (cx > dx) {
        uint16_t n = (uint16_t)cx;
        bp = (int16_t)((uint16_t)cx >> 1);
        do {
            off++;
            bp = (int16_t)(bp + dx);
            if (bp >= cx) { bp = (int16_t)(bp - cx); off += step; }
            buf[off] = c;
        } while (--n);
    } else {
        uint16_t n = (uint16_t)dx;
        bp = (int16_t)((uint16_t)dx >> 1);
        do {
            off += step;
            bp = (int16_t)(bp + cx);
            if (bp >= dx) { bp = (int16_t)(bp - dx); off++; }
            buf[off] = c;
        } while (--n);
    }
}

/* 04e2:0238  horizontal line x0..x1 on row y in colour `color`, clipped */
void intro_04e2_0238(int16_t x0, int16_t y, int16_t x1, int16_t color)
{
    int16_t ax = x0, cx = x1;
    uint8_t *buf;
    if (ax > ISS16(0x198)) return;
    if (cx < ISS16(0x196)) return;
    if (ax < ISS16(0x196)) ax = ISS16(0x196);
    if (cx > ISS16(0x198)) cx = ISS16(0x198);
    if (y < ISS16(0x192)) return;
    if (y > ISS16(0x194)) return;
    if (cx < ax) return;    /* TODO(port): x0 > x1 made the original fill ~64K bytes */
    buf = DSPTR(0x148) + (uint16_t)(ISSU16((uint16_t)(y << 1)) + ax);
    memset(buf, (uint8_t)color, (uint16_t)(cx - ax + 1));
}
