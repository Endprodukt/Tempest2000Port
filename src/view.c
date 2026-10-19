/* view.c - perspective, lines, objects and the game's pixel font. */
#include "view.h"

#include <math.h>

/* The original's perspective (polyo2d in horse.gas): a point is drawn
 * 128 * x / z pixels from the middle of a screen 240 lines high, z counted
 * from the viewer. The rim of a web is at z = 30, its far end at 190: the far
 * end is drawn at a sixth of the rim's size.
 *
 * On a 4:3 tube the 240 lines are the board's 4096 units. A 3:4 tube is too
 * narrow for that: there the width shows what x / z = 1.25 reaches. */
#define SCALE_HORIZONTAL (128.0f * 4096.0f / 240.0f)
#define SCALE_VERTICAL   1638.0f
#define Z_CLIP           2.0f       /* nothing nearer the eye than this is drawn */
#define MIN_LEVEL        0.15f      /* a beam dimmer than this draws nothing */

/* How much of the original's pixels is drawn: every row and pixel costs the
 * beam a stroke. 1 = all of them. */
#define TEXT_ROW_STEP   2       /* rows of a letter */

static vp_dvg *s_dvg;
static float   s_eye_x, s_eye_y, s_eye_z;
static float   s_scale_x, s_scale_y;
static float   s_roll_cos = 1.0f, s_roll_sin;
static float   s_spin_cos = 1.0f, s_spin_sin;
static float   s_pixel_x, s_pixel_y;    /* board units to a Jaguar screen pixel */
static int     s_pen_x, s_pen_y, s_pen_valid;
static float   s_level = 1.0f;

void view_init(int vertical)
{
    /* The board's square is stretched to the tube: undo it. */
    if (vertical) {
        s_scale_x = SCALE_VERTICAL;
        s_scale_y = SCALE_VERTICAL * 3.0f / 4.0f;
        s_pixel_x = 4096.0f / 384.0f;           /* the text keeps its width */
        s_pixel_y = s_pixel_x * 3.0f / 4.0f;
    } else {
        s_pixel_x = 4096.0f / 384.0f;           /* 384 x 240 is the whole tube */
        s_pixel_y = 4096.0f / 240.0f;
        s_scale_x = SCALE_HORIZONTAL * 3.0f / 4.0f;
        s_scale_y = SCALE_HORIZONTAL;
    }
}

void view_begin(vp_dvg *d, float eye_x, float eye_y, float eye_z)
{
    s_dvg = d;
    s_eye_x = eye_x;
    s_eye_y = eye_y;
    s_eye_z = eye_z;
    s_pen_valid = 0;
}

void view_set_roll(float roll)
{
    float a = roll * (6.28318530718f / 256.0f);
    s_roll_cos = cosf(a);
    s_roll_sin = sinf(a);
}

void view_set_spin(float spin)
{
    float a = spin * (6.28318530718f / 256.0f);
    s_spin_cos = cosf(a);
    s_spin_sin = sinf(a);
}

static void spin_point(float *x, float *z)
{
    float dx = *x, dz = *z - WEB_Z_CENTRE;
    *x = dx * s_spin_cos - dz * s_spin_sin;
    *z = WEB_Z_CENTRE + dx * s_spin_sin + dz * s_spin_cos;
}

static void project(float x, float y, float depth, int *sx, int *sy)
{
    float half = VP_DVG_COORD_MAX / 2.0f;
    float rx = x - s_eye_x, ry = y - s_eye_y;
    float tx = rx * s_roll_cos - ry * s_roll_sin;
    float ty = rx * s_roll_sin + ry * s_roll_cos;
    *sx = (int)lroundf(half + tx * s_scale_x / depth);
    *sy = (int)lroundf(half - ty * s_scale_y / depth);
}

int view_point(float x, float y, float z, int *sx, int *sy, float *scale)
{
    float depth = z - s_eye_z;
    if (depth < Z_CLIP) return 0;
    project(x, y, depth, sx, sy);
    *scale = s_scale_y / depth;
    return 1;
}

