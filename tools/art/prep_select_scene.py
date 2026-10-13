#!/usr/bin/env python3
"""Authoring tool: Player Select art from the two select mocks (ADR-020).

NOT part of the normal build (needs Pillow and numpy); run by hand when the
select art changes. Writes two committed indexed PNGs:

  assets/source/select_scene.png      256x224 BG1 art. Palette slots 16..111
                                      are BG palettes 1..6, one per tile.
  assets/source/select_portraits.png  192x96: the male and female portraits
                                      as 96x96 cells in player palette roles
                                      (ADR-013 contract v2), recoloured at
                                      run time through BG palette 7.

Composition: the mock's background, title banner, panel frames, PADDLE
header, and footer are kept. The panel interiors are repainted from
include/select_layout.h: a procedural fence/court backdrop, the MALE/FEMALE
toggle, empty thumbnail boxes with labels (the game draws OBJ thumbnails),
procedural paddles in the paddle palette parts, and stat-row pills (the game
draws BG3 bars). The portraits are traced out of the mocks by nearest
reference colour and mapped to palette roles.

Usage: python tools/art/prep_select_scene.py
"""
import json
import re
import sys
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "tools" / "art"))
from assetgen.hud import FONT  # noqa: E402
from prep_court_scene import quantize_tiles  # noqa: E402

MOCK_M = ROOT / "assets/inspiration/pickleball-snes-player-select.png"
MOCK_F = ROOT / "assets/inspiration/pickleball-snes-player-select-female.png"
OUT_SCENE = ROOT / "assets/source/select_scene.png"
OUT_PORTRAITS = ROOT / "assets/source/select_portraits.png"
PALETTES = ROOT / "assets/source/palettes"
W, H = 256, 224
CELL = 96


def layout():
    text = (ROOT / "include/select_layout.h").read_text()
    return {k: int(v) for k, v in
            re.findall(r"#define (SEL_\w+) (\d+)", text)}


L = layout()

# Mock-sampled UI colours (8-bit).
NAVY = (3, 18, 52)
FILL = (0, 8, 42)
GREY = (120, 116, 113)
GREY_D = (60, 66, 72)
CREAM = (250, 232, 185)
WHITE = (240, 240, 240)
SHADE = (16, 28, 56)
# Backdrop (portrait cluster colours).
SKY = (0, 58, 74)
SKY_D = (0, 49, 63)
MESH = (3, 88, 99)
POST = (8, 102, 111)
COURT = (0, 76, 90)
COURT_D = (0, 68, 83)
LINE = (13, 114, 121)
SHADOW = (1, 40, 60)


def c5(rgb):
    return tuple(round(v * 255 / 31) for v in rgb)


def part(name):
    return json.loads((PALETTES / f"{name}.json").read_text())


