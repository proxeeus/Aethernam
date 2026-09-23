/*
 * Segment 0053 - main(), level/room flow, resource cache, actors, input.
 *
 * Resource cache: 20 slots of 6 bytes at DS:0x928
 *   +0 s8 kind (0 = entry of A??.PAK, 1..7 = preloaded block, see sub_0053_03a5)
 *   +1 s8 idx  +2 far ptr data
 *
 * Actor table: 10 x 0x46 bytes at DS:0x11a ([0] = player)
 *   +00 x  +02 y  +06/+08 world-map pos  +0c room  +0e state (0 stand, 4 walk, 8 action)
 *   +10 anim change (0 none, 1 pending, 2 locked)  +12 dir (1 up, 2 right, 3 down, 4 left)
 *   +14 u8 hostile  +16 fptr sprite set  +1a anim  +1c frame  +1e frame delay  +20 frame count
 *   +22 u8 0xff animating  +23 u8 half width  +24 u8 depth  +25 u8 collision type
 *   +26 u8 collided door/object  +28 u8 sub-state  +2a hit points (0 = dead/inactive)
 *   +2c u8  +30 u16 walk flags  +32/+34 walk target  +38/+3a waypoint
 *   +3c/+3e s16 x range  +40/+41 u8 y range
 */
#include "../core/mem.h"
#include "../platform/platform.h"
#include "protos.h"

#define ACTOR(i)  (DSADDR(0x11a) + (i) * 0x46)
#define SLOT(i)   (DSADDR(0x928) + (i) * 6)

/* 0053:0000 - purges the resource cache when less than 20000 bytes are free */
void sub_0053_0000(void)
{
    if ((uint32_t)sub_0791_05f7() < 20000u)
        sub_0053_028e();
}

/* 0053:0016 - swaps the two int16 values *a and *b */
void sub_0053_0016(uint8_t *a, uint8_t *b)
{
    int16_t t = P16(a);

    P16(a) = P16(b);
    P16(b) = t;
}

/* 0053:003c - returns the cache slot holding resource (kind, idx), or -1 */
int16_t sub_0053_003c(int16_t kind, int16_t idx)
{
    uint8_t *slot = SLOT(0);
    int16_t i;

    for (i = 0; i < 20; i++, slot += 6) {
        if (PS8(slot) == kind && PS8(slot + 1) == idx)
            return i;
    }
    return -1;
}

/* 0053:007e - 1 if resource res is the sprite set of a living actor of the current room
 * (on the world map: also within 800 units of the camera), else 0 */
int16_t sub_0053_007e(uint8_t *res)
{
    uint8_t *a = ACTOR(0);
    int16_t i, dx, dy;

    for (i = 0; i < 10; i++, a += 0x46) {
        if (FP(a + 0x16) == res && P16(a + 0x2a) != 0 && P16(a + 0xc) == DS16(0xce)) {
            if (DS16(0xa0) != -1)
                return 1;
            dx = sub_0053_0cdd((int16_t)(P16(a + 6) - DS16(0x1fc2)));
            dy = sub_0053_0cdd((int16_t)(P16(a + 8) - DS16(0x1fc4)));
            if (dx < 0x320 && dy < 0x320)
                return 1;
        }
    }
    return 0;
}

/* 0053:0112 - frees every loaded kind-0 (A??.PAK) cache entry, whether used or not */
void sub_0053_0112(void)
{
    uint8_t *slot = SLOT(0);
    int16_t i;

    for (i = 0; i < 20; i++, slot += 6) {
        if (FP(slot + 2) != NULL && P8(slot) == 0) {
            sub_0f27_0061(FP(slot + 2));
            FP_SET(slot + 2, NULL);
        }
    }
}

/* 0053:0168 - clears the sprite pointer of actors 1..9 that use resource res */
void sub_0053_0168(uint8_t *res)
{
    uint8_t *a = ACTOR(1);
    int16_t i;

    for (i = 1; i < 10; i++, a += 0x46) {
        if (FP(a + 0x16) == res)
            FP_SET(a + 0x16, NULL);
    }
}

/* 0053:01e8 - frees the actor's sprite set from the cache if it is a kind-0 entry nobody else uses */
void sub_0053_01e8(uint8_t *actor)
{
    uint8_t *res = FP(actor + 0x16);
    uint8_t *slot;
    int16_t i;

    if (res == NULL)
        return;
    slot = SLOT(0);
    for (i = 0; i < 20; i++, slot += 6) {
        if (FP(slot + 2) == res)
            break;
    }
    if (i == 20 || P8(slot) != 0)
        return;
    if (sub_0053_007e(FP(slot + 2)) != 0)
        return;
    sub_0053_0168(FP(slot + 2));
    sub_0f27_0061(FP(slot + 2));
    FP_SET(slot + 2, NULL);
}

/* 0053:028e - frees all kind-0 cache entries not in use by a living actor of the room */
void sub_0053_028e(void)
{
    uint8_t *slot = SLOT(0);
    int16_t i;

    for (i = 0; i < 20; i++, slot += 6) {
        if (FP(slot + 2) != NULL && P8(slot) == 0 && sub_0053_007e(FP(slot + 2)) == 0) {
            sub_0053_0168(FP(slot + 2));
            sub_0f27_0061(FP(slot + 2));
            FP_SET(slot + 2, NULL);
        }
    }
}

/* 0053:030c - fatal error: "NO SLOT FREE !!" (0) / "NO MEMORY FREE !!" (1) at 100,100, wait key, exit */
void sub_0053_030c(int16_t code)
{
    sub_0596_02a0(0x9f);
    if (code == 0)
        sub_1172_0012(0x64, 0x64, DSPTR(0x2d3c), DSADDR(0x1906));
    if (code == 1)
        sub_1172_0012(0x64, 0x64, DSPTR(0x2d3c), DSADDR(0x1916));
    while (DS8(0x2d4e) != 0)
        plat_yield();
    while (DS8(0x2d4e) == 0)
        plat_yield();
    sub_0791_0665();
}

/* 0053:036c - returns the first free (NULL) cache slot, or -1 */
int16_t sub_0053_036c(void)
{
    uint8_t *slot = SLOT(0);
    int16_t i;

    for (i = 0; i < 20; i++, slot += 6) {
        if (FP(slot + 2) == NULL)
            return i;
    }
    return -1;
}

