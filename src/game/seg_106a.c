/* Segment 106a: vector shape renderer (plain / scaled / rotated / morphing) and its primitive handlers. */
#include "../core/mem.h"
#include "../platform/platform.h"
#include "protos.h"

/*
 * Code-segment data of 106a.  The original switched the vertex decoders by patching opcodes
 * (lodsw;nop <-> lodsb;cbw, nop;nop <-> neg ax) at cs:0x4d4.. (sub_106a_0477) and cs:0x559..
 * (sub_106a_04ff).  The patch state is a pure function of the flag byte stored in cs:0xc / cs:0xd,
 * so the decoders simply test these bytes.  Quirks kept:
 *   - sub_106a_0593 updates cs:0xd (it "patches" 04ff's code) but never uses the flags itself;
 *   - the primitive handlers always read cs:0xc (0477's state), whoever called them.
 */
static uint16_t s_vx, s_vy;      /* cs:4, cs:6  rotation temporaries */
static uint16_t s_colB;          /* cs:8  primitive byte 3: fill/line colour, sprite/sub-shape id high */
static uint16_t s_colA;          /* cs:0xa primitive byte 2: outline colour (0xff none), id low */
static uint8_t  s_flags477;      /* cs:0xc decode flags of 0477: 0x10 int8 coords, 0x80 neg x, 0x20 neg y */
static uint8_t  s_flags4ff;      /* cs:0xd decode flags of 04ff */
static int16_t  s_vbuf[500];     /* cs:0x1f transformed vertices (x,y pairs) */

typedef uint8_t *(*prim_fn)(uint8_t *si, uint16_t n, uint8_t *shp);
static uint8_t *sub_106a_0734(uint8_t *si, uint16_t n, uint8_t *shp);
static uint8_t *sub_106a_0774(uint8_t *si, uint16_t n, uint8_t *shp);
static uint8_t *sub_106a_07b8(uint8_t *si, uint16_t n, uint8_t *shp);
static uint8_t *sub_106a_07e3(uint8_t *si, uint16_t n, uint8_t *shp);
static uint8_t *sub_106a_080d(uint8_t *si, uint16_t n, uint8_t *shp);
static uint8_t *sub_106a_084a(uint8_t *si, uint16_t n, uint8_t *shp);
static uint8_t *sub_106a_08a8(uint8_t *si, uint16_t n, uint8_t *shp);

/* cs:0xf near jump table of the primitive handlers, indexed by primitive type */
static const prim_fn s_prim[8] = {
    sub_106a_080d, sub_106a_084a, sub_106a_08a8, sub_106a_08a8,
    sub_106a_0734, sub_106a_0774, sub_106a_07b8, sub_106a_07e3,
};

/* calls primitive handler `type` (types >= 8 jumped through garbage in the original) */
#define CALL_PRIM(type, si, n, shp) \
    do { if ((type) < 8) (si) = s_prim[(type)]((si), (n), (shp)); /* TODO(port): type>=8 undefined */ } while (0)

/* 106a:0407  records the vertex-decode flags of sub_106a_0477 (was: patch its code) */
void sub_106a_0407(uint8_t flags)
{
    s_flags477 = flags;
}

/* 106a:043f  records the vertex-decode flags of sub_106a_04ff (was: patch its code) */
void sub_106a_043f(uint8_t flags)
{
    s_flags4ff = flags;
}

/* 106a:0477  draws vector shape id&0xfff of shape file shp at (x,y); id bits 15/13 mirror x/y */
void sub_106a_0477(uint16_t id, int16_t x, int16_t y, uint8_t *shp)
{
    uint8_t *si = shp + PU16(shp);
    uint8_t dh = (uint8_t)((id >> 8) & 0xa0);
    uint16_t hdr, cnt;

    si += PU16(si + (uint16_t)((id & 0x0fff) * 4));
    hdr = PU16(si);
    si += 2;
    dh |= (uint8_t)hdr;
    cnt = (uint16_t)(hdr >> 8);
    if (cnt == 0)
        return;
    if (dh != s_flags477)
        sub_106a_0407(dh);
    do {
        uint8_t type = si[0];
        uint16_t n = si[1], k;
        int16_t *v = s_vbuf;
        s_colA = si[2];
        s_colB = si[3];
        si += 4;
        if (n != 0) {               /* TODO(port): n==0 read 65536 vertices in the original */
            k = n;
            do {
                int16_t c;
                if (s_flags477 & 0x10) { c = (int8_t)*si; si += 1; }
                else                   { c = P16(si);     si += 2; }
                if (s_flags477 & 0x80) c = (int16_t)-c;
                *v++ = (int16_t)(c + x);
                if (s_flags477 & 0x10) { c = (int8_t)*si; si += 1; }
                else                   { c = P16(si);     si += 2; }
                if (s_flags477 & 0x20) c = (int16_t)-c;
                *v++ = (int16_t)(c + y);
            } while (--k);
        }
        CALL_PRIM(type, si, n, shp);
    } while (--cnt);
}

