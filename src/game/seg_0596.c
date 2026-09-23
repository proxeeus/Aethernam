/*
 * Segment 0596 - room rendering, speech bubbles, choice menu, PAK animation player.
 *
 * Buffers: DSPTR(0x2d38) draw buffer, DSPTR(0x2d3c) screen, DSPTR(0x3056) room
 * background, DSPTR(0x17b4) 64K backdrop load buffer, DSPTR(0x3294) morph work buffer.
 *
 * Actor record (DS:0x11a + i*0x46, 0 = hero):
 *   +0x00 x  +0x02 y  +0x0c room  +0x0e anim state  +0x10 anim type (2 = morph)
 *   +0x12 kind  +0x16 fptr sprite/anim PAK  +0x1a sequence  +0x1c frame/dir
 *   +0x1e morph step  +0x20 frame count  +0x2c u8 text colour
 *   +0x3c clip x1  +0x3e clip x2  +0x40 u8 clip y1  +0x41 u8 clip y2
 *
 * Animation sequence (sub_0596_0b11): +1 u8 frame count, 8-byte frames from +2:
 *   +0 sprite, +2 (type << 14 | param), +4 dx, +6 dy (cumulative offsets).
 */
#include "../core/mem.h"
#include "../platform/platform.h"
#include "protos.h"

#define ACTOR(i)  DSADDR((uint16_t)((i) * 0x46 + 0x11a))

/* 0596:0001 - allocates the 0xFFFF-byte backdrop buffer DSPTR(0x17b4) if not yet done */
void sub_0596_0001(void)
{
    if (DSPTR(0x17b4) != NULL)
        return;
    if ((uint32_t)sub_0791_05f7() < 0xffffu)   /* farcoreleft() */
        sub_0053_028e();                       /* purge resource cache */
    DSPTR_SET(0x17b4, sub_0f27_0004(0xffffL));
}

/* 0596:0033 - frees the backdrop buffer DSPTR(0x17b4) */
void sub_0596_0033(void)
{
    if (DSPTR(0x17b4) != NULL)
        sub_0f27_0061(DSPTR(0x17b4));
    DSPTR_SET(0x17b4, NULL);
}

/* 0596:0059 - per-frame room FX (reflection, particles...) and presentation of the frame */
void sub_0596_0059(void)
{
    int16_t x = DS16(0x11a);        /* hero x */
    int16_t y = DS16(0x11c);        /* hero y */
    uint8_t *act, *frames;
    uint16_t spr;

    /* fx 1: hero reflection in two window rectangles (actor 1 mirrors hero's anim) */
    if (DS16(0x10c) == 1 && y < 0x88 && x > 0xa0) {
        act = DSADDR(0x160);
        sub_0053_0cf2(act, DS16(0x1abc + (uint16_t)(DS16(0x134) * 2)));
        P16(act + 0x1c) = DS16(0x136);
        frames = sub_0596_0b11(P16(act + 0x1a), FP(act + 0x16)) + 2;
        spr = PU16(frames + (int16_t)(P16(act + 0x1c) * 8));
        sub_0596_02b7(0xae, 0x6c, 0xc1, 0x88);
        sub_106a_0477(spr, x, y, FP(act + 0x16));
        sub_0596_02b7(0xc0, 0x64, 0xe0, 0x88);
        sub_106a_0477(spr, x, y, FP(act + 0x16));
        sub_0596_02d6();
    }

    /* fx 3: particles until timer runs out */
    if (DS16(0x10c) == 3) {
        DS16(0x3260)--;
        if (DS16(0x3260) > 0)
            sub_0791_025b();
        else
            DS16(0x10c) = 0;
    }
    if (DS16(0x1740) != 0)
        sub_0791_040c();
    if (DS16(0x32a0) != 0)
        sub_0791_05d9();
    if (DS16(0x173e) != 0) {
        DS16(0x173e) = 0;
        sub_0596_0eee();
    }
    if (DS16(0x1738) != 0)
        sub_0596_0ec3();
    if (DS16(0x17e2) == 1 && DS16(0x177c) == 0)
        sub_0dc5_017d();

    sub_0053_1e2e();                /* frame limiter */

    /* present */
    if (DS16(0xdc) != 0) {
        DSU16(0x2d66)++;
        sub_02a7_0b4a(DS16(0xde), DS16(0xe2), DS16(0xe0), DS16(0xe4));
        DS16(0xdc) = 0;
    } else if (DS16(0x177c) == 0) {
        sub_0dc5_0148();
    } else if (DS16(0x177c) == 1) {
        sub_0dc5_0355(x, y);
    } else if (DS16(0x177c) == 2) {
        sub_0dc5_02dd(x, y);
    } else if (DS16(0x177c) == 3) {
        sub_0dc5_026b(x, y);
    }

    /* room 0x17: palette pulse between base palette and tinted version */
    if (DS16(0x11c0) == 1 && DS16(0xa0) == 3 && DS16(0xce) == 0x17) {
        DS16(0xb0) += DS16(0xb2);
        if (DS16(0xb0) > 0xf) {
            DS16(0xb0) = 0xf;
            DS16(0xb2) = -1;
        }
        if (DS16(0xb0) < 0) {
            DS16(0xb0) = 0;
            DS16(0xb2) = 1;
        }
        sub_0dc5_0448(DSPTR(0x304c), DSPTR(0x3032), DS16(0xb0));
        sub_0791_067f(DSPTR(0x3032));
    }
}

