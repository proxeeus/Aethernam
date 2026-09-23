/*
 * Segment 0a86 - exterior 3D landscape view (islands) and the main game loop.
 *
 * World: 0x4000 x 0x4000 units, tile = 0x200 (32x32 tiles), sector = 0x1000 (4x4).
 * Terrain cell (DSPTR(0x34aa) + k*0x400, k = 0..3, drawn 3..0 back to front):
 *   +0x000 object points (4 words: x, y/depth, z, flags)   +0x320 far ptr tile geometry (NULL = off map)
 *   +0x324 object count   +0x328/+0x32a cell world x/y     +0x32c/+0x32d ground colours of the 2 triangles
 *   +0x334 height grid (row stride 0x41)
 * Sector sprite bank DSPTR(0x3498) (0x38270 bytes): +0 int32 offsets [10 sprites][16 scales],
 *   +0x280 {u8 half width, u8 height}[10], +0x294 sector file (DSPTR(0x3492)).
 */
#include "../core/mem.h"
#include "../platform/platform.h"
#include "protos.h"

/* 0a86:0000  advance the day/night cycle one step: interpolate sky colours and brightness, rebuild and upload the palette */
void sub_0a86_0000(void)
{
    uint8_t col[0x300];                 /* only the first 15 bytes (5 RGB colours) are used */
    uint8_t *p = DSADDR(0x204f + DS16(0x1fd2) * 16);   /* current phase */
    uint8_t *q = p + 0x10;                              /* next phase */
    uint8_t *d = col;
    uint8_t count = p[0xf];                             /* steps in this phase */
    int16_t i, a, b;

    for (i = 0; i < 0xf; i++) {
        *d++ = (uint8_t)sub_0c9b_00d8(*p++, *q++, DS16(0x1fd4), count);
    }

    if (DS16(0xa0) == -1) {
        /* exterior: 4 x 16-step sky gradient into palette entries 192..255 */
        sub_0c9b_005c(col, DSPTR(0x304c));
    } else {
        int16_t skip = 0;
        if (DS16(0xa0) == 4 || DS16(0xa0) == 8) {
            if (DS16(0xce) != 0x23) skip = 1;
        }
        if (!skip && DS16(0xa0) == 3 &&
            (DS16(0xce) == 0x18 || DS16(0xce) == 0x19 || DS16(0xce) == 0xe)) {
            skip = 1;
        }
        if (!skip) {
            sub_0dc5_03de(DSPTR(0x304c), col[0], col[1], col[2], col[12], col[13], col[14]);
        }
    }

    sub_120e_0002(DSPTR(0x304c), DSPTR(0x3032), 0x300);

    a = DS8(0x20ef + DS16(0x1fd2));
    b = DS8(0x20f0 + DS16(0x1fd2));
    /* 16-bit mul, then cwd/idiv */
    a = (int16_t)((uint16_t)(b - a) * (uint16_t)DS16(0x1fd4)) / (int16_t)count + a;
    if ((int16_t)DS8(0xd2) > a) {
        a = DS8(0xd2);
    }
    sub_0c9b_0022(DSPTR(0x3032), DSPTR(0x3032), (uint16_t)a);
    sub_0791_067f(DSPTR(0x3032));

    DS16(0x1fd4)++;
    if (DS16(0x1fd4) > (int16_t)count) {
        DS16(0x1fd2)++;
        DS16(0x1fd4) = 0;
        if (DS16(0x1fd2) > 8) {
            DS16(0x1fd2) = 0;
        }
    }
}

/* 0a86:01c6  player stands in water: unless on an island-link tile, play the 18-frame drowning animation (1 HP lost per frame) */
void sub_0a86_01c6(void)
{
    uint8_t *anim = NULL;
    int16_t i, tx, ty;

    tx = DS16(0x1fc2) / 0x200;
    ty = DS16(0x1fc4) / 0x200;
    if (tx >= 0x1d && ty == 9 && DS16(0x1fd6) == 0) {
        return;
    }
    if (tx <= 8 && ty == 0xf && DS16(0x1fd6) == 1) {
        return;
    }
    sub_0053_028e();
    anim = sub_0053_0413(DSPTR(0x18a0), 0x36);
    sub_0791_00e5(1);
    for (i = 0; i < 0x12; i++) {
        sub_0791_0638(DSADDR(0x11a), 1);        /* player HP -1 */
        sub_0c9b_0a66(0x65);
        sub_106a_0477((uint16_t)i, 0, 0, anim);
        sub_118f_0107();
        sub_0936_0e2d();
    }
    sub_0f27_0061(anim);
    sub_07fe_0f88();
}

