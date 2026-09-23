/*
 * Intro segment 066d: video mode, retrace wait, clear and flip of the back buffer.
 * Code-segment variable: cs:[0xc] saved BIOS video mode.
 * DGROUP: 0x162 int 9 chaining, 0x16a joystick flags, 0x176 frame counter, 0x178 AZERTY
 * flag, 0x17a / 0x17e fptr hooks called around the flip (066d:000d, a retf), 0x186 fptr
 * launcher config.
 */
#include "../core/mem.h"
#include "../platform/platform.h"
#include "intro_protos.h"
#include <string.h>

/* 066d:000d  default pre/post-flip hook: does nothing */
void intro_066d_000d(void)
{
}

/* 066d:000e  video init: saves the BIOS mode, hooks = 066d:000d, int 24h handler, reads the
 * launcher config (AZERTY flag +9, joystick words +0x0a / +0x16), mode 13h, clip rectangle
 * 0..319 x 0..199 and the SS row table (y * 320) */
void intro_066d_000e(void)
{
    uint8_t *cfg;
    uint16_t cx = 0, ax = 0;
    int y;
    DSPTR_SET(0x17a, (void *)intro_066d_000d);
    DSPTR_SET(0x17e, (void *)intro_066d_000d);
    DS16(0x162) = 0;
    /* int 24h (critical error) handler 066d:00bf: not needed natively */
    cfg = sys_launcher_config();
    DSPTR_SET(0x186, cfg);
    DS16(0x162) = 0;
    DS16(0x178) = cfg[9];
    if (PU16(cfg + 0x0a) != 0xffff) cx |= 1;
    if (PU16(cfg + 0x16) != 0xffff) cx |= 2;
    DS16(0x16a) = (int16_t)cx;
    /* int 10h ax=13h: mode 13h (native: nothing) */
    ISS16(0x192) = 0;
    ISS16(0x194) = 0xc7;
    ISS16(0x196) = 0;
    ISS16(0x198) = 0x13f;
    for (y = 0; y < 0xc8; y++) {
        ISSU16(y * 2) = ax;
        ax += 0x140;
    }
}

/* 066d:00d7  restores the saved BIOS video mode (native: nothing) */
void intro_066d_00d7(void)
{
}

/* 066d:00e8  waits for the next vertical retrace (70 Hz) */
void intro_066d_00e8(void)
{
    if (plat_quit_requested()) DS8(0x15e) = 0x01;   /* port: window closed = Esc */
    plat_wait_vsync();
}

/* 066d:0101  clears the draw buffer DSPTR(0x148) (64000 bytes) */
void intro_066d_0101(void)
{
    memset(DSPTR(0x148), 0, 0xfa00);
}

/* 066d:0111  flip: hook, retrace, copies the draw buffer to the screen, hook, counts the
 * frame in DS16(0x176) */
void intro_066d_0111(void)
{
    DSFN(void (*)(void), 0x17a)();
    intro_066d_00e8();
    memcpy(DSPTR(0x14c), DSPTR(0x148), 0xfa00);
    DSFN(void (*)(void), 0x17e)();
    DS16(0x176)++;
}
