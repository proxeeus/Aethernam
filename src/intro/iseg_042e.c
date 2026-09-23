/*
 * Intro segment 042e: PKWARE "implode" decompressor used by the PAK files.
 * 042e:0029 (Huffman tree builder) and 042e:02ab (bit reader) are internal to the
 * decompressor and are replaced, together with 0314, by the native explode() of loader.c.
 */
#include "../core/mem.h"
#include "../core/loader.h"
#include "../platform/platform.h"
#include "intro_protos.h"

/* 042e:0314  explode(flags, src, dst, dstlen, workend): decompresses the packed data at
 * src (which ends at workend, in the same buffer as dst) into dst[0..dstlen) */
void intro_042e_0314(uint16_t flags, uint8_t *src, uint8_t *dst, int32_t dstlen, uint8_t *workend)
{
    explode(src, (size_t)(workend - src), dst, (size_t)dstlen, flags);
}
