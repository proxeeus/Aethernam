/*
 * Intro segment 003a: main() and the scenes of the intro.
 *
 * DGROUP (intro, seg 0x6ed) variables used here:
 *   0x094+4*lang fptr text file name ("t.cc4","e.cc4","s.cc4","d.cc4","i.cc4")
 *   0x0a8[c-'A'] glyph widths (also indexed as 0x67+c)
 *   0x0d0 fptr "PLANET ETERNAM", 0x0d4 fptr "DATE 2815", 0x0d8 fptr "mus" (-> "mub")
 *   0x148 fptr draw buffer (back buffer), 0x14c fptr screen, 0x15e current key (scancode, 0 = none)
 *   0xbd6 fptr shrunk-sprite buffer (5000), 0xbda fptr SFX bank (MUS #1), 0xbde fptr palette
 *   0xbe2 fptr font (INTRO #1), 0xbe6 fptr song, 0xbea fptr sprite bank, 0xbee fptr credits text
 *   0xbf2 fptr 64000-byte picture buffer, 0xbf6 language, 0xbf8 fptr SLIDE / EARTH (INTRO #4 / #0)
 *   0xbfc sound type (launcher +7), 0xbfe fptr SPLASH (INTRO #3), 0xc02 fptr TATOU (INTRO #5)
 */
#include "../core/mem.h"
#include "../platform/platform.h"
#include "intro_protos.h"
#include <string.h>

#define KEY_ESC   0x01
#define KEY_ENTER 0x1c

/* 003a:000d  loads text block `idx` of the language file `name` (dword offset table at the
 * start of the file) into dest (0x3de bytes) */
void intro_003a_000d(uint8_t *name, int16_t idx, uint8_t *dest)
{
    uint8_t tab[0x196];                     /* [bp-0x196]: offset table */
    int16_t first;
    FILE *fh = intro_032c_0035((const char *)name, 0x3ed);
    if (!fh) return;  /* TODO(port): the original goes on with handle 0 (DOS reads fail) */
    memset(tab, 0, sizeof tab);
    intro_032c_0195(fh, tab, 4);
    first = P16(tab);
    intro_032c_0195(fh, tab + 4, (int32_t)(int16_t)(first - 4));
    intro_032c_000a(fh, P32(tab + (uint16_t)(idx << 2)), -1);
    intro_032c_0195(fh, dest, 0x3de);
    intro_032c_022f(fh);
}

/* 003a:00b0  draws a line of text (up to '\r' or NUL) with the glyph sprites of bank
 * DSPTR(0xbea) at (x, y) into the draw buffer; font2 != 0 selects the second glyph set */
void intro_003a_00b0(int16_t x, int16_t y, uint8_t *str, int16_t font2)
{
    int8_t c, spr;
    while (*str != 0x0d && *str != 0) {
        if (*str == 0x20) {
            x += 8;
        } else {
            c = (int8_t)(*str + 0xbf);          /* c - 'A' */
            spr = (int8_t)(c + 0x10);
            if (font2 != 0) spr = (int8_t)(spr + 0x1d);
            intro_0488_01d0((uint16_t)(int16_t)spr, x, y, DSPTR(0x148), DSPTR(0xbea));
            x += DS8(0xa8 + c) + 1;
        }
        str++;
    }
}

/* 003a:012b  "typewriter": prints str at (x, y) one character at a time with a click
 * sound, flipping the back buffer after each glyph and waiting 4..11 frames */
void intro_003a_012b(int16_t x, int16_t y, uint8_t *str)
{
    int8_t c, spr;
    while (*str != 0x0d && *str != 0) {
        if (*str == 0x20) {
            x += 8;
        } else {
            c = (int8_t)(*str + 0xbf);
            if ((int8_t)*str >= 0x30 && (int8_t)*str <= 0x39)
                c = (int8_t)(*str + 0xed);      /* digits follow the letters */
            spr = (int8_t)(c + 0x10);
            intro_0362_0061(0x38, 0, 0x80);
            intro_04d8_0008(DSPTR(0x14c), DSPTR(0x148));
            intro_0488_01d0((uint16_t)(int16_t)spr, x, y, DSPTR(0x148), DSPTR(0xbea));
            intro_066d_0111();
            x += DS8(0xa8 + c) + 1;
        }
        str++;
        intro_003a_0be2((int16_t)((intro_03e3_0006() & 7) + 4));
    }
}

/* 003a:01f4  credits page: restores the saved picture, prints `count` centred lines of the
 * credits text starting at line `line` (first line with font 1, the others with font 2),
 * dissolves it in and waits count*100 frames (Esc/Enter skip) */
