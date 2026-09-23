/* G6 - segments 07fe (save/load, options panel, resource re-linking) and 0c05 (copy protection,
 * 3D-world helpers).  All functions are FAR (retf); none is near/static. */

/* ---- segment 07fe ---- */
void    sub_07fe_000e(void);                        /* "Quit game?" box drawn directly on screen (msg #0x36 of text DSPTR(0x3290)); waits for a key, on Y/O/J/S scancode (0x15/0x18/0x24/0x1f) calls sub_0791_0665 (exit to DOS) */
int16_t sub_07fe_00b2(uint8_t *filename);           /* file-open error handler (installed in fnptr DSPTR(0x2d40), called by fopen sub_0e66_0031): box + msg #0x37 + filename, waits key, Esc -> sub_07fe_000e; always returns 1 (= retry) */
void    sub_07fe_015c(void);                        /* after a savegame load: rebases the anim table at DS 0x7b2 (DSU16(0xd8) entries x 0x16 bytes): +0 = anim base (from offset table at start of resource DSPTR(0x177e)), +4 = +0 + (uint16)(old+4 - old+0) */
void    sub_07fe_01d5(uint8_t *oldptr, uint8_t *newptr); /* replaces sprite pointer +0x16 of actors (DS 0x11a, 10 x 0x46) equal to oldptr by newptr, at most once per actor (flags u8[10] at DS 0x32b4) */
void    sub_07fe_0231(void);                        /* re-links the resource cache DS 0x928 (20 x 6 bytes): reloads every PAK entry (kind 0, ptr!=NULL) from PAK name DSPTR(0x18a0), else takes fixed resource sub_0053_03a5(kind); patches actor pointers via sub_07fe_01d5 */
void    sub_07fe_02df(void);                        /* out-of-memory fallback (called by sub_0053_0413): farfree every cached PAK resource of table DS 0x928 then reload all with sub_07fe_0231 (heap defragmentation) */
void    sub_07fe_032a(uint8_t *buf, int16_t len);   /* savegame (de)obfuscation: buf[i] ^= (uint8_t)(5 + 5*i) for i < len (symmetric) */
void    sub_07fe_035d(void);                        /* applies the loaded save: copies savebuf(DSPTR(0x3294))+0x3e6 -> DS 0x9e..0x166b (0x15ce bytes), restarts music track DS16(0xd6) (sub_0791_0078) */
int16_t sub_07fe_03ab(int16_t slot);                /* loads "<slot>.AVE" into DSPTR(0x3294), checks 16-bit byte sum, decrypts, restores DS 0x1fba block and schedules/does the state switch depending on room DS16(0xa0) now/in save; returns 1 if the checksum was OK, 0 if open failed; bad checksum -> error box loop (sub_07fe_00b2 always 1: never returns) */
int16_t sub_07fe_057e(int16_t slot);                /* saves "<slot>.AVE": purge cache, build 8000-byte image in DSPTR(0x3294) (name[40], DS 0x1fba[0x3e], DS 0x9e[0x15ce]), encrypt, write 0x1f3e bytes + 2-byte checksum; returns 1 on success, 0 if create failed; clears DS16(0x98) */
void    sub_07fe_06de(int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t color); /* draws a bevelled box (lines 0x97/0x9f/0x9a/0x9e/0x99) filled with color into the draw buffer */
void    sub_07fe_0847(void);                        /* reads the 40-byte name header of 0.AVE..9.AVE into the slot table DSPTR(0x32be) (10 x 40); missing file -> name[0]=0xff; disables the fopen error handler DSPTR(0x2d40) meanwhile and reinstalls sub_07fe_00b2 */
void    sub_07fe_08d2(int16_t slot, int16_t editing); /* draws one save-slot row (y = slot*14+18): number box, name box (highlight if slot==DS16(0x1af2)), slot number, name; if editing && selected draws the text cursor after the name */
void    sub_07fe_09dc(int16_t mode);                /* sets text colour 0 and draws slot rows mode..9 (save mode 1 hides slot 0) with editing=mode */
int16_t sub_07fe_0a0b(int16_t mode);                /* edits the name of the selected slot (AZERTY scancode table DS 0x1afe, max 38 chars / 220 px, backspace); returns 2 on Enter, 1 on Esc or up/down (up/down restore the old name) */
void    sub_07fe_0bd5(int16_t mode);                /* save/load slot menu (mode 0 = load, 1 = save): slot table is a 400-byte local published in DSPTR(0x32be); up/down select DS16(0x1af2), Enter/typing confirm, Esc allowed only if hero HP DS16(0x144) > 0 */
void    sub_07fe_0d4c(int16_t x, int16_t filled);   /* draws one 5-pixel volume gauge tick at column x, rows 188..192 (colours 0x33.. if filled else 0x92..) */
void    sub_07fe_0de3(void);                        /* redraws the options panel (bank DSPTR(0x32b0)): background, selected item box, 5 labels (LOAD/SAVE/Music/Sound/Window), music/sound gauges (DS16(0xe8)/8, DS16(0xea)/8), window-size marker DS16(0xe6); presents bottom panel */
void    sub_07fe_0f88(void);                        /* if in 3D outdoor mode (DS16(0xa0)==-1) and window size DS16(0xe6)!=2: clears the 3D view area on screen (sky gradient 100 rows + ground colour 0x55 rows 100..171) */
void    sub_07fe_0fdc(void);                        /* after a window-size change in 3D mode: sub_07fe_0f88 + re-render view sub_0a86_0d96(0) */
void    sub_07fe_0ff1(void);                        /* options panel main loop (loads "a99" #0x37, slides it in/out): Load/Save (-> sub_07fe_0bd5), music/sound volume 0..15, 3D window size 0..2; keys Esc/Enter/L/S */

