/*
 * Intro segment 0362: interface to the resident Infogrames AdLib driver (int 0xF0,
 * AH = function).  Every "int 0xF0" becomes snd_driver_call(ax, bx, cx, dx, si, es_ptr):
 * when the original passes a far pointer in DX(=ES):SI, es_ptr is that pointer and
 * dx/si are passed as 0; otherwise es_ptr is NULL.  AL is passed as 0 (not meaningful).
 *
 * Code-segment variables: cs:[0] driver present flag, cs:[1] far ptr of the SFX bank,
 * cs:[0xa4] immediate patched by 0061 (the DX command of fn 0x0c).
 */
#include "../core/mem.h"
#include "../platform/platform.h"
#include "intro_protos.h"

static uint8_t  s_present;                      /* cs:[0] */
static uint8_t *s_sfx_bank;                     /* cs:[1] */

/* 0362:0005  music control (fn 0x0a): CX = param, SI = voice mask (0 = all), DX = command */
void intro_0362_0005(uint16_t cx, uint16_t si, uint16_t dx)
{
    if (!s_present) return;                     /* original returns -1 */
    snd_driver_call(0x0a00, 0, cx, dx, si, NULL);
}

/* 0362:0033  SFX control (fn 0x0c) with explicit CX/SI/DX (not called by the intro) */
void intro_0362_0033(uint16_t cx, uint16_t si, uint16_t dx)
{
    if (!s_present) return;
    snd_driver_call(0x0c00, 0, cx, dx, si, NULL);
}

/* 0362:0061  plays sound effect n of the SFX bank: the bank's effect table (at
 * bank + w[bank+0x0e]) gives a voice mask word followed by one track byte per voice;
 * fn 0x0c is called for each of the 11 voices in the mask with DX = cmd */
void intro_0362_0061(int16_t n, int16_t unused, uint16_t cmd)
{
    uint8_t *tab, *p;
    uint16_t mask, bit;
    int k;
    (void)unused;
    if (!s_present) return;
    /* mov cs:[0xa4], ax: patches the "mov dx, imm" below with cmd */
    tab = s_sfx_bank + PU16(s_sfx_bank + 0x0e);
    p = tab + PU16(tab + (uint16_t)(n << 1));
    mask = PU16(p);
    p += 2;
    for (bit = 1, k = 0xb; k; k--, bit <<= 1) {
        if (mask & bit) {
            uint16_t cx = *p++;
            snd_driver_call(0x0c00, 0, cx, cmd, bit, NULL);
        }
    }
}

/* 0362:00b3  detects the driver (int 0xF0 vector target signature "IFGM"), registers the
 * driver tick 0362:0108 on the timer, resets the driver (fn 2) and identifies it (fn 0x10) */
void intro_0362_00b3(void)
{
    /* port: the native driver is always present */
    s_present = 1;
    intro_03c1_0161(intro_0362_0108);
    snd_driver_call(0x0200, 0, 0, 0, 0, NULL);
    snd_driver_call(0x1000, 0, 0, 0, 0, NULL);
}

/* 0362:0108  timer callback (60 Hz): one driver tick (fn 0) */
void intro_0362_0108(void)
{
    snd_driver_call(0x0000, 0, 0, 0, 0, NULL);
}

/* 0362:0111  shutdown: removes the tick callback and silences the chip (fn 4) */
void intro_0362_0111(void)
{
    if (!s_present) return;
    intro_03c1_01b2(intro_0362_0108);
    snd_driver_call(0x0400, 0, 0, 0, 0, NULL);
}

/* 0362:0136  loads a song (fn 6) and the SFX bank embedded in it (at song + w[song+0x38],
 * fn 8), then resets the chip (fn 4) (not called by the intro) */
void intro_0362_0136(int16_t unused, uint8_t *song)
{
    (void)unused;
    if (!s_present) return;
    snd_driver_call(0x0600, 0, 0, 0, 0, song);
    s_sfx_bank = song + PU16(song + 0x38);
    snd_driver_call(0x0800, 0, 0, 0, 0, s_sfx_bank);
    snd_driver_call(0x0400, 0, 0, 0, 0, NULL);
}

/* 0362:0170  loads the SFX bank (fn 8) and remembers it for 0061 */
void intro_0362_0170(int16_t unused, uint8_t *bank)
{
    (void)unused;
    if (!s_present) return;
    s_sfx_bank = bank;
    snd_driver_call(0x0800, 0, 0, 0, 0, bank);
}

/* 0362:019a  loads a song (fn 6) and resets the chip (fn 4) */
void intro_0362_019a(int16_t unused, uint8_t *song)
{
    (void)unused;
    if (!s_present) return;
    snd_driver_call(0x0600, 0, 0, 0, 0, song);
    snd_driver_call(0x0400, 0, 0, 0, 0, NULL);
}
