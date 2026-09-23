/*
 * Eternam intro program (AVE.CC1 entry 2: "PLANET ETERNAM / DATE 2815").
 * Prototypes of every decompiled function (intro_SSSS_OOOO = original seg:off
 * of the intro load image) plus the few external services it needs.
 *
 * DGROUP of the intro = segment 0x06ed (linear 0x6ed0).  All DS8/DS16/DSPTR
 * accesses in src/intro/ are offsets into THAT data segment: the runtime must
 * point g_ds at the intro's DGROUP image before calling intro_run().
 */
#ifndef ETERNAM_INTRO_PROTOS_H
#define ETERNAM_INTRO_PROTOS_H
#include <stdint.h>
#include <stdio.h>
#include "../core/mem.h"

#define INTRO_DGROUP_SEG 0x06edu
/* MZ header: SS = 0x07b6 = DGROUP + 0xc9 paragraphs (c0 keeps SS, sets SP = _stklen).
 * ss:[0..0x18f] = row table (y*320), ss:[0x190] = timer tick count,
 * ss:[0x192] ymin, [0x194] ymax, [0x196] xmin, [0x198] xmax (clip rectangle). */
#define INTRO_SS_BASE    0x0c90u
#define ISS16(a)   DS16(INTRO_SS_BASE + (a))
#define ISSU16(a)  DSU16(INTRO_SS_BASE + (a))
/* minimum size of the g_ds image the intro touches (DGROUP + bottom of SS) */
#define INTRO_DS_MIN     (INTRO_SS_BASE + 0x19au)

/* ---- external services (provided by the runtime / platform side) ------- */
/* Infogrames AdLib driver, "int 0xF0" with AH = function (see re/notes/intro.md). */
extern void snd_driver_call(uint16_t ax, uint16_t bx, uint16_t cx, uint16_t dx, uint16_t si, uint8_t *es_ptr);
/* launcher parameter block (CONFIG.TAT) = far ptr stored at 0000:025c (int 97h) */
extern uint8_t *sys_launcher_config(void);

/* entry point: runs the whole intro (the original main()), returns when it ends
 * or has been skipped.  Installs/uninstalls its own timer and keyboard ISRs. */
void intro_run(void);

typedef void (*intro_cb_t)(void);

/* ---- 003a: main program ------------------------------------------------ */
void    intro_003a_000d(uint8_t *name, int16_t idx, uint8_t *dest);
void    intro_003a_00b0(int16_t x, int16_t y, uint8_t *str, int16_t font2);
void    intro_003a_012b(int16_t x, int16_t y, uint8_t *str);
void    intro_003a_01f4(int16_t line, int16_t count);
void    intro_003a_031c(void);
void    intro_003a_0424(uint8_t *pal);
void    intro_003a_046e(uint8_t *pal, int16_t level, int16_t mode);
void    intro_003a_04f8(uint8_t *buf, uint8_t *pal, int16_t mode);
void    intro_003a_055c(uint8_t *buf, uint8_t *pal, int16_t mode);
void    intro_003a_059e(void);
void    intro_003a_0631(void);
void    intro_003a_0757(int16_t col, int16_t row, int16_t idx, int16_t mode);
void    intro_003a_0826(void);
void    intro_003a_08e0(void);
void    intro_003a_0ad6(int16_t ax0, int16_t ay0, int16_t ax1, int16_t ay1,
                        int16_t bx0, int16_t by0, int16_t bx1, int16_t by1);
void    intro_003a_0be2(int16_t frames);
void    intro_003a_0c13(void);
int16_t intro_003a_0ef9(void);

/* ---- 0131: line interpolation, dissolve, line reducer ------------------ */
int16_t intro_0131_0004(int16_t a, int16_t b, int16_t t, int16_t n);
void    intro_0131_0022(void);
void    intro_0131_01bd(int16_t ysrc, int16_t ydst, int16_t red, int16_t xbegin);

/* ---- 031b: palette ----------------------------------------------------- */
void    intro_031b_0008(uint8_t *src, uint8_t *dst, int16_t level);
void    intro_031b_003b(uint8_t *src, uint8_t *dst, int16_t level);
void    intro_031b_0068(uint8_t *src, uint8_t *dst, int16_t level);
void    intro_031b_0095(uint8_t *src, uint8_t *dst, int16_t level);
void    intro_031b_00c2(uint8_t *pal, uint16_t first, uint16_t count);
void    intro_031b_00e2(uint8_t *pal);
void    intro_031b_0108(void);

/* ---- 032c: DOS file wrappers (native stdio) ----------------------------- */
int32_t intro_032c_000a(FILE *fh, int32_t off, int32_t whence);
FILE   *intro_032c_0035(const char *name, uint16_t mode);
int32_t intro_032c_0195(FILE *fh, uint8_t *buf, int32_t len);
int16_t intro_032c_022f(FILE *fh);

