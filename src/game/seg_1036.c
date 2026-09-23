/* Segment 1036: full-screen buffer copy. */
#include "../core/mem.h"
#include "../platform/platform.h"
#include "protos.h"

/* 1036:000a  copies a 64000-byte 320x200 buffer src -> dst */
void sub_1036_000a(uint8_t *src, uint8_t *dst)
{
    memmove(dst, src, 0xfa00);
    if (dst == DSPTR(0x2d3c))
        plat_yield();               /* copy to the screen */
}
