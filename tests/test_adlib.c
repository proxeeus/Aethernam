/*
 * Offline test of the reimplemented AdLib driver: loads a MUS.PAK song + the
 * SFX bank (AVE.EFF) the way the game does (sub_0e9f_* wrappers), runs the
 * driver at 60.0006 Hz into the software OPL2 and writes a mono WAV.
 *
 * usage: test_adlib DATA_DIR OUT.wav [song_index=0] [seconds=20] [nosfx]
 * build: see tests/build_test_adlib.sh
 */
#include "../src/core/loader.h"
#include "../src/core/mem.h"
#include "../src/sound/audio.h"
#include "../src/sound/opl.h"
#include "../src/game/protos.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* stubs for what the full program would provide */
void plat_fatal(const char *f, ...) { va_list a; va_start(a, f); vfprintf(stderr, f, a); va_end(a); exit(1); }
const char *plat_save_dir(void) { return "/tmp"; }
void *code_ptr_lookup(uint16_t s, uint16_t o) { (void)s; (void)o; return 0; }
void audio_lock(void) {}
void audio_unlock(void) {}
double adlib_drv_hz = 1193182.0 / 19886.0;
uint8_t *g_vram;
static void (*g_cb)(void);
void sub_0efe_016f(void (*fn)(void)) { g_cb = fn; }
void sub_0efe_01c0(void (*fn)(void)) { if (g_cb == fn) g_cb = 0; }

#define RATE 49716

static uint32_t rd32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }

/* minimal PAK entry loader (like sub_0fa2_0008, no sub-tables in MUS.PAK) */
static int pak_entry(const char *pak, unsigned idx, uint8_t *dst, uint32_t cap) {
    size_t n; uint8_t *d = data_load(pak, &n);
    if (!d) return 0;
    uint32_t off = rd32(d + 4 + 4 * idx), x = rd32(d + off);
    const uint8_t *h = d + off + (x ? x : 4);
    uint32_t packed = rd32(h), unp = rd32(h + 4);
    int meth = h[8], info = h[9], doff = h[10] | h[11] << 8;
    if (unp > cap) { free(d); { extern int opl_keyon_count[9]; fprintf(stderr, "key-ons per channel:"); for (int c = 0; c < 9; c++) fprintf(stderr, " %d", opl_keyon_count[c]); fprintf(stderr, "\n"); }
    return 0; }
    if (meth == 0) memcpy(dst, h + 12 + doff, unp);
    else explode(h + 12 + doff, packed, dst, unp, info);
    free(d);
    return 1;
}

static void wr16(FILE *f, uint16_t v) { fputc(v & 0xff, f); fputc(v >> 8, f); }
static void wr32(FILE *f, uint32_t v) { wr16(f, v & 0xffff); wr16(f, v >> 16); }

static FILE *trace_f; static long trace_tick;
static void trace_cb(int reg, int val) { fprintf(trace_f, "%ld %02x %02x\n", trace_tick, reg & 0xff, val & 0xff); }
extern void (*opl_trace)(int, int);
int main(int argc, char **argv) {
    if (getenv("OPL_TRACE")) { trace_f = fopen(getenv("OPL_TRACE"), "w"); opl_trace = trace_cb; }
    if (argc < 3) { fprintf(stderr, "usage: %s DATA_DIR OUT.wav [song] [seconds]\n", argv[0]); return 2; }
    data_set_dir(argv[1]);
    int song_idx = argc > 3 ? atoi(argv[3]) : 0;
    int secs = argc > 4 ? atoi(argv[4]) : 20;
    int sfx = !(argc > 5 && !strcmp(argv[5], "nosfx"));

    uint8_t *song = calloc(1, 0x5208), *bank = calloc(1, 0x2968);   /* the game's buffer sizes */
    if (!pak_entry("MUS.PAK", song_idx, song, 0x5208 - 300) || !pak_entry("MUS.PAK", 1, bank, 0x2968 - 300)) {
        fprintf(stderr, "cannot load MUS.PAK entries\n"); return 1;
    }

    opl_init(RATE);
    sub_0e9f_00c1();                       /* driver init: fn 2, fn 0x10 */
    sub_0e9f_017e(0, bank);                /* SFX bank: fn 8 */
    sub_0e9f_0013(0, 0, 0x40);             /* sub_0791_0078: stop music */
    sub_0e9f_01a8(0, song);                /* fn 6 + fn 4 */
    sub_0e9f_0013(100, 0, 0x2000);         /* sub_0791_0005: volume */
    sub_0e9f_0013(0, 0, 0x80);             /* start */

    size_t total = (size_t)secs * RATE;
    int16_t *pcm = malloc(total * 2);
    double per = RATE / adlib_drv_hz, acc = 0;
    size_t pos = 0; long ticks = 0;
    while (pos < total) {
        int until = (int)(per - acc);
        if (until <= 0) {
            trace_tick = ticks + 1; adlib_drv_tick(); acc -= per; ticks++; trace_tick = ticks;
            if (!sfx) continue;
            if (ticks == 5 * 60) {             /* sub_0791_00e5(3): duck music, stop fx 0, play effect 3 */
                sub_0e9f_0013(0x32, 0, 0x2000);
                sub_0e9f_0013(100, 0, 0x8000);
                sub_0e9f_006f(0, 0, 0x40);
                sub_0e9f_006f(3, 0, 0x80);
            }
            if (ticks == 10 * 60) sub_0e9f_006f(10, 0, 0x80);
            if (ticks == 12 * 60) sub_0e9f_006f(120, 0, 0x2000);   /* Q5: out-of-range effect index */
            continue;
        }
        int k = (size_t)until < total - pos ? until : (int)(total - pos);
        opl_generate(pcm + pos, k);
        pos += k; acc += k;
    }

    if (trace_f) fclose(trace_f);
    long clip = 0; int peak = 0; double sum = 0;
    for (size_t i = 0; i < total; i++) {
        int v = abs(pcm[i]); if (v > peak) peak = v;
        if (v >= 32767) clip++;
        sum += (double)pcm[i] * pcm[i];
    }
    { extern int opl_keyon_count[9]; printf("key-ons per channel:"); for (int c = 0; c < 9; c++) printf(" %d", opl_keyon_count[c]); printf("\n"); }
    printf("song %d: %zu samples, %ld ticks, peak %d, rms %.1f, clipped %ld\n",
           song_idx, total, ticks, peak, sqrt(sum / total), clip);

    FILE *f = fopen(argv[2], "wb");
    if (!f) { perror(argv[2]); return 1; }
    fwrite("RIFF", 1, 4, f); wr32(f, 36 + total * 2); fwrite("WAVEfmt ", 1, 8, f);
    wr32(f, 16); wr16(f, 1); wr16(f, 1); wr32(f, RATE); wr32(f, RATE * 2); wr16(f, 2); wr16(f, 16);
    fwrite("data", 1, 4, f); wr32(f, total * 2);
    fwrite(pcm, 2, total, f);
    fclose(f);
    return peak == 0 ? 3 : 0;
}
