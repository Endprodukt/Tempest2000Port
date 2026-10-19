/* service.c - the service menu: game.ini's settings, changed on the machine.
 *
 * Worked with what a cabinet has. The spinner (or up / down) goes through
 * the rows; fire (or START 1) takes a row, the spinner (or left / right)
 * then sets it, fire lets it go again. Left / right set a row without taking
 * it first. Nothing is taken over until SAVE. What has to do with the
 * controls is on a page of its own, and so is what has to do with the picture.
 */
#include "service.h"
#include "sound.h"
#include "view.h"
#include "vpbind.h"

#include <SDL.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

enum {
    R_ADJUST, R_SENS, R_DIRECTION, R_VOLUME, R_MUSIC, R_EFFECTS, R_VOICE,
    R_ATTRACT, R_FREE, R_CHECK, R_BEASTLY, R_UNLOCK,
    R_FIRE, R_ZAP, R_JUMP, R_LEFT, R_RIGHT, R_SAVE, R_CANCEL,
    R_CONTROLS, R_DIGITAL, R_BACK,
    R_VIDEO, R_STARS, R_BRIGHT, R_BRIGHT_LAST = R_BRIGHT + SERVICE_BRIGHTS - 1, ROWS
};

/* The pages, each the rows it has from the top down. */
enum { PAGE_MAIN, PAGE_CONTROLS, PAGE_VIDEO };
static const int main_rows[] = {
    R_CONTROLS, R_VIDEO, R_VOLUME, R_MUSIC, R_EFFECTS, R_VOICE,
    R_ATTRACT, R_FREE, R_CHECK, R_BEASTLY, R_UNLOCK, R_CANCEL, R_SAVE   /* turned too far ends on SAVE */
};
static const int control_rows[] = {
    R_ADJUST, R_SENS, R_DIRECTION, R_DIGITAL, R_FIRE, R_ZAP, R_JUMP, R_LEFT, R_RIGHT, R_BACK
};
static const int video_rows[] = {
    R_STARS, R_BRIGHT, R_BRIGHT + 1, R_BRIGHT + 2, R_BRIGHT + 3, R_BRIGHT + 4, R_BRIGHT + 5,
    R_BRIGHT + 6, R_BRIGHT + 7, R_BACK
};
#define COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))

#define SENS_MIN       5                /* hundredths */
#define SENS_MAX       400
#define DIGITAL_MIN    25
#define ROW_COUNTS     0.0125f          /* rows a spinner count moves the cursor, at sensitivity 1 */
#define VALUE_COUNTS   0.05f            /* steps a count moves a value */
#define REPEAT_DELAY   24               /* a held button: frames before it repeats, and between */
#define REPEAT_EVERY   3
#define TURN_REST      90               /* spinner adjust: frames of rest that end a turn */

static const char *const row_names[ROWS] = {
    "SPINNER ADJUST", "SPINNER SENSITIVITY", "SPINNER DIRECTION",
    "MASTER VOLUME", "MUSIC VOLUME", "EFFECTS VOLUME", "VOICE VOLUME",
    "ATTRACT SOUND", "FREE PLAY", "CHECKPOINTS", "SHOW BEASTLY HINT", "FIRST DAY LEVELS",
    "FIRE BUTTON", "ZAP BUTTON", "JUMP BUTTON", "LEFT BUTTON", "RIGHT BUTTON",
    "SAVE", "CANCEL",
    "CONTROLS", "DIGITAL SPEED", "BACK",
    "VIDEO", "STARS",
    "WEB BRIGHTNESS", "STARS BRIGHTNESS", "ENEMY BRIGHTNESS", "EFFECT BRIGHTNESS",
    "CLAW BRIGHTNESS", "SHOT BRIGHTNESS", "TEXT BRIGHTNESS", "TITLE BRIGHTNESS"
};

