/*
 * Intro segment 03c1: timer (IRQ0) handler with up to 6 far callbacks.
 * The int 8 handler becomes a C function registered with plat_set_timer_isr(); the PIT
 * rate is DS16(0x138) = 0x4dae (1193182 / 19886 = 60 Hz).
 *
 * Code-segment variables: cs:[0] saved SS, cs:[2] old int 8 vector, cs:[6] PIT divisor to
 * restore (0xffff), cs:[8..0x1f] the 6 callback slots, cs:[0x20] BIOS chain counter,
 * cs:[0xcf] re-entrancy flag, cs:[0x142] chain ratio (patched immediate).
 * DGROUP: 0x138 PIT divisor, 0x13a fptr user hook (= 03c1:0022, a retf), 0x172 retrace
 * test result, 0x174 != 0 suspends the callbacks.  SS:0x190 tick counter.
 */
#include "../core/mem.h"
#include "../platform/platform.h"
#include "intro_protos.h"

static intro_cb_t s_cb[6 + 1];                  /* cs:8..0x1f (+ the overflow slot cs:0x20) */
static uint16_t   s_restore_div;                /* cs:[6] */
static uint8_t    s_busy;                       /* cs:[0xcf] */
static uint8_t    s_chain_ratio;                /* cs:[0x142] */

/* 03c1:0022  default user timer hook (DSPTR(0x13a)): does nothing */
void intro_03c1_0022(void)
{
}

/* 03c1:0023  programs PIT channel 0 (mode 3) with DS16(0x138) */
void intro_03c1_0023(void)
{
    plat_set_timer_rate(DSU16(0x138));
}

/* 03c1:003d  installs the timer: retrace test result into DS16(0x172) (0 on VGA),
 * BIOS chain ratio, int 8 handler 03c1:00d0, PIT rate */
void intro_03c1_003d(void)
{
    uint16_t ax, dx;
    DS16(0x172) = 0;                            /* VGA: retrace bit test succeeded (cx = 0) */
    s_restore_div = 0xffff;
    ax = (uint16_t)(s_restore_div / DSU16(0x138));
    dx = (uint16_t)(s_restore_div % DSU16(0x138));
    if ((uint16_t)(dx << 1) >= DSU16(0x138)) ax++;
    ax--;
    if ((int16_t)ax < 0) ax++;
    if (ax >= 0xb) ax = 0xb;
    s_chain_ratio = (uint8_t)ax;                /* only used to chain to the BIOS clock */
    plat_set_timer_isr(intro_03c1_00d0);
    intro_03c1_0023();
}

/* 03c1:00d0  int 8 handler: counts ticks in SS:0x190, runs the registered callbacks
 * (unless DS16(0x174) != 0) and the user hook DSPTR(0x13a) */
void intro_03c1_00d0(void)
{
    int i;
    if (s_busy) return;                         /* (chains to BIOS every n ticks: omitted) */
    s_busy = 0xff;
    ISS16(0x190)++;
    if (DS16(0x174) == 0) {
        for (i = 0; i < 6; i++)
            if (s_cb[i]) s_cb[i]();
    }
    intro_03c1_0022();                          /* lcall [0x13a]: never changed by the intro */
    s_busy = 0;
}

/* 03c1:014a  registers a timer callback (far entry) */
void intro_03c1_014a(intro_cb_t cb)
{
    intro_03c1_0178(cb);
}

/* 03c1:0161  registers a timer callback (second far entry, same code) */
void intro_03c1_0161(intro_cb_t cb)
{
    intro_03c1_0178(cb);
}

/* 03c1:0178  stores cb in the first free slot (or in the overflow slot cs:0x20) */
void intro_03c1_0178(intro_cb_t cb)
{
    int i;
    for (i = 0; i < 6; i++)
        if (s_cb[i] == NULL) break;
    s_cb[i] = cb;
}

/* 03c1:01b2  removes a timer callback (far entry) */
void intro_03c1_01b2(intro_cb_t cb)
{
    intro_03c1_01c7(cb);
}

/* 03c1:01c7  clears the slot holding cb (or the overflow slot if not found) */
void intro_03c1_01c7(intro_cb_t cb)
{
    int i;
    for (i = 0; i < 6; i++)
        if (s_cb[i] == cb) break;
    s_cb[i] = NULL;
}

/* 03c1:01f4  restores the old int 8 vector and the PIT divisor cs:[6] */
void intro_03c1_01f4(void)
{
    plat_set_timer_isr(NULL);
    plat_set_timer_rate(s_restore_div);
}