/* 0596:02a0 - selects the game font DSPTR(0x3042) with text colour `color` */
void sub_0596_02a0(int16_t color)
{
    sub_1172_011d(DSPTR(0x3042), (uint8_t)color);
}

/* 0596:02b7 - sets the clip rectangle (x1,y1)-(x2,y2) */
void sub_0596_02b7(int16_t x1, int16_t y1, int16_t x2, int16_t y2)
{
    sub_0fe6_01ac(x1, x2);
    sub_0fe6_01bf(y1, y2);
}

/* 0596:02d6 - clip rectangle = play area */
void sub_0596_02d6(void)
{
    sub_0596_02b7(0, DS16(0x303c), 0x13f, DS16(0x3054) - 1);
}

/* 0596:02ec - clip rectangle = full screen */
void sub_0596_02ec(void)
{
    sub_0596_02b7(0, 0, 0x13f, 0xc7);
}

/* 0596:02fe - draws string str at (x,y) in the draw buffer with colour `color` */
void sub_0596_02fe(uint8_t *str, int16_t x, int16_t y, int16_t color)
{
    sub_0596_02a0(color);
    sub_1172_0012(x, y, DSPTR(0x2d38), str);
}

/* 0596:0327 - returns the pointer just past the NUL terminating str */
uint8_t *sub_0596_0327(uint8_t *str)
{
    while (*str++ != 0)
        ;
    return str;
}

/* 0596:033e - draws up to 3 lines of a '*'-terminated text page (mode 0 = centred on x); returns next y */
int16_t sub_0596_033e(uint8_t *text, int16_t x, int16_t y, int16_t color, int16_t mode)
{
    int16_t lines = 0;
    int16_t lx = x;
    int16_t ly = y;
    int16_t rx, w;
    /* Uninitialised in the original when mode != 0: [bp-0xa] then holds the CS
     * word of a far return address left on the stack by the preceding
     * sub_0596_04b7 call chain, i.e. always non-zero => every line is drawn. */
    int16_t half = 1;

    if (DS16(0x303c) + 3 > ly)
        ly = DS16(0x303c) + 3;

    while (*text != '*') {
        if (mode == 0) {
            w = sub_1172_018c(text);
            half = w >> 1;
            lx = x - half;
            rx = x + half;
            if (lx < 3)
                lx = 3;
            if (rx > 0x13c)
                lx -= rx - 0x13c;
        }
        if (half != 0) {
            sub_0596_02fe(text, lx, ly, color);
            ly += 10;
        }
        text = sub_0596_0327(text);
        if (++lines == 3)
            break;
    }
    return ly;
}

/* 0596:0405 - measures one speech page (size, display time, next page pointer) */
void sub_0596_0405(uint8_t *text)
{
    int16_t w = 0;
    int16_t lines = 0;

    DSPTR_SET(0x3242, text);
    DS16(0x30ac) = 0;
    DS16(0x30aa) = 0;
    DS16(0x17be) = 0;
    DS16(0x328a) = 0;

    while (*text != '*') {
        w = sub_1172_018c(text);
        DS16(0x328a) += w / 4;
        if (w > DS16(0x30aa))
            DS16(0x30aa) = w;
        text = sub_0596_0327(text);
        if (w != 0)
            DS16(0x30ac)++;
        if (++lines == 3 && *text != '*') {
            DS16(0x17be) = 1;       /* another page follows */
            break;
        }
    }
    DSPTR_SET(0x3028, text);
    DS16(0x30aa) = DS16(0x30aa) >> 1;
    DS16(0x30ac) = DS16(0x30ac) * 10;
    DS16(0x32aa) = DS16(0x328a);
}

