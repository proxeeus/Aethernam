/*
 * Eternam - code segment 03dd: room script (bytecode) interpreter.
 *
 * Every room owns up to 17 cooperative scripts, which are loaded from R??.CC4 into the
 * buffer DSPTR(0x177e).  Script contexts sit at DS:0x7b2 + n*0x16:
 *   +0x00 fptr start      +0x04 fptr IP          +0x08 s8 bound actor
 *   +0x09 u8 flags        (01 wait walk, 02 wait timer, 04 wait anim, 08 wait sync,
 *                          10 wait menu, 20 stopped, 40 wait speech)
 *   +0x0a u16 timer / sync partner   +0x0c u16 loop counter
 *   +0x0e..+0x14 s16 zone x1,y1,x2,y2
 * The current context is DSPTR(0x305a).  Opcode handlers are called through the far
 * function table DS:0x1946 (ops 0x00..0x58).  A handler that must block sets
 * DS16(0x328e) (yield).
 * Actors: DS:0x11a + i*0x46 (actor 0 = hero).
 */
#include "../core/mem.h"
#include "../platform/platform.h"
#include "protos.h"

/* ---- local helpers (not original functions) ------------------------------ */

/* current script context (ds:[0x305a]) */
#define CTX()  DSPTR(0x305a)

/* IP += n  (the original adds to the offset word of the stored far pointer) */
static void ip_add(int16_t n)
{
    uint8_t *c = CTX();
    FP_SET(c + 4, FP(c + 4) + n);
}

/* actor index bound to the current script (ctx+8, sign-extended with cbw) */
static int16_t own_idx(void)
{
    return PS8(CTX() + 8);
}

/* script context n */
static uint8_t *ctx_n(uint16_t n)
{
    return DSADDR((uint16_t)(n * 0x16 + 0x7b2));
}

/* ======================================================================== */

/* 03dd:0008  reads entry `index` of a CC4/PAK file (int32 offset table at the start) into dest */
void sub_03dd_0008(uint8_t *filename, int16_t index, uint8_t *dest, int32_t size)
{
    uint8_t tab[0x196];                 /* [bp-0x196]: int32 offset table (entry 0 = table size) */
    int32_t h;
    int16_t first;

    h = sub_0e66_0031(filename, 0x3ed);
    sub_0e66_0191(h, tab, 4L);
    first = P16(tab);
    sub_0e66_0191(h, tab + 4, (int32_t)(int16_t)(first - 4));
    /* TODO(port): like the original, there is no bounds check on index (table holds at most 0x65 entries) */
    sub_0e66_0006(h, P32(tab + index * 4), -1L);
    sub_0e66_0191(h, dest, size);
    sub_0e66_022b(h);
}

/* 03dd:00ac  returns the actor struct DS:0x11a + actor*0x46 */
uint8_t *sub_03dd_00ac(int16_t actor)
{
    return DSADDR((uint16_t)(actor * 0x46 + 0x11a));
}

/* 03dd:00bc  resets script context n to the start of script n (stopped, no actor) */
void sub_03dd_00bc(uint16_t n)
{
    uint8_t *buf = DSPTR(0x177e);
    uint16_t off = PU16(buf + (uint16_t)(n * 2));   /* offset table at the start of the script buffer */
    uint8_t *start = buf + off;
    uint8_t *c = ctx_n(n);

    FP_SET(c + 4, start);           /* IP */
    FP_SET(c + 0, start);           /* start */
    P8(c + 8) = 0;                  /* actor */
    P8(c + 9) = 0x20;               /* flags: stopped */
    PU16(c + 0xa) = 0;
    PU16(c + 0xc) = 0;
}

/* 03dd:011f  clears field +0x2a (moving) of actors 1..9 */
void sub_03dd_011f(void)
{
    uint8_t *p = DSADDR(0x160);
    int16_t i;

    for (i = 1; i < 10; i++) {
        PU16(p + 0x2a) = 0;
        p += 0x46;
    }
}

/* 03dd:014c  resets the room globals that scripts set (effect mode, tables, control lock...) */
void sub_03dd_014c(void)
{
    sub_03dd_05d4();
    DS8(0xd2) = 0x10;
    DSU16(0xee) = 0;
    DSU16(0xc6) = 0;
    DS8(0xc4) = 0;
    DSU16(0x327a) = 0;
    DSU16(0xa8) = 0;
    DSU16(0xf0) = 0;
    DSU16(0xbbe) = 0;
    DSU16(0xbbc) = 0;
    DSU16(0xb6) = 0;
    DSU16(0xb4) = 0;
    DSU16(0xc0) = 0;
}

/* 03dd:017d  starts the freshly loaded room scripts: sets up all contexts, enables and runs script 0 */
void sub_03dd_017d(void)
{
    uint16_t i;

    sub_03dd_014c();
    DSU16(0xd8) = PU16(DSPTR(0x177e)) >> 1;          /* number of scripts */
    for (i = 0; i < DSU16(0xd8); i++)
        sub_03dd_00bc(i);
    DS8(0x7bb) = 0;                                   /* script 0 flags: running */
    sub_03dd_011f();
    sub_0596_0033();
    sub_03dd_1ac0(0);
}

/* 03dd:01c8  loads the room script (R??.CC4 entry [0xce]); starts it, or relinks it after a savegame load */
void sub_03dd_01c8(void)
{
    sub_03dd_0008(DSPTR(0x18a8), DS16(0xce), DSPTR(0x177e), 0x898L);
    if (DS16(0x96) == 0)
        sub_03dd_017d();
    else
        sub_07fe_015c();
}

/* 03dd:01fb  fetches the next bytecode byte */
uint8_t sub_03dd_01fb(void)
{
    uint8_t *c = CTX();
    uint8_t *ip = FP(c + 4);

    FP_SET(c + 4, ip + 1);
    return P8(ip);
}

/* 03dd:0213  fetches the next bytecode byte doubled (X coordinates are stored halved) */
uint16_t sub_03dd_0213(void)
{
    uint8_t *c = CTX();
    uint8_t *ip = FP(c + 4);

    FP_SET(c + 4, ip + 1);
    return (uint16_t)(P8(ip) << 1);
}

/* 03dd:022f  fetches the next bytecode 16-bit word */
int16_t sub_03dd_022f(void)
{
    int16_t w = P16(FP(CTX() + 4));

    ip_add(2);
    return w;
}

/* 03dd:024e  skips one bytecode byte (consumes the opcode) */
void sub_03dd_024e(void)
{
    ip_add(1);
}

/* 03dd:0257  relative jump: IP += int16 at IP (relative to the offset word itself) */
void sub_03dd_0257(void)
{
    int16_t rel = P16(FP(CTX() + 4));

    ip_add(rel);
}

