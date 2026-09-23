/*
 * Intro segment 0610: filled polygon (Sutherland-Hodgman clipping against the SS clip
 * rectangle, then two x tables filled by DDA edges and a span fill).
 *
 * DGROUP work areas: 0x394 / 0x396 current / other vertex list (0x39c and 0x52c, x,y word
 * pairs), 0x398 / 0x39a the two x tables (0x6bc and the free vertex list, indexed by y*2),
 * 0x84c / 0x84e / 0x850 vertex counts, 0x852 "clipped" flag, 0x854 max x.
 * Patched immediates of the original (code-segment variables): cs:0x60 x offset,
 * cs:0x6d y offset, cs:0x18c min y, cs:0x192 max y, cs:0x1a3 current x table, cs:0x21a
 * fill colour word.
 */
#include "../core/mem.h"
#include "../platform/platform.h"
#include "intro_protos.h"
#include <string.h>

static int16_t  s_xoff, s_yoff;                 /* cs:0x60, cs:0x6d */
static int16_t  s_ymin, s_ymax;                 /* cs:0x18c, cs:0x192 */
static uint16_t s_table;                        /* cs:0x1a3 */
static uint16_t s_color;                        /* cs:0x21a */

/* common tail of the 4 clippers: closes the output list and swaps the two lists */
static void clip_finish(uint16_t di)
{
    uint16_t si = DSU16(0x396), t;
    DSU16(di) = DSU16(si);
    DSU16(di + 2) = DSU16(si + 2);
    t = DSU16(0x394);
    DSU16(0x394) = DSU16(0x396);
    DSU16(0x396) = t;
}

/* x clip (left if !right: keeps x >= bound; right: keeps x <= bound) */
static void clip_x(int16_t bound, int right)
{
    uint16_t si, di;
    int16_t cx, bx, dx, ax, bp = bound;
    DS16(0x850) = 0;
    DS16(0x852) = 1;
    si = DSU16(0x394);
    di = DSU16(0x396);
    cx = DS16(si); bx = DS16(si + 2); si += 4;
    do {
        int out0, out1, cross;
        dx = DS16(si); ax = DS16(si + 2); si += 4;
        out0 = right ? (cx > bp) : (cx < bp);
        out1 = right ? (dx > bp) : (dx < bp);
        cross = 0;
        if (out0) {
            if (!out1) cross = 1;
        } else {
            DS16(di) = cx; DS16(di + 2) = bx; di += 4;
            DS16(0x850)++;
            if (out1) cross = 1;
        }
        if (cross) {
            /* push dx / push ax; xchg dx,ax: (ax, dx) = (x1, y1) */
            int16_t x1 = dx, y1 = ax, sx = cx, sy = bx, ex = x1, ey = y1, t;
            if (!(ex > sx)) { t = sx; sx = ex; ex = t; t = sy; sy = ey; ey = t; }
            {
                int16_t ddy = (int16_t)(ey - sy), ddx = (int16_t)(ex - sx);
                int16_t q = (int16_t)(((int32_t)(int16_t)(bp - sx) * ddy) / ddx);
                DS16(di) = bp; DS16(di + 2) = (int16_t)(sy + q); di += 4;
            }
            DS16(0x850)++;
            bx = y1; cx = x1;                   /* pop bx / pop cx */
        } else {
            cx = dx; bx = ax;
        }
    } while (--DSU16(0x84e));
    clip_finish(di);
}

/* y clip (top if !bottom: keeps y >= bound; bottom: keeps y <= bound) */
static void clip_y(int16_t bound, int bottom)
{
    uint16_t si, di;
    int16_t cx, bx, dx, ax, bp = bound;
    DS16(0x850) = 0;
    DS16(0x852) = 1;
    si = DSU16(0x394);
    di = DSU16(0x396);
    bx = DS16(si); cx = DS16(si + 2); si += 4;  /* bx = x0, cx = y0 */
    do {
        int out0, out1, cross;
        dx = DS16(si); ax = DS16(si + 2); si += 4;
        out0 = bottom ? (cx > bp) : (cx < bp);
        out1 = bottom ? (ax > bp) : (ax < bp);
        cross = 0;
        if (out0) {
            if (!out1) cross = 1;
        } else {
            DS16(di) = bx; DS16(di + 2) = cx; di += 4;
            DS16(0x850)++;
            if (out1) cross = 1;
        }
        if (cross) {
            int16_t ddx = (int16_t)(dx - bx), ddy = (int16_t)(ax - cx);
            int16_t q = (int16_t)(((int32_t)(int16_t)(bp - cx) * ddx) / ddy);
            DS16(di) = (int16_t)(q + bx); DS16(di + 2) = bp; di += 4;
            DS16(0x850)++;
        }
        cx = ax; bx = dx;                       /* pop cx / pop bx, or 035a */
    } while (--DSU16(0x84e));
    clip_finish(di);
}

/* 0610:0230  clips the current vertex list against x >= bound */
void intro_0610_0230(int16_t bound) { clip_x(bound, 0); }
/* 0610:02ac  clips the current vertex list against x <= bound */
void intro_0610_02ac(int16_t bound) { clip_x(bound, 1); }
/* 0610:0328  clips the current vertex list against y >= bound */
void intro_0610_0328(int16_t bound) { clip_y(bound, 0); }
/* 0610:039a  clips the current vertex list against y <= bound */
void intro_0610_039a(int16_t bound) { clip_y(bound, 1); }

/* 0610:0000  fills the polygon of n vertices (x, y words at pts, offset by xoff/yoff) in
 * `color` into the draw buffer, clipped to the SS clip rectangle */
