/*
 * Segment 02a7 - interface: status messages, inventory/status screen (with ECG
 * heartbeat), hotspot look/take/use actions, bottom action bar.
 *
 * Item states: DS:0xfbc s16[256] (0 none, 1 owned, 2 equipped, 3 used this turn)
 *   cursor [0xb8], equipped [0xba], first visible [0xbc], owned count [0xbe].
 * Hotspots: DS:0xa18, 8 bytes each, count [0xf0]:
 *   +0 u16 item/text index, +2 u8 described flag, +3 u8 type (0 = takeable),
 *   +4 s16 x, +6 s16 y.
 * ECG trace: DS:0x30ae, 50 points of 8 bytes: +0 x, +2 y, +4 dy, +6 colour.
 * Action bar: state [0x3266] 0 closed / 1 slide up / 2 open / 3 flash / 4 slide down.
 */
#include "../core/mem.h"
#include "../platform/platform.h"
#include "protos.h"

#define ITEM(i)    DS16(0xfbc + (uint16_t)((i) * 2))
#define GLYPHS     DSPTR(0x3062)                 /* interface glyph bank */
#define DRAWBUF    DSPTR(0x2d38)

/* 02a7:0006 - posts a status message (kind, item) for 20 frames */
void sub_02a7_0006(int16_t kind, int16_t item)
{
    DS16(0x104) = 0x14;
    DS16(0x106) = kind;
    DS16(0x108) = item;
}

/* 02a7:001d - draws a bevelled box (light top/left, dark bottom/right) filled with color */
void sub_02a7_001d(int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t color)
{
    sub_1040_0006(x1, y1, x2, y1, 0x9f);
    sub_1040_0006(x1, y1, x1, y2, 0x9f);
    sub_1040_0006(x2, y1, x2, y2, 0x98);
    sub_1040_0006(x1, y2, x2, y2, 0x98);
    sub_10f5_0028((int16_t)(x1 + 1), (int16_t)(y1 + 1), (int16_t)(x2 - 1), (int16_t)(y2 - 1),
                  (uint8_t)color);
}

/* 02a7:0098 - draws the status message box centred at x=160 (message text + item name); decrements its timer */
void sub_02a7_0098(void)
{
    int16_t w_item = 0, w_msg, half, x;
    uint8_t *s_item = NULL, *s_msg;

    DS16(0x104)--;
    sub_0596_02a0(0);
    if (DS16(0x106) < 4) {
        s_item = sub_03dd_165d(DS16(0x108), DSPTR(0x3290));
        w_item = sub_1172_018c(s_item);
    }
    s_msg = sub_03dd_165d((int16_t)(DS16(0x106) + 0x36), DSPTR(0x3290));
    w_msg = sub_1172_018c(s_msg);

    half = (int16_t)(w_item + w_msg) >> 1;
    x = (int16_t)(0xa0 - half);
    sub_02a7_001d((int16_t)(x - 4), 0xa0, (int16_t)(half + 0xa4), 0xab, 0x9c);
    sub_1172_0012(x, 0xa2, DRAWBUF, s_msg);
    if (DS16(0x106) < 4)
        sub_1172_0012((int16_t)(x + w_msg + 2), 0xa2, DRAWBUF, s_item);
}

/* 02a7:0181 - inventory open: counts owned items, used->equipped, fixes equipped item, cursor = equipped */
void sub_02a7_0181(void)
{
    int16_t i;

    DS16(0xbe) = 0;
    for (i = 0; i < 0x100; i++) {
        if (ITEM(i) > 0)
            DS16(0xbe)++;
        if (ITEM(i) == 3)
            ITEM(i) = 2;
    }
    if (ITEM(DS16(0xba)) == 0) {
        DS16(0xb8) = 0;
        DS16(0xba) = sub_02a7_09fe();
    }
    DS16(0xb8) = DS16(0xba);
}

