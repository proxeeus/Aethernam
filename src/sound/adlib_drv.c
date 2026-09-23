/*
 * Infogrames "IFGM ADLIB" resident sound driver (AVE.CC1 entry 0), reimplemented.
 * See re/notes/adlib_driver.md for the analysis.
 *
 * The driver's own segment (CS = DS, a .COM image at org 0x100) is kept as a
 * 64 KiB byte array `M`, initialised from the real driver image in AVE.CC1, and
 * the code below is a literal translation that addresses its voice structures,
 * shadows and tables by their original offsets.  This keeps every quirk of the
 * original (tables read with out-of-range indices, fn 4 clearing the melodic
 * flag, SFX stop leaving the music channel muted, no looping, ...).
 *
 * Far pointers the driver keeps (track pointers, instrument banks) are stored
 * in M as offset + "segment"; the segment word is a small region handle
 * (1 = song passed to fn 6, 2 = SFX bank of fn 8, 3 = instrument of fn 0x18)
 * that maps to a native buffer.  All reads through such a pointer are 16-bit
 * wrap-around offsets inside that buffer and return 0 outside it.
 *
 * Threading: adlib_drv_tick() runs on the audio thread with the audio lock
 * held; adlib_call() takes the lock itself.
 */
#include "audio.h"
#include "opl.h"
#include "../core/loader.h"
#include "../core/mem.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------------ */
/* driver segment image and accessors                                        */
/* ------------------------------------------------------------------------ */
static uint8_t M[0x10000];
static int     g_ready;                 /* 0 = not initialised, 1 = ok, -1 = unavailable */

static inline uint8_t  B(uint16_t o)              { return M[o]; }
static inline void     SB(uint16_t o, uint8_t v)  { M[o] = v; }
static inline uint16_t W(uint16_t o)              { return (uint16_t)(M[o] | M[(uint16_t)(o + 1)] << 8); }
static inline void     SW(uint16_t o, uint16_t v) { M[o] = (uint8_t)v; M[(uint16_t)(o + 1)] = (uint8_t)(v >> 8); }

/* variables in the driver segment */
enum {
    SFX_VOICES = 0x103, MUS_VOICES = 0x258, HW_SHADOW = 0x3ad, VOICE_END = 0x3ad,
    V_SIZE = 0x1f,
    OPL_PORT = 0x41b, OPTAB_MELODIC = 0x41d, OPTAB_RHYTHM = 0x435, FNUM_ROWS = 0x44b,
    OPCODES = 0x6bb, OP_TO_CH = 0x6cf, RHYTHM_BITS = 0x6e5, CALLBACK = 0x6ea,
    PAUSED = 0x6ee, MUS_BANK = 0x6fa, SFX_INST = 0x6fe, SFX_BASE = 0x702,
    CUR_BANK = 0x70a, OPTAB = 0x70e, BD_SHADOW = 0x710, MELODIC = 0x711,
};

/* ------------------------------------------------------------------------ */
/* regions (the "segments" of the far pointers the driver stores)            */
/* ------------------------------------------------------------------------ */
enum { SEG_NONE = 0, SEG_SONG = 1, SEG_SFX = 2, SEG_INSTR = 3, NSEG = 4 };
static struct { const uint8_t *base; uint32_t len; } g_seg[NSEG];

static void set_region(int seg, const uint8_t *p, uint32_t fallback_len) {
    g_seg[seg].base = p;
    /* game buffers live in the arena: allow the whole 64 KiB segment like DOS did */
    g_seg[seg].len = (p && arena_owns(p)) ? 0x10000u : fallback_len;
}

static inline uint8_t ES8(uint16_t seg, uint16_t off) {
    if (seg >= NSEG || !g_seg[seg].base || off >= g_seg[seg].len) return 0;
    return g_seg[seg].base[off];
}
static inline uint16_t ES16(uint16_t seg, uint16_t off) {
    return (uint16_t)(ES8(seg, off) | ES8(seg, (uint16_t)(off + 1)) << 8);
}

/* ------------------------------------------------------------------------ */
/* OPL                                                                        */
/* ------------------------------------------------------------------------ */
/* 0x739: write register al with value ah (plus status-read delays) */
static void sub_0739(uint8_t reg, uint8_t val) { opl_write_reg(reg, val); }

