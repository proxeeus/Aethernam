/*
 * SDL3 implementation of the platform layer.
 *
 * Video: the game renders 320x200x8 into g_vram exactly like VGA mode 13h.
 * On present we convert through the 6-bit DAC palette, run the selected
 * pixel-art upscaler (see scale.c) and show the result 4:3 aspect corrected
 * in a resizable, Retina-aware window.
 *
 * Time: the PC's PIT/IRQ0 and keyboard IRQ1 are replaced by plat_yield(),
 * which the decompiled code calls from its wait loops; it delivers the
 * elapsed timer ticks and queued scancodes to the game's handlers.
 */
#include "platform.h"
#include "scale.h"
#include "../core/loader.h"
#include <SDL3/SDL.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

uint8_t *g_vram;

static SDL_Window   *win;
static SDL_Renderer *ren;
static SDL_Texture  *tex;
static int           tex_w, tex_h;
static uint8_t       dac[256 * 3];           /* 6-bit values */
static uint32_t      rgba[SCREEN_W * SCREEN_H];
static uint32_t     *scaled;
static int           scaler = SCALER_XBR4;
static int           fullscreen;
static int           language = 1;        /* index into "TESDI": 0 fr, 1 en, 2 es, 3 de, 4 it */
static char          save_dir[1024];

static plat_isr_t     timer_isr;
static plat_kbd_isr_t kbd_isr;
static uint16_t       pit_div = 0;            /* 0 = 65536 (18.2 Hz) */
static uint64_t       timer_last_ns;
static uint64_t       timer_acc;               /* in PIT clocks * 1e9 units */
static uint64_t       last_present_ns, last_vsync_ns;
static int            quit_req;

static uint8_t scan_q[64];
/* --- debug hooks (env): ETERNAM_DUMP=dir (write frame PNGs every ETERNAM_DUMP_MS ms),
 *     ETERNAM_KEYS="ms:scancode:d|u,..." (scripted input), ETERNAM_EXIT_MS=n (auto quit) */
static const char *dbg_dump; static int dbg_dump_ms = 1000; static uint64_t dbg_next_dump; static int dbg_frame;
static struct { uint32_t ms; uint8_t code; } dbg_keys[256]; static int dbg_nkeys, dbg_keypos;
static uint32_t dbg_exit_ms;
static void push_scan(uint8_t c);
static void dbg_write_png(const char *path);
static void dbg_poll(void) {
    uint32_t now = (uint32_t)SDL_GetTicks();
    while (dbg_keypos < dbg_nkeys && dbg_keys[dbg_keypos].ms <= now) push_scan(dbg_keys[dbg_keypos++].code);
    if (dbg_dump && SDL_GetTicksNS() >= dbg_next_dump && g_vram) {
        char p[1200]; snprintf(p, sizeof p, "%s/f%05d_%06u.png", dbg_dump, dbg_frame++, now);
        dbg_write_png(p);
        dbg_next_dump = SDL_GetTicksNS() + (uint64_t)dbg_dump_ms * 1000000ULL;
    }
    if (dbg_exit_ms && now > dbg_exit_ms) { fprintf(stderr, "debug: auto exit\n"); exit(0); }
}
static void dbg_init(void) {
    dbg_dump = getenv("ETERNAM_DUMP");
    if (getenv("ETERNAM_DUMP_MS")) dbg_dump_ms = atoi(getenv("ETERNAM_DUMP_MS"));
    if (getenv("ETERNAM_EXIT_MS")) dbg_exit_ms = (uint32_t)atoi(getenv("ETERNAM_EXIT_MS"));
    const char *k = getenv("ETERNAM_KEYS");
    while (k && *k && dbg_nkeys < 256) {
        unsigned ms, code; char du;
        if (sscanf(k, "%u:%x:%c", &ms, &code, &du) != 3) break;
        dbg_keys[dbg_nkeys].ms = ms; dbg_keys[dbg_nkeys].code = (uint8_t)(du == 'u' ? code | 0x80 : code); dbg_nkeys++;
        k = strchr(k, ','); if (k) k++;
    }
}
static int     scan_head, scan_tail;

/* ------------------------------------------------------------------ */
_Noreturn void plat_fatal(const char *fmt, ...) {
    char buf[1024];
    va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof buf, fmt, ap); va_end(ap);
    fprintf(stderr, "Eternam: %s\n", buf);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Eternam", buf, win);
    exit(1);
}