/* 02a7:01e8 - draws up to 12 inventory item names from [0xbc]; cursor highlighted with pointer, equipped in 0x4d */
void sub_02a7_01e8(void)
{
    int16_t n = 0, y = 0x2c, i, color;

    for (i = 0; i < 0x100; i++) {
        if (ITEM(i) > 0 && i >= DS16(0xbc)) {
            color = 0x97;
            if (DS16(0xb8) == i) {
                color = 0x9f;
                sub_106a_0477(0xc, 0xf, (int16_t)(y + 8), GLYPHS);
            }
            if (ITEM(i) == 2)
                color = 0x4d;
            sub_0596_02a0(color);
            sub_1172_0012(0x1e, y, DRAWBUF, sub_03dd_165d(i, DSPTR(0x3290)));
            n++;
            y += 0xa;
        }
        if (n >= 0xc)
            break;
    }
}

/* 02a7:02a9 - initialises the ECG trace: beat phase/delay and 50 points in a flat line */
void sub_02a7_02a9(void)
{
    uint8_t *p;
    int16_t i;

    DS16(0x192a) = 0;
    DS16(0x1928) = (int16_t)(sub_0053_05f6(8) + 1);
    p = DSADDR(0x30ae);
    P16(p + 4) = 0;
    for (i = 0; i < 0x32; i++) {
        P16(p) = (int16_t)(0x109 - i);
        P16(p + 2) = 0x14;
        P16(p + 6) = (int16_t)(0x7f - i / 7);
        P16(p + 4) = 0;
        p += 8;
    }
}

/* 02a7:0319 - ECG segment: if x >= pt.x draws a line (x,y)-(pt.x,pt.y) in the point's colour */
void sub_02a7_0319(uint8_t *pt, int16_t x, int16_t y)
{
    int16_t px = P16(pt), py = P16(pt + 2);

    if (x >= px)
        sub_1040_0006(x, y, px, py, (uint8_t)P16(pt + 6));
}

/* 02a7:0351 - ECG draw: restores head column from back buffer, draws/shifts the 50-point trail on screen, plots head */
void sub_02a7_0351(uint8_t *pts)
{
    int16_t hx, hy, px, py, tx, ty, i;
    uint8_t *save;

    hx = px = P16(pts);
    hy = py = P16(pts + 2);
    sub_02a7_0b4a(px, 4, (int16_t)(px + 1), 0x24);
    pts += 8;

    save = DSPTR(0x2d38);
    DSPTR_SET(0x2d38, DSPTR(0x2d3c));          /* draw straight onto the screen */
    for (i = 1; i < 0x32; i++) {
        sub_02a7_0319(pts, px, py);
        tx = P16(pts);
        ty = P16(pts + 2);
        P16(pts) = px;                          /* shift history by one point */
        P16(pts + 2) = py;
        px = tx;
        py = ty;
        pts += 8;
    }
    sub_1100_00c0(0x9f);
    sub_1100_000b(hx, hy);
    DSPTR_SET(0x2d38, save);
}

/* 02a7:0410 - ECG step: heartbeat state machine (rest, up, down, up), advances head x (315 wraps to 215) and draws */
void sub_02a7_0410(void)
{
    uint8_t *p = DSADDR(0x30ae);
    int16_t amp;

    if (DS16(0x1928) != 0)
        DS16(0x1928)--;
    if (DS16(0x1928) == 0) {
        DS16(0x32ac) = 1;
        DS16(0x192a)++;
        if (DS16(0x192a) > 3) {
            DS16(0x192a) = 0;
            DS16(0x1928) = (int16_t)(sub_0053_05f6(5) + DS16(0x192c));
            DS16(0x32ac) = 1;
            P16(p + 4) = 0;
            P16(p + 2) = 0x14;
        }
        if (DS16(0x192a) == 1) {
            amp = (int16_t)(sub_0053_05f6(4) + 5);
            DS16(0x32ae) = amp;
            DS16(0x1928) = 2;
            P16(p + 4) = (int16_t)-amp;
        }
        if (DS16(0x192a) == 2) {
            DS16(0x1928) = 4;
            P16(p + 4) = DS16(0x32ae);
        }
        if (DS16(0x192a) == 3) {
            DS16(0x1928) = 2;
            P16(p + 4) = (int16_t)-DS16(0x32ae);
        }
    }
    P16(p) += DS16(0x32ac);
    if (P16(p) > 0x13b)
        P16(p) = 0xd7;
    P16(p + 2) += P16(p + 4);
    sub_02a7_0351(p);
}

