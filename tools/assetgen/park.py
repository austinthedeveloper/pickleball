"""Open play park (ADR-034): the BG1 picture, painted from the layout in
include/open_play.h so the art and the walking code agree."""
import re

from .common import ROOT, AssetError
from .select import indexed_scene

PARK_TILE_BUDGET = 768      # BG1 tiles at VRAM word 0x1000..0x3fff
SUB = 1                     # BG palette 1 (0 is the HUD's); court n: 1 + n
# Each court's apron shows its skill level (ROOKIE, CLUB, CHAMP): the
# court's tiles use their own copy of the palette with this apron colour.
LEVEL_APRONS = [(52, 150, 120), (190, 150, 60), (176, 66, 60)]
# The bench crowd (M4-OPEN-02) uses a palette of its own: the park's, with
# colours that never appear by the benches (the courts', the trees', the
# metal) swapped for people's. Drawn with the park name, shown as this.
CROWD = {"surface": (200, 64, 64), "kitchen": (232, 200, 64),
         "apron": (132, 84, 176), "tree": (64, 40, 28),
         "tree_light": (232, 172, 132), "metal": (60, 160, 172)}
CROWD_AS = {"red": "surface", "yellow": "kitchen", "purple": "apron",
            "hair": "tree", "skin": "tree_light", "teal": "metal"}

# The clubhouse (M4-OPEN-05) is tile-aligned, so its tiles take a palette
# of their own: BG palette 6 (after the courts' and the crowd's).
HOUSE = {
    "outline": (40, 26, 20), "wall": (120, 76, 44),
    "wall_light": (164, 112, 64), "floor": (196, 156, 104),
    "floor_line": (164, 124, 80), "board": (36, 80, 58),
    "chalk": (230, 232, 220), "frame": (140, 92, 50),
    "metal": (130, 134, 142), "metal_dark": (70, 74, 82),
    "ball": (232, 210, 60), "rug": (168, 52, 52),
    "rug_light": (214, 120, 90), "plant": (46, 110, 52),
    "plant_light": (90, 156, 70),
}
HOUSE_NAMES = list(HOUSE)

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


HOUSE_SUB = SUB + 5          # after 3 courts and the crowd


