/*
 * Segment 07fe - save/load menu and savegame files, options panel,
 * resource-cache re-linking after a load, quit / file-error boxes.
 */
#include "../core/mem.h"
#include "../platform/platform.h"
#include "protos.h"

#define SAVE_DATA_LEN   0x1f3e   /* bytes of N.AVE before the 16-bit checksum */
#define SAVE_CRYPT_LEN  0x1f16   /* obfuscated part, from offset 0x28 */
#define SAVE_BLK3D_LEN  0x3e     /* DS 0x1fba..0x1ff7: ((0x1ff8-0x1fba)/2)*2 computed with LDIV */
#define SAVE_GAME_LEN   0x15ce   /* DS 0x9e..0x166b:   ((0x166c-0x9e)/2)*2 computed with LDIV  */

/* 07fe:000e - "quit game?" box on screen; Y/O/J/S exits to DOS */
void sub_07fe_000e(void)
{
    uint8_t *saved = DSPTR(0x2d38);

    DSPTR_SET(0x2d38, DSPTR(0x2d3c));
    sub_02a7_001d(0x64, 0x5a, 0xdc, 0x6e, 0x9c);
    sub_0596_02a0(0);
    sub_1172_0012(0x67, 0x61, DSPTR(0x2d3c), sub_03dd_165d(0x36, DSPTR(0x3290)));
    DSPTR_SET(0x2d38, saved);

    while (DS8(0x2d4e) != 0)
        plat_yield();
    while (DS8(0x2d4e) == 0)
        plat_yield();
    if (DS8(0x2d4e) == 0x15 || DS8(0x2d4e) == 0x18 ||
        DS8(0x2d4e) == 0x24 || DS8(0x2d4e) == 0x1f)
        sub_0791_0665();
    while (DS8(0x2d4e) != 0)
        plat_yield();
}

/* 07fe:00b2 - fopen error handler: shows msg #0x37 and the file name, waits a key (Esc -> quit box); returns 1 = retry */
int16_t sub_07fe_00b2(uint8_t *filename)
{
    uint8_t *saved = DSPTR(0x2d38);

    DSPTR_SET(0x2d38, DSPTR(0x2d3c));
    sub_07fe_06de(0x6e, 0x55, 0xd2, 0x73, 0x9c);
    sub_0596_02a0(0);
    sub_1172_0012(0x78, 0x5a, DSPTR(0x2d3c), sub_03dd_165d(0x37, DSPTR(0x3290)));
    sub_1172_0012(0x86, 0x68, DSPTR(0x2d3c), filename);
    DSPTR_SET(0x2d38, saved);

    DS8(0x2d4e) = 0;
    while (DS8(0x2d4e) == 0)
        plat_yield();
    if (DS8(0x2d4e) == 1)
        sub_07fe_000e();
    while (DS8(0x2d4e) != 0)
        plat_yield();
    return 1;
}

/* 07fe:015c - after a load: rebases script threads DS 0x7b2 (+0 start, +4 ip) into the new script DSPTR(0x177e) */
void sub_07fe_015c(void)
{
    uint8_t *e = DSADDR(0x7b2);
    uint16_t i;

    for (i = 0; i < DSU16(0xd8); i++) {
        uint16_t off = PU16(DSPTR(0x177e) + i * 2);
        uint8_t *start = DSPTR(0x177e) + off;
        /* only the offset words of the saved pointers are used (they may be DOS seg:off values) */
        FP_SET(e + 4, start + (uint16_t)(PU16(e + 4) - PU16(e + 0)));
        FP_SET(e + 0, start);
        e += 0x16;
    }
}