void intro_003a_01f4(int16_t line, int16_t count)
{
    uint8_t *p, *start;
    int16_t first = 0, y, i, k, w;

    intro_04d8_0008(DSPTR(0xbf2), DSPTR(0x148));
    y = (int16_t)(0xc8 - count * 0x14) / 2;
    for (i = line; line + count > i; i++) {
        p = DSPTR(0xbee);
        for (k = 0; k <= i; k++) {
            while (*p++ != 0x2a) ;              /* '*' */
            while (*p++ != 0x0a) ;
        }
        start = p;
        w = 0;
        for (; *p != 0x0d; p++) {
            if (*p == 0x20) w += 8;
            else            w += DS8(0x67 + *p) + 1;
        }
        intro_003a_00b0((int16_t)(0x140 - w) / 2, y, start, first);
        first = 1;
        y += 0x14;
    }
    if (DS8(0x15e) != KEY_ESC) {
        intro_0131_0022();
        for (k = 0; count * 100 > k; k++) {
            intro_066d_00e8();
            if (DS8(0x15e) == KEY_ENTER) break;
            if (DS8(0x15e) == KEY_ESC) break;
        }
    }
}

/* 003a:031c  scene 3: SPLASH picture over the tile grid, then the credits pages */
void intro_003a_031c(void)
{
    int16_t row, col, k;
    DSPTR_SET(0xbfe, intro_03f8_000c(DSADDR(0x117), 3));
    for (row = 0; row < 4; row++)
        for (col = 0; col < 4; col++)
            intro_003a_0757(col, row, col + 0xc, 3);
    intro_050c_0475(0, 0, 0, DSPTR(0xbfe));
    intro_0131_0022();
    intro_003a_0be2(0x46);
    while (DS8(0x15e) == KEY_ENTER) plat_yield();
    for (k = 0; k < 0x32; k++) {
        intro_066d_00e8();
        if (DS8(0x15e) == KEY_ENTER) return;
        if (DS8(0x15e) == KEY_ESC) return;
    }
    intro_04d8_0008(DSPTR(0x148), DSPTR(0xbf2));
    intro_003a_01f4(0, 2);
    intro_003a_01f4(2, 2);
    intro_003a_01f4(4, 3);
    intro_003a_01f4(7, 7);
    intro_003a_01f4(0xe, 2);
    intro_003a_01f4(0x14, 3);
    intro_003a_01f4(0x12, 2);
}

/* 003a:0424  converts an 8-bit palette to 6 bits in place and loads it into the DAC
 * (colours 0..127, retrace, 128..255) */
void intro_003a_0424(uint8_t *pal)
{
    intro_031b_00e2(pal);
    intro_031b_0108();
    intro_031b_00c2(pal, 0, 0x80);
    intro_031b_0108();
    intro_031b_00c2(pal + 0x180, 0x80, 0x80);
}

/* 003a:046e  sets the palette `pal` scaled by level/256 (mode 0 all channels, 1 keeps R,
 * 2 keeps G, 3 keeps B) */
void intro_003a_046e(uint8_t *pal, int16_t level, int16_t mode)
{
    uint8_t tmp[0x300];
    if (mode == 0) intro_031b_0008(pal, tmp, level);
    if (mode == 1) intro_031b_0068(pal, tmp, level);
    if (mode == 2) intro_031b_0095(pal, tmp, level);
    if (mode == 3) intro_031b_003b(pal, tmp, level);
    intro_003a_0424(tmp);
}

/* 003a:04f8  black palette, shows `buf` on screen and fades the palette `pal` in (32 steps) */
void intro_003a_04f8(uint8_t *buf, uint8_t *pal, int16_t mode)
{
    uint8_t tmp[0x302];
    int16_t level;
    memset(tmp, 0, 0x300);                      /* setmem (06e8_0007) */
    intro_003a_0424(tmp);
    intro_04d8_0008(buf, DSPTR(0x14c));
    for (level = 0; level < 0x100; level += 8)
        intro_003a_046e(pal, level, mode);
}

/* 003a:055c  shows `buf` on screen and fades the palette `pal` out (32 steps) */
void intro_003a_055c(uint8_t *buf, uint8_t *pal, int16_t mode)
{
    int16_t level;
    intro_04d8_0008(buf, DSPTR(0x14c));
    for (level = 0x100; level > 0; level -= 8)
        intro_003a_046e(pal, level, mode);
}

