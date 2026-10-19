# Bauen

## PC (Windows, nativ)

Voraussetzung: MSYS2 mit MinGW-w64-GCC und SDL2 unter `C:\msys64\mingw64`, dazu CMake und Ninja.

```bash
export PATH="/c/msys64/mingw64/bin:$PATH"
cmake -S . -B build/pc -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_PREFIX_PATH=C:/msys64/mingw64
cmake --build build/pc
build/pc/tempest2000.exe --windowed            # 4:3
build/pc/tempest2000.exe --windowed --vertical # 3:4, wie im Tempest-Kabinett
```

`VP_STATS=1` gibt einmal pro Sekunde die Kosten eines Bildes aus (Linien, Sprünge, Weglängen).

## Raspberry Pi (arm64)

Ohne WSL: clang und lld vom PC, gegen eine Kopie der Bibliotheken und Header des Automaten. Der Automat (192.168.0.122) gibt sein Dateisystem als Laufwerk P: frei.

Einmalig die Kopie anlegen (`build/pi-sysroot`, rund 70 MB):

```bash
R="build/pi-sysroot"; mkdir -p "$R/usr/include" "$R/usr/lib/aarch64-linux-gnu" "$R/usr/lib/gcc/aarch64-linux-gnu" "$R/lib"
(cd /p/usr/include && cp -L *.h "$OLDPWD/$R/usr/include/"; for d in aarch64-linux-gnu c++ SDL2 linux asm-generic arpa net netinet sys rpc misc; do cp -rL $d "$OLDPWD/$R/usr/include/"; done)
(cd /p/usr/lib/aarch64-linux-gnu && cp -L crt1.o crti.o crtn.o Scrt1.o libc.so libc.so.6 libc_nonshared.a ld-linux-aarch64.so.1 libm.so libm.so.6 libmvec.so.1 libpthread.so.0 libdl.so.2 librt.so.1 libstdc++.so.6 libgcc_s.so.1 libSDL2-2.0.so.0 "$OLDPWD/$R/usr/lib/aarch64-linux-gnu/")
cp -rL /p/usr/lib/gcc/aarch64-linux-gnu/14 "$R/usr/lib/gcc/aarch64-linux-gnu/"
cp -r "$R/usr/lib/aarch64-linux-gnu" "$R/lib/"; cp "$R/usr/lib/aarch64-linux-gnu/ld-linux-aarch64.so.1" "$R/lib/"
```

Bauen (clang aus `C:/Users/ocare/.local/share/retcomm/toolchains/cmake-clang-v1/latest/bin` im PATH):

```bash
cmake -S . -B build/pi -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-pi-clang.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build/pi
```

Auf den Automaten kopieren (der Ordner ist über P: beschreibbar):

```bash
D=/p/usr/local/share/advance/tempest2000_dir; mkdir -p $D && cp build/pi/tempest2000 package/game.ini $D/ && cp -r assets $D/
```

Auf dem Automaten starten, vom PC aus. `run.sh` (aus `package/`) beendet das Menü, startet das Spiel auf dem Vektor-Board und holt das Menü danach zurück:

```bash
cp package/run.sh /p/usr/local/share/advance/tempest2000_dir/ && ssh -i ~/.ssh/id_ed25519_vectorpie pi@192.168.0.122 'chmod +x /usr/local/share/advance/tempest2000_dir/run.sh; nohup setsid /usr/local/share/advance/tempest2000_dir/run.sh > /dev/null 2>&1 < /dev/null &'
```

Die Bildkosten stehen danach laufend in `/usr/local/share/advance/tempest2000_dir/run.log` (`VP_STATS=1`). Das Menü reagiert nicht auf ein einfaches Beenden, `run.sh` stoppt deshalb die Anmeldung auf tty1 (`getty@tty1`) und startet sie nach dem Spiel wieder.

Der Weg des SDK über WSL2 mit Debian 13 (`vendor/VectorPie/sdk/tools/setup-wsl.sh`) geht ebenfalls, ist hier aber nicht eingerichtet.