/* 02a7:04f1 - draws status-screen numbers: play time hhh:mm:ss + 1/20s from [0x1fdc], gold [0x11c6], completion count */
void sub_02a7_04f1(void)
{
    int16_t v, a, b, c;

    sub_0596_02a0(0x9a);

    /* hours: (T/1200)/60 % 999 as 3 glyphs (0x17 + digit) */
    v = (int16_t)(DS32(0x1fdc) / 1200);
    v = (int16_t)(v / 60);
    v = (int16_t)(v % 999);
    c = (int16_t)(v / 100 + 0x17);
    a = (int16_t)(v / 10 + 0x17);      /* sic: not (v/10)%10 in the original */
    b = (int16_t)(v % 10 + 0x17);
    sub_106a_0477((uint16_t)c, 0x8c, 0x1e, GLYPHS);
    sub_106a_0477((uint16_t)a, 0x90, 0x1e, GLYPHS);
    sub_106a_0477((uint16_t)b, 0x94, 0x1e, GLYPHS);

    /* minutes: (T/1200) % 60 */
    v = (int16_t)((DS32(0x1fdc) / 1200) % 60);
    a = (int16_t)(v / 10 + 0x17);
    b = (int16_t)(v % 10 + 0x17);
    sub_106a_0477((uint16_t)a, 0x9e, 0x1e, GLYPHS);
    sub_106a_0477((uint16_t)b, 0xa2, 0x1e, GLYPHS);

    /* seconds: (T/20) % 60 */
    v = (int16_t)((DS32(0x1fdc) / 20) % 60);
    a = (int16_t)(v / 10 + 0x17);
    b = (int16_t)(v % 10 + 0x17);
    sub_106a_0477((uint16_t)a, 0xb0, 0x1e, GLYPHS);
    sub_106a_0477((uint16_t)b, 0xb4, 0x1e, GLYPHS);

    /* twentieths: T % 20, small glyphs 0x21 + digit */
    v = (int16_t)(DS32(0x1fdc) % 20);
    a = (int16_t)(v / 10 + 0x21);
    b = (int16_t)(v % 10 + 0x21);
    sub_106a_0477((uint16_t)a, 0xbc, 0x1e, GLYPHS);
    sub_106a_0477((uint16_t)b, 0xc0, 0x1e, GLYPHS);

    /* gold */
    v = DS16(0x11c6);
    a = (int16_t)(v / 10 + 0x21);
    b = (int16_t)(v % 10 + 0x21);
    sub_106a_0477((uint16_t)a, 0x1c, 0xf, GLYPHS);
    sub_106a_0477((uint16_t)b, 0x20, 0xf, GLYPHS);

    /* completion (number of set progress flags) */
    v = sub_0c05_020c();
    a = (int16_t)(v / 10 + 0x21);
    b = (int16_t)(v % 10 + 0x21);
    sub_106a_0477((uint16_t)a, 0x1c, 0x1e, GLYPHS);
    sub_106a_0477((uint16_t)b, 0x20, 0x1e, GLYPHS);
}

/* 02a7:07c8 - draws value clamped to 0..999 as three digit glyphs (0xd + digit) at x, x+0x11, x+0x22 */
void sub_02a7_07c8(int16_t value, int16_t x, int16_t y)
{
    int16_t v = value, d;

    if (v < 0)
        v = 0;
    if (v > 999)
        v = 999;
    d = (int16_t)(v / 100);
    sub_106a_0477((uint16_t)(d + 0xd), x, y, GLYPHS);
    d = (int16_t)((v / 10) % 10);
    sub_106a_0477((uint16_t)(d + 0xd), (int16_t)(x + 0x11), y, GLYPHS);
    d = (int16_t)(v % 10);
    sub_106a_0477((uint16_t)(d + 0xd), (int16_t)(x + 0x22), y, GLYPHS);
}

