# Intro program (AVE.CC1 entry 2) – `src/intro/`

**Layout.** MZ image (header 0x400), CS:IP 0000:0000, DGROUP 0x06ed (image bytes 0x6ed0..0x80ec, BSS 0x8d6..0xc86),
SS 0x07b6 = DGROUP+0xc9 paragraphs → `INTRO_SS_BASE 0xc90` (ss:[y*2] row table, ss:0x190 tick count, ss:0x192..0x198
clip ymin/ymax/xmin/xmax; macros `ISS16/ISSU16`). One file per segment `iseg_SSSS.c`, prototypes in `intro_protos.h`.
`intro_run()` (iseg_003a.c) zeroes the BSS, calls main `intro_003a_0ef9` and frees what the intro left allocated.

**main (0ef9):** launcher cfg (+7 sound type: 0 → "mus"→"mub", +8 language), video/timer init (0690_000b), keyboard
(037d_0334), sound (0362_00b3), then 4 scenes, then uninstall everything and return.
1. `0631` load: MUS.PAK #7 (song) + #1 (SFX bank), music start (fn 0xa, 0xa0); INTRO #4 SLIDE, #5 TATOU; credits =
   block 14 of the language file DSPTR(0x94+4*lang) (`t/e/s/d/i.cc4`, plain text, dword offset table); work buffers.
2. `059e` TATOU logo (vector shape 0), palette fade in/out (palette = SLIDE+P32(+0x10)).
3. `08e0` SLIDE: 4x4 tile grid, "lens" zoom (0826 + line reducer 0131_01bd), dissolve (0131_0022), 300 random tile
   updates (full / half size: 05ae_01ba shrinks a sprite). 4. `031c` SPLASH (INTRO #3) + 7 credit pages (01f4).
5. `0c13` MUS #6, EARTH (INTRO #0) + font (INTRO #1), typewriter "PLANET ETERNAM"/"DATE 2815", scroll, zoom windows.
Keys (DS8(0x15e) = current make scancode, 0 when released): Esc (1) aborts waits and whole scenes, Enter (0x1c)
skips the current wait/page (several loops wait for Enter release), some waits stop on any key.

**Graphics.** Back buffer DSPTR(0x148) (flip 066d_0111 = retrace + copy to DSPTR(0x14c)=g_vram). VA2 files: header
dwords (+0 shape dir, +4 sprite bank, +0x10 palette). Shapes (050c_0475): primitives sub-shape/sprite/skip/polygon
(0610, Sutherland-Hodgman + DDA, uses DGROUP 0x394..0x854 as work area)/rect/polyline/points; mode bits 0x10 byte
coords, 0x80/0x20 negate x/y (the original self-patches its vertex loop). RLE sprites: see iseg_0488.c header
(mirroring rewrites the rows in place). Self-modifying immediates / cs: variables became `static`s.

**Hardware → platform.** int 9 handler = `intro_037d_01d4(scancode)` via `plat_set_kbd_isr` (AZERTY table, shift /
direction / button bits, key-flag tables copied from the code segment). int 8 handler = `intro_03c1_00d0` via
`plat_set_timer_isr` + `plat_set_timer_rate(DS16(0x138)=0x4dae)` (60 Hz); it runs up to 6 callbacks: sound tick
(0362_0108) and keyboard cursor (037d_0374). The tick count ss:0x190 is never read, so no `intro_ticks()` is needed,
and keys come from the original handler, so **`intro_key_pressed()` is not used**. Retrace waits → `plat_wait_vsync()`
(also sets Esc when `plat_quit_requested()`), DAC → `plat_set_palette` (values &0x3f like the DAC), busy-waits on
DS8(0x15e) call `plat_yield()`. Mouse (int 33h) branch omitted (no driver). Ctrl+Alt+Del reboot ignored.

**Sound (`snd_driver_call(ax,bx,cx,dx,si,es_ptr)`, AH = function, AL/BX = 0):** fn 0 tick (60 Hz from the timer
ISR: make it a no-op if your driver ticks on the audio thread), fn 2 reset, fn 0x10 identify (init), fn 4 chip reset
(after fn 6, at exit), fn 6 load song (es_ptr = song, dx=si=0), fn 8 load SFX bank (es_ptr = bank), fn 0xa music
control (CX param, SI voice mask 0=all, DX cmd: 0xa0 start+loop, 0x8000 fade, 0x40 stop, 0x80 start), fn 0xc SFX
(CX track byte, SI = single voice bit, DX 0x80) for the typewriter click (effect 0x38 of the bank's table at
bank+w[bank+0x0e]). The driver is assumed present.

**Files/memory.** `data_fopen` (032c wrappers, FILE* = DOS handle), PAK loader 03f8 faithful on stdio with
`explode()` (042e_0029/02ab are the decompressor internals, not translated). DOS blocks → `arena_alloc/arena_free`,
setblock = no-op (blocks are allocated at their final size), all blocks tracked and freed by `intro_run`.
Port deviations (marked TODO(port)/comments): back buffer and shrink buffer are 64K (16-bit offset wrap stays inside),
blitters index destinations with 16-bit offsets, guards for n=0 loops (65536 iterations in the original), bad sprite
pointers, missing files (return NULL), the zoom (0826) is paced by one `plat_wait_vsync` per pass (CPU speed before).

**Runtime contract.** Before `intro_run()`: `g_ds` = intro DGROUP image (≥ `INTRO_DS_MIN`=0xe2a bytes, 0x10000
recommended, inside the arena so that far tokens work) with its far pointers relocated like mem_init does:
0x94,0x98,0x9c,0xa0,0xa4,0xd0,0xd4,0xd8 → DGROUP strings (seg 0x6ed); 0x13a (03c1:0022), 0x8b2/0x8b6/0x8ba (RTL
exit hooks) and 0x10f2 (debug info) are not read by the port. After it returns: restore the game's `g_ds`, install
the game's timer/keyboard ISRs and palette (the intro leaves ISRs NULL and PIT divisor 0xffff).
**External functions needed:** `snd_driver_call`, `sys_launcher_config` (same as the game's), plus existing
`plat_set_palette, plat_wait_vsync, plat_yield, plat_quit_requested, plat_set_timer_isr, plat_set_timer_rate,
plat_set_kbd_isr, g_vram, data_fopen, explode, arena_alloc, arena_free, arena_owns, fp_token, fp_ptr`.

**DATA WARNING (TODO).** With /Users/gaming/Desktop/Eternam/INTRO.PAK (md5 ca22d134…), entries 3 (SPLASH) and 4
(SLIDE) decode to broken sprite streams (SPLASH row 40, SLIDE sprite 1 row 53 onward; SLIDE's palette at +0x2f937 is
garbage too, so every scene gets wrong colours). Verified by running the ORIGINAL 042e:0314 explode and 0488:01d0
blitter in Unicorn: same bytes, same garbage. Entries 0/1/5 and all EARTH/TATOU sprites are consistent → the file
copy is most likely damaged; try another copy of INTRO.PAK. Headless test: all scenes run to the end (8694 frames).