/* 003a:059e  scene 1: TATOU (Infogrames armadillo) logo, fade in, wait, fade out */
void intro_003a_059e(void)
{
    int16_t k;
    intro_0488_01bd(0, 0xc6);
    intro_050c_0475(0, 0, 0, DSPTR(0xc02));
    intro_0488_01bd(0, 0xc7);
    intro_003a_04f8(DSPTR(0x148), DSPTR(0xbde), 0);
    intro_03e3_0006();
    for (k = 0; k < 0xc8; k++) {
        if (DS8(0x15e) != 0) break;
        intro_066d_00e8();
    }
    while (DS8(0x15e) == KEY_ENTER) plat_yield();
    intro_003a_055c(DSPTR(0x148), DSPTR(0xbde), 0);
}

/* 003a:0631  loads the resources: music + SFX bank, SLIDE and TATOU pictures, the credits
 * text of the current language, work buffers; starts the music */
void intro_003a_0631(void)
{
    uint8_t *p;
    DSPTR_SET(0xbe6, intro_03f8_000c(DSPTR(0xd8), 7));
    DSPTR_SET(0xbda, intro_03f8_000c(DSPTR(0xd8), 1));
    intro_0362_019a(0, DSPTR(0xbe6));
    intro_0362_0170(0, DSPTR(0xbda));
    intro_0362_0005(0, 0, 0xa0);
    DSPTR_SET(0xbf8, intro_03f8_000c(DSADDR(0x11d), 4));
    DSPTR_SET(0xc02, intro_03f8_000c(DSADDR(0x123), 5));
    p = intro_03e5_000c(0x3e8);
    DSPTR_SET(0xbee, p);
    intro_003a_000d(DSPTR(0x94 + (uint16_t)(DS16(0xbf6) << 2)), 0xe, p);
    /* port: 64K instead of 5000 bytes (0x1388): 05ae_01ba writes with 16-bit wrapping
     * offsets, damaged sprite data could overflow the original's block */
    DSPTR_SET(0xbd6, intro_03e5_000c(0x10000));
    DSPTR_SET(0xbf2, intro_03e5_000c(0xfa00));
    p = DSPTR(0xbf8);
    DSPTR_SET(0xbde, p + P32(p + 0x10));
    DSPTR_SET(0xbea, p + P32(p + 4));
}

/* 003a:0757  draws picture tile `idx` of the SLIDE bank in grid cell (col, row) (80x43
 * cells); mode 1 draws it at half size (shrunk with 05ae_01ba), mode 3 at full size,
 * mode 0 at full size with col/row masked by 2 */
void intro_003a_0757(int16_t col, int16_t row, int16_t idx, int16_t mode)
{
    int16_t si = col, di = row;
    if (mode == 0) {
        si &= 2;
        di &= 2;
        si = si * 0x50;
        di = di * 0x2b + 0x56;
        intro_0488_01d0((uint16_t)idx, si, di, DSPTR(0x148), DSPTR(0xbea));
    }
    if (mode == 1) {
        si = si * 0x50;
        di = di * 0x2b + 0x2b;
        intro_05ae_01ba((uint16_t)idx, 7, DSPTR(0xbea), DSPTR(0xbd6));
        intro_0488_01d0(0, si, di, DSPTR(0x148), DSPTR(0xbd6));
    }
    if (mode == 3) {
        si = si * 0x50;
        di = di * 0x2b + 0x2b;
        intro_0488_01d0((uint16_t)idx, si, di, DSPTR(0x148), DSPTR(0xbea));
    }
}

/* 003a:0826  "lens" zoom: draws the saved picture DSPTR(0xbf2) directly on the screen with
 * horizontally reduced lines (reduction (100-y)^2/div*2) while div grows to 199 */
void intro_003a_0826(void)
{
    int16_t div = 1, ysrc = 0, j, red;
    int16_t t;
    uint32_t saved = DSU32(0x148);              /* far pointer kept as its raw token */

    DSU32(0x148) = DSU32(0x14c);                /* draw directly on the screen */
    intro_066d_0101();
    while (div < 0xc7) {
        ysrc += 3;
        if (ysrc > 0xc7) { ysrc = 0; div++; }
        for (j = 0; j < 0xc8; j++) {
            t = (int16_t)(100 - j);
            red = (int16_t)((int32_t)(int16_t)(uint16_t)((uint16_t)t * (uint16_t)t) / div);
            red = (int16_t)(red << 1);
            if (ysrc > 0 && red < 0xa1)
                intro_0131_01bd(ysrc, j, red, 0);
            ysrc++;
            if (ysrc > 0xc7) { ysrc = 0; div++; }
        }
        if (DS8(0x15e) == KEY_ENTER) break;
        if (DS8(0x15e) == KEY_ESC) break;
        /* TODO(port): the original ran at CPU speed with no retrace wait; pace it (and show
         * the screen) once per pass. */
        plat_wait_vsync();
    }
    DSU32(0x148) = saved;
}