/* 0x712: reset the hardware shadow of channel ax (ax = al * 10) */
static void sub_0712(uint16_t ax) {
    uint16_t si = (uint16_t)(HW_SHADOW + (uint8_t)ax * 10);
    SB(si + 5, 0xff);
    SB(si + 4, 0xff);
    SW(si + 2, 0x40);
    SW(si, 0xffff);
    SB(si + 7, 0x9c);
    SW(si + 8, 0xffff);
}

/* 0x774: set the total level of operator `op` from instrument byte es:[si+1]
 * scaled by volume bp */
static void sub_0774(uint8_t op, uint16_t bp, uint16_t es, uint16_t si) {
    if (op == 0xff) return;
    si++;
    uint32_t prod = (uint32_t)(ES8(es, si) & 0x3f) * bp;
    uint16_t dx = (uint16_t)(prod >> 16), ax = (uint16_t)prod;
    uint16_t b2 = (uint16_t)(ax * 2 + 0x7f);
    uint16_t q = (uint16_t)((((uint32_t)dx << 16) | b2) / 0xfe);
    uint8_t dl = (uint8_t)(0x3f - q);
    uint8_t ah = (uint8_t)((ES8(es, si) & 0xc0) | dl);
    sub_0739((uint8_t)(0x40 + op), ah);
}

/* 0x843: set frequency of channel ch for note cl with bend bp; key on unless
 * flags & 0x40 or note bit 7.  bp bit 15 = legato (no key-off first). */
static void sub_0843(uint8_t ch, uint8_t cl, uint16_t bp, uint16_t dx) {
    if (!(bp & 0x8000)) sub_0739((uint8_t)(0xb0 + ch), 0);
    uint16_t di = W((uint16_t)(FNUM_ROWS + (cl & 0x0f) * 2));
    if ((bp & 0x80) && !(cl & 0x0f)) {
        di = OPCODES;                              /* end of the B row */
        if (cl & 0x70) cl = (uint8_t)(cl - 0x10);
    }
    if (cl & 0x80) dx = 0x40;
    di = (uint16_t)(di + (int16_t)(int8_t)(uint8_t)bp * 2);
    uint16_t fn = W(di);
    sub_0739((uint8_t)(0xa0 + ch), (uint8_t)fn);
    uint8_t ah = (uint8_t)(((cl & 0x70) >> 2) + (fn >> 8));
    if (!(dx & 0x40)) ah |= 0x20;
    sub_0739((uint8_t)(0xb0 + ch), ah);
}

/* 0x7b4: key a note on channel ch (melodic, or rhythm handling for ch >= 6) */
static void sub_07b4(uint8_t ch, uint8_t cl, uint16_t bp, uint16_t dx) {
    if (B(MELODIC) || ch < 6) { sub_0843(ch, cl, bp, dx); return; }
    uint16_t di = dx;
    if (ch == 6) {
        sub_0843(ch, cl, bp, 0x40);
    } else if (ch == 8 && !(cl & 0x80)) {
        sub_0843(8, cl, bp, 0x40);
        uint8_t al = cl & 0x70, c = (uint8_t)((cl & 0x0f) + 7);
        if (c >= 12) { c -= 12; if (al != 0x70) al = (uint8_t)(al + 0x10); }
        sub_0843(7, c | al, bp, 0x40);             /* snare/hi-hat pitch = tom + fifth */
    }
    dx = di;
    uint8_t bit = B((uint16_t)(RHYTHM_BITS + (uint8_t)(ch - 6)));
    uint8_t ah = (uint8_t)(~bit & B(BD_SHADOW));
    sub_0739(0xbd, ah);
    if (!(dx & 0x40) && !(cl & 0x80)) { ah |= bit; sub_0739(0xbd, ah); }
    SB(BD_SHADOW, ah);
}

/* 0x8b4: program operator `op` from the 6-byte operator record at es:si
 * (uses si+1..si+5); returns si + 6 */
static uint16_t sub_08b4(uint8_t op, uint16_t es, uint16_t si) {
    uint8_t c = B((uint16_t)(OP_TO_CH + op));
    if (c != 0xff) sub_0739((uint8_t)(0xc0 + c), ES8(es, si + 2));
    sub_0739((uint8_t)(0x60 + op), ES8(es, si + 4));
    sub_0739((uint8_t)(0x80 + op), ES8(es, si + 5));
    sub_0739((uint8_t)(0x20 + op), ES8(es, si + 1));
    sub_0739((uint8_t)(0xe0 + op), ES8(es, si + 3));
    return (uint16_t)(si + 6);
}