void intro_0610_0000(int16_t xoff, int16_t yoff, uint8_t *pts, uint16_t n, uint16_t color)
{
    int16_t bx, bp, cx, dx, ax;
    uint16_t di, si, cnt;
    uint8_t *p = pts, *row, dh;

    s_color = (uint16_t)((color & 0xff) * 0x101);
    s_xoff = xoff;
    s_yoff = yoff;
    DSU16(0x84c) = n;
    DSU16(0x84e) = n;
    DSU16(0x850) = n;
    DS16(0x852) = 0;
    if (n == 0) return;     /* TODO(port): n = 0 made the original read 65536 vertices */
    bx = 0x7fff; bp = 0x7fff; cx = (int16_t)0x8000; dx = (int16_t)0x8000;
    di = 0x39c;
    do {
        ax = (int16_t)(P16(p) + s_xoff); p += 2;
        if (ax < bx) bx = ax;
        if (ax > dx) dx = ax;
        DS16(di) = ax; di += 2;
        ax = (int16_t)(P16(p) + s_yoff); p += 2;
        if (ax < bp) bp = ax;
        if (ax > cx) cx = ax;
        DS16(di) = ax; di += 2;
    } while (--DSU16(0x84e));
    DSU16(di) = DSU16(0x39c);
    DSU16(di + 2) = DSU16(0x39e);
    s_ymin = bp;
    DS16(0x854) = dx;
    s_ymax = cx;
    if (cx == bp) return;
    DSU16(0x394) = 0x39c;
    DSU16(0x396) = 0x52c;
    DSU16(0x84e) = DSU16(0x850);
    if (bx < ISS16(0x196)) {
        intro_0610_0230(ISS16(0x196));
        if (DSU16(0x850) <= 2) return;
        DSU16(0x84e) = DSU16(0x850);
    }
    if (DS16(0x854) > ISS16(0x198)) {
        intro_0610_02ac(ISS16(0x198));
        if (DSU16(0x850) <= 2) return;
        DSU16(0x84e) = DSU16(0x850);
    }
    if (s_ymin < ISS16(0x192)) {
        intro_0610_0328(ISS16(0x192));
        if (DSU16(0x850) <= 2) return;
        DSU16(0x84e) = DSU16(0x850);
    }
    if (s_ymax > ISS16(0x194)) {
        intro_0610_039a(ISS16(0x194));
        if (DSU16(0x850) <= 2) return;
        DSU16(0x84e) = DSU16(0x850);
    }
    if (DS16(0x852) != 0) {                     /* 0124: new y range of the clipped list */
        si = DSU16(0x394);
        cnt = DSU16(0x84e);
        bx = 0x7fff; dx = (int16_t)0x8000;
        do {
            si += 2;
            ax = DS16(si); si += 2;
            if (ax < bx) bx = ax;
            if (ax > dx) dx = ax;
        } while (--cnt);
        s_ymin = bx;
        s_ymax = dx;
        if (bx == dx) return;
    }

    /* 014f: scan-convert the edges into the two x tables */
    DSU16(0x398) = 0x6bc;
    s_table = 0x6bc;
    DSU16(0x39a) = DSU16(0x396);
    si = DSU16(0x394);
    bx = DS16(si); cx = DS16(si + 2); si += 4;
    do {
        int16_t x1, y1;
        dx = DS16(si); ax = DS16(si + 2); si += 4;
        x1 = dx; y1 = ax;                       /* push dx / push ax */
        if (ax != cx) {
            int dir = 1;
            int16_t t, dxl, dyl, e;
            if (cx == s_ymin || cx == s_ymax) { /* 016b: extreme vertex: other table */
                uint16_t tt = DSU16(0x398);
                DSU16(0x398) = DSU16(0x39a);
                DSU16(0x39a) = tt;
                s_table = DSU16(0x398);
            }
            if (bx >= dx) { t = dx; dx = bx; bx = t; t = cx; cx = ax; ax = t; }
            di = (uint16_t)((uint16_t)(cx << 1) + s_table);
            if (cx >= ax) dir = -1;             /* std */
            else { t = cx; cx = ax; ax = t; }
            cx = (int16_t)(cx - ax);            /* number of rows */
            ax = bx;
            DS16(di) = ax; di = (uint16_t)(di + 2 * dir);
            dxl = (int16_t)(dx - ax);
            dyl = cx;
            e = (int16_t)((uint16_t)cx >> 1);
            cnt = (uint16_t)cx;
            do {
                e = (int16_t)(e + dxl);
                while (e >= dyl) { e = (int16_t)(e - dyl); ax++; }
                DS16(di) = ax; di = (uint16_t)(di + 2 * dir);
            } while (--cnt);
        }
        cx = y1; bx = x1;                       /* pop cx / pop bx */
    } while (--DSU16(0x84e));

    /* 01d3: span fill from ymin to ymax */
    dh = (uint8_t)(s_ymax - s_ymin + 1);
    row = DSPTR(0x148) + ISSU16((uint16_t)(s_ymin << 1));
    si = (uint16_t)(DSU16(0x39a) + (uint16_t)(s_ymin << 1));
    di = (uint16_t)(DSU16(0x398) + (uint16_t)(s_ymin << 1));
    do {
        int16_t a, c, t;
        c = DS16(di); di += 2;
        a = DS16(si); si += 2;
        if (a > c) { t = a; a = c; c = t; }
        memset(row + a, (uint8_t)s_color, (uint16_t)(c - a + 1));
        row += 0x140;
    } while (--dh);
}
