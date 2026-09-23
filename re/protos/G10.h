/* G10 - system layer & Turbo C runtime (implemented natively in src/game/sys_*.c) */
void sub_0000_0000(void); /* C0 startup (replaced by src/main.c) */
_Noreturn void sub_0000_010d(int16_t status); /* _exit */
void sub_0000_012f(void); /* save/hook CPU exception vectors (no-op) */
void sub_0000_0172(void); /* restore CPU exception vectors (no-op) */
void sub_0000_01a7(void); /* write to stderr (startup internal) */
_Noreturn void sub_0000_01af(void); /* abort */
void sub_0000_01d0(void); /* _setargv (no-op) */
void sub_0000_025e(void); /* _setargv helper (no-op) */
void sub_0000_02ce(void); /* _setenvp (no-op) */
void sub_0d7d_000a(void); /* install fps-meter timer hook */
void sub_0d7d_002e(void); /* remove fps-meter timer hook */
void sub_0d7d_0046(void); /* fps-meter timer ISR */
int32_t sub_0e66_0006(int32_t h, int32_t off, int32_t whence); /* lseek (whence -1 SET, 0 CUR, 1 END); 0 ok, -1 error */
int32_t sub_0e66_0031(uint8_t *name, int16_t mode); /* open (0x3ed read, 0x4d2 r/w, 0x3ee create); handle or 0 */
int32_t sub_0e66_0114(int32_t h, uint8_t *buf, int32_t n); /* write, returns bytes written */
int32_t sub_0e66_0191(int32_t h, uint8_t *buf, int32_t n); /* read, returns bytes read */
int32_t sub_0e66_022b(int32_t h); /* close */
int32_t sub_0e9f_0013(int16_t param, uint16_t mask, uint16_t cmd); /* sound driver fn 0x0a: music control */
void sub_0e9f_006f(int16_t effect, int16_t unused, uint16_t cmd); /* play/stop sound effect n */
void sub_0e9f_00c1(void); /* sound driver init */
void sub_0e9f_0116(void); /* sound driver timer tick callback */
void sub_0e9f_011f(void); /* sound driver shutdown */
void sub_0e9f_017e(int16_t unused, uint8_t *bank); /* set SFX bank */
void sub_0e9f_01a8(int16_t unused, uint8_t *song); /* load + start song */
void sub_0ebb_01d2(uint8_t scancode); /* keyboard ISR (one raw scancode) */
void sub_0ebb_0301(void); /* install keyboard handler */
void sub_0ebb_0363(void); /* remove keyboard handler */
void sub_0ebb_0372(void); /* timer callback: virtual cursor */
void sub_0ebb_037f(uint8_t dir); /* virtual cursor move */
void sub_0ebb_03aa(void); /* virtual cursor clamp */
void sub_0efe_0030(void); /* empty timer hook */
void sub_0efe_0031(void); /* program PIT from DS16(0x2d14) */
void sub_0efe_004b(void); /* install timer */
void sub_0efe_0158(void (*fn)(void)); /* register timer callback */
void sub_0efe_016f(void (*fn)(void)); /* register timer callback (duplicate) */
void sub_0efe_0186(void (*fn)(void)); /* register helper */
void sub_0efe_01c0(void (*fn)(void)); /* unregister timer callback */
void sub_0efe_01d5(void (*fn)(void)); /* unregister helper */
void sub_0efe_0202(void); /* uninstall timer */
uint8_t *sub_0f27_0004(int32_t size); /* farmalloc; farmalloc(-1) returns farcoreleft() cast to a pointer-sized number: use sys_farcoreleft() instead */
int16_t sub_0f27_0061(uint8_t *p); /* farfree, returns -1 */
int32_t sub_0f27_0079(uint8_t *p, int32_t size); /* resize block in place, -1 ok */
void sub_0f32_000d(uint8_t *dst, uint8_t *name, uint8_t *ext); /* strcpy + add extension if none */
void sub_0f48_0029(void); /* explode helper (internal) */
void sub_0f48_02ab(void); /* explode helper (internal) */
void sub_0f48_0314(uint16_t flags, uint8_t *src, uint8_t *dst, int32_t outlen, uint8_t *work); /* PKWARE explode */
uint8_t sub_0fa2_0008(uint8_t *name, int16_t index, uint8_t *dst); /* load PAK entry into dst; 1 ok */
void sub_118f_0004(void); /* video init */
void sub_118f_00cd(void); /* restore video mode */
void sub_118f_00de(void); /* wait vertical retrace */
void sub_118f_00f7(void); /* clear back buffer */
void sub_118f_0107(void); /* present back buffer to screen */
void sub_118f_01ab(uint8_t *rgb, int16_t first, int16_t count); /* set DAC from 8-bit RGB */
void sub_11b2_0001(void); /* system init: back buffer, video, timer */
void sub_11b2_0043(void); /* system shutdown */
int16_t sub_11b7_000d(int16_t doserr); /* __IOerror */
void sub_11bb_0008(void); /* empty exit hook */
_Noreturn void sub_11bb_0009(int16_t status); /* exit */
uint8_t *sub_11c1_000b(uint16_t n); /* malloc */
void sub_11c1_0020(void); /* RTL heap internal */
void sub_11c1_0088(void); /* RTL heap internal */
void sub_11c1_0138(void); /* RTL heap internal */
void sub_11c1_01a6(void); /* RTL heap internal */
uint8_t *sub_11c1_020c(uint32_t n); /* RTL farmalloc */
void sub_11f2_000b(void); /* __brk */
void sub_11f2_00e2(void); /* __sbrk */
int16_t sub_1207_000d(uint16_t seg, uint16_t paras); /* setblock */
void sub_1209_000d(uint8_t *dst, uint16_t len, uint8_t val); /* setmem (TC order) */
void sub_120e_0002(uint8_t *src, uint8_t *dst, uint16_t len); /* movmem: SOURCE FIRST */
int16_t sub_1215_0017(void); /* rand() */
/* native helpers (no original address) */
int32_t sys_farcoreleft(void); /* = farmalloc(-1L) in the original */
uint8_t *sys_launcher_config(void); /* launcher parameter block (CONFIG.TAT), = far ptr at 0000:025c */