/* 003a:08e0  scene 2: SLIDE picture: zoom effect, dissolve, then 300 random tile updates
 * (half size / full size) on a 4x4 grid with black separators */
void intro_003a_08e0(void)
{
    int16_t iter, tilepos = -1, tilerow = 0, row, col, k;
    int16_t a, b, c, d;

    intro_066d_0101();
    for (row = 0; row < 4; row++)
        for (col = 0; col < 4; col++)
            intro_003a_0757(col, row, col + 0xc, 3);
    intro_050c_0475(1, 0, -6, DSPTR(0xbf8));
    intro_04d8_0008(DSPTR(0x148), DSPTR(0xbf2));
    intro_066d_0101();
    intro_066d_0111();
    intro_003a_0424(DSPTR(0xbde));
    intro_003a_0826();
    intro_0131_0022();
    while (DS8(0x15e) == KEY_ENTER) plat_yield();
    for (k = 0; k < 0x64; k++) {
        if (DS8(0x15e) != 0) break;
        intro_066d_00e8();
    }
    for (iter = 0; iter < 0x12c; iter++) {
        if (DS8(0x15e) != 0) return;
        a = (int16_t)(intro_03e3_0006() & 3);
        b = (int16_t)(intro_03e3_0006() & 3);
        c = (int16_t)(intro_03e3_0006() % 0xc);
        d = (int16_t)(intro_03e3_0006() % 6);
        if (d > 0) d = 1;
        if ((iter & 0x1f) == 0) {
            tilepos = 0;
            tilerow = (int16_t)(intro_03e3_0006() & 3);
        }
        if (tilepos != -1) {
            d = 3;
            a = tilepos;
            tilepos++;
            b = tilerow;
            c = a + 0xc;
            if (tilepos == 4) tilepos = -1;
        }
        intro_003a_0757(a, b, c, d);
        for (k = 0; k < 3; k++)
            intro_04e2_0004(k * 0x50 + 0x4f, 0, k * 0x50 + 0x4f, 0xc7, 0);
        for (k = 0; k < 3; k++)
            intro_04e2_0004(0, k * 0x2b + 0x2b, 0x13f, k * 0x2b + 0x2b, 0);
        intro_066d_0111();
        for (k = 0; k < 0xc; k++) {
            if (DS8(0x15e) == KEY_ESC) return;
            intro_066d_00e8();
        }
    }
}

/* 003a:0ad6  animates a window growing from rectangle a to rectangle b over 40 steps:
 * copies that part of the back buffer to the screen and frames it with colour 0x15 */
void intro_003a_0ad6(int16_t ax0, int16_t ay0, int16_t ax1, int16_t ay1,
                     int16_t bx0, int16_t by0, int16_t bx1, int16_t by1)
{
    int16_t k, x0, y0, x1, y1;
    uint32_t saved;
    for (k = 0; k <= 0x28; k++) {
        x0 = intro_0131_0004(ax0, bx0, k, 0x28);
        y0 = intro_0131_0004(ay0, by0, k, 0x28);
        x1 = intro_0131_0004(ax1, bx1, k, 0x28);
        y1 = intro_0131_0004(ay1, by1, k, 0x28);
        intro_066d_00e8();
        intro_04dd_0002(DSPTR(0x148), x0, y0, DSPTR(0x14c), x0, y0, x1 - x0 + 1, y1 - y0 + 1);
        saved = DSU32(0x148);
        DSU32(0x148) = DSU32(0x14c);
        intro_060b_000a(x0 - 1, y0 - 1, x1 + 1, y1 + 1, 0x15);
        DSU32(0x148) = saved;
        if (DS8(0x15e) == KEY_ENTER || DS8(0x15e) == KEY_ESC)
            k = 0x27;                           /* jump to the final rectangle */
    }
    while (DS8(0x15e) == KEY_ENTER) plat_yield();
}

/* 003a:0be2  waits `frames` retraces (stops early on Enter or Esc) */
void intro_003a_0be2(int16_t frames)
{
    int16_t k;
    for (k = 0; k < frames; k++) {
        intro_066d_00e8();
        if (DS8(0x15e) == KEY_ENTER) break;
        if (DS8(0x15e) == KEY_ESC) break;
    }
}

/* 003a:0c13  scene 4 (EARTH): new music, "PLANET ETERNAM" / "DATE 2815" typed in, the
 * picture scrolls up, then the close-ups zoom in window by window */
