"""Open play park (ADR-034): the BG1 picture, painted from the layout in
include/open_play.h so the art and the walking code agree."""
import re

from .common import ROOT, AssetError
from .select import indexed_scene

PARK_TILE_BUDGET = 768      # BG1 tiles at VRAM word 0x1000..0x3fff
SUB = 1                     # BG palette 1 (0 is the HUD's)

# One 15-colour palette, so no tile ever mixes palettes.
COLORS = {
    "grass": (52, 128, 56), "grass_light": (70, 150, 64),
    "path": (196, 172, 120), "path_edge": (160, 136, 92),
    "tree": (26, 72, 38), "tree_light": (46, 106, 46),
    "wood": (128, 84, 48), "fence": (44, 52, 60),
    "surface": (40, 96, 170), "kitchen": (30, 58, 120),
    "apron": (60, 140, 100), "line": (236, 236, 236),
    "dark": (24, 28, 36), "light": (224, 212, 180),
    "metal": (130, 134, 142),
}
NAMES = list(COLORS)


def park_layout():
    """The integer constants of open_play.h's enum, with the derived ones
    computed the same way."""
    text = (ROOT / "include" / "open_play.h").read_text("utf-8")
    values = {k: int(v) for k, v in re.findall(r"\b(OPEN_\w+) = (\d+),", text)}
    values["OPEN_RACK_Y"] = (values["OPEN_COURT_TOP"] +
                             values["OPEN_COURT_H"] + 10)
    values["OPEN_RACK_DX"] = values["OPEN_COURT_W"] // 2
    if "OPEN_RACK_Y = OPEN_COURT_TOP + OPEN_COURT_H + 10" not in text or \
            "OPEN_RACK_DX = OPEN_COURT_W / 2" not in text:
        raise AssetError("open_play.h: the rack formulas changed; update "
                         "tools/assetgen/park.py")
    return values


def park_assets():
    lay = park_layout()
    width, height = 256, 224
    color = {name: SUB * 16 + 1 + i for i, name in enumerate(NAMES)}
    rows = [[color["grass"]] * width for _ in range(height)]

    def put(x, y, name):
        if 0 <= x < width and 0 <= y < height:
            rows[y][x] = color[name]

    def rect(x0, y0, x1, y1, name):
        for y in range(max(0, y0), min(height, y1)):
            for x in range(max(0, x0), min(width, x1)):
                rows[y][x] = color[name]

    # Grass texture with an 8-pixel period, so it dedupes to a few tiles.
    for y in range(height):
        for x in range(width):
            if (x % 8, y % 8) in ((1, 2), (5, 6), (6, 1), (3, 5)):
                rows[y][x] = color["grass_light"]

    # Trees along the top (behind the HUD panel).
    for cx in range(8, width, 24):
        for y in range(0, 30):
            for x in range(cx - 12, cx + 12):
                d = (x - cx) ** 2 + (y - 12) ** 2
                if d < 130:
                    put(x, y, "tree_light" if d < 50 and y < 12 else "tree")
        rect(cx - 1, 24, cx + 1, 32, "wood")

    # The path along the bottom, and walks between the courts.
    path_top = lay["OPEN_RACK_Y"] + 10
    rect(0, path_top, width, path_top + 24, "path")
    rect(0, path_top, width, path_top + 1, "path_edge")
    rect(0, path_top + 23, width, path_top + 24, "path_edge")
    top = lay["OPEN_COURT_TOP"]
    court_w, court_h = lay["OPEN_COURT_W"], lay["OPEN_COURT_H"]
    for c in range(lay["OPEN_COURTS"] - 1):
        x = (lay["OPEN_COURT_LEFT0"] + c * lay["OPEN_COURT_PITCH"] + court_w +
             lay["OPEN_COURT_LEFT0"] + (c + 1) * lay["OPEN_COURT_PITCH"]) // 2
        rect(x - 4, top - 8, x + 4, path_top, "path")

    # Benches below the path.
    for bx in (36, 116, 196):
        rect(bx, path_top + 28, bx + 24, path_top + 31, "wood")
        rect(bx + 2, path_top + 31, bx + 4, path_top + 34, "dark")
        rect(bx + 20, path_top + 31, bx + 22, path_top + 34, "dark")

    for c in range(lay["OPEN_COURTS"]):
        left = lay["OPEN_COURT_LEFT0"] + c * lay["OPEN_COURT_PITCH"]
        right, bottom = left + court_w, top + court_h
        # Apron and fence (with a gate at the bottom middle).
        rect(left, top, right, bottom, "apron")
        rect(left, top, right, top + 1, "fence")
        rect(left, bottom - 1, right, bottom, "fence")
        rect(left, top, left + 1, bottom, "fence")
        rect(right - 1, top, right, bottom, "fence")
        mid = left + court_w // 2
        rect(mid - 6, bottom - 1, mid + 6, bottom, "apron")
        # The court: surface, kitchens either side of the net, lines.
        x0, x1, y0, y1 = left + 6, right - 6, top + 6, bottom - 6
        net = (y0 + y1) // 2
        kitchen = (y1 - y0) * 7 // 44          # 7 ft of a 44 ft court
        rect(x0, y0, x1, y1, "surface")
        rect(x0, net - kitchen, x1, net + kitchen, "kitchen")
        for yy in (y0, y1 - 1, net - kitchen, net + kitchen - 1):
            rect(x0, yy, x1, yy + 1, "line")
        rect(x0, y0, x0 + 1, y1, "line")
        rect(x1 - 1, y0, x1, y1, "line")
        cx = (x0 + x1) // 2
        rect(cx, y0, cx + 1, net - kitchen, "line")
        rect(cx, net + kitchen, cx + 1, y1, "line")
        # The net and its posts.
        rect(x0 - 2, net, x1 + 2, net + 1, "dark")
        rect(x0 - 2, net - 1, x0, net + 2, "metal")
        rect(x1, net - 1, x1 + 2, net + 2, "metal")
        # The paddle rack at the gate: a post and a crossbar.
        rx, ry = left + lay["OPEN_RACK_DX"], lay["OPEN_RACK_Y"]
        rect(rx - 1, ry - 12, rx + 1, ry + 2, "metal")
        rect(rx - 4, ry - 12, rx + 5, ry - 11, "metal")
        rect(rx - 2, ry + 2, rx + 3, ry + 3, "dark")

    palette = [(0, 0, 0)] * (SUB * 16)
    palette += [(0, 0, 0)] + [COLORS[name] for name in NAMES]
    return indexed_scene("park", rows, palette, PARK_TILE_BUDGET,
                         first_sub=SUB, last_sub=SUB)