/* 02a7:0863 - redraws the player HP (0..99, 2 glyphs) in the bottom bar of the draw buffer */
void sub_02a7_0863(void)
{
    int16_t hp;

    sub_0596_02ec();
    sub_10f5_0028(0x106, 0xb4, 0x11c, 0xbc, 0x1d);
    hp = DS16(0x144);
    if (hp < 0)
        hp = 0;
    if (hp > 0x63)
        hp = 0x63;
    sub_106a_0477((uint16_t)(hp / 10 + 0x2c), 0x10c, 0xbb, GLYPHS);
    sub_106a_0477((uint16_t)(hp % 10 + 0x2c), 0x116, 0xbb, GLYPHS);
    sub_0596_02d6();
}

/* 02a7:08f3 - redraws the player HP directly on the screen */
void sub_02a7_08f3(void)
{
    uint8_t *save = DSPTR(0x2d38);

    DSPTR_SET(0x2d38, DSPTR(0x2d3c));
    sub_02a7_0863();
    DSPTR_SET(0x2d38, save);
}

/* 02a7:0920 - draws the status screen background, labels (Time, Name, gp, %, Don JONZ) and numbers */
void sub_02a7_0920(void)
{
    sub_0596_02ec();
    sub_0596_02a0(1);
    sub_106a_0477(0xa, 0, 0, GLYPHS);
    sub_1172_0012(0x67, 0x18, DRAWBUF, DSADDR(0x192e));   /* "Time" */
    sub_1172_0012(0x66, 9, DRAWBUF, DSADDR(0x1933));      /* "Name" */
    sub_1172_0012(9, 8, DRAWBUF, DSADDR(0x1938));         /* "gp" */
    sub_1172_0012(0xa, 0x19, DRAWBUF, DSADDR(0x193b));    /* "%" */
    sub_0596_02a0(0x7e);
    sub_1172_0012(0x90, 8, DRAWBUF, DSADDR(0x193d));      /* "Don JONZ" */
    sub_02a7_04f1();
}

/* 02a7:09d0 - returns the previous owned item before the cursor, or the cursor if none */
int16_t sub_02a7_09d0(void)
{
    int16_t i;

    for (i = (int16_t)(DS16(0xb8) - 1); i >= 0; i--) {
        if (ITEM(i) > 0)
            return i;
    }
    return DS16(0xb8);
}

/* 02a7:09fe - returns the next owned item after the cursor, or the cursor if none */
int16_t sub_02a7_09fe(void)
{
    int16_t i;

    for (i = (int16_t)(DS16(0xb8) + 1); i < 0x100; i++) {
        if (ITEM(i) > 0)
            return i;
    }
    return DS16(0xb8);
}

/* 02a7:0a2d - sets first visible inventory item [0xbc] to 5 owned items before the cursor */
void sub_02a7_0a2d(void)
{
    int16_t save = DS16(0xb8), i;

    for (i = 0; i < 4; i++)
        DS16(0xb8) = sub_02a7_09d0();
    DS16(0xbc) = sub_02a7_09d0();
    DS16(0xb8) = save;
}

/* 02a7:0a5f - inventory cursor to the previous owned item */
void sub_02a7_0a5f(void)
{
    int16_t v = sub_02a7_09d0();

    DS16(0xb8) = v;
    DS16(0xbc) = v;
}

/* 02a7:0a6a - inventory cursor to the next owned item */
void sub_02a7_0a6a(void)
{
    int16_t v = sub_02a7_09fe();

    DS16(0xb8) = v;
    DS16(0xbc) = v;
}

