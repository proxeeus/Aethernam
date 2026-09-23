/*
 * Segment 0791 - sound driver glue (music / effects from MUS.PAK), palette upload and tint,
 * particle / lightning / sprinkle screen effects, player hit points, quit.
 *
 * Music driver commands (sub_0e9f_0013 cmd): 0x40 stop, 0x80 play, 0x2000 set volume,
 * 0x8000 ramp volume.  Effect commands (sub_0e9f_006f): 0x40 stop, 0x80 / 0xa0 play.
 * DS16(0xe8) music volume, DS16(0xea) effects on, DS16(0xd6) current song.
 * Particles: DS 0x30ae, 8 bytes {x, y, vy, vx}, count DS16(0x303e), origin DS16(0x3036/0x3038).
 * Lightning bolts reuse the same array as {x1, y1, y2, x2}, count DS16(0x1740).
 */
#include "../core/mem.h"
#include "../platform/platform.h"
#include "protos.h"

/* 0791:0005 - lowers the music volume to vol (ignored if louder than the current one); 0 stops */
void sub_0791_0005(int16_t vol)
{
    if (vol <= DS16(0xe8)) {
        DS16(0xe8) = vol;
        if (vol == 0)
            sub_0e9f_0013(0, 0, 0x40);
        else
            sub_0e9f_0013(DS16(0xe8), 0, 0x2000);
    }
}

/* 0791:0039 - stops the music */
void sub_0791_0039(void)
{
    sub_0e9f_0013(0, 0, 0x40);
}

/* 0791:0048 - loads MUS.PAK entry n into the sound-effect bank buffer and registers it */
void sub_0791_0048(int16_t n)
{
    sub_0fa2_0008(DSPTR(0x18b0), n, DSPTR(0x17c4));
    sub_0e9f_017e(0, DSPTR(0x17c4));
}

/* 0791:0078 - switches to song n of MUS.PAK (quits if it cannot be loaded), restores the volume */
void sub_0791_0078(int16_t n)
{
    int16_t ok;

    sub_0791_0039();
    ok = sub_0fa2_0008(DSPTR(0x18b0), n, DSPTR(0x17c0));
    if (ok == 0)
        sub_0791_0665();
    sub_0e9f_01a8(0, DSPTR(0x17c0));
    sub_0791_0005(DS16(0xe8));
    if (DS16(0xe8) != 0)
        sub_0e9f_0013(0, 0, 0x80);
    DS16(0xd6) = n;
}

/* 0791:00e5 - plays sound effect n (if effects are on), ducking the music to volume 50 */
void sub_0791_00e5(int16_t n)
{
    if (DS16(0xe8) > 0x32) {
        sub_0e9f_0013(0x32, 0, 0x2000);
        sub_0e9f_0013(DS16(0xe8), 0, 0x8000);
    }
    sub_0e9f_006f(0, 0, 0x40);
    if (DS16(0xea) > 0)
        sub_0e9f_006f(n, 0, 0x80);
}

/* 0791:0132 - plays sound effect n with flags 0xa0 regardless of the effects switch, ducking music */
void sub_0791_0132(int16_t n)
{
    if (DS16(0xe8) > 0x32) {
        sub_0e9f_0013(0x32, 0, 0x2000);
        sub_0e9f_0013(DS16(0xe8), 0, 0x8000);
    }
    sub_0e9f_006f(0, 0, 0x40);
    sub_0e9f_006f(n, 0, 0xa0);
}

/* 0791:0178 - stops the current sound effect */
void sub_0791_0178(void)
{
    sub_0e9f_006f(0, 0, 0x40);
}

/* 0791:0187 - loads a red-tinted copy (strength level 0..16) of the base palette DSPTR(0x304c) */
void sub_0791_0187(int16_t level)
{
    uint8_t pal[0x300];

    sub_0dc5_0448(DSPTR(0x304c), pal, level);
    sub_0791_067f(pal);
}

/* 0791:01b7 - (re)spawns one particle near the effect origin with a random upward velocity */
void sub_0791_01b7(uint8_t *part)
{
    P16(part + 0) = sub_0053_05f6(11) + DS16(0x3036) - 5;       /* x  */
    P16(part + 2) = sub_0053_05f6(6) + DS16(0x3038) - 3;        /* y  */
    P16(part + 6) = sub_0053_05f6(3) - 1;                       /* vx */
    P16(part + 4) = -(sub_0053_05f6(7) + 1);                    /* vy */
}

/* 0791:020d - starts a particle burst of count particles at (x,y) lasting duration frames */
void sub_0791_020d(int16_t x, int16_t y, int16_t count, int16_t duration)
{
    uint8_t *p = DSADDR(0x30ae);
    int16_t i;

    DS16(0x3036) = x;
    DS16(0x3038) = y;
    DS16(0x303e) = count;
    for (i = 0; i < DS16(0x303e); i++) {
        sub_0791_01b7(p);
        p += 8;
    }
    DS16(0x3260) = duration;
}

/* 0791:025b - moves and draws the particles (gravity, colour 0x37); a particle that falls below
 * the floor is stamped into the background buffer DSPTR(0x3056) and respawned */
