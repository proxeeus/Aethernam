/* Segment 0d84: clipped convex polygon filler (hand-written assembly in the original). */
#include "../core/mem.h"
#include "../platform/platform.h"
#include "protos.h"

/* Self-modifying immediates of sub_0d84_0002 (cs:0x62, 0x6f, 0x18e, 0x194, 0x1a5, 0x215). */
static int16_t  s_xoff;     /* cs:0x62  x offset added to every vertex        */
static int16_t  s_yoff;     /* cs:0x6f  y offset added to every vertex        */
static int16_t  s_ymin;     /* cs:0x18e top row of the polygon                */
static int16_t  s_ymax;     /* cs:0x194 bottom row of the polygon             */
static uint16_t s_edge;     /* cs:0x1a5 DGROUP offset of the edge table being written (== DS16(0x2848)) */
static uint16_t s_color;    /* cs:0x215 fill colour in both bytes             */

/* 0d84:0002  fills a convex polygon of n (x,y) int16 pairs (+dx,+dy) into the draw buffer, clipped to the clip rect */
void sub_0d84_0002(int16_t dx, int16_t dy, uint8_t *pts, uint16_t n, uint8_t color)
{
    int16_t minx, maxx, miny, maxy, x, y;
    uint16_t di, si, cnt;

    s_color = (uint16_t)(color | (color << 8));
    s_xoff = dx;
    s_yoff = dy;
    DSU16(0x2cfc) = n;
    DSU16(0x2cfe) = n;
    DSU16(0x2d00) = n;
    DSU16(0x2d02) = 0;

    /* copy vertices (plus offset) into DGROUP buffer A, tracking the bounding box */
    minx = 0x7fff; miny = 0x7fff;
    maxx = (int16_t)0x8000; maxy = (int16_t)0x8000;
    di = 0x284c;
    do {
        x = (int16_t)(P16(pts) + s_xoff); pts += 2;
        if (x < minx) minx = x;
        if (x > maxx) maxx = x;
        DS16(di) = x; di += 2;
        y = (int16_t)(P16(pts) + s_yoff); pts += 2;
        if (y < miny) miny = y;
        if (y > maxy) maxy = y;
        DS16(di) = y; di += 2;
    } while (--DSU16(0x2cfe) != 0);
    /* close the list */
    DS16(di) = DS16(0x284c);
    DS16(di + 2) = DS16(0x284e);

    s_ymin = miny;
    DS16(0x2d04) = maxx;
    s_ymax = maxy;
    if (maxy == miny)
        return;

    DSU16(0x2844) = 0x284c;
    DSU16(0x2846) = 0x29dc;
    DSU16(0x2cfe) = DSU16(0x2d00);

    if (minx < SS16(0x196)) {
        sub_0d84_0235(SS16(0x196));
        if (DSU16(0x2d00) <= 2) return;
        DSU16(0x2cfe) = DSU16(0x2d00);
    }
    if (DS16(0x2d04) > SS16(0x198)) {
        sub_0d84_02b0(SS16(0x198));
        if (DSU16(0x2d00) <= 2) return;
        DSU16(0x2cfe) = DSU16(0x2d00);
    }
    if (s_ymin < SS16(0x192)) {
        sub_0d84_032c(SS16(0x192));
        if (DSU16(0x2d00) <= 2) return;
        DSU16(0x2cfe) = DSU16(0x2d00);
    }
    if (s_ymax > SS16(0x194)) {
        sub_0d84_039e(SS16(0x194));
        if (DSU16(0x2d00) <= 2) return;
        DSU16(0x2cfe) = DSU16(0x2d00);
    }

    if (DSU16(0x2d02) != 0) {
        /* clipped: recompute the vertical extent of the clipped polygon */
        int16_t lo = 0x7fff, hi = (int16_t)0x8000;
        si = DSU16(0x2844);
        cnt = DSU16(0x2cfe);
        do {
            si += 2;
            y = DS16(si); si += 2;
            if (y < lo) lo = y;
            if (y > hi) hi = y;
        } while (--cnt);
        s_ymin = lo;
        s_ymax = hi;
        if (lo == hi)
            return;
    }

    /* walk the edges; each non-horizontal edge fills one x-per-row table.  The table
       is toggled whenever an edge starts at the top or bottom vertex (convex polygon). */
    DSU16(0x2848) = 0x2b6c;
    s_edge = 0x2b6c;
    DSU16(0x284a) = DSU16(0x2846);
    si = DSU16(0x2844);
    {
        int16_t x0 = DS16(si), y0 = DS16(si + 2);
        si += 4;
        do {
            int16_t x1 = DS16(si), y1 = DS16(si + 2);
            si += 4;
            if (y1 != y0) {
                int16_t bx, cx, ex, ey, rows, step, acc, dxw, xx;
                uint16_t ptr;
                if (y0 == s_ymin || y0 == s_ymax) {
                    uint16_t t = DSU16(0x284a);
                    DSU16(0x284a) = DSU16(0x2848);
                    DSU16(0x2848) = t;
                    s_edge = t;
                }
                /* start from the end point with the smaller x (end point if equal) */
                if (x0 < x1) { bx = x0; cx = y0; ex = x1; ey = y1; }
                else         { bx = x1; cx = y1; ex = x0; ey = y0; }
                ptr = (uint16_t)(s_edge + cx * 2);
                if (cx < ey) { rows = (int16_t)(ey - cx); step = 2; }
                else         { rows = (int16_t)(cx - ey); step = -2; }   /* std */
                xx = bx;
                DS16(ptr) = xx; ptr = (uint16_t)(ptr + step);
                dxw = (int16_t)(ex - xx);
                acc = (int16_t)((uint16_t)rows >> 1);
                {
                    uint16_t k = (uint16_t)rows;
                    do {
                        acc = (int16_t)(acc + dxw);
                        while (acc >= rows) {
                            acc = (int16_t)(acc - rows);
                            xx++;
                        }
                        DS16(ptr) = xx; ptr = (uint16_t)(ptr + step);
                    } while (--k);
                }
            }
            x0 = x1;
            y0 = y1;
        } while (--DSU16(0x2cfe) != 0);
    }

    /* span fill between the two edge tables */
    {
        uint8_t rows = (uint8_t)(s_ymax - s_ymin + 1);
        uint8_t *row = DSPTR(0x2d38) + SSU16((uint16_t)(s_ymin * 2));
        uint16_t sa = (uint16_t)(DSU16(0x284a) + s_ymin * 2);
        uint16_t sb = (uint16_t)(DSU16(0x2848) + s_ymin * 2);
        do {
            int16_t r = DS16(sb), l = DS16(sa), t;
            sb += 2; sa += 2;
            if (l > r) { t = l; l = r; r = t; }
            memset(row + l, (uint8_t)s_color, (uint16_t)(r - l + 1));
            row += 320;
        } while (--rows);
    }
}

