# Weiterarbeiten

## Aufbau

- `reference/tempest2k/`: Klon von mwenge/tempest2k. Nur lesen. `src/yak.s` ist die Spiellogik, `src/gpu/*.gas` das Zeichnen.
- `vendor/VectorPie/`: Klon von Goldfinch2/VectorPie. Das SDK liegt in `sdk/`, die Doku in `sdk/docs/`.
- `tools/extract_data.py`: erzeugt `src/gen/t2k_data.c` aus `yak.s`.
- `src/view.c`: Perspektive, Linienausgabe, Objekte (`view_solid` für Tempest-2000-Formen mit Flächenfarben, `view_vobj` für Classic-Formen), CRY-Farben. Raum wie im Original: x rechts, y nach unten, z in die Tiefe. Netzrand bei z=30, Netzende bei z=190 (`webz` ± 80). Der Betrachter steht bei z=0, Projektion 128 * x / z wie `polyo2d` in `gpu/horse.gas`.
- `src/game.c`: Zustand, 60-Hz-Schritt, Zeichnen.
- `src/sound.c`: Effekte (`sound_fx` entspricht `fox`, Tonhöhe als Periode wie `sfx_pitch`) und Musik (`sound_tune` entspricht `modnum`).
- `tools/extract_audio.py`: erzeugt `assets/sounds/fxNN.wav` (Raten nach dem Treiber) und kopiert die Module nach `assets/music/tuneN.mod`. CMake kopiert `assets` neben das Programm. `src/mod.c` spielt die Module im Spiel, `tools/make_vpk.sh` baut das Paket.
- Bauen und Testen: siehe `BUILD.md`.

## Konventionen

- Der Schritt läuft mit 60 Hz wie `Frame` im Original. Werte pro Bild werden unverändert übernommen, 16.16-Festkomma aus `yak.s` über `FIX()`.
- Neben jedem portierten Teil steht der Name der Originalroutine als Kommentar.
- Gezeichnet wird der Tempest-2000-Modus: Formen aus `obj2d.s` (`t2k_sclaws`, `t2k_s_flipper` usw.), nicht die Classic-Vektorobjekte.
- Daten kommen aus dem Extraktor, nicht von Hand. Neue Tabellen dort ergänzen.
- Eingaben nur über die advmame.rc-Namen: `p1_dialx`, `p1_left`, `p1_right`, `p1_button1` (Feuer), `p1_button2` (Superzapper), `p1_button3` (Sprung), `coin1`/`coin2`, `start1`.
- Linien möglichst aneinanderhängend zeichnen. Jeder Sprung des Strahls kostet Zeit auf der Röhre.
- Werte, die nicht aus dem Original stammen, sind im Code als solche markiert.

## Wegweiser in yak.s

- Objektstruktur und Tabelle aller Objekttypen mit Erzeugen/Aktualisieren/Zeichnen: Zeile 100 bis 300.
- `run_vex` (Aktualisieren) ab 16183, `draw_vex` ab 16474.
- `run_wave` (Gegnerwellen) ab 17887.
- `claw_con1`, `clawcon`, `domove`, `nutargg`: 13409 bis 13700.
- `fire` (Superzapper) 9686, `frab` (Schuss) 9892, `player_shot` 10212.
- Gegner-Grundwerte (`flip_zspeed` usw.) direkt hinter `raw_webs`, ab 23950.
- Soundaufrufe: `move #n,sfx` und `jsr fox`. Musikwahl über `modnum`, `webtunes` bei 19152.
- Netze `web1` bis `web37` ab 20717, Vektorobjekte ab 20386 und ab 22042.
- `sweb` (Netz, Musik und Farbe je Level) ab 8589, `solids` (Zeichenroutinen der Tempest-2000-Objekte) bei 23707, `drawsolidxy` bei 2746.
- `obj2d.s`: Flächenobjekte. Aufbau: Anzahl Flächen, je Fläche Farbe und 3 x (Eckpunkt, Helligkeit), danach die Eckpunkte um die Mitte (9, 9).

## Aufbau von game.c