/* 0053:03a5 - returns the preloaded block for resource kind 1,2,3,6,7 (else NULL) */
uint8_t *sub_0053_03a5(int16_t kind)
{
    uint8_t *p = NULL;

    if (kind == 1)
        p = DSPTR(0x3256);
    if (kind == 2)
        p = DSPTR(0x3286);
    if (kind == 3)
        p = DSPTR(0x17b4);
    if (kind == 6)
        p = DSPTR(0x3262);
    if (kind == 7)
        p = DSPTR(0x3062);
    return p;
}

/* 0053:0413 - loads entry idx of a PAK file; on failure purges the cache / reloads and retries,
 * fatal "NO MEMORY FREE" after the third failure */
uint8_t *sub_0053_0413(uint8_t *pakname, int16_t idx)
{
    uint8_t *p = sub_0c66_0003(pakname, idx);

    if (p == NULL) {
        sub_0053_028e();
        p = sub_0c66_0003(pakname, idx);
        if (p == NULL) {
            sub_07fe_02df();
            p = sub_0c66_0003(pakname, idx);
            if (p == NULL)
                sub_0053_030c(1);
        }
    }
    return p;
}

/* 0053:0483 - returns cached resource (kind, idx), loading A??.PAK entry idx for kind 0 */
uint8_t *sub_0053_0483(int16_t kind, int16_t idx)
{
    int16_t s;
    uint8_t *slot;

    sub_0053_0000();
    s = sub_0053_003c(kind, idx);
    if (s == -1) {
        s = sub_0053_036c();
        if (s == -1) {
            sub_0053_028e();
            s = sub_0053_036c();   /* may still be -1: original then writes at DS:0x922 */
        }
    }
    slot = DSADDR(0x928) + s * 6;
    P8(slot) = (uint8_t)kind;
    P8(slot + 1) = (uint8_t)idx;
    if (kind != 0) {
        FP_SET(slot + 2, sub_0053_03a5(kind));
    } else if (FP(slot + 2) == NULL) {
        FP_SET(slot + 2, sub_0053_0413(DSPTR(0x18a0), idx));
    }
    return FP(slot + 2);
}

/* 0053:052f - directory lookup: base + ((int32 *)base)[idx] */
uint8_t *sub_0053_052f(int16_t idx, uint8_t *base)
{
    return base + P32(base + (int32_t)idx * 4);
}

/* 0053:0564 - two-level directory lookup: b = base + dir[i]; returns b + ((int32 *)b)[j] */
uint8_t *sub_0053_0564(int16_t i, int16_t j, uint8_t *base)
{
    base += P32(base + (int32_t)i * 4);
    base += P32(base + (int32_t)j * 4);
    return base;
}

/* 0053:05cd - pause on 'P': waits for release, then for any key / input / button */
void sub_0053_05cd(void)
{
    if (DS8(0x2d4e) == 0x19) {
        while (DS8(0x2d4e) == 0x19)
            plat_yield();
        DS8(0x2d4a) = 0;
        while (DS8(0x2d4e) == 0 && DS8(0x2d4a) == 0 && DS16(0x2d54) == 0)
            plat_yield();
    }
}

/* 0053:05f6 - random number 0..n-1 (0 if n < 2) */
int16_t sub_0053_05f6(int16_t n)
{
    if (n < 2)
        return 0;
    return (int16_t)(sub_1215_0017() % n);
}

/* 0053:0612 - builds the room's obstacle rectangles (u8 x1,y1,x2,y2, x halved) at DS:0x1286,
 * count in DS8(0x1284), from the room object list of the level data */
void sub_0053_0612(void)
{
    uint8_t *q, *out, *img;
    uint16_t n;
    uint8_t cnt, i, type, hw;
    int16_t id, xc, yb;

    q = sub_0053_0564(0, DS16(0xce) * 2 + 1, DSPTR(0x3286));
    n = P8(q + 1);
    cnt = P8(q + 1);
    out = DSADDR(0x1286);
    q += 2;
    for (i = 0; i < cnt; i++) {
        type = P8(q);
        q += 2;
        id = P16(q);
        q += 2;
        xc = P16(q) / 2;
        q += 2;
        yb = P16(q);
        img = sub_0053_0564(type, id & 0x7fff, DSPTR(0x3286)) - 2;
        hw = P8(img) / 2;
        img += 1;
        if (hw != 0) {
            if (type == 0)
                *out++ = (uint8_t)(xc - hw);
            else
                *out++ = (uint8_t)xc;
            *out++ = (uint8_t)((uint8_t)yb - (uint8_t)((((P8(img) >> 4) & 3) + 1) << 2));
            *out++ = (uint8_t)(xc + hw);
            *out++ = (uint8_t)yb;
        } else {
            n--;
        }
        q += 2;
    }
    DS8(0x1284) = (uint8_t)n;
}

/* 0053:075b - inserts (key = y, id) into the y-sorted depth list DSPTR(0x3270) */
void sub_0053_075b(uint8_t key, uint8_t id)
{
    uint8_t *list, *p;
    int16_t cnt, i;

    if ((int16_t)key < DS16(0x1778))
        DS16(0x1778) = key;
    list = DSPTR(0x3270);
    p = list + 2;
    cnt = P8(list);
    for (i = 0; i < cnt; i++, p += 2) {
        if (P8(p) > key)
            break;
    }
    sub_120e_0002(p, p + 2, (uint16_t)((cnt - i) * 2));
    P8(p) = key;
    p++;
    P8(p) = id;
    P8(DSPTR(0x3270))++;
}

/* 0053:07ea - resets the depth list with the room's static objects (key = object y, id 0x10) */
void sub_0053_07ea(void)
{
    uint8_t *q, *out;
    int16_t n, i;

    q = sub_0053_0564(0, DS16(0xce) * 2 + 1, DSPTR(0x3286));
    n = P8(q + 1);
    out = DSPTR(0x3270) + 2;
    q += 8;                     /* low byte of the first object's y */
    for (i = 0; i < n; i++) {
        *out++ = P8(q);
        *out++ = 0x10;
        q += 8;
    }
    P8(DSPTR(0x3270)) = (uint8_t)n;
    DS16(0x1778) = 200;
}

/* 0053:086b - clears the game flag arrays DS:0x11bc[100] and DS:0xfbc[256], word 0x11bc = -1 */
void sub_0053_086b(void)
{
    sub_1209_000d(DSADDR(0x11bc), 0x64, 0);
    sub_1209_000d(DSADDR(0xfbc), 0x100, 0);
    DS16(0x11bc) = -1;
}

