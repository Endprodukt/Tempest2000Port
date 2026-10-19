/* game.c - Tempest 2000 mode: the web, the claw, the enemies, the effects.
 *
 * The step runs at 60 Hz like the original's 'Frame' interrupt, so its
 * per-frame values are used as they are: 16.16 fixed point in yak.s, `fix`
 * here. The routine a piece comes from is named next to it.
 */
#include "game.h"
#include "view.h"
#include "sound.h"
#include "logo.h"
#include "service.h"
#include "vpbind.h"
#include "words.h"
#ifdef T2K_ROM_DATA
#include "rom.h"
#endif
#include "t2k_data.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int fix;                        /* 16.16 */
#define FIX(v)   ((float)(v) / 65536.0f)
#define ZI(z)    ((z) >> 16)            /* the word the original compares */

#define Z_TOP    30                     /* webz - 80: the rim, where the claw is */
#define Z_BOTTOM 190                    /* webz + 80: the far end */
#define WEB_Z_CENTRE_I 110               /* webz */

/* claw_con1 / clawcon / setclaw / frab / player_shot */
#define CLAW_ACCEL    FIX(0x400)        /* lanes per frame, per frame (keys) */
#define CLAW_MAX_VEL  FIX(0x6000)       /* lanes per frame */
#define FIRE_RELOAD   3                 /* frames between shots while held */
#define PLAYER_SHOTS  8                 /* 'shots' starts at 7 and may reach -1 */
#define MAX_SHOTS     16                /* the player's, then as many for the droid ('shots+2') */
#define SHOT_SPEED    0x38000           /* z per frame (NTSC) */
#define LASER_SPEED   0x48000           /* hilaser: the particle laser */
#define LASER_RELOAD  2
#define SHOT_SPIN     8                 /* roll per frame */
#define JUMP_VEL      (-0x20000)        /* z per frame at take-off */
#define JUMP_GRAVITY  0xc11
#define VIEW_SPEED    FIX(0x4000)       /* vp_xform: the viewer's speed, per frame */

/* sweb / the level set-up after it, Tempest 2000 mode */
#define OBJECT_SLOTS     32             /* 'afree' */
#define FUSE_RISETIME    0x30
#define SPIKER_BUILD     50
#define ALIEN_FIRERATE   0x70
#define ASHOT_ZSPEED     0x18000
#define PULSAR_ZSPEED    0x8000
#define PROP_DELAY       4              /* a pulsar spark's frames per lane */
#define ZOOM_ACCEL       0x400          /* zs_stuff: the same for every level */
#define ZOOM_WEB_GONE    260            /* zoom1: 'vp_z' at which the web is behind and switched off */
#define ZOOM_CLAW_GONE   2000           /* czoom2: the claw this far off, and the next web comes */
#define ZOOM_CLAUD       0x1000         /* zoom2: what the claw's own speed gains a frame */
#define ZOOM_SPACING     0x40           /* czoom2 / zoom2: the claw's images come this much further apart */
#define WARP_ADD         0.25f          /* 'warp_add' grows $4000 a frame */
#define WARPIN_SPEED     2              /* not the original's: zoom3's steps taken this many at a time */
#define WASON_FRAMES     80             /* 'wason': then "superzapper recharge" is said */
#define FAR_START        300            /* ipix: a new enemy starts this far below the web */
#define SPIKE_START      30
#define SPIKE_MAX        150
#define START_LIVES      3
#define COLLIDE_Z        6              /* colok: a shot within this of an enemy hits it */

/* Webs are drawn at 4 times their data, around (8, 8): webinfo. */
#define WEB_SCALE   4.0f
#define WEB_CENTRE  8.0f

/* Not from the original: how far one spinner count moves the claw, and what
 * replaces its pixel effects on a vector tube. Tune on the cabinet. */
#define OBJ_SCALE        0.5f
#define SPIN_LANES       (5.0f / 1200.0f)   /* a 1200-count spinner: 5 lanes a turn */
#define RING_FRAMES      50             /* run_prex */
#define SPHERE_LIMIT     64             /* run_pixex: the sphere's scale grows 2 a frame to this */
#define PIXEX_LIMIT      64             /* run_pixex: a picture's scale grows 1 a frame to this */
#define SPLAT_FRAMES     8              /* xshot */
#define ZAP_FRAMES       120            /* zap_player: until the claw's z has run down */
#define SNATCH_FRAMES    50             /* snatch_it_away: the viewer falls back */
#define MESSAGE_HOLD     100            /* setmsg: 'msgtim1' */
#define MESSAGE_BREAK    50             /* setmsg: 'msgtim2' */
#define MESSAGE_SPREAD   0.5f           /* setmsg: what the row spacing grows by, a frame */
#define GAME_OVER_FRAMES 240
#define INTRO_STEPS      256            /* rotate_web: one full turn, a step a frame */
#define INTRO_SPEED      3              /* not the original's: its steps taken this many at a time */
#define INTRO_FRAMES     (INTRO_STEPS / INTRO_SPEED + 1)
#define TRAIL_FRAMES     150            /* oblow, oblow2 */
#define DROID_FIRE       5              /* 'droidel' */
#define REZ_FRAMES       64             /* set_rezclaw: 36(a6) from $f000 to 0 in steps of $40 */
#define STAR_MAX         256            /* game.ini's 'stars' at most; initstarfield has 48 */
#define STAR_DEPTH       512
#define POWERUP_REZ      300            /* run_pup: counts down by 4 before it moves */
#define MENU_TIMEOUT     (20 * 60)      /* a menu left alone goes on by itself */
#define ATTRACT_PAGE     250            /* versionscreen: 'attime' */
#define TITLE_FRAMES     600            /* not the original's: the title stays up longer */
#define DEMO_FRAMES      1500           /* nor this: its demo plays until its lives are gone */
#define INITIALS_TIMEOUT (30 * 60)
#define NOTICE_FRAMES    150
#define VICTORY_HOLD     120            /* the page stands this long before fire ends it */
#define VICTORY_TIMEOUT  (60 * 60)

enum { ATTRACT, MODE_SELECT, PLUS_SELECT, LEVEL_SELECT, INTRO, PLAYING, CAUGHT, ZAPPED, ZOOM, WARPIN, VICTORY, GAME_OVER, INITIALS };

/* What game.ini sets the brightness of, each on its own. */
enum { BRIGHT_WEB, BRIGHT_STARS, BRIGHT_ENEMIES, BRIGHT_EFFECTS, BRIGHT_CLAW, BRIGHT_SHOTS,
       BRIGHT_TEXT, BRIGHT_TITLE, BRIGHT_COUNT };
typedef char bright_count_is_the_service_menus[BRIGHT_COUNT == SERVICE_BRIGHTS ? 1 : -1];

/* The original's game types: its 'selected'. */
enum { MODE_TRADITIONAL, MODE_PLUS, MODE_2000, MODE_COUNT };

enum {
    K_NONE,
    K_FLIPPER, K_TANKER, K_SPIKER, K_SPIKE, K_FUSEBALL, K_PULSAR, K_PSPARK, K_ASHOT,
    K_MIRROR, K_ADROID, K_REFSHOT,
    K_DROID, K_TRAIL,
    K_POWERUP,
    K_RINGS, K_SPHERE, K_BONUS, K_SPLAT         /* effects */
};

/* The original's wave types ('inits'). */
enum { W_FLIPPER, W_TANKER, W_SPIKER, W_FUSEBALL, W_PULSAR, W_FUSETANKER, W_PULSARTANKER,
       W_SFLIP2, W_MIRROR, W_ADROID, W_BEAST, W_SFLIP3 };

typedef struct shot {
    int   live;
    float x, y;
    fix   z;
    int   lane;
    int   roll;
    int   laser;        /* a particle laser shot: it goes through what it hits */
    int   droid;        /* fired by the A.I. droid */
} shot;

/* One 'activeobject'. Which fields a kind uses is said where it is made. */
typedef struct obj {
    int   kind;
    float x, y;
    fix   z;
    int   lane;
    int   roll;
    int   far;          /* still flying in from below the web, drawn as a dot */
    int   enemy;        /* holds the level open; the Superzapper's prey */
    int   mode;
    int   type;
    int   timer, reload;
    int   speed;
    int   target;
    int   xcentre;
    int   alt;
    int   scarper;
    int   count;
    float vx, vy;
    fix   len;
    int   colour;
    struct obj *link;
    const char *label;          /* a bonus or an extra life: what it says */
} obj;

static struct {
    int            state;
    int            state_time;
    unsigned       frames;
    unsigned       rng;
    int            level;               /* 0-based: 'cwave' */
    const t2k_web *web;
    int            score, lives;
    const char    *msg[2];             /* 'msg', and the second line two of them have */
    int            msgtim1, msgtim2;
    float          msgys;               /* the rows' spacing: 1, then growing */

    /* the claw */
    float claw_pos;                     /* lane + fraction across it */
    float claw_vel;
    float claw_x, claw_y;
    fix   claw_z;
    fix   jump_vel;                     /* 0 = on the web ('cjump') */
    int   fire_timer;
    int   rez;                          /* frames of arriving left: it can move and fire, nothing can touch it */
    shot  shots[MAX_SHOTS];
    obj  *catcher;                      /* the flipper dragging the claw down */
    fix   zoom_vel;                     /* the web's 20(a0) */
    fix   zoom_dist;                    /* its 24(a0): how far the zoom has come */
    fix   vp_z;                         /* the viewer, while a level ends and the next arrives */
    fix   claud;                        /* the claw's own speed once the web is gone */
    int   claw_images;                  /* its 36(a6): how far apart its three more images are */
    int   claw_spin;                    /* what czoom2 has turned it by */
    int   zoom_web_off;                 /* zoom2: 34(a0) of the web cleared */
    int   wason;
    int   holiday;                      /* frames left of the web's colours running after an extra life */
    float warp_add;

    float view_x, view_y;               /* the viewer follows the claw */
    float menu_spin;

    /* the level */
    obj  objs[OBJECT_SLOTS];
    obj *spikes[32];                    /* per lane */
    int  wave_dur;                      /* enemies still to launch, less one */
    int  wave_done;                     /* 'wave_tim' < 0 */
    struct { int max, type, timer; } wave[12];
    int  wave_entries;
    int  spike_count;                   /* 'noclog' goes down by one per spike */
    int  max_spikers;
    int  ashots;
    int  alien_fire;                    /* 'a_firerate': one timer for every enemy */
    int  pucnt, pudel, pudel_timer;
    fix  flip_zspeed, tank_zspeed, spiker_zspeed, fuse_zspeed;
    int  sflip_prob1, sflip_prob2, sflip_prob3;

    /* the Superzapper */
    int   szap_avail, szap_on, szap_pulse, szap_single, szap_idle;
    float bolt_x, bolt_y;
    fix   bolt_z;
    int   bolt_time;
    int   zapped_now;                   /* 'bolt_lock': what the Superzapper takes leaves no powerup */

    /* powerups (run_pup): 'bonum' says which comes next */
    int pupcount, pup_reload;
    int bonum, laser, jenable;
    int crackled;                       /* this pulse's crackle has been played */

    struct { float x, y, z; int colour; } stars[STAR_MAX];
    int   star_count;                   /* game.ini */
    float bright[BRIGHT_COUNT];         /* game.ini: how bright each kind of thing is drawn, 0..1 */
    int sound_ready;
    int service;                        /* the service menu is open; the game stands still */
    int selftest;                       /* game_selftest: steps a frame, the game playing itself to its end */
    int no_rom;                         /* the release edition, and no ROM: nothing to play */
    const char *notice;                 /* what became of the service menu's settings, shown a while */
    int notice_time;

    /* players take turns; each keeps their own game (not the original's:
     * its two-player games are both at once) */
    int players, cur;
    struct player {
        int score, lives, level;
        int bonum, laser, jenable, had_droid;
        int start_bonus;                /* 'startbonus': 1 to be added, -1 a life to come */
    } pl[2];
    int had_droid;                      /* 'dnt' */
    int droid_game;                     /* 'entities' = 2: Tempest Plus with a droid for the whole game */
    int demo_time;
    int checkpoints;                    /* game.ini: a lost life keeps what was already shot */
    int carry;                          /* enemies still owed after a lost life; -1 = a fresh level */
    int life_owed;                      /* what this life began owing */
    int carry_spikes[32];               /* and how long each lane's spike was */
    int start_bonus, start_bonus_value; /* 'startbonus', 'lbonus' */
    int beastly;                        /* 'beastly' */
    fix shot_speed, laser_speed;
    int over_player;                    /* whose initials are being asked for */
    float sensitivity;                  /* game.ini: the spinner's, or the mouse's */
    float digital;                      /* game.ini: how fast keys or a stick move the claw; 1 = the original's */
    float volume, music_volume, effects_volume, voice_volume;   /* game.ini: 0..1 */

    int   attract_sound;                /* game.ini: sound while nobody plays */
    int   free_play;                    /* game.ini: no coin needed */
    int   unlocked_level;               /* game.ini: start levels offered from the first day */
    int   beastly_ini;                  /* game.ini: the way into Beastly Mode is shown to everybody */
    /* game.ini: which control does what, by advmame.rc names */
    char  btn_fire[SERVICE_CONTROLS_LENGTH], btn_zap[SERVICE_CONTROLS_LENGTH], btn_jump[SERVICE_CONTROLS_LENGTH],
          btn_left[SERVICE_CONTROLS_LENGTH], btn_right[SERVICE_CONTROLS_LENGTH];
    int   muted;

    /* attract mode: the title, the scores, then the game playing itself ('auto') */
    int   attract_page;
    int   demo;

    /* the game type: Tempest 2000's rules ('t2k'), solid objects ('blanka') */
    int mode, t2k, solids;
    int flip_rospeed, flip_pause, fuse_crossdelay;
    int best_level[MODE_COUNT];         /* how far each game type has been played */

    /* the cabinet */
    vp_credits credits;
    vp_score   table[VP_HISCORE_MAX];
    int        table_count, rank;
    char       initials[4];
    int        initial_pos, initial_letter;

} G;



/* --- small things ----------------------------------------------------------- */

static int rannum(void)                 /* 0..255 */
{
    G.rng = G.rng * 1664525u + 1013904223u;
    return (int)(G.rng >> 24);
}

static int rnd(int n)                   /* 0..n-1 */
{
    return n > 0 ? (int)(((unsigned)rannum() * (unsigned)n) >> 8) : 0;
}

/* setmsg: a message in the middle of the screen. It holds, then its rows of
 * pixels come apart (runmsg, drawmsg). */
static void say2(const char *text, const char *second)
{
    if (!text) return;
    G.msg[0] = text;
    G.msg[1] = second;
    G.msgtim1 = MESSAGE_HOLD;
    G.msgtim2 = MESSAGE_BREAK;
    G.msgys = 1.0f;
}

static void say(const char *text) { say2(text, NULL); }

/* runmsg */
static void run_message(void)
{
    if (!G.msg[0]) return;
    if (G.msgtim1 >= 0) { G.msgtim1--; return; }
    G.msgys += MESSAGE_SPREAD;
    if (--G.msgtim2 < 0) G.msg[0] = NULL;
}

/* doscore: scores[] holds a digit of the 8-digit score and how often to add
 * to it, less one. An extra life every 20,000 (adddig). */
static void do_oneup(void);

static void doscore(int which)
{
    static const int digit_value[8] = {10000000, 1000000, 100000, 10000, 1000, 100, 10, 1};
    int before = G.score / 20000;
    int times = t2k_scores[2 * which + 1];

    if (G.beastly) times = times ? times * 2 : 1;       /* scorer: 'tst beastly' */
    G.score += digit_value[t2k_scores[2 * which]] * (times + 1);
    if (G.score / 20000 > before) do_oneup();
}

/* --- the web ---------------------------------------------------------------- */

static void rim_point(const t2k_web *w, int i, float *x, float *y)
{
    *x = (w->pts[2 * i]     - WEB_CENTRE) * WEB_SCALE;
    *y = (w->pts[2 * i + 1] - WEB_CENTRE) * WEB_SCALE;
}

static void lane_centre(const t2k_web *w, int lane, float *x, float *y)
{
    float x0, y0, x1, y1;
    rim_point(w, lane, &x0, &y0);
    rim_point(w, lane + 1, &x1, &y1);
    *x = (x0 + x1) * 0.5f;
    *y = (y0 + y1) * 0.5f;
}

/* sweb: Tempest 2000 has its own 32 webs, the other game types the first 16. */
static void set_level(int level)
{
    int n = t2k_level_count;
    G.level = ((level % n) + n) % n;
    G.web = &t2k_webs[G.t2k ? t2k_level_web[G.level] : t2k_raw_webs[G.level & 15]];
}

static int web_colour(void) { return t2k_webcols[(G.level & 0x7F) >> 4]; }
static int noclog(void) { return G.t2k ? 21 : 18; }

/* toweb: the middle of the object's lane, turned as the lane is. */
static void toweb(obj *o)
{
    lane_centre(G.web, o->lane, &o->x, &o->y);
    o->roll = G.web->roll[o->lane];
}

static int claw_lane(void) { return (int)G.claw_pos; }

/* Enemies cannot touch the claw while it explodes, is dragged off or flies
 * down the web (the claw's 'vulnerable' flag, and wave_tim = -2). */
static int claw_prey(void) { return G.state == PLAYING && G.rez == 0; }

/* The claw on the rim (not in a jump), in this lane: what lightning down
 * the lane takes. */
static int claw_on_rim_in(int lane)
{
    return claw_prey() && lane == claw_lane() && ZI(G.claw_z) >= Z_TOP;
}

/* checlane: the claw is in this lane and within reach. */
static int checlane(const obj *o)
{
    int dz = ZI(o->z) - ZI(G.claw_z);
    return claw_prey() && o->lane == claw_lane() && abs(dz) <= 2;
}

/* --- objects ----------------------------------------------------------------- */

/* 'afree': the room left for enemies. Only what the original takes a slot
 * for counts: enemies, their shots, spikes, and what they turn into until
 * that is gone. Not the claw, its shots or the droid's. */
static int afree(void)
{
    int i, used = 0;
    for (i = 0; i < OBJECT_SLOTS; i++) {
        int kind = G.objs[i].kind;
        if (!kind || kind == K_DROID || kind == K_TRAIL || kind == K_SPLAT || kind == K_REFSHOT) continue;
        used += kind == K_TANKER ? 3 : 1;       /* maketank: its own slot and the two it carries */
    }
    return OBJECT_SLOTS - used;
}

static obj *new_obj(int kind)
{
    int i;
    for (i = 0; i < OBJECT_SLOTS; i++) {
        obj *o = &G.objs[i];
        if (o->kind) continue;
        memset(o, 0, sizeof *o);
        o->kind = kind;
        return o;
    }
    return NULL;
}

/* An enemy at the far end of a lane, as the make_* routines start them. */
static void toweb(obj *o);
static obj *new_enemy(int kind, int lane)
{
    obj *o = new_obj(kind);
    if (!o) return NULL;
    o->lane = lane;
    o->z = Z_BOTTOM << 16;
    o->far = FAR_START;
    o->enemy = 1;
    toweb(o);
    return o;
}

/* changex / nobobo: every so many kills, what was killed leaves a powerup.
 * Not what the Superzapper took. */
