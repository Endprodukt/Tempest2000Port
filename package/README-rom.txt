TEMPEST 2000 for VectorPie - release edition
=============================================

Jeff Minter's Tempest 2000 (Atari Jaguar, 1994) on a real colour vector
monitor: a VectorPie package for a Raspberry Pi with a USB-DVG board.

This edition contains none of Atari's data. It needs the Tempest 2000 Jaguar
cartridge ROM, which you provide yourself. The game takes the level data,
the enemy waves, the sound effects and the music from it when it starts.


THE ROM
-------

Exactly this ROM is needed (the Tempest 2000 cartridge image as it is commonly
circulated, 2 MB):

  Size     2097152 bytes (2 MB)
  CRC32    6B2B95AD
  MD5      602bc9953d3737b1ba52b2a0d9932f7c
  SHA-1    3828d2f953224d06b5fe7ab3644d6a75fdfe79e3
  SHA-256  f82b80771f410b4f4cda4726f7926b07bcada728ee2c75ef4d3e8c3b20ca34ce

The hashes are of the 2 MB image itself. A file with an 8 KB header in front
of it (2105344 bytes) is accepted too; the header is skipped. The file name
does not matter, the ending does: .j64, .jag, .rom or .bin.

Any other ROM, or a damaged one, is refused: the game then shows a page that
says what is wrong and where the ROM belongs.


INSTALLING
----------

1. Copy tempest2000-<version>-rom.vpk to a USB drive (top folder or a folder
   called games) and install it from the VectorPie menu:
   Settings -> GAMES -> INSTALL FROM USB.

2. Put the ROM into the game's save folder:

     /persistent/games/tempest2000/save/

   (Next to the program, in /persistent/games/tempest2000/, works as well,
   but save/ is kept when the game is updated.)

3. Start Tempest 2000 from the menu (Atari).


CONTROLS
--------

  Spinner (or left / right)   move the claw
  Fire                        fire
  Superzapper                 superzapper
  Jump                        jump (once the jump powerup is collected)
  Coin, Start 1 / Start 2     credits, one or two players (taking turns)
  Game select: Tempest 2000 Beastly is a game type of its own

F2 on a keyboard opens the service menu: volumes, free play, checkpoints,
spinner calibration (one full turn should move the claw 5 lanes), button
layout, brightness and stars. SAVE writes save/settings.ini.

The settings the game starts with are in game.ini next to the program; it
also explains how to use music of your own (save/music/tune1.wav ...).


CREDITS
-------

  Original game designed by   Dave Theurer
  Jaguar version by           Yak (Jeff Minter)
  Bitmap artwork by           Joby
  Tunes by                    Imagitec Design
  VectorPie port by           Endprodukt

Tempest 2000 is (c) 1981, 1994 Atari Corp. This port is a fan project. It
was made from the game's source code as published at
https://github.com/mwenge/tempest2k and contains no part of the game itself.


-----------------------------------------------------------------------------

DEUTSCH, KURZ
-------------

Diese Ausgabe enthaelt keine Daten von Atari und braucht das Tempest-2000-ROM
des Jaguar (Hashes oben). Das ROM als .j64, .jag, .rom oder .bin in den Ordner

  /persistent/games/tempest2000/save/

legen und das Spiel aus dem VectorPie-Menue starten. Ein falsches oder
beschaedigtes ROM wird abgelehnt; das Spiel zeigt dann, was fehlt. F2 oeffnet
das Service-Menue.
