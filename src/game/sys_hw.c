/*
 * Native replacements for the hardware layer of the original
 * (segments 0d7d fps meter, 0ebb keyboard, 0efe timer, 118f video, 11b2 init).
 * The interrupt handlers are kept as C functions that the platform layer
 * invokes: timer at the PIT rate the game programs (60 Hz), keyboard with
 * XT scancodes.  See re/notes/G10.md.
 */
#include "../core/mem.h"
#include "../platform/platform.h"
#include "protos.h"
#include <string.h>
#include <stdlib.h>

/* ---------------------------------------------------------------- 0efe: timer */
static void (*timer_slots[6])(void);       /* cs:0efe:0016 far callback slots */
static int  timer_busy;                      /* cs:0efe:00dd */

/* 0efe:00de - timer interrupt: runs the registered callbacks */
static void timer_isr(void) {
    if (timer_busy) return;
    timer_busy = 1;
    SS16(0x190)++;
    if (DS16(0x2d64) == 0)
        for (int i = 0; i < 6; i++) if (timer_slots[i]) timer_slots[i]();
    DSFN(void (*)(void), 0x2d16)();
    timer_busy = 0;
}

void sub_0efe_0030(void) {}                                   /* empty per-tick hook */
void sub_0efe_0031(void) { plat_set_timer_rate(DSU16(0x2d14)); }

/* 0efe:004b - install the timer */
void sub_0efe_004b(void) {
    DS16(0x2d62) = 0;
    plat_set_timer_isr(timer_isr);
    sub_0efe_0031();
}

void sub_0efe_0186(void (*fn)(void)) {
    int i;
    for (i = 0; i < 5 && timer_slots[i]; i++) {}
    timer_slots[i] = fn;
}
void sub_0efe_0158(void (*fn)(void)) { sub_0efe_0186(fn); }
void sub_0efe_016f(void (*fn)(void)) { sub_0efe_0186(fn); }
void sub_0efe_01d5(void (*fn)(void)) {
    int i;
    for (i = 0; i < 5 && timer_slots[i] != fn; i++) {}
    timer_slots[i] = NULL;
}
void sub_0efe_01c0(void (*fn)(void)) { sub_0efe_01d5(fn); }

/* 0efe:0202 - uninstall the timer */
void sub_0efe_0202(void) { plat_set_timer_isr(NULL); plat_set_timer_rate(0); }

/* ---------------------------------------------------------------- 0d7d: frame-rate meter */
static int fps_hooked;

/* 0d7d:0046 - chained timer ISR: frames presented per 50 ticks */
void sub_0d7d_0046(void) {
    timer_isr();
    if (--DS16(0x2842) == 0) {
        DS16(0x2842) = 0x32;
        DS16(0x2840) = DS16(0x2d66) + 1;
        DS16(0x2d66) = 0;
    }
}
void sub_0d7d_000a(void) { fps_hooked = 1; plat_set_timer_isr(sub_0d7d_0046); }
void sub_0d7d_002e(void) { if (fps_hooked) { fps_hooked = 0; plat_set_timer_isr(timer_isr); } }

/* ---------------------------------------------------------------- 0ebb: keyboard */
#define KSEG 0x0ebb

/* 0ebb:01d2 - keyboard interrupt for one raw scancode byte */
void sub_0ebb_01d2(uint8_t sc) {
    uint8_t al = sc, ah = sc;
    if (DS16(0x2d68)) {                                           /* AZERTY translation */
        ah &= 0x80;
        al = (uint8_t)(CS8(KSEG, 0x142 + (sc & 0x7f)) | ah);
    }
    uint16_t w = (uint16_t)CS16(KSEG, 0x0c + 2 * (al & 0x7f));
    int rel = al & 0x80;
    switch (w >> 13) {
    case 1: {                                                    /* shift state */
        uint16_t m = w & 0xfff;
        if (rel) DSU16(0x2d60) &= (uint16_t)~m; else DSU16(0x2d60) |= m;
        break;
    }
    case 2: case 3: {                                            /* direction / fire */
        uint16_t t = (w >> 13) == 2 ? 0x2d4a : 0x2d4c;
        uint16_t v = w & 0xfff, b = (v >> 4) & 3, m = v & 0x8f;
        if (rel) { DSU16(t) &= (uint16_t)~m; DSU16(0x2d54) &= (uint16_t)~b; }
        else     { DSU16(t) |= m;            DSU16(0x2d54) |= b; }
        break;
    }
    case 4: {                                                    /* buttons */
        uint16_t m = w & 3;
        if (rel) DSU16(0x2d54) &= (uint16_t)~m; else DSU16(0x2d54) |= m;
        break;
    }
    default: break;
    }
    if (!(w & 0x1000)) {
        if (rel) DS8(0x2d4e) = 0;
        else {
            DS8(0x2d4e) = al;
            if (al == 0x53 && (DS16(0x2d60) & 5) == 5) exit(0);   /* Ctrl-Alt-Del */
        }
    }
}

