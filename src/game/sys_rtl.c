/*
 * Native replacements for the Turbo C runtime and the DOS wrappers of the
 * original (segments 0000, 0e66, 0f27, 0f32, 0f48, 0fa2, 11b7, 11bb, 11c1,
 * 11f2, 1207, 1209, 120e, 1215).  See re/notes/G10.md for the originals.
 */
#include "../core/mem.h"
#include "../core/loader.h"
#include "../platform/platform.h"
#include "protos.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

/* ---------------------------------------------------------------- 0000 */
_Noreturn void sub_0000_010d(int16_t status) { exit(status); }
void sub_0000_0000(void) {}
void sub_0000_012f(void) {}
void sub_0000_0172(void) {}
void sub_0000_01a7(void) {}
_Noreturn void sub_0000_01af(void) { plat_fatal("Abnormal program termination"); }
void sub_0000_01d0(void) {}
void sub_0000_025e(void) {}
void sub_0000_02ce(void) {}

/* ---------------------------------------------------------------- 0e66: file I/O */
#define MAX_FILES 32
static FILE *files[MAX_FILES];            /* handle n -> files[n], 0 = invalid */

static int is_save_name(const char *n) {
    return isdigit((unsigned char)n[0]) && n[1] == '.' && strcasecmp(n + 2, "ave") == 0;
}

/* 0e66:0031 - open (0x3ed read, 0x4d2 read/write, 0x3ee create); returns handle or 0 */
int32_t sub_0e66_0031(uint8_t *name, int16_t mode) {
    const char *n = (const char *)name;
    const char *base = strrchr(n, '\\');
    base = base ? base + 1 : n;
    const char *fm = mode == 0x3ee ? "wb" : mode == 0x4d2 ? "r+b" : "rb";
    int h;
    for (h = 1; h < MAX_FILES && files[h]; h++) {}
    if (h == MAX_FILES) return 0;
    FILE *f = is_save_name(base) ? save_fopen(base, fm) : data_fopen(base, fm);
    if (!f) {
        DS16(0x2d46) = 2;                                 /* DOS "file not found" */
        if (DSPTR(0x2d40) != NULL && !is_save_name(base))
            plat_fatal("Missing game file %s in %s", base, data_dir());
        return 0;
    }
    files[h] = f;
    return h;
}

static FILE *fh(int32_t h) { return (h > 0 && h < MAX_FILES) ? files[h] : NULL; }

/* 0e66:0006 - lseek: whence -1 SET, 0 CUR, 1 END; 0 ok / -1 error */
int32_t sub_0e66_0006(int32_t h, int32_t off, int32_t whence) {
    FILE *f = fh(h);
    int w = whence == -1 ? SEEK_SET : whence == 0 ? SEEK_CUR : SEEK_END;
    if (!f || fseek(f, off, w) != 0) return -1;
    return 0;
}

/* 0e66:0191 - read n bytes, returns bytes read */
int32_t sub_0e66_0191(int32_t h, uint8_t *buf, int32_t n) {
    FILE *f = fh(h);
    if (!f || n <= 0) return 0;
    return (int32_t)fread(buf, 1, (size_t)n, f);
}

/* 0e66:0114 - write n bytes, returns bytes written */
int32_t sub_0e66_0114(int32_t h, uint8_t *buf, int32_t n) {
    FILE *f = fh(h);
    if (!f || n <= 0) return 0;
    return (int32_t)fwrite(buf, 1, (size_t)n, f);
}

/* 0e66:022b - close */
int32_t sub_0e66_022b(int32_t h) {
    FILE *f = fh(h);
    if (!f) return 0;
    fclose(f);
    files[h] = NULL;
    return 1;
}

/* ---------------------------------------------------------------- 0f27: DOS memory */
int32_t sys_farcoreleft(void) {
    uint32_t a = arena_avail();
    return (int32_t)(a > 0x9fff0 ? 0x9fff0 : a);
}

/* 0f27:0004 - farmalloc (the -1 "coreleft" form is sys_farcoreleft) */
uint8_t *sub_0f27_0004(int32_t size) {
    if (size == -1) return (uint8_t *)(uintptr_t)(uint32_t)sys_farcoreleft();  /* not used by ported code */
    if (size <= 0) size = 16;
    return arena_alloc((uint32_t)size);
}

/* 0f27:0061 - farfree */
int16_t sub_0f27_0061(uint8_t *p) { arena_free(p); return -1; }

/* 0f27:0079 - resize block in place (only ever shrinks) */
int32_t sub_0f27_0079(uint8_t *p, int32_t size) { (void)p; (void)size; return -1; }

/* ---------------------------------------------------------------- 0f32: file names */
void sub_0f32_000d(uint8_t *dst, uint8_t *name, uint8_t *ext) {
    strcpy((char *)dst, (const char *)name);
    if (!strchr((char *)dst, '.')) strcat((char *)dst, (const char *)ext);
}