## Einstellungen in game.ini

`package/game.ini` wird neben die ausführbare Datei kopiert und beim Start gelesen.

- `sensitivity`: Spinner und Maus. 1.0 heißt 1200 Zählschritte für 5 Bahnen, 2.0 doppelt so weit, negativ kehrt die Richtung um.
- `volume`, `music_volume`, `effects_volume`, `voice_volume`: je 0 bis 100.
- `attract_sound`: `yes` oder `no`, Ton solange niemand spielt.
- `voice_pitch`: wie viel schneller als aufgenommen die Stimmen laufen.
- `free_play`: `yes` heißt Start ohne Münze.
- `unlocked_level`: bis zu welchem Level der Start von Anfang an wählbar ist (ab Werk 15).
- `beastly_mode`: `yes` schaltet den Beastly Mode frei.
- `checkpoints`: `yes` lässt nach einem verlorenen Leben nur die übrigen Gegner wiederkommen, `no` beginnt die Welle neu.
- Eigene Musik: `music/tune1.wav` bis `tune7.wav` neben `game.ini`, siehe die Erklärung in der Datei.
- `jump_button`: ein zweiter Eingang für den Sprung, mit seinem advmame.rc-Namen.

## Menü-Eintrag auf dem Automaten

Wie bei den anderen Spielen des Menüs in `/usr/local/share/advance`:

- der Ordner `tempest2000_dir` mit Programm, `game.ini`, `assets`, `marquee.png`, `save` und `run.sh`
- die Verweise `tempest2000_dvg` und `tempest2000_sdl` auf `tempest2000_dir/tempest2000`; das Programm liest an seinem Namen ab, wohin es zeichnet
- in `tempest2000_dir` der Verweis `highscores.txt` auf `save/highscores.txt` für die Anzeige im Menü
- in `gamelist.ini` die Zeile `Atari|Tempest 2000|tempest2000|tempest2000|tempest2000`; die Fassung davor liegt als `/home/pi/gamelist.ini.before-tempest2000`

## Spielstände

Im Spielordner unter `save`: `highscores.txt` (Bestenliste, vom SDK geschrieben) und `progress.txt` (höchstes erreichtes Start-Level je Modus und ob der Beastly Mode freigeschaltet ist). Geschrieben wird am Spielende. Zum Zurücksetzen die Dateien löschen.

## Marquee

`package/marquee.png` gehört neben das Spiel (das SDK zeigt es beim Spielen auf dem HDMI-Schirm) und für das Menü nach `/usr/local/share/advance/artwork/marquees/tempest2000.png`.

## Testhilfe

`T2K_LEVEL=58` in der Umgebung schaltet die Start-Level bis 58 in allen Modi frei.

## Spieldaten neu erzeugen

```bash
python tools/extract_data.py
```

Liest `reference/tempest2k/src/yak.s` und schreibt `src/gen/t2k_data.c` (Netze, Level-Reihenfolge, Vektorobjekte). Die erzeugte Datei nicht von Hand ändern.

## Ton neu erzeugen

```bash
export PATH="/c/msys64/mingw64/bin:$PATH"
python tools/extract_audio.py
```

Schreibt `assets/sounds/fx00.wav` bis `fx29.wav` und kopiert die sieben Musikstücke als `assets/music/tune1.mod` bis `tune7.mod` (zusammen 1,3 MB). Das Spiel spielt die Module selbst.

Für stille Testläufe: `SDL_AUDIODRIVER=dummy` setzen.

## Bildschirmfoto des Spielfensters

`tools/snap.ps1 -Out <pfad> [-Keys ...] [-ExeArgs ...]` startet das Spiel, schickt Tasten an das Fenster und speichert nur dieses Fenster als PNG. Tasten als virtuelle Tastencodes in Hex, `+`/`-` für gedrückt halten/loslassen, `wNNN` wartet, `snap` speichert ein Zwischenbild.