/* 0ebb:03aa - clamp the keypad cursor */
void sub_0ebb_03aa(void) {
    if (DS16(0x2d58)) {
        if (DS16(0x2d5e) < 0) DS16(0x2d5e) = 0;
        if (DS16(0x2d5e) > 199) DS16(0x2d5e) = 199;
        if (DS16(0x2d5c) < 0) DS16(0x2d5c) = 0;
        if (DS16(0x2d5c) > 319) DS16(0x2d5c) = 319;
    }
}

/* 0ebb:037f - move the keypad cursor in direction bits dir */
void sub_0ebb_037f(uint8_t dir) {
    uint8_t s = DS8(0x2d56), d = dir & 0xf;
    DS16(0x2d5c) += (int16_t)((int8_t)CS8(KSEG, 0x110 + d) << s);
    DS16(0x2d5e) += (int16_t)((int8_t)CS8(KSEG, 0x121 + d) << s);
    sub_0ebb_03aa();
}

void sub_0ebb_0372(void) { sub_0ebb_037f(DS8(0x2d4a)); }

void sub_0ebb_0301(void) { sub_0efe_0158(sub_0ebb_0372); plat_set_kbd_isr(sub_0ebb_01d2); }
void sub_0ebb_0363(void) { plat_set_kbd_isr(NULL); }

/* ---------------------------------------------------------------- 118f: video */
static void hook_noop(void) {}

/* 118f:0004 - video/system init */
void sub_118f_0004(void) {
    DSPTR_SET(0x2d6a, (void *)hook_noop);
    DSPTR_SET(0x2d6e, (void *)hook_noop);
    DS16(0x2d52) = 0;
    uint8_t *cfg = sys_launcher_config();
    DSPTR_SET(0x2d76, cfg);
    DS16(0x2d68) = cfg[9];
    DS16(0x2d5a) = (PU16(cfg + 0x0a) != 0xffff) | (PU16(cfg + 0x16) != 0xffff) << 1;
    SS16(0x192) = 0; SS16(0x194) = 199; SS16(0x196) = 0; SS16(0x198) = 319;
    for (int y = 0; y < 200; y++) SSU16(y * 2) = (uint16_t)(y * 320);
}

void sub_118f_00cd(void) {}

/* 118f:00de - wait for the next vertical retrace */
void sub_118f_00de(void) { plat_wait_vsync(); }

/* 118f:00f7 - clear the back buffer */
void sub_118f_00f7(void) { memset(DSPTR(0x2d38), 0, 64000); }

/* 118f:0107 - copy the back buffer to the screen at the retrace */
void sub_118f_0107(void) {
    DSFN(void (*)(void), 0x2d6a)();
    sub_118f_00de();
    memcpy(DSPTR(0x2d3c), DSPTR(0x2d38), 64000);
    DSFN(void (*)(void), 0x2d6e)();
    DS16(0x2d66)++;
    plat_present();
}

/* 118f:01ab - set DAC entries from 8-bit RGB */
void sub_118f_01ab(uint8_t *rgb, int16_t first, int16_t count) {
    uint8_t tmp[768];
    if (count > 256) count = 256;
    for (int i = 0; i < count * 3; i++) tmp[i] = rgb[i] >> 2;
    plat_set_palette(first, count, tmp);
}

/* ---------------------------------------------------------------- 11b2: init / shutdown */
void sub_11b2_0001(void) {
    DSPTR_SET(0x2d38, sub_0f27_0004(0xfb40));
    DSPTR_SET(0x2d3c, g_vram);
    DS32(0x2d72) = 0x0003fb40;
    sub_118f_0004();
    sub_0efe_004b();
    sub_118f_00f7();
    sub_118f_0107();
}

void sub_11b2_0043(void) {
    sub_0efe_0202();
    sub_118f_00cd();
    sub_0f27_0061(DSPTR(0x2d38));
}
