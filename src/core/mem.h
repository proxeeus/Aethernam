/*
 * Eternam native port - memory model shared by all decompiled code.
 *
 * The original program is a Turbo C 2.0 large-model DOS executable.  We keep
 * its global data segment (DGROUP, segment 0x1219) as a byte image so that
 * table layouts, initialised data and save-game blocks behave exactly as in
 * the original.  Everything else (code, locals) is plain native C.
 *
 * Far pointers that the game keeps *in memory* (DGROUP globals, pointer
 * tables inside loaded resources, save-game blocks...) are stored in their
 * original 4 bytes as a 32-bit "far token".  All game heap memory and DGROUP
 * live in one arena, so a token is simply an arena offset: copying memory
 * copies pointers, equality works, and 16-bit offset arithmetic on the low
 * word behaves like the original seg:off arithmetic.  Pointers outside the
 * arena (C stack arrays, C functions) get tokens through a 64K-page map.
 */
#ifndef ETERNAM_MEM_H
#define ETERNAM_MEM_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* unaligned-safe little-endian scalar types (Apple Silicon is little endian) */
typedef int16_t  __attribute__((aligned(1), may_alias)) i16u;
typedef uint16_t __attribute__((aligned(1), may_alias)) u16u;
typedef int32_t  __attribute__((aligned(1), may_alias)) i32u;
typedef uint32_t __attribute__((aligned(1), may_alias)) u32u;

/* ---- raw memory access through a byte pointer ------------------------- */
#define P8(p)    (*(uint8_t *)(p))
#define PS8(p)   (*(int8_t *)(p))
#define P16(p)   (*(i16u *)(p))          /* signed 16-bit lvalue   */
#define PU16(p)  (*(u16u *)(p))          /* unsigned 16-bit lvalue */
#define P32(p)   (*(i32u *)(p))
#define PU32(p)  (*(u32u *)(p))

/* ---- DGROUP ------------------------------------------------------------- */
#define DS_SIZE   0x14000u               /* DGROUP + stack segment */
#define SS_BASE   0x3560u                /* SS (0x156f) = DGROUP + 0x356 paragraphs */
extern uint8_t *g_ds;                    /* lives inside the arena */

#define DS8(a)    P8(g_ds + (a))
#define DSS8(a)   PS8(g_ds + (a))
#define DS16(a)   P16(g_ds + (a))
#define DSU16(a)  PU16(g_ds + (a))
#define DS32(a)   P32(g_ds + (a))
#define DSU32(a)  PU32(g_ds + (a))
#define DSADDR(a) (g_ds + (a))           /* ds:a as a byte pointer (e.g. push ds / push a) */
#define DSSTR(a)  ((char *)(g_ds + (a)))
/* data addressed through SS (bottom of the stack segment: row table, clip rect) */
#define SS16(a)   DS16(SS_BASE + (a))
#define SSU16(a)  DSU16(SS_BASE + (a))
#define SSADDR(a) (g_ds + SS_BASE + (a))

/* ---- far pointers stored in memory ------------------------------------- */
uint32_t fp_token(const void *p);        /* native pointer -> 32-bit far token (NULL -> 0) */
uint8_t *fp_ptr(uint32_t token);         /* far token -> native pointer */
static inline uint8_t *fp_get(const void *loc) { return fp_ptr(PU32(loc)); }
static inline void fp_set(void *loc, const void *p) { PU32(loc) = fp_token(p); }

#define FP(loc)           fp_get(loc)           /* read far pointer stored at loc  */
#define FP_SET(loc, p)    fp_set((loc), (p))    /* store far pointer at loc        */
#define DSPTR(a)          fp_get(g_ds + (a))
#define DSPTR_SET(a, p)   fp_set(g_ds + (a), (p))

/* far code pointers (function tables in DGROUP) are stored the same way */
#define DSFN(type, a)     ((type)(void *)DSPTR(a))

/* ---- game heap (farmalloc & co.) --------------------------------------- */
void    *arena_alloc(uint32_t size);     /* NULL when exhausted */
void     arena_free(void *p);
void    *arena_realloc(void *p, uint32_t size);
uint32_t arena_avail(void);              /* largest free block (farcoreleft) */
int      arena_owns(const void *p);

/* ---- original load image (code segments hold some constant tables) ----- */
extern const uint8_t *g_img;             /* whole MZ load image, read-only */
#define CS8(seg, off)   P8(g_img + (seg) * 16 + (off))
#define CS16(seg, off)  P16(g_img + (seg) * 16 + (off))
#define CSADDR(seg, off) (g_img + (seg) * 16 + (off))

void mem_init(void);                     /* arena, DGROUP image, pointer relocs */

#endif
