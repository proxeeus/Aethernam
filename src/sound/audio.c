/*
 * Audio thread: generates OPL2 samples and runs the music driver's timer at
 * its native rate, sample-accurately interleaved with synthesis.
 */
#include "audio.h"
#include "opl.h"
#include <SDL3/SDL.h>

#define RATE 49716

static SDL_AudioStream *stream;
static SDL_Mutex *mtx;
static double tick_acc;
double adlib_drv_hz = 1193182.0 / 19886.0;   /* game PIT divisor 0x4dae -> 60.0006 Hz */

void audio_lock(void)   { if (mtx) SDL_LockMutex(mtx); }
void audio_unlock(void) { if (mtx) SDL_UnlockMutex(mtx); }

static void SDLCALL feed(void *ud, SDL_AudioStream *s, int additional, int total) {
    (void)ud; (void)total;
    int16_t buf[1024];
    int frames = additional / 2;
    while (frames > 0) {
        int n = frames > 1024 ? 1024 : frames;
        SDL_LockMutex(mtx);
        int done = 0;
        while (done < n) {
            /* samples until the next driver tick */
            double per = RATE / adlib_drv_hz;
            int until = (int)(per - tick_acc);
            if (until <= 0) { adlib_drv_tick(); tick_acc -= per; continue; }
            int k = n - done < until ? n - done : until;
            opl_generate(buf + done, k);
            done += k; tick_acc += k;
        }
        SDL_UnlockMutex(mtx);
        SDL_PutAudioStreamData(s, buf, n * 2);
        frames -= n;
    }
}

void audio_init(void) {
    mtx = SDL_CreateMutex();
    opl_init(RATE);
    SDL_AudioSpec spec = { SDL_AUDIO_S16, 1, RATE };
    stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, feed, NULL);
    if (stream) SDL_ResumeAudioStreamDevice(stream);
}