/* 0a86:0298  game clock tick: advance DS32(0x1fdc), random encounters outdoors, periodic day/night step and countdown */
void sub_0a86_0298(void)
{
    if (DS16(0xa0) == -1) {
        DS32(0x1fdc) += 1;
        if (sub_0053_05f6(1000) == 0) {
            sub_0c05_056b();
        }
    } else {
        DS32(0x1fdc) = (int32_t)((uint32_t)(DS32(0x1fdc) + 2) & 0xfffffffeu);
    }
    if ((DS32(0x1fdc) / 2) % 0x40 == 0) {
        sub_0a86_0000();
        if (DS16(0x11c0) == 1) {
            DS16(0x1fe6)--;
            if (DS16(0x1fe6) == 0) {
                DS16(0xa2) = 3;
                DS16(0xd0) = 0x1f;
                DS16(0x329e) = 0x1f;
                DS16(0x12c) = 1;
            }
        }
    }
}

/* 0a86:0325  copy the current island's 6 ground colours into palette entries 186..191 of both palettes */
void sub_0a86_0325(void)
{
    sub_120e_0002(DSADDR(0x20f9 + DS16(0x1fd6) * 0x12), DSPTR(0x3032) + 0x22e, 0x12);
    sub_120e_0002(DSADDR(0x20f9 + DS16(0x1fd6) * 0x12), DSPTR(0x304c) + 0x22e, 0x12);
}

/* 0a86:0370  load the sprite bank of `sector` ("iN.PAK") and build its scale tables and 15 shrunk sprite copies */
void sub_0a86_0370(int16_t sector)
{
    uint8_t *bank = DSPTR(0x3498);
    uint8_t *L = bank + 0x294;
    uint8_t *spr, *p;
    int16_t i, lvl;

    DSPTR_SET(0x3492, L);
    DS8(0x2289) = (uint8_t)(DS8(0x1fd6) + 0x30);            /* "iN" */
    sub_0fa2_0008(DSADDR(0x2288), DS8(0x2153 + DS16(0x1fd6) * 16 + sector), L);

    spr = L + P32(L + 4);                                   /* sprite offset table */
    for (i = 0; i < 10; i++) {
        p = spr + P32(spr + i * 4);
        P32(bank + (i * 16 + 0xf) * 4) = (int32_t)(p - bank);   /* scale 15 = original */
        bank[i * 2 + 0x280] = (uint8_t)((int16_t)(sub_115f_000c((uint16_t)i, spr) & 0xff) / 2);
        bank[i * 2 + 0x281] = (uint8_t)sub_115f_0054((uint16_t)i, spr);
    }

    p = L + PU16(L + 0xc);                                  /* free space for scaled copies */
    for (lvl = 0xe; lvl >= 0; lvl--) {
        for (i = 0; i < 10; i++) {
            P32(bank + (i * 16 + lvl) * 4) = (int32_t)((p + 6) - bank);
            p = sub_1110_01bc((uint16_t)i, (uint16_t)lvl, spr, p);
        }
    }
    sub_0a86_0325();
    sub_0a86_0000();
}

