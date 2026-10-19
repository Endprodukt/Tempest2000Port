/* view.h - from the game's 3D space to the beam.
 *
 * The space is the original's: x right, y DOWN, z into the screen. A web's
 * rim is at WEB_Z_NEAR (where the claw sits), its far end at WEB_Z_FAR.
 */
#ifndef T2K_VIEW_H
#define T2K_VIEW_H

#include <vectorpie/vp.h>
#include "t2k_data.h"

#define WEB_Z_CENTRE 110.0f                    /* the original's 'webz' */
#define WEB_Z_NEAR   (WEB_Z_CENTRE - 80.0f)
#define WEB_Z_FAR    (WEB_Z_CENTRE + 80.0f)

/* Once, before drawing: `vertical` = a 3:4 monitor (a Tempest cabinet). */
void view_init(int vertical);

/* Each frame, before drawing: where the viewer is. */
void view_begin(vp_dvg *d, float eye_x, float eye_y, float eye_z);

/* Turn everything about the line of sight (256 = a full turn), until told
 * otherwise: the web spinning in at the start of a level. */
void view_set_roll(float roll);

/* Turn the space about the upright line through the middle of the web
 * (256 = a full turn), until told otherwise: the web tumbling in. */
void view_set_spin(float spin);

/* A line between two points of the space. Lines given end to end cost the
 * beam no jump. */
void view_line(float x0, float y0, float z0, float x1, float y1, float z1);

/* A vector object at (x, y, z), turned by `roll` (256 = a full turn) and
 * scaled; a negative `xscale` mirrors it. Drawn in the current colour. */
void view_vobj(const t2k_vobj *o, float x, float y, float z,
               float roll, float xscale, float yscale);

/* The same for one of Tempest 2000's solid objects: the outlines of its
 * faces, each in its face's colour. */
void view_solid(const t2k_vobj *o, float x, float y, float z,
                float roll, float xscale, float yscale);

/* Either kind of object, turned about a point `xoffset` object units to the
 * side of its centre: a flipper turns about the end it holds on to. */
void view_object(const t2k_vobj *o, float x, float y, float z, float roll,
                 float scale, float xoffset, int coloured);

/* A vector object turned about all three axes (each 256 = a full turn):
 * the A.I. droid's cube tumbles. Drawn in the current colour. */
void view_vobj3(const t2k_vobj *o, float x, float y, float z,
                float rx, float ry, float rz, float scale);

/* Where a point of the space is on the board, and how many board units one
 * unit of the space is there. 0 if it is behind the viewer. */
int view_point(float x, float y, float z, int *sx, int *sy, float *scale);

/* How bright what is drawn from now on is, 0..1: the colours set by the
 * three calls below are dimmed by it. */
void view_level(float level);

/* Set the beam's colour, red, green and blue each 0..255. */
void view_rgb(int r, int g, int b);

/* Set the beam's colour from a Jaguar CRY colour byte, at full brightness. */
void view_cry(int colour);

/* The same, dimmed: `intensity` 0..1, the brightness byte of a CRY pixel. */
void view_cry_i(int colour, float intensity);

/* 'pulser': a CRY colour that goes round the colour square as `i` grows
 * (256 = once round). */
int view_pulse(int i);

/* 'texter' (stoat.gas): a string in one of the game's fonts, each lit row of
 * each letter one stroke. In the Jaguar's screen pixels (384 x 240, y down);
 * (x, y) is the first letter's top left corner. A letter's pixel (i, j) is
 * drawn at (i * xscale + j * xshear, j * yscale + i * yshear): as the scale
 * grows the rows come apart. */
void view_text(const t2k_font *font, const char *text, float x, float y,
               float xscale, float yscale, float xshear, float yshear);

/* Where a screen pixel (384 x 240, y down) is on the board, and how many
 * board units one of them is high: for the SDK's own text. */
void view_board(float px, float py, int *bx, int *by);
float view_pixel_height(void);

#endif /* T2K_VIEW_H */
