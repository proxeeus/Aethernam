/*
 * Intro segment 0650: bitmap font text renderer set-up.  Only the set-up is called by the
 * intro (the renderer 0650:001c and the query 0650:00e7 are unused).
 * Code-segment variables (patched immediates of the renderer): cs:0x14 glyph data offset,
 * cs:0x16 char table, cs:0x96 font height, cs:0xbe advance, cs:0xe1 font, cs:0xe5 / 0xaf colour.
 */
#include "../core/mem.h"
#include "../platform/platform.h"
#include "intro_protos.h"

static uint8_t *s_font;                         /* cs:[0xe1] */
static uint8_t  s_height;                       /* cs:[0x96] */
static uint16_t s_advance;                      /* cs:[0xbe] */
static uint8_t *s_glyphs;                       /* cs:[0x14] (+ ds) */
static uint8_t *s_chartab;                      /* cs:[0x16] / [0x18] */
static uint16_t s_color;                        /* cs:[0xe5], low byte also at cs:[0xaf] */

/* 0650:0127  selects the font (header: u16 first char, u8 height, u8 advance (0 = use the
 * next word), u16 advance, u16 big-endian char table offset) and the text colour */
void intro_0650_0127(uint8_t *font, int16_t color)
{
    uint8_t *p = font;
    uint16_t dx = PU16(p), ax;
    p += 2;
    s_font = font;
    s_height = *p++;
    s_advance = *p++;
    ax = PU16(p);
    p += 2;
    if (s_advance == 0) s_advance = ax;
    ax = PU16(p);
    p += 2;
    ax = (uint16_t)(ax << 8 | ax >> 8);         /* xchg ah, al */
    s_glyphs = p;
    s_chartab = font + (uint16_t)(ax - (uint16_t)((dx & 0xff) << 1));
    s_color = (uint16_t)color;
    (void)s_font; (void)s_height; (void)s_glyphs; (void)s_chartab; (void)s_color;
}