static int leave_powerup(obj *o)
{
    if (!G.t2k || G.zapped_now) return 0;
    if (--G.pupcount >= 0) return 0;
    G.pupcount = G.pup_reload;
    sound_fx(FX_WARP, 0);
    lane_centre(G.web, o->lane, &o->x, &o->y);
    o->kind = K_POWERUP;
    o->enemy = 0;
    o->far = 0;
    o->mode = 0;
    o->count = POWERUP_REZ;
    o->timer = 0;
    if (G.level <= 3) say("collect powerups!");
    return 1;
}

/* killme / dpring: the object becomes three widening rings of sparks. */
static void explode(obj *o)
{
    if (o->kind == K_SPIKE) G.spikes[o->lane] = NULL;
    sound_fx(FX_EXPLOSION, 128 + rannum());
    if (leave_powerup(o)) return;
    o->kind = K_RINGS;
    o->enemy = 0;
    o->far = 0;
    o->timer = 0;
    o->roll = rannum();
}

/* ouch: a death costs the powerups. */
static void lose_powerups(void) { G.laser = G.jenable = G.bonum = 0; }

static void selftest_death(const char *why);

static void start_death(const char *why)
{
    if (G.state != PLAYING && G.state != ZOOM) return;
    selftest_death(why ? why : "spike");
    say(why);
    sound_loop_stop();
    sound_fx(FX_BIG_EXPLOSION, 200);    /* zson: two, a little apart in pitch */
    sound_fx(FX_BIG_EXPLOSION, 196);
    lose_powerups();
    G.state = ZAPPED;
    G.state_time = 0;
}

/* gotu: a flipper takes the claw down the web (take_me_to_your_leader). */
static void gotu(obj *o)
{
    if (!claw_prey()) return;
    selftest_death("caught you!");
    say("caught you!");
    sound_fx(FX_SCREAM, 0);
    lose_powerups();
    G.catcher = o;
    G.state = CAUGHT;
    G.state_time = 0;
}

/* zappit: the Superzapper takes this one. */
static int zappit(const obj *o)
{
    G.szap_pulse = -1;
    G.szap_idle = 0;
    G.zapped_now = 1;
    G.bolt_x = o->x; G.bolt_y = o->y; G.bolt_z = o->z;
    G.bolt_time = 6;
    if (G.szap_single) G.szap_on = 0;
    return 1;
}

/* colok: is one of the player's shots on us? The shot stops there; off a
 * spike it spatters (dxle). */
static int colok(obj *o)
{
    int i;
    for (i = 0; i < MAX_SHOTS; i++) {
        shot *s = &G.shots[i];
        if (!s->live || s->lane != o->lane) continue;
        if (abs(ZI(s->z) - ZI(o->z)) >= COLLIDE_Z) continue;
        if (s->laser && !(o->kind == K_SPIKE && o->type)) return 1;    /* xle3 */
        s->live = 0;
        if (o->kind == K_SPIKE) {
            obj *p = new_obj(K_SPLAT);
            if (p) { p->x = s->x; p->y = s->y; p->z = s->z; p->roll = rannum(); }
        }
        return 1;
    }
    return 0;
}

/* mcollie: which shot is on us, left as it is. */
static shot *mcollie(const obj *o)
{
    int i;
    for (i = 0; i < MAX_SHOTS; i++) {
        shot *s = &G.shots[i];
        if (s->live && s->lane == o->lane && abs(ZI(s->z) - ZI(o->z)) < COLLIDE_Z) return s;
    }
    return NULL;
}

/* A shot turned on the player (rumirr, beastrail): it comes back up the lane.
 * `horn` 0 for a mirror's, else which horn of a beast it is; those come at
 * half the speed. */
static void reflect(shot *s, int horn)
{
    obj *o = new_obj(K_REFSHOT);
    s->live = 0;
    if (!o) return;
    o->x = s->x; o->y = s->y; o->z = s->z;
    o->lane = s->lane;
    o->type = horn;
}

/* refsht / refsht2 */
static void run_refshot(obj *o)
{
    if (o->type) { o->roll += 4; colok(o); }    /* a horn swallows shots */
    o->z -= o->type ? G.shot_speed / 2 : G.shot_speed;
    if (checlane(o)) { o->kind = K_NONE; start_death("shot you!"); return; }
    if (ZI(o->z) < 1) o->kind = K_NONE;
}

/* collie: a shot, or the Superzapper's pulse. */
static int collie(obj *o)
{
    if (G.szap_pulse > 0) return zappit(o);
    return colok(o);
}

/* alienfire: every enemy that may fire shares one timer. */
static void alienfire(const obj *o)
{
    obj *s;
    if (--G.alien_fire >= 0) return;
    G.alien_fire = ALIEN_FIRERATE;
    if (G.state == ZOOM || G.ashots < 0) return;
    if (ZI(o->z) <= Z_TOP + 60) return;         /* not from close to the rim */
    if (!(s = new_obj(K_ASHOT))) return;
    G.ashots--;
    s->x = o->x; s->y = o->y; s->z = o->z;
    s->lane = o->lane;
    s->enemy = 1;
}

/* run_ashot */
static void run_ashot(obj *o)
{
    o->roll += 4;
    o->z -= ASHOT_ZSPEED;
    if (collie(o)) { G.ashots++; explode(o); return; }
    if (ZI(o->z) <= Z_TOP) { G.ashots++; o->kind = K_NONE; return; }
    if (checlane(o)) {
        G.ashots++;
        o->kind = K_NONE;
        start_death("shot you!");
    }
}

/* --- flippers ---------------------------------------------------------------- */

static void flip_set_right(obj *o);

static void flip_turn(obj *o, int lane, float px, float py, int xcentre, int dir)
{
    o->lane = lane;
    o->target = G.web->roll[lane] & 0xFF;
    o->mode = 1;
    o->xcentre = xcentre;
    o->speed = dir * G.flip_rospeed * (o->type == 2 ? 2 : 1);
    o->x = px; o->y = py;               /* it turns about the lane's side */
}

/* flip_set_left: turn over the lane's left side into the lane before. */
static void flip_set_left(obj *o)
{
    const t2k_web *w = G.web;
    float px, py;
    int lane = o->lane - 1;

    if (o->lane == 0) {
        if (!w->closed) { if (w->lanes > 1) flip_set_right(o); return; }
        lane = w->lanes - 1;
    }
    rim_point(w, o->lane, &px, &py);
    flip_turn(o, lane, px, py, 9, -1);
}

/* flip_set_right */
static void flip_set_right(obj *o)
{
    const t2k_web *w = G.web;
    float px, py;
    int lane = o->lane + 1;

    if (o->lane == w->lanes - 1) {
        if (!w->closed) { if (w->lanes > 1) flip_set_left(o); return; }
        lane = 0;
    }
    rim_point(w, o->lane + 1, &px, &py);
    flip_turn(o, lane, px, py, -9, 1);
}

/* flip_set / _fset: flip toward the claw, the short way round a closed web.
 * A type 3 super flipper flips away from it. */
static void flip_set(obj *o)
{
    const t2k_web *w = G.web;
    int d = o->lane - claw_lane();
    int left;

    o->alt = o->lane;
    if (d == 0) { o->mode = 2; o->xcentre = 0; return; }       /* zzzz */
    if (w->closed) {
        int half = w->lanes >> 1;
        left = d > 0 ? (d - half < 0) : !(-d - half < 0);
    } else {
        left = d > 0;
    }
    if (o->type == 3) left = !left;
    if (left) flip_set_left(o); else flip_set_right(o);
}

static void fkillme(obj *o)
{
    doscore(0);
    explode(o);
}

/* rail: climb the lane, firing; at the rim, start flipping. */
static void flipper_rail(obj *o)
{
    if (o->type == 5) {                 /* beastrail: each hit costs it a horn, sent back */
        shot *s = mcollie(o);
        if (s) {
            if (o->count == 0) { s->live = 0; fkillme(o); return; }
            reflect(s, 4 - o->count);
            o->count--;
            return;
        }
    } else if (collie(o)) { fkillme(o); return; }
    alienfire(o);
    o->z -= G.flip_zspeed;
    if (ZI(o->z) > Z_TOP) return;
    o->z = Z_TOP << 16;
    o->mode = 1;
    flip_set(o);
}

static void topulsar(obj *o)
{
    o->kind = K_PULSAR;
    o->alt = 0;
    toweb(o);
}

/* flipto: turn until lying in the new lane. For the first part of the turn
 * it can catch the claw in the lane it left; after that it can be shot in
 * the lane it is entering. */
static void flipper_flipto(obj *o)
{
    int at, dist;

    if (o->type == 3) {
        o->z -= G.flip_zspeed;
        if (ZI(o->z) <= Z_TOP) o->type = 4;
    }
    o->roll += o->speed;
    at = (o->roll + 128) & 0xFF;
    if (at == o->target) {
        o->mode = 2;
        o->xcentre = 0;
        sound_fx(FX_TINK, 0);
        toweb(o);
        return;
    }
    dist = abs((signed char)(o->target - at));
    if (dist <= 52) {
        if (collie(o)) fkillme(o);
    } else if (o->alt >= 0 && claw_prey() && o->alt == claw_lane()
               && abs(ZI(o->z) - ZI(G.claw_z)) <= 2) {
        gotu(o);
    }
}

/* stopped: lying in a lane. Wait, then flip again. */
static void flipper_stopped(obj *o)
{
    if (o->type >= 1 && o->type <= 4 && o->scarper) {          /* sustop1 */
        if (o->scarper < 0) { o->scarper++; flip_set_left(o); }
        else                { o->scarper--; flip_set_right(o); }
        return;
    }
    if (collie(o)) { fkillme(o); return; }
    if (o->alt < 0) {                   /* out of a tanker: one flip, then climb */
        if (o->alt == -2) { topulsar(o); return; }
        o->alt = 0;
        o->mode = 0;
        if (o->type == 3) { o->mode = 1; flip_set(o); }
        return;
    }
    if (checlane(o)) { gotu(o); return; }
    if (o->reload >= 0 && !G.beastly) { /* a negative reload never pauses; nor does Beastly */
        if (--o->timer >= 0) return;
        o->timer = o->reload;
    }
    flip_set(o);
}

static void run_flipper(obj *o)
{
    if (G.szap_pulse > 0) { zappit(o); fkillme(o); return; }
    if (o->mode == 0) flipper_rail(o);
    else if (o->mode == 1) flipper_flipto(o);
    else flipper_stopped(o);
}

/* maflip / newflipper */
static obj *new_flipper(int lane, fix z)
{
    obj *o = new_obj(K_FLIPPER);
    if (!o) return NULL;
    o->lane = lane;
    o->z = z;
    o->enemy = 1;
    o->timer = o->reload = G.flip_pause;
    toweb(o);
    return o;
}

/* superflip: flippers out of a tanker are sometimes super flippers. */
static void superflip(obj *o, int second)
{
    if (!G.t2k || rannum() >= G.sflip_prob1) return;
    o->type = 2;
    o->reload = -1;
    o->scarper = (rannum() & 3) + 1;
    if (second) o->scarper = -o->scarper;
    if (rannum() >= G.sflip_prob2) return;
    o->type = 3;
}

static int make_flipper(int type)
{
    obj *o;
    if (G.t2k && type == 0 && rannum() < G.sflip_prob3) type = 2;      /* make_sflip2 */
    if (!(o = new_flipper(rnd(G.web->lanes), Z_BOTTOM << 16))) return 0;
    o->far = FAR_START;
    o->type = type;
    if (type) o->reload = -1;
    if (type == 3) o->mode = 2;
    if (type == 5) o->count = 2;        /* make_beast: two horns to lose */
    return 1;
}

/* --- tankers ----------------------------------------------------------------- */

static int make_tanker(int type)
{
    obj *o;
    if (afree() < 2 || G.web->lanes < 3) return 0;
    if (!(o = new_enemy(K_TANKER, rnd(G.web->lanes - 2) + 1))) return 0;      /* never an end lane */
    o->type = type;                             /* 0 flippers, 1 fuseballs, 2 pulsars */
    return 1;
}

static void set_fuseleft(obj *o);
static void set_fuseright(obj *o);

static obj *new_fuseball(int lane, fix z)
{
    obj *o = new_obj(K_FUSEBALL);
    if (!o) return NULL;
    o->lane = lane;
    o->z = z;
    o->enemy = 1;
    o->timer = o->reload = FUSE_RISETIME;
    o->count = G.fuse_crossdelay;
    rim_point(G.web, lane, &o->x, &o->y);       /* it rides the lane's side */
    return o;
}

/* opentanker: two of what it carries, one flipping each way. */
static void opentanker(obj *o)
{
    int lane = o->lane, type = o->type, i;
    fix z = o->z;

    doscore(1);
    sound_fx(FX_EXTRA_EXPLOSION, 60);   /* opentanker: twice, far below its own pitch */
    sound_fx(FX_EXTRA_EXPLOSION, 61);
    if (!leave_powerup(o)) {            /* setsphk */
        o->kind = K_SPHERE;
        o->enemy = 0;
        o->timer = 1;
        o->colour = (rannum() & 15) << 4;
    }

    if (type == 1) {                            /* openfuseballs */
        obj *f = new_fuseball(lane + 1, z);
        if (f) set_fuseright(f);
        f = new_fuseball(lane, z);
        if (f) set_fuseleft(f);
        return;
    }
    for (i = 0; i < 2; i++) {                   /* openflippers, openpulsars */
        obj *f = new_flipper(lane, z);
        if (!f) return;
        if (i == 0) flip_set_right(f); else flip_set_left(f);
        if (type == 2) { f->reload = -1; f->alt = -2; f->type = -1; }
        else { superflip(f, i); f->alt = -1; }
    }
}

static void run_tanker(obj *o)
{
    if (collie(o)) { opentanker(o); return; }
    o->z -= G.tank_zspeed;
    if (ZI(o->z) > Z_TOP) return;
    o->z = Z_TOP << 16;
    opentanker(o);
}

/* --- spikers and spikes -------------------------------------------------------- */

/* make_spike */
static obj *make_spike(int lane)
{
    obj *o;
    if (!(o = new_obj(K_SPIKE))) return NULL;
    o->lane = lane;
    o->z = Z_BOTTOM << 16;
    o->len = SPIKE_START << 16;
    if (G.t2k && rannum() < G.sflip_prob2) o->type = 1;     /* a super spike: shots do not wear it down */
    toweb(o);
    G.spikes[lane] = o;
    G.spike_count++;
    return o;
}

/* run_spike: it is hit where its tip is. */
static void run_spike(obj *o)
{
    fix z = o->z;
    int hit;

    o->z = z - o->len;
    if (G.state == ZOOM && o->lane == claw_lane() && ZI(G.claw_z) < Z_BOTTOM
        && ZI(G.claw_z) >= ZI(o->z)) {
        o->z = z;
        start_death(NULL);              /* setsnatch: no word for it */
        return;
    }
    hit = colok(o);
    o->z = z;
    if (!hit || o->type) return;
    sound_fx(FX_SPIKE_TINK, 524 + ZI(o->len));
    doscore(2);
    o->len -= 3 << 16;
    if (o->len >= 0) return;
    G.spike_count--;
    doscore(2);
    explode(o);
}

static int make_spiker(void)
{
    obj *o;
    if (G.max_spikers < 0) return 1;            /* one at a time; the wave goes on */
    if (!(o = new_enemy(K_SPIKER, rnd(G.web->lanes)))) return 0;
    o->far = 0;                                 /* it is on the web at once */
    G.max_spikers--;
    return 1;
}

/* newspike: a lane without a spike if there is one, else the shortest spike. */
static void spiker_newspike(obj *o)
{
    int free_lanes[32], nfree = 0, i;
    obj *shortest = NULL, *spike;

    for (i = 0; i < G.web->lanes; i++) {
        if (!G.spikes[i]) free_lanes[nfree++] = i;
        else if (!shortest || G.spikes[i]->len <= shortest->len) shortest = G.spikes[i];
    }
    spike = shortest;
    if (nfree) {
        obj *made = make_spike(free_lanes[rnd(nfree)]);
        if (made) spike = made;
    }
    if (!spike) { o->kind = K_NONE; G.max_spikers++; return; }
    o->lane = spike->lane;
    o->mode = 1;
    o->link = spike;
    o->count = SPIKER_BUILD;
    toweb(o);
}

static void run_spiker(obj *o)
{
    o->roll += 4;
    if (collie(o)) { G.max_spikers++; fkillme(o); return; }

    if (o->mode == 0) {
        spiker_newspike(o);
    } else if (o->mode == 1) {                  /* climbspike */
        obj *spike = o->link;
        alienfire(o);
        o->z -= G.spiker_zspeed;
        if (ZI(o->z) < Z_TOP + 10 || !spike || spike->kind != K_SPIKE) { o->mode = 2; return; }
        if (Z_BOTTOM - ZI(o->z) - ZI(spike->len) <= 0) return;  /* not past its tip yet */
        if (ZI(spike->len + G.spiker_zspeed) < SPIKE_MAX) spike->len += G.spiker_zspeed;
        if (--o->count < 0) o->mode = 2;
    } else {                                    /* descendspike */
        alienfire(o);
        o->z += G.spiker_zspeed;
        if (ZI(o->z) < Z_BOTTOM) return;
        o->z = Z_BOTTOM << 16;
        o->mode = 0;
    }
}

/* --- fuseballs ------------------------------------------------------------------ */

/* blowmeaway: a random bonus, shown flying off. */
static void blowmeaway(obj *o)
{
    int which = rnd(3);
    doscore(4 + which);
    sound_fx(FX_CLEARED, 0);
    o->kind = K_BONUS;                  /* xbon: its picture flies off down the web, then bursts */
    o->enemy = 0;
    o->label = which == 0 ? "250" : which == 1 ? "500" : "750";
    o->speed = 8 << 16;
    o->len = 2 << 16;                  /* xbon: the scale it starts with */
}

static void fuse_cross(obj *o, float fx, float fy, float tx, float ty, int change_lane)
{
    o->vx = (tx - fx) / 16.0f;
    o->vy = (ty - fy) / 16.0f;
    o->target = 15;                     /* steps to go */
    o->alt = change_lane;
    o->mode = 1;
}

/* set_fuseleft: across the lane before, to that lane's left side. */
static void set_fuseleft(obj *o)
{
    const t2k_web *w = G.web;
    float lx, ly, rx, ry;

    if (o->lane == 0) {
        if (!w->closed) return;
        o->lane = w->lanes - 1;
    } else {
        o->lane--;
    }
    rim_point(w, o->lane, &lx, &ly);
    rim_point(w, o->lane + 1, &rx, &ry);
    fuse_cross(o, rx, ry, lx, ly, 0);
}

/* set_fuseright: across this lane; it is in the next lane when it arrives. */
static void set_fuseright(obj *o)
{
    float lx, ly, rx, ry;
    rim_point(G.web, o->lane, &lx, &ly);
    rim_point(G.web, o->lane + 1, &rx, &ry);
    fuse_cross(o, lx, ly, rx, ry, 1);
}

