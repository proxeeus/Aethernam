/*
 * Segment 0936 - outdoor (3D world) monsters, player laser, hit effects.
 *
 * Actors: 10 records of 0x46 bytes at DS 0x11a (actor 0 = player, 1..9 = monsters).
 *   +0x00/+0x02/+0x04 screen x/y/size of last draw, +0x06/+0x08 world x,y, +0x0a altitude,
 *   +0x12 facing 1..4, +0x14 u8 chasing player, +0x16 fptr sprite data, +0x1a anim, +0x1c frame,
 *   +0x1e visible this frame, +0x22 u8 anim state, +0x25 u8 unpickable mask,
 *   +0x27 u8 flags (0x40 flyer, 0x80 alternate draw), +0x2a hp/state (0 free, -1..-3 dying),
 *   +0x2d u8 heading, +0x2e u8 speed shift, +0x32/+0x34 target waypoint, +0x36 target altitude,
 *   +0x42 path start index, +0x44 current path index (word indices into s16 table DS 0x1bbc).
 * Laser / effect state: DS 0x32c2..0x32de.  Sine table (Q14): DS 0x2d8c, 256 words.
 */
#include "../core/mem.h"
#include "../platform/platform.h"
#include "protos.h"

#define ACTOR(i) DSADDR((uint16_t)(0x11a + (i) * 0x46))
#define PATHW(i) DS16((uint16_t)(0x1bbc + (uint16_t)((i) * 2)))

/* 0936:0005 - word index in the path table DS 0x1bbc of the start of path #path_no */
int16_t sub_0936_0005(int16_t path_no)
{
    uint16_t off = 0x1bbc;
    uint16_t cur;
    int16_t i;

    for (i = 0; i < path_no; i++) {
        do {
            cur = off;
            off += 2;
        } while (DS16(cur) != -1);
    }
    return (int16_t)((int32_t)(uint16_t)(off - 0x1bbc) / 2);
}

/* 0936:0053 - advance actor to its next waypoint; (0,0) waypoint = chase the player */
void sub_0936_0053(uint8_t *actor)
{
    int16_t wx, wy;

    P16(actor + 0x44) += 2;
    if (PATHW(P16(actor + 0x44)) == -1)
        P16(actor + 0x44) = P16(actor + 0x42) + 4;
    wx = PATHW(P16(actor + 0x44));
    wy = PATHW(P16(actor + 0x44) + 1);
    if (wx == 0 && wy == 0) {
        wx = DS16(0x1fc2);
        wy = DS16(0x1fc4);
        P8(actor + 0x14) = 1;
    }
    P16(actor + 0x32) = wx;
    P16(actor + 0x34) = wy;
}

/* 0936:00d1 - first free monster slot 1..9 (hp == 0), 0 if none */
int16_t sub_0936_00d1(void)
{
    int16_t i;

    for (i = 1; i < 10; i++) {
        if (P16(ACTOR(i) + 0x2a) == 0)
            return i;
    }
    return 0;
}

/* 0936:0106 - spawn a monster from path #path_no into slot (-1 = first free slot) */
void sub_0936_0106(int16_t path_no, int16_t slot)
{
    uint8_t *a;
    int16_t start;
    uint16_t p;

    if (slot == -1)
        slot = sub_0936_00d1();
    a = ACTOR(slot);
    if (P16(a + 0x2a) != 0)
        return;

    start = sub_0936_0005(path_no);
    p = (uint16_t)(0x1bbc + (uint16_t)(start * 2)) + 2;
    sub_0053_0d68(slot, 0, NULL);
    P16(a + 0x42) = start;
    P16(a + 0x44) = start + 4;
    P8(a + 0x27) = (uint8_t)(DS16(p) >> 8);
    P8(a + 0x2e) = DS8(p) & 0xff;
    p += 2;
    P16(a + 0x2a) = DS16(p);
    p += 2;
    p += 2;
    P16(a + 0x06) = DS16(p);
    p += 2;
    P16(a + 0x08) = DS16(p);
    p += 2;
    P16(a + 0x0a) = 0;
    P16(a + 0x0a) = 0;
    P16(a + 0x02) = 0;
    P16(a + 0x00) = 0;
    sub_0936_0053(a);
    P16(a + 0x36) = 0;
    P8(a + 0x2d) = 0;
    P8(a + 0x23) = 0x10;
    P8(a + 0x24) = 0x10;
    P16(a + 0x0e) = 4;
    sub_0053_0cf2(a, 3);
}