/* 07fe:01d5 - replaces actor sprite pointers (+0x16) equal to oldptr by newptr, once per actor */
void sub_07fe_01d5(uint8_t *oldptr, uint8_t *newptr)
{
    uint8_t *a = DSADDR(0x11a);
    int16_t i;

    for (i = 0; i < 10; i++) {
        /* raw 32-bit compare of the stored pointer (token or DOS seg:off, never dereferenced) */
        if (PU32(a + 0x16) == fp_token(oldptr) && DS8(0x32b4 + i) == 0) {
            FP_SET(a + 0x16, newptr);
            DS8(0x32b4 + i) = 1;
        }
        a += 0x46;
    }
}

/* 07fe:0231 - reloads every resource of the cache table DS 0x928 and re-links actor pointers */
void sub_07fe_0231(void)
{
    uint8_t *e = DSADDR(0x928);
    uint8_t *old;
    int16_t i;

    sub_1209_000d(DSADDR(0x32b4), 10, 0);
    for (i = 0; i < 20; i++) {
        old = FP(e + 2);                 /* may be a stale/DOS pointer: only compared */
        if (P8(e) == 0 && PU32(e + 2) != 0)
            FP_SET(e + 2, sub_0053_0413(DSPTR(0x18a0), PS8(e + 1)));
        else
            FP_SET(e + 2, sub_0053_03a5(PS8(e)));
        sub_07fe_01d5(old, FP(e + 2));
        e += 6;
    }
}

/* 07fe:02df - out of memory: frees all cached PAK resources, then reloads them (sub_07fe_0231) */
void sub_07fe_02df(void)
{
    uint8_t *e = DSADDR(0x928);
    int16_t i;

    for (i = 0; i < 20; i++) {
        /* TODO(port): if this runs while 0231 is re-linking a freshly loaded save, later entries still
         * hold saved (stale / DOS) pointers and get farfree'd - same bug as the original. */
        if (P8(e) == 0 && PU32(e + 2) != 0)
            sub_0f27_0061(FP(e + 2));
        e += 6;
    }
    sub_07fe_0231();
}

/* 07fe:032a - savegame (de)obfuscation: buf[i] ^= 5*(i+1) */
void sub_07fe_032a(uint8_t *buf, int16_t len)
{
    uint16_t key = 5;
    int16_t i;

    for (i = 0; i < len; i++) {
        *buf ^= (uint8_t)key;
        key += 5;
        buf++;
    }
}

/* 07fe:035d - copies the loaded game-state block savebuf+0x3e6 into DS 0x9e and restarts the saved music */
void sub_07fe_035d(void)
{
    uint16_t len = SAVE_GAME_LEN;

    sub_120e_0002(DSPTR(0x3294) + 0x3e6, DSADDR(0x9e), len);
    sub_0791_0078(DS16(0xd6));
}