/* 03dd:026b  script comparison: op 0 ==, 1 !=, 2 >, 3 >=, 4 <, 5 <=, 6 a&b, 7 a|b; returns 1/0 */
uint8_t sub_03dd_026b(int16_t a, int16_t b, uint16_t op)
{
    uint8_t r = 0;

    switch (op) {
    case 0: if (a == b) r = 1; break;
    case 1: if (a != b) r = 1; break;
    case 2: if (a > b) r = 1; break;
    case 3: if (a >= b) r = 1; break;
    case 4: if (a < b) r = 1; break;
    case 5: if (a <= b) r = 1; break;
    case 6: if (a & b) r = 1; break;
    case 7: if (a | b) r = 1; break;
    default: break;
    }
    return r;
}

/* 03dd:02f0  address of script variable var (0..0x45 via pointer table 0x166e, then two word banks) */
uint8_t *sub_03dd_02f0(int16_t var)
{
    static uint8_t s_badvar[2];

    if (var < 0x46) {
        if (var < 0 || var >= 42) {
            /* TODO(port): table 0x166e has only 42 real far pointers; slots 42..69 (and
             * negative indices) overlap other globals and point to random DOS memory in the
             * original.  Return a harmless scratch word instead of dereferencing garbage. */
            return s_badvar;
        }
        return DSPTR((uint16_t)(var * 4 + 0x166e));
    }
    if (var < 0x164)
        return DSADDR((uint16_t)((var - 0x46) * 2 + 0x11bc));
    return DSADDR((uint16_t)((var - 0x164) * 2 + 0xfbc));
}

/* 03dd:034c  fetches a variable index from the bytecode */
int16_t sub_03dd_034c(void)
{
    return sub_03dd_022f();
}

/* 03dd:035b  fetches a variable index and returns the variable's value */
int16_t sub_03dd_035b(void)
{
    int16_t v = sub_03dd_034c();

    return P16(sub_03dd_02f0(v));
}

/* 03dd:0380  fetches a tagged immediate: tag 0x0c -> 8-bit value, any other tag -> 16-bit value */
int16_t sub_03dd_0380(void)
{
    int16_t v;

    if (sub_03dd_01fb() == 0x0c)
        v = sub_03dd_01fb();
    else
        v = sub_03dd_022f();
    return v;
}

/* 03dd:03da  fetches a value operand: 0x0b var16 (variable) or a tagged immediate */
int16_t sub_03dd_03da(void)
{
    int16_t v;

    if (P8(FP(CTX() + 4)) == 0x0b) {
        sub_03dd_024e();
        v = sub_03dd_035b();
    } else {
        v = sub_03dd_0380();
    }
    return v;
}

/* 03dd:0407  effect 1 (op 0x33 fx 1): transition mode 1 */
void sub_03dd_0407(void)
{
    DSU16(0x10c) = 1;
}

/* 03dd:040e  effect 4: transition mode 4, loads palette number [0x3036] */
void sub_03dd_040e(void)
{
    DSU16(0x10c) = 4;
    sub_0791_0187(DS16(0x3036));
}

/* 03dd:0420  effect 2: transition mode 2 */
void sub_03dd_0420(void)
{
    DSU16(0x10c) = 2;
}

/* 03dd:0427  effect 3: transition mode 3, runs the rect-list effect at ([0x3036],[0x3038]) */
void sub_03dd_0427(void)
{
    DSU16(0x10c) = 3;
    sub_0791_020d(DS16(0x3036), DS16(0x3038), 0x32, 0x1f4);
}

/* 03dd:0443  effect 5: sets flag [0x17e2] */
void sub_03dd_0443(void)
{
    DSU16(0x17e2) = 1;
}

/* 03dd:044a  effect 6: draws "The End" at ([0x3036],[0x3038]) in colour 0x49 */
void sub_03dd_044a(void)
{
    sub_0596_02a0(0x49);
    sub_1172_0012(DS16(0x3036), DS16(0x3038), DSPTR(0x2d38), DSADDR(0x1ab4));
}

/* 03dd:0471  op 0x20: var = operand */
void sub_03dd_0471(void)
{
    int16_t var, val;

    sub_03dd_024e();
    var = sub_03dd_034c();
    val = sub_03dd_03da();
    P16(sub_03dd_02f0(var)) = val;
}

/* 03dd:04a3  op 0x21: var += operand */
void sub_03dd_04a3(void)
{
    int16_t var, val;

    sub_03dd_024e();
    var = sub_03dd_034c();
    val = sub_03dd_03da();
    P16(sub_03dd_02f0(var)) += val;
}

/* 03dd:04d5  op 0x38: var |= operand */
void sub_03dd_04d5(void)
{
    int16_t var, val;

    sub_03dd_024e();
    var = sub_03dd_034c();
    val = sub_03dd_03da();
    P16(sub_03dd_02f0(var)) |= val;
}

/* 03dd:0507  op 0x39: var &= operand */
void sub_03dd_0507(void)
{
    int16_t var, val;

    sub_03dd_024e();
    var = sub_03dd_034c();
    val = sub_03dd_03da();
    P16(sub_03dd_02f0(var)) &= val;
}

/* 03dd:0539  op 0x22: var -= operand */
void sub_03dd_0539(void)
{
    int16_t var, val;

    sub_03dd_024e();
    var = sub_03dd_034c();
    val = sub_03dd_03da();
    P16(sub_03dd_02f0(var)) -= val;
}

/* 03dd:056b  op 0x23: own actor +0x25 = 8 */
void sub_03dd_056b(void)
{
    sub_03dd_024e();
    P8(DSADDR((uint16_t)(own_idx() * 0x46 + 0x11a)) + 0x25) = 8;
}

/* 03dd:058b  op 0x0a IF: [var16][cmp8][operand][rel16]; jump when the comparison is false */
void sub_03dd_058b(void)
{
    int16_t a, b;
    uint16_t op;

    sub_03dd_024e();
    a = sub_03dd_035b();
    op = sub_03dd_01fb();
    b = sub_03dd_03da();
    if (sub_03dd_026b(a, b, op) == 0)
        sub_03dd_0257();
    else
        ip_add(2);
}

/* 03dd:05d4  effect 0: leaves transition mode (restores the room palette if mode was 4) */
void sub_03dd_05d4(void)
{
    if (DS16(0x10c) == 4)
        sub_0791_067f(DSPTR(0x304c));
    DSU16(0x10c) = 0;
}

/* 03dd:05f2  ops 0x0b/0x0c/0x0d/0x31: empty (operand tags, never executed as opcodes) */
void sub_03dd_05f2(void)
{
}