/* 0936:0225 - 8-way heading byte of the vector (x1-x0, y1-y0) */
uint8_t sub_0936_0225(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    int16_t dx = (int16_t)(x1 - x0);
    int16_t dy = (int16_t)(y1 - y0);
    uint8_t r = 0;

    if (dx == 0)
        r = (dy < 0) ? 0x80 : 0x00;
    else if (dy == 0)
        r = (dx < 0) ? 0x40 : 0xc0;
    else if (dx > 0)
        r = (dy < 0) ? 0xa0 : 0xe0;
    else if (dx < 0)
        r = (dy < 0) ? 0x60 : 0x20;
    return r;
}

/* 0936:029e - per-frame monster AI: steer/move to waypoint, flyer altitude, attack player, proximity */
void sub_0936_029e(uint8_t *actor)
{
    uint8_t ang;
    int16_t d, step, adx, ady, dist;
    uint8_t sh;

    ang = sub_0936_0225(P16(actor + 0x06), P16(actor + 0x08), P16(actor + 0x32), P16(actor + 0x34));
    d = (int16_t)((uint16_t)P8(actor + 0x2d) - (uint16_t)ang);
    if (d > 0) {
        if (d < 0x80)
            P8(actor + 0x2d) -= 0x10;
        else
            P8(actor + 0x2d) += 0x10;
    }
    if (d < 0) {
        if (d > -0x80)
            P8(actor + 0x2d) += 0x10;
        else
            P8(actor + 0x2d) -= 0x10;
    }

    if (sub_0053_0cdd((int16_t)(P16(actor + 0x32) - P16(actor + 0x06))) > 10 ||
        sub_0053_0cdd((int16_t)(P16(actor + 0x34) - P16(actor + 0x08))) > 10) {
        sh = P8(actor + 0x2e) & 31;
        P16(actor + 0x06) -= (int16_t)(DS16(0x2d8c + P8(actor + 0x2d) * 2) >> sh);
        sh = P8(actor + 0x2e) & 31;
        P16(actor + 0x08) += (int16_t)(DS16(0x2d8c + (uint8_t)(P8(actor + 0x2d) + 0x40) * 2) >> sh);
    } else {
        P16(actor + 0x06) = P16(actor + 0x32);
        P16(actor + 0x08) = P16(actor + 0x34);
        sub_0936_0053(actor);
    }

    if ((P8(actor + 0x27) & 0x40) && P16(actor + 0x2a) > 0) {
        if (sub_0053_05f6(10) == 0)
            P16(actor + 0x36) = sub_0053_05f6(300) + 10;
    }

    step = (int16_t)((sub_0053_0cdd((int16_t)(P16(actor + 0x0a) - P16(actor + 0x36))) >> 2) + 1);
    if (step > 0x14)
        step = 0x14;
    if (P16(actor + 0x0a) < P16(actor + 0x36))
        P16(actor + 0x0a) += step;
    if (P16(actor + 0x0a) > P16(actor + 0x36))
        P16(actor + 0x0a) -= step;

    adx = sub_0053_0cdd((int16_t)(P16(actor + 0x06) - DS16(0x1fc2)));
    ady = sub_0053_0cdd((int16_t)(P16(actor + 0x08) - DS16(0x1fc4)));
    if (P8(actor + 0x14) == 1) {
        if (adx < 10 && ady < 10) {
            DS16(0x1fe8) = 8;
            DS16(0x1fea) = P16(actor + 0x2a);
            sub_0791_0638(DSADDR(0x11a), P16(actor + 0x2a));
            sub_0791_00e5(0x31);
        }
        if (adx < 1000 && ady < 1000) {
            dist = (int16_t)(adx + ady) >> 5;
            if (dist < DS16(0xf2)) {
                DS16(0xf2) = dist;
                DS16(0xae) = 1;
            }
        }
    }
}

