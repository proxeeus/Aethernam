/* G5 - segments 0714 (actor collision / walk targets), 0791 (sound, palette, particle FX, shutdown),
 *      0a21 (polygon morph builder).  All functions are far (retf); none are near/static.
 * Common arg kinds: "rect" = far ptr to 4 x int16 {x1,y1,x2,y2}; "actor" = far ptr into the
 * actor table DSADDR(0x11a + i*0x46).  Collision codes returned: (kind<<8)|index, 0 = free. */

/* ---- segment 0714 ---- */
int16_t sub_0714_0004(int16_t x1, int16_t y1, int16_t x2, int16_t y2);
/* returns bit0 = (x1==x2), bit1 = (y1==y2); 3 = actor already at its target */
void sub_0714_002e(uint8_t *px, uint8_t *py, int16_t halfw, int16_t height);
/* clamps a walk target (*px,*py are int16 lvalues) to the screen (halfw+1..0x13e-halfw); if the actor box at the target touches an exit zone of any dir 1..4 (sub_0714_05e4) returns leaving y alone, else pushes *py below the floor horizon DS8(0xa68+x) at both box edges (+height+2) and clamps *py <= 0xab */
void sub_0714_0160(uint8_t *px, uint8_t *py, int16_t halfw, int16_t height);
/* same clamp as 0714_002e but without the exit-zone test: x into screen, y below floor horizon at x-halfw/x+halfw, y <= 0xab; writes back *px,*py */
void sub_0714_0210(uint8_t *actor);
/* if actor walk mode (flags+0x30 & 3) is set and no target is saved (flag 0x4 clear): saves target +0x32/+0x34 into +0x38/+0x3a and sets flag 0x4 */
void sub_0714_023e(int16_t actor_idx, int16_t tx, int16_t ty);
/* sets actor actor_idx's walk target: clamps it (0714_002e unless +0x25==8), stops if already there, saves old target, stores tx,ty in +0x32/+0x34, sets axis order flags&3 (both differ: random 1/2 if unset else toggled; one equal: result^3), sets anim 4 (walk) via sub_0053_0e52 and facing via sub_0053_0ed4 */
int16_t sub_0714_031f(uint8_t *rect, int16_t dir);
/* screen-border / floor-horizon test for a moving actor box: 0x100 if blocked by horizon or side border in direction dir (1=up,2=right,4=left), 0x200 if y2 > 0xab (bottom edge; overrides), else 0 */
int16_t sub_0714_03d2(int16_t self_idx, uint8_t *rect);
/* returns 0x600|i for the first other actor i (0..9, != self_idx) in the current room DS16(0xce) with hp!=0 and state!=8 whose box overlaps rect, else 0 (box built in DS 0x3014) */
int16_t sub_0714_04a0(uint8_t *rect);
/* returns 0x300|i for the first room obstacle rectangle i (DS 0x1286 table, count DS8(0x1284)) that overlaps rect, else 0 */
int16_t sub_0714_0559(uint8_t *rect);
/* returns 0x500|i for the first free-standing object i (DS 0xa18 table, count DSU16(0xf0), only if location word DS16(0xfbc+id*2)==0) whose 16x16 box around (x,y) overlaps rect, else 0 */
int16_t sub_0714_05e4(uint8_t *rect, int16_t dir);
/* returns 0x400|i for the first exit zone i (DS 0x752 6-byte table, count DS16(0xa8)) whose type byte == dir and that the box {x1,y1,x2} reaches in that direction, else 0 */
int16_t sub_0714_06c7(int16_t actor_idx, int16_t x, int16_t y, int16_t halfw, int16_t height, int16_t dir);
/* full collision test of actor box (x-halfw,y-height,x+halfw,y) moving in dir: border/horizon (then exit zone overrides), else obstacles, else other actors; returns collision code or 0 */
int16_t sub_0714_075b(uint8_t *actor, int16_t actor_idx, int16_t code);
/* records collision code in actor+0x25 (kind) / +0x26 (index); if player (idx 0) hit an exit (kind 4) triggers the room change sub_0053_11d5(exit link byte); if nothing triggered sets anim 0 and stops the actor (sub_0053_1074); returns trigger result (0 = stopped) */