/* 03dd:05f3  op 0x00: [anim8] plays an animation once on the own actor and waits for its end */
void sub_03dd_05f3(void)
{
    int16_t anim;
    uint8_t *act;

    sub_03dd_024e();
    anim = sub_03dd_01fb();
    act = sub_03dd_00ac(own_idx());
    sub_0053_0cf2(act, anim);
    P16(act + 0x10) = 2;                    /* anim locked */
    P8(CTX() + 9) |= 4;                     /* wait for animation end */
    DSU16(0x328e) = 1;
}

/* 03dd:0643  op 0x51: waits until all keys/buttons are released, then until one is pressed */
void sub_03dd_0643(void)
{
    sub_03dd_024e();
    while (DS16(0x2d54) != 0)
        plat_yield();
    while (DS8(0x2d4a) != 0)
        plat_yield();
    while (DS8(0x2d4e) != 0)
        plat_yield();
    while (DS8(0x2d4e) == 0 && DS8(0x2d4a) == 0 && DS16(0x2d54) == 0) {
        sub_1215_0017();                    /* stirs the RNG while waiting */
        plat_yield();
    }
}

/* 03dd:0679  op 0x2d: [anim8][n8] sets own actor animation id and frame fields directly (locked) */
void sub_03dd_0679(void)
{
    uint16_t anim, n;
    uint8_t *act;

    sub_03dd_024e();
    anim = sub_03dd_01fb();
    n = sub_03dd_01fb();
    act = sub_03dd_00ac(own_idx());
    PU16(act + 0x1a) = anim;
    PU16(act + 0x20) = 0;
    PU16(act + 0x10) = 2;
    P8(act + 0x22) = (uint8_t)n;
    PU16(act + 0x1c) = n;
}

/* 03dd:06d1  op 0x2e: [n8] plays sound effect n */
void sub_03dd_06d1(void)
{
    int16_t n;

    sub_03dd_024e();
    n = sub_03dd_01fb();
    sub_0791_00e5(n);
}

/* 03dd:06ee  op 0x48: [n8] plays music n (0xff = stop music) */
void sub_03dd_06ee(void)
{
    int16_t n;

    sub_03dd_024e();
    n = sub_03dd_01fb();
    if (n != 0xff)
        sub_0791_0078(n);
    else
        sub_0791_0039();
}

/* 03dd:0717  op 0x49: [v8] sets the music volume */
void sub_03dd_0717(void)
{
    int16_t v;

    sub_03dd_024e();
    v = sub_03dd_01fb();
    sub_0791_0005(v);
}

/* 03dd:0734  op 0x4a: [n8] [0x304a] = random(n) (script variable 41) */
void sub_03dd_0734(void)
{
    sub_03dd_024e();
    DS16(0x304a) = sub_0053_05f6(sub_03dd_01fb());
}

/* 03dd:074a  op 0x2f: [n8] sprite bank parameter for the next op 0x16 */
void sub_03dd_074a(void)
{
    sub_03dd_024e();
    DSU16(0x3268) = sub_03dd_01fb();
}

/* 03dd:0758  op 0x33: [fx8][x/2][y8] runs screen effect fx (table 0x1a8e) with ([0x3036],[0x3038]) */
void sub_03dd_0758(void)
{
    uint16_t fx;

    sub_03dd_024e();
    fx = sub_03dd_01fb();
    DSU16(0x3036) = sub_03dd_0213();
    DSU16(0x3038) = sub_03dd_01fb();
    DSFN(void (*)(void), (uint16_t)(fx * 4 + 0x1a8e))();
}

/* 03dd:0787  op 0x34: [v8] [0x177c] = v; when it goes from nonzero to 0 sets [0x1738] */
void sub_03dd_0787(void)
{
    int16_t old;
    uint16_t v;

    sub_03dd_024e();
    old = DS16(0x177c);
    v = sub_03dd_01fb();
    DSU16(0x177c) = v;
    if (old != 0 && v == 0)
        DSU16(0x1738) = 1;
}

/* 03dd:07b2  op 0x35: choice menu [q16][x/2][y8][n8] n*{[text16][rel16]}; waits for the choice */
void sub_03dd_07b2(void)
{
    uint8_t *e;
    uint8_t *c;
    int16_t i;

    sub_03dd_024e();
    sub_0596_067e();
    DS16(0x3278) = sub_03dd_022f();
    DSPTR_SET(0x3252, sub_03dd_165d(DS16(0x3278), DSPTR(0x325a)));
    DSU16(0x302c) = sub_03dd_0213();
    DSU16(0x302e) = sub_03dd_01fb();
    DSU16(0x303a) = sub_03dd_01fb();

    e = DSADDR(0x306a);
    for (i = 0; i < DS16(0x303a); i++) {
        P16(e) = sub_03dd_022f();                                /* text index */
        FP_SET(e + 2, sub_03dd_165d(P16(e), DSPTR(0x325a)));     /* text pointer */
        c = CTX();
        FP_SET(e + 6, FP(c + 4));                               /* address of the rel16 jump */
        ip_add(2);
        e += 10;
    }
    if (DS16(0x3278) == DS16(0x306a))
        DS16(0x3278) = -1;                  /* question equals first answer: no question */
    DSU16(0x3274) = 0;
    DSU16(0x3240) = 1;
    P8(CTX() + 9) |= 0x10;
    DSU16(0x328e) = 1;
}

/* 03dd:088c  op 0x37: sets flag [0x173e] */
void sub_03dd_088c(void)
{
    sub_03dd_024e();
    DSU16(0x173e) = 1;
}

/* 03dd:0897  op 0x3a: [a8][b8] [0xbbc]=a, [0xbbe]=b, allocates the backdrop buffer */
void sub_03dd_0897(void)
{
    sub_03dd_024e();
    DSU16(0xbbc) = sub_03dd_01fb();
    DSU16(0xbbe) = sub_03dd_01fb();
    sub_0596_0001();
}

/* 03dd:08b3  op 0x3b: [v8] byte [0xc4] = v */
void sub_03dd_08b3(void)
{
    sub_03dd_024e();
    DS8(0xc4) = sub_03dd_01fb();
}

/* 03dd:08bf  op 0x32: no-op with three operand bytes */
void sub_03dd_08bf(void)
{
    sub_03dd_024e();
    sub_03dd_024e();
    sub_03dd_024e();
    sub_03dd_024e();
}

/* 03dd:08d0  op 0x30: moves the own actor to the hero position (y+1) */
void sub_03dd_08d0(void)
{
    uint8_t *hero = DSADDR(0x11a);
    uint8_t *act;

    sub_03dd_024e();
    act = sub_03dd_00ac(own_idx());
    P16(act) = P16(hero);
    P16(act + 2) = P16(hero + 2) + 1;
}