/* 0a86:0517  rebuild the 4 visible terrain cells around the player (by view octant/half-tile) and reload the sector bank if needed */
void sub_0a86_0517(void)
{
    uint8_t *cell = DSPTR(0x34aa);
    uint8_t *geom;
    int16_t bx0, by0, key, base, row, k, idx, wx, wy, fl, tile;

    bx0 = DS16(0x1fc2) & (int16_t)0xfe00;
    by0 = DS16(0x1fc4) & (int16_t)0xfe00;
    key = (int16_t)DSS8(0x1fcb) & 0x1f;
    base = bx0 / 0x200 + by0 / 0x10;                        /* ty*32 + tx */
    row = DSS8(0x202f + key);

    for (k = 0; k < 4; k++) {
        idx = base;
        wx = bx0;
        wy = by0;
        fl = DSS8(0x201f + row * 4 + k);
        if (fl & 1) { idx++;        wx += 0x200; }
        if (fl & 2) { idx--;        wx -= 0x200; }
        if (fl & 4) { idx += 0x20;  wy += 0x200; }
        if (fl & 8) { idx -= 0x20;  wy -= 0x200; }
        /* note: the original reads the map before the range check (idx may be off the map) */
        tile = DSPTR(0x34a2)[(int16_t)(idx * 2)];
        geom = DSPTR(0x34b2) + PU16(DSPTR(0x34ca) + tile * 2);
        if (wx >= 0 && wy >= 0 && wx <= 0x4000 && wy < 0x4000) {
            sub_0c9b_05b2(geom, cell);
            FP_SET(cell + 0x320, geom);
            P16(cell + 0x324) = DS16(0x2284);
            P16(cell + 0x328) = wx;
            P16(cell + 0x32a) = wy;
            sub_0c9b_00fe(DSPTR(0x34a2) + (int16_t)(idx * 2) + 1, cell);
            sub_0c9b_01cd(cell, cell + 0x334);
        } else {
            FP_SET(cell + 0x320, NULL);
        }
        cell += 0x400;
    }

    sub_0c9b_02b6();
    DS16(0x1fda) = DS16(0x1fc2) / 0x1000 + (DS16(0x1fc4) / 0x1000) * 4;
    if (DS16(0x1fda) != DS16(0x1fc0)) {
        sub_0a86_0370(DS16(0x1fda));
        DS16(0x1fc0) = DS16(0x1fda);
    }
}

/* 0a86:06ee  allocate the exterior work buffers (map, tile tables, sprite bank, cells, point buffers, actor list) */
void sub_0a86_06ee(void)
{
    DSPTR_SET(0x34a2, sub_0f27_0004(0x800));
    DSPTR_SET(0x34ca, sub_0f27_0004(0x400));
    DSPTR_SET(0x34b2, sub_0f27_0004(0x1f40));
    DSPTR_SET(0x3498, sub_0f27_0004(0x38270));
    DSPTR_SET(0x34aa, sub_0f27_0004(0x1000));
    DSPTR_SET(0x34a6, sub_0f27_0004(0x400));
    DSPTR_SET(0x34ae, sub_0f27_0004(0x400));
    DSPTR_SET(0x349e, sub_0f27_0004(0x50));
}

/* 0a86:078e  free the exterior work buffers and the RESID#2 resource */
void sub_0a86_078e(void)
{
    sub_0f27_0061(DSPTR(0x349e));
    sub_0f27_0061(DSPTR(0x34ae));
    sub_0f27_0061(DSPTR(0x34a6));
    sub_0f27_0061(DSPTR(0x34aa));
    sub_0f27_0061(DSPTR(0x3498));
    sub_0f27_0061(DSPTR(0x34b2));
    sub_0f27_0061(DSPTR(0x34ca));
    sub_0f27_0061(DSPTR(0x34a2));
    sub_0f27_0061(DSPTR(0x34be));
}

/* 0a86:081f  draw static scenery sprite spr bottom-centred at x,y with scale 0..63, tracking the top-most drawn y */
void sub_0a86_081f(int16_t spr, int16_t x, int16_t y, int16_t scale)
{
    uint8_t *bank;
    int16_t h;

    if (x < -0x46 || x > 0x190) {
        return;
    }
    if (scale < 0) scale = 0;
    if (scale > 0x3f) scale = 0x3f;
    scale /= 4;                                             /* scale level 0..15 */

    bank = DSPTR(0x3498);
    x -= (int16_t)(bank[spr * 2 + 0x280] * (scale + 1)) / 0x10;
    h = (int16_t)(bank[spr * 2 + 0x281] * (scale + 1)) / 0x10;
    if (y - h < DS16(0x349c)) {
        DS16(0x349c) = y - h;
    }
    scale += spr << 4;
    sub_0fe6_01d2((uint16_t)scale, x, y, DSPTR(0x2d38), DSPTR(0x3498));
}