/* 0053:0895 - decrypts a text block in place: buf[i] -= (uint8)(0x5f * (i + 1)) */
void sub_0053_0895(uint8_t *buf, int32_t len)
{
    uint8_t k = 0x5f;
    uint32_t i;

    for (i = 0; i < (uint32_t)len; i++)
        *buf++ -= (uint8_t)((uint32_t)k * (i + 1));
}

/* 0053:08f0 - loads text block idx of T.CC4 into dst and decrypts it */
void sub_0053_08f0(uint8_t *dst, int16_t idx, int32_t size)
{
    sub_03dd_0008(DSPTR(0x18ac), idx, dst, size);
    sub_0053_0895(dst, size);
}

/* 0053:0925 - writes level number n as two digits into A00.PAK, D00.PAK and R00.CC4 */
void sub_0053_0925(int16_t n)
{
    int16_t hi = n / 10 + '0';
    int16_t lo = n % 10 + '0';
    uint8_t *p;

    p = DSPTR(0x18a0);
    p[1] = (uint8_t)hi;
    p[2] = (uint8_t)lo;
    p = DSPTR(0x18a4);
    p[1] = (uint8_t)hi;
    p[2] = (uint8_t)lo;
    p = DSPTR(0x18a8);
    p[1] = (uint8_t)hi;
    p[2] = (uint8_t)lo;
}

/* 0053:097e - frees the level data DSPTR(0x3286) (pointer not cleared) and the backdrop buffer */
void sub_0053_097e(void)
{
    sub_0f27_0061(DSPTR(0x3286));
    sub_0596_0033();
}

/* 0053:0994 - returns the palette sub-block (entry 4) of a level data block */
uint8_t *sub_0053_0994(uint8_t *base)
{
    return sub_0053_052f(4, base);
}

/* 0053:09a7 - loads level: file names, view clip, chapter text, D??.PAK data and palette */
void sub_0053_09a7(int16_t level)
{
    int16_t t;

    DS16(0xa2) = level;
    DS16(0xa0) = level;
    sub_0053_0925(level);
    DS16(0x303c) = 0;
    DS16(0x3054) = 0xac;
    sub_0596_02d6();
    if (level < 0xd)
        t = level;
    else
        t = 0xd;
    sub_0053_08f0(DSPTR(0x325a), t, 0x7a44);
    DSPTR_SET(0x3286, sub_0c66_0003(DSPTR(0x18a4), 0));
    sub_120e_0002(sub_0053_0994(DSPTR(0x3286)), DSPTR(0x3032), 0x300);
    if (DS16(0xa0) == 0)
        DS8(0xd3) = 0xff;
    sub_120e_0002(DSPTR(0x3032), DSPTR(0x304c), 0x300);
    sub_0596_17cd();
    sub_0791_067f(DSPTR(0x3032));
}

/* 0053:0a6d - level-session init: buffers, RESID.PAK #1 (player sprites), state reset, player actor */
void sub_0053_0a6d(void)
{
    uint8_t *p;

    DSPTR_SET(0x3056, sub_0f27_0004(0xfa00));
    DSPTR_SET(0x3270, sub_0f27_0004(0x3e8));
    DSPTR_SET(0x177e, sub_0f27_0004(0x898));
    DSPTR_SET(0x325a, sub_0f27_0004(0x7a44));
    p = sub_0c66_0003(DSPTR(0x189c), 1);
    DSPTR_SET(0x3282, p);
    DSPTR_SET(0x3256, FP(p));
    p += 4;
    DSPTR_SET(0x3262, FP(p));
    p += 4;
    DSPTR_SET(0x301c, FP(p));
    DS16(0xd0) = DS16(0x329e);
    DS16(0xce) = -1;
    DS16(0x329e) = 0;
    DS16(0xac) = 0xd;
    DS16(0x3266) = 0;
    DS16(0x328c) = 0;
    DS16(0x1740) = 0;
    DS16(0x329a) = 0;
    DS16(0x3268) = 0;
    DS16(0x3240) = 0;
    sub_0053_0cc4();
    sub_03dd_014c();
    sub_03dd_1640();
    sub_03dd_011f();
    sub_0053_0d68(0, 0, sub_0053_0483(1, 0));
    sub_0053_1c18(0, 0xa0, 0xab);
}

/* 0053:0b7f - farfree(p) (the original then clears only its own argument copy) */
void sub_0053_0b7f(uint8_t *p)
{
    sub_0f27_0061(p);
    p = NULL;
    (void)p;
}

/* 0053:0b9b - frees the level-session buffers (the DS pointers are left dangling) */
void sub_0053_0b9b(void)
{
    sub_0053_0b7f(DSPTR(0x3056));
    sub_0053_0b7f(DSPTR(0x177e));
    sub_0053_0b7f(DSPTR(0x3270));
    sub_0053_0b7f(DSPTR(0x325a));
    sub_0053_0b7f(DSPTR(0x3282));
}

/* 0053:0be7 - game init: RESID.PAK #0 (font...), palettes, UI text, flags, player defaults */
void sub_0053_0be7(void)
{
    uint8_t *p;

    p = sub_0c66_0003(DSPTR(0x189c), 0);
    DSPTR_SET(0x3042, FP(p));
    p += 4;
    DSPTR_SET(0x3066, FP(p));
    p += 4;
    DSPTR_SET(0x3062, FP(p));
    p += 4;
    DSPTR_SET(0x3032, sub_0f27_0004(0x300));
    DSPTR_SET(0x304c, sub_0f27_0004(0x300));
    DSPTR_SET(0x3290, sub_0f27_0004(0x708));
    sub_0053_08f0(DSPTR(0x3290), 0xf, 0x708);
    sub_0053_086b();
    sub_0596_02a0(0x13);
    sub_03dd_1640();
    sub_026c_0006();
    DS16(0x12c) = 1;        /* player dir */
    DS16(0x11a) = 0xa0;     /* player x */
    DS16(0x11c) = 0xab;     /* player y */
    DS16(0x144) = 0x63;     /* player hit points */
    DS16(0xa6) = 0xd2;
    DS16(0xa0) = -1;
}

/* 0053:0cc4 - rasterizes the floor polyline DS:0x1832 into the column table DS:0xa68 */
void sub_0053_0cc4(void)
{
    DS16(0xda) = DS16(0x1832);
    sub_0dc5_04b1(DSADDR(0x1832), DSADDR(0xa68));
}

/* 0053:0cdd - abs(v) (16-bit) */
int16_t sub_0053_0cdd(int16_t v)
{
    if (v < 0)
        return (int16_t)-v;
    return v;
}

