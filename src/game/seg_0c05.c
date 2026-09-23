/*
 * Segment 0c05 - copy protection, game flag bitfield, 3D-world helpers
 * (camera glide / scripted flight, door detection, zone and encounter spawns).
 */
#include "../core/mem.h"
#include "../platform/platform.h"
#include "protos.h"
#include <stdlib.h>

/* 0c05:0009 - draws the copy-protection box: "Reference : <day> <month>" and the 3-digit value typed so far */
void sub_0c05_0009(int16_t value, int16_t ref)
{
    uint8_t digits[4];

    digits[2] = (uint8_t)(value % 10 + '0');
    digits[1] = (uint8_t)(value / 10 % 10 + '0');
    digits[0] = (uint8_t)(value / 100 + '0');
    digits[3] = 0;
    sub_07fe_06de(0x6e, 0x82, 0xd2, 0x9b, 0x73);
    sub_0596_02a0(0);
    sub_1172_0012(0x78, 0x87, DSPTR(0x2d38), DSADDR(0x239e));          /* "Reference       :" */
    sub_1172_0012(0xa5, 0x87, DSPTR(0x2d38), DSPTR(0x22fe + (ref / 12) * 4)); /* day "01".."25" */
    sub_1172_0012(0xb4, 0x87, DSPTR(0x2d38), DSPTR(0x22ce + (ref % 12) * 4)); /* month "JAN".."DEC" */
    sub_0596_02a0(9);
    sub_1172_0012(0x9b, 0x91, DSPTR(0x2d38), digits);
    sub_118f_0107();
}

/* 0c05:00fc - copy protection: asks the code for a random reference; DS16(0x11c8) = 1 if correct */
void sub_0c05_00fc(void)
{
    int16_t value = 0;
    int16_t ok = 0;
    uint8_t *answers;
    int16_t ref, answer, digit;

    /* port: the code-sheet protection is disabled (as the crack shipped with this copy did):
       keep the random draw so the RNG sequence is unchanged, then accept. */
    if (!getenv("ETERNAM_COPY_PROTECTION")) {
        (void)sub_0053_05f6(0xc0);
        DS16(0x11c8) = 1;
        return;
    }
    answers = sub_0c66_0003(DSADDR(0x23b0), 2);          /* "intro" entry 2: int16[192] */
    ref = sub_0053_05f6(0xc0);
    answer = P16(answers + ref * 2);
    sub_1036_000a(DSPTR(0x2d3c), DSPTR(0x2d38));
    while (DS8(0x2d4e) != 0x1c) {
        plat_yield();
        digit = DS8(0x2d4e);
        if (digit >= 2 && digit <= 0xb)  /* scancodes of keys 1..9,0 */
            digit--;
        else
            digit = -1;
        if (digit == 10)
            digit = 0;
        if (digit != -1) {
            value = (value * 10 + digit) % 1000;
            while (DS8(0x2d4e) != 0)
                plat_yield();
        }
        sub_0c05_0009(value, ref);
    }
    if (value == answer)
        ok = 1;
    DS16(0x11c8) = ok;
    sub_1036_000a(DSPTR(0x3056), DSPTR(0x2d38));
    sub_0f27_0061(answers);
}

/* 0c05:01ea - sets bit n of the game flag bitfield DS 0xf6 */
void sub_0c05_01ea(int16_t n)
{
    DS8(0xf6 + n / 8) |= (uint8_t)(1u << ((n % 8) & 7));
}

/* 0c05:020c - number of set bits in the flag bitfield DS 0xf6[0..12] */
int16_t sub_0c05_020c(void)
{
    int16_t count = 0;
    int16_t i, bit;

    for (i = 0; i < 13; i++)
        for (bit = 0; bit < 8; bit++)
            if (DS8(0xf6 + i) & (1 << bit))
                count++;
    return count;
}

