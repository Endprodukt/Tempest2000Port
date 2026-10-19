/* sound.c - effects and tunes from assets/, made by tools/extract_audio.py. */
#include "sound.h"
#include "mod.h"
#ifdef T2K_ROM_DATA
#include "rom.h"
#endif
#include "t2k_data.h"

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>

#define FX_COUNT     30
#define FX_VOLUME    0.8f
#define MUSIC_VOLUME 1.0f
#define MUSIC_RATE   32000

static vp_audio *s_audio;
static vp_sound *s_fx[FX_COUNT];
static vp_voice  s_music, s_loop, s_loop2;
static int       s_tune;
static vp_sound *s_tune_sound;          /* the tune playing, when the game made it from its module */

/* A tune being made from its module, off the game's thread: one at a time. */
static struct {
    SDL_Thread  *thread;
    SDL_atomic_t done;
    int          tune;
    char         path[1024];
    float       *samples;
    long         frames;
} s_job;
static float     s_master = 1.0f, s_effects = 1.0f, s_voices = 1.0f, s_music_level = 1.0f;
static int       s_muted;

/* The driver's pitch (unused/syn6.o, PLAYFX2): the number from the sample
 * table, or the one a call gives, times 28 over 43, is how far the DSP moves
 * on through the sample each time, in 256ths. The bigger, the higher. */

/* The engine noises are both in the table at 428. */
#define ENGINE_PITCH 428

static int dsp_pitch(int number) { return number * 28 / 43; }

void sound_init(vp_audio *a)
{
    char name[32], path[1024];
    int i;

    s_audio = a;
    if (!a) return;
    vp_audio_set_master(a, s_master);
    for (i = 0; i < FX_COUNT; i++) {
#ifdef T2K_ROM_DATA
        /* As they are in the ROM, 8-bit signed, at the rate the driver plays
         * them (tools/extract_audio.py says how that is found). */
        long length = 0, k;
        const unsigned char *data = rom_effect(i, &length);
        float *samples = data ? malloc((size_t)length * sizeof *samples) : NULL;
        int rate = (int)(26590906.0 / (2 * 32 * (0x1B + 1)) * dsp_pitch(t2k_sfx_period[i]) / 256.0 + 0.5);
        s_fx[i] = NULL;
        if (samples) {
            for (k = 0; k < length; k++) samples[k] = (float)(signed char)data[k] / 128.0f;
            if (vp_sound_from_samples(a, samples, (int)length, 1, rate, &s_fx[i]) != VP_OK) s_fx[i] = NULL;
            free(samples);
        }
        (void)name; (void)path;
#else
        snprintf(name, sizeof name, "assets/sounds/fx%02d.wav", i);
        if (vp_sound_load(a, vp_asset_path(name, path, sizeof path), &s_fx[i]) != VP_OK)
            s_fx[i] = NULL;         /* a missing file is a silent effect */
#endif
    }
}

void sound_volumes(float master, float effects, float voices, float music)
{
    s_master = master;
    s_effects = effects;
    s_voices = voices;
    s_music_level = music;
    if (!s_audio) return;
    vp_audio_set_master(s_audio, master);
    if (s_music) vp_voice_set_volume(s_audio, s_music, MUSIC_VOLUME * music, 0.1f);
}

void sound_mute(int on)
{
    s_muted = on;
    if (on) {
        sound_loop_stop();
        sound_tune(0);
    }
}

static int is_voice(int fx) { return fx >= FX_EXCELLENT && fx <= FX_SEXY_YES; }

/* How loud an effect is against the rest. The original passes each call a
 * volume the driver scales; these are set by ear on the cabinet instead. */
static float fx_level(int fx)
{
    if (is_voice(fx)) return 2.0f * s_voices;
    if (fx == FX_POWER_SHOT) return 0.35f * s_effects;
    return s_effects;
}

void sound_fx(int fx, int pitch)
{
    float ratio = 1.0f;
    if (s_muted) return;
    if (!s_audio || fx < 0 || fx >= FX_COUNT || !s_fx[fx]) return;
    if (pitch > 0) ratio = (float)dsp_pitch(pitch) / (float)dsp_pitch(t2k_sfx_period[fx]);
    vp_sound_play(s_audio, s_fx[fx], FX_VOLUME * fx_level(fx), ratio, 0);
}

void sound_loop_stop(void)
{
    if (s_audio && s_loop) vp_voice_stop(s_audio, s_loop, 0.2f);
    if (s_audio && s_loop2) vp_voice_stop(s_audio, s_loop2, 0.2f);
    s_loop = s_loop2 = 0;
}

/* How fast an engine noise runs once CHANGEFX has added `steps` to the
 * driver's pitch it was started with. */
static float zoom_ratio(int start, int steps)
{
    return (float)(dsp_pitch(start) + steps) / (float)dsp_pitch(ENGINE_PITCH);
}