/* 0x968: frequency/key registers of channel dl = 0 */
static void sub_0968(uint8_t dl) {
    sub_0739((uint8_t)(0xa0 + dl), 0);
    sub_0739((uint8_t)(0xb0 + dl), 0);
}

/* ------------------------------------------------------------------------ */
/* int F0 fn 4 (0x8fe): chip reset / silence                                 */
/* ------------------------------------------------------------------------ */
static uint8_t song_melodic;                       /* port: melodic flag of the loaded song */

static void adl_fn04_chip_reset(void) {
    /* The original clears the melodic flag here. The game always calls fn 4 right after
       loading a song (fn 6), so in melodic songs (the AETERNAM title theme) channels 6-8
       were routed to the rhythm path and never sounded. The port keeps the flag of the
       loaded song so the whole arrangement plays (ETERNAM_ORIGINAL_MUSIC_BUG=1 restores
       the original behaviour). */
    static int original = -1;
    if (original < 0) original = getenv("ETERNAM_ORIGINAL_MUSIC_BUG") != NULL;
    SB(MELODIC, original ? 0 : song_melodic);
    sub_0739(0xbd, B(BD_SHADOW));
    sub_0739(0x60, 0x04);        /* original writes reg 0x60/0x80 (verified against the driver binary) */
    sub_0739(0x80, 0x04);
    sub_0739(0x08, 0x00);
    sub_0739(0x01, 0x20);
    for (uint8_t d = 0; d < 9; d++) sub_0968(d);
    for (uint16_t d = 0; d < 0x0b; d++) sub_0712(d);
    sub_0968(6);
    sub_0968(7);
    sub_0968(8);
    sub_0843(8, 0x00, 0, 0x40);
    sub_0843(7, 0x07, 0, 0x40);
}

/* ------------------------------------------------------------------------ */
/* sequencer                                                                  */
/* ------------------------------------------------------------------------ */
/* opcode handlers; bx = voice, cl = param, es:si = read pointer after the event */
static void op_0a08_end(uint16_t bx) {
    SW(bx + 4, W(bx + 4) | 2);
    if (W(bx + 4) & 0x20) return;
    SW(bx + 4, W(bx + 4) | 0x40);
    if (W(bx + 4) & 0x8000) {
        uint16_t si = W(bx + 2);
        SW(si + 4, W(si + 4) & 0xfffb);            /* give the channel back to the music */
    }
}

static void dispatch_op(uint16_t handler, uint16_t bx, uint16_t cx, uint16_t es, uint16_t si) {
    switch (handler) {
    case 0x0a08: op_0a08_end(bx); break;
    case 0x0a27: SB(bx + 0x17, (uint8_t)cx); break;                      /* bend */
    case 0x0a2b: SW(bx + 0x18, W(bx + 0x18) + 1); SW(bx + 0x15, cx); break; /* note */
    case 0x0a32: {                                                         /* step duration */
        uint8_t al = ES8(es, si++);
        cx = (uint16_t)(((uint16_t)al << 8 | (cx & 0xff)) + W(bx + 0x13));
        SW(bx + 0x10, cx);
        SW(bx + 0x0e, cx);
        SW(bx + 0x0a, si);
        SW(bx + 0x0c, es);
        break;
    }
    case 0x0a46: SB(bx + 0x1e, (uint8_t)cx); break;                      /* attenuation */
    case 0x0a4a: SB(bx + 0x12, (uint8_t)cx); break;                      /* instrument */
    case 0x09f7: /* far call through [0x6ea]: NULL in the game (would crash) - ignored */
    case 0x0a26: default: break;
    }
}

