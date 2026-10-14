"""BG3 HUD: chunky font, panel borders, buttons, big title glyphs."""
import struct

from .common import HUD_TILE_BUDGET, AssetError, bgr15, encode_tile

FONT = {
    "A": [".####.", "##..##", "##..##", "######", "##..##", "##..##", "##..##"],
    "B": ["#####.", "##..##", "##..##", "#####.", "##..##", "##..##", "#####."],
    "C": [".####.", "##..##", "##....", "##....", "##....", "##..##", ".####."],
    "D": ["####..", "##.##.", "##..##", "##..##", "##..##", "##.##.", "####.."],
    "E": ["######", "##....", "##....", "#####.", "##....", "##....", "######"],
    "F": ["######", "##....", "##....", "#####.", "##....", "##....", "##...."],
    "G": [".####.", "##..##", "##....", "##.###", "##..##", "##..##", ".#####"],
    "H": ["##..##", "##..##", "##..##", "######", "##..##", "##..##", "##..##"],
    "I": ["######", "..##..", "..##..", "..##..", "..##..", "..##..", "######"],
    "J": ["...###", "....##", "....##", "....##", "##..##", "##..##", ".####."],
    "K": ["##..##", "##.##.", "####..", "###...", "####..", "##.##.", "##..##"],
    "L": ["##....", "##....", "##....", "##....", "##....", "##....", "######"],
    "M": ["#....#", "##..##", "######", "######", "##..##", "##..##", "##..##"],
    "N": ["##..##", "###.##", "######", "##.###", "##..##", "##..##", "##..##"],
    "O": [".####.", "##..##", "##..##", "##..##", "##..##", "##..##", ".####."],
    "P": ["#####.", "##..##", "##..##", "#####.", "##....", "##....", "##...."],
    "Q": [".####.", "##..##", "##..##", "##..##", "##.###", "##..##", ".###.#"],
    "R": ["#####.", "##..##", "##..##", "#####.", "####..", "##.##.", "##..##"],
    "S": [".####.", "##..##", "##....", ".####.", "....##", "##..##", ".####."],
    "T": ["######", "..##..", "..##..", "..##..", "..##..", "..##..", "..##.."],
    "U": ["##..##", "##..##", "##..##", "##..##", "##..##", "##..##", ".####."],
    "V": ["##..##", "##..##", "##..##", "##..##", "##..##", ".####.", "..##.."],
    "W": ["##..##", "##..##", "##..##", "######", "######", "##..##", "#....#"],
    "X": ["##..##", "##..##", ".####.", "..##..", ".####.", "##..##", "##..##"],
    "Y": ["##..##", "##..##", "##..##", ".####.", "..##..", "..##..", "..##.."],
    "Z": ["######", "....##", "...##.", "..##..", ".##...", "##....", "######"],
    "0": [".####.", "##..##", "##.###", "######", "###.##", "##..##", ".####."],
    "1": ["..##..", ".###..", "..##..", "..##..", "..##..", "..##..", ".####."],
    "2": [".####.", "##..##", "....##", "..###.", ".##...", "##....", "######"],
    "3": [".####.", "##..##", "....##", "..###.", "....##", "##..##", ".####."],
    "4": ["...##.", "..###.", ".####.", "##.##.", "######", "...##.", "...##."],
    "5": ["######", "##....", "#####.", "....##", "....##", "##..##", ".####."],
    "6": [".####.", "##....", "##....", "#####.", "##..##", "##..##", ".####."],
    "7": ["######", "....##", "...##.", "..##..", "..##..", "..##..", "..##.."],
    "8": [".####.", "##..##", "##..##", ".####.", "##..##", "##..##", ".####."],
    "9": [".####.", "##..##", "##..##", ".#####", "....##", "....##", ".####."],
    "-": ["......", "......", "......", "######", "......", "......", "......"],
    "+": ["......", "..##..", "..##..", "######", "..##..", "..##..", "......"],
    "!": ["..##..", "..##..", "..##..", "..##..", "..##..", "......", "..##.."],
    ".": ["......", "......", "......", "......", "......", "......", "..##.."],
    ",": ["......", "......", "......", "......", "......", "..##..", ".##..."],
    ":": ["......", "..##..", "..##..", "......", "..##..", "..##..", "......"],
    "?": [".####.", "##..##", "....##", "..###.", "..##..", "......", "..##.."],
    "'": ["..##..", "..##..", ".##...", "......", "......", "......", "......"],
    "/": ["....##", "...##.", "...##.", "..##..", ".##...", ".##...", "##...."],
    ">": [".##...", "..##..", "...##.", "....##", "...##.", "..##..", ".##...."],
    "<": ["....##", "...##.", "..##..", ".##...", "..##..", "...##.", "....##"],
}

# BG3 2bpp color roles: 1 text, 2 border/bevel, 3 panel navy. Only BG3
# palettes 0-3 (CGRAM 0-15) are used (ADR-016); 2 and 3 are rewritten at
# match start with the left/right team colours from the palette parts.
HUD_PALETTES = [
    [(31, 31, 31), (21, 22, 26), (1, 3, 7)],    # 0 neutral text, borders
    [(31, 27, 4), (28, 8, 4), (1, 3, 7)],       # 1 title gold, red bevel/B
    [(12, 19, 31), (5, 9, 24), (1, 3, 7)],      # 2 team left (default blue)
    [(31, 9, 8), (20, 3, 4), (1, 3, 7)],        # 3 team right (default red)
]

