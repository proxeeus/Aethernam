/*
 * Intro segment 04d8: full-screen copy.
 */
#include "../core/mem.h"
#include "../platform/platform.h"
#include "intro_protos.h"
#include <string.h>

/* 04d8:0008  copies a 320x200 picture (64000 bytes) from src to dst */
void intro_04d8_0008(uint8_t *src, uint8_t *dst)
{
    memmove(dst, src, 0xfa00);
}
