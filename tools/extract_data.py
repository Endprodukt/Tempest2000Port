#!/usr/bin/env python3
"""extract_data.py - pull the game data out of the Tempest 2000 assembly.

Reads reference/tempest2k/src/yak.s and obj2d.s and writes src/gen/t2k_data.c:
  - the 37 webs (lane vertices, start lane, closed flag, orientation tables)
  - the web and the web colour of each of Tempest 2000's 100 levels
  - Tempest Classic's vector objects (claws, shot, ...) in make_vo3d's format
  - Tempest 2000's solid objects (obj2d.s), as the outlines of their faces
  - the enemy waves of the 100 levels and the per-level speed tables
  - the messages' font from the game's picture sheet, as rows of pixels

Nothing is typed by hand: the port draws what the original data says.

    python tools/extract_data.py
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "reference", "tempest2k", "src", "yak.s")
SRC_OBJ2D = os.path.join(ROOT, "reference", "tempest2k", "src", "obj2d.s")
OUT = os.path.join(ROOT, "src", "gen", "t2k_data.c")

LABEL = re.compile(r"^([A-Za-z_][A-Za-z0-9_]*):\s*(.*)$")
SUBLABEL = re.compile(r"^w\d+col$")
DATA = re.compile(r"^\s*dc\.([bwl])\s+(.*)$", re.I)

# Vector objects built by make_bits / make_claws, with the local centre the
# original passes to make_vo3d (d3, d4, d5).
VECTOR_OBJECTS = [
    ("claw1", 9, 9, 0), ("claw2", 9, 9, 0), ("claw3", 9, 9, 0), ("claw4", 9, 9, 0),
    ("claw5", 9, 9, 0), ("claw6", 9, 9, 0), ("claw7", 9, 9, 0), ("claw8", 9, 9, 0),
    ("shot", 9, 9, 0), ("chevre", 9, 9, 0), ("flipper", 9, 9, 0), ("zap", 9, 9, 0),
    ("fliptank", 9, 9, 0), ("fusetank", 9, 9, 0), ("pulstank", 9, 9, 0),
    ("spike", 9, 9, 0), ("spiker", 9, 9, 0), ("fuse1", 9, 9, 0), ("fuse2", 9, 9, 0),
    ("b250", 9, 9, 0), ("b500", 9, 9, 0), ("b750", 9, 9, 0), ("oneup", 9, 9, 0),
    ("pu1", 9, 9, 0), ("pu2", 9, 9, 0), ("pu3", 9, 9, 0),
    ("pu4", 9, 9, 0), ("pu5", 9, 9, 0), ("pu6", 9, 9, 0),
    ("cube", 2, 2, 2),
]


# The solid polygon objects of Tempest 2000 mode (obj2d.s), drawn by polyo2d
# around the centre (9, 9): drawsolid / zqz.
SOLID_OBJECTS = (
    ["s_flipper", "s_fliptank", "s_fliptank2", "fbpiece1", "fbpiece2"]
    + [f"spuls{i}" for i in range(1, 7)]
    + ["chevron", "chev", "pshot", "pwrlaser", "g1", "g2", "arr", "dchev", "epyr",
       "xbit", "mirr", "h2hshot1", "h2hshot2", "leaf", "blueflipper",
       "s_flip1", "s_flip2", "hornm1", "hornm2", "hornm3", "adroid"]
    + [f"sclaw{i}" for i in range(8)]
    + [f"gsclaw{i}" for i in range(8)]
)

# Tempest 2000 mode: 100 levels over raw_webs 16..47 ('webbase' $10,
# 'weband' $1f in sweb), a web colour for every 16 levels (webcols).
T2K_LEVELS = 100
T2K_WEB_BASE = 16
T2K_WEB_AND = 31


def strip_comment(text):
    return text.split(";", 1)[0].strip()


def number(tok):
    tok = tok.strip()
    neg = tok.startswith("-")
    if neg:
        tok = tok[1:]
    val = int(tok[1:], 16) if tok.startswith("$") else int(tok)
    return -val if neg else val


def load_blocks(path):
    """label -> list of (size, [tokens]) for the data lines that follow it."""
    blocks = {}
    current = None
    with open(path, encoding="latin-1") as f:
        for raw in f:
            line = strip_comment(raw)
            if not line:
                continue
            m = LABEL.match(line)
            if m:
                # 'w16col:' sits inside web16's data: it does not end the web.
                if not (current and SUBLABEL.match(m.group(1))):
                    current = m.group(1)
                    blocks[current] = []
                line = m.group(2).strip()
                if not line:
                    continue
            d = DATA.match(line)
            if d and current:
                toks = [t for t in (x.strip() for x in d.group(2).split(",")) if t]
                blocks[current].append((d.group(1).lower(), toks))
            else:
                current = None      # code or another directive ends the block
    return blocks


def numbers(blocks, label, size):
    out = []
    for sz, toks in blocks[label]:
        if sz != size:
            raise SystemExit(f"{label}: expected dc.{size}, found dc.{sz}")
        out.extend(number(t) for t in toks)
    return out


def parse_web(blocks, label):
    n = numbers(blocks, label, "w")
    channels, start = n[0], n[1]
    pos = 2
    pts = [(n[pos + 2 * i], n[pos + 2 * i + 1]) for i in range(channels + 1)]
    pos += 2 * (channels + 1)
    pos += 2                                # the first vertex, repeated
    closed = 1 if n[pos] != 0 else 0        # extrude: 'move 4(a1),connect'
    pos += 1
    rest = n[pos:]
    lanes = channels + 1 if closed else channels
    if closed:
        pts.append(pts[0])
    if len(rest) < lanes:
        raise SystemExit(f"{label}: orientation table shorter than the web")
    # The second table is not always as long as the first (web8): pad it.
    tab2 = rest[lanes:2 * lanes]
    tab2 += [0] * (lanes - len(tab2))
    return {
        "label": label, "lanes": lanes, "start": start, "closed": closed,
        "pts": pts, "roll": rest[:lanes], "tab2": tab2,
    }


def parse_vector_object(blocks, label):
    """make_vo3d: x,y,z then the vertices this one connects to, 0 ends the
    list; a negative byte sets a colour (-1,colour,vertex); x = 0 ends."""
    b = numbers(blocks, label, "b")
    verts, lines = [], []
    i, colour = 0, 0
    while i < len(b) and b[i] != 0:
        verts.append((b[i] & 0xFF, b[i + 1] & 0xFF, b[i + 2] & 0xFF))
        i += 3
        this = len(verts)                   # vertex numbers are 1-based
        if b[i] == 0:
            i += 1
            continue
        while True:
            v = b[i]
            i += 1
            if v < 0:
                colour = b[i] & 0xFF
                v = b[i + 1]
                i += 2
            v &= 0xFF
            if v == 0:
                break
            lines.append((this, v, colour))
    return verts, lines


SHEET_WIDTH = 320       # the .cry picture sheets: 16 bits a pixel, colour then brightness
LIT = 0x50              # a pixel of a letter; the dark rim around the letters is $4x


def load_sheet(name):
    with open(os.path.join(os.path.dirname(SRC), "images", name), "rb") as f:
        return f.read()


def bitmap_runs(sheet, y, x, h, w, lit=LIT):
    """The lit pixels of a rectangle of a sheet, as runs along its rows:
    (row, first column, column past the last, colour of the middle)."""
    runs = []
    for r in range(h):
        c = 0
        while c < w:
            o = ((y + r) * SHEET_WIDTH + x + c) * 2
            if o + 1 >= len(sheet) or sheet[o + 1] < lit:
                c += 1
                continue
            start = c
            while c < w and sheet[((y + r) * SHEET_WIDTH + x + c) * 2 + 1] >= lit:
                c += 1
            mid = ((y + r) * SHEET_WIDTH + x + (start + c) // 2) * 2
            runs.append((r, start, c, sheet[mid]))
    return runs


def emit_bitmap(o, name, sheet, y, x, h, w, lit=LIT):
    runs = bitmap_runs(sheet, y, x, h, w, lit)
    o.append(f"static const unsigned short {name}_r[] = {{"
             + (",".join(f"{a},{b},{c},{d}" for a, b, c, d in runs) or "0") + "};")
    return f"{{{w}, {h}, {len(runs)}, {name}_r}}"


def load_words(path):
    """A file of dc.w / dc.l data as one stream of words, and where each
    label is in it. polyo2d finds an object's vertices by their distance from
    its faces, not by a label, so labels do not split the stream."""
    words, labels = [], {}
    with open(path, encoding="latin-1") as f:
        for raw in f:
            line = strip_comment(raw)
            m = LABEL.match(line)
            if m:
                labels[m.group(1)] = len(words)
                line = m.group(2).strip()
            d = DATA.match(line)
            if not d:
                continue
            for tok in (t.strip() for t in d.group(2).split(",")):
                if not tok:
                    continue
                v = number(tok)
                if d.group(1).lower() == "l":
                    words.extend([(v >> 16) & 0xFFFF, v & 0xFFFF])
                else:
                    words.append(v)
    return words, labels


def parse_solid(words, labels, label):
    """dc.l faces; per face: colour, 3 x (vertex, intensity), 0; then the
    vertices as x, y. Returns the vertices and the faces' edges, each edge
    once, ordered so the beam can draw them end to end where they meet."""
    off = labels[label]
    faces = words[off + 1]
    base = off + 2
    tris = []
    for i in range(faces):
        f = words[base + 8 * i: base + 8 * i + 8]
        if f[7] != 0:
            raise SystemExit(f"{label}: face {i} is not a triangle")
        tris.append((f[1], f[3], f[5], f[0] & 0xFF))
    nverts = max(max(t[:3]) for t in tris) + 1
    vbase = base + 8 * faces
    verts = [(words[vbase + 2 * i], words[vbase + 2 * i + 1]) for i in range(nverts)]

    edges, seen = [], set()
    for a, b, c, col in tris:
        for p, q in ((a, b), (b, c), (c, a)):
            key = (min(p, q), max(p, q))
            if p != q and key not in seen:
                seen.add(key)
                edges.append((p, q, col))

    ordered, pen = [], None
    while edges:
        nxt = next((e for e in edges if pen in (e[0], e[1])), edges[0])
        edges.remove(nxt)
        a, b, col = nxt
        if b == pen:
            a, b = b, a
        ordered.append((a, b, col))
        pen = b
    return verts, ordered


def main():
    if not os.path.exists(SRC):
        raise SystemExit(f"missing {SRC}")
    blocks = load_blocks(SRC)

    web_labels = sorted((k for k in blocks if re.fullmatch(r"web\d+", k)),
                        key=lambda s: int(s[3:]))
    webs = [parse_web(blocks, w) for w in web_labels]
    index = {w["label"]: i for i, w in enumerate(webs)}

    raw = []
    for _, toks in blocks["raw_webs"]:
        raw.extend(t for t in toks if t != "0")
    order = [raw[T2K_WEB_BASE + (lvl & T2K_WEB_AND)] for lvl in range(T2K_LEVELS)]
    # The eighth group of 16 levels reads one word past webcols: flipcols[0].
    webcols = numbers(blocks, "webcols", "w") + numbers(blocks, "flipcols", "w")[:1]

    o = []
    o.append("/* Generated by tools/extract_data.py from reference/tempest2k/src/yak.s.")
    o.append(" * Do not edit: change the tool and run it again. */")
    o.append('#include "t2k_data.h"')
    o.append("")
    for w in webs:
        name = w["label"]
        o.append(f"static const short {name}_pts[] = {{"
                 + ",".join(f"{x},{y}" for x, y in w["pts"]) + "};")
        o.append(f"static const short {name}_roll[] = {{" + ",".join(map(str, w["roll"])) + "};")
        o.append(f"static const short {name}_tab2[] = {{" + ",".join(map(str, w["tab2"])) + "};")
    o.append("")
    o.append(f"const t2k_web t2k_webs[{len(webs)}] = {{")
    for w in webs:
        n = w["label"]
        o.append(f"    {{{w['lanes']}, {w['start']}, {w['closed']}, {n}_pts, {n}_roll, {n}_tab2}},")
    o.append("};")
    o.append("")
    o.append(f"const unsigned char t2k_level_web[{len(order)}] = {{"
             + ",".join(str(index[w]) for w in order) + "};")
    o.append(f"const int t2k_level_count = {len(order)};")
    o.append("")

    names = []
    for label, cx, cy, cz in VECTOR_OBJECTS:
        if label not in blocks:
            print(f"warning: no vector object '{label}'", file=sys.stderr)
            continue
        verts, lines = parse_vector_object(blocks, label)
        o.append(f"static const signed char {label}_v[] = {{"
                 + ",".join(f"{x - cx},{y - cy},{z - cz}" for x, y, z in verts) + "};")
        o.append(f"static const unsigned char {label}_l[] = {{"
                 + ",".join(f"{a - 1},{b - 1},{c}" for a, b, c in lines) + "};")
        o.append(f"const t2k_vobj t2k_{label} = {{{len(verts)}, {len(lines)}, {label}_v, {label}_l}};")
        names.append(label)
    o.append("")
    o.append("const t2k_vobj *const t2k_claws[8] = {"
             + ",".join(f"&t2k_claw{i}" for i in range(1, 9)) + "};")
    o.append("")

    # Enemy waves: duration, then (max time, type) for the flippers and
    # (0, max time, type) for every other enemy, -1 ends (setnewgen).
    wave_labels = []
    for _, toks in blocks["waves"]:
        wave_labels.extend(t for t in toks if t != "0")
    wave_labels = wave_labels[:T2K_LEVELS]
    if len(wave_labels) < T2K_LEVELS:
        raise SystemExit(f"only {len(wave_labels)} waves")
    for i, label in enumerate(wave_labels):
        # '_nw99:' has no data of its own: it runs into '_nw100:'.
        while not blocks[label]:
            label = "_nw%d" % (int(label[3:]) + 1)
        n = numbers(blocks, label, "w")
        flat = [n[0], n[1], n[2]]
        pos = 3
        while n[pos] >= 0:
            flat += [n[pos + 1], n[pos + 2]]
            pos += 3
        o.append(f"static const short wave{i}[] = {{" + ",".join(map(str, flat)) + ",-1};")
    o.append(f"const short *const t2k_waves[{T2K_LEVELS}] = {{"
             + ",".join(f"wave{i}" for i in range(T2K_LEVELS)) + "};")
    # Traditional and Plus: past level 48 the last 16 waves come round again.
    trad = []
    for _, toks in blocks["tradmax"]:
        trad.extend(t for t in toks if t != "0")
    for i, label in enumerate(trad[:16]):
        n = numbers(blocks, label, "w")
        flat = [n[0], n[1], n[2]]
        pos = 3
        while n[pos] >= 0:
            flat += [n[pos + 1], n[pos + 2]]
            pos += 3
        o.append(f"static const short tradwave{i}[] = {{" + ",".join(map(str, flat)) + ",-1};")
    o.append("const short *const t2k_tradmax[16] = {"
             + ",".join(f"tradwave{i}" for i in range(16)) + "};")
    o.append(f"const unsigned char t2k_raw_webs[{len(raw)}] = {{"
             + ",".join(str(index[w]) for w in raw) + "};")
    o.append(f"const unsigned char t2k_webcols[{len(webcols)}] = {{"
             + ",".join(map(str, webcols)) + "};")
    o.append("")
    for label, name in (("sz_stuff", "spiker"), ("fz_stuff", "flipper"),
                        ("tz_stuff", "tanker"), ("fuz_stuff", "fuseball")):
        v = numbers(blocks, label, "l")
        o.append(f"const int t2k_{name}_zspeed[{len(v)}] = {{" + ",".join(map(str, v)) + "};")
    v = numbers(blocks, "pudels", "w")
    o.append(f"const unsigned short t2k_pudels[{len(v)}] = {{" + ",".join(map(str, v)) + "};")
    v = numbers(blocks, "flipcols", "w")
    o.append(f"const unsigned char t2k_flipcols[{len(v)}] = {{" + ",".join(map(str, v)) + "};")
    v = numbers(blocks, "fbcols", "b")
    o.append(f"const unsigned char t2k_fbcols[{len(v)}] = {{" + ",".join(map(str, v)) + "};")
    v = numbers(blocks, "pup_stuff", "w")
    o.append(f"const unsigned short t2k_pup_stuff[{len(v)}] = {{" + ",".join(map(str, v)) + "};")
    v = numbers(blocks, "webtunes", "b")
    o.append(f"const unsigned char t2k_webtunes[{len(v)}] = {{" + ",".join(map(str, v)) + "};")
    with open(os.path.join(os.path.dirname(SRC), "incbin", "sound_samples_table.bin"), "rb") as f:
        table = f.read()
    v = [table[i * 40 + 22] * 256 + table[i * 40 + 23] for i in range(30)]
    o.append(f"const unsigned short t2k_sfx_period[{len(v)}] = {{" + ",".join(map(str, v)) + "};")
    v = numbers(blocks, "scores", "b")
    o.append(f"const unsigned char t2k_scores[{len(v)}] = {{" + ",".join(map(str, v)) + "};")
    o.append("")

    # The messages' font ('afont'): 96 characters from the space on, each a
    # place in a picture sheet. Letters are 2 pixels apart (texter). The other
    # two fonts and the game's pictures are not used: the port writes with
    # the SDK's stroke font.
    def emit_font(name, source, sheet):
        with open(os.path.join(os.path.dirname(SRC), source), encoding="latin-1") as f:
            v = [int(m, 16) for m in re.findall(r"dc\.l\s+\$([0-9a-fA-F]+)", f.read())]
        h, w = v[0] >> 16, v[0] & 0xFFFF
        chars = (v[1:97] + [v[1]] * 96)[:96]
        glyphs = [emit_bitmap(o, f"{name}{i}", sheet, c >> 16, c & 0xFFFF, h, w)
                  for i, c in enumerate(chars)]
        o.append(f"static const t2k_bitmap {name}_glyphs[96] = {{" + ",".join(glyphs) + "};")
        o.append(f"const t2k_font t2k_{name} = {{{w}, {h}, {w + 2}, {name}_glyphs}};")

    emit_font("afont", "afont.s", load_sheet("beasty4.cry"))
    o.append("")

    words, labels = load_words(SRC_OBJ2D)
    solids = []
    for label in SOLID_OBJECTS:
        verts, edges = parse_solid(words, labels, label)
        o.append(f"static const signed char {label}_v[] = {{"
                 + ",".join(f"{x - 9},{y - 9},0" for x, y in verts) + "};")
        o.append(f"static const unsigned char {label}_l[] = {{"
                 + ",".join(f"{a},{b},{c}" for a, b, c in edges) + "};")
        o.append(f"const t2k_vobj t2k_{label} = {{{len(verts)}, {len(edges)}, {label}_v, {label}_l}};")
        solids.append(label)
    o.append("")
    for name in ("sclaw", "gsclaw"):
        o.append(f"const t2k_vobj *const t2k_{name}s[8] = {{"
                 + ",".join(f"&t2k_{name}{i}" for i in range(8)) + "};")
    o.append("")

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", newline="\n") as f:
        f.write("\n".join(o))
    print(f"{len(webs)} webs, {len(order)} levels, {len(names)} vector objects -> {OUT}")
    print("objects: " + " ".join(names))
    print(f"{len(solids)} solid objects: " + " ".join(solids))


if __name__ == "__main__":
    main()