/* 0x979: advance one voice by one tick */
static void sub_0979(uint16_t bx) {
    uint16_t ax = W(bx + 4);
    if (ax & 0x40) return;
    if (ax & 2) {                                  /* (re)start */
        SW(bx + 0x0a, W(bx + 6));
        SW(bx + 0x0c, W(bx + 8));
        SW(bx + 4, W(bx + 4) & 0xfffd);
        SW(bx + 0x18, 0);
    } else {
        uint8_t cl = B(bx + 0x1a);
        if (cl != B(bx + 0x1d)) {                  /* volume fade */
            SB(bx + 0x1b, (uint8_t)(B(bx + 0x1b) - 1));
            if (B(bx + 0x1b) & 0x80) {
                SB(bx + 0x1b, B(bx + 0x1c));
                uint8_t d = (cl > B(bx + 0x1d)) ? 1 : 0xff;
                uint8_t al = (uint8_t)(d + B(bx + 0x1d));
                if (al & 0x80) al = 0;
                if (al >= 0x7f) al = 0x7f;
                SB(bx + 0x1d, al);
            }
        }
        SW(bx + 0x0e, (uint16_t)(W(bx + 0x0e) - 1));
        if (W(bx + 0x0e) != 0) return;
        SW(bx + 0x0e, W(bx + 0x10));
    }
    for (;;) {                                     /* event batch */
        uint16_t es = W(bx + 0x0c), si = W(bx + 0x0a);
        uint16_t w = ES16(es, si);
        si = (uint16_t)(si + 2);
        SW(bx + 0x0a, si);
        SW(bx + 0x0c, es);
        uint8_t op = (uint8_t)w, cl = (uint8_t)(w >> 8);
        uint8_t di = (uint8_t)(op << 1);           /* carry = op bit 7 ends the batch */
        dispatch_op(W((uint16_t)(OPCODES + di)), bx, cl, es, si);
        if (op & 0x80) break;
    }
}

/* 0xaad: bring the OPL channel in line with voice si (shadow di) */
static void sub_0aad(uint16_t si, uint16_t di) {
    uint16_t ax = W(si + 4) & 0x40;
    if (ax != W(di + 2)) {
        SW(di + 2, ax);
        if (ax & 0x40) {                           /* just stopped: key off */
            sub_07b4(B(si), (uint8_t)(B(si + 0x15) | 0x80), B(si + 0x17), 0x40);
            sub_0712(W(si));
            return;
        }
    }
    if (ax & 0x40) return;

    ax = W(si + 4) & 0x8000;                       /* owner changed -> reprogram all */
    uint16_t cx = W(di + 8);
    if ((cx & 1) || ax != cx) {
        SW(di + 8, ax);
        SB(di + 5, 0xff);
        SB(di + 4, 0xff);
        SW(di, 0xffff);
        SB(di + 7, 0x9c);
    }
    cx = W((uint16_t)(W(si) * 2 + W(OPTAB)));
    if (cx == 0xffff) return;
    uint8_t op1 = (uint8_t)cx, op2 = (uint8_t)(cx >> 8);
    uint16_t bank_es = W(CUR_BANK + 2);

    uint8_t al = B(si + 0x12);
    if (al != B(di + 4)) {                         /* instrument */
        SB(di + 4, al);
        uint16_t p = (uint16_t)(W(CUR_BANK) + (uint16_t)(0x0d * B(si + 0x12)) + 1);
        p = sub_08b4(op1, bank_es, p);
        if (op2 != 0xff) sub_08b4(op2, bank_es, p);
        SB(di + 5, 0xff);
    }

    al = (uint8_t)(B(si + 0x1d) - B(si + 0x1e));   /* volume */
    if (al & 0x80) al = 0;
    if (al != B(di + 5)) {
        SB(di + 5, al);
        uint16_t dx = B(si + 0x1d);
        if (op2 == 0xff) dx = al;
        uint16_t p = (uint16_t)(W(CUR_BANK) + (uint16_t)(0x0d * B(si + 0x12)));
        sub_0774(op1, dx, bank_es, p);
        sub_0774(op2, al, bank_es, (uint16_t)(p + 6));
    }

    uint8_t dl = B(si + 0x17);                     /* note / bend */
    uint16_t bp = dl;
    ax = W(si + 0x15);
    if (dl != B(di + 7)) {
        SB(di + 7, dl);
        if (ax == W(di)) bp |= 0x8000;             /* bend only: legato */
    } else if (ax == W(di)) {
        return;
    }
    ax |= 0x8000;
    SW(di, ax);
    SW(si + 0x15, ax);
    sub_07b4(B(si), (uint8_t)ax, bp, W(si + 4));
}