# 5x7 label font (1-px strokes); glyph rows, '#' = ink.
SMALL = {
    "A": [".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"],
    "B": ["####.", "#...#", "#...#", "####.", "#...#", "#...#", "####."],
    "C": [".###.", "#...#", "#....", "#....", "#....", "#...#", ".###."],
    "D": ["####.", "#...#", "#...#", "#...#", "#...#", "#...#", "####."],
    "E": ["#####", "#....", "#....", "####.", "#....", "#....", "#####"],
    "G": [".###.", "#...#", "#....", "#.###", "#...#", "#...#", ".####"],
    "L": ["#....", "#....", "#....", "#....", "#....", "#....", "#####"],
    "N": ["#...#", "##..#", "#.#.#", "#..##", "#...#", "#...#", "#...#"],
    "O": [".###.", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."],
    "P": ["####.", "#...#", "#...#", "####.", "#....", "#....", "#...."],
    "R": ["####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#"],
    "S": [".####", "#....", "#....", ".###.", "....#", "....#", "####."],
    "T": ["#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.."],
    "U": ["#...#", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."],
    "W": ["#...#", "#...#", "#...#", "#.#.#", "#.#.#", "##.##", "#...#"],
}


class Canvas:
    def __init__(self, rgb):
        self.rgb = rgb

    def rect(self, x0, y0, x1, y1, col):
        self.rgb[y0:y1 + 1, x0:x1 + 1] = col

    def frame(self, x0, y0, x1, y1, col):
        self.rect(x0, y0, x1, y0, col)
        self.rect(x0, y1, x1, y1, col)
        self.rect(x0, y0, x0, y1, col)
        self.rect(x1, y0, x1, y1, col)

    def box(self, x0, y0, x1, y1, fill, border):
        self.rect(x0, y0, x1, y1, fill)
        self.frame(x0, y0, x1, y1, border)
        # Rounded corners: the panel colour shows through.
        for x, y in ((x0, y0), (x1, y0), (x0, y1), (x1, y1)):
            self.rgb[y, x] = NAVY

    def pixel(self, x, y, col):
        if 0 <= x < W and 0 <= y < H:
            self.rgb[y, x] = col

    def text(self, x, y, s, col, font, pitch, shadow=None):
        for i, ch in enumerate(s):
            glyph = font.get(ch)
            if not glyph:
                continue
            for gy, row in enumerate(glyph):
                for gx, c in enumerate(row):
                    if c == "#":
                        if shadow:
                            self.pixel(x + i * pitch + gx + 1, y + gy + 1,
                                       shadow)
                        self.pixel(x + i * pitch + gx, y + gy, col)


def text_width(s, pitch, glyph_w):
    return len(s) * pitch - (pitch - glyph_w)


def paint_backdrop(cv):
    x0, x1 = L["SEL_BACKDROP_X0"], L["SEL_BACKDROP_X1"]
    y0, y1 = L["SEL_BACKDROP_Y0"], L["SEL_BACKDROP_Y1"]
    fence_bottom = 95
    cv.rect(x0, y0, x1, fence_bottom, SKY)
    # Chain-link mesh: two diagonal families every 6 px.
    for y in range(y0, fence_bottom):
        for x in range(x0, x1 + 1):
            if (x + y) % 6 == 0 or (x - y) % 6 == 0:
                cv.rgb[y, x] = MESH if (y // 3) % 2 else SKY_D
    for px in (30, 76, 122):
        cv.rect(px, y0, px + 1, fence_bottom, POST)
    cv.rect(x0, y0 + 6, x1, y0 + 7, POST)
    cv.rect(x0, fence_bottom - 1, x1, fence_bottom, POST)
    # Court floor with perspective lines.
    cv.rect(x0, fence_bottom + 1, x1, y1, COURT)
    for y in range(fence_bottom + 1, y1 + 1):
        if (y - fence_bottom) % 2 == 0 and y < fence_bottom + 6:
            cv.rect(x0, y, x1, y, COURT_D)
    base = fence_bottom + 8
    cv.rect(x0, base, x1, base, LINE)
    for y in range(base, y1 + 1):
        t = (y - base) / max(1, y1 - base)
        for sx in (34 - 22 * t, 118 + 22 * t):
            cv.pixel(int(round(sx)), y, LINE)
        cv.pixel(76, y, LINE)
    # Shadow under the portrait's feet.
    cx, cy, rx, ry = 76, 131, 46, 4
    for y in range(cy - ry, min(y1, cy + ry) + 1):
        for x in range(cx - rx, cx + rx + 1):
            if ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2 <= 1:
                cv.pixel(x, y, SHADOW)


def paint_left_lower(cv):
    cv.rect(L["SEL_BACKDROP_X0"], L["SEL_BACKDROP_Y1"] + 1,
            L["SEL_BACKDROP_X1"], 184, NAVY)
    names = ["BLUE", "RED", "GREEN", "GOLD"]
    for i, name in enumerate(names):
        x0 = L["SEL_THUMB_X0"] + i * L["SEL_THUMB_PITCH"]
        x1 = x0 + L["SEL_THUMB_W"] - 1
        cv.box(x0, L["SEL_THUMB_Y0"], x1, L["SEL_THUMB_Y1"], SHADE, GREY_D)
        cv.box(x0, L["SEL_LABEL_Y0"], x1, L["SEL_LABEL_Y1"], FILL, GREY)
        w = text_width(name, 6, 5)
        cv.text(x0 + (L["SEL_THUMB_W"] - w) // 2, L["SEL_LABEL_Y0"] + 2,
                name, WHITE, SMALL, 6)


def paint_toggle(cv):
    y0, y1 = L["SEL_TOGGLE_Y0"], L["SEL_TOGGLE_Y1"]
    for word, x0, x1 in (("MALE", L["SEL_MALE_X0"], L["SEL_MALE_X1"]),
                         ("FEMALE", L["SEL_FEMALE_X0"],
                          L["SEL_FEMALE_X1"])):
        cv.box(x0, y0, x1, y1, FILL, GREY)
        w = text_width(word, 7, 7)
        cv.text(x0 + (x1 - x0 + 1 - w) // 2 - 1, y0 + 3, word, WHITE, FONT,
                7)


def paint_paddle(cv, name, x0, y0, x1, y1):
    p = part(f"paddle_{name}")
    dark, light, edge = c5(p["face"][0]), c5(p["face"][1]), c5(p["edge"])
    cx = (x0 + x1) // 2
    fx0, fx1, fy0, fy1 = cx - 7, cx + 7, y0 + 5, y0 + 28
    outline = (8, 8, 14)
    for y in range(fy0, fy1 + 1):
        for x in range(fx0, fx1 + 1):
            corner = (min(x - fx0, fx1 - x) + min(y - fy0, fy1 - y)) < 3
            if corner:
                continue
            edge_px = (min(x - fx0, fx1 - x) == 0 or
                       min(y - fy0, fy1 - y) == 0 or
                       (min(x - fx0, fx1 - x) + min(y - fy0, fy1 - y)) == 3)
            if edge_px:
                cv.pixel(x, y, outline)
            elif name == "wood":
                cv.pixel(x, y, dark if (x * 3 + y // 4) % 5 == 0 else light)
            else:
                ring = (min(x - fx0, fx1 - x) == 1 or
                        min(y - fy0, fy1 - y) == 1 or
                        (min(x - fx0, fx1 - x) +
                         min(y - fy0, fy1 - y)) == 4)
                rim = edge if name in ("red", "gold") else light
                if ring:
                    cv.pixel(x, y, rim)
                elif name == "red" and abs((x - fx0) - (fy1 - y) * 0.6) < 1:
                    cv.pixel(x, y, rim)
                elif name == "gold" and ((x - fx0) % 3 == 0 or
                                         (y - fy0) % 3 == 0):
                    cv.pixel(x, y, light)
                else:
                    cv.pixel(x, y, dark)
    # Throat and handle.
    grip = edge if name == "wood" else (24, 24, 30)
    cap = WHITE if name == "wood" else edge
    for y in range(fy1 + 1, fy1 + 16):
        for x in range(cx - 2, cx + 2):
            border = x in (cx - 2, cx + 1)
            cv.pixel(x, y, outline if border else grip)
    cv.rect(cx - 2, fy1 + 16, cx + 1, fy1 + 17, outline)
    cv.rect(cx - 1, fy1 + 15, cx, fy1 + 15, cap)


def paint_right(cv):
    cv.rect(150, 67, 246, 184, NAVY)
    names = ["wood", "blue", "red", "gold"]
    for i, name in enumerate(names):
        x0 = L["SEL_PADDLE_X0"] + i * L["SEL_PADDLE_PITCH"]
        x1 = x0 + L["SEL_PADDLE_W"] - 1
        cv.box(x0, L["SEL_PADDLE_Y0"], x1, L["SEL_PADDLE_Y1"], SHADE, GREY_D)
        paint_paddle(cv, name, x0, L["SEL_PADDLE_Y0"], x1,
                     L["SEL_PADDLE_Y1"])
    for i, label in enumerate(("POWER", "CONTROL", "SPEED")):
        row = L["SEL_BAR_ROW"] + 2 * i
        y0, y1 = row * 8 - 3, row * 8 + 10
        cv.box(L["SEL_STAT_X0"], y0, L["SEL_STAT_X1"], y1, FILL, GREY)
        cv.text(L["SEL_STAT_X0"] + 4, row * 8, label, WHITE, SMALL, 6)
    # Paddle name strip (BG3 text on navy).
    cv.rect(L["SEL_NAME_COL"] * 8 - 2, L["SEL_NAME_ROW"] * 8 - 2,
            L["SEL_NAME_COL"] * 8 + 81, L["SEL_NAME_ROW"] * 8 + 9, FILL)


def build_scene():
    mock = Image.open(MOCK_M).convert("RGB").resize((W, H), Image.BOX)
    rgb = np.asarray(mock).astype(np.float64).copy()
    cv = Canvas(rgb)
    paint_backdrop(cv)
    paint_toggle(cv)
    paint_left_lower(cv)
    paint_right(cv)
    rgb5 = np.clip(np.round(rgb / 255.0 * 31.0), 0, 31)
    tiles = [(tx, ty) for ty in range(H // 8) for tx in range(W // 8)]
    palettes, index = quantize_tiles(rgb5, tiles, 6)
    out = np.zeros((H, W), dtype=np.uint8)
    for (tx, ty), (pal, block) in index.items():
        out[ty * 8:ty * 8 + 8, tx * 8:tx * 8 + 8] = (1 + pal) * 16 + block
    flat = [0] * 768
    for p, pal in enumerate(palettes):
        for i, col in enumerate(pal):
            slot = (1 + p) * 16 + 1 + i
            flat[slot * 3:slot * 3 + 3] = [round(v * 255 / 31) for v in col]
    img = Image.fromarray(out, mode="P")
    img.putpalette(flat)
    img.save(OUT_SCENE, optimize=False)
    return rgb


# Portrait tracing: reference colours (8-bit) -> palette role, 0 = backdrop.
BACKDROP = [(0, 68, 83), (0, 55, 71), (6, 96, 104), (1, 73, 86), (0, 79, 93),
            (8, 102, 111), (2, 88, 99), (0, 76, 89), (13, 114, 121),
            (1, 82, 95), (0, 59, 75), (0, 64, 79), (0, 49, 63), (1, 40, 60),
            (4, 97, 106), (7, 106, 115), (0, 62, 78)]
REFS = {
    "male": {
        1: [(6, 14, 18), (3, 3, 4), (5, 25, 40)],
        2: [(205, 104, 56), (110, 59, 34)],
        3: [(248, 156, 86)],
        5: [(3, 58, 197), (7, 42, 131)],
        6: [(13, 100, 242)],
        8: [(158, 160, 209), (99, 104, 147)],
        9: [(236, 231, 228)],
        11: [(31, 35, 68)],
        15: [(57, 28, 15)],
    },
    "female": {
        1: [(9, 8, 9), (5, 23, 39)],
        2: [(217, 120, 59)],
        3: [(249, 172, 87)],
        5: [(7, 55, 154), (6, 74, 211)],
        6: [(16, 111, 243)],
        8: [(163, 170, 219), (119, 120, 156)],
        9: [(237, 235, 236)],
        11: [(23, 30, 72)],
        15: [(148, 86, 34), (80, 46, 16)],
    },
}
# Crop origin in mock pixels (= screen pixels; BG2 scrolls by
# SEL_PORTRAIT_SCROLL_X), first mock row that may hold the figure, and the
# paddle face box (crop pixels) whose blues become paddle roles.
CROP = (L["SEL_PORTRAIT_COL"] * 8 - L["SEL_PORTRAIT_SCROLL_X"],
        L["SEL_PORTRAIT_ROW"] * 8)
FIRST_ROW = {"male": 56, "female": 63}
PADDLE_BOX = {"male": (61, 41, 85, 64), "female": (61, 42, 83, 63)}


def trace(mock_path, kind):
    im = Image.open(mock_path).convert("RGB").resize((W, H), Image.BOX)
    x0, y0 = CROP
    reg = np.asarray(im.crop((x0, y0, x0 + CELL, y0 + CELL))).astype(float)
    refs, roles = [], []
    for c in BACKDROP:
        refs.append(c)
        roles.append(0)
    for role, cols in REFS[kind].items():
        for c in cols:
            refs.append(c)
            roles.append(role)
    refs = np.array(refs, dtype=float)
    d = ((reg[:, :, None, :] - refs[None, None]) ** 2).sum(-1)
    out = np.array(roles)[d.argmin(-1)]
    out[: max(0, FIRST_ROW[kind] - y0), :] = 0
    px0, py0, px1, py1 = PADDLE_BOX[kind]
    face = out[py0:py1, px0:px1]
    face[(face == 5) | (face == 6)] = 12
    face[face == 11] = 11
    # Keep only the figure: drop isolated specks (no role-coloured
    # 4-neighbour) left by antialiasing against the backdrop.
    keep = out > 0
    nb = np.zeros_like(out)
    nb[1:, :] += keep[:-1, :]
    nb[:-1, :] += keep[1:, :]
    nb[:, 1:] += keep[:, :-1]
    nb[:, :-1] += keep[:, 1:]
    out[(nb == 0)] = 0
    # Holes: backdrop pixels not reachable from the cell border take the
    # most common role among their filled neighbours.
    outside = np.zeros(out.shape, dtype=bool)
    stack = [(y, x) for y in range(CELL) for x in (0, CELL - 1)] + \
        [(y, x) for x in range(CELL) for y in (0, CELL - 1)]
    while stack:
        y, x = stack.pop()
        if not (0 <= y < CELL and 0 <= x < CELL) or outside[y, x] or \
                out[y, x]:
            continue
        outside[y, x] = True
        stack += [(y + 1, x), (y - 1, x), (y, x + 1), (y, x - 1)]
    for _ in range(4):
        holes = np.argwhere((out == 0) & ~outside)
        for y, x in holes:
            near = [out[yy, xx] for yy, xx in ((y + 1, x), (y - 1, x),
                                               (y, x + 1), (y, x - 1))
                    if 0 <= yy < CELL and 0 <= xx < CELL and out[yy, xx]]
            if near:
                out[y, x] = max(set(near), key=near.count)
    # Close the outline: a backdrop pixel touching a non-outline figure
    # pixel becomes outline.
    fig = (out > 0) & (out != 1)
    touch = np.zeros(out.shape, dtype=bool)
    touch[1:, :] |= fig[:-1, :]
    touch[:-1, :] |= fig[1:, :]
    touch[:, 1:] |= fig[:, :-1]
    touch[:, :-1] |= fig[:, 1:]
    out[(out == 0) & touch] = 1
    return out.astype(np.uint8)


def build_portraits():
    male = trace(MOCK_M, "male")
    female = trace(MOCK_F, "female")
    sheet = np.zeros((CELL, CELL * 2), dtype=np.uint8)
    sheet[:, :CELL] = male
    sheet[:, CELL:] = female
    # Preview palette: blue team, blue paddle, light skin (contract v2).
    team, skin, pad = part("team_blue"), part("skin_light"), \
        part("paddle_blue")
    shared = part("shared")
    pal = [(255, 0, 255), c5(shared["outline"])] + \
        [c5(c) for c in skin["skin"]] + [c5(c) for c in team["shirt"]] + \
        [c5(c) for c in team["shorts"]] + [c5(team["shoe"])] + \
        [c5(c) for c in pad["face"]] + [c5(pad["edge"]),
                                        c5(shared["highlight"]),
                                        c5(skin["hair"])]
    flat = [v for c in pal for v in c] + [0] * (768 - 3 * len(pal))
    img = Image.fromarray(sheet, mode="P")
    img.putpalette(flat)
    img.save(OUT_PORTRAITS, optimize=False)


def main():
    build_scene()
    build_portraits()
    print(f"wrote {OUT_SCENE.relative_to(ROOT)} and "
          f"{OUT_PORTRAITS.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