static struct {
    service_settings now;
    int   sens;                         /* the sensitivity's size, in hundredths */
    int   digital;                      /* the digital one, in hundredths */
    int   reversed;
    int   page, cursor;                 /* which page, and which of its rows */
    int   row;                          /* that row */
    int   editing, adjusting, adjust_armed;
    vpb_layout lay;                     /* laying out a function (vpbind.h) */
    int   music, turn;                  /* the tune is playing; which sound the master row made last */
    float spin;                         /* spinner counts not yet used up */
    int   held;                         /* frames a value button has been down */
    float lanes_per_count;
    float claw;                         /* spinner adjust: lanes the claw is on from its start */
    float spun;                         /* and how far the turn going on, or the last one, took it */
    int   turn_rest;
    int   preview;                      /* frames until a volume's row sounds again */
    int   key;                          /* the hotkey's state */
    unsigned frames;
} S;

static const int *page_rows(int *count)
{
    if (S.page == PAGE_CONTROLS) { *count = COUNT(control_rows); return control_rows; }
    if (S.page == PAGE_VIDEO) { *count = COUNT(video_rows); return video_rows; }
    *count = COUNT(main_rows);
    return main_rows;
}

int service_hotkey(void)
{
    int key, hit;

    vpb_update();
    key = vpb_key_held(SDL_SCANCODE_F2);
    hit = key && !S.key;
    S.key = key;
    return hit;
}

const char *const service_default_button[SERVICE_BUTTONS] = {
    "key_Left_Ctrl", "key_Left_Alt", "key_Space", "key_Left", "key_Right"
};

int menu_confirm(vp_input *in)                              { return vpb_menu_confirm(in); }
int control_held(vp_input *in, const char *controls)        { (void)in; return vpb_held(controls); }
int control_pressed(vp_input *in, const char *controls)     { (void)in; return vpb_pressed(controls); }

void service_open(const service_settings *now, float lanes_per_count)
{
    int key = S.key, i;

    memset(&S, 0, sizeof S);
    S.key = key;
    S.now = *now;
    for (i = 0; i < SERVICE_BUTTONS; i++) vpb_normalize(S.now.button[i], sizeof S.now.button[i]);
    S.reversed = now->sensitivity < 0;
    S.sens = (int)(fabsf(now->sensitivity) * 100.0f + 0.5f);
    if (S.sens < SENS_MIN) S.sens = SENS_MIN;
    if (S.sens > SENS_MAX) S.sens = SENS_MAX;
    S.digital = (int)(now->digital_sensitivity * 100.0f + 0.5f);
    if (S.digital < DIGITAL_MIN) S.digital = DIGITAL_MIN;
    if (S.digital > SENS_MAX) S.digital = SENS_MAX;
    S.row = main_rows[0];
    S.lanes_per_count = lanes_per_count;
}

const service_settings *service_get(void)
{
    S.now.sensitivity = (float)S.sens / 100.0f * (S.reversed ? -1.0f : 1.0f);
    S.now.digital_sensitivity = (float)S.digital / 100.0f;
    return &S.now;
}

static void hear(void)
{
    sound_volumes((float)S.now.volume / 100.0f, (float)S.now.effects_volume / 100.0f,
                  (float)S.now.voice_volume / 100.0f, (float)S.now.music_volume / 100.0f);
}

static int *row_number(int row, int *lo, int *hi)
{
    *lo = 0; *hi = 100;
    switch (row) {
    case R_SENS:    *lo = SENS_MIN; *hi = SENS_MAX; return &S.sens;
    case R_DIGITAL: *lo = DIGITAL_MIN; *hi = SENS_MAX; return &S.digital;
    case R_VOLUME:  return &S.now.volume;
    case R_MUSIC:   return &S.now.music_volume;
    case R_EFFECTS: return &S.now.effects_volume;
    case R_VOICE:   return &S.now.voice_volume;
    case R_UNLOCK:  *lo = 1; *hi = 99; return &S.now.unlocked_level;
    case R_STARS:   *hi = 256; return &S.now.stars;
    }
    if (row >= R_BRIGHT && row <= R_BRIGHT_LAST) return &S.now.brightness[row - R_BRIGHT];
    return NULL;
}

static int *row_switch(int row)
{
    switch (row) {
    case R_DIRECTION: return &S.reversed;
    case R_ATTRACT:   return &S.now.attract_sound;
    case R_FREE:      return &S.now.free_play;
    case R_CHECK:     return &S.now.checkpoints;
    case R_BEASTLY:   return &S.now.beastly_mode;
    }
    return NULL;
}