static void run_fuseball(obj *o)
{
    const t2k_web *w = G.web;

    if (G.szap_pulse > 0) { zappit(o); blowmeaway(o); return; }
    o->roll += 2;

    if (o->mode == 0) {                         /* climbrail */
        int d;
        if (ZI(o->z) >= Z_TOP) o->z -= G.fuse_zspeed;
        if (--o->timer >= 0) return;
        o->timer = o->reload;
        d = o->lane - claw_lane();
        if (w->closed) {
            int half = w->lanes >> 1;
            if (d > 0) { if (d < half) set_fuseleft(o); else set_fuseright(o); }
            else       { if (-d < half) set_fuseright(o); else set_fuseleft(o); }
        } else if (d > 0) set_fuseleft(o);
        else set_fuseright(o);
        return;
    }

    /* crossrail: it can only be shot near the middle of the lane. */
    if (o->target >= 4 && o->target <= 12 && colok(o)) { blowmeaway(o); return; }
    if (ZI(o->z) <= Z_TOP && checlane(o)) { start_death("fried you!"); return; }
    if (--o->count >= 0) return;
    o->count = G.fuse_crossdelay;
    o->x += o->vx;
    o->y += o->vy;
    if (--o->target >= 0) return;
    o->mode = 0;
    if (o->alt) {
        if (o->lane == w->lanes - 1) {
            if (!w->closed) return;
            o->lane = 0;
        } else {
            o->lane++;
        }
    }
    rim_point(w, o->lane, &o->x, &o->y);
}

static int make_fuseball(void)
{
    obj *o = new_fuseball(rnd(G.web->lanes), Z_BOTTOM << 16);
    if (!o) return 0;
    o->far = FAR_START;
    return 1;
}

/* --- pulsars -------------------------------------------------------------------- */

static int make_pulsar(void)
{
    return new_enemy(K_PULSAR, rnd(G.web->lanes)) != NULL;
}

static int pulsar_deadly(void) { return (G.pucnt & 15) == 7; }

/* run_pulsar: at the peak of its pulse its whole lane is live. Past the rim
 * it becomes two sparks running round it (lanetop). */
static void run_pulsar(obj *o)
{
    o->z -= PULSAR_ZSPEED;
    if (G.state == ZOOM) return;
    if (collie(o)) { doscore(3); explode(o); return; }
    if (ZI(o->z) < Z_TOP && !G.t2k) {   /* flipping_heck: it goes on as a flipper */
        o->kind = K_FLIPPER;
        o->type = 0;
        o->timer = o->reload = G.flip_pause;
        o->scarper = 0;
        flip_set(o);
        return;
    }
    if (ZI(o->z) < Z_TOP) {
        obj *other = new_obj(K_PSPARK);
        o->kind = K_PSPARK;
        o->timer = PROP_DELAY;
        o->mode = 1;
        if (other) {
            *other = *o;
            other->mode = -1;
        }
        return;
    }
    if (!pulsar_deadly()) return;
    if (!G.crackled) { G.crackled = 1; sound_fx(FX_CRACKLE, 500); }
    if (claw_on_rim_in(o->lane)) start_death("fried you!");
}

/* run_pspark: round the rim, back from the ends of an open web. */
static void run_pspark(obj *o)
{
    const t2k_web *w = G.web;

    if (checlane(o)) { start_death("fried you!"); return; }
    if (collie(o)) { fkillme(o); return; }
    if (--o->timer >= 0) return;
    o->timer = PROP_DELAY;

    o->lane += o->mode;
    if (o->lane >= w->lanes) {
        if (w->closed) o->lane = 0;
        else { o->mode = -1; o->lane = w->lanes > 1 ? w->lanes - 2 : 0; }
    } else if (o->lane < 0) {
        if (w->closed) o->lane = w->lanes - 1;
        else { o->mode = 1; o->lane = w->lanes > 1 ? 1 : 0; }
    }
    toweb(o);
}

/* --- mirrors and enemy droids ------------------------------------------------------ */

/* make_mirr */
static int make_mirror(void)
{
    obj *o = new_enemy(K_MIRROR, rnd(G.web->lanes));
    if (!o) return 0;
    o->count = 4;
    return 1;
}

/* rumirr: it stops 40 short of the middle and throws back what is shot at
 * it, giving ground each time; the fifth hit, or the far end, finishes it. */
static void run_mirror(obj *o)
{
    shot *s;

    o->roll += 1;
    if (ZI(o->z) > Z_TOP + 40) o->z -= G.fuse_zspeed;
    if (!(s = mcollie(o))) return;
    if (--o->count < 0) { s->live = 0; blowmeaway(o); return; }
    o->z += G.fuse_zspeed << 4;
    if (ZI(o->z) > Z_BOTTOM) { s->live = 0; blowmeaway(o); return; }
    sound_fx(FX_PULSAR, 0);
    reflect(s, 0);
}

/* make_adroid */
static int make_adroid(void)
{
    obj *o = new_enemy(K_ADROID, rnd(G.web->lanes));
    if (!o) return 0;
    o->mode = 3;
    o->timer = 0x20;                    /* how long it zaps */
    o->reload = 2;                      /* its steps' delay */
    o->count = 2;
    o->speed = rannum() < 128 ? -1 : 1; /* which way round */
    return 1;
}

/* gstartmove: to the next lane its way, 16 steps; an open web's end turns it. */
static void adroid_start_move(obj *o)
{
    const t2k_web *w = G.web;
    float tx, ty;
    int lane = o->lane + o->speed;

    if (lane < 0 || lane >= w->lanes) {
        if (w->closed) lane = lane < 0 ? w->lanes - 1 : 0;
        else { o->speed = -o->speed; lane = o->lane + o->speed; }
    }
    if (lane < 0 || lane >= w->lanes) return;
    lane_centre(w, lane, &tx, &ty);
    o->vx = (tx - o->x) / 16.0f;
    o->vy = (ty - o->y) / 16.0f;
    o->target = lane;
    o->alt = 16;
    o->mode = 1;
}

/* run_adroid: up the web (shots only drive it back), then round the rim a
 * lane at a time, stopping in each to fill it with lightning. */
static void run_adroid(obj *o)
{
    o->roll -= 2;
    if (o->mode == 3) {                 /* uppweb */
        shot *s = mcollie(o);
        if (s) {
            s->live = 0;
            o->z += G.flip_zspeed << 5;
            if (ZI(o->z) >= Z_BOTTOM) blowmeaway(o);
            return;
        }
        o->z -= G.flip_zspeed << 1;
        if (ZI(o->z) < Z_TOP - 15) o->mode = 0;
        return;
    }
    if (collie(o)) { blowmeaway(o); return; }
    if (o->mode == 0) {
        adroid_start_move(o);
    } else if (o->mode == 1) {          /* gmove */
        if (--o->count >= 0) return;
        o->count = o->reload;
        o->x += o->vx; o->y += o->vy;
        if (o->alt == 8) o->lane = o->target;
        if (--o->alt == 0) o->mode = 2;
    } else {                            /* zappage */
        if (G.state == ZOOM) return;
        if (claw_on_rim_in(o->lane)) { start_death("fried you!"); return; }
        o->roll += 3;
        if (--o->timer == 0) { o->timer = 0x20; o->mode = 0; }
    }
}

/* --- the wave ------------------------------------------------------------------- */

/* setnewgen */
static void init_wave(void)
{
    const short *w = !G.t2k && G.level >= 48 ? t2k_tradmax[G.level & 15] : t2k_waves[G.level];
    int n = 0;

    G.wave_dur = *w++;
    while (*w >= 0 && n < 12) {
        G.wave[n].max = w[0];
        G.wave[n].type = w[1];
        G.wave[n].timer = w[0] >> 1;
        w += 2;
        n++;
    }
    G.wave_entries = n;
    G.wave_done = 0;
}

static void launch(int type)
{
    if (!G.t2k && type > W_PULSARTANKER) return;        /* launch1: Tempest 2000's own */
    switch (type) {
    case W_FLIPPER:      make_flipper(0); break;
    case W_TANKER:       make_tanker(0);  break;
    case W_SPIKER:       make_spiker();   break;
    case W_FUSEBALL:     make_fuseball(); break;
    case W_PULSAR:       make_pulsar();   break;
    case W_FUSETANKER:   make_tanker(1);  break;
    case W_PULSARTANKER: make_tanker(2);  break;
    case W_SFLIP2:       make_flipper(2); break;
    case W_SFLIP3:       make_flipper(3); break;
    case W_MIRROR:       make_mirror();   break;
    case W_ADROID:       make_adroid();   break;
    case W_BEAST:        make_flipper(5); break;
    default: break;
    }
}

static void selftest_blocked(int entry);
static void selftest_launched(int entry);

/* newgen: each kind of enemy has its own clock; nothing launches while the
 * web is crowded. */
static void run_wave(void)
{
    int i;
    if (G.wave_done) return;
    if (G.start_bonus < 0) {            /* newgen: the life that goes with the start bonus */
        G.start_bonus = 0;
        do_oneup();
    }
    for (i = 0; i < G.wave_entries; i++) {
        if (--G.wave[i].timer >= 0) continue;
        if (afree() <= noclog() - G.spike_count) { if (G.selftest) selftest_blocked(i); continue; }
        if (G.selftest) selftest_launched(i);
        G.wave[i].timer = G.wave[i].max;
        launch(G.wave[i].type);
        if (--G.wave_dur < 0) { G.wave_done = 1; return; }
    }
}

/* --- powerups ------------------------------------------------------------------- */

static void start_zoom(void);

/* any_pixex / do_oneup: a picture that starts at the far end as a point and
 * comes to the viewer for 150 frames (oblow2), its pixels coming apart all
 * the way (dmpix), so that it fills the screen within a second and then
 * flies past. Here a word, `w` by `h` being the picture's size in pixels. */
static obj *any_pixex(const char *label, int w, int h)
{
    obj *o = new_obj(K_TRAIL);
    if (!o) return NULL;
    o->z = (WEB_Z_CENTRE_I + 100) << 16;
    o->vx = G.claw_x * 0.5f / 32.0f;    /* toward where the viewer is heading: 'vp_xtarg' / 32 */
    o->vy = G.claw_y * 0.5f / 32.0f;
    o->label = label;
    o->target = w;
    o->alt = h;
    o->count = TRAIL_FRAMES;
    return o;
}

/* The picture 'Excellent' is $b2 by $2a pixels. */
static void excellent(void) { any_pixex("EXCELLENT", 0xB2, 0x2A); }

/* do_oneup */
static void do_oneup(void)
{
    obj *o;

    G.lives++;
    G.holiday = 50;
    sound_fx(FX_ONEUP, 0);
    o = any_pixex("UP", 0x32, 0x15);    /* draw_oneup: 'UP', and a row of '1's over it */
    if (o) o->type = 1;
}

static void make_droid(void);
static void run_droid(obj *o);
static void save_progress(void);
static void end_game(void);

/* suprise / bonnk: 2,000 points, or once in a while the way out of the level. */
static void surprise(void)
{
    if (rannum() <= 8) {                /* douttah */
        say("outta here!");
        doscore(13);
        excellent();
        start_zoom();
        return;
    }
    any_pixex("2000", 0xC5, 0x41);      /* bonnk: its picture is $c5 by $41 */
    doscore(12);
}

/* doita: what a collected powerup gives, in the order of 'pupvex'. */
static void collect_powerup(void)
{
    static const char *const text[8] = {       /* puptxts */
        "particle laser!", "zappo!", "jump enabled!", "outstanding!",
        "a.i. droid!", "wicked!", "zappo!", "dude!"
    };

    sound_fx(FX_WARP, 428);
    if (G.state == ZOOM) {              /* caught on the way out */
        say("yes! yes! yes!");
        sound_fx(FX_YES, 0);
        return;
    }
    say(text[G.bonum]);
    switch (G.bonum) {
    case 0: G.laser = 1; break;                         /* hilaser */
    case 4:                                             /* addroid: once; after that a surprise */
        if (G.had_droid) surprise();
        else { G.had_droid = 1; make_droid(); }
        break;
    case 2: G.jenable = 1; break;                       /* jenab */
    case 5:                                             /* sprzap: a Superzapper for nothing */
        sound_fx(FX_EXCELLENT, 0);
        excellent();
        G.szap_on = 1;
        G.szap_single = 0;
        G.szap_idle = 0;
        if (G.szap_avail < 0) G.szap_avail = 0;
        break;
    default: surprise(); break;
    }
    if (G.bonum < 7) G.bonum++;                         /* badd */
}

/* run_pup: it forms where its enemy died, then comes up the lane. The claw
 * takes it by being in its lane as it arrives. */
static void run_powerup(obj *o)
{
    int dz;

    if (o->mode) {                      /* taken: its rings widen until gone (xtoend) */
        o->timer += 5;
        if (o->timer >= 255) o->kind = K_NONE;
        return;
    }
    if (o->count >= 0) { o->count -= 4; return; }
    dz = ZI(o->z) - ZI(G.claw_z);
    if ((G.state == PLAYING || G.state == ZOOM) && o->lane == claw_lane()
        && ZI(G.claw_z) >= Z_TOP && dz >= 0 && dz <= 32) {
        o->mode = 1;
        o->timer = 11;
        collect_powerup();
        return;
    }
    o->z -= 0x18000;
    if (ZI(o->z) < Z_TOP) {             /* missed */
        o->kind = K_RINGS;
        o->timer = 0;
    }
}

/* --- the starfield -------------------------------------------------------------- */

/* initstarfield: stars scattered through the space in front of the viewer,
 * each coloured by where it is. */
static void init_stars(void)
{
    int i;
    for (i = 0; i < STAR_MAX; i++) {
        int x = rannum(), y = rannum();
        G.stars[i].x = (float)(x - 128);
        G.stars[i].y = (float)(y - 128);
        G.stars[i].z = (float)(rannum() * 2);
        G.stars[i].colour = (x & 0xF0) | (y >> 4);
    }
}

/* --- effects -------------------------------------------------------------------- */

static void run_effect(obj *o, int frames)
{
    if (++o->timer > frames) o->kind = K_NONE;
}

/* xr_pixex, then run_pixex: a picture (px_bons: 45 by 44 pixels) is thrown
 * down the web, slowing, then its pixels come apart until it is gone. */
static void run_pixex(obj *o)
{
    o->speed -= 0x8000;
    if (o->speed >= 0 && ZI(o->z) < Z_BOTTOM) { o->z += o->speed; return; }
    o->len += 1 << 16;
    if (ZI(o->len) >= PIXEX_LIMIT) o->kind = K_NONE;
}

/* run_objects */
static void run_objects(void)
{
    int i, furthest = 0;

    if (--G.pudel_timer < 0) {                  /* the pulsars' pulse */
        G.pudel_timer = G.pudel;
        G.pucnt = (G.pucnt + 1) & 15;
        if (G.pucnt == 0) G.crackled = 0;
    }
    G.szap_pulse = 0;
    if (G.szap_on && (G.frames & 3) == 0) {
        G.szap_pulse = 1;
        if (++G.szap_idle > 6) G.szap_on = 0;   /* nothing left to take */
    }

    for (i = 0; i < OBJECT_SLOTS; i++) {
        obj *o = &G.objs[i];
        if (!o->kind) continue;
        if (o->far) {
            /* A pulsar does not arrive in a dangerous phase. */
            if (o->far <= 2 && o->kind == K_PULSAR && (G.pucnt & 15) >= 3) { furthest = 999; continue; }
            o->far -= 2;
            furthest = 999;
            continue;
        }
        G.zapped_now = 0;
        switch (o->kind) {
        case K_POWERUP:  run_powerup(o);  break;
        case K_MIRROR:   run_mirror(o);   break;
        case K_ADROID:   run_adroid(o);   break;
        case K_REFSHOT:  run_refshot(o);  break;
        case K_DROID:    run_droid(o);    break;
        case K_TRAIL:                   /* oblow2 */
            o->z -= 1 << 16;
            o->x += o->vx; o->y += o->vy;
            if (--o->count < 0) o->kind = K_NONE;
            break;
        case K_FLIPPER:  run_flipper(o);  break;
        case K_TANKER:   run_tanker(o);   break;
        case K_SPIKER:   run_spiker(o);   break;
        case K_SPIKE:    run_spike(o);    break;
        case K_FUSEBALL: run_fuseball(o); break;
        case K_PULSAR:   run_pulsar(o);   break;
        case K_PSPARK:   run_pspark(o);   break;
        case K_ASHOT:    run_ashot(o);    break;
        case K_RINGS:    run_effect(o, RING_FRAMES);   break;
        case K_SPHERE:   o->timer += 2; if (o->timer >= SPHERE_LIMIT) o->kind = K_NONE; break;
        case K_BONUS:    run_pixex(o); break;
        case K_SPLAT:    run_effect(o, SPLAT_FRAMES);  break;
        default: break;
        }
        if (o->kind && o->enemy && ZI(o->z) > furthest) furthest = ZI(o->z);
    }

    /* r_end: every enemy launched and none left below the rim: fly off. */
    if (G.state == PLAYING && G.wave_done && furthest <= Z_TOP) start_zoom();
}

/* szoom */
static void start_zoom(void)
{
    if (G.state != PLAYING) return;
    G.state = ZOOM;
    G.state_time = 0;
    G.zoom_vel = 0;
    G.zoom_dist = 0;
    G.vp_z = 0;
    G.claud = 0;
    G.zoom_web_off = 0;
    G.warp_add = 0;
    G.szap_on = 0;
    sound_zoom_on();
    if (G.spike_count && G.level >= 3 && G.level <= 15) {     /* banana */
        say("avoid the spikes");
        G.msgtim1 = 250;
    }
}

/* --- the claw ------------------------------------------------------------------- */

/* setclaw */
static void reset_claw(void)
{
    G.claw_pos = (float)G.web->start;
    G.claw_vel = 0;
    G.claw_z = Z_TOP << 16;
    G.jump_vel = 0;
    G.claw_images = 0;
    G.claw_spin = 0;
    G.fire_timer = 0;
    G.catcher = NULL;
    lane_centre(G.web, G.web->start, &G.claw_x, &G.claw_y);
    memset(G.shots, 0, sizeof G.shots);
}

