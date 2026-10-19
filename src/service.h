/* service.h - the service menu: game.ini's settings, changed on the machine. */
#ifndef T2K_SERVICE_H
#define T2K_SERVICE_H

#include <vectorpie/vp.h>

/* The controls a player can lay out, in game.ini's order. */
enum { SERVICE_FIRE, SERVICE_ZAP, SERVICE_JUMP, SERVICE_LEFT, SERVICE_RIGHT, SERVICE_BUTTONS };

/* What each function has when nothing says otherwise: the keys AdvanceMAME
 * gives the first player's buttons. */
extern const char *const service_default_button[SERVICE_BUTTONS];

/* How many kinds of thing have a brightness of their own (game.c's BRIGHT_ names). */
#define SERVICE_BRIGHTS 8

/* A function's controls, as game.ini has them: up to this many characters. */
#define SERVICE_CONTROLS_LENGTH 128    /* vpbind.h's VPB_LENGTH */

/* What game.ini holds. */
typedef struct service_settings {
    float sensitivity;                  /* negative: the spinner the other way round */
    float digital_sensitivity;          /* how fast keys or a stick move the claw; 1 = the original's */
    int   volume, music_volume, effects_volume, voice_volume;      /* 0..100 */
    int   attract_sound, free_play, checkpoints, beastly_mode;     /* yes or no */
    int   unlocked_level;
    int   stars;                        /* in the background, 0..256 */
    int   brightness[SERVICE_BRIGHTS];  /* 0..100 */
    char  button[SERVICE_BUTTONS][SERVICE_CONTROLS_LENGTH];   /* controls, '+' between them */
} service_settings;

enum { SERVICE_OPEN = 0, SERVICE_SAVED, SERVICE_CANCELLED };

/* Once every game step, menu open or not, after the SDK has read its input:
 * 1 when the menu's key, F2, has just been pressed. (It also reads every
 * control for the step: vpb_update.) */
int service_hotkey(void);

/* The menus' own confirm, there whatever is laid out: START 1, Enter and the
 * keypad's Enter. */
int menu_confirm(vp_input *in);

/* A function's controls (vpbind.h's text, '+' between them): any held, or
 * down since the last step. */
int control_held(vp_input *in, const char *control);
int control_pressed(vp_input *in, const char *control);

/* Open the menu on these settings. `lanes_per_count` is how far the claw
 * moves for one count of the spinner at sensitivity 1. */
void service_open(const service_settings *now, float lanes_per_count);

/* The spinner's calibration page is up: the game draws the first level's
 * web behind it, with the claw `lanes` lanes on from its start. */
int service_calibrating(float *lanes);

/* One step. `hotkey` is service_hotkey's answer (the key again cancels).
 * While it is open nothing sounds but what the row stood on is for: a
 * volume's row plays what that volume is the volume of. */
int service_update(vp_input *in, int hotkey);

/* The settings as they stand in the menu: the ones to take over once
 * service_update has answered SERVICE_SAVED. */
const service_settings *service_get(void);

/* The view must have begun. */
void service_draw(vp_dvg *d);

#endif /* T2K_SERVICE_H */
