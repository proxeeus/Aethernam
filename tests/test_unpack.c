#include "../src/core/loader.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
void plat_fatal(const char *f, ...) { va_list a; va_start(a,f); vfprintf(stderr,f,a); exit(1); }
const char *plat_save_dir(void) { return "/tmp"; }
void *code_ptr_lookup(uint16_t s, uint16_t o) { return 0; }
static uint32_t rd32(const uint8_t *p){return p[0]|p[1]<<8|p[2]<<16|(uint32_t)p[3]<<24;}
int main(int argc, char **argv) {
    data_set_dir(argv[1]);
    for (int a = 2; a < argc; a++) {
        size_t n; uint8_t *d = data_load(argv[a], &n);
        if (!strstr(argv[a], ".PAK")) { size_t l; for (int e=0;e<8;e++){uint8_t*x=cc_load_entry(argv[a],e,&l); if(!x)break; uint32_t h=0; for(size_t i=0;i<l;i++)h=h*31+x[i]; printf("%s %d %zu %08x\n",argv[a],e,l,h); free(x);} continue; }
        unsigned cnt = rd32(d+4)/4 - 1;
        for (unsigned i = 0; i < cnt; i++) {
            uint32_t off = rd32(d+4+4*i), x = rd32(d+off);
            const uint8_t *h = d + off + (x ? x : 4);
            uint32_t packed = rd32(h), unp = rd32(h+4); int meth = h[8], info = h[9]; int doff = h[10]|h[11]<<8;
            uint8_t *out = malloc(unp+1);
            if (meth == 0) memcpy(out, h+12+doff, unp); else explode(h+12+doff, packed, out, unp, info);
            uint32_t hh = 0; for (size_t k = 0; k < unp; k++) hh = hh*31 + out[k];
            printf("%s %u %u %08x\n", argv[a], i, unp, hh); free(out);
        }
        free(d);
    }
}