static int is_button(int row) { return row >= R_FIRE && row <= R_RIGHT; }

/* Change the row's value by `steps`. */
/* Left or right on a row. A function's row: none, or its standard control. */
static void change(int row, int steps)
{
    int lo, hi, *number = row_number(row, &lo, &hi), *onoff = row_switch(row);

    if (!steps) return;
    if (is_button(row)) {
        vpb_layout_toggle(&S.lay, S.now.button[row - R_FIRE], SERVICE_CONTROLS_LENGTH,
                          service_default_button[row - R_FIRE]);
        sound_fx(FX_TINK, 0);
        return;
    }
    if (onoff) { *onoff = !*onoff; sound_fx(FX_TINK, 0); return; }
    if (!number) return;
    if (row == R_UNLOCK) steps *= 2;    /* every second level can be started on */
    *number += steps;
    if (*number < lo) *number = lo;
    if (*number > hi) *number = hi;
    if (row >= R_VOLUME && row <= R_VOICE) hear();
}

/* The menu is silent but for what a volume's row is the volume of: the
 * tune on the music's, an effect again and again on the effects', a voice
 * on the voices', and all three on the master's. */
static void sound_for_row(void)
{
    int music = S.row == R_VOLUME || S.row == R_MUSIC;

    if (music != S.music) {
        S.music = music;
        sound_tune(music ? TUNE_THEME : 0);
    }
    if (S.preview > 0) { S.preview--; return; }
    if (S.row == R_EFFECTS) {
        sound_fx(FX_EXPLOSION, 0);
        S.preview = 45;
    } else if (S.row == R_VOICE) {
        sound_fx(FX_YES, 0);
        S.preview = 100;
    } else if (S.row == R_VOLUME) {
        S.turn = !S.turn;
        sound_fx(S.turn ? FX_EXCELLENT : FX_EXPLOSION, 0);
        S.preview = 90;
    }
}

/* Go to a page, at its top. */
static void turn_to(int page)
{
    int count;

    vpb_layout_moved(&S.lay);
    S.page = page;
    S.cursor = 0;
    S.row = page_rows(&count)[0];
}

/* A value button held down: once, then again and again. */
static int repeated(int held)
{
    if (!held) return 0;
    if (S.held == 1) return 1;
    return S.held > REPEAT_DELAY && (S.held - REPEAT_DELAY) % REPEAT_EVERY == 0;
}

/* The spinner as whole steps, `per_count` of them to a count, the way the
 * claw goes (`sign` 1) or the way the game's menus go (-1: the turn that
 * takes the claw to higher lanes goes up a list). Reversed as it is set. */
static int spin_steps(vp_input *in, float per_count, float sign)
{
    int steps;

    S.spin += (float)vp_input_delta(in, "p1_dialx") * per_count * sign * (S.reversed ? -1.0f : 1.0f);
    steps = (int)S.spin;
    S.spin -= (float)steps;
    return steps;
}

/* Laying out a function: vpbind.h. */
static void capture(vp_input *in)
{
    int got = vpb_layout_step(&S.lay, in, S.now.button[S.row - R_FIRE], SERVICE_CONTROLS_LENGTH,
                              service_default_button[S.row - R_FIRE]);
    if (got == VPB_CHANGED) sound_fx(FX_SEXY_YES, 0);
    else if (got == VPB_LEFT) sound_fx(FX_TINK, 0);
}

int service_calibrating(float *lanes)
{
    *lanes = S.claw;
    return S.adjusting;
}

/* Spinner adjust: the claw on the first level's web, moved by the spinner
 * as in the game. Right when one full turn takes it five lanes on: what a
 * Tempest spinner does. A turn ends when the spinner has rested a while;
 * how far it took the claw stands on the screen. */