/* 02a7:0a75 - inventory input: up/down moves cursor, Enter/fire equips and exits; Tab/Enter/Esc exit (returns 1) */
int16_t sub_02a7_0a75(void)
{
    uint8_t joy = DS8(0x2d4a);
    uint8_t key = DS8(0x2d4e);

    if (DS16(0xbe) > 0) {
        /* port: the original moved the selection on every loop pass while a key was held;
           its speed came from the CPU redrawing the screen (~10 passes/s on a 386). Move
           once per press with keyboard-style auto-repeat instead, so the list is usable. */
        static uint8_t held;
        static uint32_t next_ms;
        uint8_t dir = joy & 0xf;
        int step = 0;
        if (dir != held) {
            held = dir;
            step = dir != 0;
            next_ms = plat_ms() + 400;
        } else if (dir && plat_ms() >= next_ms) {
            step = 1;
            next_ms = plat_ms() + 140;
        }
        if (step && dir == 1)
            sub_02a7_0a5f();
        if (step && dir == 2)
            sub_02a7_0a6a();
        if (key == 0x1c || (joy & 0x80)) {
            if (ITEM(DS16(0xba)) > 1)
                ITEM(DS16(0xba)) = 1;
            if (ITEM(DS16(0xb8)) > 0) {
                DS16(0xba) = DS16(0xb8);
                ITEM(DS16(0xba)) = 2;
            }
            return 1;
        }
    }
    if (DS8(0x2d4e) == 0xf || DS8(0x2d4e) == 0x1c || DS8(0x2d4e) == 1)
        return 1;
    return 0;
}

/* 02a7:0b4a - copies rectangle (x1,y1)-(x2,y2) inclusive from the draw buffer to the screen */
void sub_02a7_0b4a(int16_t x1, int16_t y1, int16_t x2, int16_t y2)
{
    sub_103b_0004(DSPTR(0x2d38), x1, y1, DSPTR(0x2d3c), x1, y1,
                  (uint16_t)(x2 + 1 - x1), (uint16_t)(y2 + 1 - y1));
}

/* 02a7:0b83 - blits the status screen's dynamic areas (time, gold, item list) to the screen */
void sub_02a7_0b83(void)
{
    sub_02a7_0b4a(0x82, 0x16, 0xcd, 0x20);
    sub_02a7_0b4a(9, 9, 0x20, 0xf);
    sub_02a7_0b4a(4, 0x2b, 0x13c, 0xa3);
}

/* 02a7:0bb5 - inventory/status screen: dissolve in, loop (time, input, list, ECG) until exit, restore game view */
void sub_02a7_0bb5(void)
{
    int16_t done = 0;

    while (DS8(0x2d4e) != 0)
        plat_yield();
    sub_0596_0eee();
    sub_0dc5_017d();
    sub_02a7_0181();
    sub_02a7_02a9();
    sub_02a7_0920();
    sub_0791_067f(sub_0053_0994(DSPTR(0x3062)));
    sub_0dc5_017d();

    while (done == 0) {
        DS32(0x1fdc) += 1;                     /* play time */
        DS16(0x192c) = (int16_t)(DS16(0x144) >> 2);
        if (DS16(0x192c) < 1)
            DS16(0x192c) = 1;
        if (DS16(0x192c) > 0x1e)
            DS16(0x192c) = 0x1e;
        done = sub_02a7_0a75();
        sub_02a7_0a2d();
        sub_02a7_0920();
        sub_02a7_01e8();
        if (DS8(0x2d4e) == 0x2f)               /* 'V': "Version Interne 14-04-92" */
            sub_1172_0012(0x64, 0x64, DRAWBUF, DSADDR(0x18b4));
        sub_02a7_0b83();
        sub_02a7_0410();
        plat_frame_limit(20);                  /* port: pacing (the original ran at CPU speed) */
    }

    while (DS8(0x2d4e) != 0)
        plat_yield();
    sub_0596_0eee();
    sub_0dc5_017d();
    sub_0791_067f(DSPTR(0x3032));
    sub_07fe_0f88();
}

/* 02a7:0c92 - while looking, draws a jittered dashed beam from the player's head to the look target */
void sub_02a7_0c92(void)
{
    int16_t x, y, tx, ty;

    if (DS16(0x329a) == 0)
        return;
    x = DS16(0x11a);
    y = (int16_t)(DS16(0x11c) - 0x23);
    if (DS16(0x3060) == 1) y--;
    if (DS16(0x3060) == 2) x += 4;
    if (DS16(0x3060) == 3) y++;
    if (DS16(0x3060) == 4) x -= 5;
    /* original evaluates the y jitter first, then x */
    ty = (int16_t)(sub_0053_05f6(3) + DS16(0x323e) - 1);
    tx = (int16_t)(sub_0053_05f6(3) + DS16(0x30a8) - 1);
    sub_0dc5_058d(x, y, tx, ty, 0x9f);
}