/* 0936:04af - add all active monsters to the 3D object list (id = slot + 0x20) */
void sub_0936_04af(void)
{
    int16_t i;
    uint8_t *a;

    for (i = 1; i < 10; i++) {
        a = ACTOR(i);
        if (P16(a + 0x2a) != 0)
            sub_0c9b_0861(P16(a + 0x06), P16(a + 0x08), i + 0x20);
    }
}

/* 0936:04f4 - per-frame update of all monsters (effect, proximity reset, AI, animation) */
void sub_0936_04f4(void)
{
    int16_t i;
    uint8_t *a;

    if (DS16(0x32de) != 0)
        sub_0936_0c9f();
    DS16(0xf2) = 10000;
    DS16(0xae) = 0;
    for (i = 1; i < 10; i++) {
        a = ACTOR(i);
        if (P16(a + 0x2a) == 0)
            continue;
        if (P16(a + 0x2a) != -1)
            sub_0936_029e(a);
        if (FP(a + 0x16) != NULL)
            sub_0053_1074(a);
        P16(a + 0x1e) = 0;
    }
}

/* 0936:056e - draw monster sprite at sx,sy (scale 0..15, 15 = unscaled) with shadow at ground_y */
void sub_0936_056e(uint8_t *actor, int16_t sx, int16_t sy, int16_t scale, int16_t ground_y, int16_t size)
{
    int16_t d, dir, off;
    uint8_t rel;
    uint8_t *spr, *seq, *img, *bank;
    uint16_t frame, w6, wdt;

    P16(actor + 0x00) = sx;
    P16(actor + 0x02) = sy;
    P16(actor + 0x04) = (int16_t)(scale << 4);

    /* facing relative to the camera heading */
    d = (int16_t)((int16_t)P8(actor + 0x2d) - (int16_t)DSS8(0x1fcd));
    if (d < 0)
        rel = (uint8_t)((uint8_t)d + 0xff);
    else
        rel = (uint8_t)d;
    dir = P16(actor + 0x12);
    if (rel >= 0xe0 || rel < 0x20)
        dir = 1;
    if (rel >= 0x60 && rel < 0xa0)
        dir = 3;
    if (rel >= 0xa0 && rel < 0xe0)
        dir = 4;
    if (rel >= 0x20 && rel < 0x60)
        dir = 2;
    if (P16(actor + 0x12) != dir && P8(actor + 0x22) == 0xff) {
        P16(actor + 0x12) = dir;
        sub_0053_0cf2(actor, DS16((uint16_t)(0x175e + (dir + 4) * 2)));
    }

    if (P16(actor + 0x0a) != 0)
        sub_1040_0006((int16_t)(sx - scale), ground_y, (int16_t)(sx + scale), ground_y, 0);

    if (FP(actor + 0x16) == NULL) {
        sub_0053_028e();
        spr = sub_0053_0483(0, PATHW(P16(actor + 0x42)));
        FP_SET(actor + 0x16, spr);
        if (FP(actor + 0x16) == NULL) {
            P16(actor + 0x2a) = 0;
            return;
        }
        sub_0053_0cf2(actor, P16(actor + 0x1a));
    }

    seq = sub_0596_0b11(P16(actor + 0x1a), FP(actor + 0x16)) + 2;
    frame = PU16(seq + (uint16_t)(P16(actor + 0x1c) << 3));

    if (!(P8(actor + 0x27) & 0x80)) {
        if (scale == 15) {
            sub_106a_0477(frame & 0x7fff, sx, sy, FP(actor + 0x16));
            return;
        }
        img = sub_0053_0564(0, (int16_t)frame, FP(actor + 0x16));
        w6 = PU16(img + 4);
        bank = sub_0053_052f(1, FP(actor + 0x16));
        sub_0fe6_01d2(w6 & 0x1fff, 0x14a, 0xc8, DSPTR(0x2d38), bank);
        sub_1110_01bc(w6 & 0x1fff, (uint16_t)scale, bank, DSPTR(0x3294));
        wdt = sub_115f_000c(w6, bank);
        off = (int16_t)((uint16_t)(wdt * (uint16_t)(scale + 1)) >> 5);
        if (w6 & 0x8000)
            off = (int16_t)(sub_115f_000c(0, DSPTR(0x3294)) - off);
        sub_0fe6_01d2(w6 & 0x8000, (int16_t)(sx - off), sy, DSPTR(0x2d38), DSPTR(0x3294));
    } else {
        sub_1040_0006((int16_t)(sx - (size >> 3)), ground_y, (int16_t)((size >> 3) + sx), ground_y, 0);
        sub_106a_04ff(frame, sx, sy, FP(actor + 0x16), size);
    }
}

