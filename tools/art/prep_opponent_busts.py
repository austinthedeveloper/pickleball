#!/usr/bin/env python3
"""Authoring tool: the intro card's opponent busts (M4-STORY-03).

NOT part of the normal build; run by hand when the select portraits change.
Standard library only. Writes one committed indexed PNG:

  assets/source/opponent_busts.png  96x48: male and female busts as 48x48
                                    framed cells in player palette roles
                                    (ADR-013 contract v2), recoloured at run
                                    time with the captain's palette through
                                    BG palette 1.

The busts are cut from select_portraits.png (head and shoulders) and their
roles cleaned so any opponent's colours read right:

- hair (15) is hair only; the portraits also shade the face and arms with
  it, which becomes the dark skin tone (2);
- the cap logo's whites (8, 9: the shorts roles) become the highlight (14);
- the shirt's dark (11: a paddle-face role) becomes the shirt's dark (5);
- the paddle is cut away.

The frame uses two roles a bust never shows: 10 (shoe) is the inner border
and 13 (paddle edge) the backdrop; the game sets both colours at upload, and
the outline (1) is the outer border.

Usage: python tools/art/prep_opponent_busts.py [--preview OUT.png OPPONENT]
"""
import re
import struct
import sys
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from assetgen.common import read_png  # noqa: E402

CELL = 48
CONTENT = 4             # the figure starts inside the 2 px frame plus 2 px
OUTLINE, SKIN_DARK, SHIRT_DARK, HIGHLIGHT, HAIR = 1, 2, 5, 14, 15
FRAME_INNER, BACKDROP = 10, 13
# Per variant: the crop's top-left in select_portraits.png, its size, the
# hair box (source x0, y0, x1, y1; exclusive), and the paddle cut (source x,
# y: everything right of x and below y is paddle).
VARIANTS = [
    {"crop": (26, 11), "size": (40, 42), "hair": (40, 26, 50, 33),
     "paddle": (62, 42)},
    {"crop": (124, 14), "size": (40, 42), "hair": (130, 28, 143, 46),
     "paddle": (158, 43)},
]
# Frame colours (BGR555 parts), as the game uploads them: the HUD's border
# grey and a muted blue-grey that the navy outline reads against.
FRAME_INNER_RGB5 = (21, 22, 26)
BACKDROP_RGB5 = (13, 16, 21)


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


def remap(value, x, y, spec):
    hx0, hy0, hx1, hy1 = spec["hair"]
    px, py = spec["paddle"]
    if x >= px and y >= py:
        return 0
    if value == HAIR and not (hx0 <= x < hx1 and hy0 <= y < hy1):
        return SKIN_DARK
    if value in (8, 9):
        return HIGHLIGHT
    if value == 11:
        return SHIRT_DARK
    if value in (10, 12, 13):
        return 0
    return value


def busts():
    width, height, rows, palette = read_png(
        ROOT / "assets" / "source" / "select_portraits.png")
    out = [[0] * (CELL * 2) for _ in range(CELL)]
    for variant, spec in enumerate(VARIANTS):
        x0, y0 = spec["crop"]
        w, h = spec["size"]
        base = variant * CELL
        for y in range(CELL):
            for x in range(CELL):
                ring = min(x, y, CELL - 1 - x, CELL - 1 - y)
                if ring == 0:
                    value = OUTLINE
                elif ring == 1:
                    value = FRAME_INNER
                else:
                    value = BACKDROP
                    sx, sy = x - CONTENT + x0, y - CONTENT + y0
                    if 0 <= x - CONTENT < w and 0 <= y - CONTENT < h:
                        figure = remap(rows[sy][sx], sx, sy, spec)
                        if figure:
                            value = figure
                out[y][base + x] = value
    pal = list(palette[:16])
    pal[FRAME_INNER] = tuple(c * 255 // 31 for c in FRAME_INNER_RGB5)
    pal[BACKDROP] = tuple(c * 255 // 31 for c in BACKDROP_RGB5)
    for i in (8, 9, 11, 12):
        pal[i] = (255, 0, 255)          # roles a bust must not use
    return out, pal


def opponent_palette(team):
    """The captain's 16 colours from the generated opponents.h."""
    text = (ROOT / "assets" / "generated" / "opponents.h").read_text()
    names = re.search(r"g_opponent_team\[[^]]*\] = \{([^}]*)\}", text)
    teams = re.findall(r'"([^"]*)"', names.group(1))
    body = text[text.index("g_opponent_palettes"):]
    body = body[body.index("{"):body.index("};")]
    values = [int(v, 16) for v in re.findall(r"0x([0-9a-fA-F]+)", body)]
    first = teams.index(team) * 32
    return [((c & 31) * 255 // 31, ((c >> 5) & 31) * 255 // 31,
             ((c >> 10) & 31) * 255 // 31) for c in values[first:first + 16]]


def main():
    out, pal = busts()
    target = ROOT / "assets" / "source" / "opponent_busts.png"
    if len(sys.argv) == 4 and sys.argv[1] == "--preview":
        colours = opponent_palette(sys.argv[3])
        colours[FRAME_INNER] = pal[FRAME_INNER]
        colours[BACKDROP] = pal[BACKDROP]
        colours[0] = (40, 120, 60)
        scale = 4
        big = [[v for v in row for _ in range(scale)]
               for row in out for _ in range(scale)]
        write_png(Path(sys.argv[2]), big, colours)
        return
    write_png(target, out, pal)
    print(f"wrote {target.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