/* 07fe:03ab - loads slot N.AVE; returns 1 if the checksum matched (0 if the file could not be opened) */
int16_t sub_07fe_03ab(int16_t slot)
{
    int16_t ok = 0;
    uint16_t sum = 0;
    uint8_t chk[2] = { 0, 0 };
    int32_t h;
    uint8_t *buf;
    int16_t i, cur_room, saved_room;
    uint16_t len;

    DS16(0x96) = 0;
    DS8(0x1ae4) = (uint8_t)((uint8_t)slot + '0');
    h = sub_0e66_0031(DSADDR(0x1ae4), 0x3ed);
    if (h == 0)
        return ok;

    sub_03dd_011f();
    sub_0053_0112();
    buf = DSPTR(0x3294);
    sub_0e66_0191(h, buf, SAVE_DATA_LEN);
    sub_0e66_0191(h, chk, 2);
    sub_0e66_022b(h);
    for (i = 0; i < SAVE_DATA_LEN; i++)
        sum += buf[i];
    sub_07fe_032a(buf + 0x28, SAVE_CRYPT_LEN);
    cur_room = DS16(0xa0);
    saved_room = P16(DSPTR(0x3294) + 0x3e8);   /* DS 0xa0 inside the saved block */

    if (sum == PU16(chk)) {
        len = SAVE_BLK3D_LEN;
        sub_120e_0002(buf + 0x28, DSADDR(0x1fba), len);
        ok = 1;
        if (cur_room == -1 && saved_room == -1) {
            /* 3D world -> 3D world: apply now */
            sub_0a86_078e();
            sub_0596_17cd();
            sub_0a86_13e2();
            sub_07fe_035d();
            sub_0a86_1341();
            sub_07fe_0231();
        } else if (cur_room == -1) {
            /* 3D world -> room: let the main loop enter the saved room */
            DS16(0xa2) = DS16(0x1fe2);
            DS16(0x96) = 1;
        } else if (saved_room == -1) {
            /* room -> 3D world */
            DS16(0x1fe4) = 1;
            DS16(0xa2) = -1;
            DS16(0x96) = 1;
        } else {
            /* room -> room: reload level now */
            sub_03dd_011f();
            sub_0053_0112();
            sub_0053_097e();
            sub_07fe_035d();
            sub_0053_09a7(saved_room);
            DS16(0x96) = 1;
            sub_03dd_01c8();
            sub_0596_169f();
            sub_07fe_0231();
            DS16(0x96) = 0;
        }
    } else {
        /* bad checksum: error box forever (sub_07fe_00b2 always returns 1); only the quit box exits */
        while (sub_07fe_00b2(DSADDR(0x1ae4)) != 0)
            ;
    }
    return ok;
}

/* 07fe:057e - writes slot N.AVE (name, 3D block, game block, obfuscated, checksum); returns 1 on success */
int16_t sub_07fe_057e(int16_t slot)
{
    int16_t ok = 0;
    uint16_t sum = 0;
    uint8_t chk[2];
    int32_t h;
    uint8_t *buf;
    int16_t i;
    uint16_t len3d, lengame;

    DS8(0x1ae4) = (uint8_t)((uint8_t)slot + '0');
    h = sub_0e66_0031(DSADDR(0x1ae4), 0x3ee);
    if (h != 0) {
        sub_0053_028e();
        buf = DSPTR(0x3294);
        len3d = SAVE_BLK3D_LEN;
        lengame = SAVE_GAME_LEN;
        sub_120e_0002(DSPTR(0x32be) + slot * 0x28, buf, 0x28);
        sub_120e_0002(DSADDR(0x1fba), buf + 0x28, len3d);
        sub_120e_0002(DSADDR(0x9e), buf + 0x3e6, lengame);
        sub_07fe_032a(buf + 0x28, SAVE_CRYPT_LEN);
        sub_0e66_0114(h, buf, SAVE_DATA_LEN);
        for (i = 0; i < SAVE_DATA_LEN; i++)
            sum += buf[i];
        PU16(chk) = sum;
        sub_0e66_0114(h, chk, 2);
        sub_0e66_022b(h);
        ok = 1;
    }
    DS16(0x98) = 0;
    return ok;
}

/* 07fe:06de - bevelled box (x1,y1)-(x2,y2) filled with color */
void sub_07fe_06de(int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t color)
{
    sub_1040_0006(x1, y1, x2, y1, 0x97);
    sub_1040_0006(x2, y1, x2, y2, 0x97);
    sub_1040_0006(x1, y1, x1, y2, 0x97);
    sub_1040_0006(x1 + 1, y1 + 1, x2 - 2, y1 + 1, 0x9f);
    sub_1040_0006(x2 - 1, y1 + 1, x2 - 1, y2 - 1, 0x9a);
    sub_1040_0006(x1 + 1, y1 + 2, x1 + 1, y2 - 1, 0x9e);
    sub_1040_0006(x1 + 1, y2, x2 - 1, y2, 0x99);
    sub_10f5_0028(x1 + 2, y1 + 2, x2 - 2, y2 - 1, (uint8_t)color);
}

