/* Segment 0e9b: rectangle overlap test. */
#include "../core/mem.h"
#include "../platform/platform.h"
#include "protos.h"

/* 0e9b:000c  returns 1 if int16 rects a and b ({x0,y0,x1,y1}, inclusive) overlap, else 0 */
int16_t sub_0e9b_000c(uint8_t *a, uint8_t *b)
{
    if (P16(a) > P16(b + 4)) return 0;
    if (P16(a + 2) > P16(b + 6)) return 0;
    if (P16(a + 4) < P16(b)) return 0;
    if (P16(a + 6) < P16(b + 2)) return 0;
    return 1;
}
