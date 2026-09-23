// YM3812 via ymfm, resampled from the chip rate (3.579545 MHz / 72) to the output rate.
#include "opl.h"
#include "ymfm/ymfm_opl.h"
#include <cstring>

namespace {
class host : public ymfm::ymfm_interface {};
host        g_host;
ymfm::ym3812 *g_chip;
double      g_step, g_pos;
int32_t     g_prev, g_cur;
uint32_t    g_clock = 3579545;

// Register writes are queued and applied one per chip sample (~20 us apart), like a real
// AdLib where every write took ~25 us of port I/O. Applying a whole tick's writes at once
// would hide the key-off that precedes every key-on (the driver re-strikes each note that
// way), so notes would slur together and repeated notes would vanish.
struct wr { uint8_t reg, val; };
wr       g_q[8192];
unsigned g_qh, g_qt;

int32_t chip_sample() {
    if (g_qt != g_qh) {
        wr w = g_q[g_qt];
        g_qt = (g_qt + 1) % 8192;
        g_chip->write_address(w.reg);
        g_chip->write_data(w.val);
    }
    ymfm::ym3812::output_data o;
    g_chip->generate(&o);
    return o.data[0];
}
}

extern "C" void opl_init(int rate) {
    g_chip = new ymfm::ym3812(g_host);
    g_chip->reset();
    g_step = (double)g_chip->sample_rate(g_clock) / rate;
    g_pos = 0; g_prev = g_cur = 0;
}

extern "C" void opl_reset(void) { g_chip->reset(); }

static uint8_t g_b0[9];
extern "C" int opl_keyon_count[9];
int opl_keyon_count[9];
extern "C" void (*opl_trace)(int reg, int val);
void (*opl_trace)(int reg, int val);
extern "C" void opl_write_reg(int reg, int val) {
    if (opl_trace) opl_trace(reg, val);
    if (reg >= 0xb0 && reg <= 0xb8) {                 // statistics: key-on rising edges
        int ch = reg - 0xb0;
        if ((val & 0x20) && !(g_b0[ch] & 0x20)) opl_keyon_count[ch]++;
        g_b0[ch] = (uint8_t)val;
    }
    unsigned n = (g_qh + 1) % 8192;
    if (n == g_qt) {                        // queue full (never in practice): apply the oldest now
        wr w = g_q[g_qt]; g_qt = (g_qt + 1) % 8192;
        g_chip->write_address(w.reg); g_chip->write_data(w.val);
    }
    g_q[g_qh].reg = (uint8_t)reg; g_q[g_qh].val = (uint8_t)val;
    g_qh = n;
}

extern "C" void opl_generate(int16_t *out, int frames) {
    for (int i = 0; i < frames; i++) {
        g_pos += g_step;
        while (g_pos >= 1.0) { g_prev = g_cur; g_cur = chip_sample(); g_pos -= 1.0; }
        double s = g_prev + (g_cur - g_prev) * g_pos;
        s *= 2.0;
        if (s > 32767) s = 32767; if (s < -32768) s = -32768;
        out[i] = (int16_t)s;
    }
}