/* 0596:04b7 - draws a speech-bubble frame, shifted to stay inside the screen/play area */
void sub_0596_04b7(int16_t x1, int16_t y1, int16_t x2, int16_t y2)
{
    int16_t d;

    if (x1 < 0) {
        x2 -= x1;
        x1 = 0;
    }
    if (x2 > 0x13f) {
        x1 -= x2 - 0x13f;
        x2 = 0x13f;
    }
    if (y1 < DS16(0x303c)) {
        d = y2 - y1;
        y1 = DS16(0x303c);
        y2 = y1 + d;
    }
    if (y2 > DS16(0x3054)) {
        d = y2 - y1;
        y2 = 0xab;
        y1 = y2 - d;
    }

    sub_10f5_0028(x1 + 1, y1 + 1, x2 - 1, y2 - 1, 0x1a);          /* interior */
    sub_1040_0006(x1 + 1, y1,     x2 - 1, y1,     0x43);          /* top */
    sub_1040_0006(x1 + 1, y1 + 1, x2 - 1, y1 + 1, 0x45);
    sub_1040_0006(x1 + 1, y2 - 1, x2 - 1, y2 - 1, 0x45);          /* bottom */
    sub_1040_0006(x1 + 1, y2,     x2 - 1, y2,     0x43);
    sub_1040_0006(x1,     y1 + 1, x1,     y2 - 1, 0x43);          /* left */
    sub_1040_0006(x1 + 1, y1 + 1, x1 + 1, y2 - 1, 0x45);
    sub_1040_0006(x2,     y1 + 1, x2,     y2 - 1, 0x43);          /* right */
    sub_1040_0006(x2 - 1, y1 + 1, x2 - 1, y2 - 1, 0x45);

    sub_106a_0477(0, x1, y1, DSPTR(0x3066));         /* corners */
    sub_106a_0477(1, x2, y1, DSPTR(0x3066));
    sub_106a_0477(2, x1, y2, DSPTR(0x3066));
    sub_106a_0477(3, x2, y2, DSPTR(0x3066));
}

/* 0596:067e - ends the speech display */
void sub_0596_067e(void)
{
    DS16(0x17bc) = 0;
    DS16(0x9c) = 0;
}

/* 0596:0687 - per-frame speech bubble above the speaking actor; advances pages / ends speech */
void sub_0596_0687(void)
{
    uint8_t *a;
    int16_t x, y;

    if (DS16(0x17bc) == 0)
        return;

    a = sub_03dd_00ac(DS16(0x3298));
    if (P16(a + 0xc) != DS16(0xce)) {
        sub_0596_067e();
        return;
    }

    if (DS16(0xa0) == 8)
        x = 0xa0;
    else
        x = P16(a + 0);
    if (DS16(0x9c) != 0)
        y = DS16(0x9c);
    else
        y = P16(a + 2) - (DS16(0x30ac) + 0x32);

    sub_0596_04b7(x - (DS16(0x30aa) + 3), y - 3, x + DS16(0x30aa) + 3, y + DS16(0x30ac));
    sub_0596_033e(DSPTR(0x3242), x + 1, y, DS16(0x325e), 0);

    DS16(0x328a)--;
    if (DS16(0x328a) > 0)
        return;
    DS16(0x17bc) = DS16(0x17be);
    if (DS16(0x17bc) != 0)
        sub_0596_0405(DSPTR(0x3028));
    else
        sub_0596_067e();
}

