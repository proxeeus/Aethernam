/*
 * Intro segment 050c: vector "shape" renderer (VA2 pictures).
 *
 * Shape data: data + w[data] = directory; shape i = dir + w[dir + (i & 0xfff) * 4]:
 *   u8 flags, u8 nprims, then per primitive { u8 type, u8 npts, u8 c8, u8 c6,
 *   npts x (x, y) } with words (or signed bytes when mode bit 0x10).
 * mode = ((index >> 8) & 0xa0) | flags: 0x10 byte coordinates, 0x80 negate x, 0x20 negate y.
 * The original implements the mode by patching its own vertex loop (050c_0405 toggles
 * lodsw<->lodsb/cbw and nop<->neg at cs:0x4d2/0x4d4/0x4da/0x4dc); here it is a variable.
 * Primitive types (jump table cs:0x0d): 0 sub-shape, 1 sprite, 2/3 skip block, 4 filled
 * polygon (+ outline c8), 5 filled rectangle (+ outline c8), 6 polyline, 7 points.
 *
 * Code-segment variables: cs:[6] colour c6, cs:[8] colour c8, cs:[0xa] current mode,
 * cs:[0x1d..] vertex buffer (x, y words).
 */
#include "../core/mem.h"
#include "../platform/platform.h"
#include "intro_protos.h"

static uint16_t s_c6;                           /* cs:[6] */
static uint16_t s_c8;                           /* cs:[8] */
static uint8_t  s_mode;                         /* cs:[0xa] */
static int16_t  s_vbuf[2 * 258];                /* cs:[0x1d] */

/* 050c:0405  makes the vertex loop match mode dh (re-patches the changed bits) */
void intro_050c_0405(uint8_t mode)
{
    s_mode = mode;
}

/* 050c:0475  draws shape idx of the shape data `data` at offset (x, y) into the draw buffer */
void intro_050c_0475(uint16_t idx, int16_t x, int16_t y, uint8_t *data)
{
    uint8_t *dir = data + PU16(data);
    uint8_t *si = dir + PU16(dir + (uint16_t)((idx & 0x0fff) << 2));
    uint8_t mode = (uint8_t)(((idx >> 8) & 0xa0) | si[0]);
    uint16_t nprims = si[1], npts, k;
    uint8_t type;
    si += 2;
    if (nprims == 0) return;
    if (mode != s_mode) intro_050c_0405(mode);
    do {
        npts = si[1];
        s_c8 = si[2];
        s_c6 = si[3];
        type = si[0];
        si += 4;
        if (npts == 0) return;  /* TODO(port): "loop" with cx = 0 reads 65536 vertices */
        for (k = 0; k < npts; k++) {
            int16_t vx, vy;
            if (s_mode & 0x10) { vx = (int8_t)si[0]; si += 1; }
            else               { vx = P16(si);       si += 2; }
            if (s_mode & 0x80) vx = (int16_t)-vx;
            s_vbuf[k * 2] = (int16_t)(vx + x);
            if (s_mode & 0x10) { vy = (int8_t)si[0]; si += 1; }
            else               { vy = P16(si);       si += 2; }
            if (s_mode & 0x20) vy = (int16_t)-vy;
            s_vbuf[k * 2 + 1] = (int16_t)(vy + y);
        }
        switch (type) {                         /* call cs:[bx+0x0d] (ds = DGROUP) */
        case 0: intro_050c_080b(data, npts, &si); break;
        case 1: intro_050c_0848(data, npts, &si); break;
        case 2:
        case 3: intro_050c_08a6(data, npts, &si); break;
        case 4: intro_050c_0732(data, npts, &si); break;
        case 5: intro_050c_0772(data, npts, &si); break;
        case 6: intro_050c_07b6(data, npts, &si); break;
        case 7: intro_050c_07e1(data, npts, &si); break;
        default: break;   /* TODO(port): the original jumps through the vertex buffer */
        }
    } while (--nprims);
}

/* 050c:0732  filled polygon of n vertices in colour c6 (0610_0000), outlined in c8
 * unless c8 == 0xff */
