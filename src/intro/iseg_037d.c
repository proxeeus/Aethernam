/*
 * Intro segment 037d: keyboard (int 9 handler) and keyboard-driven cursor.
 * The int 9 handler becomes a C function registered with plat_set_kbd_isr(); the platform
 * delivers raw PC (set 1) scancodes to it from plat_yield().
 *
 * DGROUP: 0x15a / 0x15c direction key bits, 0x15e current key (make scancode, 0 on release),
 * 0x162 chain to the old handler, 0x164 button bits, 0x166 cursor speed shift,
 * 0x168 cursor clamp enable, 0x16c / 0x16e cursor x / y, 0x170 shift-state bits,
 * 0x178 AZERTY translation flag (launcher byte 9).
 * The tables below are the code-segment data 037d:000e (key flags, 128 words),
 * 037d:0112 / 0123 (direction -> dx / dy) and 037d:0144 (AZERTY scancode translation).
 * Key flag word: bits 13-15 = handler (table 037d:01c4), bit 12 = not a "current key".
 */
#include "../core/mem.h"
#include "../platform/platform.h"
#include "intro_protos.h"

static const uint16_t s_key_flags[128] = {
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x3004, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x5001, 0x3002, 0x5004, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x3002, 0x0000,
    0x3001, 0x5090, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x5005,
    0x5001, 0x5009, 0x5002, 0x5004, 0x0000, 0x5008, 0x5008, 0x5006,
    0x5002, 0x500a, 0x8001, 0x8002, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x5090,
    0x0000, 0x5008, 0x5004, 0x5002, 0x5001, 0x8002, 0x8001, 0x0000,
};

static const int8_t s_dir_dx[16] = {
    0, 0, 0, 0, -1, -1, -1, 0, 1, 1, 1, 0, 0, 0, 0, 0,
};

static const int8_t s_dir_dy[16] = {
    0, -1, 1, 0, 0, -1, 1, 0, 0, -1, 1, 0, 0, 0, 0, 0,
};

static const uint8_t s_azerty[128] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0x1e, 0x2c, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x10, 0x1f,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x32, 0x28, 0x29, 0x2a, 0x2b, 0x11, 0x2d, 0x2e, 0x2f,
    0x30, 0x31, 0x27, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
};

/* 037d:01d4  int 9 handler: translates the scancode (AZERTY if DS16(0x178)), runs the
 * key's handler (shift state, direction, buttons) and stores the key in DS8(0x15e)
 * (make code) or clears it (break code) */
void intro_037d_01d4(uint8_t scancode)
{
    uint8_t al = scancode, ah = scancode;
    uint16_t flags;
    if (DS16(0x178) != 0) {
        ah &= 0x80;
        al = (uint8_t)(s_azerty[al & 0x7f] | ah);
    }
    flags = s_key_flags[al & 0x7f];
    switch ((flags >> 13) & 7) {                /* call cs:[di+0x1c4] */
    case 1:  intro_037d_0273(flags, (uint16_t)(ah << 8 | al)); break;
    case 2:  intro_037d_029c(flags, (uint16_t)(ah << 8 | al)); break;
    case 3:  intro_037d_02a3(flags, (uint16_t)(ah << 8 | al)); break;
    case 4:  intro_037d_0288(flags, (uint16_t)(ah << 8 | al)); break;
    default: intro_037d_0272(flags, (uint16_t)(ah << 8 | al)); break;
    }
    if (!(flags & 0x1000)) {
        if (!(ah & 0x80)) {
            DS8(0x15e) = al;
            if (al == 0x53 && (DS16(0x170) & 5) == 5) {
                /* Ctrl+Alt+Del: the original reboots (jmp ffff:0000).
                 * TODO(port): ignored. */
            }
        } else {
            DS8(0x15e) = 0;
        }
    }
    /* DS8(0x162) != 0 would chain to the BIOS handler; otherwise EOI.  Nothing to do. */
}

/* 037d:0272  key handler 0/5/6/7: nothing */
void intro_037d_0272(uint16_t flags, uint16_t ax)
{
    (void)flags; (void)ax;
}