/* 0053:0cf2 - starts animation anim on actor (frame 0, frame count from its sprite set) */
void sub_0053_0cf2(uint8_t *actor, int16_t anim)
{
    uint8_t *s;

    if (FP(actor + 0x16) != NULL) {
        s = sub_0596_0b11(anim, FP(actor + 0x16));
        P16(actor + 0x20) = P8(s + 1);
    } else {
        P16(actor + 0x20) = 0;
    }
    P16(actor + 0x1c) = 0;
    P16(actor + 0x1a) = anim;
    P8(actor + 0x22) = 0xff;
    if (actor == DSADDR(0x11a))
        DS16(0x326a) = anim;
}

/* 0053:0d68 - initializes actor idx in room with sprite set (standing, facing up, default bounds) */
void sub_0053_0d68(int16_t idx, int16_t room, uint8_t *sprites)
{
    uint8_t *a = ACTOR(idx);

    P16(a + 0x10) = 0;
    P16(a + 0xe) = 4;
    P16(a + 0x12) = 0;
    sub_0053_0e52(a, 0);
    sub_0053_0e27(a, 1);
    FP_SET(a + 0x16, sprites);
    P8(a + 0x23) = 4;
    P8(a + 0x24) = 2;
    P8(a + 0x26) = 0;
    P8(a + 0x25) = 0;
    P8(a + 0x28) = 0;
    P16(a + 0x30) = 0;
    P8(a + 0x14) = 0;
    P16(a + 0xc) = room;
    if (idx != 0)
        P16(a + 0x2a) = 0xa;
    P16(a + 0x3c) = 0;
    P16(a + 0x3e) = 0x13f;
    P8(a + 0x40) = 0;
    P8(a + 0x41) = 0xab;
    P8(a + 0x22) = 0xff;
    if (idx == 0)
        P8(a + 0x2c) = 0x19;
}

/* 0053:0e27 - sets actor direction if different, nonzero and anim not locked */
void sub_0053_0e27(uint8_t *actor, int16_t dir)
{
    if (P16(actor + 0x12) != dir && dir != 0 && P16(actor + 0x10) != 2) {
        P16(actor + 0x12) = dir;
        P16(actor + 0x10) = 1;
    }
}

/* 0053:0e52 - sets actor anim state if different and anim not locked */
void sub_0053_0e52(uint8_t *actor, int16_t state)
{
    if (P16(actor + 0xe) != state && P16(actor + 0x10) != 2) {
        P16(actor + 0xe) = state;
        P16(actor + 0x10) = 1;
    }
}

/* 0053:0e77 - direction (1 up, 2 right, 3 down, 4 left) from (x1,y1) towards (x2,y2) */
int16_t sub_0053_0e77(int16_t x1, int16_t y1, int16_t x2, int16_t y2)
{
    int16_t dx = x2 - x1;
    int16_t dy = y2 - y1;
    int16_t ax, ay;

    ax = sub_0053_0cdd(dx) >> 1;
    ay = sub_0053_0cdd(dy);
    if (ax > ay)
        return dx > 0 ? 2 : 4;
    return dy > 0 ? 3 : 1;
}

/* 0053:0ed4 - turns actor towards its walk target along the axis chosen by walk flags & 3 */
void sub_0053_0ed4(uint8_t *actor)
{
    int16_t mode = P16(actor + 0x30) & 3;
    int16_t ddx = P16(actor + 0x32) - P16(actor);
    int16_t ddy = P16(actor + 0x34) - P16(actor + 2);
    int16_t dir = P16(actor + 0x12);
    int16_t adx = sub_0053_0cdd(ddx);   /* pure; the original computes these inside the tests */
    int16_t ady = sub_0053_0cdd(ddy);

    if (mode == 1 || (mode == 0 && adx > ady)) {
        if (ddx > 0)
            dir = 2;
        if (ddx < 0)
            dir = 4;
    }
    if (mode == 2 || (mode == 0 && ady > adx)) {
        if (ddy > 0)
            dir = 3;
        if (ddy < 0)
            dir = 1;
    }
    sub_0053_0e27(actor, dir);
}

/* 0053:0f89 - stops actor: clears walk flags and sets the stand state */
void sub_0053_0f89(uint8_t *actor)
{
    P16(actor + 0x30) = 0;
    sub_0053_0e52(actor, 0);
}

/* 0053:0fa3 - player control from input: turn (table 0x17c8), walk, action anim */
void sub_0053_0fa3(void)
{
    uint8_t *pl = DSADDR(0x11a);
    int16_t st = P16(pl + 0xe);

    if (P16(pl + 0x30) & 3)
        return;
    if (DS16(0x3240) != 0 && P16(pl + 0xe) != 0) {
        sub_0053_0f89(DSADDR(0x11a));
        return;
    }
    P16(pl + 0x32) = P16(pl);
    P16(pl + 0x34) = P16(pl + 2);
    if (st == 4)
        st = 0;
    if (P16(pl + 0x12) == DS16(0x3250) && (DSU16(0xc0) & DSU16(0x327c)) == 0)
        st = 4;
    sub_0053_0e27(pl, DS8((uint16_t)(0x17c8 + P16(pl + 0x12) * 5 + DS16(0x3250))));
    if (DS16(0x3030) != 0 && sub_0053_1cb8() != 0 && DS16(0xae) == 1)
        st = 8;
    sub_0053_0e52(pl, st);
}

/* 0053:1074 - advances actor animation: applies pending anim change, steps frames, ends action anim */
void sub_0053_1074(uint8_t *actor)
{
    int16_t idx, anim, w, type, doinc;
    uint8_t *s;

    if (P16(actor + 0x10) == 1) {
        P16(actor + 0x10) = 0;
        idx = P16(actor + 0xe) + P16(actor + 0x12);
        if (FP(actor + 0x16) == DSPTR(0x3256))
            anim = DS16((uint16_t)(0x1744 + idx * 2));
        else
            anim = DS16((uint16_t)(0x175e + idx * 2));
        sub_0053_0cf2(actor, anim);
    } else if (P8(actor + 0x22) == 0xff) {
        s = sub_0596_0b11(P16(actor + 0x1a), FP(actor + 0x16)) + 2;
        w = P16(s + (P16(actor + 0x1c) * 4 + 1) * 2);
        type = w >> 14;
        w &= 0x3fff;
        doinc = 1;
        if (type == 1) {
            if (w < 1)
                w = 1;
            if (P16(actor + 0x1e) < w - 1)
                doinc = 0;
            else
                P16(actor + 0x1e) = 0;
        }
        if (doinc)
            P16(actor + 0x1c)++;
        if (P16(actor + 0x1c) >= P16(actor + 0x20)) {
            P16(actor + 0x1c) = 0;
            if (P16(actor + 0xe) == 8) {
                P16(actor + 0xe) = 0;
                sub_0053_0cf2(actor, DS16((uint16_t)(0x1744 + P16(actor + 0x12) * 2)));
            }
        }
    } else {
        P16(actor + 0x1e) = 0;
    }
}

