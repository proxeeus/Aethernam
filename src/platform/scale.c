/*
 * Pixel-art upscalers.
 *  - xBR 4x (Hyllian's edge-directed algorithm, level 2 rules) gives smooth,
 *    anti-aliased edges while keeping the painted look of the backgrounds.
 *  - "sharp" is plain 4x nearest-neighbour; the final GPU stretch is linear,
 *    so this gives crisp pixels without shimmering at non-integer sizes.
 */
#include "scale.h"
#include "platform.h"
#include <stdlib.h>
#include <string.h>

int scale_factor(int s) { (void)s; return 4; }

static uint8_t yuv_ready;
static int32_t tY[256], tU[256][2], tV[256][2];

static inline void yuv(uint32_t c, int *y, int *u, int *v) {
    int r = c >> 16 & 255, g = c >> 8 & 255, b = c & 255;
    *y = (r * 299 + g * 587 + b * 114) / 1000;
    *u = (-r * 169 - g * 331 + b * 500) / 1000 + 128;
    *v = (r * 500 - g * 419 - b * 81) / 1000 + 128;
}

static inline unsigned df(uint32_t a, uint32_t b) {
    if (a == b) return 0;
    int y1, u1, v1, y2, u2, v2;
    yuv(a, &y1, &u1, &v1); yuv(b, &y2, &u2, &v2);
    return (unsigned)(abs(y1 - y2) * 48 + abs(u1 - u2) * 7 + abs(v1 - v2) * 6);
}
static inline int eq(uint32_t a, uint32_t b) { return df(a, b) < 155; }

static inline uint32_t blend(uint32_t d, uint32_t s, int w) {   /* w/256 of s */
    uint32_t rb = d & 0xff00ff, g = d & 0x00ff00;
    rb += (((s & 0xff00ff) - rb) * w >> 8) & 0xff00ff;
    g  += (((s & 0x00ff00) - g) * w >> 8) & 0x00ff00;
    return 0xff000000u | rb | g;
}
#define B64(x, p)  x = blend(x, p, 64)
#define B128(x, p) x = blend(x, p, 128)
#define B192(x, p) x = blend(x, p, 192)

static inline void filt4(uint32_t *E, uint32_t PE, uint32_t PI, uint32_t PH, uint32_t PF, uint32_t PG,
                         uint32_t PC, uint32_t PD, uint32_t PB, uint32_t PA, uint32_t G5, uint32_t C4,
                         uint32_t G0, uint32_t D0, uint32_t C1, uint32_t B1, uint32_t F4, uint32_t I4,
                         uint32_t H5, uint32_t I5, int N15, int N14, int N11, int N3, int N7, int N10,
                         int N13, int N12) {
    (void)PA; (void)G5; (void)C4; (void)G0; (void)D0; (void)C1; (void)B1;
    if (PE == PH || PE == PF) return;
    unsigned e = df(PE, PC) + df(PE, PG) + df(PI, H5) + df(PI, F4) + (df(PH, PF) << 2);
    unsigned i = df(PH, PD) + df(PH, I5) + df(PF, I4) + df(PF, PB) + (df(PE, PI) << 2);
    if (e > i) return;
    uint32_t px = df(PE, PF) <= df(PE, PH) ? PF : PH;
    if (e < i && ((!eq(PF, PB) && !eq(PH, PD)) || (eq(PE, PI) && (!eq(PF, I4) && !eq(PH, I5))) ||
                  eq(PE, PG) || eq(PE, PC))) {
        unsigned ke = df(PF, PG), ki = df(PH, PC);
        int left = (ke << 1) <= ki && PE != PG && PD != PG;
        int up = ke >= (ki << 1) && PE != PC && PB != PC;
        if (left && up) {
            B192(E[N13], px); B64(E[N12], px);
            E[N15] = E[N14] = E[N11] = px;
            E[N10] = E[N3] = E[N12]; E[N7] = E[N13];
        } else if (left) {
            B192(E[N11], px); B192(E[N13], px); B64(E[N10], px); B64(E[N12], px);
            E[N14] = px; E[N15] = px;
        } else if (up) {
            B192(E[N14], px); B192(E[N7], px); B64(E[N10], px); B64(E[N3], px);
            E[N11] = px; E[N15] = px;
        } else {
            B128(E[N11], px); B128(E[N14], px); E[N15] = px;
        }
    } else {
        B128(E[N15], px);
    }
}

static void xbr4(const uint32_t *src, uint32_t *dst) {
    const int W = SCREEN_W, H = SCREEN_H, DW = W * 4;
#define P(x, y) src[((y) < 0 ? 0 : (y) >= H ? H - 1 : (y)) * W + ((x) < 0 ? 0 : (x) >= W ? W - 1 : (x))]
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            uint32_t A1 = P(x - 1, y - 2), B1 = P(x, y - 2), C1 = P(x + 1, y - 2);
            uint32_t A0 = P(x - 2, y - 1), A = P(x - 1, y - 1), B = P(x, y - 1), C = P(x + 1, y - 1), C4 = P(x + 2, y - 1);
            uint32_t D0 = P(x - 2, y), D = P(x - 1, y), E0 = P(x, y), F = P(x + 1, y), F4 = P(x + 2, y);
            uint32_t G0 = P(x - 2, y + 1), G = P(x - 1, y + 1), Hh = P(x, y + 1), I = P(x + 1, y + 1), I4 = P(x + 2, y + 1);
            uint32_t G5 = P(x - 1, y + 2), H5 = P(x, y + 2), I5 = P(x + 1, y + 2);
            uint32_t E[16];
            for (int k = 0; k < 16; k++) E[k] = E0;
            filt4(E, E0, I, Hh, F, G, C, D, B, A, G5, C4, G0, D0, C1, B1, F4, I4, H5, I5, 15, 14, 11, 3, 7, 10, 13, 12);
            filt4(E, E0, C, F, B, I, A, Hh, D, G, I4, A1, I5, H5, A0, D0, B1, C1, F4, C4, 3, 7, 2, 0, 1, 6, 11, 15);
            filt4(E, E0, A, B, D, C, G, F, Hh, I, C1, G0, C4, F4, G5, H5, D0, A0, B1, A1, 0, 1, 4, 12, 8, 5, 2, 3);
            filt4(E, E0, G, D, Hh, A, I, B, F, C, A0, I5, A1, B1, I4, F4, H5, G5, D0, G0, 12, 8, 13, 15, 14, 9, 4, 0);
            uint32_t *o = dst + (y * 4) * DW + x * 4;
            for (int r = 0; r < 4; r++) memcpy(o + r * DW, E + r * 4, 16);
        }
    }
#undef P
}

static void sharp4(const uint32_t *src, uint32_t *dst) {
    const int W = SCREEN_W, DW = W * 4;
    for (int y = 0; y < SCREEN_H; y++) {
        uint32_t *o = dst + y * 4 * DW;
        for (int x = 0; x < W; x++) { uint32_t c = src[y * W + x]; o[x*4] = o[x*4+1] = o[x*4+2] = o[x*4+3] = c; }
        for (int r = 1; r < 4; r++) memcpy(o + r * DW, o, DW * 4);
    }
}

void scale_image(int s, const uint32_t *src, uint32_t *dst) {
    (void)yuv_ready; (void)tY; (void)tU; (void)tV;
    if (s == SCALER_XBR4) xbr4(src, dst); else sharp4(src, dst);
}
