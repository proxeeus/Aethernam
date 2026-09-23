/* Segment 0f21: game pseudo-random generator. */
#include "../core/mem.h"
#include "../platform/platform.h"
#include "protos.h"

/* 0f21:0004  32-bit LFSR in DS16 0x2d1c:0x2d1a, shifted 16 times (feedback bit30^bit29); returns low word */
uint16_t sub_0f21_0004(void)
{
    uint16_t i;
    for (i = 0; i < 0x10; i++) {
        uint16_t hi = DSU16(0x2d1c), lo = DSU16(0x2d1a);
        uint16_t t = (uint16_t)((hi << 1) ^ hi);
        uint16_t fb = (uint16_t)((t >> 14) & 1);    /* carry out of the second shl */
        uint16_t c = (uint16_t)(lo >> 15);
        DSU16(0x2d1a) = (uint16_t)((lo << 1) | fb);
        DSU16(0x2d1c) = (uint16_t)((hi << 1) | c);
    }
    return DSU16(0x2d1a);
}
