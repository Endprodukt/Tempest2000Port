/* rom.h - the release edition: the game's data out of the player's ROM.
 *
 * Built with T2K_ROM_DATA, the program holds none of Atari's data. At the
 * start it looks for the Tempest 2000 Jaguar ROM (a 2 MB .j64, .jag, .rom or
 * .bin, with or without the 8 KB header some dumps have) in save/ and next
 * to the program, checks it, and takes the game's tables, sound effects and
 * tunes from it.
 */
#ifndef T2K_ROM_H
#define T2K_ROM_H

/* Where something lies in the ROM. */
typedef struct rom_place { long at, length; } rom_place;

/* Written by tools/make_rom_data.py (src/gen/t2k_data_rom.c). */
extern const rom_place rom_effects[30];     /* by the original's effect number; length 0: not in it */
extern const rom_place rom_tunes[7];        /* by the original's tune number, less one */
extern const long rom_size;
extern const unsigned long rom_crc;
void rom_fill_tables(const unsigned char *rom);

/* Find and check the ROM and fill the game's tables from it. 1 when done;
 * 0, and rom_problem() says what is wrong and where the ROM belongs. */
int rom_load(void);
const char *rom_problem(void);
const char *rom_folder(void);

/* A sound effect's samples (8-bit signed) or a tune (a ProTracker module),
 * as they are in the ROM; NULL when there is none or no ROM. */
const unsigned char *rom_effect(int fx, long *length);
const unsigned char *rom_tune(int tune, long *length);

#endif /* T2K_ROM_H */