/* sweb and what follows it: the level's web, speeds and wave. */
static void start_level(int level)
{
    int idx, lvl, extra = 0;

    set_level(level);
    lvl = G.level;
    if (G.t2k) {
        G.flip_rospeed = 4;             /* rospeeds[1], crossdels[4], pauses[1] */
        G.fuse_crossdelay = 1;
        G.flip_pause = 8;
    } else {
        /* rsetw: past level 48 the last 16 levels come round, each time
         * faster. The three delays are looked up one through the other, as
         * the original does, off the ends of its tables included. */
        static const int rospeeds[8] = {2, 4, 8, 8, 8, 8, 8, 8};
        if (lvl >= 48) { extra = (lvl - 48) * 512; lvl = 48 + (lvl & 15); }
        G.flip_rospeed = rospeeds[lvl >> 4];
        G.fuse_crossdelay = G.flip_rospeed == 2 ? 3 : G.flip_rospeed == 4 ? 1 : 16;
        G.flip_pause = G.fuse_crossdelay == 3 ? 5 : G.fuse_crossdelay == 1 ? 8 : 0;
    }
    idx = lvl / 2;
    memset(G.objs, 0, sizeof G.objs);
    memset(G.spikes, 0, sizeof G.spikes);
    reset_claw();
    G.view_x = G.claw_x * 0.5f;
    G.view_y = G.claw_y * 0.5f;

    G.spiker_zspeed = t2k_spiker_zspeed[idx] + extra;
    G.flip_zspeed   = t2k_flipper_zspeed[idx] + extra;
    G.tank_zspeed   = t2k_tanker_zspeed[idx] + extra;
    G.fuse_zspeed   = t2k_fuseball_zspeed[idx] + extra;
    G.pudel = t2k_pudels[lvl / 4] & 0xFF;
    G.pudel_timer = G.pudel;
    G.pucnt = 0;
    G.sflip_prob3 = lvl > 16 ? (lvl - 16 > 127 ? 127 : lvl - 16) * 2 : 0;
    G.sflip_prob2 = (lvl > 127 ? 127 : lvl) * 2;
    G.sflip_prob1 = (lvl > 63 ? 63 : lvl) * 4;
    G.ashots = lvl >> 3 > 3 ? 3 : lvl >> 3;
    if (G.beastly) G.ashots = 7;        /* loads of shots */
    G.shot_speed = SHOT_SPEED >> (G.beastly ? 1 : 0);
    G.laser_speed = LASER_SPEED >> (G.beastly ? 1 : 0);
    G.alien_fire = ALIEN_FIRERATE;
    G.max_spikers = 0;
    G.spike_count = 0;
    G.szap_avail = 1;
    G.szap_on = G.szap_pulse = 0;
    G.bolt_time = 0;
    G.pup_reload = t2k_pup_stuff[lvl / 4] & 0xFF;
    G.pupcount = 0;                     /* the level's first kill leaves one */
    init_wave();
    if (G.carry >= 0) {
        /* The first Tempest's way, not the cartridge's (its 'it' starts the
         * wave afresh): only what was not yet shot comes again, and the
         * spikes stand as they were. */
        int lane;
        G.wave_dur = G.carry - 1;
        if (G.wave_dur < 0) G.wave_done = 1;
        for (lane = 0; lane < G.web->lanes && lane < 32; lane++) {
            obj *spike = G.carry_spikes[lane] > 0 ? make_spike(lane) : NULL;
            if (spike) { spike->len = G.carry_spikes[lane]; spike->type = 0; }
        }
        G.carry = -1;
    }
    G.life_owed = G.wave_done ? 0 : G.wave_dur + 1;
    G.rez = REZ_FRAMES;
    if (G.droid_game) make_droid();     /* makedroid: with every new claw */
    init_stars();
    sound_loop_stop();
    sound_tune(t2k_webtunes[(G.level >> 4) & 7]);
    G.state = INTRO;
    G.state_time = 0;
}

/* 'droid_data': the lane of the enemy nearest the rim. */
static int nearest_enemy_lane(void)
{
    int target = 0, nearest = 500, i;

    for (i = 0; i < OBJECT_SLOTS; i++) {
        const obj *o = &G.objs[i];
        int z = ZI(o->z) + o->far;
        if (!o->kind || !o->enemy || z >= nearest) continue;
        nearest = z;
        target = o->lane;
    }
    return target;
}

/* lor: which way round from one lane to another is shorter. -1 left, 1 right;
 * already there counts as right. */
static int lor(int from, int target)
{
    const t2k_web *w = G.web;
    int d = from - target;

    if (!w->closed) return d > 0 ? -1 : 1;
    if (d > 0) return d - (w->lanes >> 1) < 0 ? -1 : 1;
    return -d - (w->lanes >> 1) < 0 ? 1 : -1;
}

/* claw_con1 with 'auto' set: the demo steers for that enemy and keeps
 * firing; in its lane it stands. */
static int demo_steer(void)
{
    int target = nearest_enemy_lane();
    return target == claw_lane() ? 0 : lor(claw_lane(), target);
}

/* The self test's hands: the demo's, but they do not stay in a lane that is
 * about to kill, as nobody would: a shot coming up it, a pulsar pulsing, an
 * enemy droid's lightning, a spark or a fuseball on the rim. */
static int selftest_danger(int lane)
{
    int i;

    for (i = 0; i < OBJECT_SLOTS; i++) {
        const obj *o = &G.objs[i];
        if (o->far || o->lane != lane) continue;
        if (o->kind == K_REFSHOT || o->kind == K_ASHOT || o->kind == K_PSPARK) return 1;
        if (o->kind == K_PULSAR && (G.pucnt & 15) >= 4 && (G.pucnt & 15) <= 8) return 1;
        if (o->kind == K_ADROID && o->mode != 3) return 1;
        if (o->kind == K_FUSEBALL && ZI(o->z) <= Z_TOP + 8) return 1;
    }
    return 0;
}

static int selftest_steer(void)
{
    int lane = claw_lane(), steer = demo_steer(), next, i;

    /* Every enemy launched: go for what still keeps the level open, through
     * whatever is in the way. */
    for (i = 0; G.wave_done && i < OBJECT_SLOTS; i++) {
        const obj *o = &G.objs[i];
        if (o->kind && o->enemy && !o->far && ZI(o->z) > Z_TOP)
            return o->lane == lane ? 0 : lor(lane, o->lane);
    }
    if (selftest_danger(lane)) {        /* out of it, the way that is free */
        int right = lane + 1 < G.web->lanes ? lane + 1 : G.web->closed ? 0 : lane;
        return right != lane && !selftest_danger(right) ? 1 : -1;
    }
    next = lane + steer;
    if (G.web->closed) next = (next + G.web->lanes) % G.web->lanes;
    if (steer && next >= 0 && next < G.web->lanes && selftest_danger(next)) return 0;
    return steer;
}

/* mkdroid: the A.I. droid, a cube that floats over the rim two lanes along
 * from where the claw starts. */
static void make_droid(void)
{
    obj *o = new_obj(K_DROID);
    if (!o) return;
    o->lane = (G.web->start + 2) % G.web->lanes;
    o->z = (Z_TOP - 10) << 16;
    o->timer = DROID_FIRE;
    lane_centre(G.web, o->lane, &o->x, &o->y);
}

/* rundroid: it fires up its lane all the time, and moves a lane at a time,
 * in 16 steps, toward the enemy nearest the rim. */
static void run_droid(obj *o)
{
    const t2k_web *w = G.web;
    int i, live = 0;

    o->roll += 1;
    if (G.state == ZOOM && !G.droid_game) { o->kind = K_NONE; return; }   /* it leaves when the level is done */

    if (--o->timer < 0) {
        o->timer = DROID_FIRE;
        for (i = PLAYER_SHOTS; i < MAX_SHOTS; i++) live += G.shots[i].live;
        for (i = PLAYER_SHOTS; i < MAX_SHOTS && live < PLAYER_SHOTS; i++) {
            shot *s = &G.shots[i];
            if (s->live) continue;
            memset(s, 0, sizeof *s);
            s->live = 1;
            s->droid = 1;
            s->x = o->x; s->y = o->y; s->z = o->z;
            s->lane = o->lane;
            break;
        }
    }

    if (o->count == 0) {                /* droidleft / droidright */
        float tx, ty;
        int lane = o->lane + lor(o->lane, nearest_enemy_lane());
        if (lane < 0 || lane >= w->lanes) {
            if (w->closed) lane = lane < 0 ? w->lanes - 1 : 0;
            else lane = o->lane;        /* an open web's end: it marks time */
        }
        lane_centre(w, lane, &tx, &ty);
        o->vx = (tx - o->x) / 16.0f;
        o->vy = (ty - o->y) / 16.0f;
        o->target = lane;
        o->count = 16;
        return;
    }
    o->x += o->vx;                      /* movedroid */
    o->y += o->vy;
    if (--o->count == 8) o->lane = o->target;
}

/* clawcon + domove: keys accelerate and stop dead on release or reversal;
 * the spinner sets the speed outright. */
static void move_claw(vp_input *in)
{
    const t2k_web *w = G.web;
    int   spin  = vp_input_delta(in, "p1_dialx");
    int   left  = control_held(in, G.btn_left);
    int   right = control_held(in, G.btn_right);
    float accel = CLAW_ACCEL * G.digital, top = CLAW_MAX_VEL * G.digital;
    float pos;

    if (G.demo || G.selftest) {
        int steer = G.selftest ? selftest_steer() : demo_steer();
        spin = 0;
        left = steer < 0;
        right = steer > 0;
    }

    if (spin) {
        G.claw_vel = (float)spin * SPIN_LANES * G.sensitivity;
    } else if (left == right) {
        G.claw_vel = 0;
    } else if (right) {
        if (G.claw_vel < 0) G.claw_vel = 0;
        else if (G.claw_vel + accel <= top) G.claw_vel += accel;
    } else {
        if (G.claw_vel > 0) G.claw_vel = 0;
        else if (G.claw_vel - accel >= -top) G.claw_vel -= accel;
    }

    pos = G.claw_pos + G.claw_vel;
    if (w->closed) {
        pos = fmodf(pos, (float)w->lanes);
        if (pos < 0) pos += (float)w->lanes;
        if (pos >= (float)w->lanes) pos = 0;
        G.claw_pos = pos;
    } else if (pos >= 0 && pos < (float)w->lanes) {
        G.claw_pos = pos;               /* an open web's ends stop the claw */
    }
    lane_centre(w, claw_lane(), &G.claw_x, &G.claw_y);
}

/* claw_con1: the jump. */
static void jump_claw(vp_input *in)
{
    if (G.jump_vel != 0) {
        G.claw_z += G.jump_vel;
        G.jump_vel += JUMP_GRAVITY;
        if (G.jump_vel >= 0 && ZI(G.claw_z) >= Z_TOP) {
            G.claw_z = Z_TOP << 16;
            G.jump_vel = 0;
        }
    } else if (G.jenable && !G.demo
               && (G.selftest ? rannum() < 2 : control_pressed(in, G.btn_jump))) {
        G.jump_vel = JUMP_VEL;
        sound_fx(FX_JUMP, 0);
    }
}

/* sclawr + frab */
static void fire(vp_input *in)
{
    int i;
    if (!G.demo && !G.selftest && !control_held(in, G.btn_fire)) return;
    /* The self test holds its fire now and then: an enemy droid that is shot
     * back behind a spike which swallows the shots never arrives otherwise. */
    if (G.selftest && (G.frames / 600) % 5 == 4) return;
    if (--G.fire_timer >= 0) return;
    G.fire_timer = (G.laser ? LASER_RELOAD : FIRE_RELOAD) << (G.beastly ? 1 : 0);
    for (i = 0; i < PLAYER_SHOTS; i++) {
        shot *s = &G.shots[i];
        if (s->live) continue;
        s->live = 1;
        s->x = G.claw_x; s->y = G.claw_y; s->z = G.claw_z;
        s->lane = claw_lane();
        s->roll = 0;
        s->laser = G.laser;
        s->droid = 0;
        sound_fx(G.laser ? FX_POWER_SHOT : FX_SHOT, 0);
        return;
    }
}

/* fire (the original's name for it): the Superzapper. Once a level it takes
 * every enemy on the web, one each pulse; a second time, a single one. */
static void superzap(vp_input *in)
{
    if (G.demo || !(G.selftest ? rannum() < 1 : control_pressed(in, G.btn_zap))) return;
    if (G.szap_avail < 0 || G.szap_on) return;
    G.szap_single = G.szap_avail == 0;
    if (!G.szap_single) say2("eat electric", "death!");
    sound_fx(FX_CRACKLE, 300);
    G.szap_avail--;
    G.szap_on = 1;
    G.szap_idle = 0;
}

/* player_shot */
static void run_shots(void)
{
    int i;
    for (i = 0; i < MAX_SHOTS; i++) {
        shot *s = &G.shots[i];
        if (!s->live) continue;
        s->roll += SHOT_SPIN;
        s->z += s->laser ? G.laser_speed : G.shot_speed;
        if (ZI(s->z) >= Z_BOTTOM) s->live = 0;
    }
}

/* vp_set + vp_xform: the viewer drifts toward half the claw's offset. */
static void move_view(void)
{
    float speed = G.jump_vel != 0 ? VIEW_SPEED * 2 : VIEW_SPEED;
    float tx = G.claw_x * 0.5f, ty = G.claw_y * 0.5f;

    if (fabsf(tx - G.view_x) <= speed) G.view_x = tx;
    else G.view_x += tx > G.view_x ? speed : -speed;
    if (fabsf(ty - G.view_y) <= speed) G.view_y = ty;
    else G.view_y += ty > G.view_y ? speed : -speed;
}

/* --- drawing -------------------------------------------------------------------- */

static void lane_spokes(const t2k_web *w, int lane)
{
    float x0, y0, x1, y1;
    rim_point(w, lane, &x0, &y0);
    rim_point(w, lane + 1, &x1, &y1);
    view_line(x0, y0, FIX(Z_BOTTOM << 16), x0, y0, FIX(Z_TOP << 16));
    view_line(x1, y1, FIX(Z_TOP << 16), x1, y1, FIX(Z_BOTTOM << 16));
}

/* Both rims as one stroke each, then the spokes, alternating direction so the
 * beam never flies back down the web. The claw's lane is drawn in its colour
 * (domove). */
static void draw_web(const t2k_web *w, int lit_lane)
{
    const float zn = (float)Z_TOP, zf = (float)Z_BOTTOM;
    int i, n = w->lanes;
    int spokes = w->closed ? n : n + 1;
    int colour = web_colour();
    int psych = -1, line = 0;
    float x0, y0, x1, y1;

    /* swebpsych: every line of the web its own colour, two steps round the
     * colour square from the line before, and all of them running. That is
     * the web of levels 49 to 64 ('webcol' 1, Tempest 2000 only), and any web
     * for 50 frames after an extra life ('holiday': four times as fast, a new
     * set of colours every fourth frame). The original numbers the lines as
     * its web object lists them; here they are numbered as they are drawn. */
    if (G.holiday > 0) psych = (int)((G.frames & ~3u) * 8u);
    else if (G.t2k && colour == 1) psych = (int)(G.frames * 2u);
    view_level(G.bright[BRIGHT_WEB]);
    view_cry(colour);
    for (i = 0; i < n; i++) {
        rim_point(w, i, &x0, &y0);
        rim_point(w, i + 1, &x1, &y1);
        if (psych >= 0) view_cry(view_pulse(psych + 2 * line++));
        view_line(x0, y0, zn, x1, y1, zn);
    }
    for (i = n; i > 0; i--) {
        rim_point(w, i, &x0, &y0);
        rim_point(w, i - 1, &x1, &y1);
        if (psych >= 0) view_cry(view_pulse(psych + 2 * line++));
        view_line(x0, y0, zf, x1, y1, zf);
    }
    for (i = 0; i < spokes; i++, line++) {
        if (lit_lane >= 0 && (i == lit_lane || i == (lit_lane + 1) % spokes)) continue;
        rim_point(w, i, &x0, &y0);
        if (psych >= 0) view_cry(view_pulse(psych + 2 * line));
        if (i & 1) view_line(x0, y0, zn, x0, y0, zf);
        else       view_line(x0, y0, zf, x0, y0, zn);
    }
    if (lit_lane >= 0) {
        view_level(G.bright[BRIGHT_CLAW]);
        view_rgb(255, 255, 0);
        lane_spokes(w, lit_lane);
    }
}

/* What stands in for a Jaguar pixel: a short stroke. */
static void spark(float x, float y, float z, float dx, float dy, float len)
{
    view_line(x, y, z, x + dx * len, y + dy * len, z);
}

/* pring / pring2: a ring of `count` pixels, turned by `phase` (256 = once round). */
static void pixel_ring(float x, float y, float z, float radius, int count, int phase)
{
    int i;
    for (i = 0; i < count; i++) {
        float a = ((float)phase + (float)i * 256.0f / (float)count) * (6.28318530718f / 256.0f);
        float c = cosf(a), s = sinf(a);
        spark(x + s * radius, y + c * radius, z, s, c, 0.5f);
    }
}

/* draw_prex: three rings of 17, 15 and 13 pixels, each twice as wide as the
 * one before and 4 nearer, growing 1 a frame for 50 frames. The third ends
 * 200 across: a kill fills the screen. They dim as they grow. */
static void draw_rings(const obj *o)
{
    int radius = o->timer, i;
    float z = FIX(o->z);

    for (i = 0; i < 3; i++) {
        view_cry_i(view_pulse(radius), (float)(255 - (radius & 255)) / 255.0f);
        pixel_ring(o->x, o->y, z, (float)radius, 17 - 2 * i, (int)(G.frames & 255));
        z -= 4.0f;
        radius *= 2;
    }
}

/* drawsphere / psphere, type 2: a tanker bursts into a sphere of 8 circles
 * of 16 pixels, each circle turned 16 from the last and all of them turning,
 * growing 2 a frame to 63 across. */
static void draw_sphere(const obj *o)
{
    float radius = (float)o->timer;
    int ring, k;

    view_cry_i(o->colour | ((255 - o->timer / 4) & 15), (float)(255 - o->timer) / 255.0f);
    for (ring = 0; ring < 8; ring++) {
        float t = (float)((G.frames + (unsigned)ring * 16u) & 255) * (6.28318530718f / 256.0f);
        float ct = cosf(t), st = sinf(t);
        for (k = 0; k < 16; k++) {
            float a = (float)(k * 16) * (6.28318530718f / 256.0f);
            float sa = sinf(a) * radius, ca = cosf(a) * radius;
            float z = FIX(o->z) - sa * st;
            spark(o->x + sa * ct, o->y + ca, z, 1, 0, 0.5f);
        }
    }
}

/* dxshot: a shot off a spike, the chevron three times round. */
static void draw_splat(const obj *o)
{
    int k;
    for (k = 0; k < 3; k++)
        view_solid(&t2k_xbit, o->x, o->y, FIX(o->z), (float)(8 + o->timer * 3 + k * 0x55 + o->roll),
                   OBJ_SCALE, OBJ_SCALE);
}

/* draw_sfuseball: five legs, each one of two shapes at random. */
static void draw_fuseball(const obj *o)
{
    int i;
    for (i = 0; i < 5; i++) {
        const t2k_vobj *leg = rannum() & 1 ? &t2k_fbpiece1 : &t2k_fbpiece2;
        view_cry(t2k_fbcols[i]);
        view_object(leg, o->x, o->y, FIX(o->z), (float)(o->roll + i * 51), OBJ_SCALE, 9.0f, 0);
    }
}

static const t2k_vobj *pulsar_shape(void)
{
    static const t2k_vobj *const spulsars[16] = {
        &t2k_spuls6, &t2k_spuls6, &t2k_spuls6, &t2k_spuls5, &t2k_spuls4, &t2k_spuls3,
        &t2k_spuls2, &t2k_spuls1, &t2k_spuls1, &t2k_spuls2, &t2k_spuls3, &t2k_spuls4,
        &t2k_spuls5, &t2k_spuls6, &t2k_spuls6, &t2k_spuls6
    };
    return spulsars[G.pucnt & 15];
}

/* draw_pup1. Forming (opupring): a ring of 21 pixels closing in from 300
 * out, and four of 25 widening the other way. Then the powerup turning,
 * inside four rings of 13 pixels, each 8 nearer than the last (pupring).
 * Taken: the rings alone, widening to 255. */