const char *plat_save_dir(void) { return save_dir; }
uint32_t plat_ms(void) { return (uint32_t)SDL_GetTicks(); }
int plat_quit_requested(void) { return quit_req; }
int plat_language(void) { return language; }

void plat_get_time(uint8_t *h, uint8_t *m, uint8_t *s) {
    time_t t = time(NULL); struct tm lt; localtime_r(&t, &lt);
    *h = (uint8_t)lt.tm_hour; *m = (uint8_t)lt.tm_min; *s = (uint8_t)lt.tm_sec;
}

int32_t plat_file_size(const char *name) {
    FILE *f = data_fopen(name, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END); long n = ftell(f); fclose(f);
    return (int32_t)n;
}

void plat_set_timer_isr(plat_isr_t fn) { timer_isr = fn; timer_last_ns = SDL_GetTicksNS(); timer_acc = 0; }
void plat_set_kbd_isr(plat_kbd_isr_t fn) { kbd_isr = fn; }
void plat_set_timer_rate(uint16_t d) { pit_div = d; }

/* ------------------------------------------------------------------ */
static void load_prefs(void) {
    char p[1100]; snprintf(p, sizeof p, "%s/prefs.txt", save_dir);
    FILE *f = fopen(p, "r");
    if (!f) return;
    char k[64]; int v;
    while (fscanf(f, "%63s %d", k, &v) == 2) {
        if (!strcmp(k, "scaler") && v >= 0 && v < SCALER_COUNT) scaler = v;
        if (!strcmp(k, "fullscreen")) fullscreen = v;
        if (!strcmp(k, "language") && v >= 0 && v <= 4) language = v;
    }
    fclose(f);
}

static void save_prefs(void) {
    char p[1100]; snprintf(p, sizeof p, "%s/prefs.txt", save_dir);
    FILE *f = fopen(p, "w");
    if (!f) return;
    fprintf(f, "scaler %d\nfullscreen %d\nlanguage %d\n", scaler, fullscreen, language);
    fclose(f);
}

/* ---------------------------------------------------------------- game data location */
/* The app ships without the game's files: the player points it at their own copy of
 * Eternam once, and the folder is remembered in <save dir>/datadir.txt. */
static const char *const REQUIRED[] = { "AVE.CC1", "RESID.PAK", "MUS.PAK", "D00.PAK", "A00.PAK", "R00.CC4", "T.CC4" };

static int dir_has_game(const char *dir) {
    char old[1024];
    snprintf(old, sizeof old, "%s", data_dir());
    data_set_dir(dir);
    int ok = 1;
    for (size_t i = 0; i < sizeof REQUIRED / sizeof REQUIRED[0] && ok; i++) {
        FILE *f = data_fopen(REQUIRED[i], "rb");
        if (f) fclose(f); else ok = 0;
    }
    if (!ok) data_set_dir(old);
    return ok;
}

static void remember_data_dir(const char *dir) {
    char p[1100]; snprintf(p, sizeof p, "%s/datadir.txt", save_dir);
    FILE *f = fopen(p, "w");
    if (f) { fprintf(f, "%s\n", dir); fclose(f); }
}

static int remembered_data_dir(char *out, size_t n) {
    char p[1100]; snprintf(p, sizeof p, "%s/datadir.txt", save_dir);
    FILE *f = fopen(p, "r");
    if (!f) return 0;
    int ok = fgets(out, (int)n, f) != NULL;
    fclose(f);
    if (ok) out[strcspn(out, "\r\n")] = 0;
    return ok && out[0];
}

static volatile int pick_state;         /* 0 waiting, 1 chosen, 2 cancelled */
static char pick_path[1024];
static void SDLCALL pick_cb(void *ud, const char *const *list, int filter) {
    (void)ud; (void)filter;
    if (list && list[0]) { snprintf(pick_path, sizeof pick_path, "%s", list[0]); pick_state = 1; }
    else pick_state = 2;
}

