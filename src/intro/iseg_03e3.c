/*
 * Intro segment 03e3: random number generator (32-bit shift register in DGROUP 0x13e/0x140).
 */
#include "../core/mem.h"
#include "../platform/platform.h"
#include "intro_protos.h"

/* 03e3:0006  shifts the 32-bit register DS16(0x140):DS16(0x13e) left 16 times, feeding in
 * bit13 ^ bit14 of the high word each time; returns the low word */
uint16_t intro_03e3_0006(void)
{
    int n;
    for (n = 0x10; n; n--) {
        uint16_t dx = DSU16(0x140);
        uint16_t ax = (uint16_t)((uint16_t)(dx << 1) ^ dx);
        uint16_t cf = (ax >> 14) & 1;           /* carry of the second "shl ax,1" */
        uint16_t lo = DSU16(0x13e);
        DSU16(0x13e) = (uint16_t)(lo << 1 | cf);
        DSU16(0x140) = (uint16_t)(dx << 1 | (lo >> 15));
    }
    return DSU16(0x13e);
}
