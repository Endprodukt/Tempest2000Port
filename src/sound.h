/* sound.h - the original's effects ('fox') and tunes ('modnum'). */
#ifndef T2K_SOUND_H
#define T2K_SOUND_H

#include <vectorpie/vp.h>

/* The effect numbers the original puts in 'sfx'. */
enum {
    FX_ENGINE1 = 0, FX_SHOT2 = 1, FX_ENGINE = 2, FX_SHOT = 5, FX_JUMP = 6, FX_CRACKLE = 7,
    FX_CLEARED = 8, FX_WARP = 9, FX_BIG_EXPLOSION = 10, FX_POWER_SHOT = 11, FX_GET_POWERUP = 12, FX_SPIKE_TINK = 13,
    FX_EXPLOSION = 16, FX_EXTRA_EXPLOSION = 17, FX_PULSAR = 19, FX_EXCELLENT = 21,
    FX_RECHARGE = 22, FX_YES = 23, FX_ONEUP = 24, FX_SCREAM = 25, FX_SEXY_YES = 27, FX_TINK = 28
};

/* The tune numbers the original puts in 'modnum'. */
enum { TUNE_THEME = 1 };

/* Once the game's audio exists; NULL (no sound device) is fine. */
void sound_init(vp_audio *a);

/* How loud, each 0..1: everything, the effects, the voices, the tunes. */
void sound_volumes(float master, float effects, float voices, float music);

/* Silence: nothing plays or starts until it is lifted. */
void sound_mute(int on);

/* fox: play an effect. `pitch` is the original's 'sfx_pitch': 0 for the
 * effect's own pitch, else a number in place of the sample table's (bigger
 * is higher; most effects' own is 428). */
void sound_fx(int fx, int pitch);

/* zoomon / zoomup: the two engine noises of a level's end, looped, and
 * their rise, `frames` frames in. sound_loop_stop ends them. */
void sound_zoom_on(void);
void sound_zoom_up(int frames);
void sound_loop_stop(void);

/* Play a tune, looped; the same tune again changes nothing. 0 = silence.
 * The game's own tunes are made from their modules first, which takes a
 * moment: they start a little after this. */
void sound_tune(int tune);

/* Once a game step: see sound_tune. */
void sound_update(void);

#endif /* T2K_SOUND_H */
