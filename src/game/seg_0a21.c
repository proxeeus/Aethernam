/*
 * Segment 0a21 - vector-shape morph builder.
 *
 * Shape resource: +0 u16 offset of the frame table; frame table entries are 4 bytes whose first
 * word is the frame offset relative to the table.  Frame: u8 flags (bit0: coordinates are s8,
 * else s16), u8 polygon count, then polygons: u8 colour, u8 point count, u16 param, points (x,y).
 *
 * The output is itself a one-frame shape: {4,0,4,0}, frame at +8 (flags 0, polygon count at +9),
 * each polygon = colour, point count, paramA, paramB, then per point {xA,yA,xB,yB} (int16).
 */
#include "../core/mem.h"
#include "../platform/platform.h"
#include "protos.h"

/* DGROUP state used by the builder */
#define M_COLOR_A   DS8(0x32e0)     /* colour of current polygon A */
#define M_COLOR_B   DS8(0x32e1)     /* colour of current polygon B */
#define M_BUF_A     DSADDR(0x32e2)  /* scratch copy of polygon A: 4-byte header + int16 points */
#define M_BUF_B     DSADDR(0x33aa)  /* scratch copy of polygon B */
#define M_NPOLY_A   DS8(0x3472)     /* polygon count of frame A */
#define M_NPOLY_B   DS8(0x3478)     /* polygon count of frame B */
#define M_BYTES_A   DS8(0x347e)     /* frame A has s8 coordinates */
#define M_BYTES_B   DS8(0x347f)     /* frame B has s8 coordinates */
#define M_NPT_A     DS8(0x3480)     /* point count of polygon A */
#define M_NPT_B     DS8(0x3481)     /* point count of polygon B */
#define M_POLY      DS8(0x3482)     /* polygon loop index */
#define M_OUTPOLY   DS8(0x3483)     /* output polygon count */
#define M_PT        DS8(0x3484)     /* point loop index */
#define M_MINPOLY   DS8(0x348e)
#define M_MAXPOLY   DS8(0x348f)
#define M_MINPT     DS8(0x3490)
#define M_MAXPT     DS8(0x3491)

/* copies one 4-byte record (point or header) */
#define COPY4(dst, src) (PU32(dst) = PU32(src))

/* 0a21:0005 - builds a morph shape between frame frameA of shapeA at (xA,yA) and frame frameB of
 * shapeB at (xB,yB): polygons are paired by index and points interleaved as {A point, B point} */