/* ---- 0362: sound driver interface --------------------------------------- */
void    intro_0362_0005(uint16_t cx, uint16_t si, uint16_t dx);
void    intro_0362_0033(uint16_t cx, uint16_t si, uint16_t dx);
void    intro_0362_0061(int16_t n, int16_t unused, uint16_t cmd);
void    intro_0362_00b3(void);
void    intro_0362_0108(void);
void    intro_0362_0111(void);
void    intro_0362_0136(int16_t unused, uint8_t *song);
void    intro_0362_0170(int16_t unused, uint8_t *bank);
void    intro_0362_019a(int16_t unused, uint8_t *song);

/* ---- 037d: keyboard ------------------------------------------------------ */
void    intro_037d_01d4(uint8_t scancode);
void    intro_037d_0272(uint16_t flags, uint16_t ax);
void    intro_037d_0273(uint16_t flags, uint16_t ax);
void    intro_037d_0288(uint16_t flags, uint16_t ax);
void    intro_037d_029c(uint16_t flags, uint16_t ax);
void    intro_037d_02a3(uint16_t flags, uint16_t ax);
void    intro_037d_0303(void);
void    intro_037d_0334(void);
void    intro_037d_0365(void);
void    intro_037d_0374(void);
void    intro_037d_0381(uint8_t dirs);
void    intro_037d_03ac(void);

/* ---- 03c1: timer --------------------------------------------------------- */
void    intro_03c1_0022(void);
void    intro_03c1_0023(void);
void    intro_03c1_003d(void);
void    intro_03c1_00d0(void);
void    intro_03c1_014a(intro_cb_t cb);
void    intro_03c1_0161(intro_cb_t cb);
void    intro_03c1_0178(intro_cb_t cb);
void    intro_03c1_01b2(intro_cb_t cb);
void    intro_03c1_01c7(intro_cb_t cb);
void    intro_03c1_01f4(void);

/* ---- 03e3 / 03e5 / 03f1 / 03f8 / 042e: rand, memory, PAK loader --------- */
uint16_t intro_03e3_0006(void);
uint8_t *intro_03e5_000c(int32_t size);
int16_t  intro_03e5_0069(uint8_t *p);
int32_t  intro_03e5_0081(uint8_t *p, int32_t size);
void     intro_03e5_free_all(void);   /* port helper: frees what the intro left allocated */
void     intro_03f1_0005(char *dst, const char *src, const char *ext);
uint8_t *intro_03f8_000c(uint8_t *name, int16_t idx);
void     intro_042e_0314(uint16_t flags, uint8_t *src, uint8_t *dst, int32_t dstlen, uint8_t *workend);

/* ---- graphics ------------------------------------------------------------- */
void    intro_0488_01bd(int16_t ymin, int16_t ymax);
void    intro_0488_01d0(uint16_t idx, int16_t x, int16_t y, uint8_t *dest, uint8_t *bank);
void    intro_04d8_0008(uint8_t *src, uint8_t *dst);
void    intro_04dd_0002(uint8_t *src, int16_t sx, int16_t sy, uint8_t *dst, int16_t dx, int16_t dy,
                        int16_t w, int16_t h);
void    intro_04e2_0004(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t color);
void    intro_04e2_0238(int16_t x0, int16_t y, int16_t x1, int16_t color);
void    intro_050c_0405(uint8_t mode);
void    intro_050c_0475(uint16_t idx, int16_t x, int16_t y, uint8_t *data);
/* shape primitives (near, dispatched by 050c_0475 through cs:0x0d) */
void    intro_050c_0732(uint8_t *data, uint16_t n, uint8_t **src);
void    intro_050c_0772(uint8_t *data, uint16_t n, uint8_t **src);
void    intro_050c_07b6(uint8_t *data, uint16_t n, uint8_t **src);
void    intro_050c_07e1(uint8_t *data, uint16_t n, uint8_t **src);
void    intro_050c_080b(uint8_t *data, uint16_t n, uint8_t **src);
void    intro_050c_0848(uint8_t *data, uint16_t n, uint8_t **src);
void    intro_050c_08a6(uint8_t *data, uint16_t n, uint8_t **src);
void    intro_0597_0026(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t color);
void    intro_05a2_0009(int16_t x, int16_t y);
void    intro_05a2_00be(int16_t color);
uint8_t *intro_05ae_01ba(uint16_t idx, int16_t factor, uint8_t *bank, uint8_t *dest);
int16_t intro_05fd_000a(uint16_t idx, uint8_t *bank);
int16_t intro_05fd_0052(uint16_t idx, uint8_t *bank);
void    intro_060b_000a(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t color);
void    intro_0610_0000(int16_t xoff, int16_t yoff, uint8_t *pts, uint16_t n, uint16_t color);
void    intro_0610_0230(int16_t bound);
void    intro_0610_02ac(int16_t bound);
void    intro_0610_0328(int16_t bound);
void    intro_0610_039a(int16_t bound);
void    intro_0650_0127(uint8_t *font, int16_t color);

/* ---- 066d / 0690: video ------------------------------------------------- */
void    intro_066d_000d(void);
void    intro_066d_000e(void);
void    intro_066d_00d7(void);
void    intro_066d_00e8(void);
void    intro_066d_0101(void);
void    intro_066d_0111(void);
void    intro_0690_000b(void);
void    intro_0690_004d(void);

#endif