/* 0596:0767 - draws the dialogue choice menu (question + answers, selected one highlighted) */
void sub_0596_0767(void)
{
    int16_t x = DS16(0x302c);
    int16_t y = DS16(0x302e);
    int16_t col = DS8(0x146);                   /* hero text colour */
    int16_t base = (col == 0x19) ? 0x16 : col;
    int16_t c, i;
    uint8_t *ent;

    if (DS16(0x3278) != -1) {                   /* question text */
        sub_0596_0405(DSPTR(0x3252));
        sub_0596_04b7(x - 3, y - 3, x + DS16(0x30aa) + DS16(0x30aa) + 3, y + DS16(0x30ac));
        y = sub_0596_033e(DSPTR(0x3252), x, y, col, -1);
        x += 0x10;
        y += 10;
    }

    ent = DSADDR(0x306a);
    for (i = 0; i < DS16(0x303a); i++) {
        c = base;
        if (i == DS16(0x3274)) {
            if (col == 0x19) {
                c = DS16(0x17b8);
                DS16(0x17b8) += DS16(0x17ba);
                if (DS16(0x17b8) > 0x19) {
                    DS16(0x17b8) = 0x19;
                    DS16(0x17ba) = -1;
                }
                if (DS16(0x17b8) < 0x16) {
                    DS16(0x17b8) = 0x16;
                    DS16(0x17ba) = 1;
                }
            } else if (DS16(0x1896) & 1) {
                c = col;
            } else {
                c = 0x9f;
            }
        }
        sub_0596_0405(FP(ent + 2));
        sub_0596_04b7(x - 3, y - 3, x + DS16(0x30aa) + DS16(0x30aa) + 3, y + DS16(0x30ac));
        y = sub_0596_033e(FP(ent + 2), x, y, c, 1);
        y += 4;
        ent += 10;
    }
}

/* 0596:08e0 - draws projectile slot i (flying shot or impact frame) */
void sub_0596_08e0(int16_t i)
{
    uint8_t *s = DSADDR((uint16_t)(i * 0xc + 0x692));

    if (P8(s) == 0xff)
        return;
    if (P16(s + 2) == -1) {
        uint8_t *img = sub_0053_052f(1, DSPTR(0x3262));
        sub_0fe6_01d2(0, P16(s + 6), P16(s + 8) - 25, DSPTR(0x2d38), img);
    } else {
        sub_106a_04ff(P16(s + 2), P16(s + 6), P16(s + 8), DSPTR(0x3262), P16(s + 4));
    }
}

/* 0596:0965 - horizontal room-scroll transition in 8 steps of 40 px (new room from DSPTR(0x3056)) */
void sub_0596_0965(int16_t dir)
{
    int16_t x;

    sub_1036_000a(DSPTR(0x2d3c), DSPTR(0x2d38));   /* screen -> draw buffer */
    for (x = 0; x < 0x140; x += 0x28) {
        if (dir == -1) {
            sub_0dc5_0000(0x28);                    /* shift left, strip enters at right */
            sub_0dc5_007f(0x28, x);
        } else {
            sub_0dc5_0040(0x28);                    /* shift right, strip enters at left */
            sub_0dc5_00ca(0x28, x);
        }
        /* no explicit delay in the original: speed was bounded by the copy itself */
        sub_0dc5_0148();
        sub_0053_05cd();
    }
}

/* 0596:09d5 - room-scroll transition, new room enters from the right */
void sub_0596_09d5(void)
{
    sub_0596_0965(-1);
}

/* 0596:09de - room-scroll transition, new room enters from the left */
void sub_0596_09de(void)
{
    sub_0596_0965(0);
}

/* 0596:09e7 - fx 2: draws the actor's mirror image clipped to (x1,y1)-(x2,y2) when in front of it */
void sub_0596_09e7(uint8_t *actor, int16_t x1, int16_t y1, int16_t x2, int16_t y2)
{
    int16_t ax = P16(actor + 0);
    int16_t ay = P16(actor + 2);
    int16_t dir;
    uint8_t *frames;

    if ((int16_t)(x1 - 0x10) > ax)
        return;
    if ((int16_t)(x2 + 0x10) < ax)
        return;
    if (ay < y2)
        return;

    sub_0596_02b7(x1, y1, x2, y2);
    ax -= 3;
    ay -= 8;
    dir = P16(actor + 0x1c);
    if (P16(actor + 0xe) == 4)
        dir = (dir + 4) & 7;

    if (P16(actor + 0x12) == 2 || P16(actor + 0x12) == 4) {
        frames = sub_0596_0b11(P16(actor + 0x1a), FP(actor + 0x16)) + 2;
        sub_106a_0477(PU16(frames + (int16_t)(dir * 8)), ax, ay, FP(actor + 0x16));
    } else {
        if (dir >= 4)
            ax -= 2;
        if (P16(actor + 0x12) == 1)
            sub_106a_0477(DS16(0x1820 + (uint16_t)(dir * 2)), ax, ay, FP(actor + 0x16));
        else
            sub_106a_0477(DS16(0x180e + (uint16_t)(dir * 2)), ax, ay, FP(actor + 0x16));
    }
    sub_0596_02d6();
}

