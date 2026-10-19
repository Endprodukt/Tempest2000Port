/* game.h - the game: its state, one step, one frame. */
#ifndef T2K_GAME_H
#define T2K_GAME_H

#include <vectorpie/vp.h>

void game_init(int vertical);

/* For testing: offer every start level up to this one (1-based) in all game types. */
void game_unlock(int level);

/* For testing: the game plays itself from the first level to the last,
 * `speed` steps a frame, in game type `mode` (0 Traditional, 1 Plus, 2
 * Tempest 2000), from `level` on (1 = the first), and reports each level on
 * stderr; then the program ends. */
void game_selftest(int speed, int mode, int beastly, int level);

void game_update(vp_game *g, float dt);     /* one 60 Hz step, as the original's 'Frame' */
void game_draw(vp_game *g, vp_dvg *d);

#endif /* T2K_GAME_H */