static void adjust(vp_input *in)
{
    int spin = vp_input_delta(in, "p1_dialx");
    int more = vpb_menu_right();        /* the menu's own controls: laid out ones may be anything */
    int less = vpb_menu_left();

    if (menu_confirm(in)) { S.adjusting = 0; return; }
    if (vpb_menu_up() || vpb_menu_down()) S.reversed = !S.reversed;
    if (!S.adjust_armed) {              /* the press that opened this must end first */
        S.adjust_armed = !more && !less;
        more = less = 0;
    }
    S.held = more != less ? S.held + 1 : 0;
    if (repeated(more != less)) change(R_SENS, more ? 1 : -1);
    if (spin) {
        float lanes = (float)spin * S.lanes_per_count * (float)S.sens / 100.0f * (S.reversed ? -1.0f : 1.0f);
        if (S.turn_rest >= TURN_REST) S.spun = 0;           /* a new turn */
        S.claw += lanes;
        S.spun += lanes;
        S.turn_rest = 0;
    } else if (S.turn_rest < TURN_REST) {
        S.turn_rest++;
    }
}

int service_update(vp_input *in, int hotkey)
{
    int accept, left, right, steps;

    int count;
    const int *rows = page_rows(&count);

    S.frames++;
    sound_for_row();
    if (hotkey) return SERVICE_CANCELLED;
    if (S.lay.capturing) { capture(in); return SERVICE_OPEN; }
    if (S.adjusting) { adjust(in); return SERVICE_OPEN; }

    /* The menu's own controls only, whatever the game has laid out. */
    accept = menu_confirm(in);          /* START 1 and Enter only: a function's control may be anything */
    left = vpb_menu_left();             /* the menu's own: what the game has laid out never counts here */
    right = vpb_menu_right();
    if (is_button(S.row) && !S.editing
        && (vpb_key_pressed(SDL_SCANCODE_DELETE) || vpb_key_pressed(SDL_SCANCODE_KP_PERIOD))) {
        vpb_layout_standard(&S.lay, S.now.button[S.row - R_FIRE], SERVICE_CONTROLS_LENGTH,
                            service_default_button[S.row - R_FIRE]);
        sound_fx(FX_SEXY_YES, 0);
        return SERVICE_OPEN;
    }
    vpb_layout_filter(&S.lay, &left, &right);
    S.held = left != right ? S.held + 1 : 0;
    if (repeated(left != right)) change(S.row, right ? 1 : -1);

    if (S.editing) {
        change(S.row, spin_steps(in, VALUE_COUNTS * (float)S.sens / 100.0f, 1.0f));
        if (accept) { S.editing = 0; S.spin = 0; sound_fx(FX_TINK, 0); }
        return SERVICE_OPEN;
    }

    steps = spin_steps(in, ROW_COUNTS * (float)S.sens / 100.0f, -1.0f);
    if (vpb_menu_down()) steps++;
    if (vpb_menu_up()) steps--;
    if (steps) {
        S.cursor += steps;
        vpb_layout_moved(&S.lay);
        if (S.cursor < 0) S.cursor = 0;
        if (S.cursor >= count) S.cursor = count - 1;
        S.row = rows[S.cursor];
        S.preview = 0;                  /* a volume's row is heard at once */
    }
    if (!accept) return SERVICE_OPEN;
    S.spin = 0;
    if (S.row == R_SAVE) return SERVICE_SAVED;
    if (S.row == R_CANCEL) return SERVICE_CANCELLED;
    sound_fx(FX_TINK, 0);
    if (S.row == R_CONTROLS) turn_to(PAGE_CONTROLS);
    else if (S.row == R_VIDEO) turn_to(PAGE_VIDEO);
    else if (S.row == R_BACK) turn_to(PAGE_MAIN);
    else if (S.row == R_ADJUST) {
        S.adjusting = 1; S.adjust_armed = 0; S.held = 0;
        S.claw = S.spun = 0; S.turn_rest = TURN_REST;
    }
    else if (is_button(S.row)) vpb_layout_begin(&S.lay, S.now.button[S.row - R_FIRE]);
    else if (row_switch(S.row)) change(S.row, 1);
    else S.editing = 1;
    return SERVICE_OPEN;
}

/* --- drawing -------------------------------------------------------------------- */

static void text(vp_dvg *d, const char *s, float x, float y, float height, int align, int r, int g, int b)
{
    int bx, by;
    view_board(x, y + height, &bx, &by);
    vp_color(d, (uint8_t)r, (uint8_t)g, (uint8_t)b);
    vp_text(d, s, (float)bx, (float)by, height * view_pixel_height(), align);
}