/* zoomon: both engine noises, looped, at pitches 10 and 20: a fortieth and
 * a twentieth of their own, a rumble far below what they are. */
void sound_zoom_on(void)
{
    if (s_muted || !s_audio) return;
    sound_loop_stop();
    if (s_fx[FX_ENGINE1]) s_loop = vp_sound_play(s_audio, s_fx[FX_ENGINE1], FX_VOLUME * 1.6f * s_effects, zoom_ratio(10, 0), 1);
    if (s_fx[FX_ENGINE]) s_loop2 = vp_sound_play(s_audio, s_fx[FX_ENGINE], FX_VOLUME * 1.6f * s_effects, zoom_ratio(20, 0), 1);
}

/* zoomup, once a frame: CHANGEFX adds 1 to each one's pitch, so that after
 * the six seconds of a level's end they run a third faster than their own.
 * (zoomup also means to swell their volumes; as it is written that happens
 * once at most, by an eighth, and then never again.) */
void sound_zoom_up(int frames)
{
    if (!s_audio) return;
    if (s_loop) vp_voice_set_pitch(s_audio, s_loop, zoom_ratio(10, frames));
    if (s_loop2) vp_voice_set_pitch(s_audio, s_loop2, zoom_ratio(20, frames));
}

/* The folder a player's own tune is in, if there is one: save/music first
 * (it survives an update of the game), then music next to the game. */
static int own_tune(int tune, char *path, size_t size)
{
    char name[32];
    FILE *f;

    snprintf(path, size, "%s/music/tune%d.wav", vp_save_dir(), tune);
    if (!(f = fopen(path, "rb"))) {
        snprintf(name, sizeof name, "music/tune%d.wav", tune);
        f = fopen(vp_asset_path(name, path, size), "rb");
    }
    if (f) fclose(f);
    return f != NULL;
}

static int render_tune(void *unused)
{
    FILE *f;
    (void)unused;

#ifdef T2K_ROM_DATA
    {
        long size = 0;
        const unsigned char *mod = rom_tune(s_job.tune, &size);
        if (mod) s_job.samples = mod_render(mod, size, MUSIC_RATE, &s_job.frames);
        SDL_AtomicSet(&s_job.done, 1);
        return 0;
    }
#endif
    f = fopen(s_job.path, "rb");
    if (f) {
        long size;
        unsigned char *mod;
        fseek(f, 0, SEEK_END); size = ftell(f); fseek(f, 0, SEEK_SET);
        mod = size > 0 ? malloc((size_t)size) : NULL;
        if (mod && fread(mod, 1, (size_t)size, f) == (size_t)size)
            s_job.samples = mod_render(mod, size, MUSIC_RATE, &s_job.frames);
        free(mod);
        fclose(f);
    }
    SDL_AtomicSet(&s_job.done, 1);
    return 0;
}

void sound_tune(int tune)
{
    char path[1024];

    if (s_muted) tune = 0;
    if (!s_audio || tune == s_tune) return;
    if (s_music) vp_voice_stop(s_audio, s_music, 0.5f);
    s_music = 0;
    s_tune = tune;
    if (tune > 0 && own_tune(tune, path, sizeof path))
        s_music = vp_music_play(s_audio, path, MUSIC_VOLUME * s_music_level, 1, 0.5f);
    /* Else sound_update has it made from its module, and plays it then. */
}

/* Once a game step: a tune that has been made starts playing, and the one
 * wanted now, if it is neither playing nor being made, begins to be made. */
void sound_update(void)
{
    char name[40], path[1024];

    if (!s_audio) return;
    if (s_job.thread && SDL_AtomicGet(&s_job.done)) {
        SDL_WaitThread(s_job.thread, NULL);
        s_job.thread = NULL;
        if (s_job.samples && s_job.tune == s_tune && !s_music) {
            vp_sound *made = NULL;
            if (s_tune_sound) vp_sound_free(s_audio, s_tune_sound);
            s_tune_sound = NULL;
            if (vp_sound_from_samples(s_audio, s_job.samples, (int)s_job.frames, 2, MUSIC_RATE, &made) == VP_OK) {
                s_tune_sound = made;
                s_music = vp_sound_play(s_audio, made, MUSIC_VOLUME * s_music_level, 1.0f, 1);
            }
        }
        free(s_job.samples);
        s_job.samples = NULL;
    }
    if (s_job.thread || s_tune <= 0 || s_music || own_tune(s_tune, path, sizeof path)) return;
    snprintf(name, sizeof name, "assets/music/tune%d.mod", s_tune);
    vp_asset_path(name, s_job.path, sizeof s_job.path);
    s_job.tune = s_tune;
    s_job.frames = 0;
    SDL_AtomicSet(&s_job.done, 0);
    s_job.thread = SDL_CreateThread(render_tune, "tune", NULL);
    if (!s_job.thread) s_tune = 0;      /* no thread: no tune, rather than try again every step */
}