/* 0d84:0235  Sutherland-Hodgman pass: clips the current vertex list against x >= xmin */
void sub_0d84_0235(int16_t xmin)
{
    uint16_t si = DSU16(0x2844), di = DSU16(0x2846), t;
    int16_t px, py, cx, cy;

    DSU16(0x2d00) = 0;
    DSU16(0x2d02) = 1;
    px = DS16(si); py = DS16(si + 2); si += 4;
    do {
        int cut = 0;
        cx = DS16(si); cy = DS16(si + 2); si += 4;
        if (px >= xmin) {
            DS16(di) = px; DS16(di + 2) = py; di += 4;
            DSU16(0x2d00)++;
            if (cx < xmin) cut = 1;
        } else if (cx >= xmin) {
            cut = 1;
        }
        if (cut) {
            int16_t bx_, by_, ox, oy;
            if (cx > px) { bx_ = px; by_ = py; ox = cx; oy = cy; }
            else         { bx_ = cx; by_ = cy; ox = px; oy = py; }
            DS16(di) = xmin;
            DS16(di + 2) = (int16_t)(by_ + (int16_t)((int32_t)(int16_t)(xmin - bx_) * (int16_t)(oy - by_) / (int16_t)(ox - bx_)));
            di += 4;
            DSU16(0x2d00)++;
        }
        px = cx; py = cy;
    } while (--DSU16(0x2cfe) != 0);
    DS16(di) = DS16(DSU16(0x2846));
    DS16(di + 2) = DS16(DSU16(0x2846) + 2);
    t = DSU16(0x2844); DSU16(0x2844) = DSU16(0x2846); DSU16(0x2846) = t;
}