/* 0596:0b11 - returns animation sequence `seq` (sub-entry of entry 2) of a PAK resource */
uint8_t *sub_0596_0b11(int16_t seq, uint8_t *res)
{
    return sub_0053_0564(2, seq, res);
}

/* 0596:0b28 - draws animation frame idx at (x,y)+cumulative offsets; morph frames return next step */
int16_t sub_0596_0b28(uint8_t *bank, uint8_t *frames, int16_t idx, int16_t step,
                      int16_t x, int16_t y, int16_t nframes)
{
    uint8_t *work = DSPTR(0x3294);
    uint8_t *fr = frames + (int16_t)(idx * 8);
    uint16_t spr = PU16(fr + 0);
    uint16_t param = PU16(fr + 2);
    int16_t type = param >> 14;
    uint16_t next;
    uint8_t *q = frames + 4;
    int16_t px = x, py = y;
    int16_t k;

    param &= 0x3fff;

    for (k = 0; k <= idx; k++) {                /* cumulative frame offsets */
        px += P16(q);
        q += 2;
        py += P16(q);
        q += 6;
    }

    if (type == 1) {                            /* morph toward the next frame */
        if (nframes - 1 == idx)
            next = spr;
        else
            next = PU16(fr + 8);
        if (param == 0)
            param = 1;
        sub_0a21_0005((int16_t)spr, 0, 0, bank, (int16_t)next, 0, 0, bank, work);
        sub_106a_06a1(0, px, py, work, (uint16_t)(step << 8) / param);
        step++;
        if (step >= (int16_t)(PU16(fr + 2) & 0x3fff))
            step = 0;
        return step;
    }
    if (type == 0)
        sub_106a_0477(spr, px, py, bank);
    if (type == 2)
        sub_106a_04ff(spr, px, py, bank, (int16_t)param);
    if (type == 3)
        sub_106a_0593(spr, px, py, bank, param & 0xff);
    return 0;
}

/* 0596:0cca - draws actor i (mirror image, morph or normal frame) clipped to its rectangle */
void sub_0596_0cca(int16_t i)
{
    uint8_t *a = ACTOR(i);
    int16_t x = P16(a + 0);
    int16_t y = P16(a + 2);
    int16_t b23 = P8(a + 0x23);     /* loaded but unused in the original */
    int16_t b24 = P8(a + 0x24);
    uint8_t *frames;

    (void)b23;
    (void)b24;
    frames = sub_0596_0b11(P16(a + 0x1a), FP(a + 0x16)) + 2;
    sub_0596_02b7(P16(a + 0x3c), P8(a + 0x40), P16(a + 0x3e), P8(a + 0x41));

    if (DS16(0x10c) == 2)           /* fx 2: mirror */
        sub_0596_09e7(a, DS16(0x3036), DS16(0x3038), DS16(0x3036) + 7, DS16(0x3038) + 0x19);

    if (P16(a + 0x10) == 2) {
        P16(a + 0x1e) = sub_0596_0b28(FP(a + 0x16), frames, P16(a + 0x1c), P16(a + 0x1e),
                                      x, y, P16(a + 0x20));
    } else {
        if (i == 0 && DS16(0x329a) != 0 && P16(a + 0x12) == 1)
            sub_02a7_0c92();        /* look beam behind the hero */
        sub_106a_0477(PU16(frames + (int16_t)(P16(a + 0x1c) * 8)), x, y, FP(a + 0x16));
        if (i == 0 && DS16(0x11be) != 0)
            sub_0791_0506(x - 0xc, y - 0x2a, x + 0xc, y + 2);
    }
    sub_0596_02d6();
}

/* 0596:0e28 - draws the action-bar cursor (flash state 3 or normal) */
void sub_0596_0e28(void)
{
    if (DS16(0x3266) == 3) {
        sub_106a_0477(DS16(0x17b0) + 2, 0, 0, DSPTR(0x3062));
        sub_106a_0477(1, 0, 0, DSPTR(0x3062));
        sub_106a_0477(9, DS16(0x17a6), DS16(0x17a8), DSPTR(0x3062));
    } else {
        sub_106a_0477(8, DS16(0x17a6), DS16(0x17a8), DSPTR(0x3062));
    }
}