/* 07fe:0847 - reads the 40-byte names of 0.AVE..9.AVE into DSPTR(0x32be); missing slot -> 0xff */
void sub_07fe_0847(void)
{
    uint8_t *p = DSPTR(0x32be);
    int32_t h;
    int16_t i;

    DSPTR_SET(0x2d40, NULL);             /* no "insert disk" box while probing */
    for (i = 0; i < 10; i++) {
        DS8(0x1ae4) = (uint8_t)((uint8_t)i + '0');
        h = sub_0e66_0031(DSADDR(0x1ae4), 0x3ed);
        if (h != 0) {
            sub_0e66_0191(h, p, 0x28);
            sub_0e66_022b(h);
        } else {
            p[0] = 0xff;
        }
        p += 0x28;
    }
    DSPTR_SET(0x2d40, (uint8_t *)(void *)sub_07fe_00b2);
}

/* 07fe:08d2 - draws save-slot row `slot` (number, name, text cursor when editing the selected slot) */
void sub_07fe_08d2(int16_t slot, int16_t editing)
{
    uint8_t *name = DSPTR(0x32be) + slot * 0x28;
    int16_t y = slot * 14 + 0x12;
    int16_t color = 0x9c;
    int16_t x;

    if (name[0] != 0xff)
        color = 0x7d;
    sub_07fe_06de(0x1e, y, 0x31, y + 14, color);
    color = 0x9c;
    if (slot == DS16(0x1af2))
        color = 0x98;
    sub_07fe_06de(0x32, y, 0x122, y + 14, color);
    sub_1172_0012(0x25, y + 4, DSPTR(0x2d38), sub_0f3a_0004(slot));
    if (name[0] != 0xff) {
        sub_1172_0012(0x38, y + 5, DSPTR(0x2d38), name);
        if (editing != 0 && slot == DS16(0x1af2)) {
            x = sub_1172_018c(name) + 0x38;
            y = slot * 14 + 0x17;
            sub_10f5_0028(x, y, x + 6, y + 6, 0x94);
        }
    }
}

/* 07fe:09dc - draws the slot rows mode..9 (slot 0 hidden in save mode) */
void sub_07fe_09dc(int16_t mode)
{
    int16_t i;

    sub_0596_02a0(0);
    for (i = mode; i < 10; i++)
        sub_07fe_08d2(i, mode);
}

/* 07fe:0a0b - edits the selected slot name; returns 2 on Enter, 1 on Esc / up / down */
int16_t sub_07fe_0a0b(int16_t mode)
{
    int16_t state = 0;
    int16_t pos = 0;
    uint8_t backup[0x28];
    uint8_t *name = DSPTR(0x32be) + DS16(0x1af2) * 0x28;
    int8_t i;

    sub_120e_0002(name, backup, 0x28);
    name[0] = 0;
    sub_07fe_08d2(DS16(0x1af2), mode);
    sub_118f_0107();

    while (state == 0) {
        plat_yield();
        if (DS8(0x2d4e) == 1) {
            state = 1;                   /* Esc keeps the (cleared/partial) name */
        } else if (DS8(0x2d4e) == 0x1c) {
            state = 2;
        } else if (DS8(0x2d4a) == 2 || DS8(0x2d4a) == 1) {
            sub_120e_0002(backup, name, 0x28);
            sub_07fe_08d2(DS16(0x1af2), mode);
            sub_118f_0107();
            state = 1;
        } else if (DS8(0x2d4e) == 0x0e) {  /* backspace */
            if (pos != 0)
                pos--;
            name[pos] = 0;
            sub_07fe_08d2(DS16(0x1af2), mode);
            sub_118f_0107();
            while (DS8(0x2d4e) != 0)
                plat_yield();
        } else if (DS8(0x2d4e) == 0 && DS8(0x2d4a) == 0) {
            /* nothing pressed */
        } else if (pos < 0x26 && sub_1172_018c(name) < 0xdc) {
            for (i = 0; i < 0x25; i++) {
                if (DS8(0x1afe + i) != DS8(0x2d4e) && (DS8(0x2d4a) & 0x80) == 0)
                    continue;
                if (DS8(0x2d4a) & 0x80)
                    name[pos] = ' ';
                else if (i < 0x1a)
                    name[pos] = (uint8_t)(i + 'A');
                else
                    name[pos] = (uint8_t)(i + 0x16);   /* '0'..'9', ':' */
                pos++;
                name[pos] = 0;
                sub_07fe_08d2(DS16(0x1af2), mode);
                sub_02a7_0b4a(0x32, DS16(0x1af2) * 14 + 0x12, 0x122, DS16(0x1af2) * 14 + 0x1e);
                while (DS8(0x2d4e) != 0 || DS8(0x2d4a) != 0)
                    plat_yield();
                break;
            }
        }
    }
    return state;
}

