/* Segment 115f: RLE sprite dimension queries. */
#include "../core/mem.h"
#include "../platform/platform.h"
#include "protos.h"

/* 115f:000c  returns the width in pixels (byte0 * 16) of sprite id&0xfff of bank */
uint16_t sub_115f_000c(uint16_t id, uint8_t *bank)
{
    uint8_t *spr = bank + PU32(bank + (uint16_t)((id & 0xfff) * 4));
    return (uint16_t)(spr[0] << 4);
}

/* 115f:0054  returns the height (byte1) of sprite id&0xfff of bank */
uint16_t sub_115f_0054(uint16_t id, uint8_t *bank)
{
    uint8_t *spr = bank + PU32(bank + (uint16_t)((id & 0xfff) * 4));
    return spr[1];
}