/* 02a7:0d09 - shows description text #item of text bank `texts` in the dialog box for `timer` */
void sub_02a7_0d09(int16_t item, int16_t timer, uint8_t *texts)
{
    DS16(0x3298) = 0;
    DS16(0x325e) = 0x19;
    DS16(0x326c) = item;
    sub_0596_0405(sub_03dd_165d(item, texts));
    DS16(0x328a) = timer;
    DS16(0x32aa) = timer;
    DS16(0x17bc) = 1;
}

/* 02a7:0d46 - fills rect (x1,y1,x2,y2) with the zone in front of the player, extended by reach (widen sideways facing up) */
void sub_02a7_0d46(uint8_t *rect, int16_t widen, int16_t reach)
{
    int16_t x1, y1, x2, y2;

    x1 = (int16_t)(DS16(0x11a) - DS8(0x13d) - 8);
    y1 = (int16_t)(DS16(0x11c) - DS8(0x13e) - 8);
    x2 = (int16_t)(DS8(0x13d) + DS16(0x11a) + 8);
    y2 = (int16_t)(DS16(0x11c) + 8);

    if (DS16(0x12c) == 1) {
        y1 -= reach;
        y2 -= 0xa;
        x1 -= widen;
        x2 += widen;
    }
    if (DS16(0x12c) == 2) {
        x2 += reach;
        x1 += 0xa;
    }
    if (DS16(0x12c) == 3) {
        y2 += reach;
        y1 += 0xa;
    }
    if (DS16(0x12c) == 4) {
        x1 -= reach;
        x2 -= 0xa;
    }
    if (x1 < 0)
        x1 = 0;
    if (x2 > 0x13f)
        x2 = 0x13f;
    if (y1 < 0)
        y1 = 0;
    if (y2 > 0xab)
        y2 = 0xab;

    P16(rect) = x1;
    P16(rect + 2) = y1;
    P16(rect + 4) = x2;
    P16(rect + 6) = y2;
}

/* 02a7:0e21 - auto-look: runs the look countdown, or describes an undescribed hotspot in front of the player */
void sub_02a7_0e21(void)
{
    int16_t dir = DS16(0x12c);
    int16_t rect[4];
    int16_t h;
    uint8_t *e, *texts;

    if (DS16(0x329a) != 0) {
        DS16(0x329a)--;
        if (dir == DS16(0x3060)) {
            /* stop when the player has walked up to the target */
            if (dir == 1 && DS16(0x11c) <= DS16(0x323e))
                DS16(0x329a) = 0;
            if (dir == 2 && DS16(0x11a) >= DS16(0x30a8))
                DS16(0x329a) = 0;
            if (dir == 3 && DS16(0x11c) >= DS16(0x323e))
                DS16(0x329a) = 0;
            if (dir == 4 && DS16(0x11a) <= DS16(0x30a8))
                DS16(0x329a) = 0;
        } else {
            DS16(0x329a) = 0;
        }
        return;
    }

    if (DS16(0x3240) != 0 || DS16(0xc0) == 0xf || DS16(0x17bc) != 0)
        return;

    sub_02a7_0d46((uint8_t *)rect, 0, 0x32);
    h = sub_0714_0559((uint8_t *)rect);
    if (h == 0)
        return;
    h &= 0xff;
    e = DSADDR(0xa18 + (uint16_t)(h << 3));
    if (P8(e + 2) != 0)
        return;

    P8(e + 2) = 1;
    DS16(0x329a) = 0x14;
    DS16(0x3060) = dir;
    DS16(0x30a8) = P16(e + 4);
    DS16(0x323e) = (int16_t)(P16(e + 6) - 6);
    if (P8(e + 3) == 0)
        texts = DSPTR(0x3290);
    else
        texts = DSPTR(0x325a);
    sub_02a7_0d09(P16(e), DS16(0x329a), texts);
}

