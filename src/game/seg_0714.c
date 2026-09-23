/*
 * Segment 0714 - actor walking targets and collision tests.
 *
 * Actors: 10 records of 0x46 bytes at DS 0x11a (actor 0 = player).
 *   +0x00 x, +0x02 y (feet), +0x0c room, +0x23 half width (u8), +0x24 height (u8),
 *   +0x25 last collision kind (u8, 8 = passing an exit), +0x26 last collision index (u8),
 *   +0x2a hit points, +0x30 walk flags (bits 0-1 axis order, 0x4 saved target valid),
 *   +0x32/+0x34 walk target, +0x38/+0x3a saved walk target.
 * A "rect" is 4 x int16 {x1, y1, x2, y2}.  Collision results are (kind << 8) | index:
 *   0x100 horizon/border, 0x200 bottom, 0x300 obstacle, 0x400 exit zone, 0x500 floor object,
 *   0x600 other actor.
 */
#include "../core/mem.h"
#include "../platform/platform.h"
#include "protos.h"

#define ACTOR(i)   DSADDR((uint16_t)(0x11a + (i) * 0x46))
/* per-column floor horizon (minimum walkable y), DS 0xa68[320] */
#define FLOOR_Y(x) DS8((uint16_t)(0xa68 + (x)))

/* 0714:0004 - returns bit0 = (x1 == x2), bit1 = (y1 == y2); 3 means "already at target" */
int16_t sub_0714_0004(int16_t x1, int16_t y1, int16_t x2, int16_t y2)
{
    int16_t r = 0;

    if (x1 == x2)
        r |= 1;
    if (y1 == y2)
        r |= 2;
    return r;
}

/* 0714:002e - clamps a walk target (*px,*py) to the screen and below the floor horizon,
 * unless the actor box at the target touches an exit zone */
void sub_0714_002e(uint8_t *px, uint8_t *py, int16_t halfw, int16_t height)
{
    int16_t box[4];

    if (P16(px) < halfw)
        P16(px) = halfw + 1;
    if ((int16_t)(P16(px) + halfw) > 0x13f)
        P16(px) = 0x13f - halfw - 1;

    box[0] = P16(px) - halfw;
    box[1] = P16(py) - height;
    box[2] = P16(px) + halfw;
    box[3] = P16(py);

    if (sub_0714_05e4((uint8_t *)box, 1) != 0)
        return;
    if (sub_0714_05e4((uint8_t *)box, 2) != 0)
        return;
    if (sub_0714_05e4((uint8_t *)box, 3) != 0)
        return;
    if (sub_0714_05e4((uint8_t *)box, 4) != 0)
        return;

    if ((int16_t)(P16(py) - height) <= FLOOR_Y(P16(px) - halfw))
        P16(py) = FLOOR_Y(P16(px) - halfw) + height + 2;
    if ((int16_t)(P16(py) - height) <= FLOOR_Y(P16(px) + halfw))
        P16(py) = FLOOR_Y(P16(px) + halfw) + height + 2;
    if (P16(py) > 0xab)
        P16(py) = 0xab;
}

/* 0714:0160 - clamps (*px,*py) to the screen and below the floor horizon at both box edges */
void sub_0714_0160(uint8_t *px, uint8_t *py, int16_t halfw, int16_t height)
{
    int16_t x = P16(px);
    int16_t y = P16(py);

    if (x < halfw)
        x = halfw + 1;
    if ((int16_t)(x + halfw) > 0x13f)
        x = 0x13f - halfw - 1;
    if ((int16_t)(y - height) <= FLOOR_Y(x - halfw))
        y = FLOOR_Y(x - halfw) + height + 2;
    if ((int16_t)(y - height) <= FLOOR_Y(x + halfw))
        y = FLOOR_Y(x + halfw) + height + 2;
    if (y > 0xab)
        y = 0xab;

    P16(px) = x;
    P16(py) = y;
}

/* 0714:0210 - while walking with no saved target, saves the current walk target (+0x38/+0x3a) */
void sub_0714_0210(uint8_t *actor)
{
    if ((PU16(actor + 0x30) & 3) != 0 && (PU16(actor + 0x30) & 4) == 0) {
        P16(actor + 0x38) = P16(actor + 0x32);
        P16(actor + 0x3a) = P16(actor + 0x34);
        PU16(actor + 0x30) |= 4;
    }
}

