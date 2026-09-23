/* Segment 1100: pixel plotting with a current colour. */
#include "../core/mem.h"
#include "../platform/platform.h"
#include "protos.h"

static uint8_t s_color;        /* cs:0xa current point colour */

/* 1100:000b  plots a pixel of the current colour into the draw buffer if (x,y) is inside the clip rect */
void sub_1100_000b(int16_t x, int16_t y)
{
    if (x < SS16(0x196)) return;
    if (x > SS16(0x198)) return;
    if (y > SS16(0x194)) return;
    if (y < SS16(0x192)) return;
    DSPTR(0x2d38)[(uint16_t)(x + SSU16((uint16_t)(y * 2)))] = s_color;
}

/* 1100:0096  plots a pixel of the current colour into the draw buffer, no clipping */
void sub_1100_0096(int16_t x, int16_t y)
{
    DSPTR(0x2d38)[(uint16_t)(x + SSU16((uint16_t)(y * 2)))] = s_color;
}

/* 1100:00c0  sets the current point colour */
void sub_1100_00c0(uint8_t color)
{
    s_color = color;
}