/* 0936:0855 - draw current effect frame (DS 0x32da) from effect bank DSPTR(0x3262) */
void sub_0936_0855(int16_t sx, int16_t sy, int16_t size)
{
    sub_106a_04ff(DSU16(0x32da), sx, sy, DSPTR(0x3262), size);
}

/* 0936:0876 - draw projected monster id&0x1f at (sx,sy) for given depth, mark it visible */
void sub_0936_0876(int16_t sx, int16_t depth, int16_t sy, int16_t id)
{
    int16_t slot = id & 0x1f;
    uint8_t *a = ACTOR(slot);
    int16_t sy0 = sy;
    int16_t den = (int16_t)(depth + 0x1e);
    int16_t sz, scale, lift;

    sz = den ? (int16_t)(0x15e0 / den) : 0;     /* port: idiv by 0 would trap in DOS */
    scale = (int16_t)((sz >> 3) + 1);
    if (scale < 0)
        scale = 0;
    if (scale > 15)
        scale = 15;
    lift = (int16_t)((int16_t)(uint16_t)(PU16(a + 0x0a) * (uint16_t)sz) / 0x80);
    if (lift < 0)
        lift = 0;
    sy -= lift;

    if (P16(a + 0x2a) > 0 || (P16(a + 0x2a) < -1 && (P8(a + 0x27) & 0x40)))
        sub_0936_056e(a, sx, sy, scale, sy0, (int16_t)(sz * 2));
    if (slot == DS16(0x32de))
        sub_0936_0855(sx, sy, (int16_t)(sz * 2));
    P16(a + 0x1e) = 1;
}

/* 0936:0948 - mark monsters not visible or too small as unpickable (+0x25 = 8) */
void sub_0936_0948(void)
{
    int16_t i;
    uint8_t *a;

    for (i = 1; i < 10; i++) {
        a = ACTOR(i);
        if (P16(a + 0x1e) != 1 || P16(a + 0x04) <= 0x23)
            P8(a + 0x25) = 8;
    }
}

/* 0936:0985 - clear the unpickable mark of all monsters */
void sub_0936_0985(void)
{
    int16_t i;

    for (i = 1; i < 10; i++)
        P8(ACTOR(i) + 0x25) = 0;
}

/* 0936:09b1 - fire the laser: pick target under crosshair and start the 4-step beams */
void sub_0936_09b1(void)
{
    uint8_t rect[8];
    int16_t t;
    uint8_t *a;

    sub_0936_0948();
    sub_02a7_0d46(rect, 0x20, 0xa0);
    t = sub_0714_03d2(0, rect);
    sub_0936_0985();
    a = ACTOR(t & 0xf);
    if (t != 0) {
        DS16(0x32d2) = P16(a + 0x00);
        DS16(0x32d4) = P16(a + 0x02);
        DS16(0x32d6) = P16(a + 0x04);
    } else {
        DS16(0x32d2) = 0xa0;
        DS16(0x32d4) = 0x78;
        DS16(0x32d6) = 0xf;
    }
    DS16(0x32c2) = 0;
    DS16(0x32c6) = 0x13f;
    DS16(0x32ce) = 0xab;
    DS16(0x32d8) = 4;
    DS16(0x32c4) = (int16_t)(DS16(0x32d2) - DS16(0x32c2)) >> 2;
    DS16(0x32c8) = (int16_t)(DS16(0x32d2) - DS16(0x32c6)) >> 2;
    DS16(0x32d0) = (int16_t)(DS16(0x32d4) - 0xac) >> 2;
    sub_0791_00e5(0x2a);
}

