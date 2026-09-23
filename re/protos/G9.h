/* G9 - low-level graphics & helper library (hand-written asm segments 0d84..1172).
 * All buffers are 320-pitch 8bpp. "clip rect" = SS16(0x192)=ymin, SS16(0x194)=ymax,
 * SS16(0x196)=xmin, SS16(0x198)=xmax (inclusive).  "row(y)" = SSU16(y*2) = y*320.
 * "drawbuf" = DSPTR(0x2d38), "screen" = DSPTR(0x2d3c), "bgbuf" = DSPTR(0x3056).
 * View rows: DS16(0x303c)=top (0), DS16(0x3054)=bottom-exclusive (172).
 * static = near helper / only used inside its own segment (see re/notes/G9.md). */

/* ---- 0d84 : convex polygon filler -------------------------------------- */
void sub_0d84_0002(int16_t dx, int16_t dy, uint8_t *pts, uint16_t n, uint8_t color); /* fills convex polygon of n int16 (x,y) pairs at pts, each offset by (dx,dy), solid color into drawbuf; Sutherland-Hodgman clip vs clip rect; nothing drawn if clipped to <=2 verts or ymin==ymax; uses DGROUP work area 0x2844..0x2d04 */
static void sub_0d84_0235(int16_t xmin); /* S-H clip current vertex list DS16(0x2844) (DS16(0x2cfe) edges) against x>=xmin into list DS16(0x2846); count->DS16(0x2d00), sets DS16(0x2d02)=1, closes output list and swaps 0x2844/0x2846 */
static void sub_0d84_02b0(int16_t xmax); /* same, keep x<=xmax */
static void sub_0d84_032c(int16_t ymin); /* same, keep y>=ymin (intersection x = x0+(ymin-y0)*dx/dy, endpoints not reordered) */
static void sub_0d84_039e(int16_t ymax); /* same, keep y<=ymax */