/* ------------------------------------------------------------------------ */
/* int F0 functions                                                           */
/* ------------------------------------------------------------------------ */
/* fn 0 (0xa4e): one timer tick */
static void adl_fn00_tick(void) {
    if (B(PAUSED)) return;
    uint16_t di = HW_SHADOW;
    for (uint16_t bx = MUS_VOICES; bx < VOICE_END; bx += V_SIZE, di += 10) {
        uint16_t si = bx;
        SW(CUR_BANK, W(MUS_BANK));
        SW(CUR_BANK + 2, W(MUS_BANK + 2));
        sub_0979(bx);
        if (W(bx + 4) & 4) {                       /* an SFX owns this channel */
            SW(CUR_BANK, W(SFX_INST));
            SW(CUR_BANK + 2, W(SFX_INST + 2));
            si = W(bx + 2);
            sub_0979(si);
        }
        sub_0aad(si, di);
    }
}

/* fn 2 (0xe7e): stop everything, reset shadows and chip */
static void adl_fn02_reset_all(void) {
    uint16_t bx = MUS_VOICES;
    for (uint16_t cx = 0; cx < 0x0b; cx++) {
        SW(bx + 4, W(bx + 4) | 0x40);
        uint16_t si = W(bx + 2);
        SW(si + 4, W(si + 4) | 0x40);
        bx += V_SIZE;
        sub_0712(cx);
    }
    adl_fn04_chip_reset();
}

/* fn 6 (0xd9d): load song at es:si */
static void adl_fn06_load_song(uint16_t es, uint16_t si) {
    SW(OPTAB, OPTAB_MELODIC);
    uint8_t al = ES8(es, si + 0x3c), ah = ES8(es, si + 0x3d);
    SB(MELODIC, ah);
    song_melodic = ah;
    if (!ah) { al |= 0x20; SW(OPTAB, OPTAB_RHYTHM); }
    SB(BD_SHADOW, al);
    uint16_t bx = MUS_VOICES;
    for (uint16_t cx = 0; cx < 0x0b; cx++, bx += V_SIZE) {
        uint16_t d = (uint16_t)(si + cx * 4);
        uint16_t lo = ES16(es, d + 8), hi = ES16(es, d + 0x0a);
        if (!(lo | hi)) { SW(bx + 6, 0); SW(bx + 8, 0); }
        else            { SW(bx + 6, (uint16_t)(si + lo)); SW(bx + 8, es); }
        SW(bx + 4, W(bx + 4) | 0x40);
    }
    SW(MUS_BANK, (uint16_t)(si + ES16(es, si + 0x34)));
    SW(MUS_BANK + 2, es);
    SW(CUR_BANK + 2, es);
}

/* fn 8 (0xe1d): load SFX bank at es:si */
static void adl_fn08_load_sfx(uint16_t es, uint16_t si) {
    SW(SFX_BASE, si);
    SW(SFX_BASE + 2, es);
    SW(SFX_INST, (uint16_t)(si + ES16(es, si + 0x0a)));
    SW(SFX_INST + 2, es);
    for (uint16_t bx = SFX_VOICES; bx < MUS_VOICES; bx += V_SIZE) SW(bx + 4, W(bx + 4) | 0x40);
}

/* common tail of fn 0xa / 0xc: bits 0x20, 0x2000, 0x8000, (0x1000), 0x10 */
static void ctl_common(uint16_t bx, uint16_t dx, uint16_t *cx, uint16_t *si, int fade_speed) {
    if (dx & 0x20) SW(bx + 4, W(bx + 4) | 0x20);
    if ((dx & 0x2000) && !(dx & 0x10)) { *cx &= 0x7f; SB(bx + 0x1d, (uint8_t)*cx); SB(bx + 0x1a, (uint8_t)*cx); }
    if (dx & 0x8000) { *cx &= 0x7f; SB(bx + 0x1a, (uint8_t)*cx); }
    if (fade_speed && (dx & 0x1000)) { SB(bx + 0x1b, (uint8_t)*cx); SB(bx + 0x1c, (uint8_t)*cx); }
    if (dx & 0x10) {
        if (!(dx & 0x2000)) {
            if (!(W(bx + 4) & 0x40) && !((int16_t)*si > (int16_t)W(bx + 0x18))) *si = W(bx + 0x18);
        } else if (B(bx + 0x1d) != (uint8_t)*cx) {
            *si = 0;
        }
    }
}