/* 037d:0273  key handler 1 (shift keys): sets / clears flags&0xfff in DS16(0x170) */
void intro_037d_0273(uint16_t flags, uint16_t ax)
{
    uint16_t bx = flags & 0xfff;
    if (!(ax & 0x8000)) DS16(0x170) |= bx;
    else                DS16(0x170) &= (uint16_t)~bx;
}

/* 037d:0288  key handler 4 (fire buttons): sets / clears flags&3 in DS16(0x164) */
void intro_037d_0288(uint16_t flags, uint16_t ax)
{
    uint16_t bx = flags & 3;
    if (!(ax & 0x8000)) DS16(0x164) |= bx;
    else                DS16(0x164) &= (uint16_t)~bx;
}

/* 037d:02a7 (shared tail of 029c/02a3): direction bits (flags & 0x8f) into the word at
 * `var`, button bits ((flags >> 4) & 3) into DS16(0x164) */
static void dir_key(uint16_t var, uint16_t flags, uint16_t ax)
{
    uint16_t bx = flags & 0xfff;
    uint16_t cx = (uint16_t)((uint8_t)bx >> 4) & 3;
    bx &= 0x8f;
    if (!(ax & 0x8000)) {
        DS16(var) |= bx;
        DS16(0x164) |= cx;
    } else {
        DS16(var) &= (uint16_t)~bx;
        DS16(0x164) &= (uint16_t)~cx;
    }
}

/* 037d:029c  key handler 2 (cursor keys, set 1): direction bits in DS16(0x15a) */
void intro_037d_029c(uint16_t flags, uint16_t ax)
{
    dir_key(0x15a, flags, ax);
}

/* 037d:02a3  key handler 3 (cursor keys, set 2): direction bits in DS16(0x15c) */
void intro_037d_02a3(uint16_t flags, uint16_t ax)
{
    dir_key(0x15c, flags, ax);
}

/* 037d:0303  installs the keyboard: timer callback 037d:0374 (cursor movement) and the
 * int 9 handler 037d:01d4 (old vector saved at cs:0x10e) */
void intro_037d_0303(void)
{
    intro_03c1_014a(intro_037d_0374);
    plat_set_kbd_isr(intro_037d_01d4);
}

/* 037d:0334  keyboard/mouse init: if a mouse driver is installed (int 33h vector not NULL
 * and not an iret) resets it and registers the mouse poll 037d:03e7 on the timer; then
 * installs the keyboard (0303) */
void intro_037d_0334(void)
{
    /* port: no mouse driver (the int 33h branch and the poll routine 037d:03e7 are omitted) */
    intro_037d_0303();
}

/* 037d:0365  restores the old int 9 handler */
void intro_037d_0365(void)
{
    plat_set_kbd_isr(NULL);
}

/* 037d:0374  timer callback: moves the cursor with the direction keys of DS8(0x15a) */
void intro_037d_0374(void)
{
    intro_037d_0381(DS8(0x15a));
}

/* 037d:0381  adds the direction (dirs & 0xf) << DS8(0x166) to the cursor position
 * DS16(0x16c) / DS16(0x16e) and clamps it */
void intro_037d_0381(uint8_t dirs)
{
    uint8_t cl = DS8(0x166);
    dirs &= 0xf;
    DS16(0x16c) += (int16_t)((uint16_t)(int16_t)s_dir_dx[dirs] << cl);
    DS16(0x16e) += (int16_t)((uint16_t)(int16_t)s_dir_dy[dirs] << cl);
    intro_037d_03ac();
}

/* 037d:03ac  if DS16(0x168): clamps the cursor to 0..319 x 0..199 */
void intro_037d_03ac(void)
{
    int16_t cx, ax;
    if (DS16(0x168) == 0) return;
    cx = DS16(0x16c);
    ax = DS16(0x16e);
    if (ax < 0) DS16(0x16e) = 0;
    if (ax > 0xc7) DS16(0x16e) = 0xc7;
    if (cx < 0) DS16(0x16c) = 0;
    if (cx > 0x13f) DS16(0x16c) = 0x13f;
}
