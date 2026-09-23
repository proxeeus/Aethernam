/* G4 – segment 0596: room rendering (backdrop/parallax layers, depth-sorted draw list,
 * actors/projectiles), speech bubbles & dialogue choice menu, full-screen PAK animation
 * player, clip-rect helpers, bottom-panel drawing, room scroll transition.
 * All functions are FAR (retf); none is near/static.
 * Actor = 0x46-byte record at DS:0x11a + i*0x46 (see re/notes/G4.md). */

/* ---- backdrop buffer (DSPTR 0x17b4, 64K) ---- */
void     sub_0596_0001(void); /* ensures the 0xFFFF-byte backdrop buffer DSPTR(0x17b4) is allocated: if NULL, flush resource cache (sub_0053_028e) when farcoreleft (sub_0791_05f7) < 0xFFFF, then DSPTR(0x17b4)=farmalloc(0xFFFFL) */
void     sub_0596_0033(void); /* frees the backdrop buffer DSPTR(0x17b4) (farfree) if non-NULL and sets it NULL */

/* ---- per-frame overlay / presentation (called from main loop sub_0053_1e8e) ---- */
void     sub_0596_0059(void); /* per-frame FX+present: fx 1 = redraw hero reflection (actor 1 at 0x160 with hero's anim/frame) clipped into two window rects when hero x>0xa0,y<0x88; fx 3 = count down [0x3260] (0 -> fx off) else particles (0791_025b); lightning/sprinkle/panel-refresh hooks; frame limiter; then present: dirty rect [0xde..0xe4] via 02a7_0b4a, else by zoom [0x177c] 0=plain copy,1/2/3=2x/3x/4x zoom around hero; room 0x17 palette pulse (0..15) when [0x11c0]==1 && [0xa0]==3 */

/* ---- text / clip helpers ---- */
void     sub_0596_02a0(int16_t color); /* selects the game font DSPTR(0x3042) and text colour `color` (sub_1172_011d) */
void     sub_0596_02b7(int16_t x1, int16_t y1, int16_t x2, int16_t y2); /* sets the drawing clip rectangle: xmin=x1,xmax=x2 (ss:0x196/0x198), ymin=y1,ymax=y2 (ss:0x192/0x194) */
void     sub_0596_02d6(void); /* clip rectangle = play area: (0, [0x303c]) - (319, [0x3054]-1) */
void     sub_0596_02ec(void); /* clip rectangle = full screen (0,0)-(319,199) */
void     sub_0596_02fe(uint8_t *str, int16_t x, int16_t y, int16_t color); /* draws NUL-terminated string at (x,y) into the draw buffer DSPTR(0x2d38) in colour `color` using the game font */
uint8_t *sub_0596_0327(uint8_t *str); /* returns pointer just past the terminating NUL of str (start of the next sub-string) */
int16_t  sub_0596_033e(uint8_t *text, int16_t x, int16_t y, int16_t color, int16_t mode); /* draws up to 3 NUL-separated lines of a '*'-terminated text block, 10 px apart, y clamped >= [0x303c]+3; mode 0 = each line centred on x (kept within 3..0x13c, empty lines skipped), else left-aligned at x (see notes: uninitialised local => every line drawn); returns y below last line */
void     sub_0596_0405(uint8_t *text); /* measures one page (<=3 lines) of a '*'-terminated text block: DSPTR(0x3242)=text, [0x30aa]=max width/2, [0x30ac]=10*non-empty lines, [0x328a]=[0x32aa]=display time (sum width/4), [0x17be]=1 if another page follows, DSPTR(0x3028)=next page */
void     sub_0596_04b7(int16_t x1, int16_t y1, int16_t x2, int16_t y2); /* draws a speech-bubble frame: rect shifted to stay on screen/in play area, interior filled col 0x1a, double border lines cols 0x43/0x45, corner sprites 0..3 of bank DSPTR(0x3066) */