/* 0a86:08d3  draw the current frame of animated scenery object `anim` at x,y (scaled) and advance its frame counter */
void sub_0a86_08d3(int16_t anim, int16_t x, int16_t y, int16_t scale)
{
    uint8_t *L = DSPTR(0x3492);
    uint8_t *p = L + P32(L + 8);                            /* animation table */
    uint8_t nframes;
    int16_t frame;

    p += P32(p + anim * 4);
    frame = p[0];
    p += 1;
    nframes = p[0];
    p += 1;
    sub_106a_04ff(p[frame * 8], x, y, L, scale);
    frame++;
    if (nframes == frame) {
        frame = 0;
    }
    p[-2] = (uint8_t)frame;
}

/* 0a86:09af  transform and draw the objects of a terrain cell (side 0/1 = half of tile, 2 = all), with actors of this layer */
void sub_0a86_09af(uint8_t *cell, int16_t side, int16_t layer)
{
    uint8_t *geom, *pt;
    int16_t cx, cy, i, sx, depth, sy, fl;

    DS16(0x1fc6)++;                                         /* camera height +1 for objects */
    geom = FP(cell + 0x320);
    if (geom != NULL) {
        DS16(0x2284) = P16(cell + 0x324);
        cx = P16(cell + 0x328);
        cy = P16(cell + 0x32a);
        if (DS16(0x2284) != 0) {
            sub_0c9b_04f4(cell, DSPTR(0x34a6), DS16(0x1fc2) - cx, DS16(0x1fc4) - cy, side);
        }
        if (DS16(0x2284) != 0) {
            sub_0c9b_06a4();
        }
        if (DS16(0x2284) != 0) {
            sub_0c9b_06fe();
        }
        if (DS16(0x3496) != 0) {
            sub_0c9b_09ae(layer, side);
        }
        pt = DSPTR(0x34a6);
        for (i = 0; i < DS16(0x2284); i++) {
            sx = P16(pt);
            depth = P16(pt + 2);
            sy = P16(pt + 4);
            fl = P16(pt + 6);
            pt += 8;
            if ((fl & 0x80) && depth < 0x19 && sx > -0x64 && sx < 0x1a4) {
                sub_0c05_0400();                            /* door / trigger check */
            }
            if (fl & 0x80) {
                sub_0a86_08d3(fl & 0x1f, sx, sy, (int16_t)(0x61a8 / (int16_t)(depth + 0x1e)));
            } else if (fl & 0x20) {
                sub_0936_0876(sx, depth, sy, fl & 0xff);
            } else {
                sub_0a86_081f(fl & 0xff, sx, sy, (int16_t)(0xa00 / (int16_t)(depth + 0x1e)));
            }
        }
    }
    DS16(0x1fc6)--;
}

/* 0a86:0b29  draw the flat-shaded polygons of a terrain cell on the requested side of the tile diagonal (2 = all) */
void sub_0a86_0b29(uint8_t *cell, int16_t side)
{
    uint8_t *d = FP(cell + 0x320);
    int16_t nobj, npoly, cx, cy, i, n, color, a, b;

    if (d == NULL) {
        return;
    }
    nobj = d[0];
    d++;
    npoly = d[0];
    d++;
    d += nobj * 4;
    cx = P16(cell + 0x328);
    cy = P16(cell + 0x32a);
    for (i = 0; i < npoly; i++) {
        n = d[0];
        d++;
        color = d[0];
        d++;
        a = d[0];                                           /* first vertex x */
        b = d[1];                                           /* first vertex y */
        if (!((side == 0 && a > b) || (side == 1 && a < b))) {
            sub_0c9b_0744(d);
            sub_0c9b_01cd(DSPTR(0x34a6), cell + 0x334);
            if (DS16(0x2284) != 0) {
                sub_0c9b_04f4(DSPTR(0x34a6), DSPTR(0x34ae), DS16(0x1fc2) - cx, DS16(0x1fc4) - cy, 2);
            }
            if (DS16(0x2284) != 0) {
                sub_0c9b_077c();
            }
            if (DS16(0x2284) != 0) {
                sub_0c9b_06fe();
            }
            if (DS16(0x2284) > 2) {
                sub_0c9b_083f();
                sub_0d84_0002(0, 0, DSPTR(0x34a6), (uint16_t)DS16(0x2284), (uint8_t)color);
            }
        }
        d += n * 2;
    }
}