static void ask_for_data_dir(void) {
    const char *msg =
        "Eternam needs the files of the original DOS game (AVE.CC1, *.PAK, *.CC4...).\n\n"
        "Choose the folder that contains your copy of Eternam.";
    for (;;) {
        const SDL_MessageBoxButtonData btn[] = {
            { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Choose Folder..." },
            { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Quit" } };
        const SDL_MessageBoxData mb = { SDL_MESSAGEBOX_INFORMATION, win, "Eternam", msg, 2, btn, NULL };
        int which = 0;
        if (!SDL_ShowMessageBox(&mb, &which) || which != 1) exit(0);
        pick_state = 0;
        SDL_ShowOpenFolderDialog(pick_cb, NULL, win, NULL, false);
        while (pick_state == 0) { SDL_PumpEvents(); SDL_Delay(10); }
        if (pick_state == 1 && dir_has_game(pick_path)) { remember_data_dir(pick_path); return; }
        if (pick_state == 1)
            msg = "That folder does not contain the Eternam game files (AVE.CC1, RESID.PAK, ...).\n\n"
                  "Choose the folder that contains your copy of Eternam.";
    }
}

/* order: --data <dir>, the remembered folder, data bundled inside the app
 * (make BUNDLE_DATA=1), the current directory, else ask the player */
static void find_data_dir(int argc, char **argv) {
    for (int i = 1; i + 1 < argc; i++)
        if (!strcmp(argv[i], "--data")) {
            if (dir_has_game(argv[i + 1])) return;
            plat_fatal("No Eternam game files in %s", argv[i + 1]);
        }
    char p[1100];
    if (remembered_data_dir(p, sizeof p) && dir_has_game(p)) return;
    const char *base = SDL_GetBasePath();            /* .app/Contents/Resources/ */
    if (base) { snprintf(p, sizeof p, "%sdata", base); if (dir_has_game(p)) return; }
    if (dir_has_game(".")) return;
    ask_for_data_dir();
}

int plat_init(int argc, char **argv) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS))
        plat_fatal("SDL_Init failed: %s", SDL_GetError());

    char *pref = SDL_GetPrefPath("Infogrames", "Eternam");
    snprintf(save_dir, sizeof save_dir, "%s", pref ? pref : ".");
    size_t l = strlen(save_dir);
    if (l && save_dir[l - 1] == '/') save_dir[l - 1] = 0;
    SDL_free(pref);
    load_prefs();
    for (int i = 1; i + 1 < argc; i++)
        if (!strcmp(argv[i], "--lang")) {
            const char *l = argv[i + 1];
            if (!strcmp(l, "fr")) language = 0; else if (!strcmp(l, "en")) language = 1;
            else if (!strcmp(l, "es")) language = 2; else if (!strcmp(l, "de")) language = 3;
            else if (!strcmp(l, "it")) language = 4;
            save_prefs();
        }

    win = SDL_CreateWindow("Eternam", 1280, 960, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!win) plat_fatal("Cannot create window: %s", SDL_GetError());
    find_data_dir(argc, argv);
    SDL_SetWindowMinimumSize(win, 320, 240);
    ren = SDL_CreateRenderer(win, NULL);
    if (!ren) plat_fatal("Cannot create renderer: %s", SDL_GetError());
    SDL_SetRenderVSync(ren, 0);
    if (fullscreen) SDL_SetWindowFullscreen(win, true);
    SDL_HideCursor();
    timer_last_ns = last_present_ns = last_vsync_ns = SDL_GetTicksNS();
    dbg_init();
    return 1;
}

void plat_shutdown(void) {
    save_prefs();
    if (tex) SDL_DestroyTexture(tex);
    if (ren) SDL_DestroyRenderer(ren);
    if (win) SDL_DestroyWindow(win);
    SDL_Quit();
}

/* ------------------------------------------------------------------ */
void plat_set_palette(int first, int count, const uint8_t *rgb6) {
    if (first < 0 || first >= 256) return;
    if (first + count > 256) count = 256 - first;
    for (int i = 0; i < count * 3; i++) dac[first * 3 + i] = rgb6[i] & 63;
}

void plat_get_palette(int first, int count, uint8_t *rgb6) {
    memcpy(rgb6, dac + first * 3, (size_t)count * 3);
}

static void ensure_texture(int w, int h) {
    if (tex && tex_w == w && tex_h == h) return;
    if (tex) SDL_DestroyTexture(tex);
    tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);
    SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_LINEAR);
    free(scaled);
    scaled = malloc((size_t)w * h * 4);
    tex_w = w; tex_h = h;
}

