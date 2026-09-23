/*
 * Intro segment 03e5: DOS memory blocks (int 21h 48h/49h/4Ah) -> arena_alloc / arena_free.
 * Port: every block handed out is recorded so that intro_run() can release what the
 * original simply left to DOS at exit.
 */
#include "../core/mem.h"
#include "../platform/platform.h"
#include "intro_protos.h"

static uint8_t *s_blocks[32];

static void track(uint8_t *p)
{
    int i;
    for (i = 0; i < 32; i++)
        if (!s_blocks[i]) { s_blocks[i] = p; return; }
}

/* 03e5:000c  farmalloc(size) rounded up to paragraphs; farmalloc(-1) returns the size of
 * the largest free block instead of a pointer (only used by the PAK loader, which the port
 * sizes exactly: see 03f8_000c) */
uint8_t *intro_03e5_000c(int32_t size)
{
    uint8_t *p;
    if (size == -1) return NULL;                /* TODO(port): coreleft query, not a pointer */
    p = (uint8_t *)arena_alloc((uint32_t)((size + 15) & ~15));
    if (p) track(p);
    return p;
}

/* 03e5:0069  frees a DOS block; returns -1 on success, 0 on error */
int16_t intro_03e5_0069(uint8_t *p)
{
    int i;
    if (!p) return 0;
    for (i = 0; i < 32; i++)
        if (s_blocks[i] == p) {
            s_blocks[i] = NULL;
            arena_free(p);
            return -1;
        }
    return 0;                                   /* not ours / already freed: DOS error */
}

/* 03e5:0081  setblock(p, size): shrinks the block in place.  Port: the blocks are
 * allocated at their final size, nothing to do (arena_realloc could move them). */
int32_t intro_03e5_0081(uint8_t *p, int32_t size)
{
    (void)p; (void)size;
    return -1;
}

/* port helper: releases every block the intro did not free itself */
void intro_03e5_free_all(void)
{
    int i;
    for (i = 0; i < 32; i++)
        if (s_blocks[i]) { arena_free(s_blocks[i]); s_blocks[i] = NULL; }
}