/* ---------------------------------------------------------------- 0f48: implode */
void sub_0f48_0029(void) {}
void sub_0f48_02ab(void) {}
void sub_0f48_0314(uint16_t flags, uint8_t *src, uint8_t *dst, int32_t outlen, uint8_t *work) {
    (void)work;
    /* the packed data sits at the end of the destination buffer; decode via a copy so the
       in-place overlap of the original cannot bite us */
    size_t avail = (size_t)(dst + outlen + 300 - src);
    uint8_t *tmp = malloc(avail);
    memcpy(tmp, src, avail);
    explode(tmp, avail, dst, (size_t)outlen, flags);
    free(tmp);
}

/* ---------------------------------------------------------------- 0fa2: PAK entry loader */
/* loads entry `index` of NAME.PAK into dst; offset tables inside become far pointers */
uint8_t sub_0fa2_0008(uint8_t *name, int16_t index, uint8_t *dst) {
    char path[128];
    sub_0f32_000d((uint8_t *)path, name, DSADDR(0x2d7a));
    int32_t h = sub_0e66_0031((uint8_t *)path, 0x3ed);
    if (!h) plat_fatal("Cannot open %s", path);
    uint32_t off, x, table[101];
    uint8_t hdr[12];
    sub_0e66_0006(h, (int32_t)(index + 1) * 4, -1);
    sub_0e66_0191(h, (uint8_t *)&off, 4);
    sub_0e66_0006(h, (int32_t)off, -1);
    sub_0e66_0191(h, (uint8_t *)&x, 4);
    table[0] = x;
    if (x) sub_0e66_0191(h, (uint8_t *)&table[1], (int32_t)x - 4);
    sub_0e66_0191(h, hdr, 12);
    uint32_t packed = PU32(hdr), unpacked = PU32(hdr + 4);
    uint8_t method = hdr[8], flags = hdr[9];
    sub_0e66_0006(h, PU16(hdr + 10), 0);
    if (method == 0) {
        sub_0e66_0191(h, dst, (int32_t)packed);
    } else if (method == 1) {
        uint8_t *tmp = malloc(packed + 16);
        memset(tmp, 0, packed + 16);
        sub_0e66_0191(h, tmp, (int32_t)packed);
        explode(tmp, packed, dst, unpacked, flags);
        free(tmp);
    }
    sub_0e66_022b(h);
    if (x) {
        for (uint32_t i = 0; i < x / 4; i++) {
            uint8_t *q = dst + table[i] - x;
            uint32_t n = PU32(q) / 4;
            for (uint32_t j = 0; j < n; j++) FP_SET(q + 4 * j, q + PU32(q + 4 * j));
        }
    }
    return 1;
}

/* ---------------------------------------------------------------- misc RTL */
int16_t sub_11b7_000d(int16_t doserr) {
    if (doserr >= 0) { DS16(0x2f8e) = doserr; }
    else { DS16(0x7f) = -doserr; DS16(0x2f8e) = -1; }
    return -1;
}
void sub_11bb_0008(void) {}
_Noreturn void sub_11bb_0009(int16_t status) { exit(status); }
uint8_t *sub_11c1_000b(uint16_t n) { return arena_alloc(n); }
void sub_11c1_0020(void) {}
void sub_11c1_0088(void) {}
void sub_11c1_0138(void) {}
void sub_11c1_01a6(void) {}
uint8_t *sub_11c1_020c(uint32_t n) { return arena_alloc(n); }
void sub_11f2_000b(void) {}
void sub_11f2_00e2(void) {}
int16_t sub_1207_000d(uint16_t seg, uint16_t paras) { (void)seg; (void)paras; return -1; }

/* 1209:000d - setmem(dst, len, val) */
void sub_1209_000d(uint8_t *dst, uint16_t len, uint8_t val) { memset(dst, val, len); }

/* 120e:0002 - movmem(src, dst, len): source first, overlap safe */
void sub_120e_0002(uint8_t *src, uint8_t *dst, uint16_t len) { memmove(dst, src, len); }

/* 1215:0017 - Turbo C rand() */
int16_t sub_1215_0017(void) {
    DSU32(0x3008) = DSU32(0x3008) * 0x015a4e35u + 1u;
    return (int16_t)((DSU32(0x3008) >> 16) & 0x7fff);
}

/* ---------------------------------------------------------------- launcher config */
/* The Tatou launcher loaded CONFIG.TAT and published it through int 97h. */
uint8_t *sys_launcher_config(void) {
    static uint8_t *cfg;
    if (cfg) return cfg;
    cfg = arena_alloc(0x200);
    size_t n;
    uint8_t *d = data_load("CONFIG.TAT", &n);
    if (d) { memcpy(cfg, d, n > 0x200 ? 0x200 : n); free(d); }
    else {
        static const uint8_t def[] = { 0x01, 0x00, 0x96, 0x00, 0x22, 0x00, 0x06, 0x02, 0x00, 0x00,
                                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                       0xff, 0xff, 0xff, 0xff };
        memcpy(cfg, def, sizeof def);
    }
    cfg[7] = 2;             /* sound: AdLib music (MUS.PAK) */
    cfg[8] = (uint8_t)plat_language();
    cfg[9] = 0;             /* keyboard: positional scancodes, no AZERTY remap */
    return cfg;
}