/* fn 0xa (0xbca): music voice control; returns dx:ax */
static uint32_t adl_fn0a_music_ctl(uint16_t cx, uint16_t dx, uint16_t si) {
    uint16_t bp = si ? si : 0x7ff;
    si = 0xffff;
    uint16_t di = 1;
    for (uint16_t bx = MUS_VOICES; bx < VOICE_END; bx += V_SIZE, di <<= 1) {
        if (!(di & bp)) continue;
        if (!(W(bx + 6) | W(bx + 8))) continue;    /* no track */
        if (dx & 0x100) {
            if (cx) SB(PAUSED, 0);
            else { SB(PAUSED, 1); adl_fn04_chip_reset(); }
            if (dx & 0x400) SW(bx + 0x13, cx);
        }
        if ((dx & 0x40) && !(W(bx + 4) & 0x40)) SW(bx + 4, W(bx + 4) | 0x40);
        if (dx & 0x80) {
            SW(bx + 4, 0x40);
            SB(bx + 0x1d, 0x7f);
            SB(bx + 0x1a, 0x7f);
            SB(bx + 0x1e, 0);
            sub_0712(W(bx));
            SW(bx + 4, 2);                          /* restart, no loop */
        }
        ctl_common(bx, dx, &cx, &si, 1);
    }
    uint16_t ax = si, rdx = (ax & 0x8000) ? 0xffff : 0;
    if (dx & 0x200) { rdx = 0; ax = CALLBACK; }     /* fn 0x12 via bit 0x200 (cs unknown: 0) */
    return (uint32_t)rdx << 16 | ax;
}

/* fn 0xc (0xcc6): SFX voice control; returns ax (dx unchanged) */
static uint16_t adl_fn0c_sfx_ctl(uint16_t cx, uint16_t dx, uint16_t si) {
    uint16_t bp = si ? si : 0x7ff;
    si = 0xffff;
    uint16_t di = 1;
    for (uint16_t bx = SFX_VOICES; bx < MUS_VOICES; bx += V_SIZE, di <<= 1) {
        if (!(di & bp)) continue;
        if (dx & 0x400) SW(bx + 0x13, cx);
        if (dx & 0x40) {
            /* port fix: the original stopped the SFX voice without handing its OPL channel
               back to the music (only a natural end did), so that music voice stayed mute
               until the next song. Hand it back like op_0a08_end does. */
            if (!(W(bx + 4) & 0x40) && (W(bx + 4) & 0x8000) && !getenv("ETERNAM_ORIGINAL_MUSIC_BUG")) {
                uint16_t l = W(bx + 2);
                SW(l + 4, W(l + 4) & 0xfffb);
            }
            SW(bx + 4, W(bx + 4) | 0x40);
        }
        if ((dx & 0x80) && (!(dx & 0x4000) || (W(bx + 4) & 0x40))) {
            uint16_t es = W(SFX_BASE + 2), base = W(SFX_BASE);
            uint16_t t = (uint16_t)(base + 0x12 + (uint16_t)(cx << 2));
            SW(bx + 6, (uint16_t)(ES16(es, t) + base));
            SW(bx + 8, es);
            SW(bx + 4, 0x8002);
            SB(bx + 0x1e, 0);
            uint16_t l = W(bx + 2);
            SW(l + 4, W(l + 4) | 4);                /* take the channel from the music */
            sub_0712(W(bx));
        }
        ctl_common(bx, dx, &cx, &si, 0);
    }
    return si;
}

/* fn 0x16 (0xea1): direct note */
static void adl_fn16_note(uint16_t cx, uint16_t dx) {
    sub_07b4((uint8_t)(cx >> 8), (uint8_t)cx, dx, 0);
}

/* fn 0x18 (0xecd): program channel ch from the instrument at es:si, level with volume cl */
static void adl_fn18_instrument(uint8_t al, uint16_t cx, uint16_t es, uint16_t si) {
    uint16_t pair = W((uint16_t)((cx >> 8) * 2 + W(OPTAB)));
    if (pair == 0xffff) return;
    si++;
    if (!(al & 1)) {
        uint16_t p = sub_08b4((uint8_t)pair, es, si);
        if ((pair >> 8) != 0xff) sub_08b4((uint8_t)(pair >> 8), es, p);
    }
    sub_0774((uint8_t)pair, cx & 0xff, es, si);    /* reads es:[si+2] - original off-by-one */
}

