/*
 * Intro segment 032c: DOS file handle wrappers (open/read/seek/close), native stdio.
 * The original returns the DOS handle in dx:ax (0 = failure); here the FILE * is the handle.
 * DGROUP: 0x150 fptr "insert disk" retry callback (never set by the intro, always NULL),
 *         0x154 saved drive, 0x156 last DOS error.
 */
#include "../core/mem.h"
#include "../core/loader.h"
#include "../platform/platform.h"
#include "intro_protos.h"

/* 032c:000a  lseek(fh, off, whence + 1): whence -1 = SEEK_SET, 0 = SEEK_CUR, 1 = SEEK_END;
 * returns 0 on success, -1 on error */
int32_t intro_032c_000a(FILE *fh, int32_t off, int32_t whence)
{
    int w = (int)(uint16_t)(whence + 1);        /* al = low byte of whence + 1 */
    int sw = (w & 0xff) == 0 ? SEEK_SET : (w & 0xff) == 1 ? SEEK_CUR : SEEK_END;
    if (!fh || fseek(fh, (long)off, sw) != 0) return -1;
    return 0;
}

/* 032c:0035  opens a file: mode 0x3ee creates it, 0x4d2 opens read/write, anything else
 * (0x3ed) read-only; on failure stores the error in DS16(0x156) and returns NULL (the
 * "insert disk" retry through DSPTR(0x150) is dead code in the intro) */
FILE *intro_032c_0035(const char *name, uint16_t mode)
{
    FILE *f;
    if (mode == 0x3ee) {
        f = data_fopen(name, "wb");
        if (!f) { DS16(0x156) = 5; return NULL; }
        return f;
    }
    f = data_fopen(name, mode == 0x4d2 ? "r+b" : "rb");
    if (!f) {
        DS16(0x156) = 2;                        /* DOS "file not found" */
        if (DSU32(0x150) != 0) {
            /* TODO(port): the original retries on drive A:/B: and calls the far callback
             * DSPTR(0x150)(name); the intro never installs one. */
        }
        return NULL;
    }
    return f;
}

/* 032c:0195  reads len bytes (in chunks of 48000, like the original) into buf;
 * returns the number of bytes read (0 on a DOS error) */
int32_t intro_032c_0195(FILE *fh, uint8_t *buf, int32_t len)
{
    uint32_t total = 0, left = (uint32_t)len, chunk;
    size_t got;
    if (!fh) return 0;
    for (;;) {
        chunk = 0xbb80;
        if (left < 0xbb80) { chunk = left; left = 0; }   /* sub si,0xbb80 / sbb di,0 / jae */
        else left -= 0xbb80;
        got = fread(buf + total, 1, chunk, fh);
        if (ferror(fh)) return 0;
        total += (uint32_t)got;
        if (got != chunk) break;
        if (left == 0) break;
    }
    return (int32_t)total;
}

/* 032c:022f  closes the file */
int16_t intro_032c_022f(FILE *fh)
{
    if (!fh) return 0;
    fclose(fh);
    return 0;
}