/* ---- 0dc5 : screen scroll / copy / effects ------------------------------ */
void sub_0dc5_0000(int16_t n); /* scroll drawbuf rows [0x303c]..171 left by n px (row: memmove(row, row+n, ((320-n)>>1)*2)) */
void sub_0dc5_0040(int16_t n); /* scroll drawbuf rows [0x303c]..171 right by n px (backward copy, starts at row 171) */
void sub_0dc5_007f(int16_t n, int16_t srcx); /* after left scroll: per view row copy (n>>1)*2 bytes bgbuf[row+srcx] -> drawbuf[row+320-n] */
void sub_0dc5_00ca(int16_t n, int16_t srcx); /* after right scroll: per view row copy (n>>1)*2 bytes bgbuf[row+320-srcx-n] -> drawbuf[row+0] */
void sub_0dc5_0117(void); /* copy view rows [0x303c]..[0x3054) bgbuf -> drawbuf (restore background) */
void sub_0dc5_0148(void); /* DS16(0x2d66)++ (frame counter); copy view rows [0x303c]..[0x3054) drawbuf -> screen; +plat_present() */
void sub_0dc5_017d(void); /* DS16(0x17e2)=0; 7-pass interleaved dissolve drawbuf->screen, pass k copies bytes k+7i, i<7862 (rows 0..171), 2x sub_118f_00de (vsync) before each pass */
void sub_0dc5_01c5(void); /* same dissolve over 9097*7 bytes (rows 0..198) */
void sub_0dc5_020d(uint8_t *src, uint8_t *dst); /* copy 64000 bytes src->dst skipping color 0 (transparent overlay) */
void sub_0dc5_022e(void); /* bgbuf sky gradient: scan 2-row bands bottom-up from row 171; from first band containing color 15, replace every 15 by 0xFF,0xFE.. (one value per 2-row band, stops decrementing at 0xE0) */
void sub_0dc5_026b(int16_t x, int16_t y); /* magnify x4: 80x50 drawbuf window at (clamp(x-40,0,239),clamp(y-44,0,122)) -> full screen (offset 0) */
void sub_0dc5_02dd(int16_t x, int16_t y); /* magnify x3: 106x66 window at (clamp(x-53,0,213),clamp(y-50,0,106)) -> screen, each row 318px+2 extra px of last color, last 2 screen rows cleared to 0 */
void sub_0dc5_0355(int16_t x, int16_t y); /* magnify x2: 160x100 window at (clamp(x-80,0,159),clamp(y-80,0,72)) -> screen */
void sub_0dc5_03de(uint8_t *pal, int16_t r0, int16_t g0, int16_t b0, int16_t r1, int16_t g1, int16_t b1); /* write 32-step gradient into palette entries 224..255 (pal+0x2a0): c[i] = (uint8)(((uint16)((c1-c0)*i) >> 5) + c0), i=0..31 */
void sub_0dc5_0448(uint8_t *src, uint8_t *dst, int16_t level); /* 256-entry palette tint (level 0..16): R'=R+(((R+G+B+100)>>2)-R)*level>>4 (sar), G'=G*(16-level)>>4, B'=B*(16-level)>>4 */
void sub_0dc5_04b1(uint8_t *pts, uint8_t *out); /* build byte lookup table by piecewise-linear interpolation: pts = u16 n, n x (int16 x, int16 v); writes one byte per x step (Bresenham, v low byte) plus a final byte */
void sub_0dc5_051b(uint8_t *src, uint8_t *dst); /* half-size transparent blit: ((172-[0x303c])>>1) rows x 160 px, src pixel (2x,2y) -> dst (x,y) if !=0; both pitch 320 */
void sub_0dc5_0551(uint8_t *src, uint8_t *dst); /* quarter-size transparent blit: ((172-[0x303c])>>2) rows x 80 px, src (4x,4y) -> dst (x,y) if !=0 */
void sub_0dc5_058d(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color); /* Cohen-Sutherland clipped DASHED line into drawbuf (pattern 2 on/2 off, first pixel always on); zero-length line draws nothing; temps DS 0x2d06..0x2d0c */
void sub_0dc5_0780(void); /* get time of day: DS8(0x3040)=hour, DS8(0x305e)=minute, DS8(0x3048)=second  -> plat_get_time() */

/* ---- 0e3f : VGA palette ---------------------------------------------------- */
void sub_0e3f_00be(uint8_t *rgb, uint16_t first, uint16_t count); /* set DAC entries first..first+count-1 from 6-bit RGB triplets -> plat_set_palette(first,count,rgb) */
void sub_0e3f_00de(uint8_t *pal); /* in place: 768 bytes >>= 2 (8-bit -> 6-bit palette) */
void sub_0e3f_0104(void); /* wait for start of vertical retrace -> plat_wait_vsync() */

/* ---- misc helpers ---------------------------------------------------------- */
int16_t sub_0e9b_000c(uint8_t *a, uint8_t *b); /* rect overlap test, rects are int16 {x0,y0,x1,y1} inclusive: returns 1 if a.x0<=b.x1 && a.y0<=b.y1 && a.x1>=b.x0 && a.y1>=b.y0 (signed), else 0 */
uint16_t sub_0f21_0004(void); /* game PRNG (not TC rand): 32-bit LFSR v=DS16(0x2d1c):DS16(0x2d1a), 16x { v=(v<<1)|(bit30^bit29) }, returns low word */
int32_t sub_0f23_000a(uint8_t *name); /* size of file `name` (open/lseek end/close), -1 if it cannot be opened -> plat_file_size() */
uint8_t *sub_0f3a_0004(int16_t v); /* signed int -> decimal string in static DSADDR(0x2d1e) (no leading zeros, '-' if negative), returns that pointer (custom itoa, not TC's) */
static uint16_t sub_0f3a_00d3(uint16_t v, uint16_t div, int16_t *started, uint8_t **out); /* near digit emitter: q=v/div; emits '0'+q if q!=0 or *started (then *started=1); returns v%div */