/* 0a86:0c88  draw one ground triangle of a cell (side 0 or 1) with corner heights from the cell's height grid */
void sub_0a86_0c88(uint8_t *cell, int16_t side)
{
    uint8_t *d = FP(cell + 0x320);
    int16_t cx, cy;
    int16_t color = 0;          /* port: uninitialised in the original if side is not 0/1 */

    if (d == NULL) {
        return;
    }
    cx = P16(cell + 0x328);
    cy = P16(cell + 0x32a);
    DS16(0x2298) = -(int16_t)cell[0x334];                   /* point 1 z */
    DS16(0x22a0) = -(int16_t)cell[0x3b5];                   /* point 2 z */
    if (side == 0) {
        DS16(0x228c) = 0;
        DS16(0x228e) = 0x200;
        DS16(0x2290) = -(int16_t)cell[0x375];
        color = cell[0x32c];
    } else if (side == 1) {
        DS16(0x228c) = 0x200;
        DS16(0x228e) = 0;
        DS16(0x2290) = -(int16_t)cell[0x374];
        color = cell[0x32d];
    }
    DS16(0x2284) = 3;
    sub_0c9b_04f4(DSADDR(0x228c), DSPTR(0x34ae), DS16(0x1fc2) - cx, DS16(0x1fc4) - cy, 2);
    if (DS16(0x2284) != 0) {
        sub_0c9b_077c();
    }
    if (DS16(0x2284) != 0) {
        sub_0c9b_06fe();
    }
    if (DS16(0x2284) > 2) {
        sub_0c9b_083f();
        sub_0d84_0002(0, 0, DSPTR(0x34a6), (uint16_t)DS16(0x2284), (uint8_t)(color + 0xba));
    }
}

/* 0a86:0d96  present the back buffer (rows y0..171) on screen using the blitter for the current window mode */
void sub_0a86_0d96(int16_t y0)
{
    /* port: the original outdoor loop is CPU-bound (fixed movement per frame); pace it
       at 20 frames/s, a typical 386/486 speed, so walking and the day clock match. */
    plat_frame_limit(20);
    if (DS16(0xe6) == 2) {
        if (DS16(0xec) == 0) {
            sub_0c9b_0c40(DSPTR(0x2d38), DSPTR(0x2d3c), y0);
        } else {
            sub_0c9b_0c0b(DSPTR(0x2d38), DSPTR(0x2d3c), y0);
        }
    } else if (DS16(0xe6) == 1) {
        sub_0c9b_0c70(DSPTR(0x2d38), DSPTR(0x2d3c), y0);
    } else if (DS16(0xe6) == 0) {
        sub_0c9b_0ca6(DSPTR(0x2d38), DSPTR(0x2d3c), y0);
    }
}

/* 0a86:0e25  draw the compass at (280,140): dial and needle rotated by the heading */
void sub_0a86_0e25(void)
{
    sub_106a_0477(0, 0x118, 0x8c, DSPTR(0x34c6));
    sub_106a_0593(1, 0x118, 0x8c, DSPTR(0x34c6),
                  (uint16_t)((0x80 - (int16_t)DSS8(0x1fcd)) & 0xff));
}

