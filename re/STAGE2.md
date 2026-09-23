# Stage 2 – integration decisions (read before writing C)

1. **Prototypes**: `src/game/protos.h` is merged from all groups (560 functions). Use it; do not
   redeclare other groups' functions. If one of YOUR functions needs a different prototype, edit its
   line in protos.h (only yours) and report the change. Helpers missing from the listing (hidden
   near routines) are `static` in your own .c file.
2. **Far pointers changed (re-read PORTING.md rule 4 and mem.h)**: stored far pointers are 32-bit
   *tokens in their original 4 bytes*. Read with `DSPTR(a)` / `FP(p)`, write with `DSPTR_SET(a,v)` /
   `FP_SET(p,v)`. Therefore memmove/memcpy/file I/O of blocks containing pointers need nothing
   special, and savegame re-linking code that does arithmetic/compares on the raw offset words must
   be translated literally with `DSU16()/PU16()` on the raw bytes (old DOS saves contain real seg:off
   values there; our own saves contain tokens whose low word behaves like an offset).
3. `farmalloc(-1L)` (largest free block) → `sys_farcoreleft()`. `farmalloc(n)` → `sub_0f27_0004(n)`.
   main's read of the far pointer at 0000:025c → `sys_launcher_config()`.
4. `sub_120e_0002(src, dst, len)` = movmem, **source first**. `sub_1209_000d(dst, len, val)` = setmem.
5. Timer ISRs (60 Hz) and the keyboard ISR are called by the platform from `plat_yield()` and
   `plat_wait_vsync()`. Every loop that waits on ISR-updated state (DS8 0x2d4e, 0x2d4a, DS16 0x2d54,
   0x2d60, 0x2840, 0x2d66, timers like DS32 0x1fdc updated by callbacks...) must call `plat_yield()`.
6. `sub_118f_00de` (wait retrace) and `sub_118f_0107` (present) are implemented natively; just call them.
   DSPTR(0x2d3c) is the screen (`g_vram`); drawing straight into it is fine.
7. Clip rect in SS: 0x192 ymin, 0x194 ymax, 0x196 xmin, 0x198 xmax (inclusive).
8. `sub_0791_0665` must end with `sub_11bb_0009(0)` (exit status 0).
9. Seg 0c9b: `cs:X` with X >= 0xe30 is DGROUP `X - 0x57e0` → `DS16(X-0x57e0)` etc.
   `sub_0c9b_0e15` (CPU probe) returns 1.
10. Keep the original RNG calls (`sub_1215_0017`, `sub_0f21_0004`) in the same order.
11. Output: one file per original code segment, `src/game/seg_SSSS.c`, all functions of that segment
    (including dead ones you prototyped). Compile-check each file with
    `clang -std=gnu11 -fsyntax-only -Wall -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-label -fno-strict-aliasing -I/opt/homebrew/include src/game/seg_SSSS.c`
    and fix every error and every warning about implicit declarations / incompatible pointer types /
    missing returns.
12. Wrap-around: `-fwrapv` is used; still use the right 16-bit types.
