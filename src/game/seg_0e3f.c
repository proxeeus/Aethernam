/* Segment 0e3f: VGA DAC palette helpers and retrace wait. */
#include "../core/mem.h"
#include "../platform/platform.h"
#include "protos.h"

/* 0e3f:0004  (dead code) scales all 768 palette bytes: dst = (src*level) >> 8 */
static __attribute__((unused)) void sub_0e3f_0004(uint8_t *src, uint8_t *dst, int16_t level)
{
    uint16_t cx = 0x100;
    do {
        *dst++ = (uint8_t)((uint16_t)(*src++ * level) >> 8);
        *dst++ = (uint8_t)((uint16_t)(*src++ * level) >> 8);
        *dst++ = (uint8_t)((uint16_t)(*src++ * level) >> 8);
    } while (--cx);
}

/* 0e3f:0037  (dead code) scales red and green of 256 entries by level/256, copies blue */
static __attribute__((unused)) void sub_0e3f_0037(uint8_t *src, uint8_t *dst, int16_t level)
{
    uint16_t cx = 0x100;
    do {
        *dst++ = (uint8_t)((uint16_t)(*src++ * level) >> 8);
        *dst++ = (uint8_t)((uint16_t)(*src++ * level) >> 8);
        *dst++ = *src++;
    } while (--cx);
}

/* 0e3f:0064  (dead code) copies red, scales green and blue of 256 entries by level/256 */
static __attribute__((unused)) void sub_0e3f_0064(uint8_t *src, uint8_t *dst, int16_t level)
{
    uint16_t cx = 0x100;
    do {
        *dst++ = *src++;
        *dst++ = (uint8_t)((uint16_t)(*src++ * level) >> 8);
        *dst++ = (uint8_t)((uint16_t)(*src++ * level) >> 8);
    } while (--cx);
}

/* 0e3f:0091  (dead code) scales red and blue of 256 entries by level/256, copies green */
static __attribute__((unused)) void sub_0e3f_0091(uint8_t *src, uint8_t *dst, int16_t level)
{
    uint16_t cx = 0x100;
    do {
        *dst++ = (uint8_t)((uint16_t)(*src++ * level) >> 8);
        *dst++ = *src++;
        *dst++ = (uint8_t)((uint16_t)(*src++ * level) >> 8);
    } while (--cx);
}

/* 0e3f:00be  loads DAC entries first..first+count-1 from 6-bit RGB triplets */
void sub_0e3f_00be(uint8_t *rgb, uint16_t first, uint16_t count)
{
    plat_set_palette(first & 0xff, count, rgb);
}

/* 0e3f:00de  converts an 8-bit 256-entry palette to 6-bit in place */
void sub_0e3f_00de(uint8_t *pal)
{
    uint16_t i;
    for (i = 0; i < 0x300; i++)
        pal[i] = (uint8_t)(pal[i] >> 2);
}

/* 0e3f:0104  waits for the start of the vertical retrace */
void sub_0e3f_0104(void)
{
    plat_wait_vsync();
}