/* 0596:0e96 - draws the bottom panel background (glyphs 0 and 1) */
void sub_0596_0e96(void)
{
    sub_106a_0477(0, 0, 0, DSPTR(0x3062));
    sub_106a_0477(1, 0, 0, DSPTR(0x3062));
}

/* 0596:0ec3 - redraws the bottom panel and copies it to the screen */
void sub_0596_0ec3(void)
{
    DS16(0x1738) = 0;
    sub_0596_02ec();
    sub_0596_0e96();
    sub_02a7_0b4a(0, 0xac, 0x140, 0xc7);
    sub_02a7_08f3();
    sub_0596_02d6();
}

/* 0596:0eee - clears the play area of the draw buffer */
void sub_0596_0eee(void)
{
    sub_10f5_0028(0, 0, 0x140, 0xab, 0);
}

/* 0596:0f03 - (dead code) draws the bottom panel background */
void sub_0596_0f03(void)
{
    sub_0596_0e96();
}

/* 0596:0f08 - (dead code) draws the bottom panel on the screen and in the draw buffer */
void sub_0596_0f08(void)
{
    uint8_t *save;

    sub_0596_02ec();
    save = DSPTR(0x2d38);
    DSPTR_SET(0x2d38, DSPTR(0x2d3c));
    sub_0596_0f03();
    DSPTR_SET(0x2d38, save);
    sub_0596_0f03();
    sub_0596_02d6();
}

/* 0596:0f41 - plays animation seq of A??.PAK entry pak `reps` times over background bg, with sound cues */
void sub_0596_0f41(int16_t pak, int16_t seq, int16_t bg, int16_t reps)
{
    uint8_t *res;
    uint8_t *pal = NULL;
    uint8_t *frames, *cues;
    int16_t haspal = 0;
    int16_t nfr, fr, rep, step = 0;
    int16_t cnt, next = 0, snd;
    uint8_t key;

    sub_0596_02ec();
    res = sub_0053_0413(DSPTR(0x18a0), pak);
    if (PU32(res) == 0x14) {                    /* 5-entry PAK: entry 4 = palette */
        haspal = 1;
        pal = sub_0053_052f(4, res);
    }
    frames = sub_0596_0b11(seq, res);
    nfr = P8(frames + 1);
    frames += 2;

    if (bg == 0)
        sub_118f_00f7();                                /* black */
    else if (bg < 0)
        sub_106a_0477(-bg, 0, 0, res);                  /* sprite of the animation */
    else if (bg < 0x7d00)
        sub_106a_0477(bg, 0, 0, DSPTR(0x3286));         /* room sprite */
    else if (bg == 0x7d00)
        sub_1036_000a(DSPTR(0x2d3c), DSPTR(0x2d38));    /* current screen */
    sub_1036_000a(DSPTR(0x2d38), DSPTR(0x3056));

    cues = DSPTR(0x32a2);
    cnt = DS16(0x328c);
    for (rep = 0; rep < reps; rep++) {
        if (DS16(0x328c) != 0) {                /* rewind sound cues: 3 bytes {frame, sfx, -} */
            DSPTR_SET(0x32a2, cues);
            cnt = DS16(0x328c);
            next = PS8(DSPTR(0x32a2));
            DSPTR_SET(0x32a2, DSPTR(0x32a2) + 1);
        }
        step = 0;
        for (fr = 0; fr < nfr; fr++) {
            sub_1036_000a(DSPTR(0x3056), DSPTR(0x2d38));
            step = sub_0596_0b28(res, frames, fr, step, 0, 0, nfr);
            sub_0053_1e2e();
            sub_0596_0687();
            if (haspal == 1) {
                haspal = 2;
                sub_0791_067f(pal);
            }
            sub_118f_0107();
            if (cnt != 0 && fr == next) {
                snd = PS8(DSPTR(0x32a2));
                DSPTR_SET(0x32a2, DSPTR(0x32a2) + 2);
                sub_0791_00e5(snd);
                cnt--;
                if (cnt != 0) {
                    next = PS8(DSPTR(0x32a2));
                    DSPTR_SET(0x32a2, DSPTR(0x32a2) + 1);
                }
            }
            sub_0053_05cd();
            key = DS8(0x2d4e);
            if (step != 0)
                fr--;                           /* morph in progress: stay on this frame */
            if (DS8(0x2d4e) == 0x1c) {          /* Enter: skip the animation */
                fr = nfr;
                rep = reps;
                if (DS16(0x17bc) != 0)
                    sub_0596_067e();
            }
            if (key == 1)                       /* Esc: quit dialog */
                sub_07fe_000e();
        }
    }

    sub_0f27_0061(res);
    if (haspal != 0)
        sub_0791_067f(DSPTR(0x3032));
    if (sub_03dd_0bbb() == 0) {                 /* next opcode is not another animation */
        if (DS16(0x17bc) != 0)
            sub_0596_067e();
        sub_0596_169f();
        sub_0596_0ec3();
    }
}