/* 106a:04ff  draws vector shape id&0xfff scaled by scale/256 at (x,y) */
void sub_106a_04ff(uint16_t id, int16_t x, int16_t y, uint8_t *shp, int16_t scale)
{
    uint8_t *si = shp + PU16(shp);
    uint8_t dh = (uint8_t)((id >> 8) & 0xa0);
    uint16_t hdr, cnt;

    si += PU16(si + (uint16_t)((id & 0x0fff) * 4));
    hdr = PU16(si);
    si += 2;
    dh |= (uint8_t)hdr;
    cnt = (uint16_t)(hdr >> 8);     /* no zero check here in the original */
    if (dh != s_flags4ff)
        sub_106a_043f(dh);
    do {
        uint8_t type = si[0];
        uint16_t n = si[1], k;
        int16_t *v = s_vbuf;
        s_colA = si[2];
        s_colB = si[3];
        si += 4;
        if (n != 0) {               /* TODO(port): n==0 read 65536 vertices in the original */
            k = n;
            do {
                int16_t c;
                if (s_flags4ff & 0x10) { c = (int8_t)*si; si += 1; }
                else                   { c = P16(si);     si += 2; }
                if (s_flags4ff & 0x80) c = (int16_t)-c;
                *v++ = (int16_t)((int16_t)(((int32_t)c * scale) >> 8) + x);
                if (s_flags4ff & 0x10) { c = (int8_t)*si; si += 1; }
                else                   { c = P16(si);     si += 2; }
                if (s_flags4ff & 0x20) c = (int16_t)-c;
                *v++ = (int16_t)((int16_t)(((int32_t)c * scale) >> 8) + y);
            } while (--k);
        }
        CALL_PRIM(type, si, n, shp);
    } while (--cnt);
}

/* 106a:0593  draws vector shape id&0xfff rotated by angle (256 = full turn) at (x,y); int16 coords, no mirroring */
void sub_106a_0593(uint16_t id, int16_t x, int16_t y, uint8_t *shp, uint16_t angle)
{
    uint8_t a = (uint8_t)angle;
    int16_t sn = DS16(0x2d8c + a * 2);                       /* cs:0x640 / cs:0x66a */
    int16_t cs = DS16(0x2d8c + (uint8_t)(a + 0x40) * 2);     /* cs:0x62f / cs:0x65d */
    uint8_t *si = shp + PU16(shp);
    uint8_t dh = (uint8_t)((id >> 8) & 0xa0);
    uint16_t hdr, cnt;

    si += PU16(si + (uint16_t)((id & 0x0fff) * 4));
    hdr = PU16(si);
    si += 2;
    dh |= (uint8_t)hdr;
    cnt = (uint16_t)(hdr >> 8);
    if (dh != s_flags4ff)
        sub_106a_043f(dh);          /* original bug: patches 04ff's decoder, not its own */
    do {
        uint8_t type = si[0];
        uint16_t n = si[1], k;
        int16_t *v = s_vbuf;
        s_colA = si[2];
        s_colB = si[3];
        si += 4;
        if (n != 0) {               /* TODO(port): n==0 read 65536 vertices in the original */
            k = n;
            do {
                int16_t vx = P16(si), vy = P16(si + 2);
                int32_t t;
                si += 4;
                s_vx = (uint16_t)vx;
                s_vy = (uint16_t)vy;
                t = (int32_t)vx * cs - (int32_t)vy * sn;
                *v++ = (int16_t)((int16_t)(t >> 14) + x);
                t = (int32_t)vy * cs + (int32_t)vx * sn;
                *v++ = (int16_t)((int16_t)(t >> 14) + y);
            } while (--k);
        }
        CALL_PRIM(type, si, n, shp);
    } while (--cnt);
}

/* 106a:06a1  draws morph shape #0 of shp at (x,y): vertices a+(b-a)*t/256, colours chosen per primitive by t */
void sub_106a_06a1(uint16_t unused, int16_t x, int16_t y, uint8_t *shp, uint16_t t)
{
    uint8_t *si = shp + PU16(shp);
    uint16_t cnt;
    int16_t thr;

    (void)unused;
    si += PU16(si);
    cnt = (uint16_t)(PU16(si) >> 8);
    si += 2;
    thr = (int16_t)(uint16_t)(((uint32_t)t * cnt) >> 8);
    do {
        uint16_t w0 = PU16(si), wa = PU16(si + 2), wb = PU16(si + 4);
        uint16_t col, n, k;
        uint8_t type;
        int16_t *v = s_vbuf;
        si += 6;
        col = ((int16_t)cnt >= thr) ? wa : wb;
        s_colA = (uint8_t)col;
        s_colB = (uint8_t)(col >> 8);
        type = (uint8_t)w0;
        n = (uint16_t)(w0 >> 8);
        if (n != 0) {               /* TODO(port): n==0 read 65536 vertices in the original */
            k = n;
            do {
                int16_t xa = P16(si), ya = P16(si + 2), xb = P16(si + 4), yb = P16(si + 6);
                si += 8;
                *v++ = (int16_t)((int16_t)(((int32_t)(int16_t)(xb - xa) * (int16_t)t) >> 8) + xa + x);
                *v++ = (int16_t)((int16_t)(((int32_t)(int16_t)(yb - ya) * (int16_t)t) >> 8) + ya + y);
            } while (--k);
        }
        CALL_PRIM(type, si, n, shp);
    } while (--cnt);
}

