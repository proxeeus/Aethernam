/*
 * Game data access + the two decompressors of the original:
 *  - Tatou BPE (byte pair encoding) for executables in *.CC1  (TATOU.COM 0x3ef)
 *  - PKWARE implode for *.PAK resources                        (AVE 0f48:0314)
 */
#include "loader.h"
#include "../platform/platform.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <sys/stat.h>

static char g_data_dir[1024] = ".";

void data_set_dir(const char *dir) { snprintf(g_data_dir, sizeof g_data_dir, "%s", dir); }
const char *data_dir(void) { return g_data_dir; }

static FILE *open_ci(const char *dir, const char *name, const char *mode) {
    char path[2048];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE *f = fopen(path, mode);
    if (f) return f;
    DIR *d = opendir(dir);
    if (!d) return NULL;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (strcasecmp(e->d_name, name) == 0) {
            snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
            f = fopen(path, mode);
            break;
        }
    }
    closedir(d);
    if (!f && strchr(mode, 'w')) {           /* create with the requested (upper) case */
        snprintf(path, sizeof path, "%s/%s", dir, name);
        f = fopen(path, mode);
    }
    return f;
}

FILE *data_fopen(const char *name, const char *mode) { return open_ci(g_data_dir, name, mode); }

FILE *save_fopen(const char *name, const char *mode) {
    FILE *f = open_ci(plat_save_dir(), name, mode);
    if (!f && !strchr(mode, 'w')) f = open_ci(g_data_dir, name, mode);  /* original saves shipped with the game */
    return f;
}

uint8_t *data_load(const char *name, size_t *len) {
    FILE *f = data_fopen(name, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc(n ? n : 1);
    if (fread(b, 1, n, f) != (size_t)n) { free(b); fclose(f); return NULL; }
    fclose(f);
    if (len) *len = n;
    return b;
}

/* ---------------------------------------------------------------- BPE */
size_t bpe_unpack(const uint8_t *src, size_t srclen, uint8_t *dst, size_t dstcap) {
    size_t sp = 0, dp = 0;
    uint8_t code[256], left[256], right[256], head[256], next[256];
    uint8_t stk[512][2];
    for (;;) {
        if (sp + 4 > srclen) break;
        unsigned npairs = src[sp], more = src[sp + 1], len = src[sp + 2] | src[sp + 3] << 8;
        sp += 4;
        if (npairs == 0) {
            for (unsigned i = 0; i < len && dp < dstcap; i++) dst[dp++] = src[sp++];
        } else {
            memset(head, 0, sizeof head);
            memcpy(code + 1, src + sp, npairs); sp += npairs;
            memcpy(left + 1, src + sp, npairs); sp += npairs;
            memcpy(right + 1, src + sp, npairs); sp += npairs;
            for (unsigned i = 1; i <= npairs; i++) { next[i] = head[code[i]]; head[code[i]] = (uint8_t)i; }
            for (unsigned n = 0; n < len; n++) {
                int top = 0;
                stk[top][0] = src[sp++]; stk[top][1] = 0; top++;   /* limit 0 = none */
                while (top) {
                    top--;
                    uint8_t c = stk[top][0], lim = stk[top][1];
                    unsigned k = head[c];
                    while (k && lim && k >= lim) k = next[k];
                    if (!k) { if (dp < dstcap) dst[dp++] = c; }
                    else {
                        stk[top][0] = right[k]; stk[top][1] = (uint8_t)k; top++;
                        stk[top][0] = left[k];  stk[top][1] = (uint8_t)k; top++;
                    }
                }
            }
        }
        if (!more) break;
    }
    return dp;
}

static uint32_t be32(const uint8_t *p) { return (uint32_t)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]; }

uint8_t *cc_load_entry(const char *name, int entry, size_t *len) {
    size_t flen;
    uint8_t *f = data_load(name, &flen);
    if (!f) return NULL;
    unsigned n = f[0] << 8 | f[1];
    if (entry >= (int)n) { free(f); return NULL; }
    size_t off = be32(f + 2 + 4 * entry) + 2 + 4 * n;
    uint32_t packed = be32(f + off), unpacked = be32(f + off + 4);
    uint8_t *out = malloc(unpacked);
    size_t got = bpe_unpack(f + off + 8, packed, out, unpacked);
    free(f);
    if (got != unpacked) { free(out); return NULL; }
    if (len) *len = unpacked;
    return out;
}

