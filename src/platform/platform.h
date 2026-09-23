/*
 * Native platform layer (SDL3) replacing the PC hardware the game drove
 * directly: VGA mode 13h, DAC palette, retrace, PIT timer, keyboard IRQ,
 * AdLib sound driver.  The decompiled game code only talks to this API.
 */
#ifndef ETERNAM_PLATFORM_H
#define ETERNAM_PLATFORM_H
#include <stdint.h>

#define SCREEN_W 320
#define SCREEN_H 200

int  plat_init(int argc, char **argv);
void plat_shutdown(void);
_Noreturn void plat_fatal(const char *fmt, ...);
const char *plat_save_dir(void);
int  plat_language(void);        /* 0 fr, 1 en, 2 es, 3 de, 4 it (launcher byte +8) */

/* --- video ------------------------------------------------------------ */
extern uint8_t *g_vram;          /* the "A000:0000" framebuffer (320x200, allocated in the arena) */
void plat_set_palette(int first, int count, const uint8_t *rgb6);   /* 6-bit DAC values */
void plat_get_palette(int first, int count, uint8_t *rgb6);
void plat_wait_vsync(void);      /* waits for the next 70 Hz retrace; presents g_vram */
void plat_present(void);         /* show g_vram now */
#define plat_screen() (g_vram)
void plat_get_time(uint8_t *h, uint8_t *m, uint8_t *s);   /* local wall-clock time */
int32_t plat_file_size(const char *dosname);              /* -1 if missing */

/* --- time / events ------------------------------------------------------ */
/* Pumps OS events, delivers pending keyboard scancodes and timer ticks to the
 * game's (decompiled) interrupt handlers, and sleeps a little.  Must be called
 * from every busy-wait loop. */
void plat_yield(void);
uint32_t plat_ms(void);
void plat_frame_limit(int fps);  /* sleep (while pumping events) so this is called at most fps times/s */
void plat_set_timer_rate(uint16_t pit_divisor);  /* PIT channel 0 reload value */

/* hooks the game installs (the old int 8 / int 9 handlers, as C functions) */
typedef void (*plat_isr_t)(void);
typedef void (*plat_kbd_isr_t)(uint8_t scancode);
void plat_set_timer_isr(plat_isr_t fn);
void plat_set_kbd_isr(plat_kbd_isr_t fn);
int  plat_quit_requested(void);

/* --- sound (software OPL2 + reimplemented Infogrames AdLib driver) ------ */
void plat_audio_lock(void);
void plat_audio_unlock(void);
void opl_write(int reg, int val);                /* from the audio thread */

#endif
