/*
 * Intro segment 0597 (shares its code with the end of 050c): clipped filled rectangle.
 * Code-segment variable: cs:[4] fill colour.
 */
#include "../core/mem.h"
#include "../platform/platform.h"
#include "intro_protos.h"
#include <string.h>

static uint16_t s_color;                        /* cs:[4] */

/* 0597:0026  fills the rectangle (x0,y0)-(x1,y1) (inclusive) of the draw buffer with
 * `color`, clipped; nothing is drawn when the clipped width or height is 1 or less */
void intro_0597_0026(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t color)
{
    int16_t ax = x0, bx = x1, cx = y0, dx = y1;
    uint16_t rows, w, words;
    uint8_t *di;
    s_color = (uint16_t)color;
    if (ax < ISS16(0x196)) ax = ISS16(0x196);
    if (bx > ISS16(0x198)) bx = ISS16(0x198);
    if (cx < ISS16(0x192)) cx = ISS16(0x192);
    if (dx > ISS16(0x194)) dx = ISS16(0x194);
    if (bx <= ax) return;
    if (dx <= cx) return;
    w = (uint16_t)(bx - ax + 1);
    rows = (uint16_t)(dx - cx + 1);
    di = DSPTR(0x148) + (uint16_t)(ISSU16((uint16_t)(cx << 1)) + ax);
    words = (uint8_t)(w >> 1);                  /* 8-bit word count (bh) */
    do {                                        /* dec si / jne */
        memset(di, (uint8_t)s_color, (size_t)words * 2 + (w & 1));
        di += words * 2 + (w & 1) + (uint16_t)(0x140 - w);
    } while (--rows);
}