/* 0a86:0e64  render one exterior frame: sky, ground, the 4 cells back to front (triangles, polygons, objects), compass, countdown */
void sub_0a86_0e64(int16_t draw_actors)
{
    uint8_t *cell;
    int16_t s, t, layer;

    DS16(0x349c) = 100;
    sub_0a86_0298();
    if (DS16(0xec) != 0) {
        sub_0c9b_0a15(DS16(0x34b8));
    } else {
        sub_0c9b_0a66(DS16(0x34b8));
    }
    if (DS16(0x1fd2) >= 4 && DS16(0x1fd2) <= 9) {
        sub_0c9b_0b20();                                    /* night sky */
    }
    DS16(0x3496) = 0;
    if (draw_actors != 0) {
        sub_0936_04af();
    }
    if (DS16(0x3496) != 0) {
        sub_0c9b_08e8();
    }

    if (DS16(0x34b6) != 0) {
        /* all 4 cells are sea: no ground triangles */
        if (DS16(0xec) != 0) {
            sub_0c9b_0b85(DS16(0x34b8), 0xac - DS16(0x34b8), 0xbd);
        } else {
            sub_0c9b_0bba(DS16(0x34b8), 0xac - DS16(0x34b8), 0xbd);
        }
        cell = DSPTR(0x34aa) + 0xc00;
        sub_0a86_0b29(cell, 2);
        sub_0a86_09af(cell, 2, 3);
        cell -= 0x400;
        sub_0a86_0b29(cell, 2);
        sub_0a86_09af(cell, 2, 2);
        cell -= 0x400;
        sub_0a86_0b29(cell, 2);
        sub_0a86_09af(cell, 2, 1);
        cell -= 0x400;
        sub_0a86_0b29(cell, 2);
        sub_0a86_09af(cell, 2, 0);
    } else {
        if (DS16(0xec) != 0) {
            sub_0c9b_0b85(DS16(0x34b8), 0xac - DS16(0x34b8), 0xbd);
        } else {
            sub_0c9b_0bba(DS16(0x34b8), 0xac - DS16(0x34b8), 0xbd);
        }
        /* order of the two tile halves depends on the heading */
        s = 1;
        if (DSS8(0x1fcd) > -0x20 && DSS8(0x1fcd) < 0x60) {
            s = 0;
        }
        t = s ^ 1;
        cell = DSPTR(0x34aa) + 0xc00;
        for (layer = 3; layer >= 1; layer--) {
            sub_0a86_0c88(cell, s);
            sub_0a86_0b29(cell, s);
            sub_0a86_09af(cell, s, layer);
            sub_0a86_0c88(cell, t);
            sub_0a86_0b29(cell, t);
            sub_0a86_09af(cell, t, layer);
            cell -= 0x400;
        }
        /* player's own cell: order by position inside the tile */
        s = 1;
        if ((DS16(0x1fc2) & 0x1ff) > (DS16(0x1fc4) & 0x1ff)) {
            s = 0;
        }
        t = s ^ 1;
        sub_0a86_0c88(cell, s);
        sub_0a86_0b29(cell, s);
        sub_0a86_09af(cell, s, 0);
        sub_0a86_0c88(cell, t);
        sub_0a86_0b29(cell, t);
        sub_0a86_09af(cell, t, 0);
    }

    if (DS16(0x1002) >= 2) {
        sub_0a86_0e25();
    }
    if (DS16(0x11c0) == 1) {
        sub_02a7_07c8(DS16(0x1fe6), 10, 0x14);
    }
}

/* 0a86:11ee  load island map "iN.dat": map, tile offset table and tile geometry; returns tile count (0 if missing) */
int16_t sub_0a86_11ee(void)
{
    int32_t fh, size;
    int16_t n = 0;

    DS8(0x1ffb) = (uint8_t)(DS8(0x1fd6) + 0x30);
    fh = sub_0e66_0031(DSADDR(0x1ffa), 0x3ed);
    size = sub_0f23_000a(DSADDR(0x1ffa));
    if (fh == 0) {
        return 0;
    }
    sub_0e66_0191(fh, (uint8_t *)&n, 2);
    sub_0e66_0191(fh, DSPTR(0x34a2), 0x800);
    sub_0e66_0191(fh, DSPTR(0x34ca), (int32_t)n * 2);
    sub_0e66_0191(fh, DSPTR(0x34b2), size - 0x802 - (int32_t)n * 2);
    /* end of the geometry data (16-bit offset arithmetic in the original) */
    DSPTR_SET(0x34ba, DSPTR(0x34b2) + (uint16_t)((uint16_t)size - 0x802 - (uint16_t)(n * 2)));
    sub_0e66_022b(fh);
    return n;
}

/* 0a86:12e5  load the exterior palette (RESID.PAK entry 3) into both palettes */
void sub_0a86_12e5(void)
{
    uint8_t *pal = sub_0c66_0003(DSPTR(0x189c), 3);

    sub_120e_0002(pal, DSPTR(0x3032), 0x300);
    sub_120e_0002(pal, DSPTR(0x304c), 0x300);
    sub_0f27_0061(pal);
}