/* 07fe:0bd5 - save/load slot menu (mode 0 load, 1 save) */
void sub_07fe_0bd5(int16_t mode)
{
    uint8_t names[0x190];                /* 10 x 40-byte slot names, published in DSPTR(0x32be) */
    int16_t done = 0;
    int16_t confirm = 0;
    uint8_t key;

    DSPTR_SET(0x32be, names);
    sub_07fe_0847();
    sub_0596_02ec();
    sub_07fe_09dc(mode);
    sub_0dc5_01c5();
    while (DS8(0x2d4e) != 0)
        plat_yield();

    while (done == 0) {
        plat_yield();
        key = DS8(0x2d4e);
        if (DS8(0x2d4a) == 1) {          /* up */
            DS16(0x1af2)--;
            if ((mode != 0 && DS16(0x1af2) == 0) || DS16(0x1af2) == -1)
                DS16(0x1af2) = 9;
            sub_07fe_09dc(mode);
            sub_02a7_0b4a(0, 0, 0x13f, 0xab);
            while (DS8(0x2d4a) != 0)
                plat_yield();
        }
        if (DS8(0x2d4a) == 2) {          /* down */
            DS16(0x1af2)++;
            if (DS16(0x1af2) == 10)
                DS16(0x1af2) = 0;
            if (mode != 0 && DS16(0x1af2) == 0)
                DS16(0x1af2) = 1;
            sub_07fe_09dc(mode);
            sub_02a7_0b4a(0, 0, 0x13f, 0xab);
            while (DS8(0x2d4a) != 0)
                plat_yield();
        }
        if (key == 1 && DS16(0x144) > 0) {  /* Esc only while the hero is alive */
            done = 1;
            DS16(0x96) = 0;
            DS16(0x98) = 0;
        } else if (key == 0x1c) {
            confirm = 1;
        } else if (key != 0 && mode != 0) {
            if (sub_07fe_0a0b(mode) == 2)
                confirm = 1;
        }
        if (confirm != 0) {
            confirm = 0;
            if (mode == 0 && sub_07fe_03ab(DS16(0x1af2)) != 0)
                done = 1;
            if (mode == 1 && sub_07fe_057e(DS16(0x1af2)) != 0)
                done = 1;
        }
    }
    while (DS8(0x2d4e) != 0)
        plat_yield();
    sub_0596_02d6();
    sub_07fe_0f88();
    /* note: DSPTR(0x32be) is left pointing to the dead local array, as in the original */
}

/* 07fe:0d4c - one 5-pixel volume gauge tick at column x (bright if filled) */
void sub_07fe_0d4c(int16_t x, int16_t filled)
{
    int16_t color = 0x92;

    if (filled != 0)
        color = 0x33;
    sub_1100_00c0((uint8_t)color);
    sub_1100_0096(x, 0xbc);
    color++;
    sub_1100_00c0((uint8_t)color);
    sub_1100_0096(x, 0xbd);
    color++;
    sub_1100_00c0((uint8_t)color);
    sub_1100_0096(x, 0xbe);
    color--;
    sub_1100_00c0((uint8_t)color);
    sub_1100_0096(x, 0xbf);
    color--;
    sub_1100_00c0((uint8_t)color);
    sub_1100_0096(x, 0xc0);
}

