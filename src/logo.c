/* logo.c - the title: 'TEMPEST' and '2000' as solid letters in space.
 *
 * Not the original's title (a shaded picture breathing in front of a video
 * feedback effect, neither of which a beam can draw): slanted block letters
 * with depth, swinging in front of four big digits, each with a near face, a
 * far face and the edges between. The near faces are drawn twice, which on a
 * vector tube is what makes a line glow.
 */
#include "logo.h"
#include "view.h"

#include <math.h>

#define COUNT(a) ((int)(sizeof(a) / sizeof((a)[0]) / 2))

/* The letters, each in a box 28 wide and 70 high, y down. */
static const short letter_t[] = { 0,0, 28,0, 28,12, 19,12, 19,70, 9,70, 9,12, 0,12 };
static const short letter_e[] = {
    0,0, 28,0, 28,12, 10,12, 10,29, 24,29, 24,41, 10,41, 10,58, 28,58, 28,70, 0,70
};
static const short letter_m[] = {
    0,70, 0,0, 9,0, 14,24, 19,0, 28,0, 28,70, 19,70, 19,30, 14,50, 9,30, 9,70
};
static const short letter_p[] = { 0,70, 0,0, 28,0, 28,42, 10,42, 10,70 };
static const short hole_p[]   = { 10,12, 18,12, 18,30, 10,30 };
static const short letter_s[] = {
    28,0, 28,12, 10,12, 10,29, 28,29, 28,70, 0,70, 0,58, 18,58, 18,41, 0,41, 0,0
};

static const short letter_o[] = { 4,0, 24,0, 28,6, 28,64, 24,70, 4,70, 0,64, 0,6 };
static const short hole_o[]   = { 10,12, 18,12, 18,58, 10,58 };
static const short letter_g[] = {
    28,0, 28,12, 10,12, 10,58, 18,58, 18,41, 13,41, 13,29, 28,29, 28,70, 0,70, 0,0
};
static const short letter_a[] = { 0,70, 0,0, 28,0, 28,70, 18,70, 18,48, 10,48, 10,70 };
static const short hole_a[]   = { 10,12, 18,12, 18,36, 10,36 };
static const short letter_c[] = { 28,0, 28,12, 10,12, 10,58, 28,58, 28,70, 0,70, 0,0 };
static const short letter_r[] = { 0,70, 0,0, 28,0, 28,42, 21,42, 28,70, 18,70, 12,44, 10,44, 10,70 };
static const short letter_d[] = { 0,0, 20,0, 28,8, 28,62, 20,70, 0,70 };
static const short hole_d[]   = { 10,12, 18,12, 18,58, 10,58 };
static const short letter_u[] = { 0,0, 10,0, 10,58, 18,58, 18,0, 28,0, 28,70, 0,70 };
static const short letter_n[] = {
    0,70, 0,0, 9,0, 19,40, 19,0, 28,0, 28,70, 19,70, 9,30, 9,70
};

/* The digits, each in a box 44 wide and 65 high. */
static const short digit_2[] = {
    0,8, 8,0, 36,0, 44,8, 44,30, 14,52, 44,52, 44,65, 0,65, 0,50, 30,28, 30,13, 14,13, 14,20, 0,20
};
static const short digit_0[] = { 8,0, 36,0, 44,8, 44,57, 36,65, 8,65, 0,57, 0,8 };
static const short hole_0[]  = { 16,14, 28,14, 28,51, 16,51 };

/* How a group of shapes stands in the space. */
typedef struct pose {
    float x, y, z;              /* its middle */
    float scale;
    float slant;                /* how far the top leans right, per unit of height */
    float depth;                /* from the near face to the far one */
    float cy, sy, cx, sx;       /* turned about the upright axis, then the level one */
} pose;

static void set_turn(pose *p, float about_y, float about_x)
{
    p->cy = cosf(about_y); p->sy = sinf(about_y);
    p->cx = cosf(about_x); p->sx = sinf(about_x);
}