static void draw_powerup(const obj *o)
{
    float z = FIX(o->z);
    int phase = (int)(G.frames & 255), k;

    if (!o->mode && o->count >= 0) {
        view_cry(view_pulse((int)G.frames));
        pixel_ring(o->x, o->y, z, (float)o->count, 21, phase);
        view_cry(view_pulse(o->count));
        for (k = 3; k >= 0; k--)
            pixel_ring(o->x, o->y, z, (float)((POWERUP_REZ - o->count) >> k), 25, 0);
        return;
    }
    if (!o->mode)
        view_solid(&t2k_pwrlaser, o->x, o->y, z, (float)(ZI(o->z) & 255), OBJ_SCALE, OBJ_SCALE);
    for (k = 0; k < 4; k++) {
        view_cry(view_pulse(ZI(o->z) - 8 * k));
        pixel_ring(o->x, o->y, z - 8.0f * (float)k, o->mode ? (float)o->timer : 10.0f, 13, phase);
    }
}

/* starfield1: the stars stand still; they shift as the viewer does. Each is
 * one pixel, dimmer the further off. While the claw flies out of the web
 * each becomes a streak ('warp_count', 'warp_add'). */
static void draw_stars(float eye_z, float streak)
{
    int i;
    view_level(G.bright[BRIGHT_STARS]);
    for (i = 0; i < G.star_count; i++) {
        float depth = fmodf(G.stars[i].z - eye_z, (float)STAR_DEPTH);
        if (depth < 0) depth += (float)STAR_DEPTH;
        if (depth < 4.0f) continue;
        view_cry_i(G.stars[i].colour,
                   (1.0f - depth / (float)STAR_DEPTH) * (0.35f + (float)rannum() / 392.0f));
        view_line(G.stars[i].x, G.stars[i].y, eye_z + depth,
                  G.stars[i].x, G.stars[i].y, eye_z + depth + 1.0f + streak);
    }
}

static const t2k_vobj *obj_shape(const obj *o, int *coloured, int *colour)
{
    static const t2k_vobj *const pus[6] = {&t2k_pu1, &t2k_pu2, &t2k_pu3, &t2k_pu4, &t2k_pu5, &t2k_pu6};
    static const unsigned char pucycl[16] = {0, 0, 0, 1, 2, 3, 4, 5, 5, 4, 3, 2, 1, 0, 0, 0};
    const t2k_vobj *pulsar = G.solids ? pulsar_shape() : pus[pucycl[G.pucnt & 15]];

    *coloured = 0;
    *colour = 0x88;
    switch (o->kind) {
    case K_FLIPPER:
        if (o->type == -1) { *colour = 0xFF; return pulsar; }      /* out of a pulsar tanker */
        *colour = t2k_flipcols[(G.level & 0x7F) >> 4];
        if (!G.solids) return &t2k_flipper;
        if (o->type == 0) return &t2k_s_flipper;
        if (o->type == 5) { *coloured = 1; return &t2k_hornm1; }
        *coloured = 1;
        return o->type == 2 ? &t2k_s_flip1 : &t2k_s_flip2;
    case K_TANKER:
        if (G.solids) { *coloured = 1; return &t2k_s_fliptank; }
        *colour = 128;
        return o->type == 1 ? &t2k_fusetank : o->type == 2 ? &t2k_pulstank : &t2k_fliptank;
    case K_SPIKER:   *colour = 143; return &t2k_spiker;
    case K_PULSAR:
    case K_PSPARK:   *colour = 0xFF; return pulsar;
    case K_FUSEBALL: *colour = t2k_fbcols[rnd(5)]; return rannum() & 1 ? &t2k_fuse1 : &t2k_fuse2;
    case K_ASHOT:    *colour = 0x48; return &t2k_shot;
    case K_MIRROR:   *coloured = 1; return &t2k_mirr;
    case K_ADROID:   *coloured = 1; return &t2k_adroid;
    case K_REFSHOT:
        if (!o->type) { *colour = 0x48; return &t2k_shot; }
        *coloured = 1;
        return o->type == 2 ? &t2k_hornm2 : &t2k_hornm3;
    default:         return NULL;
    }
}

/* A word standing in the space: `height` of it there, drawn as tall as that
 * looks from here. */
static void world_text(vp_dvg *d, const char *text, float x, float y, float z, float height, int colour)
{
    int sx, sy;
    float scale, tall;

    if (!text || !view_point(x, y, z, &sx, &sy, &scale)) return;
    tall = height * scale;
    if (tall < 20.0f) return;
    if (tall > 500.0f) tall = 500.0f;
    view_level(G.bright[BRIGHT_TEXT]);
    view_cry(colour);
    vp_text(d, text, (float)sx, (float)sy - tall / 2.0f, tall, VP_TEXT_CENTER);
}

static void draw_obj(const obj *o)
{
    const t2k_vobj *shape;
    float z = FIX(o->z);
    int coloured, colour;

    view_level(G.bright[o->kind >= K_DROID ? BRIGHT_EFFECTS : BRIGHT_ENEMIES]);
    if (o->far) {                       /* ipix: still a point, far below */
        view_cry(o->kind == K_TANKER ? 0x80 : t2k_flipcols[(G.level & 0x7F) >> 4]);
        spark(o->x, o->y, z + (float)o->far, 1, 0, 0.8f);
        return;
    }
    shape = obj_shape(o, &coloured, &colour);
    switch (o->kind) {
    case K_FLIPPER:
    case K_SPIKER:
    case K_MIRROR:
    case K_REFSHOT:
        view_cry(colour);
        if (o->kind == K_FLIPPER && o->type == 5) {     /* draw_beast: as many parts as horns left */
            static const t2k_vobj *const beastybits[3] = {&t2k_hornm1, &t2k_hornm2, &t2k_hornm3};
            float roll = (float)(o->mode == 1 ? o->roll * 2 : o->roll);
            int   part;
            for (part = 0; part <= o->count && part < 3; part++)
                view_object(beastybits[part], o->x, o->y, z, roll, OBJ_SCALE, (float)o->xcentre, 1);
            break;
        }
        view_object(shape, o->x, o->y, z, (float)o->roll, OBJ_SCALE, (float)o->xcentre, coloured);
        break;
    case K_ADROID:
        if (o->mode == 2) {             /* draw_adroid: lightning down its lane from the far end */
            view_cry(0x80 | (int)(G.frames & 15));
            view_line(o->x, o->y, (float)Z_BOTTOM, o->x, o->y, z);
        }
        view_object(shape, o->x, o->y, z, (float)o->roll, OBJ_SCALE, 0, coloured);
        break;
    case K_TANKER:
        view_cry(colour);
        view_object(shape, o->x, o->y, z, (float)o->roll, OBJ_SCALE, 0, coloured);
        if (G.solids && o->type == 1) {
            view_cry(t2k_fbcols[0]);
            view_vobj(&t2k_fuse1, o->x, o->y, z, (float)(G.frames * 2), OBJ_SCALE * 0.5f, OBJ_SCALE * 0.5f);
        } else if (G.solids && o->type == 2) {
            view_cry(0xFF);
            view_vobj(pulsar_shape(), o->x, o->y, z, (float)o->roll, OBJ_SCALE * 0.5f, OBJ_SCALE * 0.5f);
        }
        break;
    case K_SPIKE:
        if (o->type) view_cry((int)(G.frames * 0x31u) & 0xFF);     /* flashcol */
        else view_cry(143);
        view_line(o->x, o->y, z, o->x, o->y, z - FIX(o->len));
        spark(o->x - 0.4f, o->y, z - FIX(o->len), 1, 0, 0.8f);
        break;
    case K_FUSEBALL:
        if (G.solids) { draw_fuseball(o); break; }
        view_cry(colour);
        view_vobj(shape, o->x, o->y, z, (float)o->roll, OBJ_SCALE, OBJ_SCALE);
        break;
    case K_PULSAR:
    case K_PSPARK:
        if (pulsar_deadly()) view_rgb(255, 255, 255);
        else view_cry(colour);
        view_vobj(shape, o->x, o->y, z, (float)o->roll, OBJ_SCALE, OBJ_SCALE);
        if (o->kind == K_PULSAR && pulsar_deadly()) lane_spokes(G.web, o->lane);
        break;
    case K_ASHOT:
        if (G.solids) {                 /* s_shot: the chevron three times round */
            int k;
            for (k = 0; k < 3; k++)
                view_solid(&t2k_chevron, o->x, o->y, z, (float)(o->roll + k * 0x55), OBJ_SCALE, OBJ_SCALE);
            break;
        }
        if (G.frames & 1) view_cry(colour); else view_rgb(255, 255, 255);
        view_vobj(shape, o->x, o->y, z, (float)o->roll, OBJ_SCALE * 1.5f, OBJ_SCALE * 1.5f);
        break;
    case K_POWERUP: draw_powerup(o); break;
    case K_DROID:                       /* '_cube', in 'flashcol' */
        view_cry((int)(G.frames * 0x31u) & 0xFF);
        view_vobj3(&t2k_cube, o->x, o->y, z, (float)(o->roll * 2), (float)o->roll, (float)(o->roll * 3),
                   OBJ_SCALE * 4.0f);
        break;
    case K_TRAIL: {                     /* dmpix: the picture's pixels are 1/16 further apart every frame */
        float n = (float)(TRAIL_FRAMES - o->count);
        int   k;
        words_draw(o->label, o->x, o->y, z, words_height(o->label, o->target, o->alt, n / 16.0f),
                   (int)G.frames * 8);
        /* draw_oneup: the '1' at half the size, 8 above, six times: the
         * first further off than 'UP', the rest nearer and nearer. */
        for (k = 0; o->type && k < 6 && z + n / 4.0f * (float)(1 - k) >= 0; k++)
            words_draw("1", o->x, o->y - 8.0f, z + n / 4.0f * (float)(1 - k),
                       (float)0x15 * n / 32.0f, (int)G.frames * 8 + k * 40);
        break;
    }
    case K_RINGS:
        if (G.solids) { draw_rings(o); break; }
        /* vectorzap: Traditional's explosion, the zap shape spinning as it doubles. */
        view_cry(0xC8);
        view_vobj(&t2k_zap, o->x, o->y, z, (float)(o->roll + o->timer * 10),
                  OBJ_SCALE * (1.0f + (float)o->timer * 0.25f), OBJ_SCALE * (1.0f + (float)o->timer * 0.25f));
        break;
    case K_SPHERE: draw_sphere(o); break;
    case K_SPLAT:  draw_splat(o);  break;
    case K_BONUS:                       /* draw_mpixex: its pixels a quarter of the scale apart */
        words_draw(o->label, o->x, o->y, z, words_height(o->label, 0x2D, 0x2C, FIX(o->len) / 4.0f),
                   (int)G.frames * 8);
        break;
    default: break;
    }
}

/* dsclaw: Tempest 2000's claw, one of 8 shapes by how far across its lane
 * it is. nutargg: Traditional's, one of 8 and mirrored for the lane's other
 * half. */
static void draw_claw(fix at)
{
    int   lane = claw_lane();
    float part = G.claw_pos - (float)lane;
    float z = FIX(at);

    view_level(G.bright[BRIGHT_CLAW]);
    if (G.solids) {
        view_solid(t2k_sclaws[(int)(part * 8.0f) & 7], G.claw_x, G.claw_y, z,
                   (float)(G.web->roll[lane] + G.claw_spin), OBJ_SCALE, OBJ_SCALE);
    } else {
        int   frame = (int)(part * 16.0f);
        float flip = 1.0f;
        if (frame < 8) { frame = 7 - frame; flip = -1.0f; }
        else frame -= 8;
        view_cry(0xFF);
        view_vobj(t2k_claws[frame & 7], G.claw_x, G.claw_y, z, (float)(G.web->roll[lane] + G.claw_spin),
                  OBJ_SCALE * flip, OBJ_SCALE);
    }
}

/* zap_player + draw_z: the claw's end. The zap shape turning, 12 a frame,
 * and three more of it, each further toward the viewer than the last and
 * the gaps growing, while the viewer falls back from the web. */
static void draw_claw_zapped(void)
{
    int   t = G.state_time < ZAP_FRAMES ? G.state_time : ZAP_FRAMES;
    float z = FIX(G.claw_z) - 0.25f * (float)t;
    int   k;

    view_level(G.bright[BRIGHT_CLAW]);
    for (k = 0; k < 4; k++) {
        view_cry((255 - 17 * k) & 0xFF);
        view_vobj(&t2k_zap, G.claw_x, G.claw_y, z - 0.5f * (float)(t * k), (float)(t * 12),
                  OBJ_SCALE, OBJ_SCALE);
    }
}

/* A shot's picture in the solid game types: a small disc, as a diamond; the
 * droid's has points, as a star. */
static void dot(float x, float y, float z, float r, int spiky)
{
    if (spiky) {
        view_line(x - r, y, z, x + r, y, z);
        view_line(x, y - r, z, x, y + r, z);
        view_line(x - r * 0.7f, y - r * 0.7f, z, x + r * 0.7f, y + r * 0.7f, z);
        view_line(x - r * 0.7f, y + r * 0.7f, z, x + r * 0.7f, y - r * 0.7f, z);
        return;
    }
    view_line(x - r, y, z, x, y - r, z);
    view_line(x, y - r, z, x + r, y, z);
    view_line(x + r, y, z, x, y + r, z);
    view_line(x, y + r, z, x - r, y, z);
}

static void draw_shots(void)
{
    int i;
    view_level(G.bright[BRIGHT_SHOTS]);
    for (i = 0; i < MAX_SHOTS; i++) {
        const shot *s = &G.shots[i];
        if (!s->live) continue;
        if (s->droid && !s->laser) {
            if (G.solids) {             /* oaafire: its own picture, a spiky dot 9 pixels across */
                view_cry(0x76);
                dot(s->x, s->y, FIX(s->z), 9.0f * 0.25f / 2.0f, 1);
            } else {                    /* frab0 with '_chevre' */
                view_cry(0x88);
                view_vobj(&t2k_chevre, s->x, s->y, FIX(s->z), (float)s->roll, OBJ_SCALE, OBJ_SCALE);
            }
            continue;
        }
        if (s->laser) {                 /* draw_pring: a ring of 5 pixels, 8 across */
            view_cry(view_pulse(ZI(s->z)));
            pixel_ring(s->x, s->y, FIX(s->z), 8.0f, 5, ZI(s->z) & 255);
            continue;
        }
        if (G.solids) {                 /* oaafire: a round dot 7 pixels across, a quarter unit each */
            view_cry(0x3F);
            dot(s->x, s->y, FIX(s->z), 7.0f * 0.25f / 2.0f, 0);
            continue;
        }
        if ((G.frames + (unsigned)i) & 1) view_rgb(255, 255, 255);
        else                              view_rgb(255, 60, 60);
        view_vobj(&t2k_shot, s->x, s->y, FIX(s->z), (float)s->roll, OBJ_SCALE, OBJ_SCALE);
    }
}

/* The Superzapper's bolt, from the claw to what it took. */
static void draw_bolt(void)
{
    float x = G.claw_x, y = G.claw_y, z = FIX(G.claw_z);
    int i;

    view_level(G.bright[BRIGHT_SHOTS]);
    view_rgb(255, 255, 255);
    for (i = 1; i <= 6; i++) {
        float t = (float)i / 6.0f;
        float nx = G.claw_x + (G.bolt_x - G.claw_x) * t;
        float ny = G.claw_y + (G.bolt_y - G.claw_y) * t;
        float nz = FIX(G.claw_z) + (FIX(G.bolt_z) - FIX(G.claw_z)) * t;
        if (i < 6) {
            nx += (float)(rannum() - 128) * 0.02f;
            ny += (float)(rannum() - 128) * 0.02f;
        }
        view_line(x, y, z, nx, ny, nz);
        x = nx; y = ny; z = nz;
    }
}

/* Plain stroke lettering of the first Tempest's kind (the SDK's Asteroids
 * font), for everything that is not one of the game's own messages: `x`, `y`
 * and the letters' height in screen pixels, `y` their top. */
static void plain(vp_dvg *d, const char *text, float x, float y, float height, int align,
                  int r, int g, int b)
{
    int bx, by;
    if (G.state == ATTRACT || G.demo) {
        /* Nobody is playing and the same words stand for hours: they drift
         * a few pixels round, slowly, so no line burns into the tube. */
        x += 6.0f * sinf((float)G.frames * 0.0031f);
        y += 4.0f * sinf((float)G.frames * 0.0047f);
    }
    view_board(x, y + height, &bx, &by);
    view_level(G.bright[BRIGHT_TEXT]);
    view_rgb(r, g, b);
    vp_text(d, text, (float)bx, (float)by, height * view_pixel_height(), align);
}

static void draw_hud(vp_dvg *d)
{
    char text[32];

    snprintf(text, sizeof text, "%d", G.score);
    plain(d, text, 8.0f, 5.0f, 9.0f, VP_TEXT_LEFT, 0, 255, 0);
    if (G.players == 2) snprintf(text, sizeof text, "P%d  LEVEL %d", G.cur + 1, G.level + 1);
    else snprintf(text, sizeof text, "LEVEL %d", G.level + 1);
    plain(d, text, 192.0f, 5.0f, 7.0f, VP_TEXT_CENTER, 0, 200, 255);
    if (G.players == 2 && G.state == INTRO && !G.demo) {
        snprintf(text, sizeof text, "PLAYER %d", G.cur + 1);
        plain(d, text, 192.0f, 60.0f, 18.0f, VP_TEXT_CENTER, 255, 255, 255);
    }
    snprintf(text, sizeof text, "X%d", G.lives < 0 ? 0 : G.lives);
    plain(d, text, 376.0f, 5.0f, 9.0f, VP_TEXT_RIGHT, 255, 255, 0);
    /* Not the original's, which shows nothing: whether the superzapper is
     * still there ('szap_avail'), dim once only its second, single zap is. */
    if (G.szap_avail > 0) plain(d, "ZAP", 376.0f, 18.0f, 6.0f, VP_TEXT_RIGHT, 0, 255, 255);
    else if (G.szap_avail == 0) plain(d, "ZAP", 376.0f, 18.0f, 6.0f, VP_TEXT_RIGHT, 90, 40, 40);
}

/* drawmsg: centred, 80 lines down (a second line at 100), leaning to and
 * fro; once 'msgtim1' has run out the rows of its letters come apart. */
static void draw_message(void)
{
    int   lean = (int)((G.frames * 2) & 255) - 127;
    float xshear, yshear;
    int   line;

    if (!G.msg[0]) return;
    if (lean < 0) lean = -lean;
    view_level(G.bright[BRIGHT_TEXT]);
    xshear = (float)(lean - 63) / 64.0f;
    yshear = -xshear / 2.0f;
    for (line = 0; line < 2 && G.msg[line]; line++) {
        float x = 192.0f - (float)((int)strlen(G.msg[line]) * t2k_afont.advance) / 2.0f;
        float y = 80.0f + 20.0f * (float)line;
        view_text(&t2k_afont, G.msg[line], x, y, 1.0f, G.msgys, xshear, yshear);
    }
}

/* --- the cabinet: attract, coin, game type, start level ---------------------------- */