/* 0714:023e - makes actor actor_idx walk to (tx,ty): clamps the target, picks the axis order,
 * sets the walk animation and facing */
void sub_0714_023e(int16_t actor_idx, int16_t tx, int16_t ty)
{
    uint8_t *a = ACTOR(actor_idx);
    int16_t eq;

    if (P8(a + 0x25) != 8)
        sub_0714_002e((uint8_t *)&tx, (uint8_t *)&ty, P8(a + 0x23), P8(a + 0x24));

    eq = sub_0714_0004(P16(a + 0), P16(a + 2), tx, ty);
    if (eq == 3)
        return;

    sub_0714_0210(a);
    P16(a + 0x32) = tx;
    P16(a + 0x34) = ty;

    if (eq == 0) {
        if ((PU16(a + 0x30) & 3) == 0)
            PU16(a + 0x30) |= (uint16_t)(sub_0053_05f6(2) + 1);  /* random: x first or y first */
        else
            PU16(a + 0x30) ^= 3;                                  /* swap axis order */
    } else {
        PU16(a + 0x30) &= 0xfffc;
        PU16(a + 0x30) |= (uint16_t)(eq ^ 3);                     /* walk along the differing axis */
    }

    sub_0053_0e52(a, 4);
    sub_0053_0ed4(a);
}

/* 0714:031f - border/horizon test of a box moving in dir (1 up, 2 right, 4 left):
 * 0x100 blocked, 0x200 below the bottom line (y2 > 0xab), else 0 */
int16_t sub_0714_031f(uint8_t *rect, int16_t dir)
{
    int16_t hit = 0;
    int16_t x1 = P16(rect + 0);
    int16_t y1 = P16(rect + 2);
    int16_t x2 = P16(rect + 4);
    int16_t y2 = P16(rect + 6);
    int16_t floor_l, floor_r;

    if (x1 < 0)
        x1 = 0;
    if (x2 > 0x13f)
        x2 = 0x13f;
    floor_l = FLOOR_Y(x1);
    floor_r = FLOOR_Y(x2);

    switch (dir) {
    case 1:
        if (y1 <= floor_l || y1 <= floor_r)
            hit = 1;
        break;
    case 2:
        if (floor_r >= y1 || x2 == 0x13f)
            hit = 1;
        break;
    case 4:
        if (floor_l >= y1 || x1 == 0)
            hit = 1;
        break;
    }

    if (y2 > 0xab)
        hit = 2;
    return (int16_t)(hit << 8);
}

/* 0714:03d2 - returns 0x600|i for the first other living actor i of the current room whose box
 * overlaps rect, else 0 (box built in DS 0x3014) */
int16_t sub_0714_03d2(int16_t self_idx, uint8_t *rect)
{
    uint8_t *a = DSADDR(0x11a);
    uint16_t i;

    for (i = 0; i < 10; i++, a += 0x46) {
        if ((int16_t)i == self_idx)
            continue;
        if (P16(a + 0x0c) != DS16(0xce))
            continue;
        if (P16(a + 0x2a) == 0)
            continue;
        if (P8(a + 0x25) == 8)
            continue;

        DS16(0x3014) = P16(a + 0) - P8(a + 0x23);
        DS16(0x3016) = P16(a + 2) - P8(a + 0x24);
        DS16(0x3018) = P8(a + 0x23) + P16(a + 0);
        DS16(0x301a) = P16(a + 2);
        if ((uint8_t)sub_0e9b_000c(rect, DSADDR(0x3014)) != 0)
            return (int16_t)(0x600 | i);
    }
    return 0;
}

/* 0714:04a0 - returns 0x300|i for the first room obstacle rectangle (DS 0x1286, count DS8 0x1284,
 * bytes x1/2,y1,x2/2,y2) overlapping rect, else 0 */
int16_t sub_0714_04a0(uint8_t *rect)
{
    int16_t n = DS8(0x1284);
    uint8_t *q = DSADDR(0x1286);
    int16_t i;
    int16_t x1, x2;

    for (i = 0; i < n; i++) {
        x1 = P8(q) << 1;
        DS16(0x3014) = x1;
        q++;
        DS16(0x3016) = P8(q);
        q++;
        x2 = P8(q) << 1;
        DS16(0x3018) = x2;
        q++;
        DS16(0x301a) = P8(q);
        q++;
        if (x1 != x2 && (uint8_t)sub_0e9b_000c(rect, DSADDR(0x3014)) != 0)
            return (int16_t)(0x300 | i);
    }
    return 0;
}