/* A point of a shape (lx, ly from the group's middle, lz into it) to the space. */
static void place(const pose *p, float lx, float ly, float lz, float *x, float *y, float *z)
{
    float px = (lx - ly * p->slant) * p->scale, py = ly * p->scale, pz = lz;
    float rx = px * p->cy - pz * p->sy, rz = px * p->sy + pz * p->cy;
    *x = p->x + rx;
    *y = p->y + py * p->cx - rz * p->sx;
    *z = p->z + py * p->sx + rz * p->cx;
}

/* One closed shape as a slab: its near face (`glow` times over), its far
 * face and the edges joining them. (ox, oy) is where its box starts, counted
 * from the group's middle. */
static void slab(const pose *p, const short *pts, int n, float ox, float oy,
                 int near_colour, int far_colour, int glow)
{
    int i, pass;

    for (pass = 0; pass < glow; pass++) {
        view_cry(near_colour);
        for (i = 0; i < n; i++) {
            const short *a = pts + 2 * i, *b = pts + 2 * ((i + 1) % n);
            float x0, y0, z0, x1, y1, z1;
            place(p, (float)a[0] + ox, (float)a[1] + oy, 0, &x0, &y0, &z0);
            place(p, (float)b[0] + ox, (float)b[1] + oy, 0, &x1, &y1, &z1);
            view_line(x0, y0, z0, x1, y1, z1);
        }
    }
    view_cry_i(far_colour, 0.55f);
    for (i = 0; i < n; i++) {
        const short *a = pts + 2 * i, *b = pts + 2 * ((i + 1) % n);
        float x0, y0, z0, x1, y1, z1, x2, y2, z2;
        place(p, (float)a[0] + ox, (float)a[1] + oy, p->depth, &x0, &y0, &z0);
        place(p, (float)b[0] + ox, (float)b[1] + oy, p->depth, &x1, &y1, &z1);
        place(p, (float)a[0] + ox, (float)a[1] + oy, 0, &x2, &y2, &z2);
        view_line(x2, y2, z2, x0, y0, z0);      /* the edge back from this corner */
        view_line(x0, y0, z0, x1, y1, z1);      /* and on along the far face */
    }
}

void logo_title(unsigned frames)
{
    static const short *const letters[7] = {
        letter_t, letter_e, letter_m, letter_p, letter_e, letter_s, letter_t
    };
    static const int counts[7] = {
        COUNT(letter_t), COUNT(letter_e), COUNT(letter_m), COUNT(letter_p),
        COUNT(letter_e), COUNT(letter_s), COUNT(letter_t)
    };
    /* From yellow through orange to red across the word, and back. */
    static const int warm[7] = { 0xFF, 0xFC, 0xF8, 0xF4, 0xF8, 0xFC, 0xFF };
    float t = (float)frames;
    int flash = frames % 300 < 4;               /* now and then the whole word flares white */
    pose p;
    int i;

    /* '2000' stands behind, big and blue, turning slowly the other way. */
    p.x = 0; p.y = 14.0f; p.z = 300.0f;
    p.scale = 3.0f; p.slant = 0.0f; p.depth = 30.0f;
    set_turn(&p, -0.16f * sinf(t * 0.011f), 0.08f * sinf(t * 0.017f));
    slab(&p, digit_2, COUNT(digit_2), -98.0f, -32.0f, 0x06, 0x02, 1);
    for (i = 1; i < 4; i++) {
        slab(&p, digit_0, COUNT(digit_0), -98.0f + 51.0f * (float)i, -32.0f, 0x06, 0x02, 1);
        slab(&p, hole_0, COUNT(hole_0), -98.0f + 51.0f * (float)i, -32.0f, 0x06, 0x02, 1);
    }

    /* 'TEMPEST' swings in front of it, leaning, lit from the front. */
    p.x = 0; p.y = -52.0f + 5.0f * sinf(t * 0.031f); p.z = 230.0f;
    p.scale = 1.75f; p.slant = 0.22f; p.depth = 24.0f;
    set_turn(&p, 0.26f * sinf(t * 0.019f), 0.12f * sinf(t * 0.027f));
    for (i = 0; i < 7; i++) {
        float ox = -110.0f + 32.0f * (float)i;
        int near_colour = flash ? 0x88 : warm[i];
        slab(&p, letters[i], counts[i], ox, -35.0f, near_colour, 0xF0, 2);
        if (letters[i] == letter_p) slab(&p, hole_p, COUNT(hole_p), ox, -35.0f, near_colour, 0xF0, 2);
    }
}

