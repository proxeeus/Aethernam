/*
 * Memory model runtime: arena (DGROUP + game heap), far tokens, load image.
 *
 * At start-up the original executable (entry 3 of AVE.CC1, BPE packed) is
 * unpacked in memory; its data segment initialises DGROUP and its relocation
 * table tells us which DGROUP words are far pointers (to data or to code).
 */
#include "mem.h"
#include "loader.h"
#include "../platform/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

uint8_t *g_ds;
const uint8_t *g_img;
static size_t g_img_len;

#define DGROUP_SEG 0x1219u
#define STACK_SEG  0x156fu

/* ------------------------------------------------------------------ */
/* arena                                                               */
/* ------------------------------------------------------------------ */
#define ARENA_SIZE   (512u << 20)          /* reserved address space; pages commit lazily */
#define TOKEN_BASE   0x00010000u           /* token of arena byte 0 (keeps NULL == 0)       */
#define PAGE_TOKEN   0x80000000u           /* tokens for pointers outside the arena         */
#define FAKE_BASE    0x7e0000000000ull     /* unknown tokens (e.g. DOS pointers in old saves) */

static uint8_t *arena;
static uint8_t *heap_lo, *heap_hi;

/* 64 KiB page allocator: like DOS farmalloc, every block starts at offset 0 of its
 * own "segment", so the low word of a far token is the offset inside the block. */
#define PG        0x10000u
#define HEAP_OFF  0x20000u                  /* DGROUP+stack occupy the first 2 pages */
#define NPG       ((ARENA_SIZE - HEAP_OFF) / PG)
static uint16_t pg_len[NPG];               /* run length at the first page of a block, 0 = free */
static uint8_t  pg_used[NPG];
static uint32_t pg_rover;

int arena_owns(const void *p) { return (const uint8_t *)p >= arena && (const uint8_t *)p < arena + ARENA_SIZE; }

void *arena_alloc(uint32_t size) {
    uint32_t n = size ? (size + PG - 1) / PG : 1;
    for (uint32_t pass = 0; pass < 2; pass++) {
        uint32_t i = pass ? 0 : pg_rover;
        uint32_t lim = pass ? pg_rover + n : NPG;
        if (lim > NPG) lim = NPG;
        while (i + n <= lim) {
            uint32_t k = 0;
            while (k < n && !pg_used[i + k]) k++;
            if (k == n) {
                memset(pg_used + i, 1, n);
                pg_len[i] = (uint16_t)n;
                pg_rover = i + n;
                uint8_t *p = heap_lo + (size_t)i * PG;
                memset(p, 0, size);
                return p;
            }
            i += k + 1;
        }
    }
    return NULL;
}

void arena_free(void *p) {
    if (!p || (uint8_t *)p < heap_lo || (uint8_t *)p >= heap_hi) return;
    uint32_t i = (uint32_t)(((uint8_t *)p - heap_lo) / PG);
    uint32_t n = pg_len[i];
    if (!n) return;
    memset(pg_used + i, 0, n);
    pg_len[i] = 0;
    madvise(heap_lo + (size_t)i * PG, (size_t)n * PG, MADV_FREE);
}

void *arena_realloc(void *p, uint32_t size) {
    if (!p) return arena_alloc(size);
    uint32_t i = (uint32_t)(((uint8_t *)p - heap_lo) / PG);
    if ((uint32_t)pg_len[i] * PG >= size) return p;
    void *q = arena_alloc(size);
    if (q) { memcpy(q, p, (size_t)pg_len[i] * PG); arena_free(p); }
    return q;
}

uint32_t arena_avail(void) {
    uint32_t best = 0, run = 0;
    for (uint32_t i = 0; i < NPG; i++) {
        run = pg_used[i] ? 0 : run + 1;
        if (run > best) best = run;
    }
    return best > 0xffff ? 0xffffffffu : best * PG;
}

/* ------------------------------------------------------------------ */
/* far tokens                                                          */
/* ------------------------------------------------------------------ */
#define MAX_PAGES 4096
static uintptr_t page_base[MAX_PAGES];
static int       npages;