/* 0d84:02b0  Sutherland-Hodgman pass: clips the current vertex list against x <= xmax */
void sub_0d84_02b0(int16_t xmax)
{
    uint16_t si = DSU16(0x2844), di = DSU16(0x2846), t;
    int16_t px, py, cx, cy;

    DSU16(0x2d00) = 0;
    DSU16(0x2d02) = 1;
    px = DS16(si); py = DS16(si + 2); si += 4;
    do {
        int cut = 0;
        cx = DS16(si); cy = DS16(si + 2); si += 4;
        if (px <= xmax) {
            DS16(di) = px; DS16(di + 2) = py; di += 4;
            DSU16(0x2d00)++;
            if (cx > xmax) cut = 1;
        } else if (cx <= xmax) {
            cut = 1;
        }
        if (cut) {
            int16_t bx_, by_, ox, oy;
            if (cx > px) { bx_ = px; by_ = py; ox = cx; oy = cy; }
            else         { bx_ = cx; by_ = cy; ox = px; oy = py; }
            DS16(di) = xmax;
            DS16(di + 2) = (int16_t)(by_ + (int16_t)((int32_t)(int16_t)(xmax - bx_) * (int16_t)(oy - by_) / (int16_t)(ox - bx_)));
            di += 4;
            DSU16(0x2d00)++;
        }
        px = cx; py = cy;
    } while (--DSU16(0x2cfe) != 0);
    DS16(di) = DS16(DSU16(0x2846));
    DS16(di + 2) = DS16(DSU16(0x2846) + 2);
    t = DSU16(0x2844); DSU16(0x2844) = DSU16(0x2846); DSU16(0x2846) = t;
}

/* 0d84:032c  Sutherland-Hodgman pass: clips the current vertex list against y >= ymin */
void sub_0d84_032c(int16_t ymin)
{
    uint16_t si = DSU16(0x2844), di = DSU16(0x2846), t;
    int16_t px, py, cx, cy;

    DSU16(0x2d00) = 0;
    DSU16(0x2d02) = 1;
    px = DS16(si); py = DS16(si + 2); si += 4;
    do {
        int cut = 0;
        cx = DS16(si); cy = DS16(si + 2); si += 4;
        if (py >= ymin) {
            DS16(di) = px; DS16(di + 2) = py; di += 4;
            DSU16(0x2d00)++;
            if (cy < ymin) cut = 1;
        } else if (cy >= ymin) {
            cut = 1;
        }
        if (cut) {
            /* end points are not reordered for the y clippers */
            DS16(di) = (int16_t)(px + (int16_t)((int32_t)(int16_t)(ymin - py) * (int16_t)(cx - px) / (int16_t)(cy - py)));
            DS16(di + 2) = ymin;
            di += 4;
            DSU16(0x2d00)++;
        }
        px = cx; py = cy;
    } while (--DSU16(0x2cfe) != 0);
    DS16(di) = DS16(DSU16(0x2846));
    DS16(di + 2) = DS16(DSU16(0x2846) + 2);
    t = DSU16(0x2844); DSU16(0x2844) = DSU16(0x2846); DSU16(0x2846) = t;
}

/* 0d84:039e  Sutherland-Hodgman pass: clips the current vertex list against y <= ymax */
void sub_0d84_039e(int16_t ymax)
{
    uint16_t si = DSU16(0x2844), di = DSU16(0x2846), t;
    int16_t px, py, cx, cy;

    DSU16(0x2d00) = 0;
    DSU16(0x2d02) = 1;
    px = DS16(si); py = DS16(si + 2); si += 4;
    do {
        int cut = 0;
        cx = DS16(si); cy = DS16(si + 2); si += 4;
        if (py <= ymax) {
            DS16(di) = px; DS16(di + 2) = py; di += 4;
            DSU16(0x2d00)++;
            if (cy > ymax) cut = 1;
        } else if (cy <= ymax) {
            cut = 1;
        }
        if (cut) {
            DS16(di) = (int16_t)(px + (int16_t)((int32_t)(int16_t)(ymax - py) * (int16_t)(cx - px) / (int16_t)(cy - py)));
            DS16(di + 2) = ymax;
            di += 4;
            DSU16(0x2d00)++;
        }
        px = cx; py = cy;
    } while (--DSU16(0x2cfe) != 0);
    DS16(di) = DS16(DSU16(0x2846));
    DS16(di + 2) = DS16(DSU16(0x2846) + 2);
    t = DSU16(0x2844); DSU16(0x2844) = DSU16(0x2846); DSU16(0x2846) = t;
}
