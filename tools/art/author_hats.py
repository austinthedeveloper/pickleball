#!/usr/bin/env python3
"""Authoring tool: opponent hats (M4-OPP-04) into objects.png.

NOT part of the normal build; run by hand when the hats change. Standard
library only. Stamps each hat's two 8x8 tiles into its objects.png cells
(assets/manifest/player_frames.json "hats": the left cell; the right is the
next one) and writes objects.png back.

A hat is 16x8 in the owner's palette (ADR-013 contract v2), drawn in front
of its body at the frame's accessory offset plus the manifest's "hat_offset"
(facing right). It can only add pixels, so each hat covers the cap the body
already wears: COVER below lists the cap's pixels in hat coordinates, and
the tool refuses a hat that leaves any of them showing.

Letters: O outline (1), H hair (15), w highlight (14, the tricorn's
trim), V shoe (10, the visor and headband colour), d T L shirt
dark/mid/light (5-7).

Usage: python tools/art/author_hats.py
       python tools/art/author_hats.py --preview OUT.png TEAM PLAYER
"""
import json
import re
import struct
import sys
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from assetgen.common import read_png  # noqa: E402

ROLE = {".": 0, "O": 1, "d": 5, "T": 6, "L": 7, "V": 10, "w": 14,
        "H": 15}

HATS = {
    # Hair crown, an outline under it, a band and brim in the shoe colour.
    "visor": [
        "................",
        "................",
        "....OOOOO.......",
        "...OHHHHHO......",
        "..OHHHHHHHO.....",
        "..OOOOOOOOOOOO..",
        "..OVVVVVVVVVVVO.",
        ".........OOOOO..",
    ],
    # Hair swept forward over a band in the shoe colour.
    "headband": [
        "................",
        "................",
        "....OOOOO.......",
        "...OHHHHHOOO....",
        "..OHHHHHHHHHO...",
        "..OHHHHHHHHHHHO.",
        "..OVVVVVVVVVVO..",
        "................",
    ],
    # Upturned brim front and back, trimmed in white.
    "tricorn": [
        "................",
        ".....OOOOO......",
        "....OHHHHHO.....",
        "O..OHHHHHHHO..O.",
        "OwOHHHHHHHHHOwO.",
        ".OwwHHHHHHHHwwO.",
        "..OwwwwwwwwwwwO.",
        "................",
    ],
    # Shirt-coloured wrap, dotted, knot tails at the back.
    "bandana": [
        "................",
        "................",
        "....OOOOO.......",
        "...OTTLTTO......",
        ".OOTTTTTTTO.....",
        "OdOTLTTTLTTOO...",
        "OddOTTTTTTTTTO..",
        ".OO.............",
    ],
    # No cap: the hair, a fringe swept forward over the brow.
    "hair": [
        "................",
        "................",
        "....OOOOO.......",
        "...OHHHHHOO.....",
        "..OHHHHHHHHO....",
        "..OHHHHHHHHHHO..",
        "..OHHHHHHHHHHO..",
        "................",
    ],
}

# The cap's pixels (rows 2-6 of the hat box) that a hat must cover.
COVER = {2: range(4, 9), 3: range(3, 10), 4: range(2, 11), 5: range(2, 13),
         6: range(2, 14)}


def write_png(path, rows, palette):
    def chunk(kind, body):
        return (struct.pack(">I", len(body)) + kind + body +
                struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF))
    height, width = len(rows), len(rows[0])
    raw = b"".join(b"\0" + bytes(row) for row in rows)
    plte = b"".join(bytes(c) for c in palette)
    path.write_bytes(
        b"\x89PNG\r\n\x1a\n" +
        chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 3, 0, 0, 0)) +
        chunk(b"PLTE", plte) + chunk(b"IDAT", zlib.compress(raw, 9)) +
        chunk(b"IEND", b""))


def hat_pixels(name):
    art = HATS[name]
    if len(art) != 8 or any(len(row) != 16 for row in art):
        raise SystemExit(f"hat {name} must be 16x8")
    pixels = [[ROLE[c] for c in row] for row in art]
    for y, xs in COVER.items():
        if any(pixels[y][x] == 0 for x in xs):
            raise SystemExit(f"hat {name} leaves the cap showing on row {y}")
    return pixels


