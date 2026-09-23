/* G7 prototypes - segments 0936 (actors / combat), 0c66 (PAK loader), 0c9b (3D terrain engine, asm)
 * All functions are FAR (retf).  "actor" = 0x46-byte record in the DGROUP actor table at 0x11a
 * (slot 0 = player, slots 1..9 = monsters).  Field offsets: see re/notes/G7.md.
 */

/* ---- segment 0936 : outdoor monsters, player laser, hit effects ---------------------------- */
int16_t sub_0936_0005(int16_t path_no);
/* returns word index (into s16 table DS:0x1bbc) of the start of monster path #path_no (paths are separated by -1 words) */
void sub_0936_0053(uint8_t *actor);
/* advance actor to next waypoint of its path (+0x44 += 2, wraps to +0x42+4 at -1); waypoint (0,0) = chase player (target=player pos, +0x14=1); stores target in +0x32/+0x34 */
int16_t sub_0936_00d1(void);
/* returns first free monster slot 1..9 (actor+0x2a == 0), 0 if none */
void sub_0936_0106(int16_t path_no, int16_t slot);
/* spawn monster in slot (-1 = first free) from path table entry path_no: init via sub_0053_0d68, flags/speed/hp/start pos, first waypoint, anim 3; no-op if slot busy */
uint8_t sub_0936_0225(int16_t x0, int16_t y0, int16_t x1, int16_t y1);
/* 8-way heading byte (0x00,0x20..0xe0) of vector (x1-x0, y1-y0) */
void sub_0936_029e(uint8_t *actor);
/* per-frame monster AI: turn heading toward waypoint, move by sin table >> speed, next waypoint when close, random altitude for flyers, attack player when adjacent, update nearest-enemy distance (0xf2/0xae) */
void sub_0936_04af(void);
/* add every active monster (slots 1..9) to the 3D object list via sub_0c9b_0861(x, y, slot+0x20) */
void sub_0936_04f4(void);
/* per-frame update of all monsters: effect anim, reset proximity alarm, AI (sub_0936_029e), sprite anim (sub_0053_1074), clear visible flag +0x1e */
void sub_0936_056e(uint8_t *actor, int16_t sx, int16_t sy, int16_t scale, int16_t ground_y, int16_t size);
/* draw a monster sprite at screen sx,sy with scale 0..15 (15 = unscaled), shadow line at ground_y; picks facing anim, lazily loads sprite (kills actor if load fails) */
void sub_0936_0855(int16_t sx, int16_t sy, int16_t size);
/* draw current hit/explosion effect frame (DS:0x32da) from effect bank DSPTR(0x3262) scaled to size */
void sub_0936_0876(int16_t sx, int16_t depth, int16_t sy, int16_t id);
/* draw 3D-projected monster #(id&0x1f): compute scale from depth, lift by altitude, call sub_0936_056e / sub_0936_0855, mark visible (+0x1e=1) */
void sub_0936_0948(void);
/* mark monsters that are not visible or too small (+4 <= 0x23) as unpickable (+0x25 = 8) */
void sub_0936_0985(void);
/* clear the unpickable mark (+0x25 = 0) of all monsters */
void sub_0936_09b1(void);
/* fire laser: pick monster under crosshair (sub_0714_03d2), set beam target 0x32d2..d6 and 4-step beam from both bottom corners, sound 0x2a */
void sub_0936_0a73(void);
/* laser arrival: hit-test box around target; hit => aggro, hp-1, death states -1/-3 + effect; miss => spark; sounds */
void sub_0936_0b5f(void);
/* per-frame laser logic: start shot on fire bit (0x2d4a&0x80) when idle, advance beam, resolve on last step */
void sub_0936_0ba4(int16_t x0, int16_t y0, int16_t x1, int16_t y1);
/* draw a 3-pixel-wide laser beam line (color 0x4d centre, 0x37 at x-1/x+1) */
void sub_0936_0bec(void);
/* proximity alarm blink counter (0xf4) driven by 0xae / 0xf2 */
void sub_0936_0c14(void);
/* per-frame: update alarm counter and draw both laser beams while a shot is in flight */
void sub_0936_0c5e(int16_t id, int16_t type);
/* start effect animation on actor id: type 0 = hit spark frames 0..7, 1 = explosion 8..0x17, 2 = crash 0x18..0x1d */
void sub_0936_0c9f(void);
/* advance effect frame; at end clear effect and step death state of actor (-1->0 removed, -2->-1 + expl., -3->-2 + crash) */
void sub_0936_0d18(void);
/* encounter check: nearest non-chasing live monster (slots 1..8) with |dx|+|dy| < 100 triggers scene 0x63 (0xa2) with id 0x1fd6*8+slot in 0x329e; clears 0x327a */
void sub_0936_0dc0(void);
/* save outdoor state: purge (sub_0053_028e), copy monsters 0x160->0x41c (0x276 B) and resource cache 0x928->0x9a0 (0x78 B), sub_0053_0112 */
void sub_0936_0df4(void);
/* restore outdoor state saved by sub_0936_0dc0 and reload (sub_07fe_0231) */
void sub_0936_0e23(void);
/* sub_0c05_056b() then --DS16(0x11c2) */
void sub_0936_0e2d(void);
/* draw proximity-alarm lamp on screen buffer (erase at 0xf4==-1, sprite 0x2b of UI bank at 0xf4==-2) and refresh status bar when player hp (0x144) changed */