/* 07fe:0de3 - redraws the options panel (items, labels, volume gauges, window-size marker) */
void sub_07fe_0de3(void)
{
    int16_t music = DS16(0xe8) / 8;
    int16_t sound = DS16(0xea) / 8;
    uint8_t *r;
    int16_t i, color;

    sub_106a_0477(2, 0, 0xc7, DSPTR(0x32b0));
    r = DSADDR(0x1b38 + DS16(0x1b8c) * 8);
    sub_10f5_0028(P16(r), P16(r + 2), P16(r + 4), P16(r + 6), 0x5f);
    for (i = 0; i < 5; i++) {
        color = 0x9f;
        if (i == DS16(0x1b8c))
            color = 0x70;
        sub_0596_02a0(color);
        sub_1172_0012(DS16(0x1b60 + i * 4), DS16(0x1b60 + i * 4 + 2), DSPTR(0x2d38), DSPTR(0x1b24 + i * 4));
    }
    for (i = 0; i < 16; i++)
        sub_07fe_0d4c(i * 3 + 0x6d, i < music);
    for (i = 0; i < 16; i++)
        sub_07fe_0d4c(i * 3 + 0xbd, i < sound);
    sub_106a_0477(3, music * 3 + 0x6d, 0xc2, DSPTR(0x32b0));
    sub_106a_0477(3, sound * 3 + 0xbd, 0xc2, DSPTR(0x32b0));
    r = DSADDR(0x1b74 + DS16(0xe6) * 8);
    sub_116d_000c(P16(r), P16(r + 2), P16(r + 4), P16(r + 6), 0x77);
    sub_02a7_1264();
}

/* 07fe:0f88 - 3D mode with a reduced view window: clears the view area on screen (sky + ground) */
void sub_07fe_0f88(void)
{
    uint8_t *saved;

    if (DS16(0xa0) == -1 && DS16(0xe6) != 2) {
        saved = DSPTR(0x2d38);
        DSPTR_SET(0x2d38, DSPTR(0x2d3c));
        sub_0c9b_0a66(0x64);
        sub_10f5_0028(0, 0x64, 0x13f, 0xab, 0x55);
        DSPTR_SET(0x2d38, saved);
    }
}

/* 07fe:0fdc - after a window-size change in 3D mode: clear the view area and re-render */
void sub_07fe_0fdc(void)
{
    if (DS16(0xa0) == -1) {
        sub_07fe_0f88();
        sub_0a86_0d96(0);
    }
}