/* 0053:1192 - animates all living actors of the current room */
void sub_0053_1192(void)
{
    uint8_t *a = ACTOR(0);
    int16_t i;

    for (i = 0; i < 10; i++, a += 0x46) {
        if (P16(a + 0xc) == DS16(0xce) && P16(a + 0x2a) != 0)
            sub_0053_1074(a);
    }
}

/* 0053:11d5 - player entered a door to room: request room change (-1 = leave level);
 * side doors make the player walk off screen first. Returns 1 (0 for room -1). */
int16_t sub_0053_11d5(int16_t room)
{
    uint8_t *pl;
    int16_t dir, x1, y1, x2, y2;

    if (room == -1) {
        DS16(0xa2) = -1;
        return 0;
    }
    DS16(0xd0) = room;
    pl = DSADDR(0x11a);
    dir = P16(pl + 0x12);
    if (dir != 1 && dir != 3) {
        P8(pl + 0x28) = 4;
        sub_0053_13cf(P8(pl + 0x26), (uint8_t *)&x1, (uint8_t *)&y1,
                      (uint8_t *)&x2, (uint8_t *)&y2);
        P8(pl + 0x25) = 8;
        if (dir == 2) {
            P16(pl + 0x3c) = 0;
            P16(pl + 0x3e) = x2;
            sub_0714_023e(0, 0x13f, P16(pl + 2));
        } else {
            P16(pl + 0x3c) = x1;
            P16(pl + 0x3e) = 0x13f;
            sub_0714_023e(0, 0, P16(pl + 2));
        }
        P16(pl + 0x30) &= 0xfffb;
    }
    return 1;
}

/* 0053:1298 - places actor at the door of the new room leading back to its old room,
 * then (no cutscene) prepares the room and plays the scroll transition for side doors */
void sub_0053_1298(uint8_t *actor)
{
    int16_t dir = 1, x1 = 0xa0, x2 = 0xa0, y1 = 0xaa, y2 = 0xaa;
    int16_t i, d;
    uint8_t *door;

    for (i = 0; i < DS16(0xa8); i++) {
        door = DSADDR(0x752) + i * 6;
        if (PS8(door + 1) == P16(actor + 0xc)) {
            dir = DS16((uint16_t)(0x17e4 + PS8(door) * 2));
            if (P16(actor + 0x12) == dir) {
                sub_0053_13cf(i, (uint8_t *)&x1, (uint8_t *)&y1,
                              (uint8_t *)&x2, (uint8_t *)&y2);
                break;
            }
        }
    }
    d = x2 - x1;
    P16(actor) = (d >> 1) + x1;
    d = y2 - y1;
    P16(actor + 2) = (d >> 1) + y1;
    P16(actor + 0x12) = dir;
    if (DS16(0x177c) != 0)
        return;
    if (dir == 1 || dir == 3) {
        DS16(0x17e2) = 1;
    } else {
        sub_1036_000a(DSPTR(0x3056), DSPTR(0x2d38));
        sub_0053_07ea();
        sub_0053_1c4e();
        sub_0596_1252();
        sub_1036_000a(DSPTR(0x2d38), DSPTR(0x3056));
        if (dir == 2)
            sub_0596_09d5();
        if (dir == 4)
            sub_0596_09de();
        sub_0596_169f();
    }
}

/* 0053:13cf - door rectangle: x1,x2 from door table 0x752, y from floor table 0xa68; y1 <= y2 */
void sub_0053_13cf(int16_t door, uint8_t *x1, uint8_t *y1, uint8_t *x2, uint8_t *y2)
{
    uint8_t *e = DSADDR(0x752) + door * 6;

    P16(x1) = P16(e + 2);
    P16(x2) = P16(e + 4);
    P16(y1) = DS8((uint16_t)(0xa68 + P16(x1)));
    if (P16(x1) == P16(x2)) {
        P16(y2) = 0xab;
    } else if (P8(e) == 3) {
        P16(y2) = 0xab;
        P16(y1) = 0xab;
    } else {
        P16(y2) = DS8((uint16_t)(0xa68 + P16(x2)));
    }
    if (P16(y1) > P16(y2))
        sub_0053_0016(y1, y2);
}

/* 0053:1479 - performs the pending room change 0xd0 -> 0xce (deferred until the player
 * has walked off a side edge): obstacles, visit counter, room script, player placement */
void sub_0053_1479(void)
{
    uint8_t *pl = DSADDR(0x11a);
    int16_t k;

    if ((P16(pl + 0x12) == 2 && P16(pl) < 0x13f) ||
        (P16(pl + 0x12) == 4 && P16(pl) > 0)) {
        P16(pl + 2) = P16(pl + 0x34);
        return;
    }
    if (DS16(0xd0) == -1) {
        DS16(0xa2) = -1;
        DS16(0xd0) = 0;
        return;
    }
    sub_0596_067e();
    DS16(0xce) = DS16(0xd0);
    sub_026c_0006();
    sub_0053_0000();
    sub_0053_0612();
    if (DS16(0xa0) < 0xa) {
        k = DS16(0xa0) * 0x32 + DS16(0xce);
        if (DS16((uint16_t)(0xbd4 + k * 2)) < 0x2710)
            DS16((uint16_t)(0xbd4 + k * 2))++;
        DS16(0xaa) = DS16((uint16_t)(0xbd4 + k * 2));
    }
    sub_03dd_01c8();
    sub_0596_169f();
    sub_0053_1298(pl);
    sub_0053_0f89(pl);
    P16(pl + 0xc) = DS16(0xce);
    P8(pl + 0x25) = 0;
    P8(pl + 0x28) = 0;
    P16(pl + 0x3c) = 0;
    P8(pl + 0x40) = 0;
    P16(pl + 0x3e) = 0x13f;
    P8(pl + 0x41) = 0xc7;
}