- Ein Objekt-Pool mit 32 Plätzen wie `activeobjects`. `kind` wählt Aktualisieren (`run_objects`) und Zeichnen (`draw_obj`).
- z und Geschwindigkeiten sind 16.16-Festkomma (`fix`). Vergleiche laufen wie im Original über das obere Wort (`ZI`). Rand bei 30, Netzende bei 190.
- Neue Gegner starten mit `far` = 300 unter dem Netz und werden bis zur Ankunft nur als Punkt gezeichnet (`ipix`).
- Zustände: Attract, Modus-Wahl, Start-Level-Wahl, Einflug, Spiel, gefangen, explodiert, Flug durch das Netz, Game Over, Initialen.
- Spielmodus über zwei Schalter wie im Original: `G.t2k` (Regeln von Tempest 2000) und `G.solids` (Flächenformen statt Classic-Vektorformen, `blanka`). `obj_shape` liefert die Form je Modus.
- Text für Anzeigen und Menüs mit `plain()` (Strichschrift des SDK). Nur die zerfallenden Meldungen im Spiel zeichnen Pixelzeilen (`view_text`, `t2k_afont`).
- `src/service.c`: das Service-Menü. `game.c` gibt ihm die Einstellungen (`settings_to_service`), nimmt sie zurück (`settings_from_service`) und schreibt die `game.ini` (`save_settings`). Eine neue Einstellung braucht: Feld in `service_settings`, Zeile in `service.c`, beide Übergaben, `load_settings`, `save_settings`.
- Helligkeit: vor dem Zeichnen jeder Gruppe `view_level(G.bright[BRIGHT_...])`; Farben nur über `view_cry`, `view_cry_i`, `view_rgb`, nie `vp_color` direkt (sonst greift die Stufe nicht).
- Release-Ausgabe: `tools/make_rom_data.py` nach jeder Änderung an den erzeugten Daten neu laufen lassen, sonst passen Release- und volle Ausgabe nicht zusammen. `src/rom.c` sucht und prüft das ROM; `sound.c` und `game.c` haben dafür `#ifdef T2K_ROM_DATA`.
- `src/words.c`: die großen Bonuswörter aus Strichen, die zerfliegen. Neue Buchstaben in `glyph()` eintragen, sonst bleibt die Stelle leer.
- `src/logo.c`: die Titelseite, "TEMPEST" und "2000" als Körper im Raum.
- Pixel-Effekte des Originals liegen in `src/view.c`: `view_text` entspricht `texter` (Schrift aus `t2k_afont`). Ringe und Kugel (`pixel_ring`, `draw_rings`, `draw_sphere`) stehen in `src/game.c`.
- Nichts erfinden. Vor jedem Effekt, Text oder Ablauf die Routine in `yak.s` und `gpu/*.gas` lesen und die Rechnung übernehmen. Was nicht aus dem Quelltext stammt, steht in `STATUS.md` unter den Einschränkungen.
- Effekte und Powerups sind Objekte im selben Pool (`K_POWERUP`, `K_RINGS`, `K_SPHERE`, `K_BONUS`, `K_SPLAT`).
- Werte möglichst aus dem Original ableiten statt schätzen. Die geschätzte Perspektive war der Grund, warum das Spiel zuerst falsch aussah.

## Nächste Schritte

1. Rückmeldung des Nutzers vom Automaten einarbeiten (er testet laufend). Offen vor allem: Klang und Tempo des neuen Level-Endes (`update_zoom`, `update_warpin`, `sound_zoom_up`; Stellschrauben `WARPIN_SPEED` und der Tonhöhenschritt in `sound_zoom_up`), und ob die Anzeige "ZAP" bleiben soll.
2. Nach jeder Änderung an der Spiellogik den Selbsttest laufen lassen (`BUILD.md`, Abschnitt Selbsttest).
3. Musik-Lücke klären: nachfragen, in welchem Level sie auftritt.

Der Automat ist erreichbar: Bauen und Starten stehen in `BUILD.md`, die Bildkosten nach jedem Start in `run.log` im Spielordner.

## Belegung (Stand 2026-10-19, 0.2.4)

- Gemeinsames Modul `src/vpbind.[ch]` (Hauptkopie) – Kopien in allen Vektorspielen (siehe STATUS.md). Ändert man es, alle Kopien ersetzen und neu bauen.
- Regeln: Menüs nur feste Eingaben; START/Enter, erster Knopf ersetzt, weitere fügen hinzu; Entf Standard, START 2 NONE, Links/Rechts NONE/Standard; Taste auf mehreren Funktionen erlaubt.
- Am Automaten noch nicht von Hand ausprobiert (alle Spiele installiert, alte Programme als `*.before-vpbind` daneben).
