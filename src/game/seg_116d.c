/* Segment 116d: rectangle outline. */
#include "../core/mem.h"
#include "../platform/platform.h"
#include "protos.h"

/* 116d:000c  draws the outline of rectangle (x0,y0)-(x1,y1): two clipped hlines and two clipped vlines */
void sub_116d_000c(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color)
{
    sub_1040_023a(x0, y0, x1, color);
    sub_1040_023a(x0, y1, x1, color);
    sub_1040_0006(x0, y0, x0, y1, color);
    sub_1040_0006(x1, y0, x1, y1, color);
}