/* 0596:11cf - draws n random sparkles in rectangle (x1,y1)-(x2,y2) */
void sub_0596_11cf(int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t n)
{
    uint8_t *img = sub_0053_052f(1, DSPTR(0x3286));
    int16_t w = x2 - x1;
    int16_t h = y2 - y1;
    int16_t i, rx, ry, id;

    for (i = 0; i < n; i++) {
        /* original evaluation (push) order: y, then x, then image id */
        ry = sub_0053_05f6(h) + y1;
        rx = sub_0053_05f6(w) + x1;
        id = sub_0053_05f6(2) + 0x67;
        sub_0fe6_01d2(id, rx, ry, DSPTR(0x2d38), img);
    }
}

/* 0596:1252 - draws room contents in depth order (actors, projectiles, foreground pieces) */
void sub_0596_1252(void)
{
    uint8_t *pieces, *img, *dl;
    int16_t count, i, code, kind, idx;
    int16_t pk, w1, w2, w3;

    sub_0596_02d6();
    if (DS16(0xce) == 0x13 && DS16(0xa0) == 0)
        sub_0596_11cf(0x7c, 0x74, 0xa3, 0x7b, sub_0053_05f6(10) + 9);

    count = P8(DSPTR(0x3270));
    pieces = sub_0053_0564(0, DS16(0xce) * 2 + 1, DSPTR(0x3286));
    img = sub_0053_052f(1, DSPTR(0x3286));
    pieces += 2;
    dl = DSPTR(0x3270) + 3;

    for (i = 0; i < count; i++) {
        code = P8(dl);
        dl += 2;
        idx = code & 0xf;
        kind = code & 0xf0;
        if (kind == 0)
            sub_0596_0cca(idx);                 /* actor */
        if (kind == 0x20)
            sub_0596_08e0(idx);                 /* projectile */
        if (kind == 0x10) {                     /* next foreground piece of the room */
            pk = P8(pieces);
            pieces += 2;
            w1 = P16(pieces);
            pieces += 2;
            w2 = P16(pieces);
            pieces += 2;
            w3 = P16(pieces);
            pieces += 2;
            if (pk == 1)
                sub_0fe6_01d2(w1, w2, w3, DSPTR(0x2d38), img);
            else
                sub_106a_0477(w1, w2, w3, DSPTR(0x3286));
        }
    }

    if (DS16(0x329a) != 0 && DS16(0x12c) != 1)
        sub_02a7_0c92();
    sub_0596_171e();
}

/* 0596:13bb - draws backdrop layer `slot` (D??.PAK, with palette) then the room background sprite */
void sub_0596_13bb(int16_t room, int16_t slot)
{
    if (DS16(0xbbc + (uint16_t)(slot * 2)) > 0) {
        sub_0596_0001();
        sub_0fa2_0008(DSPTR(0x18a4), DS16(0xbbc + (uint16_t)(slot * 2)), DSPTR(0x17b4));
        if (PU32(DSPTR(0x17b4)) == 0x14) {     /* has a palette (entry 4) */
            sub_120e_0002(sub_0053_0994(DSPTR(0x17b4)), DSPTR(0x3032), 0x300);
            sub_120e_0002(DSPTR(0x3032), DSPTR(0x304c), 0x300);
            sub_0791_067f(DSPTR(0x3032));
        }
        sub_106a_0477(DS16(0xbbc + (uint16_t)((slot + 1) * 2)), 0, 0, DSPTR(0x17b4));
    }

    if (DS16(0xa0) != 0x63) {
        if (DS16(0xa0) == 0)
            sub_10f5_0028(0, 0, 0x140, 0x12, 0);
        sub_106a_0477(room * 2, 0, 0, DSPTR(0x3286));
    } else {
        sub_1036_000a(DSPTR(0x34c2), DSPTR(0x2d38));
        sub_0a86_0325();
    }
}