/* 0053:1568 - performs the pending level change 0xa2 (unless -1) */
void sub_0053_1568(void)
{
    if (DS16(0xa2) == -1)
        return;
    sub_03dd_011f();
    sub_0053_0112();
    sub_0053_097e();
    sub_0053_09a7(DS16(0xa2));
    DS16(0x12c) = 1;        /* player dir */
    sub_0053_1479();
}

/* 0053:1591 - clamps the new position so the actor does not overshoot its walk target */
void sub_0053_1591(uint8_t *actor, uint8_t *px, uint8_t *py)
{
    switch (P16(actor + 0x12)) {
    case 1:
        if (P16(actor + 0x34) > P16(py))
            P16(py) = P16(actor + 0x34);
        break;
    case 2:
        if (P16(actor + 0x32) < P16(px))
            P16(px) = P16(actor + 0x32);
        break;
    case 3:
        if (P16(actor + 0x34) < P16(py))
            P16(py) = P16(actor + 0x34);
        break;
    case 4:
        if (P16(actor + 0x32) > P16(px))
            P16(px) = P16(actor + 0x32);
        break;
    }
}

/* 0053:1678 - adjusts a walk step: vertical speed modifier 0xc4, room drift 0xb4/0xb6 */
void sub_0053_1678(int16_t dir, uint8_t *pdx, uint8_t *pdy)
{
    if (P16(pdy) >= 1) {
        P16(pdy) += DSS8(0xc4);
        if (P16(pdy) < 1)
            P16(pdy) = 1;
    } else if (P16(pdy) <= -1) {
        P16(pdy) -= DSS8(0xc4);
        if (P16(pdy) > -1)
            P16(pdy) = -1;
    }
    if (DS16(0xce) == DS16(0xd0)) {
        if (dir == 1)
            P16(pdx) += DS16(0xb4);
        if (dir == 2)
            P16(pdy) += DS16(0xb6);
        if (dir == 3)
            P16(pdx) -= DS16(0xb4);
        if (dir == 4)
            P16(pdy) -= DS16(0xb6);
    }
}

/* 0053:16fa - moves walking actor idx by one animation step with collision handling;
 * returns 1 if moved, 0 if not walking or blocked */
int16_t sub_0053_16fa(int16_t idx)
{
    uint8_t *a = ACTOR(idx);
    uint8_t *s;
    int16_t nx, ny, dx, dy, r;

    if (P16(a + 0xe) != 4)
        return 0;
    nx = P16(a);
    ny = P16(a + 2);
    s = sub_0596_0b11(P16(a + 0x1a), FP(a + 0x16)) + 2;
    dx = P16(s + (P16(a + 0x1c) * 4 + 2) * 2);
    dy = P16(s + (P16(a + 0x1c) * 4 + 3) * 2);
    sub_0053_1678(P16(a + 0x12), (uint8_t *)&dx, (uint8_t *)&dy);
    nx += dx;
    ny += dy;
    if (P16(a + 0x30) & 3)
        sub_0053_1591(a, (uint8_t *)&nx, (uint8_t *)&ny);
    if (P8(a + 0x25) != 8) {
        r = sub_0714_06c7(idx, nx, ny, P8(a + 0x23), P8(a + 0x24), P16(a + 0x12));
        if (r != 0) {
            r = sub_0714_075b(a, idx, r);
            if (r == 0)
                return 0;
        } else {
            P8(a + 0x25) = 0;
        }
    }
    P16(a) = nx;
    P16(a + 2) = ny;
    return 1;
}

/* 0053:1831 - floor polyline, forward: y of the first point with px > x and py >= y
 * (>= 0xab -> 0xa0) */
int16_t sub_0053_1831(int16_t x, int16_t y)
{
    uint8_t *p = DSADDR(0x1834);
    int16_t px, py = 0;     /* TODO(port): uninitialised in the original if the polyline is empty */
    uint16_t i;

    for (i = 0; i < DSU16(0xda); i++) {
        px = P16(p);
        p += 2;
        py = P16(p);
        p += 2;
        if (px > x && py >= y)
            break;
    }
    if (py >= 0xab)
        py = 0xa0;
    return py;
}

/* 0053:188d - floor polyline, backward: y of the first point with px < x and py >= y
 * (>= 0xab -> 0xa0) */
int16_t sub_0053_188d(int16_t x, int16_t y)
{
    uint8_t *p = DSADDR((uint16_t)(0x1832 + DSU16(0xda) * 4));
    int16_t px, py = 0;     /* TODO(port): uninitialised in the original if the polyline is empty */
    uint16_t i;

    for (i = 0; i < DSU16(0xda); i++) {
        py = P16(p);
        p -= 2;
        px = P16(p);
        p -= 2;
        if (px < x && py >= y)
            break;
    }
    if (py >= 0xab)
        py = 0xa0;
    return py;
}

/* 0053:18f4 - actor blocked by the floor edge: re-targets its walk along the floor contour */
void sub_0053_18f4(int16_t idx, uint8_t *actor)
{
    int16_t x = P16(actor);
    int16_t y = P16(actor + 0x34);

    switch (P16(actor + 0x12)) {
    case 1:
    case 3:
        x = P16(actor + 0x32);
        break;
    case 2:
        if (P16(actor + 0x34) <= P16(actor + 2))
            y = sub_0053_1831(P8(actor + 0x23) + x, P16(actor + 2) - P8(actor + 0x24))
                + P8(actor + 0x24) + 2;
        break;
    case 4:
        if (P16(actor + 0x34) <= P16(actor + 2))
            y = sub_0053_188d(x - P8(actor + 0x23), P16(actor + 2) - P8(actor + 0x24))
                + P8(actor + 0x24) + 1;
        break;
    }
    sub_0714_023e(idx, x, y);
}

/* 0053:19c1 - actor blocked by an obstacle: re-targets its walk randomly around the
 * obstacle bounding box 0x3014..0x301a */
void sub_0053_19c1(int16_t idx, uint8_t *actor)
{
    int16_t x = P16(actor);
    int16_t y = P16(actor + 2);

    switch (P16(actor + 0x12)) {
    case 1:
    case 3:
        if (sub_0053_05f6(2) == 0)
            x = DS16(0x3014) - (P8(actor + 0x23) + 2);
        else
            x = P8(actor + 0x23) + DS16(0x3018) + 2;
        break;
    case 2:
    case 4:
        if (sub_0053_05f6(2) == 0)
            y = DS16(0x3016) - 2;
        else
            y = P8(actor + 0x24) + DS16(0x301a) + 2;
        break;
    }
    sub_0714_023e(idx, x, y);
}