void sub_0791_025b(void)
{
    uint8_t *p = DSADDR(0x30ae);
    uint8_t *saved;
    int16_t i;
    int16_t ox, oy;

    sub_1100_00c0(0x37);
    for (i = 0; i < DS16(0x303e); i++, p += 8) {
        ox = P16(p + 0);
        oy = P16(p + 2);
        P16(p + 0) += P16(p + 6);
        P16(p + 2) += P16(p + 4);
        P16(p + 4)++;

        if ((int16_t)(sub_0053_05f6(10) + 0x8c) < P16(p + 2)) {
            saved = DSPTR(0x2d38);
            DSPTR_SET(0x2d38, DSPTR(0x3056));
            sub_1100_000b(P16(p + 0), P16(p + 2));
            DSPTR_SET(0x2d38, saved);
            sub_0791_01b7(p);
            ox = DS16(0x3036);
            oy = DS16(0x3038);
        }

        if (sub_0053_05f6(2) == 0)
            sub_1040_0006(P16(p + 0), P16(p + 2), ox, oy, 0x37);
        else
            sub_1100_000b(P16(p + 0), P16(p + 2));
    }
}

/* 0791:034e - draws a jagged lightning bolt from (x1,y1) to (x2,y2) in 10 jittered segments */
void sub_0791_034e(int16_t x1, int16_t y1, int16_t x2, int16_t y2)
{
    int16_t px = x1, py = y1;
    int16_t nx, ny;
    int16_t i;
    int16_t special;
    int16_t color;
    int16_t jit;

    for (i = 1; i <= 10; i++) {
        special = (int16_t)(sub_0f21_0004() % 10u) + 1;
        color = (int16_t)(sub_0f21_0004() & 0xf) + 0xc0;
        jit = (int16_t)(sub_0f21_0004() & 3);
        nx = jit + ((int16_t)(uint16_t)((uint16_t)(x2 - x1) * (uint16_t)i) / 10 + x1 - 2);
        jit = (int16_t)(sub_0f21_0004() & 3);
        ny = jit + ((int16_t)(uint16_t)((uint16_t)(y2 - y1) * (uint16_t)i) / 10 + y1 - 2);
        if (i == special)
            color = 0x36;
        sub_1040_0006(px, py, nx, ny, (uint8_t)color);
        px = nx;
        py = ny;
    }
}

/* 0791:040c - draws all queued lightning bolts, empties the queue and steps the 0xc1..0xce cycle */
void sub_0791_040c(void)
{
    uint8_t *p = DSADDR(0x30ae);
    int16_t i;

    for (i = 0; i < DS16(0x1740); i++) {
        sub_0791_034e(P16(p + 0), P16(p + 2), P16(p + 6), P16(p + 4));
        p += 8;
    }
    DS16(0x1740) = 0;

    DS16(0x173a) += DS16(0x173c);
    if (DS16(0x173a) > 0xce)
        DS16(0x173c) = -1;
    if (DS16(0x173a) < 0xc1)
        DS16(0x173c) = 1;
}

/* 0791:0506 - sprinkles random colour-1 pixels (some as '+' crosses) inside the rectangle */
void sub_0791_0506(int16_t x1, int16_t y1, int16_t x2, int16_t y2)
{
    int16_t w, h, n, i;
    int16_t px, py;

    if (x1 >= x2)
        return;
    if (y1 >= y2)
        return;

    w = x2 - x1;
    h = y2 - y1;
    n = (w >> 1) + h;
    sub_1100_00c0(1);
    for (i = 0; i < n; i++) {
        px = sub_0053_05f6(w) + x1;
        py = sub_0053_05f6(h) + y1;
        sub_1100_000b(px, py);
        if (sub_0053_05f6(2) != 0) {
            sub_1100_000b(px, py - 1);
            sub_1100_000b(px, py + 1);
            sub_1100_000b(px - 1, py);
            sub_1100_000b(px + 1, py);
        }
    }
}

/* 0791:05d9 - one-shot sprinkle effect in the rectangle stored at DS 0x3020..0x3026 */
void sub_0791_05d9(void)
{
    DS16(0x32a0) = 0;
    sub_0791_0506(DS16(0x3020), DS16(0x3022), DS16(0x3024), DS16(0x3026));
}

/* 0791:05f7 - returns the size of the largest free memory block (farcoreleft) */
int32_t sub_0791_05f7(void)
{
    return sys_farcoreleft();
}

/* 0791:0604 - player hit points: +1 every 512 ticks up to 99; at 0 starts the death scene */
void sub_0791_0604(void)
{
    if (DS16(0x144) > 0) {
        if (DS32(0x1fdc) % 512L == 0 && DS16(0x144) < 0x63)
            DS16(0x144)++;
    } else {
        sub_03dd_1147(0);
    }
}

/* 0791:0638 - subtracts damage from the actor's hit points (+0x2a), not below 0 */
void sub_0791_0638(uint8_t *actor, int16_t damage)
{
    int16_t hp = P16(actor + 0x2a);

    hp -= damage;
    if (hp < 0)
        hp = 0;
    P16(actor + 0x2a) = hp;
}

/* 0791:0665 - quits the game: removes timer/keyboard hooks, stops the sound driver, restores
 * the video mode and exits */
void sub_0791_0665(void)
{
    sub_0d7d_002e();
    sub_0e9f_011f();
    sub_0ebb_0363();
    sub_11b2_0043();
    sub_11bb_0009(0);
}

/* 0791:067f - loads a 768-byte 8-bit RGB palette into the DAC, two halves after vertical retraces */
void sub_0791_067f(uint8_t *pal)
{
    uint8_t buf[0x300];

    sub_120e_0002(pal, buf, 0x300);
    sub_0e3f_00de(buf);
    sub_0e3f_0104();
    sub_0e3f_00be(buf, 0, 0x80);
    sub_0e3f_0104();
    sub_0e3f_00be(buf + 0x180, 0x80, 0x80);
}
