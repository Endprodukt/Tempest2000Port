#!/bin/sh
# make_vpk.sh - the game as a VectorPie package.
#
#   sh tools/make_vpk.sh        build/tempest2000-<version>.vpk      (full edition)
#   sh tools/make_vpk.sh rom    build/tempest2000-<version>-rom.vpk  (release edition)
#
# Run from the project folder after the Pi build (BUILD.md): build/pi for the
# full edition, which also needs tools/extract_audio.py; build/pi-rom for the
# release edition. The full edition holds the Pi program, game.ini, the sounds
# and tunes and the marquee. The release edition holds no sound and no tune:
# it takes them, and the game's tables, from the player's Jaguar ROM (src/rom.h).
# The cabinet's menu installs a package from a USB drive (Settings -> GAMES ->
# INSTALL FROM USB).
set -eu
root=$(cd "$(dirname "$0")/.." && pwd)
edition=${1:-full}
case "$edition" in
    full) build="$root/build/pi";     stage="$root/build/pkg/tempest2000" ;;
    rom)  build="$root/build/pi-rom"; stage="$root/build/pkg-rom/tempest2000" ;;
    *)    echo "make_vpk: full or rom, not $edition" >&2; exit 1 ;;
esac
[ -f "$build/tempest2000" ] || { echo "make_vpk: no Pi build in $build" >&2; exit 1; }
rm -rf "$(dirname "$stage")"
mkdir -p "$stage"
cp "$build/tempest2000" "$root/package/game.ini" "$root/package/marquee.png" "$stage/"
if [ "$edition" = full ]; then
    cp -r "$root/assets" "$stage/assets"
else
    cp "$root/package/README-rom.txt" "$stage/README.txt"
    cat >> "$stage/game.ini" <<'INI'

; This edition holds none of Atari's data: it needs the Tempest 2000 Jaguar
; ROM (2 MB, .j64 .jag .rom or .bin, with or without the 8 KB header). Put it
; into /persistent/games/rom (looked at first), the folder save next to this
; file, or next to this file, and start the game; without it the game says
; what is missing.
INI
fi
version=$(sed -n 's/^version[[:space:]]*=[[:space:]]*\([0-9.]*\).*/\1/p' "$stage/game.ini" | head -n 1)
out=$(mktemp -d)
sh "$root/vendor/VectorPie/sdk/tools/vp-pack/vp-pack.sh" "$stage" "$out"
vpk="$root/build/tempest2000-$version.vpk"
[ "$edition" = rom ] && vpk="$root/build/tempest2000-$version-rom.vpk"
mv -f "$out/tempest2000-$version.vpk" "$vpk"
rmdir "$out"

# Built on Windows the program in the archive is not marked as one: the file
# system there has no such mark for vp-pack's chmod to set. Mark it here.
python - "$vpk" <<'PY'
import io, sys, tarfile
path = sys.argv[1]
out = io.BytesIO()
with tarfile.open(path, "r:gz") as old, tarfile.open(fileobj=out, mode="w:gz") as new:
    for member in old.getmembers():
        if member.name == "tempest2000/tempest2000":
            member.mode = 0o755
        new.addfile(member, old.extractfile(member) if member.isfile() else None)
open(path, "wb").write(out.getvalue())
PY
echo "make_vpk: $vpk"