void view_line(float x0, float y0, float z0, float x1, float y1, float z1)
{
    float d0, d1;
    int ax, ay, bx, by;

    if (s_spin_sin != 0.0f) { spin_point(&x0, &z0); spin_point(&x1, &z1); }
    d0 = z0 - s_eye_z;
    d1 = z1 - s_eye_z;
    if (d0 < Z_CLIP && d1 < Z_CLIP) return;
    if (d0 < Z_CLIP) {
        float t = (Z_CLIP - d0) / (d1 - d0);
        x0 += (x1 - x0) * t; y0 += (y1 - y0) * t; d0 = Z_CLIP;
    } else if (d1 < Z_CLIP) {
        float t = (Z_CLIP - d1) / (d0 - d1);
        x1 += (x0 - x1) * t; y1 += (y0 - y1) * t; d1 = Z_CLIP;
    }
    project(x0, y0, d0, &ax, &ay);
    project(x1, y1, d1, &bx, &by);

    if (!s_pen_valid || ax != s_pen_x || ay != s_pen_y) vp_moveto(s_dvg, ax, ay);
    vp_lineto(s_dvg, bx, by);
    s_pen_x = bx; s_pen_y = by; s_pen_valid = 1;
}

/* A CRY colour byte is a place in a square of colours: blue at $00, red at
 * $f0, green at $0f, yellow at $ff, white in the middle ($88). This follows
 * those corners, not the Jaguar's own tables, and always gives the brightest
 * version: a vector line has no dark colours worth drawing. */
void view_level(float level)
{
    s_level = level;
}

void view_rgb(int r, int g, int b)
{
    float level = s_level < MIN_LEVEL ? MIN_LEVEL : s_level > 1.0f ? 1.0f : s_level;
    vp_color(s_dvg, (uint8_t)lroundf((float)r * level), (uint8_t)lroundf((float)g * level),
                    (uint8_t)lroundf((float)b * level));
}

void view_cry_i(int colour, float intensity)
{
    float r = (float)((colour >> 4) & 15) / 15.0f;
    float g = (float)(colour & 15) / 15.0f;
    float b = 1.0f - (r > g ? r : g);
    float top = r > g ? r : g;

    if (b > top) top = b;
    intensity *= s_level;
    if (intensity < MIN_LEVEL) intensity = MIN_LEVEL;
    if (intensity > 1.0f) intensity = 1.0f;
    top /= intensity;
    vp_color(s_dvg, (uint8_t)lroundf(255.0f * r / top),
                    (uint8_t)lroundf(255.0f * g / top),
                    (uint8_t)lroundf(255.0f * b / top));
}

void view_cry(int colour)
{
    view_cry_i(colour, 1.0f);
}

int view_pulse(int i)
{
    float a = (float)(i & 255) * (6.28318530718f / 256.0f);
    int s = (int)(sinf(a) * 127.0f) + 128, c = (int)(cosf(a) * 127.0f) + 128;
    return (s & 0xF0) | ((c >> 4) & 15);
}

static void pixel_line(float x0, float y0, float x1, float y1)
{
    float half = VP_DVG_COORD_MAX / 2.0f;
    vp_moveto(s_dvg, (int)lroundf(half + (x0 - 192.0f) * s_pixel_x),
                     (int)lroundf(half - (y0 - 120.0f) * s_pixel_y));
    vp_lineto(s_dvg, (int)lroundf(half + (x1 - 192.0f) * s_pixel_x),
                     (int)lroundf(half - (y1 - 120.0f) * s_pixel_y));
    s_pen_valid = 0;
}

static void run_lines(const t2k_bitmap *g, int row_step, float x, float y,
                      float xscale, float yscale, float xshear, float yshear)
{
    int i;
    for (i = 0; i < g->nruns; i++) {
        const unsigned short *r = g->runs + 4 * i;
        float j = (float)r[0], a = (float)r[1], b = (float)r[2];
        if (r[0] % row_step) continue;
        view_cry(r[3]);
        pixel_line(x + a * xscale + j * xshear, y + j * yscale + a * yshear,
                   x + b * xscale + j * xshear, y + j * yscale + b * yshear);
    }
}

