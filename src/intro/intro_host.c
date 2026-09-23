/*
 * Runs the decompiled intro program (AVE.CC1 entry 2) before the game, the way the
 * Tatou launcher ran it: the intro gets its own DGROUP image (it is a separate
 * executable), then the game's data segment is restored.
 */
#include "../core/mem.h"
#include "../core/loader.h"
#include "../platform/platform.h"
#include "intro_protos.h"
#include <stdlib.h>
#include <stdio.h>

#define INTRO_DGROUP 0x06edu

/* The copy of INTRO.PAK shipped with some releases is truncated: SLIDE.VA2 (entry 4)
 * lacks its last 645 bytes, which hold the palette every intro scene uses, and the
 * pictures come out as noise. Check every compressed entry decodes completely. */
static int intro_data_intact(void) {
    size_t n;
    uint8_t *d = data_load("INTRO.PAK", &n);
    if (!d) return 0;
    int ok = 1;
    uint32_t cnt = PU32(d + 4) / 4 - 1;
    for (uint32_t i = 0; i < cnt && ok; i++) {
        uint32_t off = PU32(d + 4 + 4 * i), x = PU32(d + off);
        const uint8_t *h = d + off + (x ? x : 4);
        uint32_t packed = PU32(h), unp = PU32(h + 4);
        const uint8_t *src = h + 12 + PU16(h + 10);
        if (h[8] != 1) continue;
        if (src + packed > d + n) { ok = 0; break; }
        uint8_t *out = malloc(unp);
        explode(src, packed, out, unp, h[9]);
        free(out);
        if (explode_overrun) ok = 0;
    }
    free(d);
    return ok;
}

void intro_host_run(void) {
    if (!getenv("ETERNAM_FORCE_INTRO") && !intro_data_intact()) {
        fprintf(stderr, "Eternam: INTRO.PAK is damaged (truncated SLIDE.VA2) - skipping the intro\n");
        return;
    }
    size_t len;
    uint8_t *exe = cc_load_entry("AVE.CC1", 2, &len);
    if (!exe) return;
    uint16_t last = PU16(exe + 2), pages = PU16(exe + 4), nrel = PU16(exe + 6);
    uint16_t hdrpar = PU16(exe + 8), reloff = PU16(exe + 0x18);
    size_t size = (size_t)(pages - 1) * 512 + (last ? last : 512);
    const uint8_t *img = exe + hdrpar * 16u;
    size_t img_len = size - hdrpar * 16u, dsl = INTRO_DGROUP * 16u;

    uint8_t *ids = arena_alloc(0x10000);            /* 64K-aligned: tokens match seg:off */
    memcpy(ids, img + dsl, img_len - dsl);

    uint8_t *game_ds = g_ds;
    g_ds = ids;
    for (unsigned i = 0; i < nrel; i++) {           /* far pointers to the intro's own data */
        uint16_t off = PU16(exe + reloff + 4 * i), seg = PU16(exe + reloff + 4 * i + 2);
        size_t lin = seg * 16u + off;
        if (lin < dsl + 2 || lin >= img_len) continue;
        if (PU16(img + lin) == INTRO_DGROUP) DSPTR_SET(lin - 2 - dsl, ids + PU16(img + lin - 2));
    }
    free(exe);

    intro_run();

    g_ds = game_ds;
    plat_set_timer_isr(NULL);
    plat_set_kbd_isr(NULL);
    memset(g_vram, 0, SCREEN_W * SCREEN_H);
    arena_free(ids);
}
