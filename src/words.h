/* words.h - big words in the space, written in strokes.
 *
 * What stands in for the original's bonus pictures (250, 500, 750, 2000,
 * 1UP, Excellent). There each is a picture whose pixels come apart as it
 * grows; a beam cannot draw that many points, so here it is a word of a few
 * strokes that grows the same way.
 */
#ifndef T2K_WORDS_H
#define T2K_WORDS_H

/* The word centred on (x, y, z), `height` high, each stroke in a colour of
 * its own from `phase` on round the colour square. Letters it has: the
 * digits and capitals the game's words need (see words.c). */
void words_draw(const char *text, float x, float y, float z, float height, int phase);

/* How high a word is drawn to be as big as a picture `w` by `h` pixels with
 * its pixels `spacing` apart: as wide as the picture, but no higher. */
float words_height(const char *text, int w, int h, float spacing);

#endif /* T2K_WORDS_H */