/* 0c05:0254 - glides the 3D camera to (x,y,heading), rendering every step; overlay draws DSPTR(0x34ce) shape 0 */
void sub_0c05_0254(int16_t x, int16_t y, int16_t heading, int16_t overlay)
{
    int16_t x0 = DS16(0x1fc2);
    int16_t y0 = DS16(0x1fc4);
    int16_t h0 = DSS8(0x1fcd);
    int16_t dist, dx, dh, steps, i;

    dist = y - y0;
    if (dist < 0)
        dist = -dist;
    dx = x - x0;
    if (dx < 0)
        dx = -dx;
    dist += dx;
    dist /= 40;
    dh = heading - h0;
    if (dh < 0)
        dh = -dh;
    steps = dist;
    if (dh > dist)
        steps = dh;

    for (i = 1; i <= steps; i++) {
        DS16(0x1fc2) = sub_0c9b_00d8(x0, x, i, steps);
        DS16(0x1fc4) = sub_0c9b_00d8(y0, y, i, steps);
        if (i <= dh && dh != 0)
            DS8(0x1fcd) = (uint8_t)sub_0c9b_00d8(h0, heading, i, dh);
        sub_0c9b_04a1();
        sub_0a86_0e64(0);
        if (overlay != 0)
            sub_106a_0477(0, 0, 0xab, DSPTR(0x34ce));
        sub_0a86_0d96(0);
        if (DS16(0x1fc6) > 8)
            DS16(0x1fc6) -= 5;
    }
}

/* 0c05:036f - scripted 3D flight along the 7 waypoints of DS 0x22a4 with the "i2" overlay and music 0x24 */
void sub_0c05_036f(void)
{
    int16_t i;

    DSPTR_SET(0x34ce, sub_0053_0413(DSADDR(0x23b6), 0xc));   /* "i2" */
    sub_0791_0132(0x24);
    DS16(0x1fc2) = 0xd00;
    DS16(0x1fc4) = 0x1d63;
    DS8(0x1fcd) = 0xfc;
    for (i = 0; i < 7; i++)
        sub_0c05_0254(DS16(0x22a4 + i * 6), DS16(0x22a4 + i * 6 + 2), DS16(0x22a4 + i * 6 + 4), 1);
    sub_0f27_0061(DSPTR(0x34ce));
    sub_0791_0178();
}

/* 0c05:0400 - 3D door check: camera 512-unit cell + zone vs door tables DS 0x21a4 (rooms) and 0x2244 (special) */
void sub_0c05_0400(void)
{
    int16_t cx = DS16(0x1fc2) / 0x200;
    int16_t cy = DS16(0x1fc4) / 0x200;
    uint8_t *p;
    int16_t i, zone, ex, ey;

    p = DSADDR(0x21a4);
    for (i = 0; i < 20; i++) {
        zone = P16(p);
        p += 2;
        ex = P16(p) / 0x200;
        p += 2;
        ey = P16(p) / 0x200;
        p += 4;
        if (cx == ex && cy == ey && zone == DS16(0x1fd6)) {
            DS16(0xa2) = i;
            break;
        }
    }

    p = DSADDR(0x2244);
    for (i = 0; i < 3; i++) {
        zone = P16(p);
        p += 2;
        ex = P16(p) / 0x200;
        p += 2;
        ey = P16(p) / 0x200;
        p += 4;
        if (cx == ex && cy == ey && zone == DS16(0x1fd6)) {
            if (DS16(0xff4) == 0) {
                DS16(0x2286) = 1;
            } else {
                DS16(0x11bc) = i;
                DS16(0xa2) = 3;
                DS16(0x329e) = 7;
            }
        }
    }
}

/* 0c05:0511 - 3D zone change: clears objects and spawns up to 8 zone objects (lists zone*8+i of DS 0x1bbc) */
void sub_0c05_0511(void)
{
    int16_t i, list, idx;

    sub_03dd_011f();
    sub_0053_0112();
    for (i = 0; i < 8; i++) {
        list = (DS16(0x1fd6) << 3) + i;
        idx = sub_0936_0005(list);
        if (DS16(0x1bbc + idx * 2) == -1)
            break;
        sub_0936_0106(list, i + 1);
    }
    DS16(0x1fd8) = DS16(0x1fd6);
}

/* 0c05:056b - random encounter: spawns list zone*16+DS16(0x1fda)+0x28 at its position jittered by +-250 */
void sub_0c05_056b(void)
{
    int16_t list, idx, sx, sy;

    list = (DS16(0x1fd6) << 4) + DS16(0x1fda) + 0x28;
    idx = sub_0936_0005(list);
    if (DS16(0x1bbc + idx * 2) != -1) {
        sx = DS16(0x1bbc + (idx + 4) * 2);
        sy = DS16(0x1bbc + (idx + 5) * 2);
        DS16(0x1bbc + (idx + 4) * 2) += sub_0053_05f6(0x1f5) - 250;
        DS16(0x1bbc + (idx + 5) * 2) += sub_0053_05f6(0x1f5) - 250;
        sub_0936_0106(list, -1);
        DS16(0x1bbc + (idx + 4) * 2) = sx;
        DS16(0x1bbc + (idx + 5) * 2) = sy;
    }
}
