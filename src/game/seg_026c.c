/*
 * Segment 026c - projectiles (shots fired by the player).
 *
 * Projectile table: 16 slots of 12 bytes at DS:0x692
 *   +0  u8  used (0xff = free, 0 = in use)
 *   +1  s8  owner character index
 *   +2  s16 age (-1 = flying, 0..8 = impact animation)
 *   +4  s16 rand(128)+0x20 (variant)
 *   +6  s16 x
 *   +8  s16 y
 *   +a  s16 direction (1 up, 2 right, 3 down, 4 left)
 * Character table: 10 x 0x46 bytes at DS:0x11a (0 = player).
 */
#include "../core/mem.h"
#include "../platform/platform.h"
#include "protos.h"

#define PROJ(i)   DSADDR(0x692 + (uint16_t)((i) * 0xc))

/* 026c:0006 - marks all 16 projectile slots free */
void sub_026c_0006(void)
{
    int16_t i;

    for (i = 0; i < 0x10; i++)
        P8(PROJ(i)) = 0xff;
}

/* 026c:0032 - returns the index of the first free projectile slot, or -1 */
int16_t sub_026c_0032(void)
{
    uint8_t *p = DSADDR(0x692);
    int16_t i;

    for (i = 0; i < 0x10; i++) {
        if (P8(p) == 0xff)
            return i;
        p += 0xc;
    }
    return -1;
}

/* 026c:0067 - spawns a projectile fired by character `owner` in its facing direction; sound 0x31 */
void sub_026c_0067(int16_t owner)
{
    int16_t slot;
    uint8_t *p, *c;

    slot = sub_026c_0032();
    if (slot == -1)
        return;
    p = PROJ(slot);
    c = DSADDR(0x11a + (uint16_t)(owner * 0x46));
    P8(p) = 0;
    P8(p + 1) = (uint8_t)owner;
    P16(p + 6) = (int16_t)(sub_0053_05f6(9) + P16(c) - 8);   /* x = owner.x + rand(9) - 8 */
    P16(p + 8) = P16(c + 2);                                  /* y */
    P16(p + 0xa) = P16(c + 0x12);                             /* dir = owner facing */
    P16(p + 2) = -1;                                          /* flying */
    P16(p + 4) = (int16_t)(sub_0053_05f6(0x80) + 0x20);
    sub_0791_00e5(0x31);
}

/* 026c:010d - projectile hit: knocks character back 32px in dir if free, damages it, particle burst + sound 0x32 */
void sub_026c_010d(int16_t target, int16_t dir, int16_t damage)
{
    uint8_t *c;
    int16_t x, y;
    int16_t rect[4];

    c = sub_03dd_00ac(target & 0xf);
    if (P8(c + 0x14) == 0)
        return;

    x = P16(c);
    y = P16(c + 2);
    if (dir == 1) y -= 0x20;
    if (dir == 2) x += 0x20;
    if (dir == 3) y += 0x20;
    if (dir == 4) x -= 0x20;

    sub_0714_0160((uint8_t *)&x, (uint8_t *)&y, P8(c + 0x23), P8(c + 0x24));
    rect[0] = (int16_t)(x - P8(c + 0x23));
    rect[1] = (int16_t)(y - P8(c + 0x24));
    rect[2] = (int16_t)(P8(c + 0x23) + x);
    rect[3] = y;

    if (sub_0714_03d2(target, (uint8_t *)rect) == 0 &&
        sub_0714_04a0((uint8_t *)rect) == 0) {
        P16(c) = x;
        P16(c + 2) = y;
    }

    if (P8(c + 0x14) == 1) {
        sub_0791_0638(c, damage);
        DS16(0x10c) = 3;                       /* fx: particle burst */
        sub_0791_020d(x, (int16_t)(y - 10), 10, 15);
        sub_0791_00e5(0x32);
    }
}

/* 026c:0237 - per-frame projectile update: move, test hits vs characters/walls, impact animation, free */
void sub_026c_0237(void)
{
    uint8_t *p = DSADDR(0x692);
    int16_t i, hit, target, dir;
    int16_t rect[4];

    for (i = 0; i < 0x10; i++, p += 0xc) {
        if (P8(p) == 0xff)
            continue;

        if (P16(p + 2) == -1) {
            hit = 0;
            dir = P16(p + 0xa);
            if (dir == 1) P16(p + 8) -= 5;
            if (dir == 2) P16(p + 6) += 0xf;
            if (dir == 3) P16(p + 8) += 5;
            if (dir == 4) P16(p + 6) -= 0xf;

            rect[0] = (int16_t)(P16(p + 6) - 8);
            rect[1] = (int16_t)(P16(p + 8) - 4);
            rect[2] = (int16_t)(P16(p + 6) + 8);
            rect[3] = P16(p + 8);

            target = sub_0714_03d2(PS8(p + 1), (uint8_t *)rect) & 0xf;
            hit = sub_0714_031f((uint8_t *)rect, dir);
            if (target != 0 || hit != 0)
                P16(p + 2) = 0;                /* start impact animation */
            if (target != 0)
                sub_026c_010d(target, dir, 1);
        } else if (P16(p + 2) < 8) {
            P16(p + 2)++;
        } else {
            P8(p) = 0xff;
        }
    }
}

/* 026c:034a - adds every active projectile to the depth-sorted draw list (key y, id 0x20+slot) */
void sub_026c_034a(void)
{
    uint8_t *p = DSADDR(0x692);
    int16_t i;

    for (i = 0; i < 0x10; i++, p += 0xc) {
        if (P8(p) != 0xff)
            sub_0053_075b((uint8_t)P16(p + 8), (uint8_t)(i + 0x20));
    }
}

/* 026c:038b - player fires a shot when its anim state is 8 and frame is 1; sound 0x14 */
void sub_026c_038b(void)
{
    if (DS16(0x128) == 8 && DS16(0x136) == 1) {
        sub_026c_0067(0);
        sub_0791_00e5(0x14);
    }
}