/* ---- segment 0c66 : PAK resource loader ------------------------------------------------------ */
uint8_t *sub_0c66_0003(uint8_t *name, int16_t index);
/* load entry #index of "<name>.PAK": reloc table + 12-byte header, stored (0) or imploded (1, sub_0f48_0314); allocates block, relocates internal offset tables into far pointers; NULL on alloc failure */

/* ---- segment 0c9b : 3D outdoor engine (hand-written asm, self-modifying) -------------------- */
void sub_0c9b_0022(uint8_t *src_pal, uint8_t *dst_pal, uint16_t level);
/* scale palette: dst[i] = src[i]*level>>4 for 0x240 bytes (scene 0xa0 == -1/0x63) else 0x2a0 bytes */
void sub_0c9b_005c(uint8_t *src_rgb, uint8_t *dst_pal);
/* build 4 x 16-step RGB gradients between 5 consecutive src triples into dst_pal+0x240 (colours 192..255, sky/ground) */
int16_t sub_0c9b_00d8(int16_t a, int16_t b, int16_t num, int16_t den);
/* linear interpolation a + (b-a)*num/den (32-bit intermediate) */
void sub_0c9b_00fe(uint8_t *hmap, uint8_t *tile);
/* build terrain tile edge profiles from 4 corner heights hmap[0],[2],[0x40],[0x42]: tile+0x334 top/bottom/diagonal 65-sample lines, slope class word at tile+0x32c. NOTE initial AL = (uint8_t)(hmap - DSPTR(0x34a2) - 1) (caller's ax) */
void sub_0c9b_01cd(uint8_t *verts, uint8_t *edges);
/* for DS16(0x2284) 8-byte vertices (x,y,z,attr) set z = -(terrain height at x,y) interpolated from tile edge lines (edges = tile+0x334) */
void sub_0c9b_024e(void);
/* recompute view flags 0x1fcb (octant|x/y half-cell|quadrant); if changed store in 0x1fcc and rebuild tiles (sub_0a86_0517) */
void sub_0c9b_0293(void);
/* horizon row 0x34b8 from camera height 0x1fc6 */
void sub_0c9b_02b6(void);
/* 0x34b6 = 1 if all 4 tiles have slope class 0x303 (flat), else 0 */
int16_t sub_0c9b_0323(int16_t x, int16_t y, int16_t tile);
/* terrain height at world x,y inside terrain tile #tile (0..3), triangle interpolation of corners; 0 if x<=0 or y<=0 */
void sub_0c9b_03e5(void);
/* camera height 0x1fc6 = height under player (tile 0) + eye offset 0x1fc8 */
void sub_0c9b_0404(void);
/* read direction input 0x2d4a: turn heading 0x1fcd by 0x1fce, set move dir 0x1fca, update sin/cos, then sub_0c9b_0464 */
void sub_0c9b_0464(void);
/* move player along heading (cos/sin >> 0x1fd0, sign 0x1fca), then view flags, camera height, horizon */
void sub_0c9b_04a1(void);
/* recompute sin/cos (0x1fbc/0x1fbe) from heading 0x1fcd and view flags (sub_0c9b_024e) */
void sub_0c9b_04cc(void);
/* recompute sin/cos (0x1fbc/0x1fbe) from heading 0x1fcd only */
void sub_0c9b_04f4(uint8_t *src, uint8_t *dst, int16_t camx, int16_t camy, int16_t mode);
/* transform DS16(0x2284) vertices to camera space (translate by cam, rotate by cos/sin >>14, z += 0x1fc6); mode 0 skip if x>=y, 1 skip if x<y, 2 keep all (skips decrement 0x2284); appends copy of dst[0] (close polygon) */
void sub_0c9b_05b2(uint8_t *def, uint8_t *dst);
/* expand tile object definition (count byte, 4-byte (x,y,attr) entries) into 8-byte vertices (x*8,y*8,y*8,attr) in one of 4 orders chosen by 0x1fcb bits 6/7; sets 0x2284 */
void sub_0c9b_06a4(void);
/* frustum cull in place on DSPTR(0x34a6): keep 10 < depth < 600, |x| < 256, |x| < depth; 0x2284 = kept count */
void sub_0c9b_06fe(void);
/* perspective project DSPTR(0x34a6) in place: sx = x*256/(d+10)+160, sy = z*256/(d+10)+100 */
void sub_0c9b_0744(uint8_t *src);
/* expand polygon byte pairs (count at src[-2]) to 8-byte vertices (x*8,y*8,y*8,y*8) in DSPTR(0x34a6); sets 0x2284 */
void sub_0c9b_077c(void);
/* clip closed polygon DSPTR(0x34ae) against near plane depth>0 into DSPTR(0x34a6); 0x2284 = new count */
void sub_0c9b_083f(void);
/* compact DSPTR(0x34a6) vertices (sx,d,sy,attr) to 2D point list (sx,sy) for polygon fill */
void sub_0c9b_0861(int16_t x, int16_t y, int16_t id);
/* append object (x,y,y,attr) to 3D object list DSPTR(0x349e)[DS16(0x3496)++]; attr = id | (tile|0x80 if upper triangle)<<8; dropped if outside the 4 tiles */
void sub_0c9b_08e8(void);
/* place all objects on terrain (z=-height), transform (mode 2), cull, project, copy back to object list, update 0x3496 */
void sub_0c9b_0974(uint8_t *entry, uint8_t *pos, int16_t n);
/* insert 8-byte entry at pos, shifting the n following entries up by 8 bytes */
void sub_0c9b_09ae(int16_t tile, int16_t side);
/* depth-sort insert of objects belonging to tile (attr>>8 &0x7f) and side (bit15, 2 = any) into polygon list DSPTR(0x34a6) (count 0x2284) */
void sub_0c9b_0a15(int16_t horizon);
/* 386: paint sky into draw buffer rows 0..horizon-1: max(0,horizon-128) rows of colour 0xc0, then 2-row bands of colours (512-min(horizon,128))/2 .. 0xff (dword stores) */
void sub_0c9b_0a66(int16_t horizon);
/* 286 version of sub_0c9b_0a15 (word stores) */
void sub_0c9b_0ae4(int16_t color, uint16_t src, int16_t x0, int16_t y0);
/* REGISTER ARGS (ax=color, ds:si=src DS offset, bx=x0, patched imm cs:0xaf8=y0): plot 6 columns (stride 0x20 bytes / 64 px) x 4 star points (x0+b0, b1+y0) with colour; asm advances si by 8 per call, C caller passes src+8*layer */
void sub_0c9b_0b20(void);
/* draw 4 layers of distant stars/skyline (colours 0x9f,0x9d,0x9b,0x99) from DS:0x23c0 scrolled by heading, relative to horizon */
void sub_0c9b_0b85(int16_t y, int16_t nrows, int16_t color);
/* 386: fill nrows full rows of draw buffer from row y with color */
void sub_0c9b_0bba(int16_t y, int16_t nrows, int16_t color);
/* 286 version of sub_0c9b_0b85 */
void sub_0c9b_0c0b(uint8_t *src, uint8_t *dst, int16_t y);
/* 386: copy rows y..171 (offsets SS row table, ss:[0x158]=row 172) from src buffer to dst (screen), ++DS16(0x2d66); pointer offsets replaced by row offset */
void sub_0c9b_0c40(uint8_t *src, uint8_t *dst, int16_t y);
/* 286 version of sub_0c9b_0c0b */
void sub_0c9b_0c70(uint8_t *src, uint8_t *dst, int16_t y_unused);
/* blit 320x172 view scaled 3/4 (240x129) to dst at (40,21), ++DS16(0x2d66); 3rd arg pushed by caller but unused */
void sub_0c9b_0ca6(uint8_t *src, uint8_t *dst, int16_t y_unused);
/* blit 320x172 view scaled 1/2 (160x86) to dst at (80,43), ++DS16(0x2d66); 3rd arg pushed by caller but unused */
int16_t sub_0c9b_0e15(void);
/* CPU test: 1 if FLAGS bits 12-14 are writable (386+), else 0; port: return 1 */