/* ---- 0fe6 : clip rect + RLE sprite blitter ------------------------------- */
void sub_0fe6_01ac(int16_t xmin, int16_t xmax); /* SS16(0x196)=xmin, SS16(0x198)=xmax */
void sub_0fe6_01bf(int16_t ymin, int16_t ymax); /* SS16(0x192)=ymin, SS16(0x194)=ymax */
void sub_0fe6_01d2(uint16_t id, int16_t x, int16_t y, uint8_t *dst, uint8_t *bank); /* draw RLE sprite (id&0xfff) of bank into dst (pitch 320) with left x and BOTTOM row y, clipped to clip rect; id&0x8000 = mirrored (re-mirrors sprite data IN PLACE if its flag word differs), id&0x4000 = opaque (transparent runs written as 0) */

/* ---- 1036 / 103b : block copies ----------------------------------------------- */
void sub_1036_000a(uint8_t *src, uint8_t *dst); /* copy 64000 bytes src -> dst (full screen buffer copy) */
void sub_103b_0004(uint8_t *src, int16_t sx, int16_t sy, uint8_t *dst, int16_t dx, int16_t dy, uint16_t w, uint16_t h); /* copy w x h rectangle, both pitch 320, src(sx,sy) -> dst(dx,dy); per row (uint8)(w>>1) words + (w&1) bytes */

/* ---- 1040 : lines ------------------------------------------------------------- */
void sub_1040_0006(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color); /* solid line into drawbuf: y0==y1 -> clipped hline fast path; else Cohen-Sutherland clip + Bresenham; clipped zero-length line draws nothing; temps DS 0x2d84..0x2d8a */
void sub_1040_023a(int16_t x0, int16_t y, int16_t x1, uint8_t color); /* horizontal line x0..x1 (x0<=x1) at y in drawbuf, clipped to clip rect */

/* ---- 106a : vector shape renderer ------------------------------------------ */
static void sub_106a_0407(uint8_t flags); /* set coord-decode state of sub_106a_0477 (was code patching): cs:0xc = flags (0x10 int8 coords, 0x80 negate x, 0x20 negate y) */
static void sub_106a_043f(uint8_t flags); /* same for sub_106a_04ff: cs:0xd = flags */
void sub_106a_0477(uint16_t id, int16_t x, int16_t y, uint8_t *shp); /* draw vector shape (id&0xfff) of shape file shp at (x,y); id high byte 0x80 = mirror x, 0x20 = mirror y; vertices decoded per flags, + (x,y), then primitive handler */
void sub_106a_04ff(uint16_t id, int16_t x, int16_t y, uint8_t *shp, int16_t scale); /* same, vertices scaled: (int16)((int32)v*scale >> 8) + x/y  (scale 8.8, 256 = 1:1) */
void sub_106a_0593(uint16_t id, int16_t x, int16_t y, uint8_t *shp, uint16_t angle); /* same, vertices rotated by angle&0xff (256 = full turn, sine table DS16(0x2d8c), 16383 = 1.0): x'=((vx*cos - vy*sin)>>14)+x, y'=((vy*cos + vx*sin)>>14)+y; always int16 coords, NO mirroring (original patches 04ff's code by mistake) */
void sub_106a_06a1(uint16_t unused, int16_t x, int16_t y, uint8_t *shp, uint16_t t); /* draw morph shape (always shape #0 of shp): each vertex a+(b-a)*t>>8 (t 0..256); color pair per primitive chosen by prim index vs (t*nprims)>>8 */
static uint8_t *sub_106a_080d(uint8_t *si, uint16_t n, uint8_t *shp); /* prim 0: draw sub-shape: sub_106a_0477(colA | (colB|cs:0xc)<<8, v0.x, v0.y, shp), then restore cs:0xc state; returns si */
static uint8_t *sub_106a_084a(uint8_t *si, uint16_t n, uint8_t *shp); /* prim 1: sprite id=colA|(colB^cs:0xc)<<8 from bank shp+P16(shp+4) at v0 (if cs:0xc&0x80 x-=sub_115f_000c, if &0x20 y+=sub_115f_0054) via sub_0fe6_01d2 into drawbuf */
static uint8_t *sub_106a_08a8(uint8_t *si, uint16_t n, uint8_t *shp); /* prims 2,3: returns si + (uint16)((0x100-n)*4) (data skip; probably unused) */
static uint8_t *sub_106a_0734(uint8_t *si, uint16_t n, uint8_t *shp); /* prim 4: sub_0d84_0002(0,0,vbuf,n,colB); if colA!=0xff: close vbuf, polyline n+1 pts in colA */
static uint8_t *sub_106a_0774(uint8_t *si, uint16_t n, uint8_t *shp); /* prim 5: box v0-v1 (sorted): sub_10f5_0028 fill colB; if colA!=0xff sub_116d_000c outline colA */
static uint8_t *sub_106a_07b8(uint8_t *si, uint16_t n, uint8_t *shp); /* prim 6: polyline, n-1 segments via sub_1040_0006 in colB (n must be >=2) */
static uint8_t *sub_106a_07e3(uint8_t *si, uint16_t n, uint8_t *shp); /* prim 7: n points: sub_1100_00c0(colB) then sub_1100_000b(v[i]) */

