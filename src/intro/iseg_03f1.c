/*
 * Intro segment 03f1: file name helper.
 */
#include "../core/mem.h"
#include "../platform/platform.h"
#include "intro_protos.h"

/* 03f1:0005  dst = src, then appends ext if the name contains no '.' */
void intro_03f1_0005(char *dst, const char *src, const char *ext)
{
    char *p = dst;
    while ((*p++ = *src++) != 0) ;
    p = dst;
    while (*p != 0) {
        if (*p++ == 0x2e) return;
    }
    while ((*p++ = *ext++) != 0) ;
}