def house_paint(rows, lay, path_top, park_color):
    """The clubhouse, seen from above with no roof: wooden walls with a
    door in the bottom one, a plank floor, the records board on the top
    wall, the ball machine, the bench the waiting teams sit on, a rug, and
    plants. Its rectangle is whole tiles (checked), all in HOUSE colours."""
    left, right = lay["OPEN_HOUSE_LEFT"], lay["OPEN_HOUSE_RIGHT"]
    top, bottom = lay["OPEN_HOUSE_TOP"], lay["OPEN_HOUSE_BOTTOM"]
    wall = lay["OPEN_HOUSE_WALL"]
    door_x, door_w = lay["OPEN_HOUSE_DOOR_X"], lay["OPEN_HOUSE_DOOR_W"]
    if any(v % 8 for v in (left, right, top, bottom)):
        raise AssetError("park: the clubhouse must be whole tiles")
    color = {name: HOUSE_SUB * 16 + 1 + i for i, name in enumerate(HOUSE_NAMES)}

    def rect(x0, y0, x1, y1, name):
        for y in range(y0, y1):
            for x in range(x0, x1):
                rows[y][x] = color[name]

    # The walk from the path to the door (park colours, below the house).
    for y in range(bottom, path_top + 1):
        for x in range(door_x - door_w // 2, door_x + door_w // 2):
            rows[y][x] = park_color["path"]
    # Floor planks: 6 px boards, joints staggered every 32.
    rect(left, top, right, bottom, "floor")
    for n, y in enumerate(range(top + wall, bottom - wall, 6)):
        rect(left, y, right, y + 1, "floor_line")
        for x in range(left + (n % 2) * 16, right, 32):
            rect(x, y, x + 1, min(y + 6, bottom - wall), "floor_line")
    # A rug in the middle.
    rect(360, 92, 424, 118, "rug")
    rect(363, 95, 421, 96, "rug_light")
    rect(363, 114, 421, 115, "rug_light")
    rect(363, 95, 364, 115, "rug_light")
    rect(420, 95, 421, 115, "rug_light")
    # Walls, outlined, the inner edge lit; the door in the bottom wall and
    # a mat inside it.
    rect(left, top, right, top + wall, "wall")
    rect(left, bottom - wall, right, bottom, "wall")
    rect(left, top, left + wall, bottom, "wall")
    rect(right - wall, top, right, bottom, "wall")
    rect(left, top, right, top + 1, "outline")
    rect(left, bottom - 1, right, bottom, "outline")
    rect(left, top, left + 1, bottom, "outline")
    rect(right - 1, top, right, bottom, "outline")
    rect(left + wall, top + wall - 1, right - wall, top + wall, "wall_light")
    rect(left + wall - 1, top + wall, left + wall, bottom - wall, "outline")
    rect(right - wall, top + wall, right - wall + 1, bottom - wall, "outline")
    rect(left + wall, bottom - wall, right - wall, bottom - wall + 1,
         "outline")
    rect(door_x - door_w // 2, bottom - wall, door_x + door_w // 2, bottom,
         "floor")
    rect(door_x - door_w // 2 - 1, bottom - wall, door_x - door_w // 2,
         bottom, "outline")
    rect(door_x + door_w // 2, bottom - wall, door_x + door_w // 2 + 1,
         bottom, "outline")
    rect(door_x - 10, bottom - wall - 7, door_x + 10, bottom - wall - 1,
         "rug")
    # The records board on the top wall: a frame, chalk lines of records.
    bx = lay["OPEN_BOARD_X"]
    by = top + wall
    rect(bx - 22, by, bx + 22, by + 14, "frame")
    rect(bx - 20, by + 1, bx + 20, by + 12, "board")
    for n, (y, length) in enumerate(((by + 3, 30), (by + 6, 22),
                                     (by + 9, 26))):
        rect(bx - 16, y, bx - 16 + length, y + 1, "chalk")
        rect(bx + 14, y, bx + 17, y + 1, "chalk")
    # The ball machine: a metal box, a hopper of balls on top, a chute.
    mx, my = lay["OPEN_MACHINE_X"], lay["OPEN_MACHINE_Y"]
    rect(mx - 8, my - 22, mx + 8, my - 6, "metal")
    rect(mx - 8, my - 22, mx + 8, my - 21, "outline")
    rect(mx - 8, my - 7, mx + 8, my - 6, "outline")
    rect(mx - 8, my - 22, mx - 7, my - 6, "outline")
    rect(mx + 7, my - 22, mx + 8, my - 6, "outline")
    rect(mx - 7, my - 27, mx + 7, my - 22, "metal_dark")
    for x in range(mx - 6, mx + 6, 3):
        rect(x, my - 26, x + 2, my - 24, "ball")
    rect(mx - 2, my - 6, mx + 2, my - 4, "metal_dark")
    rect(mx - 5, my - 16, mx + 5, my - 13, "metal_dark")
    # The bench: a back rail, the seat, its legs.
    sx, sy = lay["OPEN_BENCH_X"], lay["OPEN_BENCH_Y"]
    rect(sx, sy - 14, sx + 48, sy - 12, "wall")
    rect(sx, sy - 2, sx + 48, sy + 2, "wall_light")
    rect(sx, sy + 2, sx + 48, sy + 3, "outline")
    for x in (sx + 2, sx + 22, sx + 44):
        rect(x, sy + 3, x + 2, sy + 6, "outline")
    # Plants in pots, in the corners clear of the board and the machine.
    for px, py in ((left + 12, top + 16), (left + 12, bottom - 16),
                   (right - 12, bottom - 16), (right - 12, top + 40)):
        for y in range(py - 6, py + 4):
            for x in range(px - 6, px + 6):
                d = (x - px) ** 2 + (y - py + 1) ** 2
                if d < 30:
                    rows[y][x] = color["plant_light" if d < 9 else "plant"]
        rect(px - 3, py + 2, px + 3, py + 6, "wall")


def park_assets():
    lay = park_layout()
    width, height = lay["OPEN_PARK_W"], 224
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

    # Benches below the path, each with three people watching.
    benches = (36, 116, 196)
    shirts = ("red", "yellow", "purple", "teal")
    for n, bx in enumerate(benches):
        seat = path_top + 28
        rect(bx, seat, bx + 24, seat + 3, "wood")
        rect(bx + 2, seat + 3, bx + 4, seat + 6, "dark")
        rect(bx + 20, seat + 3, bx + 22, seat + 6, "dark")
        for k, px in enumerate((bx + 2, bx + 9, bx + 16)):
            shirt = CROWD_AS[shirts[(n + k) % len(shirts)]]
            rect(px + 1, seat - 10, px + 5, seat - 9, CROWD_AS["hair"])
            rect(px + 1, seat - 9, px + 5, seat - 6, CROWD_AS["skin"])
            rect(px, seat - 6, px + 6, seat, shirt)

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

    # The second screen: bushes on the grass, a walk from the path to the
    # clubhouse door, and the clubhouse.
    for bx, by in ((268, 64), (276, 118), (500, 70), (498, 140)):
        for y in range(by - 6, by + 6):
            for x in range(bx - 7, bx + 7):
                d = (x - bx) ** 2 + (y - by) ** 2 * 2
                if d < 50:
                    put(x, y, "tree_light" if d < 16 else "tree")
    house_paint(rows, lay, path_top, color)

    # Each court's tile cells take that court's palette (same indices).
    courts = lay["OPEN_COURTS"]
    if len(LEVEL_APRONS) != courts:
        raise AssetError("park.py: one apron colour per court")
    for c in range(courts):
        left = lay["OPEN_COURT_LEFT0"] + c * lay["OPEN_COURT_PITCH"]
        for ty in range(top // 8, (top + court_h - 1) // 8 + 1):
            for tx in range(left // 8, (left + court_w - 1) // 8 + 1):
                for y in range(ty * 8, ty * 8 + 8):
                    for x in range(tx * 8, tx * 8 + 8):
                        rows[y][x] += 16 * (1 + c)

    # The benches' tile cells take the crowd palette.
    crowd_sub = SUB + courts + 1
    for bx in benches:
        for ty in range((path_top + 16) // 8, (path_top + 34) // 8 + 1):
            for tx in range(bx // 8, (bx + 23) // 8 + 1):
                for y in range(ty * 8, ty * 8 + 8):
                    for x in range(tx * 8, tx * 8 + 8):
                        if rows[y][x] // 16 != SUB:
                            raise AssetError("park: a bench overlaps a court")
                        rows[y][x] += 16 * (crowd_sub - SUB)

    palette = [(0, 0, 0)] * (SUB * 16)
    palette += [(0, 0, 0)] + [COLORS[name] for name in NAMES]
    for apron in LEVEL_APRONS:
        palette += [(0, 0, 0)] + [apron if name == "apron" else COLORS[name]
                                  for name in NAMES]
    palette += [(0, 0, 0)] + [CROWD.get(name, COLORS[name]) for name in NAMES]
    palette += [(0, 0, 0)] + [HOUSE[name] for name in HOUSE_NAMES]
    return indexed_scene("park", rows, palette, PARK_TILE_BUDGET,
                         first_sub=SUB, last_sub=HOUSE_SUB)