/* 0714:0559 - returns 0x500|i for the first floor object (DS 0xa18, 8 bytes, count DS16 0xf0) lying
 * in the room (DS16(0xfbc+id*2)==0) whose 16x16 box around (x,y) overlaps rect, else 0 */
int16_t sub_0714_0559(uint8_t *rect)
{
    uint8_t *o = DSADDR(0xa18);
    uint16_t i;
    int16_t box[4];

    for (i = 0; i < DSU16(0xf0); i++, o += 8) {
        if (DS16((uint16_t)(0xfbc + PU16(o) * 2)) != 0)
            continue;
        box[0] = P16(o + 4) - 8;
        box[1] = P16(o + 6) - 8;
        box[2] = P16(o + 4) + 8;
        box[3] = P16(o + 6) + 8;
        if ((uint8_t)sub_0e9b_000c(rect, (uint8_t *)box) != 0)
            return (int16_t)(0x500 | i);
    }
    return 0;
}

/* 0714:05e4 - returns 0x400|i for the first exit zone i (DS 0x752, 6 bytes, count DS16 0xa8) of
 * direction dir that the box reaches in that direction, else 0 */
int16_t sub_0714_05e4(uint8_t *rect, int16_t dir)
{
    int16_t hit = 0;
    uint8_t *z = DSADDR(0x752);
    int16_t x1 = P16(rect + 0);
    int16_t y1 = P16(rect + 2);
    int16_t x2 = P16(rect + 4);
    int16_t zx1, zy1, zx2, zy2;
    int16_t i;

    for (i = 0; i < DS16(0xa8); i++, z += 6) {
        if (PS8(z) != dir)
            continue;

        sub_0053_13cf(i, (uint8_t *)&zx1, (uint8_t *)&zy1, (uint8_t *)&zx2, (uint8_t *)&zy2);

        if (dir == 1 || dir == 3) {
            if (x1 >= zx1 && x2 <= zx2)
                hit = 1;
        } else {
            if (dir == 2) {
                if (y1 >= zy1 && y1 <= zy2 && x2 >= zx1)
                    hit = 1;
            }
            if (dir == 4) {
                if (y1 >= zy1 && y1 <= zy2 && x1 <= zx2)
                    hit = 1;
            }
        }

        if (hit != 0)
            return (int16_t)(0x400 | i);
    }
    return 0;
}

/* 0714:06c7 - full collision test of the actor box at (x,y) moving in dir: border/horizon
 * (an exit zone overrides it), then obstacles, then other actors; returns code or 0 */
int16_t sub_0714_06c7(int16_t actor_idx, int16_t x, int16_t y, int16_t halfw, int16_t height, int16_t dir)
{
    int16_t box[4];
    int16_t res;
    int16_t zone;

    box[0] = x - halfw;
    box[1] = y - height;
    box[2] = x + halfw;
    box[3] = y;

    res = sub_0714_031f((uint8_t *)box, dir);
    if (res != 0) {
        zone = sub_0714_05e4((uint8_t *)box, dir);
        if (zone != 0)
            res = zone;
        return res;
    }

    res = sub_0714_04a0((uint8_t *)box);
    if (res != 0)
        return res;
    res = sub_0714_03d2(actor_idx, (uint8_t *)box);
    if (res != 0)
        return res;
    return 0;
}

/* 0714:075b - stores collision code in the actor (+0x25 kind, +0x26 index); the player touching
 * an exit starts a room change, otherwise the actor stops; returns the room-change result */
int16_t sub_0714_075b(uint8_t *actor, int16_t actor_idx, int16_t code)
{
    int16_t res = 0;

    P8(actor + 0x25) = (uint8_t)((uint16_t)code >> 8);
    P8(actor + 0x26) = (uint8_t)(code & 0xff);

    if (actor_idx == 0 && P8(actor + 0x25) == 4)
        res = sub_0053_11d5(DSS8((uint16_t)(0x752 + P8(actor + 0x26) * 6) + 1));

    if (res == 0) {
        sub_0053_0e52(actor, 0);
        sub_0053_1074(actor);
    }
    return res;
}