/* 03dd:0915  op 0x01: [dir8] turns the own actor to face dir */
void sub_03dd_0915(void)
{
    int16_t dir;
    uint8_t *act;

    sub_03dd_024e();
    dir = sub_03dd_01fb();
    act = sub_03dd_00ac(own_idx());
    P16(act + 0x10) = 0;
    sub_0053_0e27(act, dir);
}

/* 03dd:0956  op 0x15: turns the own actor towards the hero */
void sub_03dd_0956(void)
{
    uint8_t *act;
    uint8_t *hero = DSADDR(0x11a);
    int16_t dir;

    sub_03dd_024e();
    act = sub_03dd_00ac(own_idx());
    dir = sub_0053_0e77(P16(act), P16(act + 2), P16(hero), P16(hero + 2));
    P16(act + 0x10) = 0;
    sub_0053_0e27(act, dir);
}

/* 03dd:09b7  op 0x17: [n8] n*{[x/2][y8]} sets the floor polyline (0x1832) and rasterizes it */
void sub_03dd_09b7(void)
{
    uint16_t n;
    uint8_t *p;
    int16_t i;

    sub_03dd_024e();
    n = sub_03dd_01fb();
    p = DSADDR(0x1832);
    PU16(p) = n;
    p += 2;
    for (i = 0; i < (int16_t)n; i++) {
        PU16(p) = sub_03dd_0213();
        p += 2;
        PU16(p) = sub_03dd_01fb();
        p += 2;
    }
    sub_0053_0cc4();
}

/* 03dd:0a13  op 0x18: [n8] n*{[b8][b8][x/2][x/2]} fills the 6-byte table at 0x752, [0xa8]=n */
void sub_03dd_0a13(void)
{
    uint16_t n;
    uint8_t *p;
    int16_t i;

    sub_03dd_024e();
    n = sub_03dd_01fb();
    p = DSADDR(0x752);
    for (i = 0; i < (int16_t)n; i++) {
        P8(p) = sub_03dd_01fb();
        P8(p + 1) = sub_03dd_01fb();
        PU16(p + 2) = sub_03dd_0213();
        PU16(p + 4) = sub_03dd_0213();
        p += 6;
    }
    DSU16(0xa8) = n;
}

/* 03dd:0a77  op 0x19: [mode8] fills 5 words (10 if mode==2) at 0xbc0, [0xc6]=mode */
void sub_03dd_0a77(void)
{
    uint16_t mode;
    uint8_t *p;

    sub_03dd_024e();
    mode = sub_03dd_01fb();
    p = DSADDR(0xbc0);
    PU16(p) = sub_03dd_01fb();  p += 2;
    PU16(p) = sub_03dd_01fb();  p += 2;
    PU16(p) = sub_03dd_01fb();  p += 2;
    PU16(p) = sub_03dd_0213();  p += 2;
    PU16(p) = sub_03dd_01fb();  p += 2;
    if (mode == 2) {
        PU16(p) = sub_03dd_01fb();  p += 2;
        PU16(p) = sub_03dd_01fb();  p += 2;
        PU16(p) = sub_03dd_01fb();  p += 2;
        PU16(p) = sub_03dd_0213();  p += 2;
        PU16(p) = sub_03dd_01fb();  p += 2;
    }
    DSU16(0xc6) = mode;
}

/* 03dd:0b3c  helper of ops 0x1a/0x4f: appends {id (byte, or word if wide), 0, wide, x, y} to 0xa18 */
void sub_03dd_0b3c(int16_t wide)
{
    int16_t id, x, y;
    uint8_t *e;

    sub_03dd_024e();
    if (wide == 0)
        id = sub_03dd_01fb();
    else
        id = sub_03dd_022f();
    x = sub_03dd_0213();
    y = sub_03dd_01fb();
    e = DSADDR((uint16_t)(DSU16(0xf0) * 8 + 0xa18));
    DSU16(0xf0)++;
    P16(e) = id;
    P8(e + 2) = 0;
    P8(e + 3) = (uint8_t)wide;
    P16(e + 4) = x;
    P16(e + 6) = y;
}

/* 03dd:0ba9  op 0x1a: table 0xa18 entry with a byte id */
void sub_03dd_0ba9(void)
{
    sub_03dd_0b3c(0);
}

/* 03dd:0bb2  op 0x4f: table 0xa18 entry with a word id */
void sub_03dd_0bb2(void)
{
    sub_03dd_0b3c(1);
}

/* 03dd:0bbb  returns 1 if the next opcode is another animation (0x1f) or wait-key (0x51) */
int16_t sub_03dd_0bbb(void)
{
    uint8_t op = P8(FP(CTX() + 4));

    return (op == 0x1f || op == 0x51) ? 1 : 0;
}

/* 03dd:0bf5  op 0x1f: [pak8][seq8][bg16][reps8][n8] n*{3 bytes sound cues}: plays a PAK animation */
void sub_03dd_0bf5(void)
{
    int16_t pak, seq, bg, reps;

    sub_03dd_024e();
    pak = sub_03dd_01fb();
    seq = sub_03dd_01fb();
    bg = sub_03dd_022f();
    reps = sub_03dd_01fb();
    DSU16(0x328c) = sub_03dd_01fb();
    DSPTR_SET(0x32a2, FP(CTX() + 4));           /* sound cue triples */
    ip_add((int16_t)(DSU16(0x328c) * 3));
    sub_0596_0f41(pak, seq, bg, reps);
}

/* 03dd:0c5b  op 0x29: stops the own actor (not the hero) and releases its sprite */
void sub_03dd_0c5b(void)
{
    uint8_t *act;

    sub_03dd_024e();
    if (P8(CTX() + 8) != 0) {
        act = sub_03dd_00ac(own_idx());
        PU16(act + 0x2a) = 0;
        if (FP(act + 0x16) != NULL)
            sub_0053_01e8(act);
    }
}

/* 03dd:0ca5  op 0x2a: [a/2][b/2] own actor +0x3c = a, +0x3e = b */
void sub_03dd_0ca5(void)
{
    uint8_t *act;

    sub_03dd_024e();
    act = sub_03dd_00ac(own_idx());
    PU16(act + 0x3c) = sub_03dd_0213();
    PU16(act + 0x3e) = sub_03dd_0213();
}

/* 03dd:0cdd  op 0x2b: [a8][b8] own actor bytes +0x40 = a, +0x41 = b */
void sub_03dd_0cdd(void)
{
    uint8_t *act;

    sub_03dd_024e();
    act = sub_03dd_00ac(own_idx());
    P8(act + 0x40) = sub_03dd_01fb();
    P8(act + 0x41) = sub_03dd_01fb();
}

