# Eternam decompilation – porting guide

Goal: turn the original DOS game **Eternam** (Infogrames 1992, Turbo C 2.0 large model)
into readable, native C (no emulation) that compiles on macOS/arm64 with clang, on top
of an SDL-based platform layer. Every original function becomes one C function.

## Files

| Path | What |
|---|---|
| `re/ave3.asm` | annotated disassembly of the main game (AVE.CC1 entry 3). Linear addresses, `sub_SSSS_OOOO` = function at seg:off |
| `re/ave3.img` | raw load image (linear address = file offset) |
| `tools/fn.sh re/ave3.asm sub_XXXX_YYYY ...` | print the listing of given functions |
| `re/sigs_ave3.txt` | *heuristic* signature guesses (params from `[bp+N]`, arg bytes from caller cleanup). Verify! |
| `src/core/mem.h` | memory model macros (read it) |
| `re/protos/` `re/globals/` | stage-1 outputs (one file per segment group) |
| `src/game/` | stage-2 output: `seg_SSSS.c` per original code segment |

Useful facts:
* DGROUP (DS) = segment **0x1219**. `[0x1234]` with no segment override = DGROUP offset 0x1234.
* SS = 0x156f = DGROUP + 0x3560. `ss:[x]` = `SS16(x)`. Offsets 0x192..0x198 in SS are the clip
  rectangle (0x192 ymin, 0x194 ymax, 0x196 xmin, 0x198 xmax), `ss:[y*2]` is a row-offset table (y*320).
* The listing shows `cbw`/`cwd` correctly; `leave` = `mov sp,bp; pop bp`.
* `lcall sub_...` = far call, `call sub_...` preceded by `push cs` = far call to a function in the
  same segment (compiler idiom). All game functions are **far** (`retf`) unless shown otherwise.
* Screen is 320x200, 1 byte per pixel (VGA mode 13h). The game draws into far buffers; the
  pointer at `DSPTR(0x2d38)` is the current draw buffer, `DSPTR(0x2d3c)`/`0x3056` other buffers.
* `[0x2d3e]`=0xa000 is the VGA segment (value written only; treat as "the screen").

## Turbo C calling convention (large model)
* cdecl, args pushed right to left, caller pops. Far function: first arg at `[bp+6]`, near
  function: `[bp+4]`. A far pointer arg occupies 4 bytes (offset then segment).
* Return: `ax` (int / char), `dx:ax` (long or far pointer).
* `si`, `di` are register variables (locals). Locals at `[bp-N]`.
* `push ss / lea ax,[bp-N] / push ax` = address of a local (array/struct) passed as far pointer.
* `push ds / mov ax,0xNNNN / push ax` = pointer to DGROUP data → `DSADDR(0xNNNN)` or `DSSTR(0xNNNN)`.

### Runtime helpers in segment 0000 – translate inline
| helper | meaning | C |
|---|---|---|
| `sub_0000_0411` | LXMUL: `dx:ax * cx:bx` | `(int32_t)a * b` |
| `sub_0000_042a` | LDIV: stack args (long a, long b), returns dx:ax | `a / b` |
| `sub_0000_0433` | LMOD: stack args | `a % b` |
| `sub_0000_04cd` | LXLSH: `dx:ax << cl` | `(int32_t)v << n` |
| `sub_0000_04ec` | LXURSH: `dx:ax >> cl` (unsigned) | `(uint32_t)v >> n` |
| `sub_0000_0395` | huge ptr add: `dx:ax(ptr) + cx:bx(long)` → ptr | `p + n` |
| `sub_0000_03c2` | huge ptr sub: `ptr - cx:bx` | `p - n` |
| `sub_0000_0312` | huge ptr add-assign: `*(ptr*)(dx:ax) += cx:bx` | `q += n` |
| `sub_0000_03f0` | huge ptr compare (flags) | `p < q` etc. |
| `sub_0000_050b` | huge ptr difference `dx:ax - cx:bx` → long | `p - q` |

## C conventions (MANDATORY – code from many people must link together)
1. `#include "../core/mem.h"`, `"../platform/platform.h"`, `"protos.h"` (src/game/protos.h, generated from all groups' prototypes) at top of each `.c` file.
2. Keep the original name `sub_SSSS_OOOO` for every function (we rename globally later).
   Put a comment above each function: original address and a one-line description of
   what it does (be concrete: "draws a masked sprite at x,y", not "processes data").
3. Types: `int16_t`/`uint16_t` for 16-bit values (choose signed if compared with jl/jg/jle/jge or
   shifted with `sar`, unsigned for jb/ja/`shr`), `int8_t`/`uint8_t` for bytes, `int32_t` for longs.
   Far pointers are `uint8_t *` (byte pointers) and fields are read with `P8/P16/PU16/P32(p + off)`.
   You may define a `struct` only if it is purely local to one segment; otherwise use offsets
   with a comment (`P16(obj + 0x0e) /* x */`).
4. Globals: DGROUP variables are accessed with `DS8/DSS8/DS16/DSU16/DS32(0xADDR)` — do **not**
   declare C globals for them.
   **Far pointers stored in memory** keep their original 4 bytes, which hold a 32-bit *far token*
   (see mem.h). Read them with `DSPTR(0xADDR)` (DGROUP) or `FP(p + off)` (anywhere else), write them
   with `DSPTR_SET(0xADDR, ptr)` / `FP_SET(p + off, ptr)`. Because the token lives in the raw bytes,
   plain block copies (movmem/memcpy/file read/write) carry pointers correctly, `==` comparisons of the
   two words work, and code that does 16-bit arithmetic on the *offset word* of a stored pointer
   (e.g. `ip.off - start.off`) can be translated literally with `DSU16(a)`/`PU16(p)` on the raw bytes.
   Null tests: `DSPTR(a) == NULL` (token 0). When the original pushes/compares a far pointer as two
   words, use `fp_token(ptr)` to get the 32-bit value. Tables of far code pointers are called as
   `DSFN(int16_t (*)(int16_t), 0x1a8e + i*4)(args)`; the tables are pre-filled at start-up.
   All game heap memory comes from `farmalloc` (→ `arena_alloc`); never use malloc for game data.
5. Code-segment variables (writes to `cs:[x]`, self-modifying immediates like `add ax,0x1234` that
   another instruction patches) become `static` C variables. Read-only tables in a code segment
   are read with `CS8/CS16(seg, off)`.
6. 16-bit wrap-around and signedness must be preserved where they can matter.
7. No inline assembly, no hardware access, no DOS calls. Port I/O / `int` instructions only occur
   in the low-level segments and are replaced by `plat_*` calls (see platform.h).
8. **Busy-wait loops** that wait for something changed by an interrupt handler (timer counters,
   keyboard state `DS8(0x2d4e)`, key table, `[0x2840]`, `[0x2d66]`, sound status…) must call
   `plat_yield();` inside the loop body, otherwise the port hangs. Same for any wait-for-key loop.
9. Structure control flow properly (if/else/while/for/switch). `goto` only when unavoidable.
   Jump tables (`jmp word ptr cs:[bx+T]`) are `switch` statements.
10. When the code is clearly unreachable/debug-only, still translate it (it is small).
11. Don't guess: if something is truly ambiguous, write the literal translation and add
    `/* TODO(port): ... */`.