/* 07fe:0ff1 - options panel: Load / Save / music volume / sound volume / 3D window size */
void sub_07fe_0ff1(void)
{
    int16_t done = 0;
    int16_t pending = 0;                 /* key pressed during the slide-in */
    int16_t i, music, sound, old_music, old_sound, old_window, changed, activate;
    uint8_t *tmp;
    uint8_t dir, key;

    sub_0596_02ec();
    DSPTR_SET(0x32b0, sub_0053_0413(DSADDR(0x1bb7), 0x37));
    tmp = sub_0f27_0004(0xbb8);
    sub_0a21_0005(0, 0, 0, DSPTR(0x32b0), 1, 0, 0, DSPTR(0x32b0), tmp);
    for (i = 0; i < 0x20; i++) {        /* slide the panel in */
        if (DS8(0x2d4e) != 0)
            pending = DS8(0x2d4e);
        sub_106a_06a1(0, 0, 0xc7, tmp, (uint16_t)(i << 3));
        sub_02a7_1264();
    }
    sub_07fe_0de3();
    music = DS16(0xe8) / 8;
    sound = DS16(0xea) / 8;

    while (done == 0) {
        plat_yield();
        old_music = music;
        old_sound = sound;
        old_window = DS16(0xe6);
        dir = DS8(0x2d4a);
        if (pending != 0) {
            key = (uint8_t)pending;
            pending = 0;
        } else {
            key = DS8(0x2d4e);
        }

        changed = 0;
        if (dir & 4) {                   /* left: previous item */
            DS16(0x1b8c)--;
            if (DS16(0x1b8c) < 0)
                DS16(0x1b8c) = 4;
            changed = 1;
        }
        if (dir & 8) {                   /* right: next item */
            DS16(0x1b8c)++;
            if (DS16(0x1b8c) == 5)
                DS16(0x1b8c) = 0;
            changed = 1;
        }
        if (dir & 2) {                   /* down: decrease */
            if (DS16(0x1b8c) == 0)
                DS16(0x1b8c) = 1;
            if (DS16(0x1b8c) == 2 && music != 0)
                music--;
            if (DS16(0x1b8c) == 3 && sound != 0)
                sound--;
            if (DS16(0x1b8c) == 4 && DS16(0xe6) != 0)
                DS16(0xe6)--;
            changed = 1;
        }
        if (dir & 1) {                   /* up: increase */
            if (DS16(0x1b8c) == 1)
                DS16(0x1b8c) = 0;
            if (DS16(0x1b8c) == 2 && music < 15)
                music++;
            if (DS16(0x1b8c) == 3 && sound < 15)
                sound++;
            if (DS16(0x1b8c) == 4 && DS16(0xe6) < 2)
                DS16(0xe6)++;
            changed = 1;
        }
        if (old_music != music) {
            DS16(0xe8) = music << 3;
            sub_0e9f_0013(DS16(0xe8), 0, 0x2000);
        }
        if (old_sound != sound) {
            DS16(0xea) = sound << 3;
            sub_0e9f_006f(DS16(0xea), 0, 0x2000);
        }
        if (changed != 0) {
            sub_07fe_0de3();
            if (DS16(0xe6) != old_window)
                sub_07fe_0fdc();
            while (DS8(0x2d4a) != 0)
                plat_yield();
        }

        if (key == 1)
            done = 1;
        activate = 0;
        if (key == 0x1c)
            activate = 1;
        if (key == 0x26) {               /* 'L' */
            DS16(0x1b8c) = 0;
            activate = 1;
        }
        if (key == 0x1f) {               /* 'S' */
            DS16(0x1b8c) = 1;
            activate = 1;
        }
        if (activate != 0) {
            done = 1;
            sub_07fe_0de3();
            if (DS16(0x1b8c) == 0) {
                sub_0f27_0061(tmp);
                sub_0f27_0061(DSPTR(0x32b0));
                DSPTR_SET(0x32b0, NULL);
                tmp = NULL;
                sub_07fe_0bd5(0);
                done = 1;
                sub_0596_0eee();
                sub_0596_0e96();
                sub_0dc5_01c5();
            } else if (DS16(0x1b8c) == 1) {
                sub_07fe_0bd5(1);
            }
        }
    }

    sub_0596_02ec();
    if (tmp != NULL) {                   /* slide the panel out */
        sub_0a21_0005(0, 0, 0, DSPTR(0x32b0), 1, 0, 0, DSPTR(0x32b0), tmp);
        for (i = 0; i < 0x20; i++) {
            sub_0596_0e96();
            sub_106a_06a1(0, 0, 0xc7, tmp, (uint16_t)(0xf8 - (i << 3)));
            sub_02a7_1264();
        }
        sub_0f27_0061(tmp);
        sub_0f27_0061(DSPTR(0x32b0));    /* pointer not cleared, as in the original */
    }
    sub_0596_0ec3();
    sub_07fe_0f88();
    while (DS8(0x2d4e) != 0)
        plat_yield();
    sub_0596_02d6();
}
