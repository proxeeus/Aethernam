/*
 * Intro segment 0690: graphics/timer start-up and shutdown.
 */
#include "../core/mem.h"
#include "../platform/platform.h"
#include "intro_protos.h"

/* 0690:000b  allocates the back buffer DSPTR(0x148) (0xfb40 bytes), screen = A000:0000,
 * video init (066d_000e), timer init (03c1_003d), clears and shows the back buffer */
void intro_0690_000b(void)
{
    /* port: 0x10000 instead of 0xfb40 so that 16-bit offset wrap-around of the blitters
     * stays inside the block (the original silently overwrote the next DOS block) */
    DSPTR_SET(0x148, intro_03e5_000c(0x10000));
    DSPTR_SET(0x14c, g_vram);                   /* 0xa000:0000 */
    DS16(0x182) = (int16_t)0xfb40;
    DS16(0x184) = 3;
    intro_066d_000e();
    intro_03c1_003d();
    intro_066d_0101();
    intro_066d_0111();
}

/* 0690:004d  restores the timer and the video mode and frees the back buffer */
void intro_0690_004d(void)
{
    intro_03c1_01f4();
    intro_066d_00d7();
    intro_03e5_0069(DSPTR(0x148));
}