void intro_003a_0c13(void)
{
    int16_t y;
    uint8_t *p;
    if (DS8(0x15e) == KEY_ESC) return;
    intro_0362_0005(0, 0, 0x8000);
    intro_03e5_0069(DSPTR(0xbfe));
    intro_03e5_0069(DSPTR(0xbf8));
    intro_03e5_0069(DSPTR(0xc02));
    intro_0362_0005(0, 0, 0x40);
    intro_03e5_0069(DSPTR(0xbe6));
    DSPTR_SET(0xbe6, intro_03f8_000c(DSPTR(0xd8), 6));
    intro_0362_019a(0, DSPTR(0xbe6));
    intro_0362_0005(0, 0, 0x80);
    DSPTR_SET(0xbf8, intro_03f8_000c(DSADDR(0x129), 0));
    DSPTR_SET(0xbe2, intro_03f8_000c(DSADDR(0x12f), 1));
    p = DSPTR(0xbf8);
    DSPTR_SET(0xbea, p + P32(p + 4));
    intro_0650_0127(DSPTR(0xbe2), 0x37);
    intro_050c_0475(8, 0, 0, DSPTR(0xbf8));
    intro_0131_0022();
    intro_003a_012b(0x55, 0xa0, DSPTR(0xd0));
    intro_003a_0be2(0x50);
    intro_003a_012b(0x73, 0xae, DSPTR(0xd4));
    intro_003a_0be2(0x12c);
    intro_050c_0475(0, 0, 0x6e, DSPTR(0xbf8));
    intro_066d_0111();
    intro_003a_0be2(0x32);
    for (y = 0x6e; y >= 0; y -= 2) {
        intro_050c_0475(0, 0, y, DSPTR(0xbf8));
        intro_066d_0111();
        if (DS8(0x15e) == KEY_ESC || DS8(0x15e) == KEY_ENTER)
            y = 2;                              /* last frame (y = 0) next */
    }
    intro_003a_0be2(0x96);
    intro_050c_0475(2, 0, 0, DSPTR(0xbf8));
    intro_003a_0be2(0x64);
    intro_050c_0475(3, 0, 0, DSPTR(0xbf8));
    intro_003a_0ad6(0x83, 0x74, 0x8d, 0x7e, 0x59, 0x3d, 0xc7, 0xab);
    intro_003a_0be2(0x64);
    intro_050c_0475(4, 0, 0, DSPTR(0xbf8));
    intro_003a_0ad6(0x83, 0x74, 0x8d, 0x7e, 0x59, 0x3d, 0xc7, 0xab);
    intro_003a_0be2(0x64);
    intro_050c_0475(5, 0, 0, DSPTR(0xbf8));
    intro_003a_0ad6(0x83, 0x74, 0x8d, 0x7e, 0x59, 0x3d, 0xc7, 0xab);
    intro_003a_0be2(0x64);
    intro_050c_0475(7, 0, 0, DSPTR(0xbf8));
    intro_003a_0ad6(0x83, 0x74, 0x8d, 0x7e, 0x59, 0x3d, 0xc7, 0xab);
    intro_003a_0be2(0x64);
    intro_003a_0ad6(0x59, 0x3d, 0xc7, 0xab, 0, 0, 0x13f, 0xc7);
    intro_003a_0be2(0xc8);
}

/* 003a:0ef9  main(): reads the launcher config (sound type -> "mub" music file, language),
 * installs video/timer, keyboard and sound, plays the four scenes, uninstalls; returns -1 */
int16_t intro_003a_0ef9(void)
{
    uint8_t *cfg = sys_launcher_config();       /* far ptr at 0000:025c */
    DS16(0xbfc) = cfg[7];
    if (DS16(0xbfc) == 0)
        DSPTR(0xd8)[2] = 0x62;                  /* "mus" -> "mub" (no AdLib) */
    DS16(0xbf6) = cfg[8];
    intro_0690_000b();
    intro_037d_0334();
    intro_0362_00b3();
    DS16(0x162) = 0;
    intro_003a_0631();
    intro_003a_059e();
    intro_003a_08e0();
    intro_003a_031c();
    intro_003a_0c13();
    intro_0362_0111();
    intro_037d_0365();
    intro_0690_004d();
    return -1;
}

/* Port entry point (replaces c0l start-up + exit): clears the intro's BSS like c0 does,
 * runs main() and releases the memory the original left to DOS. */
void intro_run(void)
{
    memset(DSADDR(0x8d6), 0, 0xc86 - 0x8d6);   /* c0: zero BSS (0x8d6..0xc86) */
    intro_003a_0ef9();
    intro_03e5_free_all();
}
