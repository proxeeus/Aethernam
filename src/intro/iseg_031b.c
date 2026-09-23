/*
 * Intro segment 031b: palette scaling and VGA DAC access.
 */
#include "../core/mem.h"
#include "../platform/platform.h"
#include "intro_protos.h"

/* one colour component: al (0..255, ah = 0) * level with imul, keeps ah */
static inline uint8_t scale8(uint8_t v, int16_t level)
{
    return (uint8_t)((uint16_t)((int16_t)v * level) >> 8);
}

/* 031b:0008  dst = src palette (256 RGB) with all three components * level / 256 */
void intro_031b_0008(uint8_t *src, uint8_t *dst, int16_t level)
{
    int i;
    for (i = 0; i < 0x100; i++) {
        *dst++ = scale8(*src++, level);
        *dst++ = scale8(*src++, level);
        *dst++ = scale8(*src++, level);
    }
}

/* 031b:003b  like 0008 but the blue component is copied unscaled */
void intro_031b_003b(uint8_t *src, uint8_t *dst, int16_t level)
{
    int i;
    for (i = 0; i < 0x100; i++) {
        *dst++ = scale8(*src++, level);
        *dst++ = scale8(*src++, level);
        *dst++ = *src++;
    }
}

/* 031b:0068  like 0008 but the red component is copied unscaled */
void intro_031b_0068(uint8_t *src, uint8_t *dst, int16_t level)
{
    int i;
    for (i = 0; i < 0x100; i++) {
        *dst++ = *src++;
        *dst++ = scale8(*src++, level);
        *dst++ = scale8(*src++, level);
    }
}

/* 031b:0095  like 0008 but the green component is copied unscaled */
void intro_031b_0095(uint8_t *src, uint8_t *dst, int16_t level)
{
    int i;
    for (i = 0; i < 0x100; i++) {
        *dst++ = scale8(*src++, level);
        *dst++ = *src++;
        *dst++ = scale8(*src++, level);
    }
}

/* 031b:00c2  loads `count` DAC colours starting at `first` from pal (6-bit RGB triplets) */
void intro_031b_00c2(uint8_t *pal, uint16_t first, uint16_t count)
{
    uint8_t tmp[0x300];
    unsigned i, n = (unsigned)count * 3;
    if (first > 0xff) return;
    if (first + count > 0x100) n = (0x100u - first) * 3;   /* the DAC index wraps; not reached */
    for (i = 0; i < n; i++) tmp[i] = pal[i] & 0x3f;         /* the DAC ignores bits 6-7 */
    plat_set_palette(first, (int)(n / 3), tmp);
}

/* 031b:00e2  converts 255 (sic: cx = 0xff) 8-bit palette entries to 6 bits in place;
 * the last colour is left unconverted */
void intro_031b_00e2(uint8_t *pal)
{
    int i;
    for (i = 0; i < 0xff * 3; i++)
        pal[i] = (uint8_t)(pal[i] >> 2);
}

/* 031b:0108  waits for the start of the next vertical retrace (port 0x3da bit 3) */
void intro_031b_0108(void)
{
    if (plat_quit_requested()) DS8(0x15e) = 0x01;   /* port: window closed = Esc */
    plat_wait_vsync();
}
