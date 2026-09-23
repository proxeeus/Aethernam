/* G2 – segments 026c (player/NPC projectiles) and 02a7 (messages, inventory screen,
 * look/take/use actions, bottom action bar).  All functions are FAR (retf). */

/* ---- segment 026c : projectiles (16 slots x 12 bytes at DS:0x692) ---- */
void    sub_026c_0006(void); /* marks all 16 projectile slots free (slot+0 = 0xff); called on room change */
int16_t sub_026c_0032(void); /* returns index of first free projectile slot (byte +0 == 0xff), or -1 if none */
void    sub_026c_0067(int16_t owner); /* spawns a projectile fired by character `owner` (char table 0x11a+owner*0x46): x=owner.x+rand(9)-8, y=owner.y, dir=owner.dir(+0x12), age=-1, +4=rand(128)+0x20; plays sound 0x31. No-op if no free slot */
void    sub_026c_010d(int16_t target, int16_t dir, int16_t damage); /* projectile hit on character (target&0xf): if char.+0x14!=0 knock it back 0x20 px in dir (if new rect free of chars/obstacles); if char.+0x14==1 subtract `damage` from hp(+0x2a), start particle burst (fx mode [0x10c]=3) at (x,y-10) and play sound 0x32 */
void    sub_026c_0237(void); /* per-frame projectile update: flying ones (age -1) move 15px horiz / 5px vert per dir, test hit vs characters (sub_0714_03d2) and walls (sub_0714_031f); on hit start impact (age 0) and call sub_026c_010d(hit,dir,1); impact frames count age 0..8 then slot freed */
void    sub_026c_034a(void); /* inserts every active projectile into the depth-sorted draw list: sub_0053_075b(y, 0x20+slot) */
void    sub_026c_038b(void); /* if player anim state (char0+0x0e,[0x128])==8 and anim frame (char0+0x1c,[0x136])==1: player fires (sub_026c_0067(0)) and plays sound 0x14 */

