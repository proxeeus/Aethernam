/* Segment 110c: pixel read. */
#include "../core/mem.h"
#include "../platform/platform.h"
#include "protos.h"

/* 110c:000c  returns the draw-buffer pixel at (x,y), no clipping */
uint8_t sub_110c_000c(int16_t x, int16_t y)
{
    return DSPTR(0x2d38)[(uint16_t)(x + SSU16((uint16_t)(y * 2)))];
}