void plat_present(void) {
    if (!g_vram) return;
    uint32_t pal[256];
    for (int i = 0; i < 256; i++) {
        unsigned r = dac[i * 3], g = dac[i * 3 + 1], b = dac[i * 3 + 2];
        r = r << 2 | r >> 4; g = g << 2 | g >> 4; b = b << 2 | b >> 4;
        pal[i] = 0xff000000u | r << 16 | g << 8 | b;
    }
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++) rgba[i] = pal[g_vram[i]];

    int f = scale_factor(scaler);
    ensure_texture(SCREEN_W * f, SCREEN_H * f);
    scale_image(scaler, rgba, scaled);
    SDL_UpdateTexture(tex, NULL, scaled, tex_w * 4);

    int ww, wh;
    SDL_GetRenderOutputSize(ren, &ww, &wh);
    /* the original ran on 4:3 monitors: 320x200 is shown as 320x240 */
    float dw = (float)ww, dh = dw * 3.0f / 4.0f;
    if (dh > wh) { dh = (float)wh; dw = dh * 4.0f / 3.0f; }
    SDL_FRect dst = { (ww - dw) / 2, (wh - dh) / 2, dw, dh };
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);
    SDL_RenderTexture(ren, tex, NULL, &dst);
    SDL_RenderPresent(ren);
    last_present_ns = SDL_GetTicksNS();
}

/* ------------------------------------------------------------------ */
/* SDL scancode -> PC/XT set 1 make code */
static uint8_t xt_code(SDL_Scancode s) {
    switch (s) {
    case SDL_SCANCODE_ESCAPE: return 0x01;
    case SDL_SCANCODE_1: return 0x02; case SDL_SCANCODE_2: return 0x03; case SDL_SCANCODE_3: return 0x04;
    case SDL_SCANCODE_4: return 0x05; case SDL_SCANCODE_5: return 0x06; case SDL_SCANCODE_6: return 0x07;
    case SDL_SCANCODE_7: return 0x08; case SDL_SCANCODE_8: return 0x09; case SDL_SCANCODE_9: return 0x0a;
    case SDL_SCANCODE_0: return 0x0b; case SDL_SCANCODE_MINUS: return 0x0c; case SDL_SCANCODE_EQUALS: return 0x0d;
    case SDL_SCANCODE_BACKSPACE: return 0x0e; case SDL_SCANCODE_TAB: return 0x0f;
    case SDL_SCANCODE_Q: return 0x10; case SDL_SCANCODE_W: return 0x11; case SDL_SCANCODE_E: return 0x12;
    case SDL_SCANCODE_R: return 0x13; case SDL_SCANCODE_T: return 0x14; case SDL_SCANCODE_Y: return 0x15;
    case SDL_SCANCODE_U: return 0x16; case SDL_SCANCODE_I: return 0x17; case SDL_SCANCODE_O: return 0x18;
    case SDL_SCANCODE_P: return 0x19; case SDL_SCANCODE_LEFTBRACKET: return 0x1a; case SDL_SCANCODE_RIGHTBRACKET: return 0x1b;
    case SDL_SCANCODE_RETURN: case SDL_SCANCODE_KP_ENTER: return 0x1c;
    case SDL_SCANCODE_LCTRL: case SDL_SCANCODE_RCTRL: return 0x1d;
    case SDL_SCANCODE_A: return 0x1e; case SDL_SCANCODE_S: return 0x1f; case SDL_SCANCODE_D: return 0x20;
    case SDL_SCANCODE_F: return 0x21; case SDL_SCANCODE_G: return 0x22; case SDL_SCANCODE_H: return 0x23;
    case SDL_SCANCODE_J: return 0x24; case SDL_SCANCODE_K: return 0x25; case SDL_SCANCODE_L: return 0x26;
    case SDL_SCANCODE_SEMICOLON: return 0x27; case SDL_SCANCODE_APOSTROPHE: return 0x28; case SDL_SCANCODE_GRAVE: return 0x29;
    case SDL_SCANCODE_LSHIFT: return 0x2a; case SDL_SCANCODE_BACKSLASH: return 0x2b;
    case SDL_SCANCODE_Z: return 0x2c; case SDL_SCANCODE_X: return 0x2d; case SDL_SCANCODE_C: return 0x2e;
    case SDL_SCANCODE_V: return 0x2f; case SDL_SCANCODE_B: return 0x30; case SDL_SCANCODE_N: return 0x31;
    case SDL_SCANCODE_M: return 0x32; case SDL_SCANCODE_COMMA: return 0x33; case SDL_SCANCODE_PERIOD: return 0x34;
    case SDL_SCANCODE_SLASH: return 0x35; case SDL_SCANCODE_RSHIFT: return 0x36; case SDL_SCANCODE_KP_MULTIPLY: return 0x37;
    case SDL_SCANCODE_LALT: case SDL_SCANCODE_RALT: return 0x38; case SDL_SCANCODE_SPACE: return 0x39;
    case SDL_SCANCODE_CAPSLOCK: return 0x3a;
    case SDL_SCANCODE_F1: return 0x3b; case SDL_SCANCODE_F2: return 0x3c; case SDL_SCANCODE_F3: return 0x3d;
    case SDL_SCANCODE_F4: return 0x3e; case SDL_SCANCODE_F5: return 0x3f; case SDL_SCANCODE_F6: return 0x40;
    case SDL_SCANCODE_F7: return 0x41; case SDL_SCANCODE_F8: return 0x42; case SDL_SCANCODE_F9: return 0x43;
    case SDL_SCANCODE_F10: return 0x44;
    case SDL_SCANCODE_KP_7: case SDL_SCANCODE_HOME: return 0x47;
    case SDL_SCANCODE_KP_8: case SDL_SCANCODE_UP: return 0x48;
    case SDL_SCANCODE_KP_9: case SDL_SCANCODE_PAGEUP: return 0x49;
    case SDL_SCANCODE_KP_MINUS: return 0x4a;
    case SDL_SCANCODE_KP_4: case SDL_SCANCODE_LEFT: return 0x4b;
    case SDL_SCANCODE_KP_5: return 0x4c;
    case SDL_SCANCODE_KP_6: case SDL_SCANCODE_RIGHT: return 0x4d;
    case SDL_SCANCODE_KP_PLUS: return 0x4e;
    case SDL_SCANCODE_KP_1: case SDL_SCANCODE_END: return 0x4f;
    case SDL_SCANCODE_KP_2: case SDL_SCANCODE_DOWN: return 0x50;
    case SDL_SCANCODE_KP_3: case SDL_SCANCODE_PAGEDOWN: return 0x51;
    case SDL_SCANCODE_KP_0: case SDL_SCANCODE_INSERT: return 0x52;
    case SDL_SCANCODE_KP_PERIOD: case SDL_SCANCODE_DELETE: return 0x53;
    default: return 0;
    }
}