/* ---- speech bubble & choice menu ---- */
void     sub_0596_067e(void); /* ends speech display: [0x17bc]=0 (speech active), [0x9c]=0 (bubble y override) */
void     sub_0596_0687(void); /* per-frame speech bubble: if speaking and speaker actor [0x3298] is in current room [0xce], draws bubble+centred text DSPTR(0x3242) (colour [0x325e]) above it (x=0xa0 if [0xa0]==8, y=[0x9c] if set); counts down [0x328a], then shows next page (0405) or ends speech (067e) */
void     sub_0596_0767(void); /* draws the dialogue choice menu at ([0x302c],[0x302e]): optional question DSPTR(0x3252) (if [0x3278]!=-1) then [0x303a] answers from table 0x306a (10-byte entries, text fptr +2), each in its own bubble; selected [0x3274] highlighted (colour cycle 0x16..0x19 via [0x17b8]/[0x17ba] if hero text colour [0x146]==0x19, else blink with [0x1896]&1 / col 0x9f) */

/* ---- room object drawing ---- */
void     sub_0596_08e0(int16_t i); /* draws projectile slot i (table 0x692, 12 bytes): if used (+0!=0xff): age(+2)==-1 -> packed image 0 of entry 1 of DSPTR(0x3262) at (+6,+8-25); else impact frame `age` of bank DSPTR(0x3262) at (+6,+8) with param +4 (sub_106a_04ff) */
void     sub_0596_0965(int16_t dir); /* horizontal room-scroll transition: copies screen DSPTR(0x2d3c) to draw buffer, then 8 steps of 40 px: shifts play area by 40 px (dir==-1: image moves left, new room strip enters at the right edge; else moves right, strip enters at left) pulling the strip of the new room from DSPTR(0x3056), presenting each step (0dc5_0148) with pause check */
void     sub_0596_09d5(void); /* room-scroll transition, new room enters from the right: sub_0596_0965(-1) */
void     sub_0596_09de(void); /* room-scroll transition, new room enters from the left: sub_0596_0965(0) */
void     sub_0596_09e7(uint8_t *actor, int16_t x1, int16_t y1, int16_t x2, int16_t y2); /* fx 2 mirror: if actor x within [x1-16,x2+16] and actor y >= y2, draws actor's mirrored image (reversed facing if +0x0e==4) at (x-3,y-8) clipped to (x1,y1)-(x2,y2); sprite from anim frames (kind +0x12 2/4) or dir tables 0x1820 (kind 1)/0x180e; restores play-area clip */
uint8_t *sub_0596_0b11(int16_t seq, uint8_t *res); /* returns animation sequence `seq` of a PAK resource: sub-entry seq of entry 2 (sub_0053_0564(2,seq,res)); byte +1 = frame count, frames (8 bytes each) start at +2 */
int16_t  sub_0596_0b28(uint8_t *bank, uint8_t *frames, int16_t idx, int16_t step, int16_t x, int16_t y, int16_t nframes); /* draws frame idx of an animation at (x,y)+sum of frame dx/dy 0..idx; frame type (word+2 bits 14-15): 0 plain, 2 sprite w/ param (106a_04ff), 3 shaded (106a_0593), 1 morph toward next frame over `param` steps using work buffer DSPTR(0x3294): returns next step (0 when done), other types return 0 */
void     sub_0596_0cca(int16_t i); /* draws actor i (0x11a+i*0x46) clipped to its rect (+0x3c,+0x40b)-(+0x3e,+0x41b): fx 2 mirror image, then morph anim (+0x10==2, stores step in +0x1e) or frame +0x1c of sequence +0x1a; hero extras: look beam (02a7_0c92) and sparkle (0791_0506) if [0x11be]; restores play-area clip */

/* ---- bottom panel (action bar) ---- */
void     sub_0596_0e28(void); /* draws action-bar cursor: state [0x3266]==3 (flash) -> glyph [0x17b0]+2 and 1 at (0,0) and glyph 9 at ([0x17a6],[0x17a8]); else glyph 8 at ([0x17a6],[0x17a8]); bank DSPTR(0x3062) */
void     sub_0596_0e96(void); /* draws bottom panel background: glyphs 0 and 1 of bank DSPTR(0x3062) at (0,0) */
void     sub_0596_0ec3(void); /* redraws bottom panel: [0x1738]=0, full clip, 0e96, copies rect (0,0xac)-(0x140,0xc7) to screen, 02a7_08f3 (panel contents on screen), play-area clip */
void     sub_0596_0eee(void); /* clears play area of draw buffer: fill (0,0)-(0x140,0xab) colour 0 */
/* NOT in function list, unreferenced dead code found in the gap 0f03..0f40: */
void     sub_0596_0f03(void); /* dead: calls sub_0596_0e96() */
void     sub_0596_0f08(void); /* dead: full clip; draws panel (0f03) directly on screen (temporarily DSPTR(0x2d38)=DSPTR(0x2d3c)) and again in draw buffer; play-area clip */

