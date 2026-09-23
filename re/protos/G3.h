/* G3 - code segment 03dd: room script (bytecode) interpreter.
 * All functions are FAR (retf); none are near.  Intra-segment calls use push cs/call.
 * Opcode handlers are reached ONLY through the far fn-ptr table DSPTR(0x1946 + op*4)
 * (0x59 entries, ops 0x00..0x58) and share the signature  void (*)(void).
 * The sub-table DSPTR(0x1a8e + i*4) (7 entries, i=0..6) used by op 0x33 aliases
 * entries 0x52..0x58 of the 0x1946 table (same functions, also void (*)(void)).
 * "ctx" = current script context = DSPTR(0x305a) (points at DS:0x7b2 + n*0x16).
 * "IP"  = FP(ctx + 4).  "actor" = DS:0x11a + i*0x46.  "hero" = actor 0.
 * "x2 byte" = byte operand doubled by sub_03dd_0213 (X coordinates stored /2). */

/* ---- loading / init / helpers ---- */
void     sub_03dd_0008(uint8_t *filename, int16_t index, uint8_t *dest, int32_t size); /* opens filename, reads long offset table, seeks to table[index], reads size bytes into dest, closes (PAK/CC4 entry loader) */
uint8_t *sub_03dd_00ac(int16_t actor);            /* returns pointer to actor struct DS:0x11a + actor*0x46 */
void     sub_03dd_00bc(uint16_t n);               /* inits script context n: start=IP=scriptbuf+wordtab[n], actor=0, flags=0x20 (stopped), +0xa=+0xc=0 */
void     sub_03dd_011f(void);                     /* clears field +0x2a of actors 1..9 */
void     sub_03dd_014c(void);                     /* resets script-driven room globals (effect mode, zone tables counts, control lock...) */
void     sub_03dd_017d(void);                     /* sets script count [0xd8]=wordtab[0]/2, inits all contexts, enables script 0, frees temp buf, runs script 0 once */
void     sub_03dd_01c8(void);                     /* loads room script (R??.CC4 entry [0xce]) into [0x177e] (0x898 bytes); fresh start -> 017d, savegame load -> sub_07fe_015c */
uint8_t  sub_03dd_01fb(void);                     /* fetches next bytecode byte (IP++) */
uint16_t sub_03dd_0213(void);                     /* fetches next bytecode byte and returns it *2 (X coordinate) */
int16_t  sub_03dd_022f(void);                     /* fetches next bytecode 16-bit LE word (IP+=2) */
void     sub_03dd_024e(void);                     /* skips one bytecode byte (IP++; used to consume the opcode) */
void     sub_03dd_0257(void);                     /* relative jump: IP += (int16) word at IP (offset relative to the word itself) */
uint8_t  sub_03dd_026b(int16_t a, int16_t b, uint16_t op); /* compare: op 0 ==,1 !=,2 >,3 >=,4 <,5 <=,6 a&b,7 a|b (signed); returns 1/0; op>7 -> 0 */
uint8_t *sub_03dd_02f0(int16_t var);              /* script variable address: var<0x46 -> DSPTR(0x166e+var*4); var<0x164 -> DS:0x11bc+(var-0x46)*2; else DS:0xfbc+(var-0x164)*2 */
int16_t  sub_03dd_034c(void);                     /* fetches a variable index word from bytecode */
int16_t  sub_03dd_035b(void);                     /* fetches a variable index word and returns that variable's value */
int16_t  sub_03dd_0380(void);                     /* fetches tagged immediate: tag byte; tag==0x0c -> imm8 (zero-ext), else imm16 */
int16_t  sub_03dd_03da(void);                     /* fetches operand: if byte at IP==0x0b -> skip it, variable value (035b); else immediate (0380) */

