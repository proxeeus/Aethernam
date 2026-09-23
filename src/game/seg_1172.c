/* Segment 1172: 1bpp proportional bitmap font renderer. */
#include "../core/mem.h"
#include "../platform/platform.h"
#include "protos.h"

/* Code-segment data / self-modifying immediates of 1172 */
static uint8_t *s_bmp;         /* cs:0xa  glyph bitmap (font+8) */
static int32_t  s_gtab;        /* cs:0xc  glyph table offset relative to s_font, pre-biased by -2*first_char */
static int16_t  s_penx;        /* cs:0x10 pen x, advanced per glyph */
static int16_t  s_peny;        /* cs:0x7d (imm) pen y */
static uint8_t  s_height;      /* cs:0x8c (imm) glyph height */
static uint8_t  s_color;       /* cs:0xa5 (imm) text colour */
static uint16_t s_stride;      /* cs:0xb4 (imm) bitmap bytes per row */
static uint8_t *s_font;        /* cs:0xd7 current font */
static uint16_t s_colorw;      /* cs:0xdb colour word (stored only) */
#define CHAR_SPACING 1         /* cs:0xc3: immediate of "add ax,1" (never patched) */
#define SPACE_WIDTH  2         /* cs:0xd3: immediate of "add ax,2" (never patched) */

/* 1172:0012  draws a NUL-terminated string at (x,y) (top-left) into dst with the current font and colour */
void sub_1172_0012(int16_t x, int16_t y, uint8_t *dst, uint8_t *str)
{
    uint8_t c;

    /* the original only uses the segment of dst: rows are addressed from offset 0 */
    s_penx = x;
    s_peny = y;
    while ((c = *str++) != 0) {
        uint16_t g = PU16(s_font + s_gtab + c * 2);
        uint8_t w;
        g = (uint16_t)((g >> 8) | (g << 8));        /* big-endian entry */
        w = (uint8_t)((g >> 8) >> 4);
        if (w == 0) {
            s_penx = (int16_t)(s_penx + SPACE_WIDTH + CHAR_SPACING);
            continue;
        }
        {
            uint16_t bits = (uint16_t)(g & 0xfff);
            uint8_t *src = s_bmp + (bits >> 3);
            uint8_t mask0 = CS8(0x1172, 2 + (bits & 7));   /* 0x80 >> (bits & 7) */
            uint16_t bp = (uint16_t)(s_peny * 2);
            uint8_t rows = s_height;
            do {
                uint8_t *d = dst + (uint16_t)(SSU16(bp) + s_penx);
                uint8_t m = mask0, cl = w, al = src[0];
                uint16_t bx = 0;
                bp += 2;
                do {
                    if (m & al)
                        *d = s_color;
                    d++;
                    if (m & 1) {                  /* ror carried out: next bitmap byte */
                        m = 0x80;
                        bx++;
                        al = src[bx];
                    } else {
                        m >>= 1;
                    }
                } while (--cl);
                src += s_stride;
            } while (--rows);
        }
        s_penx = (int16_t)(s_penx + w + CHAR_SPACING);
    }
}

/* 1172:011d  selects the font (parses its header) and the text colour */
void sub_1172_011d(uint8_t *font, uint8_t color)
{
    uint16_t first, tbl;

    s_font = font;
    first = font[0];
    s_height = font[2];
    s_stride = font[3];
    if (s_stride == 0)
        s_stride = PU16(font + 4);
    tbl = (uint16_t)((font[6] << 8) | font[7]);   /* big-endian */
    s_bmp = font + 8;
    s_gtab = (int32_t)tbl - (int32_t)first * 2;
    s_colorw = color;
    s_color = color;
}

/* 1172:018c  returns the pixel width of a string in the current font (glyph width or 2 for spaces, +1 each) */
int16_t sub_1172_018c(uint8_t *str)
{
    uint16_t dx = 0;
    uint8_t c;

    while ((c = *str++) != 0) {
        uint16_t g = PU16(s_font + s_gtab + c * 2);
        uint16_t w;
        g = (uint16_t)((g >> 8) | (g << 8));
        w = (uint16_t)(g >> 12);
        if (w == 0)
            w += SPACE_WIDTH;
        w += CHAR_SPACING;
        dx = (uint16_t)(dx + w);
    }
    return (int16_t)dx;
}
