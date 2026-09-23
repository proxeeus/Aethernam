/*
 * Eternam for macOS - entry point.
 *
 * Replaces the Tatou launcher (TATOU.COM) and Turbo C start-up (c0l): it
 * brings up the platform layer, rebuilds the original data segment from
 * AVE.CC1 and calls the game's own main().
 */
#include "core/mem.h"
#include "core/loader.h"
#include "platform/platform.h"
#include "game/protos.h"
#include "sound/audio.h"
#include <stdlib.h>
#include <string.h>

void intro_host_run(void);

int main(int argc, char **argv) {
    plat_init(argc, argv);
    mem_init();
    audio_init();
    atexit(plat_shutdown);
    int intro = 1;
    for (int i = 1; i < argc; i++) if (!strcmp(argv[i], "--no-intro")) intro = 0;
    if (getenv("ETERNAM_NO_INTRO")) intro = 0;
    if (intro) intro_host_run();  /* the intro program the Tatou launcher ran first */
    sub_0053_2014();            /* the original main() */
    exit(0);
}