/* ---- segment 0791 ---- */
void sub_0791_0005(int16_t vol);
/* lowers the music volume: if vol <= DS16(0xe8) stores it and sends it to the int F0 music driver (vol 0 = stop cmd 0x40, else set-volume cmd 0x2000) */
void sub_0791_0039(void);
/* stops the music (driver cmd 0x40 via sub_0e9f_0013(0,0,0x40)) */
void sub_0791_0048(int16_t n);
/* loads entry n of MUS.PAK (name DSPTR(0x18b0)) into the sound-effect bank buffer DSPTR(0x17c4) and registers it with the driver (sub_0e9f_017e) */
void sub_0791_0078(int16_t n);
/* switches music: stops, loads MUS.PAK entry n into DSPTR(0x17c0) (on failure calls sub_0791_0665 = quit), starts it, re-applies volume, plays; stores n in DS16(0xd6) */
void sub_0791_00e5(int16_t n);
/* plays sound effect n: ducks music to vol 50 (restores with cmd 0x8000), stops current effect, plays n (flags 0x80) if effects enabled DS16(0xea) > 0 */
void sub_0791_0132(int16_t n);
/* like sub_0791_00e5 but always plays effect n with flags 0xa0 (ignores DS16(0xea)) */
void sub_0791_0178(void);
/* stops the current sound effect (sub_0e9f_006f(0,0,0x40)) */
void sub_0791_0187(int16_t level);
/* builds a red-tinted/desaturated copy of the base palette DSPTR(0x304c) with strength level (0..16) via sub_0dc5_0448 and loads it into the DAC (sub_0791_067f) */
void sub_0791_01b7(uint8_t *part);
/* (re)spawns one 8-byte particle around the origin DS16(0x3036),DS16(0x3038): x=ox-5+rnd(11), y=oy-3+rnd(6), vx(+6)=rnd(3)-1, vy(+4)=-(rnd(7)+1) */
void sub_0791_020d(int16_t x, int16_t y, int16_t count, int16_t duration);
/* starts a particle burst: stores origin x,y and count (DS 0x3036/0x3038/0x303e), spawns count particles in DS 0x30ae, sets timer DS16(0x3260)=duration */
void sub_0791_025b(void);
/* per-frame particle update: moves each particle with gravity, draws a streak line (col 0x37) or pixel; a particle falling below y~0x8c..0x95 is stamped as a pixel into the background buffer DSPTR(0x3056) and respawned */
void sub_0791_034e(int16_t x1, int16_t y1, int16_t x2, int16_t y2);
/* draws one jagged lightning bolt from (x1,y1) to (x2,y2): 10 line segments to points (x1+(x2-x1)*i/10-2+rnd&3, ...), colour 0xc0+(rnd&15) or 0x36 with 1/10 chance; uses LFSR rng sub_0f21_0004 */
void sub_0791_040c(void);
/* draws all queued lightning bolts (DS 0x30ae entries {x1,y1,y2,x2}, count DS16(0x1740)) then clears the queue; bounces DS16(0x173a) between 0xc1..0xce by DS16(0x173c) */
void sub_0791_0506(int16_t x1, int16_t y1, int16_t x2, int16_t y2);
/* sprinkles (x2-x1)/2+(y2-y1) random colour-1 pixels (half of them as '+' shapes) in the rectangle; nothing if empty */
void sub_0791_05d9(void);
/* clears the one-shot flag DS16(0x32a0) and sprinkles the rectangle stored at DS 0x3020..0x3026 (sub_0791_0506) */
int32_t sub_0791_05f7(void);
/* returns the size of the largest free DOS memory block in bytes (farcoreleft-like: sub_0f27_0004(-1L)) */
void sub_0791_0604(void);
/* per-frame player hit-point handling: if hp DS16(0x144) > 0 regenerates +1 every 512 ticks of DS32(0x1fdc) up to 99, else triggers death (sub_03dd_1147(0)) */
void sub_0791_0638(uint8_t *actor, int16_t damage);
/* subtracts damage from actor hit points (+0x2a), clamped at 0 */
void sub_0791_0665(void);
/* quits the game: restores timer (int 8) and keyboard (int 9) handlers, shuts the music driver, restores video/frees screen, calls exit() - never returns */
void sub_0791_067f(uint8_t *pal);
/* loads a 256-colour 8-bit RGB palette (768 bytes at pal) into the VGA DAC: copies, converts to 6-bit, writes colours 0-127 and 128-255 each after a vertical retrace */

/* ---- segment 0a21 ---- */
void sub_0a21_0005(int16_t frameA, int16_t xA, int16_t yA, uint8_t *shapeA, int16_t frameB, int16_t xB, int16_t yB, uint8_t *shapeB, uint8_t *out);
/* builds a polygon morph list between frame frameA of vector shape shapeA placed at (xA,yA) and frame frameB of shapeB at (xB,yB): pairs up polygons and vertices (padding the smaller one) and writes interleaved (A point, B point) records into out; out+9 gets the polygon count */