/* 106a:0734  primitive 4: filled polygon in colB, outlined in colA unless colA == 0xff */
static uint8_t *sub_106a_0734(uint8_t *si, uint16_t n, uint8_t *shp)
{
    sub_0d84_0002(0, 0, (uint8_t *)s_vbuf, n, (uint8_t)s_colB);
    if ((uint8_t)s_colA != 0xff) {
        s_colB = (uint16_t)((s_colB & 0xff00) | (uint8_t)s_colA);
        s_vbuf[n * 2] = s_vbuf[0];
        s_vbuf[n * 2 + 1] = s_vbuf[1];
        sub_106a_07b8(si, (uint16_t)(n + 1), shp);
    }
    return si;
}

/* 106a:0774  primitive 5: box between vertices 0 and 1, filled colB, outlined colA unless 0xff */
static uint8_t *sub_106a_0774(uint8_t *si, uint16_t n, uint8_t *shp)
{
    int16_t ax = s_vbuf[0], bx = s_vbuf[1], cx = s_vbuf[2], dx = s_vbuf[3], t;

    (void)n; (void)shp;
    if (ax > cx) { t = ax; ax = cx; cx = t; }
    if (bx > dx) { t = bx; bx = dx; dx = t; }
    sub_10f5_0028(ax, bx, cx, dx, (uint8_t)s_colB);
    if ((uint8_t)s_colA != 0xff)
        sub_116d_000c(ax, bx, cx, dx, (uint8_t)s_colA);
    return si;
}

/* 106a:07b8  primitive 6: polyline through the n vertices in colB */
static uint8_t *sub_106a_07b8(uint8_t *si, uint16_t n, uint8_t *shp)
{
    int16_t *v = s_vbuf;
    uint16_t cx = (uint16_t)(n - 1);

    (void)shp;
    if (cx == 0)                    /* TODO(port): n==1 drew 65536 segments in the original */
        return si;
    do {
        sub_1040_0006(v[0], v[1], v[2], v[3], (uint8_t)s_colB);
        v += 2;
    } while (--cx);
    return si;
}

/* 106a:07e3  primitive 7: n single points in colB */
static uint8_t *sub_106a_07e3(uint8_t *si, uint16_t n, uint8_t *shp)
{
    int16_t *v = s_vbuf;
    uint16_t cx = n;

    (void)shp;
    sub_1100_00c0((uint8_t)s_colB);
    if (cx == 0)                    /* TODO(port): n==0 plotted 65536 points in the original */
        return si;
    do {
        sub_1100_000b(v[0], v[1]);
        v += 2;
    } while (--cx);
    return si;
}

/* 106a:080d  primitive 0: draws sub-shape colA|(colB|flags477)<<8 at vertex 0 (recursion), then restores 0477's flags */
static uint8_t *sub_106a_080d(uint8_t *si, uint16_t n, uint8_t *shp)
{
    uint8_t saved = s_flags477;
    uint16_t id = (uint16_t)((uint8_t)s_colA | ((uint16_t)(uint8_t)((uint8_t)s_colB | s_flags477) << 8));

    (void)n;
    sub_106a_0477(id, s_vbuf[0], s_vbuf[1], shp);
    if (saved != s_flags477)
        sub_106a_0407(saved);
    return si;
}

/* 106a:084a  primitive 1: RLE sprite colA|(colB^flags477)<<8 from the file's sprite bank at vertex 0 */
static uint8_t *sub_106a_084a(uint8_t *si, uint16_t n, uint8_t *shp)
{
    uint8_t *bank = shp + PU16(shp + 4);
    uint16_t id = (uint16_t)((uint8_t)s_colA | ((uint16_t)(uint8_t)((uint8_t)s_colB ^ s_flags477) << 8));

    (void)n;
    if (s_flags477 & 0x80)
        s_vbuf[0] = (int16_t)(s_vbuf[0] - sub_115f_000c(id, bank));
    if (s_flags477 & 0x20)
        s_vbuf[1] = (int16_t)(s_vbuf[1] + sub_115f_0054(id, bank));
    sub_0fe6_01d2(id, s_vbuf[0], s_vbuf[1], DSPTR(0x2d38), bank);
    return si;
}

/* 106a:08a8  primitives 2 and 3: move the data pointer by (0x100-n)*4 bytes */
static uint8_t *sub_106a_08a8(uint8_t *si, uint16_t n, uint8_t *shp)
{
    (void)shp;
    return si + (uint16_t)((0x100 - n) * 4);
}