/* 02a7:0f54 - clears the described flag of every hotspot whose item is not owned */
void sub_02a7_0f54(void)
{
    uint8_t *e = DSADDR(0xa18);
    int16_t i;

    for (i = 0; i < DS16(0xf0); i++, e += 8) {
        if (ITEM(PU16(e)) == 0)
            P8(e + 2) = 0;
    }
}

/* 02a7:0f92 - action Take: picks up the takeable hotspot item in front of the player (messages 2/4/5) */
void sub_02a7_0f92(void)
{
    int16_t rect[4];
    int16_t h, it;
    uint8_t *e;

    if (DS16(0xa0) == -1)
        return;
    sub_02a7_0d46((uint8_t *)rect, 0, 0x32);
    h = sub_0714_0559((uint8_t *)rect);
    if (h == 0) {
        sub_02a7_0006(5, 0);                   /* nothing here */
        return;
    }
    h &= 0xff;
    e = DSADDR(0xa18 + (uint16_t)(h << 3));
    if (P8(e + 3) == 0) {
        it = P16(e);
        DS16(0xbc) = it;
        DS16(0xb8) = it;
        ITEM(it) = 1;
        sub_02a7_0d09(it, 0xa, DSPTR(0x3290));
        DS16(0xbe)++;
        sub_02a7_0006(2, DS16(0xb8));          /* "taken" */
    } else {
        sub_02a7_0006(4, 0);                   /* can't take */
    }
}

/* 02a7:1038 - action Speak: sets the speak request flag [0x327a] */
void sub_02a7_1038(void)
{
    DS16(0x327a) = 1;
}

/* 02a7:103f - action Inventory: opens the inventory/status screen */
void sub_02a7_103f(void)
{
    sub_02a7_0bb5();
}

/* 02a7:1044 - action Look: re-enables hotspot descriptions and looks; message 6 if nothing found */
void sub_02a7_1044(void)
{
    if (DS16(0xa0) == -1)
        return;
    sub_02a7_0f54();
    sub_02a7_0e21();
    if (DS16(0x329a) == 0)
        sub_02a7_0006(6, 0);
}

/* 02a7:1066 - action Use: marks the equipped item as used (state 3) and posts message 3 */
void sub_02a7_1066(void)
{
    if (ITEM(DS16(0xba)) > 1) {
        ITEM(DS16(0xba)) = 3;
        sub_02a7_0006(3, DS16(0xba));
    }
}

/* 02a7:108d - action Disk: opens the options / load-save panel */
void sub_02a7_108d(void)
{
    sub_07fe_0ff1();
}

/* 02a7:1093 - opens the action bar (state 1, hidden y, cursor on pending action) if the player may act */
void sub_02a7_1093(void)
{
    if (sub_0053_1cb8() == 0) {
        DS16(0x3266) = 0;
        DS16(0x17aa) = -1;
        return;
    }
    DS16(0x3266) = 1;
    DS16(0x17a8) = 0xd4;
    if (DS16(0x17aa) != -1) {
        DS16(0x17a6) = DS16(0x179a + (uint16_t)(DS16(0x17aa) * 2));
        DS16(0x17b0) = DS16(0x17aa);
    } else {
        DS16(0x17a6) = DS16(0x179a);
        DS16(0x17b0) = 0;
    }
}

/* 02a7:10df - closes the action bar (state 0) */
void sub_02a7_10df(void)
{
    DS16(0x3266) = 0;
}

