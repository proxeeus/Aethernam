/* Software YM3812 (OPL2) synthesizer (ymfm, BSD-3) - C interface */
#ifndef ETERNAM_OPL_H
#define ETERNAM_OPL_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void opl_init(int sample_rate);
void opl_reset(void);
void opl_write_reg(int reg, int val);
void opl_generate(int16_t *out, int frames);   /* mono */
#ifdef __cplusplus
}
#endif
#endif
