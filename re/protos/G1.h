/* G1 - code segment 0053 (main, level/room flow, resource cache, actors, input)
 * All functions are far (retf); none is near/static.
 * "actor" = far ptr to a 0x46-byte actor struct, table at DS:0x11a (10 entries, [0]=player).
 * Pointer params that point to int16 values (sub_0016, sub_13cf, sub_1591, sub_1678)
 * are uint8_t * per convention; access with P16(p).
 */
void     sub_0053_0000(void); /* if farcoreleft() < 20000 purge the resource cache (sub_0053_028e) */
void     sub_0053_0016(uint8_t *a, uint8_t *b); /* swaps the two int16 values *a and *b */
int16_t  sub_0053_003c(int16_t kind, int16_t idx); /* finds resource-cache slot (DS:0x928, 20x6) with (s8)kind,(s8)idx; returns slot index or -1 */
int16_t  sub_0053_007e(uint8_t *res); /* 1 if resource ptr is used (+0x16) by a living actor in the current room (on world map [0xa0]==-1: also within 800 of camera 0x1fc2/0x1fc4), else 0 */
void     sub_0053_0112(void); /* farfree every kind-0 (A??.PAK) cache slot and clear its ptr, unconditionally */
void     sub_0053_0168(uint8_t *res); /* clears the sprite ptr (+0x16) of actors 1..9 that point to res */
void     sub_0053_01e8(uint8_t *actor); /* frees the actor's sprite resource from the cache if kind 0 and no other actor uses it */
void     sub_0053_028e(void); /* purges cache: frees all kind-0 slots whose resource is not in use (sub_0053_007e), detaching actors */
void     sub_0053_030c(int16_t code); /* fatal error: prints "NO SLOT FREE !!"(0)/"NO MEMORY FREE !!"(1) on screen at 100,100, waits for a key, shuts down/exits */
int16_t  sub_0053_036c(void); /* returns index of first free (NULL ptr) resource-cache slot, or -1 */
uint8_t *sub_0053_03a5(int16_t kind); /* returns preloaded resource block for kind 1..7 (1:0x3256 2:0x3286 3:0x17b4 6:0x3262 7:0x3062), else NULL */
uint8_t *sub_0053_0413(uint8_t *pakname, int16_t idx); /* loads entry idx of a .PAK file (sub_0c66_0003), purging cache/retrying on failure; fatal "NO MEMORY" if still NULL */
uint8_t *sub_0053_0483(int16_t kind, int16_t idx); /* gets/loads a cached resource: kind 0 = entry idx of A??.PAK, other kinds = preloaded blocks; returns data ptr */
uint8_t *sub_0053_052f(int16_t idx, uint8_t *base); /* returns base + ((int32*)base)[idx]  (resource directory lookup) */
uint8_t *sub_0053_0564(int16_t i, int16_t j, uint8_t *base); /* two-level directory lookup: b = base + dir[i]; returns b + ((int32*)b)[j] */
void     sub_0053_05cd(void); /* pause: if 'P' (scancode 0x19) pressed, wait release then wait for any key/input/button (busy-wait) */
int16_t  sub_0053_05f6(int16_t n); /* random number 0..n-1 (rand()%n), 0 if n < 2 */
void     sub_0053_0612(void); /* builds current room's obstacle rectangles (DS:0x1286, count byte 0x1284) from room object list in level data */
void     sub_0053_075b(uint8_t key, uint8_t id); /* inserts (y-key,id) pair into sorted depth list DSPTR(0x3270); tracks min y in 0x1778 */
void     sub_0053_07ea(void); /* resets depth list DSPTR(0x3270) with the room's static objects (key = object y, id 0x10) */
void     sub_0053_086b(void); /* clears game flag arrays DS:0x11bc[100] and DS:0xfbc[256]; sets word 0x11bc = -1 */
void     sub_0053_0895(uint8_t *buf, int32_t len); /* decrypts buffer in place: buf[i] -= (uint8)(0x5f*(i+1)), i unsigned 32-bit */
void     sub_0053_08f0(uint8_t *dst, int16_t idx, int32_t size); /* loads text block idx from T.CC4 (sub_03dd_0008) into dst and decrypts it */
void     sub_0053_0925(int16_t n); /* writes 2-digit level number n into file names A00.PAK, D00.PAK, R00.CC4 */
void     sub_0053_097e(void); /* frees level data DSPTR(0x3286) and the kind-3 block 0x17b4 (sub_0596_0033) */
uint8_t *sub_0053_0994(uint8_t *base); /* returns sub-block 4 (palette) of a level data block: sub_0053_052f(4, base) */
void     sub_0053_09a7(int16_t level); /* loads level: sets 0xa0/0xa2, file names, clip rect, chapter text, D??.PAK data, palette */
void     sub_0053_0a6d(void); /* per-level-session init: allocs back buffer/depth list/script/text buffers, loads RESID.PAK#1, inits player actor */
void     sub_0053_0b7f(uint8_t *p); /* farfree(p) (then zeroes its local copy only) */
void     sub_0053_0b9b(void); /* frees level-session buffers 0x3056,0x177e,0x3270,0x325a,0x3282 (pointers are NOT nulled) */
void     sub_0053_0be7(void); /* game init: loads RESID.PAK#0 (font etc.), allocs palettes, loads UI text, clears flags, sets player defaults */
void     sub_0053_0cc4(void); /* copies floor polyline point count (0x1832) to 0xda and rasterizes it into per-column floor-y table DS:0xa68 */
int16_t  sub_0053_0cdd(int16_t v); /* abs(v) */
void     sub_0053_0cf2(uint8_t *actor, int16_t anim); /* starts animation anim on actor: frame 0, frame count from sprite data, +0x22=0xff; records player anim in 0x326a */
void     sub_0053_0d68(int16_t idx, int16_t room, uint8_t *sprites); /* initializes actor idx (standing, facing up, default size/bounds) in room with sprite set */
void     sub_0053_0e27(uint8_t *actor, int16_t dir); /* sets actor direction (1 up,2 right,3 down,4 left) if changed, nonzero and not locked; flags anim change */
void     sub_0053_0e52(uint8_t *actor, int16_t state); /* sets actor anim state (0 stand,4 walk,8 action) if changed and not locked; flags anim change */
int16_t  sub_0053_0e77(int16_t x1, int16_t y1, int16_t x2, int16_t y2); /* returns direction 1..4 from (x1,y1) towards (x2,y2) (horizontal if |dx|/2 > |dy|) */
void     sub_0053_0ed4(uint8_t *actor); /* turns actor towards its walk target (+0x32,+0x34) along the axis selected by flags&3 */
void     sub_0053_0f89(uint8_t *actor); /* stops actor: clears walk flags (+0x30) and sets stand state */
void     sub_0053_0fa3(void); /* player control from joystick/keys: turn, walk, action anim (8) */
void     sub_0053_1074(uint8_t *actor); /* advances actor animation (applies pending state/dir anim change, steps frames, ends action anim) */
void     sub_0053_1192(void); /* animates all living actors of the current room (sub_0053_1074) */
int16_t  sub_0053_11d5(int16_t room); /* player touched door leading to room: requests room change (-1 = leave level); for side doors starts walk-off; returns 1 (0 if room==-1) */
void     sub_0053_1298(uint8_t *actor); /* places actor at the door of the new room leading back to its old room; plays room scroll transition */
void     sub_0053_13cf(int16_t door, uint8_t *x1, uint8_t *y1, uint8_t *x2, uint8_t *y2); /* gets door door's rectangle (x from door table 0x752, y from floor table), y1<=y2 */
void     sub_0053_1479(void); /* performs pending room change (0xd0 -> 0xce): room script, obstacles, visit counter, player placement */
void     sub_0053_1568(void); /* performs pending level change (0xa2): frees current level, loads new one, changes room */
void     sub_0053_1591(uint8_t *actor, uint8_t *px, uint8_t *py); /* clamps new position (*px,*py) so the actor does not overshoot its walk target in its direction */
void     sub_0053_1678(int16_t dir, uint8_t *pdx, uint8_t *pdy); /* adjusts step (*pdx,*pdy): vertical speed bonus 0xc4, room drift 0xb4/0xb6 by direction */
int16_t  sub_0053_16fa(int16_t idx); /* moves walking actor idx one animation step with collision handling; returns 1 if moved, 0 if not walking/blocked */
int16_t  sub_0053_1831(int16_t x, int16_t y); /* floor polyline: y of first point with px > x and py >= y, scanning forward (>=0xab -> 0xa0) */
int16_t  sub_0053_188d(int16_t x, int16_t y); /* floor polyline: y of first point with px < x and py >= y, scanning backward (>=0xab -> 0xa0) */
void     sub_0053_18f4(int16_t idx, uint8_t *actor); /* blocked by floor edge: re-targets actor walk around the floor contour */
void     sub_0053_19c1(int16_t idx, uint8_t *actor); /* blocked by obstacle: re-targets actor walk randomly around obstacle bbox 0x3014..0x301a */
void     sub_0053_1a64(int16_t idx, uint8_t *actor); /* chooses detour/stop for a blocked actor according to collision type +0x25 */
void     sub_0053_1adf(uint8_t *actor, int16_t idx, int16_t moved); /* auto-walk logic after a step: detour if blocked, arrival/next waypoint, axis switch */
void     sub_0053_1b98(void); /* moves all living actors of the room; sets enemy-present flags 0xae/0xf2 */
void     sub_0053_1c18(int16_t idx, int16_t x, int16_t y); /* sets actor idx position and clears +0x28 and walk flags */
void     sub_0053_1c4e(void); /* inserts all living actors of the room into the depth list (key = y, id = actor index) */
void     sub_0053_1c94(int16_t lang); /* selects language: 0x10a = lang, first char of "T.CC4" = "TESDI"[lang] */
int16_t  sub_0053_1cb8(void); /* returns 1 if the player may act (no menu, cutscene, input lock, auto-walk); may reset dialogue page 0x328a */
void     sub_0053_1cf8(void); /* handles keyboard: key dispatch, pause 'P', music off ';'(AZERTY 'M'), Enter closes menu/dialogue (busy-waits on release) */
void     sub_0053_1daa(void); /* reads input bits 0x2d4a into 0x327c, direction 0x3250 and fire 0x3030/0x3012 (masked by 0xc0 / menu) */
void     sub_0053_1deb(void); /* menu cursor: up/down moves selection 0x3274 within 0..0x303a-1 */
void     sub_0053_1e2e(void); /* frame limiter: adapts number of extra vsync waits 0x1736 from measured rate 0x2840 vs target 0xac, then waits */
void     sub_0053_1e8e(void); /* one game-logic+render tick of a room (input, actors, depth list, draw, UI, present) */
void     sub_0053_1f21(void); /* one main-loop frame: level/room changes, input, tick, dialogue, save/load request */
void     sub_0053_1f74(int16_t level); /* plays one level (chapter) until 0xa2 == -1, then frees everything */
void     sub_0053_2014(void); /* main(): hardware/driver init, launcher params, alloc, music, RNG seed, game init, intro level, world loop; never returns (sub_0791_0665 exits) */