/* 02a7:10e6 - action bar key handler: T/U/L/S/I/D hotkeys, Enter, Tab open/close, Esc quit prompt */
void sub_02a7_10e6(uint8_t key)
{
    int16_t prev = DS16(0x17aa);

    switch (key) {
    case 0x14: DS16(0x17aa) = 0; break;        /* T: take */
    case 0x16: DS16(0x17aa) = 1; break;        /* U: use */
    case 0x26: DS16(0x17aa) = 2; break;        /* L: look */
    case 0x1f: DS16(0x17aa) = 3; break;        /* S: speak */
    case 0x17: DS16(0x17aa) = 4; break;        /* I: inventory */
    case 0x20: DS16(0x17aa) = 5; break;        /* D: disk */
    case 0x1c:                                 /* Enter */
        if (DS16(0x3266) == 2)
            DS16(0x17aa) = DS16(0x17b0);
        break;
    case 0x0f:                                 /* Tab */
        if (DS16(0x3266) == 0)
            sub_02a7_1093();
        if (DS16(0x3266) == 2)
            DS16(0x3266) = 4;
        while (DS8(0x2d4e) == 0xf)
            plat_yield();
        break;
    case 0x01:                                 /* Esc */
        sub_07fe_000e();
        break;
    default:
        break;
    }

    if (DS16(0x17aa) != -1) {
        if (DS16(0x3266) == 0) {
            sub_02a7_1093();
        } else if (DS16(0x3266) == 2 && prev == -1) {
            DS16(0x17a6) = DS16(0x179a + (uint16_t)(DS16(0x17aa) * 2));
            DS16(0x17b0) = DS16(0x17aa);
        } else {
            DS16(0x17aa) = prev;
        }
    }
}

/* 02a7:11ed - slides the action bar cursor toward its icon; when there, joystick left/right changes the action */
void sub_02a7_11ed(void)
{
    int16_t t = DS16(0x179a + (uint16_t)(DS16(0x17b0) * 2));

    if (DS16(0x17a6) < t) {
        DS16(0x17a6) += 2;
        if (DS16(0x17a6) > t)
            DS16(0x17a6) = t;
    }
    if (DS16(0x17a6) > t) {
        DS16(0x17a6) -= 2;
        if (DS16(0x17a6) < t)
            DS16(0x17a6) = t;
    }
    if (DS16(0x17a6) == t) {
        if ((DS8(0x2d4a) & 4) && DS16(0x17b0) > 0)
            DS16(0x17b0)--;
        if ((DS8(0x2d4a) & 8) && DS16(0x17b0) < 5)
            DS16(0x17b0)++;
    }
}

/* 02a7:1264 - waits for vertical retrace and blits the bottom panel (0,0xac)-(0x140,0xc7) to the screen */
void sub_02a7_1264(void)
{
    sub_118f_00de();
    /* TODO(port): x2=0x140 copies 321 bytes per row; the last row ends 1 byte past a 64000-byte screen */
    sub_02a7_0b4a(0, 0xac, 0x140, 0xc7);
}

/* 02a7:127c - action bar modal loop (slide up, choose, flash, slide down), then runs the chosen action */
void sub_02a7_127c(void)
{
    uint8_t key;

    sub_02a7_1093();
    while (DS16(0x3266) != 0) {
        key = DS8(0x2d4e);
        if (key == 0 && (DS8(0x2d4a) & 0x80))
            key = 0x1c;                        /* fire = Enter */
        sub_02a7_10e6(key);

        switch (DS16(0x3266)) {
        case 1:                                /* sliding up */
            if (DS16(0x17a8) > 0xb8)
                DS16(0x17a8) -= 2;
            else
                DS16(0x3266) = 2;
            break;
        case 2:                                /* open: choose */
            if (DS16(0x17aa) == -1) {
                sub_02a7_11ed();
            } else {
                DS16(0x17ae) = 5;
                DS16(0x3266) = 3;
            }
            break;
        case 3:                                /* flash the chosen icon */
            if (DS16(0x17ae) != 0)
                DS16(0x17ae)--;
            if (DS16(0x17ae) == 0)
                DS16(0x3266) = 4;
            break;
        case 4:                                /* sliding down */
            if (DS16(0x17a8) < 0xd4)
                DS16(0x17a8) += 2;
            else
                sub_02a7_10df();
            break;
        default:
            break;
        }

        sub_0596_02ec();
        sub_0596_0e96();
        sub_0596_0e28();
        sub_02a7_0863();
        sub_02a7_1264();                       /* vsync + present panel */
    }

    if (DS16(0x17aa) != -1)
        DSFN(void (*)(void), 0x1782 + (uint16_t)(DS16(0x17aa) * 4))();
    DS16(0x17aa) = -1;
    sub_0596_02d6();
}