/* 03dd:0d15  op 0x2c: [v8] [0xd0] = v (0xff -> -1) */
void sub_03dd_0d15(void)
{
    int16_t v;

    sub_03dd_024e();
    v = sub_03dd_01fb();
    if (v == 0xff)
        v = -1;
    DS16(0xd0) = v;
}

/* 03dd:0d3a  op 0x4b: [room8] requests a room change ([0xa2] = [0x32a6] = room) */
void sub_03dd_0d3a(void)
{
    uint16_t room;

    sub_03dd_024e();
    room = sub_03dd_01fb();
    DSU16(0xa2) = room;
    DSU16(0x32a6) = room;
}

/* 03dd:0d4b  op 0x1b: copy-protection question */
void sub_03dd_0d4b(void)
{
    sub_03dd_024e();
    sub_0c05_00fc();
}

/* 03dd:0d55  op 0x02: stops the current script (does not advance IP) */
void sub_03dd_0d55(void)
{
    P8(CTX() + 9) |= 0x20;
    DSU16(0x328e) = 1;
}

/* 03dd:0d65  op 0x1c: [n8] restarts script n from its beginning and enables it (if n < count) */
void sub_03dd_0d65(void)
{
    uint16_t n;

    sub_03dd_024e();
    n = sub_03dd_01fb();
    if (n < DSU16(0xd8)) {
        sub_03dd_00bc(n);
        P8(ctx_n(n) + 9) &= 0xdf;
    }
}

/* 03dd:0d9c  op 0x1d: [n8] stops script n */
void sub_03dd_0d9c(void)
{
    uint16_t n;

    sub_03dd_024e();
    n = sub_03dd_01fb();
    P8(ctx_n(n) + 9) |= 0x20;
}

/* 03dd:0dc3  op 0x1e: list of script numbers ending with 0xff: restarts and enables each */
void sub_03dd_0dc3(void)
{
    uint16_t n;

    sub_03dd_024e();
    while ((n = sub_03dd_01fb()) != 0xff) {
        sub_03dd_00bc(n);
        P8(ctx_n(n) + 9) &= 0xdf;
    }
}

/* 03dd:0e01  op 0x03: GOTO rel16 */
void sub_03dd_0e01(void)
{
    sub_03dd_024e();
    sub_03dd_0257();
}

/* 03dd:0e0a  op 0x43: resets the loop counter, then GOTO rel16 */
void sub_03dd_0e0a(void)
{
    PU16(CTX() + 0xc) = 0;
    sub_03dd_0e01();
}

/* 03dd:0e19  op 0x24: [x/2][y8] own actor walks to (x,y); waits for arrival */
void sub_03dd_0e19(void)
{
    int16_t x, y;

    sub_03dd_024e();
    x = sub_03dd_0213();
    y = sub_03dd_01fb();
    P16(DSADDR((uint16_t)(own_idx() * 0x46 + 0x11a)) + 0x10) = 0;
    sub_0714_023e(own_idx(), x, y);
    P8(CTX() + 9) |= 1;
    DSU16(0x328e) = 1;
}

/* 03dd:0e78  helper of ops 0x04/0x05: [c8][anim8] walks to x=c*2 (vert=0) or y=c (vert=1) */
void sub_03dd_0e78(int16_t vert)
{
    uint16_t c, anim;
    int16_t x, y;
    uint8_t *act;

    sub_03dd_024e();
    c = sub_03dd_01fb();
    anim = sub_03dd_01fb();
    act = sub_03dd_00ac(own_idx());
    P16(act + 0x10) = 0;
    x = P16(act);
    y = P16(act + 2);
    if (vert == 0)
        x = (int16_t)(c << 1);
    else
        y = (int16_t)c;
    sub_0714_023e(own_idx(), x, y);
    if (vert == 0)
        P16(act + 0x30) |= 8;               /* horizontal walk */
    else
        P16(act + 0x30) |= 0x10;            /* vertical walk */
    sub_0053_0cf2(act, anim);
    P16(act + 0x10) = 0;
    P8(CTX() + 9) |= 1;
    DSU16(0x328e) = 1;
}

/* 03dd:0f38  op 0x04: walk horizontally */
void sub_03dd_0f38(void)
{
    sub_03dd_0e78(0);
}

/* 03dd:0f41  op 0x05: walk vertically */
void sub_03dd_0f41(void)
{
    sub_03dd_0e78(1);
}

/* 03dd:0f4a  helper of ops 0x0f/0x10: [anim8] walks to the hero's x (vert=0) or y (vert=1) */
void sub_03dd_0f4a(int16_t vert)
{
    uint16_t anim;
    int16_t x, y;
    uint8_t *act;

    sub_03dd_024e();
    anim = sub_03dd_01fb();
    act = sub_03dd_00ac(own_idx());
    P16(act + 0x10) = 0;
    if (vert == 0) {
        x = DS16(0x11a);
        y = P16(act + 2);
    } else {
        x = P16(act);
        y = DS16(0x11c);
    }
    sub_0714_023e(own_idx(), x, y);
    if (vert == 0)
        P16(act + 0x30) |= 8;
    else
        P16(act + 0x30) |= 0x10;
    sub_0053_0cf2(act, anim);
    P16(act + 0x10) = 0;
    P8(CTX() + 9) |= 1;
    DSU16(0x328e) = 1;
}

/* 03dd:0ffd  op 0x0f: walk to the hero's column */
void sub_03dd_0ffd(void)
{
    sub_03dd_0f4a(0);
}

/* 03dd:1006  op 0x10: walk to the hero's row */
void sub_03dd_1006(void)
{
    sub_03dd_0f4a(1);
}

/* 03dd:100f  op 0x11: [dx s8][dy s8] own actor walks next to the hero (hero pos + dx,dy, clipped) */
void sub_03dd_100f(void)
{
    int16_t dx, dy;
    int16_t x, y;
    uint8_t *act;

    sub_03dd_024e();
    dx = (int8_t)sub_03dd_01fb();
    dy = (int8_t)sub_03dd_01fb();
    act = sub_03dd_00ac(own_idx());
    P16(act + 0x10) = 0;
    x = DS16(0x11a) + dx;
    y = DS16(0x11c) + dy;
    sub_0714_0160((uint8_t *)&x, (uint8_t *)&y, P8(act + 0x23), P8(act + 0x24));
    sub_0714_023e(own_idx(), x, y);
    P8(CTX() + 9) |= 1;
    P8(act + 0x28) = 6;
    DSU16(0x328e) = 1;
}

/* 03dd:10ab  op 0x42: own actor enters the action stance (8), +0x14 = 1 */
void sub_03dd_10ab(void)
{
    uint8_t *act;

    sub_03dd_024e();
    act = sub_03dd_00ac(own_idx());
    sub_0053_0e52(act, 8);
    P8(act + 0x14) = 1;
}

