/* mod.h - a 4-channel ProTracker module ("M.K.") played into memory. */
#ifndef T2K_MOD_H
#define T2K_MOD_H

/* One play-through of the module in `mod` (`size` bytes), up to where the
 * song jumps back to a place it has been, so that it loops as the song
 * does: stereo, `rate` frames a second, left and right by turns, each -1..1.
 * Returns the frames (free() them) and sets *frames, or NULL: no module, or
 * no memory. Takes a moment: call it off the game's thread. */
float *mod_render(const unsigned char *mod, long size, int rate, long *frames);

#endif /* T2K_MOD_H */
