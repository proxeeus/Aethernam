/* xbr_upscale: 320x200 raw RGB24 on stdin -> 1280x800 raw RGB24 on stdout (uses src/platform/scale.c) */
#include "../src/platform/scale.h"
#include <stdio.h>
#include <stdint.h>
int main(void) {
    static uint8_t in[320 * 200 * 3]; static uint32_t s[320 * 200], d[1280 * 800];
    if (fread(in, 1, sizeof in, stdin) != sizeof in) return 1;
    for (int i = 0; i < 320 * 200; i++) s[i] = 0xff000000u | in[i*3] << 16 | in[i*3+1] << 8 | in[i*3+2];
    scale_image(SCALER_XBR4, s, d);
    for (int i = 0; i < 1280 * 800; i++) { uint8_t p[3] = { d[i] >> 16, d[i] >> 8, d[i] }; fwrite(p, 1, 3, stdout); }
    return 0;
}