/* 03dd:10e1  op 0x45: [v8] own actor +0x14 = v */
void sub_03dd_10e1(void)
{
    uint8_t *act;

    sub_03dd_024e();
    act = sub_03dd_00ac(own_idx());
    P8(act + 0x14) = sub_03dd_01fb();
}

/* 03dd:110e  op 0x4c: [x1/2][y1][x2/2][y2] sets the rect at 0xde and its valid flag [0xdc] */
void sub_03dd_110e(void)
{
    sub_03dd_024e();
    DSU16(0xdc) = 1;
    DSU16(0xde) = sub_03dd_0213();
    DSU16(0xe2) = sub_03dd_01fb();
    DSU16(0xe0) = sub_03dd_0213();
    DSU16(0xe4) = sub_03dd_01fb();
}

/* 03dd:1139  op 0x46: [v8] [0xac] = v */
void sub_03dd_1139(void)
{
    sub_03dd_024e();
    DSU16(0xac) = sub_03dd_01fb();
}

/* 03dd:1147  requests special scene 8 with parameter param (script var 0x4a), locks controls */
void sub_03dd_1147(int16_t param)
{
    DSU16(0xa2) = 8;
    DSU16(0x32a6) = 8;
    DSU16(0xd0) = 0;
    DSU16(0x329e) = 0;
    DS16(0x11c4) = param;
    DSU16(0xc0) = 0xf;
    DSU16(0x12c) = 1;                       /* hero direction */
}

/* 03dd:116f  op 0x47: [p8] goes to special scene 8 with parameter p, yields */
void sub_03dd_116f(void)
{
    int16_t p;

    sub_03dd_024e();
    p = sub_03dd_01fb();
    sub_03dd_1147(p);
    DSU16(0x173e) = 1;
    DSU16(0x328e) = 1;
}

/* 03dd:1197  op 0x06 LOOP: [rel16][count8] jumps back until the loop counter reaches count */
void sub_03dd_1197(void)
{
    uint16_t count;
    uint8_t *c;

    sub_03dd_024e();
    count = P8(FP(CTX() + 4) + 2);
    c = CTX();
    PU16(c + 0xc)++;
    if (PU16(c + 0xc) < count) {
        sub_03dd_0257();
    } else {
        c = CTX();
        PU16(c + 0xc) = 0;
        ip_add(3);
    }
}

/* 03dd:11da  op 0x07: [x/2][y8] places the own actor at (x,y) */
void sub_03dd_11da(void)
{
    int16_t x, y;

    sub_03dd_024e();
    x = sub_03dd_0213();
    y = sub_03dd_01fb();
    sub_0053_1c18(own_idx(), x, y);
}

/* 03dd:120c  op 0x08: [n8] waits until script n reaches its matching sync with this script */
void sub_03dd_120c(void)
{
    uint16_t n;
    uint8_t *c;

    sub_03dd_024e();
    n = sub_03dd_01fb();
    c = CTX();
    PU16(c + 0xa) = n;
    P8(c + 9) |= 8;
    DSU16(0x328e) = 1;
}

/* 03dd:122a  op 0x0e: IF hero in zone [x1/2][y1][x2/2][y2][rel16] (zone kept in ctx), else jump */
void sub_03dd_122a(void)
{
    uint8_t *hero = DSADDR(0x11a);
    int16_t zone[4];                        /* [bp-0x18] */
    int16_t box[4];                         /* [bp-0xc]: hero bounding box */
    uint8_t *c;

    sub_03dd_024e();
    zone[0] = sub_03dd_0213();  c = CTX(); P16(c + 0xe)  = zone[0];
    zone[1] = sub_03dd_01fb();  c = CTX(); P16(c + 0x10) = zone[1];
    zone[2] = sub_03dd_0213();  c = CTX(); P16(c + 0x12) = zone[2];
    zone[3] = sub_03dd_01fb();  c = CTX(); P16(c + 0x14) = zone[3];

    box[0] = P16(hero) - P8(hero + 0x23);
    box[1] = P16(hero + 2) - P8(hero + 0x24);
    box[2] = P16(hero) + P8(hero + 0x23);
    box[3] = P16(hero + 2);

    if ((uint8_t)sub_0e9b_000c((uint8_t *)zone, (uint8_t *)box) == 0)
        sub_03dd_0257();
    else
        ip_add(2);
}

/* 03dd:1331  op 0x3d: IF event [0x327a] (and clear it) else jump rel16 */
void sub_03dd_1331(void)
{
    sub_03dd_024e();
    if (DS16(0x327a) == 0) {
        sub_03dd_0257();
    } else {
        ip_add(2);
        DSU16(0x327a) = 0;
    }
}

/* 03dd:1352  op 0x3e: [x1/2][y1][x2/2][y2] appends a rect to the effect list 0x30ae (x1,y1,y2,x2) */
void sub_03dd_1352(void)
{
    uint8_t *e;

    sub_03dd_024e();
    e = DSADDR((uint16_t)(DSU16(0x1740) * 8 + 0x30ae));
    DSU16(0x1740)++;
    PU16(e) = sub_03dd_0213();
    PU16(e + 2) = sub_03dd_01fb();
    PU16(e + 6) = sub_03dd_0213();
    PU16(e + 4) = sub_03dd_01fb();
}

/* 03dd:13a6  op 0x44: [x1/2][y1][x2/2][y2] sets the rect 0x3020..0x3026 and its flag [0x32a0] */
void sub_03dd_13a6(void)
{
    sub_03dd_024e();
    DSU16(0x32a0) = 1;
    DSU16(0x3020) = sub_03dd_0213();
    DSU16(0x3022) = sub_03dd_01fb();
    DSU16(0x3024) = sub_03dd_0213();
    DSU16(0x3026) = sub_03dd_01fb();
}

/* 03dd:13d1  op 0x3c: [4 bytes] appends a 4-byte entry to table 0x1286 (count byte [0x1284]) */
void sub_03dd_13d1(void)
{
    uint16_t n;
    uint8_t *p;

    sub_03dd_024e();
    n = DS8(0x1284);
    p = DSADDR((uint16_t)(n * 4 + 0x1286));
    P8(p) = sub_03dd_01fb();  p++;
    P8(p) = sub_03dd_01fb();  p++;
    P8(p) = sub_03dd_01fb();  p++;
    P8(p) = sub_03dd_01fb();  p++;
    DS8(0x1284) = (uint8_t)(n + 1);
}