/* 0053:1a64 - reaction of a blocked actor according to its collision type +0x25 */
void sub_0053_1a64(int16_t idx, uint8_t *actor)
{
    switch (P8(actor + 0x25)) {
    case 1:
    case 2:
        sub_0053_18f4(idx, actor);
        return;
    case 6:
        if (P8(actor + 0x28) == 6 && P8(actor + 0x26) == 0) {
            P8(actor + 0x28) = 0;
            sub_0053_0f89(actor);
            if (P8(actor + 0x14) == 1)
                sub_0791_0638(DSADDR(0x11a), P16(actor + 0x2a));
            return;
        }
        break;
    }
    sub_0053_19c1(idx, actor);
}

/* 0053:1adf - auto-walk after a step: detour if blocked, arrival (next waypoint or stop),
 * or switch walking axis when one coordinate is reached */
void sub_0053_1adf(uint8_t *actor, int16_t idx, int16_t moved)
{
    int16_t r;

    if (moved == 0)
        sub_0053_1a64(idx, actor);
    r = sub_0714_0004(P16(actor), P16(actor + 2), P16(actor + 0x32), P16(actor + 0x34));
    if (r == 3 ||
        ((P16(actor + 0x30) & 8) && r == 1) ||
        ((P16(actor + 0x30) & 0x10) && r == 2)) {
        if (P16(actor + 0x30) & 4) {
            sub_0714_023e(idx, P16(actor + 0x38), P16(actor + 0x3a));
            P16(actor + 0x30) &= 0xfffb;
        } else {
            sub_0053_0f89(actor);
        }
    } else if ((P16(actor + 0x30) & 3) == r) {
        P16(actor + 0x30) ^= 3;
        sub_0053_0ed4(actor);
    }
}

/* 0053:1b98 - moves all living actors of the room; flags an enemy presence in 0xae/0xf2 */
void sub_0053_1b98(void)
{
    uint8_t *a = ACTOR(0);
    int16_t i, moved;

    DS16(0xf2) = 0x2710;
    DS16(0xae) = 0;
    for (i = 0; i < 10; i++, a += 0x46) {
        if (P16(a + 0xc) == DS16(0xce) && P16(a + 0x2a) != 0) {
            moved = sub_0053_16fa(i);
            if (P16(a + 0x30) & 3)
                sub_0053_1adf(a, i, moved);
            if (P8(a + 0x14) == 1) {
                DS16(0xf2) = 2;
                DS16(0xae) = 1;
            }
        }
    }
}

/* 0053:1c18 - sets actor idx position and clears its sub-state and walk flags */
void sub_0053_1c18(int16_t idx, int16_t x, int16_t y)
{
    uint8_t *a = ACTOR(idx);

    P16(a) = x;
    P16(a + 2) = y;
    P8(a + 0x28) = 0;
    P16(a + 0x30) = 0;
}

/* 0053:1c4e - inserts all living actors of the room into the depth list (key y, id index) */
void sub_0053_1c4e(void)
{
    uint8_t *a = ACTOR(0);
    int16_t i;

    for (i = 0; i < 10; i++, a += 0x46) {
        if (P16(a + 0xc) == DS16(0xce) && P16(a + 0x2a) != 0)
            sub_0053_075b((uint8_t)P16(a + 2), (uint8_t)i);
    }
}

/* 0053:1c94 - selects the language: 0x10a = lang, "T.CC4"[0] = "TESDI"[lang] */
void sub_0053_1c94(int16_t lang)
{
    uint8_t c;

    DS16(0x10a) = lang;
    c = P8(DSPTR(0x1898) + lang);
    P8(DSPTR(0x18ac)) = c;
}

/* 0053:1cb8 - 1 if the player may act now (no menu, speech, lock, auto-walk, cutscene) */
int16_t sub_0053_1cb8(void)
{
    if (DS16(0x329a) != 0 && DS16(0x328a) > 4)
        DS16(0x328a) = 1;
    if (DS16(0x3240) != 0 || DS16(0x17bc) != 0 || DS16(0xc0) == 0xf ||
        (DS16(0x14a) & 3) != 0 || DS16(0x177c) != 0)
        return 0;
    return 1;
}

/* 0053:1cf8 - keyboard handling: action keys, pause 'P', music off (0x27), Enter
 * (fire = Enter) closes menus/dialogue pages; waits for key release */
void sub_0053_1cf8(void)
{
    uint8_t key = DS8(0x2d4e);
    uint8_t fire = DS8(0x2d4a) & 0x80;

    if (key == 0) {
        if (fire == 0)
            return;
        key = 0x1c;
    }
    sub_02a7_10e6(key);
    if (key == 0x19)
        sub_0053_05cd();
    if (DS8(0x2d4e) == 0x27) {
        DS16(0xe8) = 0;
        DS16(0xea) = 0;
        sub_0791_0005(0);
        while (DS8(0x2d4e) == 0x27)
            plat_yield();
    }
    if (DS16(0xa0) == -1 || key != 0x1c)
        return;
    if (DS16(0x328a) > 1 && (int16_t)(DS16(0x32aa) - 3) > DS16(0x328a))
        DS16(0x328a) = 1;
    if (DS16(0x3240) != 0)
        DS16(0x3240) = 0;
    if (DS16(0xa0) == 3 && DS16(0xce) == 0x17)
        DS16(0x3012) = 1;
    while (DS8(0x2d4e) == 0x1c)
        plat_yield();
}

/* 0053:1daa - reads input bits: 0x327c raw, 0x3250 direction, 0x3030/0x3012 fire */
void sub_0053_1daa(void)
{
    DS16(0x327c) = DS8(0x2d4a);
    DS16(0x3250) = DS16((uint16_t)(0x17ee + (DSU16(0x327c) & 0xf) * 2));
    if ((DSU16(0xc0) & DSU16(0x327c)) != 0 || DS16(0x3240) != 0) {
        DS16(0x3250) = 0;
        DS16(0x3030) = 0;
    } else {
        DS16(0x3030) = DSU16(0x327c) & 0x80;
    }
    DS16(0x3012) = DS16(0x3030);
}