def manifest():
    return json.loads((ROOT / "assets" / "manifest" /
                       "player_frames.json").read_text("utf-8"))


def stamp(objects, cell, pixels):
    for y in range(8):
        for x in range(16):
            cx = (cell + x // 8) % 16 * 8 + x % 8
            cy = (cell + x // 8) // 16 * 8 + y
            objects[cy][cx] = pixels[y][x]


def opponent_palette(team, player):
    """A player's 16 colours from the generated opponents.h."""
    text = (ROOT / "assets" / "generated" / "opponents.h").read_text()
    names = re.search(r"g_opponent_team\[[^]]*\] = \{([^}]*)\}", text)
    teams = re.findall(r'"([^"]*)"', names.group(1))
    body = text[text.index("g_opponent_palettes"):]
    body = body[body.index("{"):body.index("};")]
    values = [int(v, 16) for v in re.findall(r"0x([0-9a-fA-F]+)", body)]
    first = teams.index(team) * 32 + player * 16
    return [((c & 31) * 255 // 31, ((c >> 5) & 31) * 255 // 31,
             ((c >> 10) & 31) * 255 // 31) for c in values[first:first + 16]]


def preview(out, team, player):
    """Every frame, facing right then left, with each hat in turn: rows of
    frames, one row per hat (and the plain cap first)."""
    m = manifest()
    _, _, sheet, _ = read_png(ROOT / "assets" / "source" /
                              "player_sprites.png")
    _, _, objects, _ = read_png(ROOT / "assets" / "source" / "objects.png")
    dx, dy = m["hat_offset"]
    frames = list(m["frames"].values())
    names = [None] + list(HATS)
    pony = m["objects"]["ponytail"]
    spec = json.loads((ROOT / "assets" / "source" / "opponents.json")
                      .read_text("utf-8"))
    team_spec = next(o for o in spec["opponents"] if o["team"] == team)
    female = team_spec["players"][player]["variant"] == "female"
    width = len(frames) * 2 * 34
    canvas = [[16] * width for _ in range(len(names) * 34)]
    for row, name in enumerate(names):
        hat = hat_pixels(name) if name else None
        for i, info in enumerate(frames):
            sx, sy = (info["slot"] % 4) * 32, (info["slot"] // 4) * 32
            ax, ay = info["accessory"]
            for flip in (0, 1):
                ox, oy = (i * 2 + flip) * 34 + 1, row * 34 + 1

                def put(x, y, v):
                    if v and 0 <= x < 32 and 0 <= y < 32:
                        canvas[oy + y][ox + x] = v
                # Ponytail behind the body (female), as render_bodies.
                if female:
                    px = 24 - ax if flip else ax
                    for y in range(8):
                        for x in range(8):
                            v = objects[pony // 16 * 8 + y][pony % 16 * 8 + x]
                            put(px + (7 - x if flip else x), ay + y, v)
                for y in range(32):
                    for x in range(32):
                        put(31 - x if flip else x, y, sheet[sy + y][sx + x])
                if hat:
                    hx = 16 - (ax + dx) if flip else ax + dx
                    for y in range(8):
                        for x in range(16):
                            put(hx + (15 - x if flip else x), ay + dy + y,
                                hat[y][x])
    colours = opponent_palette(team, player) + [(40, 120, 60)]
    colours[0] = (0, 0, 0)
    scale = 3
    big = [[v for v in r for _ in range(scale)]
           for r in canvas for _ in range(scale)]
    write_png(Path(out), big, colours)


def main():
    if len(sys.argv) == 5 and sys.argv[1] == "--preview":
        preview(sys.argv[2], sys.argv[3], int(sys.argv[4]))
        return
    m = manifest()
    path = ROOT / "assets" / "source" / "objects.png"
    _, _, objects, palette = read_png(path)
    objects = [list(r) for r in objects]
    if set(m["hats"]) != set(HATS):
        raise SystemExit("manifest hats and HATS differ")
    for name, cell in m["hats"].items():
        stamp(objects, cell, hat_pixels(name))
    write_png(path, objects, palette)
    print(f"wrote {path.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
