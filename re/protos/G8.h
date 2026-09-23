/* G8 - segment 0a86: exterior 3D view (island landscape) - prototypes
 * All functions are far (retf) and called with lcall or push cs/call.
 * Callers never use a return value unless stated. */

void sub_0a86_0000(void);
/* advance day/night cycle one step: interpolate 5 sky RGB colors between phase tables 0x204f[phase]/[phase+1] (step 0x1fd4 of count), write sky gradient (exterior, 0xa0==-1) or 32-step gradient 224..255 (other scenes) into base palette DSPTR(0x304c), copy it to DSPTR(0x3032), scale by interpolated brightness (0x20ef, min DS8(0xd2)) and upload with sub_0791_067f; then step++ / phase++ (0..8) */
void sub_0a86_01c6(void);
/* player walked into water (pixel 4 under feet): unless at the two island-link spots, play 18-frame drowning animation (A99.PAK entry 0x36) losing 1 HP per frame, then redraw HUD */
void sub_0a86_0298(void);
/* per-frame game-clock tick: increment 32-bit time DS32(0x1fdc) (random event sub_0c05_056b 1/1000 in exterior; +2 rounded even in other scenes); every 64 half-ticks call sub_0a86_0000 and, if countdown mode DS16(0x11c0)==1, decrement DS16(0x1fe6) and on 0 force scene 3/room 0x1f */
void sub_0a86_0325(void);
/* copy current island's 6 ground colors (0x20f9 + zone*18) into palette entries 186..191 of both palettes DSPTR(0x3032) and DSPTR(0x304c) */
void sub_0a86_0370(int16_t sector);
/* load sector sprite bank: file "iN" (N=zone) .PAK entry 0x2153[zone*16+sector] into DSPTR(0x3498)+0x294, build long offset table [10 sprites][16 scales] and width/height table, generate 15 downscaled copies (sub_1110_01bc); applies zone colors and a palette step */
void sub_0a86_0517(void);
/* rebuild the 4 visible terrain cells (DSPTR(0x34aa), 0x400 bytes each) from player tile/view octant (0x1fcb&0x1f -> 0x202f/0x201f neighbour tables), decode tile geometry, and reload sector bank if sector changed; called by sub_0c9b_024e */
void sub_0a86_06ee(void);
/* allocate exterior work buffers: map 0x800, tile offsets 0x400, tile geometry 0x1f40, sprite bank 0x38270, cells 0x1000, point bufs 2x0x400, actor list 0x50 */
void sub_0a86_078e(void);
/* free all exterior buffers allocated by sub_0a86_06ee plus the RESID#2 resource DSPTR(0x34be) */
void sub_0a86_081f(int16_t spr, int16_t x, int16_t y, int16_t scale);
/* draw static scenery sprite spr (0..9) bottom-centred at screen x,y with scale 0..63 (-> level 0..15) from the sector bank; tracks min top y in DS16(0x349c); rejects x<-70 or x>400 */
void sub_0a86_08d3(int16_t anim, int16_t x, int16_t y, int16_t scale);
/* draw animated scenery object anim (0..31) from sector data table P32(L+8): draws current frame with sub_106a_04ff and advances/wraps the frame byte stored in the data */
void sub_0a86_09af(uint8_t *cell, int16_t side, int16_t layer);
/* transform and draw the objects (sprites) of a terrain cell (side 0/1/2 = half of tile / all), handles special objects (0x80 animated + proximity trigger sub_0c05_0400, 0x20 actor-type via sub_0936_0876, else static sprite) and inserts moving actors of this layer (sub_0c9b_09ae); temporarily raises camera height DS16(0x1fc6) */
void sub_0a86_0b29(uint8_t *cell, int16_t side);
/* draw the flat-shaded polygons of a terrain cell (only those on the requested side of the tile diagonal unless side==2): build points, lookup heights, transform, clip, fill with sub_0d84_0002 */
void sub_0a86_0c88(uint8_t *cell, int16_t side);
/* draw one ground triangle of a cell (side 0: corner (0,0x200), side 1: corner (0x200,0); plus diagonal corners) with heights from the cell height grid and color cell[0x32c/0x32d]+0xba */
void sub_0a86_0d96(int16_t y0);
/* present the frame: copy back buffer DSPTR(0x2d38) rows y0..171 to the screen DSPTR(0x2d3c) using the blitter for display mode DS16(0xe6) (0/1/2) and 386 flag DS16(0xec) */
void sub_0a86_0e25(void);
/* draw the compass at (280,140): background frame 0 and needle frame 1 rotated by (0x80-heading) from DSPTR(0x34c6) */
void sub_0a86_0e64(int16_t draw_actors);
/* render one exterior 3D frame into the back buffer: clock tick, sky (+stars at night), ground fill, then the 4 cells back-to-front (ground triangles, polygons, objects, ordered by heading/position), compass and countdown; draw_actors!=0 also collects moving actors (sub_0936_04af) */
int16_t sub_0a86_11ee(void);
/* load island map file "iN.dat" (N=zone): u16 ntiles, 32x32x2 map -> DSPTR(0x34a2), ntiles u16 offsets -> DSPTR(0x34ca), rest -> DSPTR(0x34b2); returns ntiles, 0 if file missing (callers ignore) */
void sub_0a86_12e5(void);
/* load exterior palette (RESID.PAK entry 3, 0x300 bytes) into both palettes DSPTR(0x3032) and DSPTR(0x304c), then free it */
void sub_0a86_1341(void);
/* quest patch: on island 3 when DS16(0xbb0)==3, empty tile geometry #20 (set its object count byte to 0) */
void sub_0a86_136f(void);
/* U-turn animation: rotate heading by 16 eight times (180 deg), rendering and presenting each frame */
void sub_0a86_13a1(void);
/* bump-back animation after hitting a blocked door: clear DS16(0x2286), play sound 10, step backwards 5 frames with rendering */
void sub_0a86_13e2(void);
/* (re)enter exterior view: load palette, init sky, load RESID#2 sprites (DSPTR(0x3262), compass DSPTR(0x34c6)), allocate buffers, load island map, reset view state, set scene 99 resource names, redraw HUD, place player sprite at (160,171) */
void sub_0a86_14ae(void);
/* snapshot back buffer into new 64000-byte buffer DSPTR(0x34c2) and replace colors >=0xc0 by 0x0f in its first 48000 bytes (150 rows) */
void sub_0a86_1519(void);
/* free the snapshot buffer DSPTR(0x34c2) and set DS16(0x1fe4)=1 (keep position on return) */
void sub_0a86_1530(void);
/* exterior key handling: Esc (scancode 1) -> game menu sub_07fe_000e, Enter (0x1c) -> U-turn sub_0a86_136f, then generic key handler sub_0053_1cf8 */
void sub_0a86_154d(void);
/* exterior main game loop (called at end of main, never returns: no retf): intro camera descent, per frame movement/render/present/keys/events, island switching at x edges, running indoor scenes via sub_0053_1f74 and repositioning the player from door tables 0x21a4/0x2244 on return */