/* 03dd:1434  op 0x3f: [a8][b8] for every 0x1286 entry with e[0]==a and e[1]==b sets e[2] = e[0] */
void sub_03dd_1434(void)
{
    int16_t n, a, b, i;
    uint8_t *p;

    sub_03dd_024e();
    n = DS8(0x1284);
    p = DSADDR((uint16_t)(n * 4 + 0x1286));
    a = sub_03dd_01fb();
    b = sub_03dd_01fb();
    for (i = 0; i < n; i++) {
        p -= 4;
        if (P8(p) == a && P8(p + 1) == b)
            P8(p + 2) = P8(p);
    }
}

/* 03dd:14a2  returns 1 if the hero's bounding box overlaps the rect (x1,y1)-(x2,y2) */
uint8_t sub_03dd_14a2(int16_t x1, int16_t y1, int16_t x2, int16_t y2)
{
    uint8_t *hero = DSADDR(0x11a);
    int16_t zone[4];
    int16_t box[4];

    zone[0] = x1;
    zone[1] = y1;
    zone[2] = x2;
    zone[3] = y2;
    box[0] = P16(hero) - P8(hero + 0x23);
    box[1] = P16(hero + 2) - P8(hero + 0x24);
    box[2] = P16(hero) + P8(hero + 0x23);
    box[3] = P16(hero + 2);
    return (uint8_t)sub_0e9b_000c((uint8_t *)zone, (uint8_t *)box);
}

/* 03dd:156c  op 0x27: waits while the hero is inside the zone kept in ctx (from ops 0x0e/0x28) */
void sub_03dd_156c(void)
{
    uint8_t *c = CTX();

    if (sub_03dd_14a2(P16(c + 0xe), P16(c + 0x10), P16(c + 0x12), P16(c + 0x14)) != 0)
        DSU16(0x328e) = 1;
    else
        sub_03dd_024e();
}

/* 03dd:1598  op 0x28: [x1/2][y1][x2/2][y2] waits until the hero enters the zone (op is re-run) */
void sub_03dd_1598(void)
{
    int16_t x1, y1, x2, y2;

    sub_03dd_024e();
    x1 = sub_03dd_0213();  P16(CTX() + 0xe) = x1;
    y1 = sub_03dd_01fb();  P16(CTX() + 0x10) = y1;
    x2 = sub_03dd_0213();  P16(CTX() + 0x12) = x2;
    y2 = sub_03dd_01fb();  P16(CTX() + 0x14) = y2;
    if (sub_03dd_14a2(x1, y1, x2, y2) == 0) {
        ip_add(-5);
        DSU16(0x328e) = 1;
    }
}

/* 03dd:1608  op 0x12: [b8] locks player controls: 0 = all (0xf), else [0xc0] |= DS:0x1aaa[b] */
void sub_03dd_1608(void)
{
    uint8_t b;

    sub_03dd_024e();
    b = sub_03dd_01fb();
    if (b == 0) {
        DSU16(0x3250) = 0;
        DSU16(0xc0) = 0xf;
    } else {
        DSU16(0xc0) |= DSU16(0x1aaa + b * 2);
    }
}

/* 03dd:1640  unlocks player controls; releases a locked hero animation */
void sub_03dd_1640(void)
{
    DSU16(0xc0) = 0;
    if (DS16(0x12a) == 2)
        DSU16(0x12a) = 0;
}

/* 03dd:1654  op 0x13: unlocks player controls */
void sub_03dd_1654(void)
{
    sub_03dd_024e();
    sub_03dd_1640();
}

/* 03dd:165d  text bank lookup: skips to the idx-th '*' entry and returns the text after its NUL */
uint8_t *sub_03dd_165d(int16_t idx, uint8_t *text)
{
    int16_t n = -1;
    uint8_t *p = text;

    while (n != idx) {
        if (*p++ == '*')
            n++;
    }
    while (*p++ != 0)
        ;
    return p;
}

/* 03dd:169e  starts displaying speech text textidx for actor in colour color */
void sub_03dd_169e(int16_t actor, int16_t textidx, int16_t color)
{
    DS16(0x3298) = actor;
    DS16(0x326c) = textidx;
    sub_0596_0405(sub_03dd_165d(textidx, DSPTR(0x325a)));
    DSU16(0x17bc) = 1;
    DS16(0x325e) = color;
}

/* 03dd:16d3  op 0x40: [c8] sets the own actor's text colour */
void sub_03dd_16d3(void)
{
    uint8_t c;

    sub_03dd_024e();
    c = sub_03dd_01fb();
    P8(DSADDR((uint16_t)(own_idx() * 0x46 + 0x11a)) + 0x2c) = c;
}

/* 03dd:16f8  actor says text textidx with talk animation anim (0xff = none); waits for the speech */
void sub_03dd_16f8(int16_t actor, int16_t textidx, int16_t anim)
{
    uint8_t *act = sub_03dd_00ac(actor);

    sub_03dd_169e(actor, textidx, P8(act + 0x2c));
    if (anim != 0xff) {
        DS16(0x9a) = P16(act + 0x1a);           /* saved anim, restored by 1a23 */
        DS16(0x32a8) = PS8(act + 0x22);
        DS16(0x329c) = P16(act + 0x1c);
        sub_0053_0cf2(act, anim);
        P16(act + 0x10) = 2;
    } else {
        DS16(0x9a) = -1;
    }
    P8(CTX() + 9) |= 0x40;
    DSU16(0x328e) = 1;
}

/* 03dd:1775  op 0x14 SAY: [text16][anim8] spoken by the own actor */
void sub_03dd_1775(void)
{
    int16_t text, anim;

    sub_03dd_024e();
    text = sub_03dd_022f();
    anim = sub_03dd_01fb();
    sub_03dd_16f8(own_idx(), text, anim);
}

/* 03dd:17a6  op 0x4e: requests the end of the game loop ([0x17ac]=1), yields */
void sub_03dd_17a6(void)
{
    sub_03dd_024e();
    DSU16(0x17ac) = 1;
    DSU16(0x328e) = 1;
}

/* 03dd:17b7  op 0x4d SAY: [actor8][text16][anim8] spoken by another actor */
void sub_03dd_17b7(void)
{
    int16_t actor, text, anim;

    sub_03dd_024e();
    actor = sub_03dd_01fb();
    text = sub_03dd_022f();
    anim = sub_03dd_01fb();
    sub_03dd_16f8(actor, text, anim);
}

/* 03dd:17ea  op 0x36: [text16][t8] shows the own actor's text without waiting; display time t */
void sub_03dd_17ea(void)
{
    int16_t text;
    uint16_t t;
    uint8_t *act;

    sub_03dd_024e();
    text = sub_03dd_022f();
    t = sub_03dd_01fb();
    act = sub_03dd_00ac(own_idx());
    sub_03dd_169e(own_idx(), text, P8(act + 0x2c));
    DSU16(0x328a) = t;
    DSU16(0x32aa) = t;
}

