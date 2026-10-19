/* mod.c - a 4-channel ProTracker module ("M.K.") played into memory.
 *
 * Tempest 2000's tunes are plain ProTracker modules, which its sound driver
 * plays as an Amiga in Europe would: the driver's note table (initpit2 in
 * unused/syn6.o) gives the PAL Amiga's pitches, and the game calls it 50
 * times a second on either kind of Jaguar.
 */
#include "mod.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define CHANNELS    4
#define PAL_CLOCK   3546894.6           /* Amiga: sample rate = clock / period */
#define MAX_SECONDS 900                 /* a song that never comes round is cut here */
#define SEPARATION  0.3                 /* how much of a channel leaks to the other side */
#define GAIN        1.7

typedef struct sample {
    const signed char *data;
    int length, loop_start, loop_length;
    int finetune, volume;
} sample;

typedef struct channel {
    const sample *smp;
    int    volume;
    double period;              /* as played, before vibrato and arpeggio */
    double pos, step;
    int    playing;
    int    effect, param;
    double target;              /* 3xx */
    int    porta_speed;
    int    vib_pos, vib_speed, vib_depth;
    int    trem_pos, trem_speed, trem_depth;
    int    offset_memory;
    int    loop_row, loop_count;
    int    note_period;         /* this row's note, for EDx */
    const sample *note_smp;
    double out_period;
    int    out_volume;
} channel;

static const unsigned char sine[32] = {
    0, 24, 49, 74, 97, 120, 141, 161, 180, 197, 212, 224, 235, 244, 250, 253,
    255, 253, 250, 244, 235, 224, 212, 197, 180, 161, 141, 120, 97, 74, 49, 24
};

static int be16(const unsigned char *p) { return (p[0] << 8) | p[1]; }

static double tuned(int period, int finetune)
{
    return (double)period * pow(2.0, -(double)finetune / 96.0);
}

static void start_note(channel *c)
{
    if (c->note_smp) c->smp = c->note_smp;
    if (!c->smp || !c->note_period) return;
    c->period = tuned(c->note_period, c->smp->finetune);
    c->pos = 0;
    c->playing = 1;
    c->vib_pos = 0;
}

static void volume_slide(channel *c, int param)
{
    if (param >> 4) c->volume += param >> 4;
    else c->volume -= param & 15;
    if (c->volume > 64) c->volume = 64;
    if (c->volume < 0) c->volume = 0;
}

static void tone_porta(channel *c)
{
    if (!c->target || !c->period) return;
    if (c->period < c->target) {
        c->period += c->porta_speed;
        if (c->period > c->target) c->period = c->target;
    } else {
        c->period -= c->porta_speed;
        if (c->period < c->target) c->period = c->target;
    }
}

static double vibrato(channel *c)
{
    int v = sine[c->vib_pos & 31] * c->vib_depth / 128;
    return (c->vib_pos & 32) ? -v : v;
}