/* ---- segment 02a7 ---- */
void    sub_02a7_0006(int16_t kind, int16_t item); /* posts a status message: timer [0x104]=20, kind [0x106] (text 0x36+kind of table DSPTR(0x3290)), item [0x108] (item name appended if kind<4) */
void    sub_02a7_001d(int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t color); /* draws a bevelled box in the draw buffer: top/left lines col 0x9f, right/bottom 0x98, interior (x1+1,y1+1)-(x2-1,y2-1) filled with `color` */
void    sub_02a7_0098(void); /* draws the current status message box (centred at x=160, y 0xa0..0xab, colour 0x9c) = message text [+ item name]; decrements timer [0x104] */
void    sub_02a7_0181(void); /* inventory open: counts owned items into [0xbe], turns state 3 (just used) back into 2; if equipped item [0xba] no longer owned, picks next owned item; cursor [0xb8]=[0xba] */
void    sub_02a7_01e8(void); /* draws the inventory list: up to 12 owned items from [0xbc] at x=0x1e, y=0x2c+10*k; colour 0x97, cursor item 0x9f + pointer glyph 0xc at (0xf,y+8), equipped item (state 2) 0x4d */
void    sub_02a7_02a9(void); /* initialises the heartbeat (ECG) trace: [0x192a]=0, [0x1928]=rand(8)+1, 50 points at 0x30ae: x=0x109-i, y=0x14, dy=0, colour=0x7f-i/7 */
void    sub_02a7_0319(uint8_t *pt, int16_t x, int16_t y); /* ECG helper: if x >= pt.x draws a line (x,y)-(pt.x,pt.y) in colour pt+6 */
void    sub_02a7_0351(uint8_t *pts); /* ECG draw: restores column (head.x,4)-(head.x+1,0x24) on screen from back buffer, then draws the 50-point trail directly on the screen (shifting point history by one) and plots the head pixel in colour 0x9f */
void    sub_02a7_0410(void); /* ECG step: beat state machine (delay [0x1928], phase [0x192a], amplitude [0x32ae]), head x += [0x32ac] wrapping 315->215, y += dy, then sub_02a7_0351 */
void    sub_02a7_04f1(void); /* draws inventory-screen numbers with interface glyphs: play time [0x1fdc] as hhh mm ss (glyphs 0x17+d) and T%20 (glyphs 0x21+d), gold [0x11c6], completion % (sub_0c05_020c) */
void    sub_02a7_07c8(int16_t value, int16_t x, int16_t y); /* draws `value` clamped 0..999 as 3 digit glyphs (0xd+digit, font DSPTR(0x3062)) at x, x+0x11, x+0x22 */
void    sub_02a7_0863(void); /* redraws player HP box in the bottom bar: fill (0x106,0xb4)-(0x11c,0xbc) col 0x1d, HP [0x144] clamped 0..99 as 2 glyphs (0x2c+d) at y 0xbb; full-screen clip during, game-view clip after */
void    sub_02a7_08f3(void); /* same as sub_02a7_0863 but draws directly into the screen buffer DSPTR(0x2d3c) */
void    sub_02a7_0920(void); /* draws the inventory/status screen background (glyph 0xa), labels "Time","Name","gp","%","Don JONZ", then sub_02a7_04f1 (no own prologue; void) */
int16_t sub_02a7_09d0(void); /* returns previous owned item index before cursor [0xb8], or [0xb8] if none */
int16_t sub_02a7_09fe(void); /* returns next owned item index after cursor [0xb8] (<256), or [0xb8] if none */
void    sub_02a7_0a2d(void); /* computes first visible inventory row [0xbc] = 5 owned items before the cursor (cursor unchanged) */
void    sub_02a7_0a5f(void); /* inventory cursor up: [0xb8]=[0xbc]=previous owned item */
void    sub_02a7_0a6a(void); /* inventory cursor down: [0xb8]=[0xbc]=next owned item */
int16_t sub_02a7_0a75(void); /* inventory input: up/down move cursor; Enter or fire equips cursor item (old equipped -> state 1, new -> 2) and returns 1; Tab/Enter/Esc return 1; else 0 */
void    sub_02a7_0b4a(int16_t x1, int16_t y1, int16_t x2, int16_t y2); /* copies rectangle (x1,y1)-(x2,y2) inclusive from draw buffer DSPTR(0x2d38) to the screen DSPTR(0x2d3c) (sub_103b_0004) */
void    sub_02a7_0b83(void); /* blits the three dynamic areas of the inventory screen (time, gold, item list) to the screen */
void    sub_02a7_0bb5(void); /* inventory/status screen main loop (action "I"): dissolve in, per-frame input/redraw/ECG, advances play time [0x1fdc] each frame, 'V' key shows version string; on exit restores palette DSPTR(0x3032) and game view */
void    sub_02a7_0c92(void); /* while looking ([0x329a]!=0) draws a scan beam (sub_0dc5_058d, col 0x9f) from the player's head (offset by look dir [0x3060]) to target ([0x30a8],[0x323e]) jittered by rand(3)-1 */
void    sub_02a7_0d09(int16_t item, int16_t timer, uint8_t *texts); /* shows description text #item of text table `texts` in the dialog box (sub_0596_0405); sets [0x3298]=0,[0x325e]=25,[0x326c]=item,[0x328a]=[0x32aa]=timer,[0x17bc]=1 */
void    sub_02a7_0d46(uint8_t *rect, int16_t widen, int16_t reach); /* fills rect[4] (x1,y1,x2,y2 s16) with the zone in front of the player according to facing [0x12c], extended by `reach` (and `widen` sideways when facing up), clamped to 0..319 x 0..171 */
void    sub_02a7_0e21(void); /* auto-look (every frame + action "L"): counts down/cancels active look [0x329a]; else, if player free, finds an undescribed hotspot in front (reach 0x32), marks it, starts 20-frame look and shows its description */
void    sub_02a7_0f54(void); /* clears the "described" flag (+2) of every hotspot whose item is not owned */
void    sub_02a7_0f92(void); /* action "T" (Take): picks up the hotspot item in front of the player: owns it, selects it, shows its text, message 2; message 4 if not takeable, 5 if nothing */
void    sub_02a7_1038(void); /* action "S": sets flag [0x327a]=1 (talk/speak request handled by main loop) */
void    sub_02a7_103f(void); /* action "I": opens the inventory screen (sub_02a7_0bb5) */
void    sub_02a7_1044(void); /* action "L" (Look): resets hotspot flags, runs sub_02a7_0e21; message 6 if nothing found */
void    sub_02a7_1066(void); /* action "U" (Use): marks the equipped item [0xba] as used (state 3) and posts message 3 */
void    sub_02a7_108d(void); /* action "D" (Disk): calls the load/save menu sub_07fe_0ff1 */
void    sub_02a7_1093(void); /* opens the action bar if actions are allowed (sub_0053_1cb8): state [0x3266]=1, bar y [0x17a8]=0xd4, cursor to pending action [0x17aa] or 0; else state 0, [0x17aa]=-1 */
void    sub_02a7_10df(void); /* action bar state [0x3266]=0 (closed) */
void    sub_02a7_10e6(uint8_t key); /* action bar key handler (scancode): T,U,L,S,I,D select action 0..5, Enter picks cursor action, Tab opens/closes (waits Tab release), Esc -> quit prompt sub_07fe_000e; applies pending action to the bar */
void    sub_02a7_11ed(void); /* action bar cursor: slides [0x17a6] 2px/frame toward x of action [0x17b0] (table 0x179a); when there, joystick left/right changes [0x17b0] (0..5) */
void    sub_02a7_1264(void); /* waits for vertical retrace (sub_118f_00de) then blits bottom panel (0,0xac)-(0x140,0xc7) to the screen */
void    sub_02a7_127c(void); /* action bar modal loop: slide up, choose, flash, slide down, then calls action DSPTR(0x1782+4*[0x17aa]) */
