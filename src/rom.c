/* rom.c - the release edition: the game's data out of the player's ROM. */
#include "rom.h"

#include <vectorpie/vp.h>

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HEADER 8192             /* what some dumps put before the ROM itself */

static unsigned char *s_rom;
static const char *s_problem = "NO ROM FOUND";
static char s_folder[1024];

static unsigned long crc32_of(const unsigned char *p, long n)
{
    unsigned long crc = 0xFFFFFFFFUL;
    long i;
    int k;

    for (i = 0; i < n; i++) {
        crc ^= p[i];
        for (k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320UL & (0UL - (crc & 1)));
    }
    return crc ^ 0xFFFFFFFFUL;
}

static int is_rom_name(const char *name)
{
    const char *dot = strrchr(name, '.');
    char ext[8];
    int i;

    if (!dot || strlen(dot) > 5) return 0;
    for (i = 0; dot[i] && i < 7; i++) ext[i] = (char)tolower((unsigned char)dot[i]);
    ext[i] = 0;
    return !strcmp(ext, ".j64") || !strcmp(ext, ".jag") || !strcmp(ext, ".rom") || !strcmp(ext, ".bin");
}

/* The ROM in this file, checked: 1 and s_rom set, or 0. */
static int try_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    long size, skip;
    unsigned char *data;

    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    skip = size == rom_size + HEADER ? HEADER : 0;
    if (size - skip != rom_size) { fclose(f); return 0; }
    data = malloc((size_t)size);
    if (!data || fread(data, 1, (size_t)size, f) != (size_t)size) { free(data); fclose(f); return 0; }
    fclose(f);
    if (crc32_of(data + skip, rom_size) != rom_crc) {
        s_problem = "THE ROM FOUND IS NOT TEMPEST 2000, OR IS DAMAGED";
        free(data);
        return 0;
    }
    if (skip) memmove(data, data + skip, (size_t)rom_size);
    s_rom = data;
    return 1;
}

static int try_folder(const char *folder)
{
    char path[1300];
    DIR *dir = opendir(folder);
    struct dirent *e;
    int found = 0;

    if (!dir) return 0;
    while (!found && (e = readdir(dir)) != NULL) {
        if (!is_rom_name(e->d_name)) continue;
        snprintf(path, sizeof path, "%s/%s", folder, e->d_name);
        found = try_file(path);
    }
    closedir(dir);
    return found;
}

int rom_load(void)
{
    char here[1024];

    snprintf(s_folder, sizeof s_folder, "%s", vp_save_dir());
    vp_asset_path("", here, sizeof here);
    if (!try_folder(s_folder) && !try_folder(here)) return 0;
    rom_fill_tables(s_rom);
    s_problem = NULL;
    return 1;
}

const char *rom_problem(void) { return s_problem; }
const char *rom_folder(void)  { return s_folder; }

const unsigned char *rom_effect(int fx, long *length)
{
    if (!s_rom || fx < 0 || fx >= 30 || !rom_effects[fx].length) return NULL;
    *length = rom_effects[fx].length;
    return s_rom + rom_effects[fx].at;
}

const unsigned char *rom_tune(int tune, long *length)
{
    if (!s_rom || tune < 1 || tune > 7) return NULL;
    *length = rom_tunes[tune - 1].length;
    return s_rom + rom_tunes[tune - 1].at;
}