/* 03dd:1843  op 0x41: [v8] [0x9c] = v (speech bubble y override) */
void sub_03dd_1843(void)
{
    sub_03dd_024e();
    DSU16(0x9c) = sub_03dd_01fb();
}

/* 03dd:1851  op 0x26: [v8] [0xd4] = high nibble (only if [0xa0]==0), [0xd2] = low nibble + 1 */
void sub_03dd_1851(void)
{
    uint8_t v;

    sub_03dd_024e();
    v = sub_03dd_01fb();
    if (DS16(0xa0) == 0)
        DS8(0xd4) = v >> 4;
    DS8(0xd2) = (uint8_t)((v & 0xf) + 1);
}

/* 03dd:187e  wait flag 0x08: releases both scripts once the partner waits for this one too */
void sub_03dd_187e(void)
{
    uint8_t *c = CTX();
    uint16_t s = PU16(c + 0xa);
    uint8_t *o = ctx_n(s);

    if ((P8(o + 9) & 8) && PU16(o + 0xa) == DSU16(0x30a6)) {
        P8(o + 9) &= 0xf7;
        PU16(o + 0xa) = 0;
        c = CTX();
        P8(c + 9) &= 0xf7;
        PU16(c + 0xa) = 0;
    } else {
        DSU16(0x328e) = 1;
    }
}

/* 03dd:18d7  op 0x09 WAIT: [n8] sleeps for n ticks */
void sub_03dd_18d7(void)
{
    uint16_t n;
    uint8_t *c;

    sub_03dd_024e();
    n = sub_03dd_01fb();
    c = CTX();
    PU16(c + 0xa) = n;
    P8(c + 9) |= 2;
    DSU16(0x328e) = 1;
}

/* 03dd:18f5  op 0x50: [n8] sets game flag bit n */
void sub_03dd_18f5(void)
{
    sub_03dd_024e();
    sub_0c05_01ea(sub_03dd_01fb());
}

/* 03dd:1908  op 0x16: [a8][spr8] binds the script to actor a and loads its sprite (a==0: hero) */
void sub_03dd_1908(void)
{
    int16_t a, spr;

    sub_03dd_024e();
    a = sub_03dd_01fb();
    spr = sub_03dd_01fb();
    P8(CTX() + 8) = (uint8_t)a;
    if (a != 0) {
        sub_0053_0d68(a, DS16(0xce), sub_0053_0483(DS16(0x3268), spr));
    } else {
        DS8(0x146) = 0x19;                  /* hero text colour */
        DS8(0x13f) = 0;                     /* hero +0x25 */
    }
    DSU16(0x3268) = 0;
}

/* 03dd:1969  op 0x25: [0xee] = 1, [0xd2] = 0 */
void sub_03dd_1969(void)
{
    sub_03dd_024e();
    DSU16(0xee) = 1;
    DS8(0xd2) = 0;
}

/* 03dd:1979  wait flag 0x04: done when the own actor shows the last frame of its animation */
void sub_03dd_1979(void)
{
    uint8_t *act = sub_03dd_00ac(own_idx());

    if (PU16(act + 0x1c) == (uint16_t)(PU16(act + 0x20) - 1))
        P8(CTX() + 9) &= 0xfb;
    else
        DSU16(0x328e) = 1;
}

/* 03dd:19b8  wait flag 0x01: done when the own actor has stopped walking */
void sub_03dd_19b8(void)
{
    uint8_t *act = sub_03dd_00ac(own_idx());

    if ((P16(act + 0x30) & 3) == 0 || P16(act + 0x2a) == 0)
        P8(CTX() + 9) &= 0xfe;
    else
        DSU16(0x328e) = 1;
}

/* 03dd:19fb  wait flag 0x02: counts the timer down; done at 0 */
void sub_03dd_19fb(void)
{
    uint8_t *c = CTX();

    if (PU16(c + 0xa) > 0)
        PU16(c + 0xa)--;
    c = CTX();
    if (PU16(c + 0xa) == 0)
        P8(c + 9) &= 0xfd;
    else
        DSU16(0x328e) = 1;
}

/* 03dd:1a23  wait flag 0x40: when the speech ends, restores the speaker's saved animation */
void sub_03dd_1a23(void)
{
    uint8_t *act;

    if (DS16(0x17bc) == 0) {
        P8(CTX() + 9) &= 0xbf;
        if (DS16(0x9a) != -1) {
            act = sub_03dd_00ac(DS16(0x3298));
            sub_0053_0cf2(act, DS16(0x9a));
            P8(act + 0x22) = DS8(0x32a8);
            P16(act + 0x1c) = DS16(0x329c);
            DS16(0x9a) = -1;
        }
    } else {
        DSU16(0x328e) = 1;
    }
}

/* 03dd:1a7f  wait flag 0x10: when the choice menu closes, jumps via the chosen entry's rel16 */
void sub_03dd_1a7f(void)
{
    uint8_t *e;

    if (DS16(0x3240) == 0) {
        P8(CTX() + 9) &= 0xef;
        e = DSADDR((uint16_t)(DSU16(0x3274) * 10 + 0x306a));
        FP_SET(CTX() + 4, FP(e + 6));
        sub_03dd_0257();
    } else {
        DSU16(0x328e) = 1;
    }
}

/* 03dd:1ac0  runs script n for one tick: wait handlers, then opcodes until one yields */
void sub_03dd_1ac0(uint16_t n)
{
    uint8_t op;

    DSU16(0x30a6) = n;
    DSPTR_SET(0x305a, ctx_n(n));
    DSU16(0x328e) = 0;
    if (P8(CTX() + 9) & 0x20)               /* stopped */
        return;
    if (P8(CTX() + 9) & 0x08)
        sub_03dd_187e();
    if (P8(CTX() + 9) & 0x02)
        sub_03dd_19fb();
    if (P8(CTX() + 9) & 0x01)
        sub_03dd_19b8();
    if (P8(CTX() + 9) & 0x04)
        sub_03dd_1979();
    if (P8(CTX() + 9) & 0x10)
        sub_03dd_1a7f();
    if (P8(CTX() + 9) & 0x40)
        sub_03dd_1a23();

    while (DS16(0x328e) == 0) {
        op = P8(FP(CTX() + 4));
        if (op > 0x58)                      /* TODO(port): table 0x1946 has 0x59 entries; beyond is data */
            plat_fatal("script %u: bad opcode 0x%02x", n, op);
        DSFN(void (*)(void), (uint16_t)(op * 4 + 0x1946))();
    }
}

/* 03dd:1b6d  runs one tick of every room script except script 0 */
void sub_03dd_1b6d(void)
{
    uint16_t i;

    for (i = 1; i < DSU16(0xd8); i++)
        sub_03dd_1ac0(i);
}
