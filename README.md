# Tempest 2000 for VectorPie

Jeff Minter's Tempest 2000 (Atari Jaguar, 1994) on a real colour vector
monitor: a fan port for [VectorPie](https://github.com/Goldfinch2/VectorPie),
a Raspberry Pi with a USB-DVG board driving an arcade vector screen. It also
runs in a window on Windows.

The game is ported from the original's published source code
([mwenge/tempest2k](https://github.com/mwenge/tempest2k)): webs, waves,
enemies, powerups, effects and texts follow `yak.s` and the GPU code, not
guesswork.

## You need the Jaguar ROM

This repository and the release packages contain **none of Atari's data**.
The game takes its tables, sound effects and music from the Tempest 2000
Jaguar cartridge ROM when it starts. You provide that ROM yourself:

| | |
|---|---|
| Size | 2,097,152 bytes (2 MB); a file with an 8 KB header in front is accepted too |
| CRC32 | `6B2B95AD` |
| MD5 | `602bc9953d3737b1ba52b2a0d9932f7c` |
| SHA-1 | `3828d2f953224d06b5fe7ab3644d6a75fdfe79e3` |
| File ending | `.j64`, `.jag`, `.rom` or `.bin` (the name does not matter) |

Any other or damaged ROM is refused, and the game shows a page that says what
is wrong and where the ROM belongs.

## Installing on a VectorPie cabinet

1. Download `tempest2000-<version>-rom.vpk` from the
   [releases](https://github.com/Endprodukt/Tempest2000Port/releases).
2. Copy it to a USB drive (top folder or a folder called `games`) and install
   it from the VectorPie menu: Settings → GAMES → INSTALL FROM USB.
3. Put the ROM into `/persistent/games/rom/`. The game looks there first,
   then in its save folder `/persistent/games/tempest2000/save/`, then next
   to the program.
4. Start Tempest 2000 from the menu (under Atari).

Settings, high scores and music of your own live in the save folder and are
kept when the game is updated.

## Playing

- Spinner (or left / right): move the claw
- Fire, Superzapper, Jump (once the jump powerup is collected)
- Coin, Start 1: credits and start

Game types: Traditional, Tempest Plus (with the A.I. droid), Tempest 2000, and
Tempest 2000 Beastly (offered when `beastly_mode = yes`). Each game type has
its own high score table; in attract mode the tables take turns, and turning
the spinner on the table page shows the next one.

F2 on a keyboard opens the service menu: volumes, free play, checkpoints,
spinner calibration, button layout, brightness and stars.

## Building

See [BUILD.md](BUILD.md) (in German). In short:

- **Release edition** (needs the ROM at run time): builds straight from this
  repository with CMake and `-DT2K_ROM_DATA=ON`, for Windows (MSYS2 MinGW-w64
  with SDL2) or cross-compiled for the Raspberry Pi (arm64).
  `sh tools/make_vpk.sh rom` makes the package.
- **Full edition** (data built in, for your own cabinet only): clone the
  original's source into `reference/tempest2k`, run
  `python tools/extract_data.py` and `python tools/extract_audio.py`, then
  build without `T2K_ROM_DATA`. The generated files are ignored by Git and
  must never be committed.

`STATUS.md` records the state of the port, what differs from the original
and the known limitations; `RESUME.md` is the guide for working on the code.
Both are in German.

## Credits

- Original game designed by Dave Theurer
- Jaguar version by Yak (Jeff Minter)
- Bitmap artwork by Joby
- Tunes by Imagitec Design
- VectorPie port by Endprodukt

Tempest 2000 is © 1981, 1994 Atari Corp. This is a non-commercial fan
project, not affiliated with or endorsed by Atari.

---

**Deutsch, kurz:** Ein Fan-Port von Tempest 2000 für VectorPie-Vektorautomaten.
Repository und Pakete enthalten keine Daten von Atari; man braucht das
Tempest-2000-ROM des Jaguar (Hashes oben) und legt es nach
`/persistent/games/rom/`. Paket aus den Releases per USB installieren, ROM
ablegen, Spiel aus dem Menü starten.