static void line(vp_dvg *d, float x0, float y0, float x1, float y1)
{
    int ax, ay, bx, by;
    view_board(x0, y0, &ax, &ay);
    view_board(x1, y1, &bx, &by);
    vp_moveto(d, ax, ay);
    vp_lineto(d, bx, by);
}

static void diamond(vp_dvg *d, float x, float y, float r)
{
    line(d, x - r, y, x, y - r);
    line(d, x, y - r, x + r, y);
    line(d, x + r, y, x, y + r);
    line(d, x, y + r, x - r, y);
}

/* A slider: the part set bright, the rest dim, a knob where they meet. */
static void slider(vp_dvg *d, float x, float y, float width, float part, int lit, float level)
{
    float knob = x + width * part;
    uint8_t bright = (uint8_t)((lit ? 255.0f : 150.0f) * level);

    vp_color(d, 0, bright, bright);
    line(d, x, y - 2.5f, x, y + 2.5f);
    line(d, x, y, knob, y);
    vp_color(d, 40, 60, 110);
    line(d, knob, y, x + width, y);
    line(d, x + width, y - 2.5f, x + width, y + 2.5f);
    if (lit) vp_color(d, 255, 255, 0); else vp_color(d, 200, 200, 200);
    diamond(d, knob, y, 3.0f);
}


static void draw_adjust(vp_dvg *d)
{
    float turn = S.spun < 0 ? -S.spun : S.spun;
    char value[48];

    text(d, "SPINNER ADJUST", 192.0f, 4.0f, 10.0f, VP_TEXT_CENTER, 255, 255, 0);
    text(d, "ONE FULL TURN OF THE SPINNER", 192.0f, 20.0f, 6.0f, VP_TEXT_CENTER, 0, 200, 255);
    text(d, "SHOULD MOVE THE CLAW 5 LANES", 192.0f, 30.0f, 6.0f, VP_TEXT_CENTER, 0, 200, 255);
    snprintf(value, sizeof value, "THIS TURN: %d.%d LANES", (int)turn, (int)(turn * 10.0f) % 10);
    text(d, value, 192.0f, 184.0f, 7.0f, VP_TEXT_CENTER, 255, 255, 255);
    snprintf(value, sizeof value, "SENSITIVITY %d.%02d  %s", S.sens / 100, S.sens % 100, S.reversed ? "REVERSED" : "NORMAL");
    text(d, value, 192.0f, 196.0f, 5.0f, VP_TEXT_CENTER, 255, 255, 255);
    text(d, "LEFT - LESS    RIGHT - MORE", 192.0f, 206.0f, 4.5f, VP_TEXT_CENTER, 0, 255, 0);
    text(d, "WRONG WAY ROUND - UP OR DOWN TURNS IT", 192.0f, 215.0f, 4.5f, VP_TEXT_CENTER, 0, 255, 0);
    text(d, "START 1 - BACK", 192.0f, 226.0f, 5.5f, VP_TEXT_CENTER, 255, 255, 0);
}