/* ------------------------------------------------------------ implode */
typedef struct { const uint8_t *p, *end; uint32_t buf; int n; } bits_t;
int explode_overrun;                       /* set when a stream needed bytes past its end */

static unsigned getbits(bits_t *b, int n) {
    while (b->n < n) { if (b->p >= b->end) explode_overrun = 1; b->buf |= (uint32_t)(b->p < b->end ? *b->p++ : 0) << b->n; b->n += 8; }
    unsigned v = b->buf & ((1u << n) - 1);
    b->buf >>= n; b->n -= n;
    return v;
}

/* Shannon-Fano tree exactly as built by the original (sorted by length then value) */
typedef struct { uint16_t child[2]; uint8_t leaf[2]; } sfnode;
typedef struct { sfnode node[600]; int count; } sftree;

static const uint8_t *read_tree(const uint8_t *p, int n, sftree *t) {
    struct { uint16_t code; uint8_t val, len; } e[256];
    int cnt = *p++ + 1, k = 0;
    for (int i = 0; i < cnt; i++) {
        uint8_t b = *p++;
        for (int r = 0; r < (b >> 4) + 1 && k < n; r++) { e[k].val = (uint8_t)k; e[k].len = (b & 15) + 1; k++; }
    }
    n = k;
    for (int i = 1; i < n; i++) {           /* insertion sort by (len, val) */
        __typeof__(e[0]) x = e[i]; int j = i - 1;
        while (j >= 0 && (e[j].len > x.len || (e[j].len == x.len && e[j].val > x.val))) { e[j + 1] = e[j]; j--; }
        e[j + 1] = x;
    }
    uint16_t code = 0, inc = 0; uint8_t last = 0;
    for (int i = n - 1; i >= 0; i--) {
        code += inc;
        if (e[i].len != last) { last = e[i].len; inc = (uint16_t)(1u << (16 - last)); }
        uint16_t r = 0;
        for (int b = 0; b < 16; b++) if (code & (1u << b)) r |= (uint16_t)(1u << (15 - b));
        e[i].code = r;
    }
    memset(t, 0, sizeof *t); t->count = 1;
    for (int i = 0; i < n; i++) {
        unsigned c = e[i].code, cur = 0, parent = 0, bit = 0;
        for (int l = 0; l < e[i].len; l++) {
            bit = c & 1; c >>= 1;
            int s = bit ? 0 : 1;
            if (t->node[cur].child[s] == 0 || t->node[cur].leaf[s]) {
                t->node[cur].child[s] = (uint16_t)t->count;
                t->node[cur].leaf[s] = 0;
                memset(&t->node[t->count], 0, sizeof(sfnode));
                t->count++;
            }
            parent = cur; cur = t->node[cur].child[s];
        }
        int s = bit ? 0 : 1;
        t->node[parent].child[s] = e[i].val; t->node[parent].leaf[s] = 1;
    }
    return p;
}

static unsigned decode(const sftree *t, bits_t *b) {
    unsigned cur = 0;
    for (;;) {
        int s = getbits(b, 1) ? 0 : 1;
        if (t->node[cur].leaf[s]) return t->node[cur].child[s];
        cur = t->node[cur].child[s];
    }
}

size_t explode(const uint8_t *src, size_t srclen, uint8_t *dst, size_t dstlen, int flags) {
    static sftree lit, len, dist;
    const uint8_t *p = src;
    explode_overrun = 0;
    int minlen = 2, dbits = (flags & 2) ? 7 : 6, haslit = flags & 4;
    if (haslit) { minlen = 3; p = read_tree(p, 256, &lit); }
    p = read_tree(p, 64, &len);
    p = read_tree(p, 64, &dist);
    bits_t b = { p, src + srclen, 0, 0 };
    size_t o = 0;
    while (o < dstlen) {
        if (getbits(&b, 1)) {
            dst[o++] = (uint8_t)(haslit ? decode(&lit, &b) : getbits(&b, 8));
        } else {
            unsigned low = getbits(&b, dbits);
            size_t d = ((decode(&dist, &b) << dbits) | low) + 1;
            unsigned l = decode(&len, &b) + minlen;
            if (l == 63u + minlen) l += getbits(&b, 8);
            while (l-- && o < dstlen) { dst[o] = o >= d ? dst[o - d] : 0; o++; }
        }
    }
    return o;
}