/* 0596:14c9 - draws room background with main backdrop (slot 0) */
void sub_0596_14c9(int16_t room)
{
    sub_0596_13bb(room, 0);
}

/* 0596:14d9 - draws the room foreground sprite room*2+1 if it is a real sprite */
void sub_0596_14d9(int16_t room)
{
    uint8_t *s = sub_0053_0564(0, room * 2 + 1, DSPTR(0x3286));

    if (P8(s + 1) != 0)
        sub_106a_0477(room * 2 + 1, 0, 0, DSPTR(0x3286));
}

/* 0596:1543 - clears the draw buffer and draws a room (backdrop slot, background, foreground) */
void sub_0596_1543(int16_t room, int16_t slot)
{
    sub_118f_00f7();
    sub_0596_13bb(room, slot);
    sub_0596_14d9(room);
}

/* 0596:1564 - builds the parallax background in DSPTR(0x3056) from scaled-down far rooms */
void sub_0596_1564(void)
{
    uint16_t ofs = (uint16_t)(DS16(0xbc8) * 0x140 + DS16(0xbc6));

    sub_1036_000a(DSPTR(0x2d38), DSPTR(0x3056));
    sub_0596_1543(DS16(0xbc0), 3);
    if (DS16(0xc6) == 2) {
        sub_0dc5_0551(DSPTR(0x2d38) + (uint16_t)(DS16(0x303c) * 0x140), DSPTR(0x3056) + ofs);
        ofs = (uint16_t)(DS16(0xbd2) * 0x140 + DS16(0xbd0));
        sub_0596_1543(DS16(0xbca), 8);
    }
    sub_0dc5_051b(DSPTR(0x2d38) + (uint16_t)(DS16(0x303c) * 0x140), DSPTR(0x3056) + ofs);
    sub_118f_00f7();
}

/* 0596:1614 - installs the requested 16-colour sub-palette (colours 0xa0..0xaf) */
void sub_0596_1614(void)
{
    if (DS8(0xd3) == DS8(0xd4))
        return;
    DS8(0xd3) = DS8(0xd4);
    sub_120e_0002(DSPTR(0x301c) + (uint16_t)(DS8(0xd3) * 16 * 3), DSPTR(0x3032) + 0x1e0, 0x30);
    sub_120e_0002(DSPTR(0x301c) + (uint16_t)(DS8(0xd3) * 16 * 3), DSPTR(0x304c) + 0x1e0, 0x30);
    sub_0e3f_0104();
    sub_118f_01ab(DSPTR(0x3032) + 0x1e0, 0xa0, 0x10);
}

/* 0596:169f - rebuilds the room's static background into DSPTR(0x3056) */
void sub_0596_169f(void)
{
    sub_118f_00f7();
    if (DS16(0xc6) != 0)
        sub_0596_1564();
    sub_0596_14c9(DS16(0xce));
    sub_0596_14d9(DS16(0xce));
    sub_0596_02ec();
    sub_0596_0e96();
    sub_0596_02d6();
    if (DS16(0xc6) != 0)
        sub_0dc5_020d(DSPTR(0x2d38), DSPTR(0x3056));    /* over the parallax, colour 0 transparent */
    else
        sub_1036_000a(DSPTR(0x2d38), DSPTR(0x3056));
    if (DS16(0xee) != 0)
        sub_0dc5_022e();
    sub_0a86_0000();
    sub_0596_1614();
}

/* 0596:171e - draws markers for exit zones of type 3 on the bottom edge of the play area */
void sub_0596_171e(void)
{
    uint8_t *z = DSADDR(0x752);
    int16_t i, xa, xb, ya, yb;

    for (i = 0; i < DS16(0xa8); i++) {
        xa = P16(z + 2);
        xb = P16(z + 4);
        if (P8(z) == 3) {
            ya = yb = 0xab;
            sub_1040_0006(xa, ya - 1, xb, yb - 1, 1);
            sub_1040_0006(xa, ya, xb, yb, 1);
            sub_116d_000c(xa, ya - 1, xa + 1, ya, 9);
            sub_116d_000c(xb - 1, yb - 1, xb, yb, 9);
        }
        z += 6;
    }
}

/* 0596:17cd - blanks the play area in the draw buffer and on screen */
void sub_0596_17cd(void)
{
    sub_0596_0eee();
    sub_02a7_0b4a(0, 0, 0x140, 0xab);
}