/* fn 0x1a (0xea9): select melodic (cl != 0) or rhythm mode */
static void adl_fn1a_mode(uint8_t cl) {
    SW(OPTAB, OPTAB_MELODIC);
    uint8_t al = 0;
    SB(MELODIC, cl);
    if (!cl) { al |= 0x20; SW(OPTAB, OPTAB_RHYTHM); }
    SB(BD_SHADOW, al);
}

/* ------------------------------------------------------------------------ */
/* initialisation (the installer at 0xfa4)                                    */
/* ------------------------------------------------------------------------ */
static int adl_init(void) {
    if (g_ready) return g_ready > 0;
    size_t len = 0;
    uint8_t *img = cc_load_entry("AVE.CC1", 0, &len);
    if (!img || len < 0xf30 - 0x100 || len > 0x10000 - 0x100 ||
        memcmp(img + 0xf21 - 0x100, "IFGM ADLIB", 10) != 0) {
        fprintf(stderr, "adlib: cannot load the AdLib driver image (AVE.CC1 entry 0); no sound\n");
        free(img);
        g_ready = -1;
        return 0;
    }
    memset(M, 0, sizeof M);
    memcpy(M + 0x100, img, len);
    free(img);
    /* chip detection writes of the installer (0xf68); status polling not needed */
    sub_0739(0x01, 0x00);
    sub_0739(0x04, 0x60);
    sub_0739(0x04, 0x80);
    sub_0739(0x02, 0xff);
    sub_0739(0x04, 0x21);
    sub_0739(0x04, 0x60);
    sub_0739(0x04, 0x80);
    g_ready = 1;
    return 1;
}

/* ------------------------------------------------------------------------ */
/* public interface                                                          */
/* ------------------------------------------------------------------------ */
/* audio thread, lock held */
/* Songs play once, as in the original: the game never sets the driver's loop flag (0x20)
   and the song data has no loop opcode, so jingles end and themes fall silent at their end. */
void adlib_drv_tick(void) {
    if (g_ready > 0) adl_fn00_tick();
}

/* int F0: ah = function; es_ptr = native base of the DX segment (the offset is si) */
uint32_t adlib_call(uint16_t ax, uint16_t bx, uint16_t cx, uint16_t dx, uint16_t si, uint8_t *es_ptr) {
    (void)bx;
    uint8_t fn = (uint8_t)(ax >> 8);
    uint32_t ret = (uint32_t)dx << 16 | ax;
    if (fn == 0x00) return ret;                    /* ticks come from the audio thread */
    audio_lock();
    if (!adl_init()) { audio_unlock(); return ret; }
    switch (fn) {
    case 0x02: adl_fn02_reset_all(); break;
    case 0x04: adl_fn04_chip_reset(); break;
    case 0x06: set_region(SEG_SONG, es_ptr, 0x5208); adl_fn06_load_song(SEG_SONG, si); break;
    case 0x08: set_region(SEG_SFX, es_ptr, 0x2968);  adl_fn08_load_sfx(SEG_SFX, si); break;
    case 0x0a: ret = adl_fn0a_music_ctl(cx, dx, si); break;
    case 0x0c: ret = (uint32_t)dx << 16 | adl_fn0c_sfx_ctl(cx, dx, si); break;
    case 0x0e: break;                              /* uninstall: nothing to do */
    case 0x10: ret = 0x0001414du; break;           /* identify: AX='AM', DX=1 */
    case 0x12: ret = CALLBACK; break;              /* address of the opcode-6 callback slot */
    case 0x14: SW(OPL_PORT, cx); break;
    case 0x16: adl_fn16_note(cx, dx); break;
    case 0x18: set_region(SEG_INSTR, es_ptr, (uint32_t)si + 16); adl_fn18_instrument((uint8_t)ax, cx, SEG_INSTR, si); break;
    case 0x1a: adl_fn1a_mode((uint8_t)cx); break;
    default: break;                                /* odd / unknown: the original jumps into garbage */
    }
    audio_unlock();
    return ret;
}