void sub_0a21_0005(int16_t frameA, int16_t xA, int16_t yA, uint8_t *shapeA,
                   int16_t frameB, int16_t xB, int16_t yB, uint8_t *shapeB, uint8_t *out)
{
    /* cursors DS 0x3474 (a), 0x347a (b), 0x3486 (saved) of the original, kept as native pointers */
    uint8_t *a;
    uint8_t *b;
    uint8_t *saved;

    /* shape header: frame table at +4, frame 0 at table+4 */
    PU16(out + 2) = 0;
    PU16(out + 0) = 4;
    out += 4;
    PU16(out + 2) = 0;
    PU16(out + 0) = 4;
    out += 4;
    DSPTR_SET(0x348a, out);
    PU16(out) = 0;
    out += 2;

    /* locate the frames */
    shapeA += PU16(shapeA);
    shapeB += PU16(shapeB);
    shapeA += PU16(shapeA + (uint16_t)(frameA << 2));
    shapeB += PU16(shapeB + (uint16_t)(frameB << 2));

    M_BYTES_A = P8(shapeA) & 1;
    shapeA++;
    M_BYTES_B = P8(shapeB) & 1;
    shapeB++;
    M_NPOLY_A = P8(shapeA);
    shapeA++;
    M_NPOLY_B = P8(shapeB);
    shapeB++;

    if (M_NPOLY_A < M_NPOLY_B) {
        M_MINPOLY = M_NPOLY_A;
        M_MAXPOLY = M_NPOLY_B;
    } else {
        M_MINPOLY = M_NPOLY_B;
        M_MAXPOLY = M_NPOLY_A;
    }

    M_OUTPOLY = 0;
    for (M_POLY = 0; M_POLY < M_MAXPOLY; M_POLY++) {
        M_OUTPOLY++;

        /* read polygon headers (always) and points (while the frame has polygons left) */
        a = M_BUF_A;
        b = M_BUF_B;
        COPY4(a, shapeA);
        a += 4;
        COPY4(b, shapeB);
        b += 4;
        M_COLOR_A = P8(shapeA);
        shapeA++;
        M_COLOR_B = P8(shapeB);
        shapeB++;
        M_NPT_A = P8(shapeA);
        shapeA++;
        shapeA += 2;
        M_NPT_B = P8(shapeB);
        shapeB++;
        shapeB += 2;

        if (M_POLY < M_NPOLY_A) {
            if (M_BYTES_A != 0) {
                for (M_PT = 0; M_PT < M_NPT_A; M_PT++) {
                    P16(a) = PS8(shapeA) + xA;
                    shapeA++;
                    a += 2;
                    P16(a) = PS8(shapeA) + yA;
                    shapeA++;
                    a += 2;
                }
            } else {
                for (M_PT = 0; M_PT < M_NPT_A; M_PT++) {
                    P16(a) = P16(shapeA) + xA;
                    shapeA += 2;
                    a += 2;
                    P16(a) = P16(shapeA) + yA;
                    shapeA += 2;
                    a += 2;
                }
            }
        }

        if (M_POLY < M_NPOLY_B) {
            if (M_BYTES_B & 1) {
                for (M_PT = 0; M_PT < M_NPT_B; M_PT++) {
                    P16(b) = PS8(shapeB) + xB;
                    shapeB++;
                    b += 2;
                    P16(b) = PS8(shapeB) + yB;
                    shapeB++;
                    b += 2;
                }
            } else {
                for (M_PT = 0; M_PT < M_NPT_B; M_PT++) {
                    P16(b) = P16(shapeB) + xB;
                    shapeB += 2;
                    b += 2;
                    P16(b) = P16(shapeB) + yB;
                    shapeB += 2;
                    b += 2;
                }
            }
        }

        /* emit */
        a = M_BUF_A;
        b = M_BUF_B;
        if (M_NPT_A < M_NPT_B) {
            M_MINPT = M_NPT_A;
            M_MAXPT = M_NPT_B;
        } else {
            M_MINPT = M_NPT_B;
            M_MAXPT = M_NPT_A;
        }

        if (M_POLY < M_MINPOLY) {
            if (M_COLOR_A == M_COLOR_B) {
                /* same colour: one polygon, the shorter one repeats its last point */
                COPY4(out, a);
                a += 4;
                out += 4;
                b += 4;
                P8(out - 3) = M_MAXPT;
                PU16(out) = PU16(b - 2);
                out += 2;
                for (M_PT = 0; M_PT < M_MAXPT; M_PT++) {
                    if (M_PT < M_MINPT) {
                        COPY4(out, a);
                        a += 4;
                        out += 4;
                        COPY4(out, b);
                        b += 4;
                        out += 4;
                    } else if (M_NPT_A == M_MINPT) {
                        COPY4(out, a - 4);
                        out += 4;
                        COPY4(out, b);
                        b += 4;
                        out += 4;
                    } else {
                        COPY4(out, a);
                        a += 4;
                        out += 4;
                        COPY4(out, b - 4);
                        out += 4;
                    }
                }
            } else {
                /* different colours: A collapses onto B's first point, B grows from A's first */
                COPY4(out, a);
                a += 4;
                out += 4;
                PU16(out) = PU16(a - 2);
                out += 2;
                saved = a;
                for (M_PT = 0; M_PT < M_NPT_A; M_PT++) {
                    COPY4(out, a);
                    a += 4;
                    out += 4;
                    COPY4(out, b + 4);
                    out += 4;
                }
                COPY4(out, b);
                b += 4;
                out += 4;
                PU16(out) = PU16(b - 2);
                out += 2;
                for (M_PT = 0; M_PT < M_NPT_B; M_PT++) {
                    COPY4(out, saved);
                    out += 4;
                    COPY4(out, b);
                    b += 4;
                    out += 4;
                }
                M_OUTPOLY++;
            }
        } else if (M_NPOLY_A == M_MINPOLY) {
            /* frame A has no polygon here: B grows from its first point */
            COPY4(out, b);
            b += 4;
            out += 4;
            PU16(out) = PU16(b - 2);
            out += 2;
            saved = b;
            for (M_PT = 0; M_PT < M_NPT_B; M_PT++) {
                COPY4(out, saved);
                out += 4;
                COPY4(out, b);
                b += 4;
                out += 4;
            }
        } else {
            /* frame B has no polygon here: A shrinks to its first point */
            COPY4(out, a);
            a += 4;
            out += 4;
            PU16(out) = PU16(a - 2);
            out += 2;
            saved = a;
            for (M_PT = 0; M_PT < M_NPT_A; M_PT++) {
                COPY4(out, a);
                a += 4;
                out += 4;
                COPY4(out, saved);
                out += 4;
            }
        }
    }

    P8(DSPTR(0x348a) + 1) = M_OUTPOLY;
}