/* 0a86:1341  on island 3 with quest state 0xbb0 == 3, remove the objects of tile geometry #20 */
void sub_0a86_1341(void)
{
    uint8_t *g;

    if (DS16(0x1fd6) == 3 && DS16(0xbb0) == 3) {
        g = DSPTR(0x34b2) + PU16(DSPTR(0x34ca) + 0x28);
        g[0] = 0;
    }
}

/* 0a86:136f  U-turn: rotate the heading by 16 eight times, rendering and presenting each frame */
void sub_0a86_136f(void)
{
    int16_t i;

    for (i = 0; i < 8; i++) {
        DS8(0x1fcd) += 0x10;
        sub_0c9b_04a1();
        sub_0a86_0e64(1);
        sub_0a86_0d96(0);
    }
}

/* 0a86:13a1  bump back after a blocked door: sound 10, then 5 frames walking backwards */
void sub_0a86_13a1(void)
{
    int16_t i;

    DS16(0x2286) = 0;
    sub_0791_00e5(10);
    for (i = 0; i < 5; i++) {
        DS8(0x1fca) = 0xff;                                 /* move backwards */
        sub_0c9b_0464();
        sub_0a86_0e64(1);
        sub_0a86_0d96(0);
    }
}

/* 0a86:13e2  (re)enter the exterior view: palettes, sky, RESID#2 sprites, buffers, island map, view state, HUD */
void sub_0a86_13e2(void)
{
    uint8_t *res;

    sub_0a86_12e5();
    sub_0c9b_005c(DSADDR(0x2010), DSPTR(0x3032));
    sub_0a86_0000();
    DS8(0x1fcc) = 0xff;                                     /* force cell rebuild */
    DS16(0x303c) = 0;
    DS16(0x3054) = 0xac;
    DS8(0xd2) = 0;
    sub_0596_02d6();
    res = sub_0c66_0003(DSPTR(0x189c), 2);
    DSPTR_SET(0x34be, res);
    DSPTR_SET(0x3262, FP(res));
    res += 4;
    DSPTR_SET(0x34c6, FP(res));                             /* compass */
    res += 4;
    sub_0a86_06ee();
    sub_0a86_11ee();
    DS16(0x1fc0) = -1;
    sub_0c9b_04cc();
    DS16(0xa2) = -1;
    sub_0053_0925(0x63);
    sub_0a86_1341();
    DS16(0x1fc8) = 8;
    sub_07fe_0f88();
    DS16(0x17e2) = 0;
    DS16(0x12c) = 1;
    DS16(0x11a) = 0xa0;                                     /* player screen x */
    DS16(0x11c) = 0xab;                                     /* player screen y */
}

/* 0a86:14ae  snapshot the back buffer into a new 64000-byte buffer, mapping colours >= 0xc0 to 0x0f in the top 150 rows */
void sub_0a86_14ae(void)
{
    uint8_t *p;
    int32_t i;

    DSPTR_SET(0x34c2, sub_0f27_0004(0xfa00));
    sub_1036_000a(DSPTR(0x2d38), DSPTR(0x34c2));
    p = DSPTR(0x34c2);
    for (i = 0; i < 0xbb80; i++) {
        if (*p >= 0xc0) {
            *p = 0xf;
        }
        p++;
    }
}

/* 0a86:1519  free the exterior snapshot and keep the player position on return */
void sub_0a86_1519(void)
{
    sub_0f27_0061(DSPTR(0x34c2));
    DS16(0x1fe4) = 1;
}

/* 0a86:1530  exterior keys: Esc -> quit prompt, Enter -> U-turn, then the generic key handler */
void sub_0a86_1530(void)
{
    if (DS8(0x2d4e) == 1) {
        sub_07fe_000e();
    }
    if (DS8(0x2d4e) == 0x1c) {
        sub_0a86_136f();
    }
    sub_0053_1cf8();
}

