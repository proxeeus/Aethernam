/* Segment 0f3a: integer to decimal string (static buffer DS:0x2d1e). */
#include "../core/mem.h"
#include "../platform/platform.h"
#include "protos.h"

/* 0f3a:0004  converts a signed int to decimal text in DS:0x2d1e and returns that buffer */
uint8_t *sub_0f3a_0004(int16_t v)
{
    uint8_t *p = DSADDR(0x2d1e);
    uint16_t u = (uint16_t)v;
    int16_t started = 0;

    if (u & 0x8000) {
        *p++ = '-';
        u = (uint16_t)-u;
    }
    u = sub_0f3a_00d3(u, 10000, &started, &p);
    u = sub_0f3a_00d3(u, 1000, &started, &p);
    u = sub_0f3a_00d3(u, 100, &started, &p);
    u = sub_0f3a_00d3(u, 10, &started, &p);
    *p++ = (uint8_t)((uint8_t)u + '0');
    *p = 0;
    return DSADDR(0x2d1e);
}

/* 0f3a:0052  (dead code) converts a signed long (< 655360 in magnitude) to decimal text in DS:0x2d1e, else "*Overflow*" */
static __attribute__((unused)) uint8_t *sub_0f3a_0052(int32_t v)
{
    uint8_t *p = DSADDR(0x2d1e);
    uint16_t lo = (uint16_t)v, hi = (uint16_t)((uint32_t)v >> 16);
    int16_t started = 0;
    uint32_t n;
    uint16_t q;
    uint8_t rem;

    if (hi & 0x8000) {
        uint32_t neg = (uint32_t)0 - (((uint32_t)hi << 16) | lo);
        *p++ = '-';
        lo = (uint16_t)neg;
        hi = (uint16_t)(neg >> 16);
    }
    if (hi > 9)
        return DSADDR(0x2d2c);
    n = ((uint32_t)hi << 16) | lo;
    q = (uint16_t)(n / 10);
    rem = (uint8_t)(n % 10);
    q = sub_0f3a_00d3(q, 10000, &started, &p);
    q = sub_0f3a_00d3(q, 1000, &started, &p);
    q = sub_0f3a_00d3(q, 100, &started, &p);
    q = sub_0f3a_00d3(q, 10, &started, &p);
    if ((uint8_t)q != 0 || started)
        *p++ = (uint8_t)((uint8_t)q + '0');
    *p++ = (uint8_t)(rem + '0');
    *p = 0;
    return DSADDR(0x2d1e);
}

/* 0f3a:00d3  near helper: emits digit v/div unless it is a leading zero; returns v%div */
uint16_t sub_0f3a_00d3(uint16_t v, uint16_t div, int16_t *started, uint8_t **out)
{
    uint16_t q = (uint16_t)(v / div);
    if ((uint8_t)q != 0 || *started) {
        *started = 1;
        *(*out)++ = (uint8_t)((uint8_t)q + '0');
    }
    return (uint16_t)(v % div);
}