/* 0053:1deb - menu cursor: up/down moves the selection 0x3274 within 0..[0x303a]-1 */
void sub_0053_1deb(void)
{
    int16_t old = DS16(0x3274);
    /* port: the original stepped once per frame while the key was held (its speed came from
       the CPU); step once per press with keyboard-style auto-repeat instead. */
    static uint16_t held;
    static uint32_t next_ms;
    uint16_t dir = DSU16(0x327c) & 3;
    int step = 0;
    if (dir != held) {
        held = dir;
        step = dir != 0;
        next_ms = plat_ms() + 400;
    } else if (dir && plat_ms() >= next_ms) {
        step = 1;
        next_ms = plat_ms() + 140;
    }

    if (step && (dir & 1) && old > 0)
        DS16(0x3274)--;
    if (step && (dir & 2) && (int16_t)(DS16(0x303a) - 1) > DS16(0x3274))
        DS16(0x3274)++;
    if (old != DS16(0x3274))
        DS16(0x17b8) = 0x4f;
}

/* 0053:1e2e - frame limiter: adapts the extra vsync count 0x1736 to the measured
 * frame rate 0x2840 vs target 0xac, then waits that many retraces */
void sub_0053_1e2e(void)
{
    int16_t t = DS16(0x2840);
    int16_t i;

    if (sub_0053_0cdd((int16_t)(t - DS16(0xac))) > 1 && t != DS16(0x3246)) {
        DS16(0x3246) = t;
        if (t < DS16(0xac) && DS16(0x1736) > 0)
            DS16(0x1736)--;
        if (t > DS16(0xac))
            DS16(0x1736)++;
    }
    for (i = 0; i < DS16(0x1736); i++)
        sub_118f_00de();
}

/* 0053:1e8e - one room tick: keys, scripts, player, actors, depth list, render, UI, present */
void sub_0053_1e8e(void)
{
    sub_0053_1cf8();
    sub_03dd_1b6d();
    if (DS16(0x17ac) != 0)
        return;
    DS16(0x327a) = 0;
    sub_0053_0fa3();
    sub_0053_1192();
    sub_026c_038b();
    sub_0053_1b98();
    sub_026c_0237();
    sub_0053_07ea();
    sub_0053_1c4e();
    sub_026c_034a();
    sub_02a7_0e21();
    sub_0596_1252();
    if (DS16(0x11c0) == 1)
        sub_02a7_07c8(DS16(0x1fe6), 0xa, 0x14);
    if (DS16(0x104) != 0)
        sub_02a7_0098();
    sub_0596_0687();
    if (DS16(0x3240) != 0) {
        sub_0053_1deb();
        sub_0596_0767();
    }
    sub_0596_0059();
    sub_0936_0bec();
    sub_0936_0e2d();
    sub_0791_0604();
}

/* 0053:1f21 - one main-loop frame: level/room change, input, tick, action bar, save/load menu */
void sub_0053_1f21(void)
{
    sub_0a86_0298();
    DS16(0x1896)++;
    if (DS16(0xa2) != DS16(0xa0))
        sub_0053_1568();
    if (DS16(0xd0) != DS16(0xce))
        sub_0053_1479();
    sub_0dc5_0117();
    sub_0053_1daa();
    sub_0053_1e8e();
    if (DS16(0x3266) != 0)
        sub_02a7_127c();
    if (DS16(0x17ac) != 0) {
        sub_07fe_0bd5(0);
        DS16(0x17ac) = 0;
    }
}

/* 0053:1f74 - plays level (chapter) level until 0xa2 == -1, then frees it */
void sub_0053_1f74(int16_t level)
{
    DS16(0xa0) = level;
    sub_0053_0a6d();
    if (DS16(0x96) != 0)
        sub_07fe_035d();
    sub_0053_09a7(DS16(0xa0));
    if (DS16(0x96) != 0) {
        sub_03dd_01c8();
        sub_0596_169f();
        sub_07fe_0231();
        DS16(0x96) = 0;
    }
    while (DS16(0xa2) != -1)
        sub_0053_1f21();
    sub_03dd_1640();
    DS16(0x177c) = 0;
    DS16(0x17bc) = 0;
    DS16(0x3240) = 0;
    DS16(0x328a) = 0;
    DS16(0x329a) = 0;
    sub_03dd_011f();
    sub_0053_0112();
    sub_0053_097e();
    sub_0053_0b9b();
    DS16(0xa0) = -1;
    sub_0596_17cd();
    DS16(0xce) = 0;
    DS16(0x126) = 0;        /* player room */
}

/* 0053:2014 - main(): drivers, launcher config, buffers, music, RNG seeding from the clock,
 * game init, intro scene, world loop; never returns (sub_0791_0665 exits) */
void sub_0053_2014(void)
{
    uint8_t *cfg;
    int32_t n;
    int16_t i, sv_32a6, sv_329e;

    if (sub_0c9b_0e15() != 0)
        DS16(0xec) = 1;
    DS16(0x32a6) = 0;
    DS16(0x329e) = 0;
    DS16(0xd6) = 0;
    DS16(0xd6) = 2;
    cfg = sys_launcher_config();        /* far pointer at 0000:025c */
    DS16(0x326e) = P8(cfg + 7);
    sub_0053_1c94(P8(cfg + 8));
    if (DS16(0x326e) == 0)
        P8(DSPTR(0x18b0) + 2) = 0x62;  /* "MUS.PAK" -> "MUb.PAK" */
    sub_11b2_0001();
    sub_0ebb_0301();
    sub_0dc5_0780();
    sub_0e9f_00c1();
    sub_0d7d_000a();                    /* original pushes 0x3c (unused) */
    DS16(0x2d52) = 0;
    DSPTR_SET(0x17c0, sub_0f27_0004(0x5208));
    DSPTR_SET(0x17c4, sub_0f27_0004(0x2968));
    DSPTR_SET(0x3294, sub_0f27_0004(0x1f40));
    sub_0791_0078(DS16(0xd6));
    sub_0791_0048(1);
    n = (int16_t)(DSS8(0x3040) + DSS8(0x305e) + DSS8(0x3048));
    for (i = 0; (uint32_t)(int32_t)i < (uint32_t)n; i++)
        sub_1215_0017();
    sub_0053_0be7();
    if ((uint32_t)sub_0791_05f7() < 0x4f970u)
        sub_0053_030c(1);
    DSPTR_SET(0x2d40, (const void *)sub_07fe_00b2);
    sv_32a6 = DS16(0x32a6);
    sv_329e = DS16(0x329e);
    sub_03dd_1147(0x12);
    sub_0053_1f74(DS16(0xa2));
    DS16(0x32a6) = sv_32a6;
    DS16(0x329e) = sv_329e;
    if (DS16(0x11c8) == 0)
        sub_0791_0665();
    sub_0a86_154d();
    sub_0791_0665();
}
