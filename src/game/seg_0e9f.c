/*
 * Segment 0e9f - game-side interface to the resident AdLib driver (int 0xF0).
 * The driver itself is src/sound/adlib_drv.c (ticked by the audio thread).
 * See re/notes/G10.md section c and re/notes/adlib_driver.md.
 */
#include "../core/mem.h"
#include "../platform/platform.h"
#include "../sound/audio.h"
#include "protos.h"

/* code-segment variables of 0e9f */
static uint8_t  s_present;        /* cs:000e  driver present */
static uint8_t *s_sfx_bank;       /* cs:000f  current SFX bank (far ptr) */

/* reads inside the SFX bank's segment with 16-bit offset wrap (the effect
 * table walk can be fed garbage indices by sub_07fe_0ff1, see Q5) */
static uint8_t bank8(uint16_t off) {
    uint32_t len = arena_owns(s_sfx_bank) ? 0x10000u : 0x2968u;
    return (s_sfx_bank && off < len) ? s_sfx_bank[off] : 0;
}
static uint16_t bank16(uint16_t off) { return (uint16_t)(bank8(off) | bank8((uint16_t)(off + 1)) << 8); }

/* 0e9f:0013 - driver fn 0x0a (music control): CX=param, SI=voice mask, DX=cmd; -1 without driver */
int32_t sub_0e9f_0013(int16_t param, uint16_t mask, uint16_t cmd) {
    if (!s_present) return -1;
    return (int32_t)adlib_call(0x0a00, 0, (uint16_t)param, cmd, mask, NULL);
}

/* 0e9f:0041 (unreferenced) - raw driver fn 0x0c: CX=a, SI=b, DX=c */
__attribute__((unused)) static int32_t sub_0e9f_0041(int16_t a, int16_t b, int16_t c) {
    if (!s_present) return -1;
    return (int32_t)adlib_call(0x0c00, 0, (uint16_t)a, (uint16_t)c, (uint16_t)b, NULL);
}

/* 0e9f:006f - start/stop effect n of the SFX bank: for each voice of the effect's
 * mask, driver fn 0x0c with CX = that voice's track, SI = voice bit, DX = cmd */
void sub_0e9f_006f(int16_t effect, int16_t unused, uint16_t cmd) {
    (void)unused;
    if (!s_present) return;
    uint16_t si = bank16(0x0e);                             /* effect table */
    uint16_t bx = (uint16_t)(bank16((uint16_t)(si + (uint16_t)effect * 2)) + si);
    uint16_t mask = bank16(bx);
    bx = (uint16_t)(bx + 2);
    uint16_t bit = 1;
    for (int n = 0; n < 11; n++, bit <<= 1) {
        if (!(mask & bit)) continue;
        uint8_t track = bank8(bx++);
        adlib_call(0x0c00, bx, track, cmd, bit, NULL);
    }
}

/* 0e9f:00c1 - sound init: the driver is built in; register the (now empty) timer
 * callback like the original, then driver fn 2 (reset) and fn 0x10 (identify) */
void sub_0e9f_00c1(void) {
    s_present = 1;
    sub_0efe_016f(sub_0e9f_0116);
    adlib_call(0x0200, 0, 0, 0, 0, NULL);
    adlib_call(0x1000, 0, 0, 0, 0, NULL);
}

/* 0e9f:0116 - timer callback (driver fn 0). The audio thread ticks the driver now. */
void sub_0e9f_0116(void) {}

/* 0e9f:011f - sound shutdown: unregister the tick callback, driver fn 4 (silence) */
void sub_0e9f_011f(void) {
    if (!s_present) return;
    sub_0efe_01c0(sub_0e9f_0116);
    adlib_call(0x0400, 0, 0, 0, 0, NULL);
}

/* 0e9f:0144 (unreferenced) - load song + the SFX bank embedded at song+w[0x38], start */
__attribute__((unused)) static void sub_0e9f_0144(int16_t unused, uint8_t *song) {
    (void)unused;
    if (!s_present) return;
    adlib_call(0x0600, 0, 0, 0, 0, song);
    s_sfx_bank = song + PU16(song + 0x38);
    adlib_call(0x0800, 0, 0, 0, 0, s_sfx_bank);
    adlib_call(0x0400, 0, 0, 0, 0, NULL);
}

/* 0e9f:017e - set the SFX bank (driver fn 8) */
void sub_0e9f_017e(int16_t unused, uint8_t *bank) {
    (void)unused;
    if (!s_present) return;
    s_sfx_bank = bank;
    adlib_call(0x0800, 0, 0, 0, 0, bank);
}

/* 0e9f:01a8 - load a song: driver fn 6 then fn 4 (the fn 4 clears the driver's
 * melodic flag - original behaviour, keep it) */
void sub_0e9f_01a8(int16_t unused, uint8_t *song) {
    (void)unused;
    if (!s_present) return;
    adlib_call(0x0600, 0, 0, 0, 0, song);
    adlib_call(0x0400, 0, 0, 0, 0, NULL);
}

/* direct driver access for the intro program */
void snd_driver_call(uint16_t ax, uint16_t bx, uint16_t cx, uint16_t dx, uint16_t si, uint8_t *es_ptr) {
    (void)adlib_call(ax, bx, cx, dx, si, es_ptr);
}
