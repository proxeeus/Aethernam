/* Segment 103b: rectangle copy between 320-pitch buffers. */
#include "../core/mem.h"
#include "../platform/platform.h"
#include "protos.h"

/* 103b:0004  copies a w x h rectangle from src(sx,sy) to dst(dx,dy), both pitch 320 */
void sub_103b_0004(uint8_t *src, int16_t sx, int16_t sy, uint8_t *dst, int16_t dx, int16_t dy, uint16_t w, uint16_t h)
{
    uint8_t *s = src + (uint16_t)(SSU16((uint16_t)(sy * 2)) + sx);
    uint8_t *d = dst + (uint16_t)(SSU16((uint16_t)(dy * 2)) + dx);
    uint16_t bytes = (uint16_t)((uint8_t)(w >> 1) * 2 + (w & 1));   /* cl = low byte of w/2 */
    int16_t skip = (int16_t)(0x140 - w);
    uint16_t rows = h;

    do {
        memmove(d, s, bytes);
        s += bytes + skip;
        d += bytes + skip;
    } while (--rows);
    if (dst == DSPTR(0x2d3c))
        plat_yield();               /* copy to the screen */
}