float *mod_render(const unsigned char *mod, long size, int rate, long *frames_out)
{
    sample smp[32];
    channel ch[CHANNELS];
    int song_length, patterns = 0, i, c;
    const unsigned char *orders, *pattern_data, *p;
    char visited[128];
    int order = 0, row = 0, speed = 6, bpm = 125, tick = 0, pattern_delay = 0;
    long frames = 0, max_frames, room;
    float *out;
    double tick_frames, tick_left = 0;
    int done = 0;

    *frames_out = 0;
    if (!mod || size < 1084 || memcmp(mod + 1080, "M.K.", 4)) return NULL;

    song_length = mod[950];
    orders = mod + 952;
    for (i = 0; i < 128; i++) if (orders[i] >= patterns) patterns = orders[i] + 1;
    pattern_data = mod + 1084;
    p = pattern_data + (long)patterns * 1024;
    memset(smp, 0, sizeof smp);
    for (i = 1; i <= 31; i++) {
        const unsigned char *h = mod + 20 + (i - 1) * 30;
        sample *s = &smp[i];
        s->length = be16(h + 22) * 2;
        s->finetune = h[24] & 15; if (s->finetune > 7) s->finetune -= 16;
        s->volume = h[25] > 64 ? 64 : h[25];
        s->loop_start = be16(h + 26) * 2;
        s->loop_length = be16(h + 28) * 2;
        if (p + s->length > mod + size) s->length = (int)(mod + size - p);
        if (s->length < 0) s->length = 0;
        if (s->loop_length <= 2 || s->loop_start >= s->length) s->loop_length = 0;
        else if (s->loop_start + s->loop_length > s->length) s->loop_length = s->length - s->loop_start;
        s->data = (const signed char *)p;
        p += s->length;
    }

    memset(ch, 0, sizeof ch);
    memset(visited, 0, sizeof visited);
    max_frames = (long)rate * MAX_SECONDS;
    room = (long)rate * 120;
    out = malloc((size_t)room * 2 * sizeof *out);
    if (!out) return NULL;
    tick_frames = rate * 2.5 / bpm;

    while (!done && frames < max_frames) {
        if (tick_left <= 0) {
            int next_order = -1, next_row = -1;

            if (tick == 0) {
                const unsigned char *cell;
                if (row == 0 && !pattern_delay) {
                    if (visited[order]) break;          /* the song has come round */
                    visited[order] = 1;
                }
                cell = pattern_data + (long)orders[order] * 1024 + row * 16;
                for (c = 0; c < CHANNELS; c++, cell += 4) {
                    channel *k = &ch[c];
                    int n = (cell[0] & 0xF0) | (cell[2] >> 4);
                    int period = ((cell[0] & 15) << 8) | cell[1];
                    int x, y;

                    k->effect = cell[2] & 15;
                    k->param = cell[3];
                    x = k->param >> 4; y = k->param & 15;
                    k->note_period = period;
                    k->note_smp = NULL;
                    if (n >= 1 && n <= 31) {
                        k->note_smp = &smp[n];
                        k->volume = smp[n].volume;
                        if (!period) k->smp = &smp[n];
                    }
                    if (period) {
                        if (k->effect == 3 || k->effect == 5) {
                            const sample *s = k->note_smp ? k->note_smp : k->smp;
                            k->target = tuned(period, s ? s->finetune : 0);
                        } else if (!(k->effect == 14 && x == 13 && y)) {
                            start_note(k);
                        }
                    }
                    switch (k->effect) {
                    case 3: if (k->param) k->porta_speed = k->param; break;
                    case 4: if (x) k->vib_speed = x; if (y) k->vib_depth = y; break;
                    case 7: if (x) k->trem_speed = x; if (y) k->trem_depth = y; break;
                    case 9:
                        if (k->param) k->offset_memory = k->param;
                        if (period && k->smp) {
                            k->pos = k->offset_memory * 256;
                            if (k->pos >= k->smp->length) k->playing = 0;
                        }
                        break;
                    case 11: next_order = k->param; next_row = 0; break;
                    case 12: k->volume = k->param > 64 ? 64 : k->param; break;
                    case 13: if (next_order < 0) next_order = order + 1; next_row = x * 10 + y; if (next_row > 63) next_row = 0; break;
                    case 15:
                        if (k->param == 0) break;
                        if (k->param < 32) speed = k->param; else { bpm = k->param; tick_frames = rate * 2.5 / bpm; }
                        break;
                    case 14:
                        switch (x) {
                        case 1: k->period -= y; if (k->period < 113) k->period = 113; break;
                        case 2: k->period += y; if (k->period > 856) k->period = 856; break;
                        case 6:
                            if (!y) k->loop_row = row;
                            else if (k->loop_count == 0) { k->loop_count = y; next_row = k->loop_row; next_order = order; }
                            else if (--k->loop_count) { next_row = k->loop_row; next_order = order; }
                            break;
                        case 10: k->volume += y; if (k->volume > 64) k->volume = 64; break;
                        case 11: k->volume -= y; if (k->volume < 0) k->volume = 0; break;
                        case 14: pattern_delay = y + 1; break;
                        default: break;
                        }
                        break;
                    default: break;
                    }
                }
            } else {
                for (c = 0; c < CHANNELS; c++) {
                    channel *k = &ch[c];
                    int x = k->param >> 4, y = k->param & 15;
                    switch (k->effect) {
                    case 1: k->period -= k->param; if (k->period < 113) k->period = 113; break;
                    case 2: k->period += k->param; if (k->period > 856) k->period = 856; break;
                    case 3: tone_porta(k); break;
                    case 4: k->vib_pos += k->vib_speed; break;
                    case 5: tone_porta(k); volume_slide(k, k->param); break;
                    case 6: k->vib_pos += k->vib_speed; volume_slide(k, k->param); break;
                    case 7: k->trem_pos += k->trem_speed; break;
                    case 10: volume_slide(k, k->param); break;
                    case 14:
                        if (x == 9 && y && tick % y == 0) { k->pos = 0; k->playing = k->smp != NULL; }
                        else if (x == 12 && tick == y) k->volume = 0;
                        else if (x == 13 && tick == y) start_note(k);
                        break;
                    default: break;
                    }
                }
            }

            /* What each channel sounds like for this tick. */
            for (c = 0; c < CHANNELS; c++) {
                channel *k = &ch[c];
                int x = k->param >> 4, y = k->param & 15;
                k->out_period = k->period;
                k->out_volume = k->volume;
                if (k->effect == 0 && k->param && k->period) {
                    int semis = tick % 3 == 1 ? x : tick % 3 == 2 ? y : 0;
                    k->out_period = k->period * pow(2.0, -semis / 12.0);
                } else if (k->effect == 4 || k->effect == 6) {
                    k->out_period = k->period + vibrato(k);
                } else if (k->effect == 7) {
                    int v = sine[k->trem_pos & 31] * k->trem_depth / 64;
                    k->out_volume += (k->trem_pos & 32) ? -v : v;
                    if (k->out_volume > 64) k->out_volume = 64;
                    if (k->out_volume < 0) k->out_volume = 0;
                }
                k->step = k->out_period > 0 ? PAL_CLOCK / k->out_period / rate : 0;
            }

            if (++tick >= speed) {
                tick = 0;
                if (pattern_delay > 1) { pattern_delay--; }
                else {
                    pattern_delay = 0;
                    if (next_row >= 0 || next_order >= 0) {
                        if (next_order >= 0) order = next_order;
                        row = next_row >= 0 ? next_row : 0;
                    } else if (++row >= 64) { row = 0; order++; }
                    if (order >= song_length) done = 1;
                }
            }
            tick_left += tick_frames;
        }

        {
            double left = 0, right = 0;
            for (c = 0; c < CHANNELS; c++) {
                channel *k = &ch[c];
                const sample *s = k->smp;
                double v, frac;
                int ip, a, b;
                if (!k->playing || !s || !s->length) continue;
                ip = (int)k->pos;
                if (ip >= s->length) { k->playing = 0; continue; }
                frac = k->pos - ip;
                a = s->data[ip];
                if (ip + 1 < s->length) b = s->data[ip + 1];
                else b = s->loop_length ? s->data[s->loop_start] : 0;
                v = (a + (b - a) * frac) * k->out_volume;
                if (c == 0 || c == 3) { left += v; right += v * SEPARATION; }
                else                  { right += v; left += v * SEPARATION; }
                k->pos += k->step;
                if (s->loop_length) {
                    while (k->pos >= s->loop_start + s->loop_length) k->pos -= s->loop_length;
                } else if (k->pos >= s->length) {
                    k->playing = 0;
                }
            }
            left *= GAIN / 32768.0; right *= GAIN / 32768.0;
            if (left > 1.0) left = 1.0; else if (left < -1.0) left = -1.0;
            if (right > 1.0) right = 1.0; else if (right < -1.0) right = -1.0;
            if (frames == room) {
                float *more = realloc(out, (size_t)room * 4 * sizeof *out);
                if (!more) break;
                out = more;
                room *= 2;
            }
            out[frames * 2] = (float)left;
            out[frames * 2 + 1] = (float)right;
            frames++;
            tick_left -= 1.0;
        }
    }

    *frames_out = frames;
    return out;
}