void service_draw(vp_dvg *d)
{
    int count;
    const int *rows = page_rows(&count);
    char value[SERVICE_CONTROLS_LENGTH * 2];
    int i;

    if (S.adjusting) { draw_adjust(d); return; }
    text(d, S.page == PAGE_CONTROLS ? "CONTROLS" : S.page == PAGE_VIDEO ? "VIDEO" : "SERVICE MENU",
         192.0f, 4.0f, 10.0f, VP_TEXT_CENTER, 255, 255, 0);
    for (i = 0; i < count; i++) {
        int   row = rows[i];
        int   here = row == S.row, set = here && (S.editing || S.lay.capturing);
        int   lo, hi, *number = row_number(row, &lo, &hi), *onoff = row_switch(row);
        float y = 28.0f + 15.0f * (float)i;
        int   r = here ? 255 : 60, g = here ? 255 : 110, b = 255;

        if (row == R_SAVE || row == R_CANCEL) continue;         /* side by side, below */
        if (here) text(d, ">", 12.0f, y, 6.0f, VP_TEXT_RIGHT, 255, 255, 0);
        text(d, row_names[row], 16.0f, y, 6.0f, VP_TEXT_LEFT, r, g, b);
        if (set) { r = 255; g = 255; b = 0; }
        if (row == R_ADJUST || row == R_CONTROLS || row == R_VIDEO) {
            text(d, "OPEN", 214.0f, y, 6.0f, VP_TEXT_LEFT, r, g, b);
        } else if (row == R_SENS || row == R_DIGITAL) {
            snprintf(value, sizeof value, "%d.%02d", *number / 100, *number % 100);
            text(d, value, 214.0f, y, 6.0f, VP_TEXT_LEFT, r, g, b);
        } else if (row == R_DIRECTION) {
            text(d, S.reversed ? "REVERSED" : "NORMAL", 214.0f, y, 6.0f, VP_TEXT_LEFT, r, g, b);
        } else if (row == R_UNLOCK) {
            snprintf(value, sizeof value, "UP TO %d", *number);
            text(d, value, 214.0f, y, 6.0f, VP_TEXT_LEFT, r, g, b);
        } else if (number) {
            /* A brightness's slider is drawn as bright as it sets. */
            slider(d, 214.0f, y + 3.0f, 100.0f, (float)(*number - lo) / (float)(hi - lo), here,
                   row >= R_BRIGHT && row <= R_BRIGHT_LAST ? (float)(*number < 15 ? 15 : *number) / 100.0f : 1.0f);
            snprintf(value, sizeof value, "%d", *number);
            text(d, value, 356.0f, y, 6.0f, VP_TEXT_RIGHT, r, g, b);
        } else if (onoff) {
            text(d, *onoff ? "YES" : "NO", 214.0f, y, 6.0f, VP_TEXT_LEFT, r, g, b);
        } else if (S.lay.capturing && here && ((S.frames / 15) & 1)) {
            if ((S.frames / 15) & 1) text(d, "PRESS A BUTTON", 214.0f, y, 6.0f, VP_TEXT_LEFT, 255, 255, 0);
        } else if (is_button(row)) {
            /* All its controls; smaller when they would not fit. */
            float size = 6.0f, room = 384.0f - 214.0f - 6.0f;
            vpb_spelled(S.now.button[row - R_FIRE], value, sizeof value);
            if ((float)strlen(value) * size * 1.6f > room) size = room / ((float)strlen(value) * 1.6f);
            text(d, value, 214.0f, y + (6.0f - size), size, VP_TEXT_LEFT, r, g, b);
        }
    }
    if (S.page == PAGE_MAIN) {
        if (S.row == R_CANCEL) text(d, "CANCEL", 140.0f, 201.0f, 10.0f, VP_TEXT_CENTER, 255, 255, 255);
        else text(d, "CANCEL", 140.0f, 203.0f, 7.0f, VP_TEXT_CENTER, 200, 60, 60);
        if (S.row == R_SAVE) text(d, "SAVE", 244.0f, 201.0f, 10.0f, VP_TEXT_CENTER, 255, 255, 255);
        else text(d, "SAVE", 244.0f, 203.0f, 7.0f, VP_TEXT_CENTER, 0, 200, 0);
    }
    {
        const char *said = vpb_layout_said(&S.lay);
        if (said) text(d, said, 192.0f, 214.0f, 4.5f, VP_TEXT_CENTER, 255, 80, 80);
    }
    {
        const char *one, *two;
        if (S.lay.capturing) {
            vpb_layout_help(&S.lay, &one, &two);
        } else if (S.editing) {
            one = "SPINNER, LEFT, RIGHT - SET";
            two = "ENTER, START 1 - DONE";
        } else {
            one = "SPINNER, UP, DOWN - ROW   LEFT, RIGHT - SET";
            two = "ENTER, START 1 - TAKE";
            if (is_button(S.row)) vpb_layout_help(&S.lay, &one, &two);
        }
        text(d, one, 192.0f, 222.0f, 4.5f, VP_TEXT_CENTER, 0, 150, 0);
        text(d, two, 192.0f, 230.0f, 4.5f, VP_TEXT_CENTER, 0, 150, 0);
    }
}
