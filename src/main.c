/* Tempest 2000 for VectorPie: the vector monitor port.
 *
 * Controls, by their advmame.rc names (the cabinet's Tempest bindings):
 *   p1_dialx              the spinner
 *   p1_left / p1_right    the same on buttons or keys
 *   p1_button1            fire
 *   p1_button2            Superzapper
 *   p1_button3            jump
 *   coin1 / coin2         a credit
 *   start1                start
 */
#include <vectorpie/vp.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game.h"

static void update(vp_game *g, float dt) { game_update(g, dt); }
static void draw(vp_game *g, vp_dvg *d)  { game_draw(g, d); }

int main(int argc, char **argv)
{
    vp_game_desc desc = {0};
    const char *env = getenv("VP_VERTICAL");
    int vertical = env && !strcmp(env, "1");
    int i;

    /* The VectorPie menu starts a game as <name>_dvg or <name>_sdl. */
    if (!getenv("VP_OUTPUT") && argc > 0) {
        size_t n = strlen(argv[0]);
        const char *output = n >= 4 && !strcmp(argv[0] + n - 4, "_sdl") ? "sdl"
                           : n >= 4 && !strcmp(argv[0] + n - 4, "_dvg") ? "dvg" : NULL;
#ifdef _WIN32
        if (output) _putenv_s("VP_OUTPUT", output);
#else
        if (output) setenv("VP_OUTPUT", output, 1);
#endif
    }
    for (i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--vertical")) vertical = 1;
    game_init(vertical);
    env = getenv("T2K_LEVEL");          /* a testing aid, see game_unlock */
    if (env) game_unlock(atoi(env));
    env = getenv("T2K_SELFTEST");       /* "speed[,mode[,beastly[,level]]]": see game_selftest */
    if (env) {
        int speed = 1, mode = 2, beastly = 0, level = 1;
        sscanf(env, "%d,%d,%d,%d", &speed, &mode, &beastly, &level);
        game_selftest(speed, mode, beastly, level);
    }

    desc.title          = "Tempest 2000";
    desc.update         = update;
    desc.draw           = draw;
    desc.update_hz      = 60;       /* the original's frame interrupt */
    desc.frame_hz       = 60;
    desc.frame_hz_board = 40;
    return vp_run(&desc, argc, argv);
}