/* ---- segment 0c05 ---- */
void    sub_0c05_0009(int16_t value, int16_t ref);  /* draws the copy-protection box: "Reference :" day string DSPTR(0x22fe+(ref/12)*4), month DSPTR(0x22ce+(ref%12)*4), and the 3-digit entered value; flips to screen */
void    sub_0c05_00fc(void);                        /* copy protection: picks ref = random(192), answer = word[ref] of PAK "intro" entry 2, reads a 3-digit code (digit keys, Enter ends), sets DS16(0x11c8) = (code == answer) */
void    sub_0c05_01ea(int16_t n);                   /* sets bit n in the flag bitfield u8[13] at DS 0xf6 (script opcode) */
int16_t sub_0c05_020c(void);                        /* returns the number of set bits in the flag bitfield DS 0xf6[0..12] (shown as a 2-digit counter in the status panel) */
void    sub_0c05_0254(int16_t x, int16_t y, int16_t heading, int16_t overlay); /* 3D mode: moves the camera (DS 0x1fc2 x, 0x1fc4 y, s8 0x1fcd heading) linearly to x,y,heading in max(dist/40,|dh|) rendered steps; overlay!=0 draws sprite 0 of DSPTR(0x34ce); decelerates DS16(0x1fc6) */
void    sub_0c05_036f(void);                        /* scripted 3D flight: loads PAK "i2" #0xc into DSPTR(0x34ce), music 0x24, camera at (0xd00,0x1d63,-4), follows 7 waypoints of DS 0x22a4 (x,y,heading), frees, stops music */
void    sub_0c05_0400(void);                        /* 3D mode door check: if camera cell (x/512,y/512) and zone DS16(0x1fd6) match an entry of DS 0x21a4 (20 x {zone,x,y,?}) sets next room DS16(0xa2)=index; special doors DS 0x2244 (3 entries) -> DS 0x2286/0x11bc/0xa2=3/0x329e=7 */
void    sub_0c05_0511(void);                        /* 3D zone change: resets objects (sub_03dd_011f, sub_0053_0112) and spawns up to 8 zone objects from list table DS 0x1bbc (lists zone*8+i) via sub_0936_0106; DS16(0x1fd8) = DS16(0x1fd6) */
void    sub_0c05_056b(void);                        /* 3D random encounter: spawns list zone*16+DS16(0x1fda)+0x28 of DS 0x1bbc at its x,y (words +4,+5) jittered by random(501)-250, then restores the table */