/* ---- full-screen animation player (script op 0x1f/0x51) ---- */
void     sub_0596_0f41(int16_t pak, int16_t seq, int16_t bg, int16_t reps); /* plays animation seq of A??.PAK entry `pak` (DSPTR(0x18a0)) `reps` times over background bg (0 black, <0 sprite -bg of the anim, <0x7d00 room sprite bg, ==0x7d00 current screen); per frame: restore bg from 0x3056, draw (0b28), speech (0687), palette (entry 4) on first frame, present, sound cues from DSPTR(0x32a2) ([0x328c] triples frame/sfx/-); Enter skips, Esc -> sub_07fe_000e; frees anim, restores palette, redraws room unless next script op is another animation */

/* ---- room background ---- */
void     sub_0596_11cf(int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t n); /* draws n random sparkles (packed image 0x67+rand(2) of entry 1 of room pack DSPTR(0x3286)) at random points in (x1,y1)-(x2,y2) */
void     sub_0596_1252(void); /* draws room contents in depth order from draw list DSPTR(0x3270) (count byte +0, 2-byte records from +3: hi nibble 0=actor 0cca, 2=projectile 08e0, 1=next room foreground piece from sub-entry [0xce]*2+1 of DSPTR(0x3286)); room 0x13 sparkles; look beam; exit markers (171e) */
void     sub_0596_13bb(int16_t room, int16_t slot); /* draws backdrop layer `slot` of table 0xbbc (if pak idx [0xbbc+slot*2]>0: load D??.PAK entry into DSPTR(0x17b4), install its palette (entry 4) if present, draw sprite [0xbbe+slot*2]) then room background sprite room*2 of DSPTR(0x3286) (clearing rows 0..0x12 if [0xa0]==0); [0xa0]==0x63: copy DSPTR(0x34c2) to draw buffer + sub_0a86_0325 instead */
void     sub_0596_14c9(int16_t room); /* sub_0596_13bb(room, 0) */
void     sub_0596_14d9(int16_t room); /* draws room foreground sprite room*2+1 of DSPTR(0x3286) at (0,0) if it is a real sprite (byte +1 != 0) */
void     sub_0596_1543(int16_t room, int16_t slot); /* clears draw buffer, draws room background with backdrop slot (13bb) and foreground (14d9) */
void     sub_0596_1564(void); /* builds parallax background in DSPTR(0x3056): copies draw buffer there, renders room [0xbc0] (backdrop slot 3); if [0xc6]==2 blits it 1/4-scaled at ([0xbc6],[0xbc8]) then renders room [0xbca] (slot 8) and uses ([0xbd0],[0xbd2]); finally blits the last rendered room 1/2-scaled at that position (colour 0 transparent) and clears the draw buffer */
void     sub_0596_1614(void); /* if sub-palette byte [0xd4] != [0xd3]: [0xd3]=[0xd4], copies 16 colours (48 bytes) from DSPTR(0x301c)+idx*48 into palettes DSPTR(0x3032) and DSPTR(0x304c) at colour 0xa0, and uploads them to the DAC (118f_01ab) */
void     sub_0596_169f(void); /* full room background rebuild: clear, parallax (1564 if [0xc6]), room bg/fg for [0xce], bottom panel, then stores result in DSPTR(0x3056) (transparent overlay if [0xc6]), colour-0xf gradient effect if [0xee], sub_0a86_0000, sub-palette update (1614) */
void     sub_0596_171e(void); /* draws exit markers: for each zone in table 0x752 (count [0xa8]) with type 3, two lines col 1 on rows 0xaa/0xab from x(+2) to x(+4) and small col-9 boxes at both ends */
void     sub_0596_17cd(void); /* blanks the play area: clears it in the draw buffer (0eee) and copies (0,0)-(0x140,0xab) to screen */