HUD_TILE_CLEAR = 0
HUD_TILE_PANEL_TL = 1
HUD_TILE_PANEL_T = 2
HUD_TILE_PANEL_L = 3
HUD_TILE_BUTTON_B = 4
HUD_TILE_CURSOR = 5
HUD_TILE_BAR_ON = 6
HUD_TILE_BAR_OFF = 7
HUD_TILE_BALL_ICON = 8
HUD_TILE_BIG_BASE = 128


def tile_from_strings(rows, mapping):
    return [[mapping[ch] for ch in row] for row in rows]


def hud_assets():
    tiles = [[[0] * 8 for _ in range(8)] for _ in range(HUD_TILE_BIG_BASE)]
    m = {".": 0, "t": 1, "g": 2, "n": 3}
    tiles[HUD_TILE_PANEL_TL] = tile_from_strings([
        "........", "........", "........", ".....nnn",
        "....nggg", "...ngnnn", "...ngnnn", "...ngnnn"], m)
    tiles[HUD_TILE_PANEL_T] = tile_from_strings([
        "........", "........", "........", "nnnnnnnn",
        "gggggggg", "nnnnnnnn", "nnnnnnnn", "nnnnnnnn"], m)
    tiles[HUD_TILE_PANEL_L] = tile_from_strings(["...ngnnn"] * 8, m)
    # B button for palette 1: red disk (bevel colour), gold letter.
    tiles[HUD_TILE_BUTTON_B] = tile_from_strings([
        "nnggggnn", "nggttggn", "ggtggtgg", "ggtttggg",
        "ggtggtgg", "ggtttggg", "nggggggn", "nnggggnn"], m)
    tiles[HUD_TILE_CURSOR] = tile_from_strings([
        "nnnnnnnn", "nttnnnnn", "nttttnnn", "ntttttnn",
        "nttttnnn", "nttnnnnn", "nnnnnnnn", "nnnnnnnn"], m)
    tiles[HUD_TILE_BAR_ON] = tile_from_strings([
        "nnnnnnnn", "nttttttn", "nttttttn", "nttttttn",
        "nttttttn", "nttttttn", "nnnnnnnn", "nnnnnnnn"], m)
    tiles[HUD_TILE_BAR_OFF] = tile_from_strings([
        "nnnnnnnn", "nggggggn", "ngnnnngn", "ngnnnngn",
        "ngnnnngn", "nggggggn", "nnnnnnnn", "nnnnnnnn"], m)
    tiles[HUD_TILE_BALL_ICON] = tile_from_strings([
        "nnggggnn", "ngtgtggn", "gtgggtgg", "gggtgggg",
        "gtgggtgg", "ggtgggtg", "ngggtggn", "nnggggnn"], m)
    # ASCII 32..95 map to tile number == character code (navy background).
    for code in range(32, 96):
        glyph = FONT.get(chr(code))
        px = [[3] * 8 for _ in range(8)]
        if glyph:
            for y, row in enumerate(glyph):
                for x, ch in enumerate(row[:6]):
                    if ch == "#":
                        px[y][x + 1] = 1
        tiles[code] = px
    # Big 16x16 glyphs: fill 1, navy outline 3, transparent background.
    big_index = {}
    big_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789'!"
    for n, ch in enumerate(big_chars):
        glyph = FONT[ch]
        canvas = [[0] * 16 for _ in range(16)]
        for y, row in enumerate(glyph):
            for x, c in enumerate(row[:6]):
                if c == "#":
                    for dy in range(2):
                        for dx in range(2):
                            canvas[1 + y * 2 + dy][2 + x * 2 + dx] = 1
        outlined = [row[:] for row in canvas]
        for y in range(16):
            for x in range(16):
                if canvas[y][x]:
                    continue
                for dy in (-1, 0, 1):
                    for dx in (-1, 0, 1):
                        yy, xx = y + dy, x + dx
                        if 0 <= yy < 16 and 0 <= xx < 16 and canvas[yy][xx]:
                            outlined[y][x] = 3
        # Lower half of each stroke uses the border shade for a bevel.
        for y in range(9, 16):
            for x in range(16):
                if outlined[y][x] == 1:
                    outlined[y][x] = 2
        base = HUD_TILE_BIG_BASE + n * 4
        big_index[ch] = base
        while len(tiles) < base + 4:
            tiles.append([[0] * 8 for _ in range(8)])
        tiles[base + 0] = [r[0:8] for r in outlined[0:8]]
        tiles[base + 1] = [r[8:16] for r in outlined[0:8]]
        tiles[base + 2] = [r[0:8] for r in outlined[8:16]]
        tiles[base + 3] = [r[8:16] for r in outlined[8:16]]
    if len(tiles) > HUD_TILE_BUDGET:
        raise AssetError(f"HUD uses {len(tiles)} tiles > {HUD_TILE_BUDGET}")
    pal = []
    for colors in HUD_PALETTES:
        pal.append(0)
        pal.extend(bgr15(*c) for c in colors)
    return b"".join(encode_tile(t, 2) for t in tiles), \
        struct.pack(f"<{len(pal)}H", *pal), big_chars, len(tiles)
