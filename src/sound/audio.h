/* Audio output: SDL3 stream -> software OPL2, driven by the reimplemented
 * Infogrames AdLib driver (AVE.CC1 entry 0) which ticks on the audio thread. */
#ifndef ETERNAM_AUDIO_H
#define ETERNAM_AUDIO_H
#include <stdint.h>
void audio_init(void);
void audio_lock(void);
void audio_unlock(void);
/* the driver: called with the audio lock held */
void adlib_drv_tick(void);          /* one driver timer tick (fn 0), src/sound/adlib_drv.c */
extern double adlib_drv_hz;         /* driver tick rate = game PIT rate 1193182/19886 Hz */

/* int 0xF0 of the resident driver: AH = function, registers as in the original;
 * es_ptr = native base of the DX segment (SI is the offset in it).  Returns DX<<16|AX.
 * Takes the audio lock itself.  Fn 0 (tick) is a no-op here: the audio thread ticks. */
uint32_t adlib_call(uint16_t ax, uint16_t bx, uint16_t cx, uint16_t dx, uint16_t si, uint8_t *es_ptr);
/* thin void wrapper (src/game/seg_0e9f.c), used by the intro program */
void snd_driver_call(uint16_t ax, uint16_t bx, uint16_t cx, uint16_t dx, uint16_t si, uint8_t *es_ptr);
#endif