void intro_050c_0732(uint8_t *data, uint16_t n, uint8_t **src)
{
    (void)data; (void)src;
    intro_0610_0000(0, 0, (uint8_t *)s_vbuf, n, s_c6);
    if ((uint8_t)s_c8 == 0xff) return;
    s_c6 = (s_c6 & 0xff00) | (uint8_t)s_c8;
    s_vbuf[n * 2] = s_vbuf[0];                  /* close the outline */
    s_vbuf[n * 2 + 1] = s_vbuf[1];
    intro_050c_07b6(data, (uint16_t)(n + 1), src);
}

/* 050c:0772  filled rectangle between the first two vertices in colour c6 (0597_0026),
 * framed in c8 (060b_000a) unless c8 == 0xff */
void intro_050c_0772(uint8_t *data, uint16_t n, uint8_t **src)
{
    int16_t ax = s_vbuf[0], bx = s_vbuf[1], cx = s_vbuf[2], dx = s_vbuf[3], t;
    (void)data; (void)n; (void)src;
    if (ax > cx) { t = ax; ax = cx; cx = t; }
    if (bx > dx) { t = bx; bx = dx; dx = t; }
    intro_0597_0026(ax, bx, cx, dx, (int16_t)s_c6);
    if ((uint8_t)s_c8 == 0xff) return;
    /* the colour argument on the stack gets its low byte replaced by c8 */
    intro_060b_000a(ax, bx, cx, dx, (int16_t)((s_c6 & 0xff00) | (uint8_t)s_c8));
}

/* 050c:07b6  polyline through the n vertices in colour c6 */
void intro_050c_07b6(uint8_t *data, uint16_t n, uint8_t **src)
{
    uint16_t cx = (uint16_t)(n - 1);
    int16_t *v = s_vbuf;
    (void)data; (void)src;
    if (cx == 0) return;    /* TODO(port): cx = 0 made the original draw 65536 segments */
    do {
        intro_04e2_0004(v[0], v[1], v[2], v[3], (int16_t)s_c6);
        v += 2;
    } while (--cx);
}

/* 050c:07e1  plots the n vertices in colour c6 */
void intro_050c_07e1(uint8_t *data, uint16_t n, uint8_t **src)
{
    uint16_t cx = n;
    int16_t *v = s_vbuf;
    (void)data; (void)src;
    intro_05a2_00be((int16_t)s_c6);
    do {
        intro_05a2_0009(v[0], v[1]);
        v += 2;
    } while (--cx);
}

/* 050c:080b  draws sub-shape (c6 | mode) << 8 | c8 at the vertex, then restores the mode */
void intro_050c_080b(uint8_t *data, uint16_t n, uint8_t **src)
{
    uint8_t saved = s_mode;
    uint16_t idx = (uint16_t)(((uint8_t)s_c6 | s_mode) << 8 | (uint8_t)s_c8);
    (void)n; (void)src;
    intro_050c_0475(idx, s_vbuf[0], s_vbuf[1], data);
    if (saved != s_mode) intro_050c_0405(saved);
}

/* 050c:0848  draws sprite (c6 ^ mode) << 8 | c8 of the bank data + w[data+4] at the vertex
 * (mirrored sprites are shifted left by their width, y-flipped ones down by their height) */
void intro_050c_0848(uint8_t *data, uint16_t n, uint8_t **src)
{
    uint8_t *bank = data + PU16(data + 4);
    uint16_t ax = (uint16_t)((uint8_t)((uint8_t)s_c6 ^ s_mode) << 8 | (uint8_t)s_c8);
    (void)n; (void)src;
    if (s_mode & 0x80) s_vbuf[0] = (int16_t)(s_vbuf[0] - intro_05fd_000a(ax, bank));
    if (s_mode & 0x20) s_vbuf[1] = (int16_t)(s_vbuf[1] + intro_05fd_0052(ax, bank));
    intro_0488_01d0(ax, s_vbuf[0], s_vbuf[1], DSPTR(0x148), bank);
}

/* 050c:08a6  types 2/3: skips the rest of a 256-entry table (si += (0x100 - n) * 4) */
void intro_050c_08a6(uint8_t *data, uint16_t n, uint8_t **src)
{
    (void)data;
    *src += (uint16_t)((uint16_t)(0x100 - n) << 2);
}