static void push_scan(uint8_t c) {
    int n = (scan_head + 1) % (int)sizeof scan_q;
    if (n != scan_tail) { scan_q[scan_head] = c; scan_head = n; }
}

static void handle_event(const SDL_Event *e) {
    switch (e->type) {
    case SDL_EVENT_QUIT: quit_req = 1; break;
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP: {
        int down = e->type == SDL_EVENT_KEY_DOWN;
        SDL_Keymod m = e->key.mod;
        if (down && (m & SDL_KMOD_GUI)) {                 /* macOS shortcuts */
            if (e->key.scancode == SDL_SCANCODE_Q) quit_req = 1;
            if (e->key.scancode == SDL_SCANCODE_F || e->key.scancode == SDL_SCANCODE_RETURN) {
                fullscreen = !fullscreen; SDL_SetWindowFullscreen(win, fullscreen); save_prefs();
            }
            return;
        }
        if (down && (e->key.scancode == SDL_SCANCODE_F11)) {
            fullscreen = !fullscreen; SDL_SetWindowFullscreen(win, fullscreen); save_prefs(); return;
        }
        if (down && (e->key.scancode == SDL_SCANCODE_F12)) {
            scaler = (scaler + 1) % SCALER_COUNT; save_prefs(); plat_present(); return;
        }
        if (e->key.repeat) return;                         /* typematic handled by the game */
        uint8_t c = xt_code(e->key.scancode);
        if (c) push_scan(down ? c : (uint8_t)(c | 0x80));
        break;
    }
    default: break;
    }
}

void plat_yield(void) {
    dbg_poll();
    SDL_Event e;
    while (SDL_PollEvent(&e)) handle_event(&e);

    while (scan_tail != scan_head) {
        uint8_t c = scan_q[scan_tail];
        scan_tail = (scan_tail + 1) % (int)sizeof scan_q;
        if (kbd_isr) kbd_isr(c);
    }

    uint64_t now = SDL_GetTicksNS();
    if (timer_isr) {
        uint64_t div = pit_div ? pit_div : 65536;
        timer_acc += (now - timer_last_ns) * 1193182ULL;
        timer_last_ns = now;
        uint64_t per = div * 1000000000ULL;
        int n = 0;
        while (timer_acc >= per) {
            timer_acc -= per;
            timer_isr();
            if (++n > 64) { timer_acc = 0; break; }      /* don't spiral after a long stall */
        }
    }
    if (now - last_present_ns > 16000000ULL) plat_present();
    else SDL_DelayNS(500000);
}