/* ---- 10f5 / 1100 / 110c : rect & pixels -------------------------------------- */
void sub_10f5_0028(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color); /* filled rectangle (inclusive) into drawbuf, clamped to clip rect; nothing drawn if (clamped) x1<=x0 or y1<=y0 */
void sub_1100_000b(int16_t x, int16_t y); /* plot pixel in current point color (static, set by sub_1100_00c0) into drawbuf if inside clip rect */
void sub_1100_0096(int16_t x, int16_t y); /* plot pixel in current point color, no clipping */
void sub_1100_00c0(uint8_t color); /* set current point color */
uint8_t sub_110c_000c(int16_t x, int16_t y); /* read pixel of drawbuf at (x,y), no clipping */

/* ---- 1110 / 115f : sprite helpers -------------------------------------------- */
uint8_t *sub_1110_01bc(uint16_t idx, uint16_t level, uint8_t *bank, uint8_t *dst); /* build a shrunk copy of sprite idx (no mask) as a 1-sprite bank at dst: level 0..14 keeps level+1 of every 16 columns (fixed pattern) and rows whose (1<<(rowsleft&15)) is in a mask; re-RLE-encodes; returns pointer to end of written data */
uint16_t sub_115f_000c(uint16_t id, uint8_t *bank); /* width in pixels (byte0*16) of sprite id&0xfff in bank */
uint16_t sub_115f_0054(uint16_t id, uint8_t *bank); /* height (byte1) of sprite id&0xfff in bank */

/* ---- 116d / 1172 : outline rect, bitmap font ---------------------------------- */
void sub_116d_000c(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color); /* rectangle outline: hline y0, hline y1 (sub_1040_023a), vlines x0 and x1 (sub_1040_0006) */
void sub_1172_0012(int16_t x, int16_t y, uint8_t *dst, uint8_t *str); /* draw NUL-terminated string with current font/color at (x,y) (top-left) into dst (320 pitch; ORIGINAL USES ONLY THE SEGMENT of dst, i.e. offset treated as 0); 1bpp glyphs, set bits drawn, no clipping; pen x left in static cs:0x10 */
void sub_1172_011d(uint8_t *font, uint8_t color); /* select font (parse header into statics) and text color */
int16_t sub_1172_018c(uint8_t *str); /* pixel width of string in current font: sum(glyphw ? glyphw : 2) + 1 per char */