/* 0936:0a73 - laser arrival: hit test around target, damage / kill monster, effect + sound */
void sub_0936_0a73(void)
{
    uint8_t rect[8];
    int16_t t, id, hp;
    uint8_t *a;

    P16(rect + 0) = (int16_t)(DS16(0x32d2) - 8);
    P16(rect + 2) = (int16_t)(DS16(0x32d4) - 8);
    P16(rect + 4) = (int16_t)(DS16(0x32d2) + 8);
    P16(rect + 6) = (int16_t)(DS16(0x32d4) + 8);
    sub_0936_0948();
    t = sub_0714_03d2(0, rect);
    sub_0936_0985();
    if (t == 0)
        return;

    id = t & 0xf;
    a = ACTOR(id);
    P8(a + 0x14) = 1;
    hp = (int16_t)(P16(a + 0x2a) - 1);
    if (hp <= 0) {
        if (P8(a + 0x27) & 0x40) {
            sub_0936_0c5e(id, 2);
            hp = -3;
            P16(a + 0x36) = 0;
            P8(a + 0x22) = 0;
            P16(a + 0x1c) = 0;
        } else {
            sub_0936_0c5e(id, 1);
            hp = -1;
        }
        sub_0791_00e5(0x14);
    } else {
        sub_0936_0c5e(id, 0);
        sub_0791_00e5(0x31);
    }
    P16(a + 0x2a) = hp;
}

/* 0936:0b5f - per-frame laser logic: fire on button, advance beams, resolve on arrival */
void sub_0936_0b5f(void)
{
    if ((DS8(0x2d4a) & 0x80) && DS16(0x32d8) == 0 && DS16(0x32de) == 0)
        sub_0936_09b1();
    if (DS16(0x32d8) != 0) {
        DS16(0x32c2) += DS16(0x32c4);
        DS16(0x32c6) += DS16(0x32c8);
        DS16(0x32ce) += DS16(0x32d0);
        DS16(0x32d8)--;
        if (DS16(0x32d8) == 0)
            sub_0936_0a73();
    }
}

/* 0936:0ba4 - draw a 3-pixel laser beam line (0x4d centre, 0x37 sides) */
void sub_0936_0ba4(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    sub_1040_0006(x0, y0, x1, y1, 0x4d);
    sub_1040_0006((int16_t)(x0 - 1), y0, x1, y1, 0x37);
    sub_1040_0006((int16_t)(x0 + 1), y0, x1, y1, 0x37);
}

/* 0936:0bec - proximity alarm blink counter DS 0xf4 */
void sub_0936_0bec(void)
{
    if (DS16(0xae) == 1) {
        DS16(0xf4)++;
        if (DS16(0xf4) > DS16(0xf2))
            DS16(0xf4) = -2;
    } else if (DS16(0xf4) < 0) {
        DS16(0xf4)++;
    }
}

/* 0936:0c14 - update alarm counter and draw both laser beams while a shot is in flight */
void sub_0936_0c14(void)
{
    sub_0936_0bec();
    if (DS16(0x32d8) != 0) {
        sub_0936_0ba4(DS16(0x32c2), DS16(0x32ce), (int16_t)(DS16(0x32c2) + DS16(0x32c4)),
                      (int16_t)(DS16(0x32ce) + DS16(0x32d0)));
        sub_0936_0ba4(DS16(0x32c6), DS16(0x32ce), (int16_t)(DS16(0x32c6) + DS16(0x32c8)),
                      (int16_t)(DS16(0x32ce) + DS16(0x32d0)));
    }
}