/* o1s1..o1s3 */
static const char *const mode_names[MODE_COUNT] = {"TRADITIONAL", "TEMPEST PLUS", "TEMPEST 2000"};
static const char letters[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ .";

static void set_mode(int mode)
{
    G.mode = mode;
    G.t2k = mode == MODE_2000;
    G.solids = mode != MODE_TRADITIONAL;
}

static void enter(int state)
{
    G.state = state;
    G.state_time = 0;
    G.menu_spin = 0;
}

/* lsel: the title, the scores, then the game playing itself, round and round. */
static void enter_attract(void)
{
    G.demo = 0;
    G.attract_page = 0;
    set_mode(MODE_2000);
    G.table_count = vp_hiscore_load(G.table);
    sound_loop_stop();
    sound_tune(TUNE_THEME);
    enter(ATTRACT);
}

/* The spinner, or left and right, as steps through a menu. Menus go by their
 * own controls only (the spinner, the arrow keys or any stick, START 1,
 * Enter, the keypad's Enter: vpbind.h): what the service menu lays out is the game's,
 * and never counts in a menu. The spinner is
 * scaled as in the game: `per_lane` steps for what moves the claw a lane. */
static int menu_step(vp_input *in, float per_lane)
{
    int step = 0;
    /* Turning the way that takes the claw to lower lanes goes on through a list. */
    G.menu_spin -= (float)vp_input_delta(in, "p1_dialx") * SPIN_LANES * G.sensitivity * per_lane;
    while (G.menu_spin >=  1.0f) { G.menu_spin -= 1.0f; step++; }
    while (G.menu_spin <= -1.0f) { G.menu_spin += 1.0f; step--; }
    if (vpb_menu_right_pressed()) step++;
    if (vpb_menu_left_pressed())  step--;
    return step;
}

static int menu_chosen(vp_input *in)
{
    return menu_confirm(in)
           || G.state_time >= MENU_TIMEOUT;
}

static void save_player(void)
{
    struct player *p = &G.pl[G.cur];
    p->score = G.score; p->lives = G.lives; p->level = G.level;
    p->bonum = G.bonum; p->laser = G.laser; p->jenable = G.jenable;
    p->had_droid = G.had_droid;
    p->start_bonus = G.start_bonus;
}

static void load_player(int who)
{
    const struct player *p = &G.pl[who];
    G.cur = who;
    G.score = p->score; G.lives = p->lives;
    G.bonum = p->bonum; G.laser = p->laser; G.jenable = p->jenable;
    G.had_droid = p->had_droid;
    G.start_bonus = p->start_bonus;
    set_level(p->level);
}

/* slebon: what starting at a level is worth, paid when its first web is done. */
static int level_bonus(int level)
{
    return level * (level * 200 + 2600);
}

static void start_game(void)
{
    int i;

    G.msg[0] = NULL;
    G.rng ^= G.frames * 2654435761u;
    G.start_bonus_value = level_bonus(G.level);
    for (i = 0; i < 2; i++) {
        memset(&G.pl[i], 0, sizeof G.pl[i]);
        G.pl[i].lives = START_LIVES;
        G.pl[i].level = G.level;
        G.pl[i].start_bonus = 1;        /* waitfor: 'move #1,startbonus' */
    }
    load_player(0);
    G.carry = -1;
    start_level(G.level);
}

/* setauto: Tempest 2000, three lives, a level from 8 to 23, and nobody at
 * the controls. */
static void start_demo(void)
{
    set_mode(MODE_2000);
    G.demo = 1;
    G.demo_time = 0;
    G.droid_game = 0;
    G.players = 1;
    G.beastly = 0;
    set_level((rannum() & 15) + 7);
    start_game();
    G.start_bonus = 0;
}

/* A coin and START end whatever attract mode is showing. */
static int coin_start(vp_input *in)
{
    if (vp_input_pressed(in, "start2") && vp_credits_take(&G.credits, 2)) G.players = 2;
    else if (vp_input_pressed(in, "start1") && vp_credits_take(&G.credits, 1)) G.players = 1;
    else return 0;
    vp_lamps_set(0);
    G.beastly = 0;
    G.demo = 0;
    G.muted = 0;
    sound_mute(0);
    sound_loop_stop();
    sound_tune(TUNE_THEME);
    sound_fx(FX_SEXY_YES, 0);
    memset(G.objs, 0, sizeof G.objs);
    G.msg[0] = NULL;
    set_level(0);                       /* its web turns behind the choices */
    enter(MODE_SELECT);
    return 1;
}

static void update_attract(vp_input *in)
{
    vp_credits_blink(&G.credits, (double)G.frames / 60.0);
    if (coin_start(in)) return;
    if (G.state_time < (G.attract_page == 0 ? TITLE_FRAMES : ATTRACT_PAGE)) return;
    G.state_time = 0;
    if (++G.attract_page >= 3) start_demo();
}

static void update_mode_select(vp_input *in)
{
    /* Listed from the newest down, the other way round from the original. */
    int mode = G.mode - menu_step(in, 1.0f);

    if (mode < 0) mode = 0;
    if (mode >= MODE_COUNT) mode = MODE_COUNT - 1;
    if (mode != G.mode) { set_mode(mode); set_level(0); sound_fx(FX_TINK, 0); G.state_time = 0; }
    if (!menu_chosen(in)) return;
    sound_fx(FX_SEXY_YES, 0);
    set_level(0);
    G.droid_game = 0;
    enter(G.mode == MODE_PLUS ? PLUS_SELECT : LEVEL_SELECT);
}

/* option7, "select options for this game": Tempest Plus alone, or with an
 * A.I. droid from start to finish. (Its third choice, two players at once,
 * is not built.) */
static void update_plus_select(vp_input *in)
{
    int choice = G.droid_game - menu_step(in, 1.0f);

    if (choice < 0) choice = 0;
    if (choice > 1) choice = 1;
    if (choice != G.droid_game) { G.droid_game = choice; sound_fx(FX_TINK, 0); G.state_time = 0; }
    if (!menu_chosen(in)) return;
    sound_fx(FX_SEXY_YES, 0);
    enter(LEVEL_SELECT);
}

/* The original's start level choice: every second level up to where this
 * game type has been played to. */
static void update_level_select(vp_input *in)
{
    int level = G.level + 2 * menu_step(in, 1.0f);    /* a level per lane's worth of turn */

    if (level < 0) level = 0;
    if (level > G.best_level[G.mode]) level = G.best_level[G.mode];
    if (level != G.level) { set_level(level); sound_fx(FX_TINK, 0); G.state_time = 0; }
    /* waitfor: the original's Option button starts Beastly Mode, once the
     * game has been beaten and the cartridge remembers it. A cabinet does
     * not know who is playing: here the Superzapper button held while the
     * level is taken starts it, for whoever knows. The page for beating the
     * game tells. */
    if (!menu_chosen(in)) return;
    G.beastly = G.state_time < MENU_TIMEOUT && control_held(in, G.btn_zap);
    start_game();
}

/* A Tempest cabinet has no stick: the spinner picks the letter, START 1 (or Enter) takes it. */
static void update_initials(vp_input *in)
{
    int n = (int)sizeof letters - 1;
    int step = menu_step(in, 2.0f);

    G.initial_letter = ((G.initial_letter + step) % n + n) % n;
    G.initials[G.initial_pos] = letters[G.initial_letter];
    if (menu_confirm(in)) {
        sound_fx(FX_TINK, 0);
        G.initial_pos++;
        G.state_time = 0;
    }
    if (G.initial_pos < 3 && G.state_time < INITIALS_TIMEOUT) return;
    G.initials[3] = 0;
    vp_hiscore_insert(G.initials, G.score);
    G.over_player++;
    end_game();                         /* the other player's turn, or back to the title */
}

/* moveclaw */
static void update_playing(vp_input *in)
{
    if (G.rez > 0) G.rez--;
    run_wave();
    superzap(in);
    fire(in);
    jump_claw(in);
    move_claw(in);
    run_shots();
    run_objects();
    move_view();
}

/* zoom1 and zoom2, with the claw's czoom1 and czoom2: every enemy dealt
 * with, the claw flies down the web (spikes are still there) and the viewer
 * with it. Past the far end it starts to turn and is drawn with three more
 * of itself, coming apart; once the web is behind the viewer it is switched
 * off and the claw pulls away, until it is far enough for the next web. */
static void update_zoom(vp_input *in)
{
    int on_web = ZI(G.claw_z) < Z_BOTTOM;

    if (on_web) {                       /* go_con: control while still on the web */
        fire(in);
        move_claw(in);
        run_shots();
    }
    run_objects();
    move_view();
    if (G.state != ZOOM) return;
    sound_zoom_up(G.state_time);
    G.claw_z += G.zoom_vel;
    if (!on_web) {                      /* czoom2 */
        G.szap_on = 0;
        G.claw_spin += 4;
        G.claw_images -= ZOOM_SPACING;
        G.claw_z += G.claud;
    }
    G.warp_add += WARP_ADD;
    G.vp_z += G.zoom_vel;
    G.zoom_dist += G.zoom_vel;
    G.zoom_vel += ZOOM_ACCEL;
    if (G.zoom_web_off) {               /* zoom2 */
        G.claw_images -= ZOOM_SPACING;
        G.claud += ZOOM_CLAUD;
    } else if (ZI(G.vp_z) > ZOOM_WEB_GONE) {
        G.zoom_web_off = 1;
    }
    if (ZI(G.claw_z) < ZOOM_CLAW_GONE) return;

    /* zoo2off: the next web, as far ahead as this one is behind. */
    if (G.start_bonus > 0) {            /* swip1: the bonus for where the game was started */
        G.score += G.start_bonus_value;
        G.start_bonus = -1;
    }
    if (G.t2k && G.level + 1 >= t2k_level_count) {      /* sweb0: the player has beaten Tempest 2000 */
        sound_loop_stop();
        sound_tune(TUNE_THEME);
        sound_fx(FX_EXCELLENT, 0);
        memset(G.objs, 0, sizeof G.objs);
        G.msg[0] = NULL;
        enter(VICTORY);
        return;
    }
    start_level(G.level + 1);
    G.state = WARPIN;
    G.vp_z = -G.zoom_dist;
    G.warp_add = -G.warp_add;
    G.wason = WASON_FRAMES;
    sound_fx(FX_WARP, 0);               /* twice, as the original has it */
    sound_fx(FX_WARP, 0);
    say2("superzapper", "recharge");    /* sweb00 */
    G.msgtim1 = 250;
}

/* zoom3: the new web comes up out of the distance, the viewer slowing as
 * it had gathered speed, and then the claw appears on it. */
static void update_warpin(void)
{
    int step;

    if (G.wason && --G.wason == 0) {
        sound_fx(FX_RECHARGE, 0);
        sound_fx(FX_RECHARGE, 0);
    }
    for (step = 0; step < WARPIN_SPEED; step++) {
        G.warp_add += WARP_ADD;
        G.zoom_vel -= ZOOM_ACCEL;
        G.vp_z += G.zoom_vel;
        G.zoom_dist -= G.zoom_vel;
        if (G.zoom_dist > 0) continue;
        G.vp_z = 0;
        G.warp_add = 0;
        G.rez = REZ_FRAMES;
        G.state = PLAYING;
        G.state_time = 0;
        return;
    }
}

/* dobeastly: the page stands until fire, then the game is over ('lives'
 * to -1). With two players the other plays on. */
static void update_victory(vp_input *in)
{
    int other = 1 - G.cur;

    if (G.state_time < VICTORY_HOLD) return;
    if (!G.selftest && G.state_time < VICTORY_TIMEOUT && !menu_confirm(in)) return;
    if (!G.demo && !G.selftest && G.best_level[G.mode] < 98) { G.best_level[G.mode] = 98; save_progress(); }
    G.lives = -1;
    save_player();
    if (G.demo) { enter_attract(); return; }
    if (G.players == 2 && G.pl[other].lives >= 0) {
        load_player(other);
        start_level(G.level);
        return;
    }
    G.over_player = 0;
    end_game();
}

/* What the original keeps in its cartridge's memory ('t2k_max', 'trad_max'):
 * how far each game type has been played, for the start level choice. */
static const char *progress_path(char *buf, size_t size)
{
    snprintf(buf, size, "%s/progress.txt", vp_save_dir());
    return buf;
}

static void load_progress(void)
{
    char path[1024];
    FILE *f = fopen(progress_path(path, sizeof path), "r");
    int i, floor_level = (G.unlocked_level - 1) & ~1;

    if (f) {
        for (i = 0; i < MODE_COUNT; i++)
            if (fscanf(f, "%d", &G.best_level[i]) != 1) G.best_level[i] = 0;
        fclose(f);
    }
    for (i = 0; i < MODE_COUNT; i++) {
        if (G.best_level[i] < floor_level) G.best_level[i] = floor_level;
        if (G.best_level[i] < 0 || G.best_level[i] > 98) G.best_level[i] = 0;
    }
}

static void save_progress(void)
{
    char path[1024];
    FILE *f = fopen(progress_path(path, sizeof path), "w");
    if (!f) return;
    fprintf(f, "%d %d %d\n", G.best_level[0], G.best_level[1], G.best_level[2]);
    fclose(f);
}

/* A machine nobody has played yet shows a table to beat. The cartridge's
 * own is empty; these names and scores are made up. */
static void default_scores(void)
{
    static const struct { const char *name; int score; } made_up[10] = {
        {"YAK", 20000}, {"JEF", 18000}, {"LLA", 16000}, {"MAX", 14000}, {"T2K", 12000},
        {"ZAP", 10000}, {"WEB", 8000}, {"CLW", 6000}, {"PUP", 4000}, {"FLP", 2000}
    };
    vp_score table[VP_HISCORE_MAX];
    int i;

    if (vp_hiscore_load(table) > 0) return;
    for (i = 0; i < 10; i++) vp_hiscore_insert(made_up[i].name, made_up[i].score);
}

/* loiter / xquick: a life gone. The level starts again. */
static void lose_life(void)
{
    int other = 1 - G.cur;

    G.lives--;
    if (G.demo) {
        if (G.lives < 0) enter_attract(); else start_level(G.level);
        return;
    }
    if (G.lives < 0) {
        int reached = G.level & ~1;
        if (reached > G.best_level[G.mode]) { G.best_level[G.mode] = reached; save_progress(); }
    }
    save_player();
    if (G.checkpoints && G.players == 1 && G.lives >= 0) {
        int i;
        G.carry = G.wave_done ? 0 : G.wave_dur + 1;
        memset(G.carry_spikes, 0, sizeof G.carry_spikes);
        for (i = 0; i < OBJECT_SLOTS; i++) {
            const obj *o = &G.objs[i];
            if (o->kind == K_SPIKE && o->lane < 32) G.carry_spikes[o->lane] = o->len;
            else if (o->enemy && o->kind != K_ASHOT && o->kind != K_PSPARK) G.carry++;
        }
        /* What a tanker let out counts twice over: dying must not make
         * the level longer than it was. */
        if (G.carry > G.life_owed) G.carry = G.life_owed;
    }
    if (G.players == 2 && G.pl[other].lives >= 0) load_player(other);     /* the other's turn */
    else if (G.lives < 0) {
        sound_tune(TUNE_THEME);
        enter(GAME_OVER);
        return;
    }
    start_level(G.level);
}

/* After the game: each player who made the table gives their initials. */
static void end_game(void)
{
    for (; G.over_player < G.players; G.over_player++) {
        G.score = G.pl[G.over_player].score;
        G.rank = vp_hiscore_rank(G.score);
        if (G.rank < 0) continue;
        memcpy(G.initials, "A  ", 4);
        G.initial_pos = G.initial_letter = 0;
        enter(INITIALS);
        return;
    }
    G.over_player = 0;
    enter_attract();
}

/* game.ini's names for the brightnesses, in the order of the BRIGHT_ names. */
static const char *const bright_key[BRIGHT_COUNT] = {
    "brightness_web", "brightness_stars", "brightness_enemies", "brightness_effects",
    "brightness_claw", "brightness_shots", "brightness_text", "brightness_title"
};

/* The file the game writes its settings to: in save/, which an update of
 * the game leaves alone. */
static const char *settings_path(char *buf, size_t size)
{
    snprintf(buf, size, "%s/settings.ini", vp_save_dir());
    return buf;
}

static void read_settings(const char *path);

/* The settings: what the game starts with, then game.ini next to the
 * executable (what the package ships, or its owner has written into it),
 * then save/settings.ini (what the service menu saved), each over the one
 * before. */
static void load_settings(void)
{
    /* Not everything as bright as the beam can: the claw, its shots and
     * the title stand out from the rest. */
    static const int bright_default[BRIGHT_COUNT] = { 60, 60, 85, 75, 100, 100, 70, 100 };
    char path[1024];
    int i;

    G.sensitivity = 1.0f;
    G.digital = 1.0f;
    G.volume = G.music_volume = G.effects_volume = 1.0f;
    G.voice_volume = 0.9f;
    G.attract_sound = 1;
    G.beastly_ini = 1;
    G.unlocked_level = 15;              /* 't2k_max' as the cartridge leaves the factory */
    G.checkpoints = 1;
    G.star_count = 64;
    for (i = 0; i < BRIGHT_COUNT; i++) G.bright[i] = (float)bright_default[i] / 100.0f;
    strcpy(G.btn_fire, service_default_button[SERVICE_FIRE]);
    strcpy(G.btn_zap, service_default_button[SERVICE_ZAP]);
    strcpy(G.btn_jump, service_default_button[SERVICE_JUMP]);
    strcpy(G.btn_left, service_default_button[SERVICE_LEFT]);
    strcpy(G.btn_right, service_default_button[SERVICE_RIGHT]);
    read_settings(vp_asset_path("game.ini", path, sizeof path));
    read_settings(settings_path(path, sizeof path));
    vpb_normalize(G.btn_fire, sizeof G.btn_fire);       /* advmame.rc names of an older file */
    vpb_normalize(G.btn_zap, sizeof G.btn_zap);
    vpb_normalize(G.btn_jump, sizeof G.btn_jump);
    vpb_normalize(G.btn_left, sizeof G.btn_left);
    vpb_normalize(G.btn_right, sizeof G.btn_right);
}

/* One file of settings, read into the game. */
static void read_settings(const char *path)
{
    char line[512], key[64], value[SERVICE_CONTROLS_LENGTH];
    FILE *f = fopen(path, "r");
    int i;

    if (!f) return;
    while (fgets(line, sizeof line, f)) {
        if (sscanf(line, " %63[A-Za-z_] = %127[^ ;#\r\n]", key, value) != 2) continue;
        if (!strcmp(key, "sensitivity"))         G.sensitivity = (float)atof(value);
        else if (!strcmp(key, "digital_sensitivity")) G.digital = (float)atof(value);
        else if (!strcmp(key, "volume"))         G.volume = (float)atof(value) / 100.0f;
        else if (!strcmp(key, "music_volume"))   G.music_volume = (float)atof(value) / 100.0f;
        else if (!strcmp(key, "effects_volume")) G.effects_volume = (float)atof(value) / 100.0f;
        else if (!strcmp(key, "voice_volume"))   G.voice_volume = (float)atof(value) / 100.0f;
        else if (!strcmp(key, "attract_sound"))  G.attract_sound = value[0] == 'y' || value[0] == '1';
        else if (!strcmp(key, "free_play"))      G.free_play = value[0] == 'y' || value[0] == '1';
        else if (!strcmp(key, "unlocked_level")) G.unlocked_level = atoi(value);
        else if (!strcmp(key, "checkpoints"))    G.checkpoints = value[0] == 'y' || value[0] == '1';
        else if (!strcmp(key, "beastly_mode"))   G.beastly_ini = value[0] == 'y' || value[0] == '1';
        else if (!strcmp(key, "fire_button"))    snprintf(G.btn_fire, sizeof G.btn_fire, "%s", value);
        else if (!strcmp(key, "zap_button"))     snprintf(G.btn_zap, sizeof G.btn_zap, "%s", value);
        else if (!strcmp(key, "jump_button"))    snprintf(G.btn_jump, sizeof G.btn_jump, "%s", value);
        else if (!strcmp(key, "left_button"))    snprintf(G.btn_left, sizeof G.btn_left, "%s", value);
        else if (!strcmp(key, "right_button"))   snprintf(G.btn_right, sizeof G.btn_right, "%s", value);
        else if (!strcmp(key, "stars")) {
            G.star_count = atoi(value);
            if (G.star_count < 0) G.star_count = 0;
            if (G.star_count > STAR_MAX) G.star_count = STAR_MAX;
        }
        for (i = 0; i < BRIGHT_COUNT; i++)
            if (!strcmp(key, bright_key[i])) G.bright[i] = (float)atof(value) / 100.0f;
    }
    fclose(f);
}

/* The service menu's side of the settings: what it shows, what it gives
 * back, and the file it saves them in. */
static void settings_to_service(service_settings *v)
{
    const char *const buttons[SERVICE_BUTTONS] = { G.btn_fire, G.btn_zap, G.btn_jump, G.btn_left, G.btn_right };
    int i;

    v->sensitivity    = G.sensitivity;
    v->digital_sensitivity = G.digital;
    v->volume         = (int)(G.volume * 100.0f + 0.5f);
    v->music_volume   = (int)(G.music_volume * 100.0f + 0.5f);
    v->effects_volume = (int)(G.effects_volume * 100.0f + 0.5f);
    v->voice_volume   = (int)(G.voice_volume * 100.0f + 0.5f);
    v->attract_sound  = G.attract_sound;
    v->free_play      = G.free_play;
    v->checkpoints    = G.checkpoints;
    v->beastly_mode   = G.beastly_ini;
    v->unlocked_level = G.unlocked_level;
    v->stars          = G.star_count;
    for (i = 0; i < BRIGHT_COUNT; i++) v->brightness[i] = (int)(G.bright[i] * 100.0f + 0.5f);
    for (i = 0; i < SERVICE_BUTTONS; i++) snprintf(v->button[i], sizeof v->button[i], "%s", buttons[i]);
}

/* Everything here works from the next frame on; nothing needs a restart. */
static void settings_from_service(const service_settings *v)
{
    char *const buttons[SERVICE_BUTTONS] = { G.btn_fire, G.btn_zap, G.btn_jump, G.btn_left, G.btn_right };
    int i;

    G.sensitivity    = v->sensitivity;
    G.digital        = v->digital_sensitivity;
    G.volume         = (float)v->volume / 100.0f;
    G.music_volume   = (float)v->music_volume / 100.0f;
    G.effects_volume = (float)v->effects_volume / 100.0f;
    G.voice_volume   = (float)v->voice_volume / 100.0f;
    G.attract_sound  = v->attract_sound;
    G.free_play      = v->free_play;
    G.credits.free_play = v->free_play;
    G.checkpoints    = v->checkpoints;
    G.beastly_ini    = v->beastly_mode;
    G.unlocked_level = v->unlocked_level;
    G.star_count     = v->stars;
    for (i = 0; i < BRIGHT_COUNT; i++) G.bright[i] = (float)v->brightness[i] / 100.0f;
    for (i = 0; i < SERVICE_BUTTONS; i++) snprintf(buttons[i], sizeof G.btn_fire, "%s", v->button[i]);
    /* The start levels again from what has been played. */
    for (i = 0; i < MODE_COUNT; i++) G.best_level[i] = 0;
    load_progress();
}

/* save/settings.ini written: every setting the service menu has. */
static int save_settings(void)
{
    char path[1024];
    FILE *f = fopen(settings_path(path, sizeof path), "w");
    int i;

    if (!f) return 0;
    fprintf(f, "; Written by the service menu. What stands here goes before game.ini.\n");
    fprintf(f, "sensitivity = %.2f\n", (double)G.sensitivity);
    fprintf(f, "digital_sensitivity = %.2f\n", (double)G.digital);
    fprintf(f, "volume = %d\n", (int)(G.volume * 100.0f + 0.5f));
    fprintf(f, "music_volume = %d\n", (int)(G.music_volume * 100.0f + 0.5f));
    fprintf(f, "effects_volume = %d\n", (int)(G.effects_volume * 100.0f + 0.5f));
    fprintf(f, "voice_volume = %d\n", (int)(G.voice_volume * 100.0f + 0.5f));
    fprintf(f, "attract_sound = %s\n", G.attract_sound ? "yes" : "no");
    fprintf(f, "free_play = %s\n", G.free_play ? "yes" : "no");
    fprintf(f, "unlocked_level = %d\n", G.unlocked_level);
    fprintf(f, "checkpoints = %s\n", G.checkpoints ? "yes" : "no");
    fprintf(f, "beastly_mode = %s\n", G.beastly_ini ? "yes" : "no");
    fprintf(f, "fire_button = %s\n", G.btn_fire);
    fprintf(f, "zap_button = %s\n", G.btn_zap);
    fprintf(f, "jump_button = %s\n", G.btn_jump);
    fprintf(f, "left_button = %s\n", G.btn_left);
    fprintf(f, "right_button = %s\n", G.btn_right);
    fprintf(f, "stars = %d\n", G.star_count);
    for (i = 0; i < BRIGHT_COUNT; i++)
        fprintf(f, "%s = %d\n", bright_key[i], (int)(G.bright[i] * 100.0f + 0.5f));
    return fclose(f) == 0;
}

/* The service menu, open: the game stands still behind it. */
static void open_service(void)
{
    service_settings now;

    settings_to_service(&now);
    service_open(&now, SPIN_LANES);
    G.service = 1;
    G.muted = 0;                        /* the menu makes its own sound, and little of it */
    sound_mute(0);
    sound_loop_stop();
    sound_tune(0);
}

static void update_service(vp_input *in, int hotkey)
{
    int result = service_update(in, hotkey);

    if (result == SERVICE_OPEN) return;
    G.service = 0;
    G.notice = "SETTINGS NOT SAVED";    /* left by CANCEL or the menu's key */
    if (result == SERVICE_SAVED) {
        settings_from_service(service_get());
        G.notice = save_settings() ? "SETTINGS SAVED" : "CANNOT SAVE SETTINGS";
        sound_fx(FX_SEXY_YES, 0);
    }
    G.notice_time = NOTICE_FRAMES;
    sound_volumes(G.volume, G.effects_volume, G.voice_volume, G.music_volume);
    /* The tune the game was playing, again. */
    sound_tune(G.state >= INTRO && G.state <= WARPIN ? t2k_webtunes[(G.level >> 4) & 7] : TUNE_THEME);
}

void game_init(int vertical)
{
    memset(&G, 0, sizeof G);
#ifdef T2K_ROM_DATA
    G.no_rom = !rom_load();
#endif
    load_settings();
    G.carry = -1;
    G.rng = 0x2000;
    view_init(vertical);
    vp_credits_init(&G.credits);
    if (G.free_play) G.credits.free_play = 1;
    load_progress();
    default_scores();
    init_stars();
    set_mode(MODE_2000);
    set_level(0);
    G.table_count = vp_hiscore_load(G.table);
    G.rank = -1;
    G.state = ATTRACT;
}

void game_unlock(int level)
{
    int i;
    for (i = 0; i < MODE_COUNT; i++) G.best_level[i] = (level - 1) & ~1;
}

/* One 60 Hz step of the game. */
static void step(vp_input *in)
{
    G.frames++;
    G.state_time++;
    if (G.notice_time > 0) G.notice_time--;
    if (G.holiday > 0) G.holiday--;
    run_message();
    if (G.bolt_time > 0) G.bolt_time--;
    {
        int quiet = G.selftest || (!G.attract_sound && (G.state == ATTRACT || G.demo));
        if (quiet != G.muted) { G.muted = quiet; sound_mute(quiet); }
    }
    if (vp_credits_update(&G.credits, in)) sound_fx(FX_GET_POWERUP, 0);     /* a coin: not the original's, it has no slot */
    if (G.demo) {
        vp_credits_blink(&G.credits, (double)G.frames / 60.0);
        if (coin_start(in)) return;
        if (++G.demo_time >= DEMO_FRAMES) { enter_attract(); return; }  /* back to the title */
    }

    switch (G.state) {
    case ATTRACT:      update_attract(in);      break;
    case MODE_SELECT:  update_mode_select(in);  break;
    case PLUS_SELECT:  update_plus_select(in);  break;
    case LEVEL_SELECT: update_level_select(in); break;
    case INITIALS:     update_initials(in);     break;
    case INTRO:
        if (G.state_time >= INTRO_FRAMES) G.state = PLAYING;
        break;
    case PLAYING: update_playing(in); break;
    case ZOOM:    update_zoom(in);    break;
    case WARPIN:  update_warpin();    break;
    case VICTORY: update_victory(in); break;
    case CAUGHT:                        /* take_me_to_your_leader */
        G.claw_z += 2 << 16;
        if (G.catcher) { G.catcher->z = G.claw_z; G.catcher->x = G.claw_x; G.catcher->y = G.claw_y; }
        if (ZI(G.claw_z) >= Z_BOTTOM) lose_life();
        break;
    case ZAPPED:
        if (G.state_time >= ZAP_FRAMES + SNATCH_FRAMES) lose_life();
        break;
    case GAME_OVER:
        if (G.state_time >= GAME_OVER_FRAMES || vp_input_pressed(in, "start1")) { G.over_player = 0; end_game(); }
        break;
    }
}

/* --- the self test ----------------------------------------------------------------
 *
 * T2K_SELFTEST (see main.c): the game plays itself from the first level to
 * the last, many steps a frame, with the demo's hands on the controls and
 * lives that never run out. For every level a line goes to stderr: how long
 * it took, which enemies its wave asks for and which were seen, what killed
 * the claw how often. Anything out of bounds is said, and a level that will
 * not end is ended and said. */

#define SELFTEST_STUCK (60 * 300)       /* frames: five minutes of play */

static struct {
    int      started, level, frames, peak, powerups, stuck, bad;
    unsigned wanted, seen;              /* bit per wave type */
    int      hits[5];                   /* deaths: caught, shot, fried, spike, other */
    int      launched[12], blocked[12]; /* per wave entry: launches, frames ready without room */
    int      total_hits, total_stuck, total_bad, total_missing;
} ST;

static const char *const wave_names[12] = {
    "flipper", "tanker", "spiker", "fuseball", "pulsar", "fusetanker", "pulsartanker",
    "sflip2", "mirror", "adroid", "beast", "sflip3"
};

static void selftest_blocked(int entry)  { if (entry < 12) ST.blocked[entry]++; }
static void selftest_launched(int entry) { if (entry < 12) ST.launched[entry]++; }

static void selftest_death(const char *why)
{
    static const char *const causes[4] = {"caught you!", "shot you!", "fried you!", "spike"};
    int i;

    if (!G.selftest) return;
    for (i = 0; i < 4 && strcmp(why, causes[i]); i++) {}
    ST.hits[i]++;
    if (G.lives < 1) G.lives = 1;       /* there is always one more */
}

/* Which wave type an object on the web is evidence of. */
static int selftest_type(const obj *o)
{
    switch (o->kind) {
    case K_FLIPPER:  return o->type == 5 ? W_BEAST : o->type == 2 ? W_SFLIP2
                          : o->type == 3 || o->type == 4 ? W_SFLIP3 : W_FLIPPER;
    case K_TANKER:   return o->type == 1 ? W_FUSETANKER : o->type == 2 ? W_PULSARTANKER : W_TANKER;
    case K_SPIKER:   return W_SPIKER;
    case K_FUSEBALL: return W_FUSEBALL;
    case K_PULSAR:   return W_PULSAR;
    case K_MIRROR:   return W_MIRROR;
    case K_ADROID:   return W_ADROID;
    default:         return -1;
    }
}

static void selftest_names(unsigned bits)
{
    int i;
    for (i = 0; i < 12; i++)
        if (bits & (1u << i)) fprintf(stderr, " %s", wave_names[i]);
}

static void selftest_level_begin(void)
{
    int i;

    ST.level = G.level;
    ST.frames = ST.peak = ST.powerups = ST.stuck = ST.bad = 0;
    ST.wanted = ST.seen = 0;
    memset(ST.launched, 0, sizeof ST.launched);
    memset(ST.blocked, 0, sizeof ST.blocked);
    for (i = 0; i < G.wave_entries; i++) ST.wanted |= 1u << G.wave[i].type;
    if (!G.t2k) ST.wanted &= (1u << (W_PULSARTANKER + 1)) - 1;      /* launch1 */
}

static void selftest_level_end(void)
{
    unsigned missing = ST.wanted & ~ST.seen;
    int i, hits = 0;

    for (i = 0; i < 5; i++) hits += ST.hits[i];
    fprintf(stderr, "level %3d  web %2d  %5d frames  peak %2d objects  wave:", ST.level + 1,
            (int)(G.web - t2k_webs), ST.frames, ST.peak);
    selftest_names(ST.wanted);
    if (missing) { fprintf(stderr, "  NOT SEEN:"); selftest_names(missing); ST.total_missing++; }
    fprintf(stderr, "  died: caught %d shot %d fried %d spike %d other %d",
            ST.hits[0], ST.hits[1], ST.hits[2], ST.hits[3], ST.hits[4]);
    fprintf(stderr, "  launched:");
    for (i = 0; i < G.wave_entries && i < 12; i++)
        fprintf(stderr, " %s %d/%d", wave_names[G.wave[i].type], ST.launched[i], ST.blocked[i]);
    if (ST.stuck) fprintf(stderr, "  STUCK, ended by the test");
    if (ST.bad) fprintf(stderr, "  OUT OF BOUNDS %d times", ST.bad);
    fprintf(stderr, "\n");
    ST.total_hits += hits;
    memset(ST.hits, 0, sizeof ST.hits);
    ST.total_stuck += ST.stuck;
    ST.total_bad += ST.bad;
}

/* What must hold in every frame of play. */
static void selftest_check(void)
{
    int i, count = 0;

    if (G.claw_pos < 0 || claw_lane() >= G.web->lanes) {
        if (!ST.bad++) fprintf(stderr, "  level %d: claw at %f of %d lanes\n", G.level + 1, (double)G.claw_pos, G.web->lanes);
    }
    for (i = 0; i < OBJECT_SLOTS; i++) {
        const obj *o = &G.objs[i];
        int type = selftest_type(o);
        if (!o->kind) continue;
        count++;
        if (type >= 0) ST.seen |= 1u << type;
        if (o->kind <= K_POWERUP && o->kind != K_TRAIL && (o->lane < 0 || o->lane >= G.web->lanes)) {
            if (!ST.bad++) fprintf(stderr, "  level %d: kind %d in lane %d of %d\n", G.level + 1, o->kind, o->lane, G.web->lanes);
        }
        if (o->x != o->x || o->y != o->y || ZI(o->z) < -1000 || ZI(o->z) > 3000) {
            if (!ST.bad++) fprintf(stderr, "  level %d: kind %d at %f %f %d\n", G.level + 1, o->kind, (double)o->x, (double)o->y, ZI(o->z));
        }
    }
    if (count > ST.peak) ST.peak = count;
}

static void selftest_step(vp_input *in)
{
    int before = G.state, i;

    if (!ST.started) {
        ST.started = 1;
        G.players = 1;
        G.droid_game = 0;
        set_level(ST.level);
        start_game();
        selftest_level_begin();
        fprintf(stderr, "self test: %s%s, %d levels\n", mode_names[G.mode], G.beastly ? ", BEASTLY" : "", t2k_level_count);
    }
    step(in);
    if (G.state == PLAYING || G.state == ZOOM) {
        ST.frames++;
        selftest_check();
    }
    if (G.state == PLAYING && ST.frames > SELFTEST_STUCK && !ST.stuck) {
        ST.stuck = 1;
        fprintf(stderr, "  level %d will not end; wave done %d, still to launch %d, claw in lane %d of %d, on the web:",
                G.level + 1, G.wave_done, G.wave_dur, claw_lane(), G.web->lanes);
        for (i = 0; i < OBJECT_SLOTS; i++)
            if (G.objs[i].kind && G.objs[i].enemy)
                fprintf(stderr, " [kind %d type %d mode %d lane %d z %d far %d]", G.objs[i].kind, G.objs[i].type,
                        G.objs[i].mode, G.objs[i].lane, ZI(G.objs[i].z), G.objs[i].far);
        fprintf(stderr, "\n");
        memset(G.objs, 0, sizeof G.objs);
        memset(G.spikes, 0, sizeof G.spikes);
        G.spike_count = 0;
        G.wave_done = 1;
    }
    /* A level done: the next one arriving, or the page for the last. */
    if (before == ZOOM && (G.state == WARPIN || G.state == VICTORY)) {
        int last = G.state == VICTORY || (!G.t2k && ST.level + 1 >= t2k_level_count);
        selftest_level_end();
        if (last) {
            fprintf(stderr, "self test done: %s, score %d, lives %d; died %d times, "
                            "%d levels stuck, %d out of bounds, %d levels with an enemy not seen\n",
                    G.state == VICTORY ? "the victory page came" : "the last level is done", G.score, G.lives,
                    ST.total_hits, ST.total_stuck, ST.total_bad, ST.total_missing);
            exit(0);
        }
        selftest_level_begin();
    }
}

void game_selftest(int speed, int mode, int beastly, int level)
{
    G.selftest = speed > 0 ? speed : 1;
    set_mode(mode >= 0 && mode < MODE_COUNT ? mode : MODE_2000);
    G.beastly = beastly;
    ST.level = level > 0 ? level - 1 : 0;
}

void game_update(vp_game *g, float dt)
{
    vp_input *in = vp_game_input(g);
    int i;
    (void)dt;

    if (!G.sound_ready) {
        G.sound_ready = 1;
        sound_init(vp_game_audio(g));
        sound_volumes(G.volume, G.effects_volume, G.voice_volume, G.music_volume);
        G.muted = !G.attract_sound;
        sound_mute(G.muted);
        sound_tune(TUNE_THEME);
    }
    if (G.no_rom) return;
    sound_update();
    if (G.selftest) {
        for (i = 0; i < G.selftest; i++) selftest_step(in);
        return;
    }
    {
        int hotkey = service_hotkey();
        if (G.service) { update_service(in, hotkey); return; }
        if (hotkey) { open_service(); return; }
    }
    step(in);
}

/* Not the original's: it has no coin slot. */
static void draw_coin_line(vp_dvg *d)
{
    char text[32];

    if (G.credits.free_play) snprintf(text, sizeof text, "FREE PLAY");
    else snprintf(text, sizeof text, "CREDITS %d", G.credits.credits);
    plain(d, text, 192.0f, 228.0f, 7.0f, VP_TEXT_CENTER, 0, 255, 0);
    if ((G.frames / 30) & 1)
        plain(d, vp_credits_enough(&G.credits, 2) ? "PRESS 1 OR 2 PLAYER START"
                 : vp_credits_enough(&G.credits, 1) ? "PRESS START" : "INSERT COIN",
              192.0f, 212.0f, 9.0f, VP_TEXT_CENTER, 255, 255, 255);
}

/* The title. The original's (versionscreen, rexfb) is a picture breathing
 * over a video feedback effect; this one is made for the beam: the stars
 * streaming past, and the lettering as solid shapes swinging in space. */
static void draw_title(vp_dvg *d)
{
    float fly = (float)G.frames * 1.5f;

    view_begin(d, 0, 0, fly);
    draw_stars(fly, 6.0f);
    view_begin(d, 0, 0, 0);
    view_level(G.bright[BRIGHT_TITLE]);
    logo_title(G.frames);
    plain(d, "DEVELOPED BY LLAMASOFT", 192.0f, 170.0f, 6.0f, VP_TEXT_CENTER, 60, 100, 255);
    plain(d, "COPYRIGHT 1981, 1994 ATARI CORP.", 192.0f, 181.0f, 6.0f, VP_TEXT_CENTER, 60, 100, 255);
}

/* showscores. The original's is a picture for a heading over a plain table;
 * this one goes with the title: the stars streaming past, the heading as
 * solid letters swinging in space, and the table coloured from the best
 * down, each row swaying a little after the one above. */
static void draw_scores(vp_dvg *d)
{
    /* From yellow at the top through orange, red and violet to blue. */
    static const unsigned char rank_rgb[9][3] = {
        {255, 255, 0}, {255, 190, 0}, {255, 120, 0}, {255, 60, 30}, {255, 40, 120},
        {220, 40, 220}, {140, 70, 255}, {70, 110, 255}, {0, 170, 255}
    };
    float fly = (float)G.frames * 1.5f;
    char text[48];
    int i;

    view_begin(d, 0, 0, fly);
    draw_stars(fly, 6.0f);
    view_begin(d, 0, 0, 0);
    view_level(G.bright[BRIGHT_TITLE]);
    logo_topguns(G.frames);
    for (i = 0; i < G.table_count && i < 9; i++) {
        const unsigned char *c = rank_rgb[i];
        float sway = 5.0f * sinf((float)G.frames * 0.035f - (float)i * 0.45f);
        float size = i == 0 ? 11.0f : 8.5f;
        float y = (i == 0 ? 74.0f : 78.0f) + 14.0f * (float)i;
        int   glow = i == 0 ? 2 : 1;

        if (i == G.rank && ((G.frames / 15) & 1)) continue;
        while (glow--) {                /* the best one twice over, as the title's near faces */
            snprintf(text, sizeof text, "%d", i + 1);
            plain(d, text, 100.0f + sway, y, size, VP_TEXT_RIGHT, c[0], c[1], c[2]);
            plain(d, G.table[i].name, 116.0f + sway, y, size, VP_TEXT_LEFT, c[0], c[1], c[2]);
            snprintf(text, sizeof text, "%d", G.table[i].score);
            plain(d, text, 310.0f + sway, y, size, VP_TEXT_RIGHT, c[0], c[1], c[2]);
        }
    }
}

/* yakscreen, "tempest dudes": who made it. The original's page has a
 * picture coming up out of the depth behind the names; this one goes with
 * the title, and adds who made this version. */
static void draw_credits(vp_dvg *d)
{
    static const char *const dudes[8][2] = {   /* testpage */
        {"ORIGINAL GAME DESIGNED BY", "DAVE THEURER"},
        {"JAGUAR VERSION BY", "YAK"},
        {"BITMAP ARTWORK BY", "JOBY"},
        {"TUNES BY", "IMAGITEC DESIGN"},
        {"GAME TESTING BY", "JOE, ANDREW, HANS AND GOKU"},
        {"VOICES BY", "TED AND CARRIE TAHQUECHI"},
        {"PRODUCED BY", "JOHN SKRUCH"},
        {"VECTORPIE PORT BY", "ENDPRODUKT"}
    };
    float fly = (float)G.frames * 1.5f;
    int i;

    view_begin(d, 0, 0, fly);
    draw_stars(fly, 6.0f);
    view_begin(d, 0, 0, 0);
    view_level(G.bright[BRIGHT_TITLE]);
    logo_dudes(G.frames);
    for (i = 0; i < 8; i++) {
        float y = 72.0f + 16.5f * (float)i;
        plain(d, dudes[i][0], 192.0f, y, 4.5f, VP_TEXT_CENTER, 60, 100, 255);
        plain(d, dudes[i][1], 192.0f, y + 6.5f, 6.5f, VP_TEXT_CENTER, 255, 255, 255);
    }
}

/* dobeastly: "congratulations!" and what the original says to who beat the
 * game (victpage), or beat it in Beastly Mode (victpage2). Its "press option
 * on the level select screen" is this cabinet's way in. */
static void draw_victory(vp_dvg *d)
{
    static const char *const won[] = {
        "YOU BEAT THE GAME!", "",
        "NOW TRY TO BEAT TEMPEST -", "THE BEASTLY MODE!", "",
        "HOLD SUPERZAPPER WHILE YOU TAKE", "THE START LEVEL TO BEGIN THIS", "HARDER VERSION OF THE GAME.", "",
        "ALL POINTS SCORED IN", "BEASTLY MODE ARE DOUBLED!", "",
        "NOW GO AND HAVE A NICE CUP OF TEA", "TO CALM DOWN!", NULL
    };
    static const char *const won_beastly[] = {
        "TOTALLY AWESOME!", "",
        "YOU BEAT THE GAME IN BEASTLY MODE!", "",
        "YOU HAD BETTER HAVE ABOUT", "SIX CUPS OF TEA AFTER THIS!", "",
        "WITH REFLEXES LIKE THAT, YOU SHOULD", "JOIN THE AIR FORCE AND", "BECOME A FIGHTER PILOT?", "",
        "OUTSTANDING TEMPEST, DUDE!", NULL
    };
    const char *const *page = G.beastly ? won_beastly : won;
    float fly = (float)G.frames * 1.5f, y = 46.0f;
    int i;

    view_begin(d, 0, 0, fly);
    draw_stars(fly, 6.0f);
    view_begin(d, 0, 0, 0);
    plain(d, "CONGRATULATIONS!", 192.0f, 18.0f, 11.0f, VP_TEXT_CENTER, 255, 255, 0);
    for (i = 0; page[i]; i++) {
        if (!page[i][0]) { y += 5.0f; continue; }
        plain(d, page[i], 192.0f, y, 6.0f, VP_TEXT_CENTER, i == 0 ? 255 : 0, 255, 255);
        y += 11.0f;
    }
    if (G.state_time >= VICTORY_HOLD && ((G.frames / 30) & 1))
        plain(d, "PRESS START", 192.0f, 222.0f, 8.0f, VP_TEXT_CENTER, 255, 255, 255);
}

/* gameover / gofeed. The viewer has fallen away from the web (snatch_it_away)
 * and the level is gone: the original swirls what was on the screen into a
 * video feedback and stretches a 'Game Over' picture over it, its width by
 * one sine (15 a frame) and its height by another (17 a frame), between
 * half and three quarters size. A beam has no feedback: the stars stream
 * past instead, and two words do what the picture does. */
static void draw_gameover(vp_dvg *d)
{
    float fly = (float)G.frames * 1.5f;
    float wide = 0.625f + 0.125f * sinf((float)G.state_time * 15.0f * (6.28318530718f / 256.0f));
    float tall = 0.625f + 0.125f * sinf((float)G.state_time * 17.0f * (6.28318530718f / 256.0f));

    view_begin(d, 0, 0, fly);
    draw_stars(fly, 6.0f);
    view_begin(d, 0, 0, 0);
    world_text(d, "GAME", 0.0f, -14.0f * tall * 1.6f, 96.0f, 34.0f * wide, view_pulse(G.state_time * 3));
    world_text(d, "OVER", 0.0f, 14.0f * tall * 1.6f, 96.0f, 34.0f * wide, view_pulse(G.state_time * 3 + 40));
    draw_hud(d);
}

/* The web turning in the stars behind the choices. */
static void draw_backdrop(vp_dvg *d, float distance)
{
    view_begin(d, 0, 0, -distance);
    view_set_roll((float)G.frames * 0.4f);
    draw_web(G.web, -1);
    view_set_roll(0);
    draw_stars(-distance, 0.0f);
}

static void draw_menus(vp_dvg *d)
{
    char text[32];
    int i;

    switch (G.state) {
    case ATTRACT:
        if (G.attract_page == 0) draw_title(d);
        else if (G.attract_page == 1) draw_scores(d);
        else draw_credits(d);
        draw_coin_line(d);
        break;
    case MODE_SELECT:
        draw_backdrop(d, 220.0f);
        plain(d, "SELECT GAME", 192.0f, 24.0f, 14.0f, VP_TEXT_CENTER, 255, 255, 0);
        for (i = 0; i < MODE_COUNT; i++) {
            float y = 34.0f * (float)(MODE_COUNT - 1 - i);
            if (i == G.mode) plain(d, mode_names[i], 192.0f, 84.0f + y, 18.0f, VP_TEXT_CENTER, 255, 255, 255);
            else plain(d, mode_names[i], 192.0f, 88.0f + y, 11.0f, VP_TEXT_CENTER, 60, 100, 255);
        }
        plain(d, "SPIN TO CHOOSE - START TO PLAY", 192.0f, 222.0f, 7.0f, VP_TEXT_CENTER, 0, 255, 0);
        break;
    case PLUS_SELECT:
        draw_backdrop(d, 220.0f);
        plain(d, "SELECT OPTIONS", 192.0f, 24.0f, 14.0f, VP_TEXT_CENTER, 255, 255, 0);
        for (i = 0; i < 2; i++) {
            const char *name = i ? "A.I. DROID" : "1 PLAYER";
            if (i == G.droid_game) plain(d, name, 192.0f, 92.0f + 40.0f * (float)i, 18.0f, VP_TEXT_CENTER, 255, 255, 255);
            else plain(d, name, 192.0f, 96.0f + 40.0f * (float)i, 11.0f, VP_TEXT_CENTER, 60, 100, 255);
        }
        plain(d, "SPIN TO CHOOSE - START TO PLAY", 192.0f, 222.0f, 7.0f, VP_TEXT_CENTER, 0, 255, 0);
        break;
    case LEVEL_SELECT:
        draw_backdrop(d, 20.0f);
        plain(d, "START LEVEL", 192.0f, 24.0f, 14.0f, VP_TEXT_CENTER, 255, 255, 0);
        snprintf(text, sizeof text, "%d", G.level + 1);
        plain(d, text, 192.0f, 52.0f, 26.0f, VP_TEXT_CENTER, 255, 255, 255);
        snprintf(text, sizeof text, "%d BONUS", level_bonus(G.level));     /* setcsmsg */
        plain(d, text, 192.0f, 190.0f, 10.0f, VP_TEXT_CENTER, 255, 255, 0);
        if (G.beastly_ini && ((G.frames / 30) & 1))                         /* bstymsg */
            plain(d, "HOLD SUPERZAPPER FOR BEASTLY MODE!", 192.0f, 206.0f, 7.0f, VP_TEXT_CENTER, 255, 60, 60);
        plain(d, "SPIN TO CHOOSE - START TO PLAY", 192.0f, 222.0f, 7.0f, VP_TEXT_CENTER, 0, 255, 0);
        break;
    case VICTORY:
        draw_victory(d);
        break;
    case GAME_OVER:
        draw_gameover(d);
        break;
    case INITIALS: {
        /* As the title and the scores: the stars streaming past, and the
         * heading as solid letters swinging in space. */
        float fly = (float)G.frames * 1.5f;
        view_begin(d, 0, 0, fly);
        draw_stars(fly, 6.0f);
        view_begin(d, 0, 0, 0);
        view_level(G.bright[BRIGHT_TITLE]);
        logo_greatscore(G.frames);
        if (G.players == 2) {
            snprintf(text, sizeof text, "PLAYER %d", G.over_player + 1);
            plain(d, text, 192.0f, 62.0f, 7.0f, VP_TEXT_CENTER, 255, 255, 255);
        }
        snprintf(text, sizeof text, "%d", G.score);
        plain(d, text, 192.0f, 76.0f, 12.0f, VP_TEXT_CENTER, 255, 255, 0);
        for (i = 0; i < 3; i++) {
            char one[2] = {G.initials[i] ? G.initials[i] : ' ', 0};
            if (i == G.initial_pos && ((G.frames / 10) & 1)) continue;
            plain(d, one, 192.0f + 40.0f * (float)(i - 1), 116.0f, 30.0f, VP_TEXT_CENTER,
                  255, 255, i == G.initial_pos ? 255 : 0);
        }
        plain(d, "SPIN FOR LETTER - START TO ENTER", 192.0f, 222.0f, 7.0f, VP_TEXT_CENTER, 0, 255, 0);
        break;
    }
    default: break;
    }
}

/* Behind the spinner's calibration page: the first level of Tempest 2000,
 * nobody on it but the claw, `lanes` lanes on from where it starts. The game
 * stands still under the menu, so what is borrowed is put back. */
static void draw_calibration(vp_dvg *d, float lanes)
{
    const t2k_web *keep_web = G.web, *w = &t2k_webs[t2k_level_web[0]];
    float keep_pos = G.claw_pos, keep_x = G.claw_x, keep_y = G.claw_y;
    int keep_level = G.level, keep_t2k = G.t2k, keep_solids = G.solids;
    float pos = (float)w->start + lanes;

    if (w->closed) {
        pos = fmodf(pos, (float)w->lanes);
        if (pos < 0) pos += (float)w->lanes;
    } else {
        if (pos < 0) pos = 0;
        if (pos > (float)w->lanes - 0.01f) pos = (float)w->lanes - 0.01f;
    }
    G.web = w; G.level = 0; G.t2k = 1; G.solids = 1;
    G.claw_pos = pos;
    lane_centre(w, claw_lane(), &G.claw_x, &G.claw_y);
    view_begin(d, G.claw_x * 0.5f, G.claw_y * 0.5f + 6.0f, 0);    /* a little higher: room below for the words */
    draw_web(w, claw_lane());
    draw_claw(Z_TOP << 16);
    G.web = keep_web; G.level = keep_level; G.t2k = keep_t2k; G.solids = keep_solids;
    G.claw_pos = keep_pos; G.claw_x = keep_x; G.claw_y = keep_y;
}

/* What became of the service menu's settings, for a while after it shut. */
static void draw_notice(vp_dvg *d)
{
    if (G.notice_time > 0) plain(d, G.notice, 192.0f, 150.0f, 9.0f, VP_TEXT_CENTER, 255, 255, 0);
}

void game_draw(vp_game *g, vp_dvg *d)
{
    float eye_z;
    int i;
    (void)g;

    vp_text_font(VP_FONT_ASTEROIDS);

#ifdef T2K_ROM_DATA
    if (G.no_rom) {
        char folder[80];
        size_t k;
        snprintf(folder, sizeof folder, "%s", rom_folder());
        for (k = 0; folder[k]; k++) folder[k] = (char)toupper((unsigned char)folder[k]);
        view_begin(d, 0, 0, 0);
        plain(d, "TEMPEST 2000", 192.0f, 30.0f, 16.0f, VP_TEXT_CENTER, 255, 255, 0);
        plain(d, "NEEDS THE JAGUAR ROM", 192.0f, 56.0f, 9.0f, VP_TEXT_CENTER, 255, 255, 0);
        plain(d, rom_problem(), 192.0f, 92.0f, 6.0f, VP_TEXT_CENTER, 255, 80, 80);
        plain(d, "PUT YOUR TEMPEST 2000 ROM", 192.0f, 124.0f, 7.0f, VP_TEXT_CENTER, 255, 255, 255);
        plain(d, "(.J64 .JAG .ROM .BIN) INTO", 192.0f, 138.0f, 7.0f, VP_TEXT_CENTER, 255, 255, 255);
        plain(d, folder, 192.0f, 160.0f, 5.5f, VP_TEXT_CENTER, 0, 255, 255);
        plain(d, "AND START THE GAME AGAIN", 192.0f, 184.0f, 7.0f, VP_TEXT_CENTER, 255, 255, 255);
        return;
    }
#endif

    if (G.service) {
        float lanes;
        if (service_calibrating(&lanes)) draw_calibration(d, lanes);
        view_begin(d, 0, 0, 0);
        service_draw(d);
        return;
    }

    if (G.state == ATTRACT || G.state == MODE_SELECT || G.state == PLUS_SELECT
        || G.state == LEVEL_SELECT || G.state == INITIALS || G.state == VICTORY
        || G.state == GAME_OVER) {
        draw_menus(d);
        draw_notice(d);
        return;
    }

    /* A jumping or departing claw takes the viewer with it (claw_con1: vp_z). */
    eye_z = FIX(G.claw_z) - (float)Z_TOP;
    if (G.state == ZOOM || G.state == WARPIN) eye_z = FIX(G.vp_z);
    if (G.state == INTRO) {
        /* rotate_web: the web starts 256 further off and comes 1 nearer a
         * frame, turning once about its upright axis and half as fast in
         * the plane of the screen, from upside down. */
        float turn = (float)(G.state_time * INTRO_SPEED + 1);
        if (turn > (float)INTRO_STEPS) turn = (float)INTRO_STEPS;
        eye_z -= (float)INTRO_STEPS - turn;
        view_set_spin(turn);
        view_set_roll(turn / 2.0f + 128.0f);
    }
    if (G.state == ZAPPED) {            /* zap_player, then snatch_it_away: 'vp_z' runs back */
        int t = G.state_time < ZAP_FRAMES ? G.state_time : ZAP_FRAMES;
        eye_z -= 1.0625f * (float)t + 8.0f * (float)(G.state_time - t);
    }
    view_begin(d, G.view_x, G.view_y, eye_z);
    if (!(G.state == ZOOM && G.zoom_web_off))      /* zoom2: web off for hyperspace */
        draw_web(G.web, G.state == PLAYING || G.state == ZOOM ? claw_lane() : -1);
    view_set_roll(0);
    view_set_spin(0);
    /* zoom1: three more pixels to a star, 'warp_add' apart, and that grows. */
    draw_stars(eye_z, G.state == ZOOM || G.state == WARPIN ? 3.0f * fabsf(G.warp_add) : 0.0f);
    for (i = 0; i < OBJECT_SLOTS; i++)
        if (G.objs[i].kind) draw_obj(&G.objs[i]);
    draw_shots();
    if (G.bolt_time > 0) draw_bolt();
    if (G.state == ZAPPED) {
        if (G.state_time < ZAP_FRAMES) draw_claw_zapped();
    } else if (G.state != INTRO && G.state != WARPIN) {
        /* srezclaw + dsclaw2: the claw arrives once the web is there, as
         * three more of itself closing in on it from the viewer's side;
         * draw_z: it leaves with three more spreading out behind it. */
        fix apart = G.state == PLAYING && G.rez > 0 ? -(G.rez << 14)
                  : G.state == ZOOM ? G.claw_images * 256 : 0;
        int k;
        for (k = 3; apart && k > 0; k--) draw_claw(G.claw_z + k * apart);
        draw_claw(G.claw_z);
    }
    draw_hud(d);
    draw_message();
    if (G.demo) {                       /* the original: "press FIRE to play" under it */
        plain(d, "DEMO", 192.0f, 44.0f, 22.0f, VP_TEXT_CENTER, 255, 255, 255);
        draw_coin_line(d);
    }
    draw_notice(d);
}