## Selbsttest

Das Spiel spielt sich selbst durch alle Level und meldet jedes auf der Fehlerausgabe. Am PC, im Ordner `build/pc`:

```bash
T2K_SELFTEST=400,2,0,1 SDL_AUDIODRIVER=dummy ./tempest2000.exe --windowed 2> selftest.txt
```

Die vier Zahlen: Schritte je Bild (400 heißt 100 Level in rund einer halben Minute), Spielart (0 Traditional, 1 Plus, 2 Tempest 2000), Beastly Mode (0 oder 1), Startlevel. Das Programm endet von selbst; die letzte Zeile fasst zusammen (Tode, hängende Level, Bereichsverletzungen, Level mit nicht gesehenem Gegner). Es gibt unendlich Leben, Bestenliste und Spielstand werden nicht angefasst. Je Level steht hinter `launched:` für jeden Welleneintrag, wie oft er gestartet hat und wie viele Bilder er startbereit war, ohne Platz zu bekommen (`Starts/Bilder`).

Prüf-Version mit Fallen für Feldgrenzen und Nullzeiger, unter `gdb` gestartet, damit ein Fehler seine Stelle zeigt:

```bash
cmake -S . -B build/pc-check -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_PREFIX_PATH=C:/msys64/mingw64 -DCMAKE_BUILD_TYPE=Debug "-DCMAKE_C_FLAGS=-O1 -g -fsanitize=bounds,null,integer-divide-by-zero -fsanitize-undefined-trap-on-error"
```

```bash
cmake --build build/pc-check
```

```bash
T2K_SELFTEST=200,2,0,1 SDL_AUDIODRIVER=dummy gdb -batch -ex run -ex bt --args build/pc-check/tempest2000.exe --windowed
```

## Paket für den Automaten

Nach dem Pi-Build:

```bash
sh tools/make_vpk.sh
```

Das schreibt `build/tempest2000-<version>.vpk`: Pi-Programm, `game.ini`, `assets`, Marquee. Auf einen USB-Stick in den Ordner `games` kopieren und am Automaten über Settings, GAMES, INSTALL FROM USB installieren. Einstellungen, Bestenliste und eigene Musik liegen dort im Ordner `save` und bleiben bei einem Update erhalten.

## Release-Ausgabe (braucht das Jaguar-ROM)

Neben der vollen Ausgabe gibt es eine Ausgabe ohne Daten von Atari. Sie holt sich Tabellen, Soundeffekte und Musik beim Start aus dem Tempest-2000-ROM des Spielers (2 MB, `.j64` `.jag` `.rom` `.bin`, mit oder ohne 8-KB-Kopfteil, geprüft per CRC32 `6b2b95ad`). Das ROM liegt im Ordner `save` des Spiels oder neben dem Programm; fehlt es, zeigt das Spiel, was fehlt und wohin es gehört.

Die Datendatei dazu nach jeder Änderung an `tools/extract_data.py` neu erzeugen (liest `src/gen/t2k_data.c` und das ROM aus `reference/tempest2k/orig/t2k.jag`):

```bash
python tools/make_rom_data.py
```

Bauen in eigenen Ordnern, PC und Pi:

```bash
cmake -S . -B build/pc-rom -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_PREFIX_PATH=C:/msys64/mingw64 -DT2K_ROM_DATA=ON
```

```bash
cmake -S . -B build/pi-rom -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-pi-clang.cmake -DCMAKE_BUILD_TYPE=Release -DT2K_ROM_DATA=ON
```

Paket: `sh tools/make_vpk.sh rom` schreibt `build/tempest2000-<version>-rom.vpk`. Es enthält `package/README-rom.txt` als `README.txt` (Installation, Hashes des ROM, Bedienung, Credits); bei einer neuen ROM-Prüfsumme dort mit anpassen. Beide Ausgaben haben dieselbe Paket-Kennung; auf einem Automaten ersetzt die eine die andere.
