/*
 * Intro segment 04dd: rectangular block copy between 320-wide buffers.
 */
#include "../core/mem.h"
#include "../platform/platform.h"
#include "intro_protos.h"
#include <string.h>

/* 04dd:0002  copies a w x h block from (sx, sy) of src to (dx, dy) of dst (both 320 wide,
 * rows from the SS row table); the word count per row is 8-bit ((w >> 1) & 0xff) */
void intro_04dd_0002(uint8_t *src, int16_t sx, int16_t sy, uint8_t *dst, int16_t dx, int16_t dy,
                     int16_t w, int16_t h)
{
    uint8_t *si = src + (uint16_t)(ISSU16((uint16_t)(sy << 1)) + sx);
    uint8_t *di = dst + (uint16_t)(ISSU16((uint16_t)(dy << 1)) + dx);
    int16_t stride = (int16_t)(0x140 - w);
    uint16_t n = (uint16_t)((((uint16_t)w >> 1) & 0xff) * 2 + (w & 1));
    uint16_t rows = (uint16_t)h;
    do {                                        /* dec bx / jne */
        memmove(di, si, n);
        si += n + stride;
        di += n + stride;
    } while (--rows);
}
