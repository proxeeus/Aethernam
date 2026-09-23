/* Segment 10f5: filled rectangle. */
#include "../core/mem.h"
#include "../platform/platform.h"
#include "protos.h"

static uint8_t s_color;        /* cs:6 fill colour */

/* 10f5:0008  (dead code) unclipped entry of the rectangle fill: fills (x0,y0)-(x1,y1) inclusive */
static __attribute__((unused)) void sub_10f5_0008(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color)
{
    uint16_t w, h, bytes;
    int16_t skip;
    uint8_t *d;

    s_color = color;
    if (x1 <= x0 || y1 <= y0)
        return;
    w = (uint16_t)(x1 - x0 + 1);
    h = (uint16_t)(y1 - y0 + 1);
    d = DSPTR(0x2d38) + (uint16_t)(SSU16((uint16_t)(y0 * 2)) + x0);
    skip = (int16_t)(0x140 - w);
    bytes = (uint16_t)((uint8_t)(w >> 1) * 2 + (w & 1));
    do {
        memset(d, s_color, bytes);
        d += bytes + skip;
    } while (--h);
}

/* 10f5:0028  filled rectangle (x0,y0)-(x1,y1) inclusive in the draw buffer, clamped to the clip rect; needs x1>x0 and y1>y0 */
void sub_10f5_0028(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color)
{
    uint16_t w, h, bytes;
    int16_t skip;
    uint8_t *d;

    s_color = color;
    if (x0 < SS16(0x196)) x0 = SS16(0x196);
    if (x1 > SS16(0x198)) x1 = SS16(0x198);
    if (y0 < SS16(0x192)) y0 = SS16(0x192);
    if (y1 > SS16(0x194)) y1 = SS16(0x194);
    if (x1 <= x0 || y1 <= y0)
        return;
    w = (uint16_t)(x1 - x0 + 1);
    h = (uint16_t)(y1 - y0 + 1);
    d = DSPTR(0x2d38) + (uint16_t)(SSU16((uint16_t)(y0 * 2)) + x0);
    skip = (int16_t)(0x140 - w);
    bytes = (uint16_t)((uint8_t)(w >> 1) * 2 + (w & 1));
    do {
        memset(d, s_color, bytes);
        d += bytes + skip;
    } while (--h);
}