/* ---- opcode handlers (table 0x1946) and effect sub-table (0x1a8e) ---- */
void     sub_03dd_0407(void);                     /* op 0x53 / effect 1: [0x10c]=1 (transition mode 1); does NOT advance IP */
void     sub_03dd_040e(void);                     /* op 0x56 / effect 4: [0x10c]=4, set palette #[0x3036] from palette bank (sub_0791_0187) */
void     sub_03dd_0420(void);                     /* op 0x54 / effect 2: [0x10c]=2 */
void     sub_03dd_0427(void);                     /* op 0x55 / effect 3: [0x10c]=3, sub_0791_020d([0x3036],[0x3038],50,500) (rect-list effect) */
void     sub_03dd_0443(void);                     /* op 0x57 / effect 5: [0x17e2]=1 */
void     sub_03dd_044a(void);                     /* op 0x58 / effect 6: sub_0596_02a0(0x49), draws "The End" at ([0x3036],[0x3038]) into draw buffer */
void     sub_03dd_0471(void);                     /* op 0x20: var = operand          [20][var16][operand] */
void     sub_03dd_04a3(void);                     /* op 0x21: var += operand */
void     sub_03dd_04d5(void);                     /* op 0x38: var |= operand */
void     sub_03dd_0507(void);                     /* op 0x39: var &= operand */
void     sub_03dd_0539(void);                     /* op 0x22: var -= operand */
void     sub_03dd_056b(void);                     /* op 0x23: own actor +0x25 = 8 */
void     sub_03dd_058b(void);                     /* op 0x0a: IF  [0a][var16][cmpop8][operand][rel16]: if !(var cmp operand) jump else skip rel16 */
void     sub_03dd_05d4(void);                     /* op 0x52 / effect 0: if [0x10c]==4 restore palette DSPTR(0x304c); [0x10c]=0 (also called from 014c) */
void     sub_03dd_05f2(void);                     /* ops 0x0b,0x0c,0x0d,0x31: empty (operand tags, never executed; would not advance IP) */
void     sub_03dd_05f3(void);                     /* op 0x00: [anim8] play animation on own actor once (+0x10=2), wait flag 0x04, yield */
void     sub_03dd_0643(void);                     /* op 0x51: wait until mouse/keys released, then until key/mouse pressed (spins rand()) - BUSY WAIT */
void     sub_03dd_0679(void);                     /* op 0x2d: [anim8][n8] own actor: +0x1a=anim,+0x20=0,+0x10=2,+0x22=n,+0x1c=n */
void     sub_03dd_06d1(void);                     /* op 0x2e: [n8] play sound effect n (sub_0791_00e5) */
void     sub_03dd_06ee(void);                     /* op 0x48: [n8] play music n (sub_0791_0078), 0xff = stop music */
void     sub_03dd_0717(void);                     /* op 0x49: [v8] set music volume (sub_0791_0005) */
void     sub_03dd_0734(void);                     /* op 0x4a: [n8] [0x304a] = random(n) (script var 41) */
void     sub_03dd_074a(void);                     /* op 0x2f: [n8] [0x3268] = sprite bank parameter for next op 0x16 */
void     sub_03dd_0758(void);                     /* op 0x33: [fx8][x2 byte][y8] [0x3036]=x,[0x3038]=y, call effect table DSPTR(0x1a8e+fx*4) */
void     sub_03dd_0787(void);                     /* op 0x34: [v8] [0x177c]=v; if it went nonzero->0 set [0x1738]=1 */
void     sub_03dd_07b2(void);                     /* op 0x35: choice menu: [q16][x2 byte][y8][n8] n*{[text16][rel16]} -> tables 0x306a, flag 0x10, yield */
void     sub_03dd_088c(void);                     /* op 0x37: [0x173e]=1 */
void     sub_03dd_0897(void);                     /* op 0x3a: [a8][b8] [0xbbc]=a,[0xbbe]=b, sub_0596_0001 (allocate work buffer) */
void     sub_03dd_08b3(void);                     /* op 0x3b: [v8] byte [0xc4]=v */
void     sub_03dd_08bf(void);                     /* op 0x32: no-op with 3 operand bytes (skips 4 bytes) */
void     sub_03dd_08d0(void);                     /* op 0x30: own actor x=hero x, y=hero y+1 */
void     sub_03dd_0915(void);                     /* op 0x01: [dir8] own actor +0x10=0, set facing dir (sub_0053_0e27) */
void     sub_03dd_0956(void);                     /* op 0x15: own actor faces the hero (dir from sub_0053_0e77) */
void     sub_03dd_09b7(void);                     /* op 0x17: [n8] n*{[x2 byte][y8]} -> word list at 0x1832 (count,x,y..), sub_0053_0cc4 (walk polygon) */
void     sub_03dd_0a13(void);                     /* op 0x18: [n8] n*{[b8][b8][x2 byte][x2 byte]} -> 6-byte entries at 0x752, [0xa8]=n */
void     sub_03dd_0a77(void);                     /* op 0x19: [mode8] 5 words (b,b,b,x2,b) (+5 more if mode==2) -> 0xbc0, [0xc6]=mode */
void     sub_03dd_0b3c(int16_t wide);             /* helper for 0x1a/0x4f: append entry to 0xa18[[0xf0]++]: id(byte or word if wide),0,wide,x2 byte,byte */
void     sub_03dd_0ba9(void);                     /* op 0x1a: sub_03dd_0b3c(0) */
void     sub_03dd_0bb2(void);                     /* op 0x4f: sub_03dd_0b3c(1) */
int16_t  sub_03dd_0bbb(void);                     /* returns 1 if the opcode at IP is 0x1f or 0x51 (used by dialog code sub_0596_0f41) */
void     sub_03dd_0bf5(void);                     /* op 0x1f: dialog: [a8][b8][text16][d8][n8] n*3 bytes; [0x328c]=n, DSPTR(0x32a2)=IP, IP+=3n, sub_0596_0f41(a,b,text,d) */
void     sub_03dd_0c5b(void);                     /* op 0x29: if own actor!=0: +0x2a=0 and release its sprite (sub_0053_01e8) if loaded */
void     sub_03dd_0ca5(void);                     /* op 0x2a: [x2 byte][x2 byte] own actor +0x3c,+0x3e */
void     sub_03dd_0cdd(void);                     /* op 0x2b: [a8][b8] own actor bytes +0x40,+0x41 */
void     sub_03dd_0d15(void);                     /* op 0x2c: [v8] [0xd0]=v (0xff -> -1) */
void     sub_03dd_0d3a(void);                     /* op 0x4b: [room8] [0xa2]=[0x32a6]=room (request room change) */
void     sub_03dd_0d4b(void);                     /* op 0x1b: sub_0c05_00fc() */
void     sub_03dd_0d55(void);                     /* op 0x02: stop this script (flag 0x20), yield (IP not advanced) */
void     sub_03dd_0d65(void);                     /* op 0x1c: [n8] if n<[0xd8]: restart script n and enable it */
void     sub_03dd_0d9c(void);                     /* op 0x1d: [n8] stop script n (flag 0x20), no bounds check */
void     sub_03dd_0dc3(void);                     /* op 0x1e: list of bytes until 0xff: restart+enable each script */
void     sub_03dd_0e01(void);                     /* op 0x03: GOTO [rel16] */
void     sub_03dd_0e0a(void);                     /* op 0x43: ctx loop counter (+0xc)=0, then GOTO */
void     sub_03dd_0e19(void);                     /* op 0x24: [x2 byte][y8] own actor walks to (x,y), wait flag 0x01, yield */
void     sub_03dd_0e78(int16_t vert);             /* helper for 0x04/0x05: [c8][anim8] walk own actor to x=c*2 (vert=0) or y=c (vert=1), wait 0x01, yield */
void     sub_03dd_0f38(void);                     /* op 0x04: sub_03dd_0e78(0) walk horizontally */
void     sub_03dd_0f41(void);                     /* op 0x05: sub_03dd_0e78(1) walk vertically */
void     sub_03dd_0f4a(int16_t vert);             /* helper for 0x0f/0x10: [anim8] walk own actor to hero x (vert=0) or hero y (vert=1), wait 0x01, yield */
void     sub_03dd_0ffd(void);                     /* op 0x0f: sub_03dd_0f4a(0) */
void     sub_03dd_1006(void);                     /* op 0x10: sub_03dd_0f4a(1) */
void     sub_03dd_100f(void);                     /* op 0x11: [dx s8][dy s8] walk own actor to hero pos+(dx,dy) (clipped by sub_0714_0160), +0x28=6, wait 0x01, yield */
void     sub_03dd_10ab(void);                     /* op 0x42: own actor stance 8 (sub_0053_0e52), +0x14=1 */
void     sub_03dd_10e1(void);                     /* op 0x45: [v8] own actor +0x14=v */
void     sub_03dd_110e(void);                     /* op 0x4c: [x2 byte][y8][x2 byte][y8] [0xdc]=1,[0xde]=x1,[0xe2]=y1,[0xe0]=x2,[0xe4]=y2 */
void     sub_03dd_1139(void);                     /* op 0x46: [v8] [0xac]=v */
void     sub_03dd_1147(int16_t param);            /* switch to special scene 8: [0xa2]=[0x32a6]=8, [0xd0]=[0x329e]=0, var[0x11c4]=param, [0xc0]=0xf, hero dir=1 */
void     sub_03dd_116f(void);                     /* op 0x47: [p8] sub_03dd_1147(p), [0x173e]=1, yield */
void     sub_03dd_1197(void);                     /* op 0x06: LOOP [rel16][count8]: ++ctx+0xc < count -> jump; else counter=0, skip 3 */
void     sub_03dd_11da(void);                     /* op 0x07: [x2 byte][y8] place own actor at (x,y) (sub_0053_1c18) */
void     sub_03dd_120c(void);                     /* op 0x08: [n8] sync with script n: ctx+0xa=n, flag 0x08, yield */
void     sub_03dd_122a(void);                     /* op 0x0e: IF hero in zone [x2 byte][y8][x2 byte][y8][rel16] (zone saved in ctx+0xe..0x14), else jump */
void     sub_03dd_1331(void);                     /* op 0x3d: if [0x327a]: clear it, skip rel16; else jump */
void     sub_03dd_1352(void);                     /* op 0x3e: [x2 byte][y8][x2 byte][y8] append rect to 0x30ae[[0x1740]++] (stored x1,y1,y2,x2) */
void     sub_03dd_13a6(void);                     /* op 0x44: [x2 byte][y8][x2 byte][y8] [0x32a0]=1, [0x3020..0x3026]=x1,y1,x2,y2 */
void     sub_03dd_13d1(void);                     /* op 0x3c: 4 bytes appended to 4-byte table 0x1286[[0x1284]++] */
void     sub_03dd_1434(void);                     /* op 0x3f: [a8][b8] for each 0x1286 entry with e[0]==a && e[1]==b: e[2]=e[0] */
uint8_t  sub_03dd_14a2(int16_t x1, int16_t y1, int16_t x2, int16_t y2); /* returns 1 if hero bounding box overlaps rect (sub_0e9b_000c) */
void     sub_03dd_156c(void);                     /* op 0x27: wait while hero is inside the zone saved in ctx (yield w/o advancing), else IP++ */
void     sub_03dd_1598(void);                     /* op 0x28: [zone 4 bytes] saves zone; if hero not inside: IP-=5 and yield (wait for hero to enter) */
void     sub_03dd_1608(void);                     /* op 0x12: [b8] b==0: [0x3250]=0,[0xc0]=0xf; else [0xc0] |= word table DS:0x1aaa[b] (player control lock) */
void     sub_03dd_1640(void);                     /* [0xc0]=0 (unlock); if hero +0x10==2 set it to 0 */
void     sub_03dd_1654(void);                     /* op 0x13: sub_03dd_1640 */
uint8_t *sub_03dd_165d(int16_t idx, uint8_t *text); /* text bank lookup: finds the idx-th '*' (0-based) and returns pointer just past the following NUL */
void     sub_03dd_169e(int16_t actor, int16_t textidx, int16_t color); /* shows speech text textidx of bank DSPTR(0x325a) for actor; [0x17bc]=1, [0x325e]=color */
void     sub_03dd_16d3(void);                     /* op 0x40: [c8] own actor +0x2c = text colour */
void     sub_03dd_16f8(int16_t actor, int16_t textidx, int16_t anim); /* actor says text, plays talk anim (0xff=none) saving old anim in [0x9a]/[0x32a8]/[0x329c]; wait flag 0x40, yield */
void     sub_03dd_1775(void);                     /* op 0x14: SAY [text16][anim8] by own actor */
void     sub_03dd_17a6(void);                     /* op 0x4e: [0x17ac]=1 (end/quit request), yield */
void     sub_03dd_17b7(void);                     /* op 0x4d: SAY [actor8][text16][anim8] */
void     sub_03dd_17ea(void);                     /* op 0x36: [text16][t8] show text by own actor without waiting; [0x328a]=[0x32aa]=t */
void     sub_03dd_1843(void);                     /* op 0x41: [v8] [0x9c]=v */
void     sub_03dd_1851(void);                     /* op 0x26: [v8] if [0xa0]==0: [0xd4]=v>>4; [0xd2]=(v&15)+1 */
void     sub_03dd_187e(void);                     /* wait handler flag 0x08: if partner script ctx[+0xa] also waits (flag 8) for us, clear both; else yield */
void     sub_03dd_18d7(void);                     /* op 0x09: [n8] WAIT n ticks: ctx+0xa=n, flag 0x02, yield */
void     sub_03dd_18f5(void);                     /* op 0x50: [n8] set game flag bit n (sub_0c05_01ea) */
void     sub_03dd_1908(void);                     /* op 0x16: [a8][spr8] bind script to actor a; a!=0: load sprite (bank [0x3268],spr) and init actor; a==0: hero colour 0x19; [0x3268]=0 */
void     sub_03dd_1969(void);                     /* op 0x25: [0xee]=1, [0xd2]=0 */
void     sub_03dd_1979(void);                     /* wait handler flag 0x04: clears flag when own actor frame +0x1c == +0x20-1, else yield */
void     sub_03dd_19b8(void);                     /* wait handler flag 0x01: clears flag when own actor stopped walking ((+0x30&3)==0 or +0x2a==0), else yield */
void     sub_03dd_19fb(void);                     /* wait handler flag 0x02: decrements ctx+0xa; clears flag at 0, else yield */
void     sub_03dd_1a23(void);                     /* wait handler flag 0x40: when text finished ([0x17bc]==0) clear flag, restore saved talk anim; else yield */
void     sub_03dd_1a7f(void);                     /* wait handler flag 0x10: when menu done ([0x3240]==0) IP = chosen entry ptr (0x306a[[0x3274]]+6), relative jump; else yield */
void     sub_03dd_1ac0(uint16_t n);               /* runs script n for one tick: sets ctx, runs wait handlers, then dispatches opcodes via 0x1946 until [0x328e] (yield) */
void     sub_03dd_1b6d(void);                     /* runs scripts 1..[0xd8]-1 for one tick (per-frame call from sub_0053_1e8e) */
