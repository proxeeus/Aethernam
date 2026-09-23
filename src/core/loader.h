/* Access to the original game data files. */
#ifndef ETERNAM_LOADER_H
#define ETERNAM_LOADER_H
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>

void  data_set_dir(const char *dir);
const char *data_dir(void);
FILE *data_fopen(const char *name, const char *mode);   /* case-insensitive lookup in data dir */
FILE *save_fopen(const char *name, const char *mode);   /* saves live in the user's App Support dir */
uint8_t *data_load(const char *name, size_t *len);

/* BPE packed multi-entry code container (*.CC1) used by the Tatou loader */
uint8_t *cc_load_entry(const char *name, int entry, size_t *len);
size_t bpe_unpack(const uint8_t *src, size_t srclen, uint8_t *dst, size_t dstcap);

/* PKWARE implode ("explode") as used by .PAK files; returns bytes written */
size_t explode(const uint8_t *src, size_t srclen, uint8_t *dst, size_t dstlen, int flags);
extern int explode_overrun;   /* last explode() ran out of input (truncated data) */

/* generated: C function for an original far code address */
void *code_ptr_lookup(uint16_t seg, uint16_t off);
#endif
