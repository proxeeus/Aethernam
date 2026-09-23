/*
 * Intro segment 05fd: sprite size queries (bank format: see iseg_0488.c).
 */
#include "../core/mem.h"
#include "../platform/platform.h"
#include "intro_protos.h"

/* 05fd:000a  width in pixels of sprite idx of `bank` (16 * header byte 0) */
int16_t intro_05fd_000a(uint16_t idx, uint8_t *bank)
{
    uint8_t *spr = bank + PU32(bank + (uint16_t)((idx & 0xfff) << 2));
    return (int16_t)(spr[0] << 4);
}

/* 05fd:0052  height of sprite idx of `bank` (header byte 1) */
int16_t intro_05fd_0052(uint16_t idx, uint8_t *bank)
{
    uint8_t *spr = bank + PU32(bank + (uint16_t)((idx & 0xfff) << 2));
    return spr[1];
}