/* A letter's outline and the hole in it, if it has one. */
static const short *letter(char c, int *n, const short **hole, int *hole_n)
{
    *hole = 0; *hole_n = 0;
    switch (c) {
    case 'A': *hole = hole_a; *hole_n = COUNT(hole_a); *n = COUNT(letter_a); return letter_a;
    case 'C': *n = COUNT(letter_c); return letter_c;
    case 'R': *hole = hole_p; *hole_n = COUNT(hole_p); *n = COUNT(letter_r); return letter_r;
    case 'D': *hole = hole_d; *hole_n = COUNT(hole_d); *n = COUNT(letter_d); return letter_d;
    case 'E': *n = COUNT(letter_e); return letter_e;
    case 'G': *n = COUNT(letter_g); return letter_g;
    case 'M': *n = COUNT(letter_m); return letter_m;
    case 'N': *n = COUNT(letter_n); return letter_n;
    case 'O': *hole = hole_o; *hole_n = COUNT(hole_o); *n = COUNT(letter_o); return letter_o;
    case 'P': *hole = hole_p; *hole_n = COUNT(hole_p); *n = COUNT(letter_p); return letter_p;
    case 'S': *n = COUNT(letter_s); return letter_s;
    case 'T': *n = COUNT(letter_t); return letter_t;
    case 'U': *n = COUNT(letter_u); return letter_u;
    default:  *n = 0; return 0;
    }
}

/* A page's heading at the top: the words as slabs, swinging, their near
 * faces in `colours` (one a letter, a space's is skipped). */
static void heading(const char *text, float y, float scale, const int *colours, unsigned frames)
{
    float t = (float)frames;
    int flash = frames % 300 < 4;
    int count = 0, i;
    pose p;

    while (text[count]) count++;
    p.x = 0; p.y = y + 3.0f * sinf(t * 0.031f); p.z = 230.0f;
    p.scale = scale; p.slant = 0.22f; p.depth = 16.0f;
    set_turn(&p, 0.30f * sinf(t * 0.019f), 0.10f * sinf(t * 0.027f));
    for (i = 0; i < count; i++) {
        float ox = -((float)count * 32.0f - 4.0f) / 2.0f + 32.0f * (float)i;
        int near_colour = flash ? 0x88 : colours[i], n, hole_n;
        const short *hole, *shape = letter(text[i], &n, &hole, &hole_n);
        if (!shape) continue;
        slab(&p, shape, n, ox, -35.0f, near_colour, 0x0A, 2);
        if (hole) slab(&p, hole, hole_n, ox, -35.0f, near_colour, 0x0A, 2);
    }
}

void logo_topguns(unsigned frames)
{
    /* From green to yellow across the words, and back. */
    static const int cool[8] = { 0x0F, 0x4F, 0x8F, 0, 0xFF, 0xFF, 0x8F, 0x4F };
    heading("TOP GUNS", -140.0f, 0.85f, cool, frames);
}

void logo_dudes(unsigned frames)
{
    /* 'TEMPEST' as the title has it, 'DUDES' as the scores' heading. */
    static const int both[13] = {
        0xFF, 0xFC, 0xF8, 0xF4, 0xF8, 0xFC, 0xFF, 0, 0x0F, 0x4F, 0x8F, 0x4F, 0x0F
    };
    heading("TEMPEST DUDES", -158.0f, 0.58f, both, frames);
}

void logo_greatscore(unsigned frames)
{
    /* 'GREAT' in the title's colours, 'SCORE' in the scores' heading's. */
    static const int both[11] = {
        0xFF, 0xFC, 0xF8, 0xFC, 0xFF, 0, 0x0F, 0x4F, 0x8F, 0x4F, 0x0F
    };
    heading("GREAT SCORE", -150.0f, 0.68f, both, frames);
}