/* 0a86:154d  main game loop (never returns): exterior walk, island switching, running scenes and placing the player on return */
void sub_0a86_154d(void)
{
    int16_t special = 0;        /* [bp-2]; port: uninitialised on the first pass in the original */
    uint8_t *rec;

    sub_0a86_12e5();
    sub_0791_067f(DSPTR(0x3032));
    sub_0596_0ec3();
    sub_0a86_13e2();
    sub_0c9b_0404();
    DS16(0x1fc6) = 0x3a;                                    /* intro: camera descends */
    sub_0791_00e5(0x36);
    sub_0c05_0254(0x4e0, 0x2716, -0x44, 0);
    DS16(0x1fc6) = 0;

    for (;;) {
        plat_yield(); /* port */

        if (DS16(0x1fe8) != 0) {                            /* screen shake */
            DS16(0x1fe8)--;
            if (DS16(0x1fe8) == 0) {
                DS16(0x1fc8) = 8;
            } else {
                DS16(0x1fc8) = sub_0053_05f6(DS16(0x1fea)) + 8;
            }
        }
        sub_0c9b_0404();                                    /* movement from input */
        if (DS16(0x1fd8) != DS16(0x1fd6)) {
            sub_0c05_0511();
        }
        sub_0936_0b5f();
        sub_0936_04f4();
        sub_0a86_0e64(1);
        if (sub_110c_000c(0xa0, 0xa8) == 4) {               /* water under the player */
            sub_0a86_01c6();
        }
        if (DS16(0x2286) != 0) {
            sub_0a86_13a1();
        }
        if (DS16(0x1fd6) == 0 && DS16(0x1fc2) > 0x3fde) {
            sub_03dd_011f();
            sub_0053_0112();
            sub_0a86_078e();
            DS16(0x1fd6) = 1;
            DS16(0x1fc2) = 0xc1c;
            DS16(0x1fc4) = 0x1f00;
            sub_0a86_13e2();
        }
        if (DS16(0x1fd6) == 1 && DS16(0x1fc2) < 0xc1c) {
            sub_03dd_011f();
            sub_0053_0112();
            sub_0a86_078e();
            DS16(0x1fd6) = 0;
            DS16(0x1fc2) = 0x3fde;
            DS16(0x1fc4) = 0x1300;
            sub_0a86_13e2();
        }
        if (DS16(0x327a) != 0) {
            sub_0936_0d18();
        }
        sub_0936_0c14();
        sub_0936_0e2d();
        if (DS16(0x17e2) != 0) {
            sub_0dc5_01c5();
            DS16(0x17e2) = 0;
        }
        sub_0a86_0d96(0);
        if (DS8(0x2d4e) != 0) {
            sub_0a86_1530();
        }
        if (DS16(0x3266) != 0) {
            sub_02a7_127c();
        }
        sub_0791_0604();
        if (DS16(0x11c2) != 0) {
            sub_0936_0e23();
        }
        if (DS16(0xa2) == -1) {
            continue;
        }

        /* leave the exterior: run scene DS16(0xa2) */
        sub_0936_0dc0();
        if (DS16(0xa2) == 0x63) {
            sub_0a86_0e64(0);
        }
        sub_0a86_078e();
        DS16(0x1fe2) = DS16(0xa2);
        if (DS16(0x1fe2) >= 10 && DS16(0x1fe2) < 0x14) {
            DS16(0xa2) = DS8(0x2262 + DS16(0x1fe2));
            DS16(0x329e) = DS8(0x226c + DS16(0x1fe2));
        }
        if (DS16(0xa2) == 0x63) {
            sub_0a86_14ae();
        }
        sub_0053_1f74(DS16(0xa2));
        if (DS16(0x1fe2) == 0x63) {
            sub_0a86_1519();
        }
        if (DS16(0x1fe4) == 0 && DS16(0x1fe2) != 8) {
            special = DS16(0x11bc);
            if (special == -1) {
                rec = DSADDR(0x21a4 + DS16(0x1fe2) * 8);    /* door of the scene we came from */
            } else {
                rec = DSADDR(0x2244 + special * 8);         /* special teleport */
            }
            DS16(0x11bc) = -1;
            DS16(0x1fd6) = P16(rec);
            DS16(0x1fc2) = P16(rec + 2);
            DS16(0x1fc4) = P16(rec + 4);
            DS8(0x1fcd) = rec[6];
        }
        DS16(0x1fe4) = 0;
        sub_0a86_13e2();
        if (DS16(0x96) != 0) {
            sub_07fe_035d();
            sub_0a86_1341();
            sub_07fe_0231();
            DS16(0x96) = 0;
        } else {
            sub_0936_0df4();
        }
        if (special == 3) {
            sub_0c05_036f();
        }
        special = 0;
    }
}