void view_text(const t2k_font *font, const char *text, float x, float y,
               float xscale, float yscale, float xshear, float yshear)
{
    /* The small font has too few rows to leave any out. */
    int step = font->h > 10 ? TEXT_ROW_STEP : 1;

    for (; *text; text++, x += (float)font->advance * xscale) {
        unsigned ch = (unsigned char)*text - 32u;
        if (ch < 96u) run_lines(&font->glyphs[ch], step, x, y, xscale, yscale, xshear, yshear);
    }
}

static void board_point(float px, float py, int *bx, int *by)
{
    float half = VP_DVG_COORD_MAX / 2.0f;
    *bx = (int)lroundf(half + (px - 192.0f) * s_pixel_x);
    *by = (int)lroundf(half - (py - 120.0f) * s_pixel_y);
}

void view_board(float px, float py, int *bx, int *by)
{
    board_point(px, py, bx, by);
}

float view_pixel_height(void)
{
    return s_pixel_y;
}

static void draw_object(const t2k_vobj *o, float x, float y, float z,
                        float roll, float xscale, float yscale, int coloured,
                        float xoffset)
{
    float a = roll * (6.28318530718f / 256.0f);
    float c = cosf(a), s = sinf(a);
    int i, colour = -1;

    for (i = 0; i < o->nlines; i++) {
        const signed char *p = o->verts + 3 * o->lines[3 * i];
        const signed char *q = o->verts + 3 * o->lines[3 * i + 1];
        float px = (p[0] + xoffset) * xscale, py = p[1] * yscale;
        float qx = (q[0] + xoffset) * xscale, qy = q[1] * yscale;
        if (coloured && o->lines[3 * i + 2] != colour) {
            colour = o->lines[3 * i + 2];
            view_cry(colour);
        }
        view_line(x + px * c - py * s, y + px * s + py * c, z,
                  x + qx * c - qy * s, y + qx * s + qy * c, z);
    }
}

void view_vobj(const t2k_vobj *o, float x, float y, float z,
               float roll, float xscale, float yscale)
{
    draw_object(o, x, y, z, roll, xscale, yscale, 0, 0);
}

void view_solid(const t2k_vobj *o, float x, float y, float z,
                float roll, float xscale, float yscale)
{
    draw_object(o, x, y, z, roll, xscale, yscale, 1, 0);
}

void view_object(const t2k_vobj *o, float x, float y, float z, float roll,
                 float scale, float xoffset, int coloured)
{
    draw_object(o, x, y, z, roll, scale, scale, coloured, xoffset);
}

void view_vobj3(const t2k_vobj *o, float x, float y, float z,
                float rx, float ry, float rz, float scale)
{
    const float k = 6.28318530718f / 256.0f;
    float cx = cosf(rx * k), sx = sinf(rx * k);
    float cy = cosf(ry * k), sy = sinf(ry * k);
    float cz = cosf(rz * k), sz = sinf(rz * k);
    float p[2][3];
    int i, e;

    for (i = 0; i < o->nlines; i++) {
        for (e = 0; e < 2; e++) {
            const signed char *v = o->verts + 3 * o->lines[3 * i + e];
            float ax = (float)v[0] * scale, ay = (float)v[1] * scale, az = (float)v[2] * scale;
            float bx = ax * cz - ay * sz, by = ax * sz + ay * cz;       /* in the screen's plane */
            float dx = bx * cy - az * sy, dz = bx * sy + az * cy;       /* about the upright axis */
            p[e][0] = dx;
            p[e][1] = by * cx - dz * sx;                                /* about the level one */
            p[e][2] = by * sx + dz * cx;
        }
        view_line(x + p[0][0], y + p[0][1], z + p[0][2], x + p[1][0], y + p[1][1], z + p[1][2]);
    }
}
