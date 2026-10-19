/* t2k_data.h - the game data taken from the Tempest 2000 assembly.
 * The tables are in gen/t2k_data.c, written by tools/extract_data.py. */
#ifndef T2K_DATA_H
#define T2K_DATA_H

/* The release edition (T2K_ROM_DATA) fills the tables of numbers from the
 * player's ROM when it starts: there they cannot be const. */
#ifdef T2K_ROM_DATA
#define T2K_DATA_CONST
#else
#define T2K_DATA_CONST const
#endif

/* A web: `lanes` lanes between lanes+1 rim points (x, y pairs, the original's
 * units: 0..~19, y down). A closed web repeats its first point at the end. */
typedef struct t2k_web {
    int          lanes;
    int          start;     /* the player's first lane */
    int          closed;
    const short *pts;       /* (lanes + 1) x, y pairs */
    const short *roll;      /* per lane: how an object in it is turned (256 = a full turn) */
    const short *tab2;      /* per lane: the original's second table */
} t2k_web;

/* A vector object, as make_vo3d builds it: vertices around the local centre,
 * lines as (vertex a, vertex b, colour) with 0-based vertices.
 * Tempest 2000's solid objects are kept the same way: the edges of their
 * faces, each with its face's colour (a Jaguar CRY colour byte). */
typedef struct t2k_vobj {
    int                  nverts;
    int                  nlines;
    const signed char   *verts;     /* x, y, z */
    const unsigned char *lines;     /* a, b, colour */
} t2k_vobj;

/* A picture from the game's sheets as the runs of lit pixels along its rows:
 * (row, first column, column past the last, CRY colour). */
typedef struct t2k_bitmap {
    short                w, h;
    short                 nruns;
    const unsigned short *runs;
} t2k_bitmap;

/* A font: 96 characters from the space on, as pixels. */
typedef struct t2k_font {
    short              w, h;
    short              advance;     /* from one letter to the next */
    const t2k_bitmap  *glyphs;
} t2k_font;

extern const t2k_font t2k_afont;            /* the messages' font, 15 x 18 */

extern const t2k_web t2k_webs[];
extern T2K_DATA_CONST unsigned char t2k_level_web[];    /* level -> index into t2k_webs */
extern const int t2k_level_count;

/* A level's enemies (setnewgen): how many to launch less one, then pairs of
 * (frames between launches, enemy type), -1 ends. The first pair is the
 * flippers'. */
extern const short *const t2k_waves[];
extern const short *const t2k_tradmax[16];  /* Traditional and Plus, from level 49 on */
extern T2K_DATA_CONST unsigned char t2k_raw_webs[];  /* 'raw_webs': 0..15 Traditional and Plus, 16..47 Tempest 2000 */
extern T2K_DATA_CONST unsigned char t2k_webcols[];   /* per 16 levels: the web's CRY colour; 1 = colour cycling */

/* Per pair of levels (index level / 2): speeds down the web, 16.16 per frame. */
extern T2K_DATA_CONST int t2k_spiker_zspeed[], t2k_flipper_zspeed[];
extern T2K_DATA_CONST int t2k_tanker_zspeed[], t2k_fuseball_zspeed[];
extern T2K_DATA_CONST unsigned short t2k_pudels[];   /* per 4 levels: pulsar phase delay, in both bytes */
extern T2K_DATA_CONST unsigned char t2k_flipcols[];  /* per 16 levels: the flippers' CRY colour */
extern T2K_DATA_CONST unsigned char t2k_fbcols[];    /* a fuseball's five legs */
extern T2K_DATA_CONST unsigned short t2k_pup_stuff[]; /* per 4 levels: kills between powerups, in both bytes */
extern T2K_DATA_CONST unsigned char t2k_webtunes[];  /* per 16 levels: the tune */
extern T2K_DATA_CONST unsigned short t2k_sfx_period[]; /* per effect: the period it is recorded at */
extern T2K_DATA_CONST unsigned char t2k_scores[];    /* doscore: (score digit, times - 1) pairs */

extern const t2k_vobj *const t2k_claws[8];
extern const t2k_vobj t2k_shot, t2k_chevre, t2k_flipper, t2k_zap;
extern const t2k_vobj t2k_fliptank, t2k_fusetank, t2k_pulstank;
extern const t2k_vobj t2k_spike, t2k_spiker, t2k_fuse1, t2k_fuse2;
extern const t2k_vobj t2k_b250, t2k_b500, t2k_b750, t2k_oneup;
extern const t2k_vobj t2k_pu1, t2k_pu2, t2k_pu3, t2k_pu4, t2k_pu5, t2k_pu6;
extern const t2k_vobj t2k_cube;

/* Tempest 2000 mode's objects (obj2d.s). */
extern const t2k_vobj *const t2k_sclaws[8];     /* the claw, 8 steps across a lane */
extern const t2k_vobj *const t2k_gsclaws[8];    /* player 2's */
extern const t2k_vobj t2k_s_flipper, t2k_s_fliptank, t2k_s_fliptank2;
extern const t2k_vobj t2k_fbpiece1, t2k_fbpiece2;
extern const t2k_vobj t2k_spuls1, t2k_spuls2, t2k_spuls3, t2k_spuls4, t2k_spuls5, t2k_spuls6;
extern const t2k_vobj t2k_chevron, t2k_chev, t2k_pshot, t2k_pwrlaser, t2k_g1, t2k_g2;
extern const t2k_vobj t2k_arr, t2k_dchev, t2k_epyr, t2k_xbit, t2k_mirr;
extern const t2k_vobj t2k_h2hshot1, t2k_h2hshot2, t2k_leaf, t2k_blueflipper;
extern const t2k_vobj t2k_s_flip1, t2k_s_flip2, t2k_hornm1, t2k_hornm2, t2k_hornm3, t2k_adroid;

#endif /* T2K_DATA_H */