/* 0936:0c5e - start effect animation (0 spark, 1 explosion, 2 crash) on actor id */
void sub_0936_0c5e(int16_t id, int16_t type)
{
    if (type == 0) {
        DS16(0x32da) = 0;
        DS16(0x32dc) = 7;
    }
    if (type == 1) {
        DS16(0x32da) = 8;
        DS16(0x32dc) = 0x17;
    }
    if (type == 2) {
        DS16(0x32da) = 0x18;
        DS16(0x32dc) = 0x1d;
    }
    DS16(0x32de) = id;
}

/* 0936:0c9f - advance effect frame; at its end step the actor's death-state chain */
void sub_0936_0c9f(void)
{
    int16_t id;
    uint8_t *a;

    DS16(0x32da)++;
    if (DS16(0x32da) <= DS16(0x32dc))
        return;
    id = DS16(0x32de);
    DS16(0x32de) = 0;
    a = ACTOR(id);
    if (P16(a + 0x2a) == -1)
        P16(a + 0x2a) = 0;
    if (P16(a + 0x2a) == -2) {
        P16(a + 0x2a) = -1;
        sub_0936_0c5e(id, 1);
    }
    if (P16(a + 0x2a) == -3) {
        P16(a + 0x2a) = -2;
        sub_0936_0c5e(id, 2);
    }
}

/* 0936:0d18 - encounter check: nearest non-chasing monster within 100 triggers scene 0x63 */
void sub_0936_0d18(void)
{
    int16_t best = 0x7d00, bslot = 0, i, dx, dy;
    uint8_t *a;

    DS16(0x327a) = 0;
    for (i = 1; i < 9; i++) {
        a = ACTOR(i);
        if (P16(a + 0x2a) <= 0 || P8(a + 0x14) == 1)
            continue;
        dx = sub_0053_0cdd((int16_t)(DS16(0x1fc2) - P16(a + 0x06)));
        dy = sub_0053_0cdd((int16_t)(DS16(0x1fc4) - P16(a + 0x08)));
        if ((int16_t)(dx + dy) < best) {
            best = (int16_t)(dx + dy);
            bslot = i;
        }
    }
    if (bslot != 0 && best < 100) {
        DS16(0xa2) = 0x63;
        DS16(0x329e) = (int16_t)((DS16(0x1fd6) << 3) + bslot);
    }
}

/* 0936:0dc0 - save outdoor state: monsters 0x160->0x41c, resource cache 0x928->0x9a0 */
void sub_0936_0dc0(void)
{
    sub_0053_028e();
    sub_120e_0002(DSADDR(0x160), DSADDR(0x41c), 0x276);
    sub_120e_0002(DSADDR(0x928), DSADDR(0x9a0), 0x78);
    sub_0053_0112();
}

/* 0936:0df4 - restore outdoor state saved by sub_0936_0dc0 and relink the resource cache */
void sub_0936_0df4(void)
{
    sub_120e_0002(DSADDR(0x41c), DSADDR(0x160), 0x276);
    sub_120e_0002(DSADDR(0x9a0), DSADDR(0x928), 0x78);
    sub_07fe_0231();
}

/* 0936:0e23 - random encounter spawn (sub_0c05_056b) and decrement counter DS 0x11c2 */
void sub_0936_0e23(void)
{
    sub_0c05_056b();
    DS16(0x11c2)--;
}

/* 0936:0e2d - draw/erase proximity alarm lamp and refresh the status bar on player hp change */
void sub_0936_0e2d(void)
{
    uint8_t *save;

    if (DS16(0x177c) != 0)
        return;
    if (DS16(0xf4) == -1)
        sub_02a7_0b4a(0x1a, 0xb4, 0x42, 0xbc);
    if (DS16(0xf4) == -2) {
        sub_0596_02ec();
        save = DSPTR(0x2d38);
        DSPTR_SET(0x2d38, DSPTR(0x2d3c));
        sub_106a_0477(0x2b, 0x1a, 0xbb, DSPTR(0x3062));
        DSPTR_SET(0x2d38, save);
        sub_0596_02d6();
    }
    if (DS16(0x32ca) != DS16(0x144)) {
        DS16(0x32ca) = DS16(0x144);
        sub_02a7_08f3();
    }
}