uint32_t fp_token(const void *p) {
    if (!p) return 0;
    const uint8_t *q = p;
    if (q >= arena && q < arena + ARENA_SIZE) return (uint32_t)(q - arena) + TOKEN_BASE;
    uintptr_t u = (uintptr_t)p;
    if (u >= FAKE_BASE && u < FAKE_BASE + 0x100000000ull) return (uint32_t)(u - FAKE_BASE);
    uintptr_t pg = u & ~(uintptr_t)0xffff;
    for (int i = 0; i < npages; i++)
        if (page_base[i] == pg) return PAGE_TOKEN | (uint32_t)i << 16 | (uint32_t)(u & 0xffff);
    if (npages >= MAX_PAGES) plat_fatal("far pointer page table full");
    page_base[npages] = pg;
    return PAGE_TOKEN | (uint32_t)npages++ << 16 | (uint32_t)(u & 0xffff);
}

uint8_t *fp_ptr(uint32_t t) {
    if (t == 0) return NULL;
    if (t >= TOKEN_BASE && t < TOKEN_BASE + ARENA_SIZE) return arena + (t - TOKEN_BASE);
    if ((t & PAGE_TOKEN) && ((t >> 16) & 0x7fff) < (uint32_t)npages)
        return (uint8_t *)(page_base[(t >> 16) & 0x7fff] | (t & 0xffff));
    return (uint8_t *)(uintptr_t)(FAKE_BASE + t);   /* e.g. a DOS seg:off from an original save */
}

/* ------------------------------------------------------------------ */
/* start-up                                                            */
/* ------------------------------------------------------------------ */
void mem_init(void) {
    arena = mmap(NULL, ARENA_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (arena == MAP_FAILED) plat_fatal("cannot reserve memory");
    g_ds = arena;                 /* token low word == original DGROUP offset */
    heap_lo = arena + HEAP_OFF;
    heap_hi = arena + ARENA_SIZE;

    size_t exe_len;
    uint8_t *exe = cc_load_entry("AVE.CC1", 3, &exe_len);
    if (!exe) plat_fatal("Cannot read AVE.CC1 from the game data folder.");

    const uint8_t *h = exe;
    uint16_t last = PU16(h + 2), pages = PU16(h + 4), nrel = PU16(h + 6);
    uint16_t hdrpar = PU16(h + 8), reloff = PU16(h + 0x18);
    size_t size = (size_t)(pages - 1) * 512 + (last ? last : 512);
    g_img_len = size - hdrpar * 16u;
    uint8_t *img = malloc(g_img_len);
    memcpy(img, exe + hdrpar * 16u, g_img_len);
    g_img = img;

    const size_t dsl = DGROUP_SEG * 16u;
    memcpy(g_ds, img + dsl, g_img_len - dsl);

    /* far pointers stored in initialised data */
    for (unsigned i = 0; i < nrel; i++) {
        uint16_t off = PU16(h + reloff + 4 * i), seg = PU16(h + reloff + 4 * i + 2);
        size_t lin = seg * 16u + off;
        if (lin < dsl + 2 || lin >= g_img_len) continue;
        uint16_t sval = PU16(img + lin), oval = PU16(img + lin - 2);
        size_t slot = lin - 2 - dsl;
        if (sval == DGROUP_SEG) DSPTR_SET(slot, g_ds + oval);
        else if (sval == STACK_SEG) DSPTR_SET(slot, g_ds + SS_BASE + oval);
        else {
            void *fn = code_ptr_lookup(sval, oval);
            if (!fn) fprintf(stderr, "mem_init: no C function for code ptr %04x:%04x (ds:%04zx)\n", sval, oval, slot);
            DSPTR_SET(slot, fn);
        }
    }
    free(exe);

    /* the VGA framebuffer lives in the arena too, so far pointers to it are ordinary tokens */
    g_vram = arena_alloc(0x10000);   /* 64000 + slack: some original copies overrun by a few bytes */

    /* row offset table at the bottom of the stack segment (ss:[y*2] = y*320) */
    for (int y = 0; y < 200; y++) SSU16(y * 2) = (uint16_t)(y * 320);
}
