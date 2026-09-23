/*
 * Intro segment 05a2 (code shared with 050c): single pixel plotting.
 * Code-segment variable: cs:[8] pixel colour.
 */
#include "../core/mem.h"
#include "../platform/platform.h"
#include "intro_protos.h"

static uint8_t s_color;                         /* cs:[8] */

/* 05a2:0009  plots a pixel at (x, y) of the draw buffer in the current colour, clipped */
void intro_05a2_0009(int16_t x, int16_t y)
{
    if (x < ISS16(0x196)) return;
    if (x > ISS16(0x198)) return;
    if (y > ISS16(0x194)) return;
    if (y < ISS16(0x192)) return;
    DSPTR(0x148)[(uint16_t)(ISSU16((uint16_t)(y << 1)) + x)] = s_color;
}

/* 05a2:00be  sets the pixel colour for 0009 */
void intro_05a2_00be(int16_t color)
{
    s_color = (uint8_t)color;
}
