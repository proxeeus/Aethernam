#ifndef ETERNAM_SCALE_H
#define ETERNAM_SCALE_H
#include <stdint.h>
enum { SCALER_XBR4, SCALER_SHARP, SCALER_COUNT };
int  scale_factor(int scaler);
/* src: 320x200 XRGB, dst: (320*f)x(200*f) XRGB */
void scale_image(int scaler, const uint32_t *src, uint32_t *dst);
#endif
