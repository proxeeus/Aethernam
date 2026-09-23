/*
 * Intro segment 060b: rectangle outline.
 */
#include "../core/mem.h"
#include "../platform/platform.h"
#include "intro_protos.h"

/* 060b:000a  draws the frame of the rectangle (x0,y0)-(x1,y1) in `color` (two clipped
 * horizontal lines, then the two vertical sides; the original reuses the pushed
 * arguments and patches them on the stack between the calls) */
void intro_060b_000a(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t color)
{
    intro_04e2_0238(x0, y0, x1, color);
    intro_04e2_0238(x0, y1, x1, color);
    intro_04e2_0004(x0, y0, x0, y1, color);
    intro_04e2_0004(x1, y0, x1, y1, color);
}
