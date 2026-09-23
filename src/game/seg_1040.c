/* Segment 1040: clipped solid line and horizontal line. */
#include "../core/mem.h"
#include "../platform/platform.h"
#include "protos.h"

/* Cohen-Sutherland outcode against the SS clip rect (1 left, 2 right, 4 above, 8 below) */
#define OUTCODE(x, y) ((uint16_t)(((x) < SS16(0x196) ? 1 : 0) | ((x) > SS16(0x198) ? 2 : 0) | \
                                  ((y) < SS16(0x192) ? 4 : 0) | ((y) > SS16(0x194) ? 8 : 0)))

/* 1040:0006  clipped solid line (Cohen-Sutherland + Bresenham) into the draw buffer; fast path for horizontal lines */
void sub_1040_0006(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color)
{
    int16_t ax = x0, bx = y0, cx = x1, dx = y1, t, e;
    uint16_t si, di, bp;

    if (ax > cx) { t = ax; ax = cx; cx = t; t = bx; bx = dx; dx = t; }
    if (dx == bx) {
        /* horizontal line */
        if (ax > SS16(0x198)) return;
        if (cx < SS16(0x196)) return;
        if (ax < SS16(0x196)) ax = SS16(0x196);
        if (cx > SS16(0x198)) cx = SS16(0x198);
        if (bx < SS16(0x192)) return;
        if (bx > SS16(0x194)) return;
        memset(DSPTR(0x2d38) + (uint16_t)(SSU16((uint16_t)(bx * 2)) + ax), color, (uint16_t)(cx - ax + 1));
        return;
    }

    si = OUTCODE(ax, bx);
    di = OUTCODE(cx, dx);
    while ((si | di) != 0) {
        int16_t ddx, ddy;
        if (si & di)
            return;
        bp = si ? si : di;
        DSU16(0x2d84) = 8;
        e = SS16(0x194);
        if (!(DSU16(0x2d84) & bp)) {
            e = SS16(0x192);
            DSU16(0x2d84) >>= 1;
            if (!(DSU16(0x2d84) & bp)) {
                e = SS16(0x198);
                DSU16(0x2d84) >>= 1;
                if (!(DSU16(0x2d84) & bp)) {
                    e = SS16(0x196);
                    DSU16(0x2d84) >>= 1;
                }
            }
        }
        ddx = (int16_t)(cx - ax);
        ddy = (int16_t)(dx - bx);
        if (DSU16(0x2d84) & 0xc) {
            DS16(0x2d8a) = e;
            DS16(0x2d88) = (int16_t)((int16_t)((int32_t)(int16_t)(e - bx) * ddx / ddy) + ax);
        } else {
            DS16(0x2d88) = e;
            DS16(0x2d8a) = (int16_t)((int16_t)((int32_t)(int16_t)(e - ax) * ddy / ddx) + bx);
        }
        DSU16(0x2d86) = OUTCODE(DS16(0x2d88), DS16(0x2d8a));
        if (si == bp) {
            ax = DS16(0x2d88); bx = DS16(0x2d8a); si = DSU16(0x2d86);
        } else {
            cx = DS16(0x2d88); dx = DS16(0x2d8a); di = DSU16(0x2d86);
        }
    }

    {
        int16_t ddx = (int16_t)(cx - ax), ddy = (int16_t)(dx - bx), step = 0x140, acc;
        uint16_t n;
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
                *p = color;
            } while (--n);
        } else {
            acc = (int16_t)((uint16_t)ddy >> 1);
            n = (uint16_t)ddy;
            do {
                p += step;
                acc = (int16_t)(acc + ddx);
                if (acc >= ddy) { acc = (int16_t)(acc - ddy); p++; }
                *p = color;
            } while (--n);
        }
    }
}

/* 1040:023a  horizontal line x0..x1 at row y into the draw buffer, clipped to the clip rect */
void sub_1040_023a(int16_t x0, int16_t y, int16_t x1, uint8_t color)
{
    if (x0 > SS16(0x198)) return;
    if (x1 < SS16(0x196)) return;
    if (x0 < SS16(0x196)) x0 = SS16(0x196);
    if (x1 > SS16(0x198)) x1 = SS16(0x198);
    if (y < SS16(0x192)) return;
    if (y > SS16(0x194)) return;
    memset(DSPTR(0x2d38) + (uint16_t)(SSU16((uint16_t)(y * 2)) + x0), color, (uint16_t)(x1 - x0 + 1));
}