void plat_wait_vsync(void) {
    /* 70 Hz VGA retrace */
    const uint64_t frame = 1000000000ULL / 70;
    uint64_t target = last_vsync_ns + frame;
    uint64_t now = SDL_GetTicksNS();
    if (target < now || target > now + frame) target = now;
    while ((now = SDL_GetTicksNS()) < target) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) handle_event(&e);
        uint64_t left = target - now;
        SDL_DelayNS(left > 1000000 ? left - 1000000 : left);
    }
    last_vsync_ns = target;
    plat_yield();
    if (SDL_GetTicksNS() - last_present_ns > 8000000ULL) plat_present();
}

void plat_frame_limit(int fps) {
    static uint64_t next;
    uint64_t period = 1000000000ULL / (uint64_t)fps;
    uint64_t now = SDL_GetTicksNS();
    if (next == 0 || now > next + period) next = now;
    for (;;) {
        plat_yield();
        now = SDL_GetTicksNS();
        if (now >= next) break;
        uint64_t left = next - now;              /* now < next here: no wrap-around */
        if (left > 2000000) SDL_DelayNS(left - 1000000);
    }
    next += period;
}

/* minimal PNG writer for debug frame dumps (stored, uncompressed deflate) */
static uint32_t crc_tab[256];
static uint32_t crc32b(uint32_t c, const uint8_t *b, size_t n) {
    if (!crc_tab[1]) for (uint32_t i = 0; i < 256; i++) { uint32_t x = i; for (int k = 0; k < 8; k++) x = x & 1 ? 0xedb88320u ^ (x >> 1) : x >> 1; crc_tab[i] = x; }
    c = ~c; while (n--) c = crc_tab[(c ^ *b++) & 255] ^ (c >> 8); return ~c;
}
static void put32(FILE *f, uint32_t v) { uint8_t b[4] = { v >> 24, v >> 16, v >> 8, v }; fwrite(b, 1, 4, f); }
static void chunk(FILE *f, const char *t, const uint8_t *d, uint32_t n) {
    put32(f, n); fwrite(t, 1, 4, f); if (n) fwrite(d, 1, n, f);
    uint32_t c = crc32b(0, (const uint8_t *)t, 4); c = crc32b(c, d, n); put32(f, c);
}
static void dbg_write_png(const char *path) {
    const int W = SCREEN_W, H = SCREEN_H, row = 1 + W * 3;
    size_t raw_n = (size_t)row * H;
    uint8_t *raw = malloc(raw_n);
    for (int y = 0; y < H; y++) {
        raw[y * row] = 0;
        for (int x = 0; x < W; x++) {
            uint8_t c = g_vram[y * W + x];
            for (int k = 0; k < 3; k++) { uint8_t v = dac[c * 3 + k]; raw[y * row + 1 + x * 3 + k] = (uint8_t)(v << 2 | v >> 4); }
        }
    }
    size_t nblk = (raw_n + 65534) / 65535, zn = 2 + raw_n + nblk * 5 + 4;
    uint8_t *z = malloc(zn), *q = z; *q++ = 0x78; *q++ = 1;
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < raw_n; i++) { a = (a + raw[i]) % 65521; b = (b + a) % 65521; }
    for (size_t off = 0; off < raw_n; off += 65535) {
        size_t len = raw_n - off > 65535 ? 65535 : raw_n - off;
        *q++ = off + len >= raw_n; *q++ = len & 255; *q++ = len >> 8; *q++ = ~len & 255; *q++ = (~len >> 8) & 255;
        memcpy(q, raw + off, len); q += len;
    }
    uint32_t ad = b << 16 | a; *q++ = ad >> 24; *q++ = ad >> 16; *q++ = ad >> 8; *q++ = ad;
    FILE *f = fopen(path, "wb");
    if (f) {
        uint8_t ihdr[13] = { 0,0,W>>8,W&255, 0,0,H>>8,H&255, 8, 2, 0, 0, 0 };
        fwrite("\x89PNG\r\n\x1a\n", 1, 8, f);
        chunk(f, "IHDR", ihdr, 13); chunk(f, "IDAT", z, (uint32_t)(q - z)); chunk(f, "IEND", NULL, 0);
        fclose(f);
    }
    free(raw); free(z);
}
