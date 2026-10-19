/* words.c - big words in the space, written in strokes. */
#include "words.h"
#include "view.h"

#include <string.h>

/* A letter is strokes on a grid 4 wide and 6 high, y down: pairs of digits
 * (x, y) from point to point, a space where the pen lifts. */
static const char *glyph(char c)
{
    switch (c) {
    case '0': return "0040460600";
    case '1': return "112026";
    case '2': return "004043030646";
    case '5': return "400003434606";
    case '7': return "004016";
    case 'C': return "40000646";
    case 'E': return "40000646 0333";
    case 'L': return "000646";
    case 'N': return "06004640";
    case 'P': return "0600404303";
    case 'T': return "0040 2026";
    case 'U': return "00064640";
    case 'X': return "0046 4006";
    default:  return "";
    }
}

#define GLYPH_HEIGHT 6.0f
#define ADVANCE      6.0f       /* from one letter to the next: 4 wide, 2 between */

float words_height(const char *text, int w, int h, float spacing)
{
    float fit = (float)w / (float)strlen(text);     /* a letter and its gap are as wide as it is high */
    return (fit < (float)h ? fit : (float)h) * spacing;
}

void words_draw(const char *text, float x, float y, float z, float height, int phase)
{
    float unit = height / GLYPH_HEIGHT;
    float left = x - ((float)strlen(text) * ADVANCE - 2.0f) * unit / 2.0f;
    float top = y - height / 2.0f;
    int   stroke = 0;

    if (height <= 0) return;
    for (; *text; text++, left += ADVANCE * unit) {
        const char *g = glyph(*text);
        int have = 0;
        float px = 0, py = 0;

        for (; g[0] && g[1]; g += 2) {
            float qx, qy;
            if (g[0] == ' ') { have = 0; g--; continue; }
            qx = left + (float)(g[0] - '0') * unit;
            qy = top + (float)(g[1] - '0') * unit;
            if (have) {
                view_cry(view_pulse(phase + stroke++ * 23));
                view_line(px, py, z, qx, qy, z);
            }
            px = qx; py = qy; have = 1;
        }
    }
}
