#!/usr/bin/env python3
"""Authoring tool: writes the indexed sprite source PNGs from text pixel art.

Not part of the normal build (needs Pillow). Outputs, both committed:
  assets/source/player_sprites.png  128x96, 12 frame slots of 32x32, indexed
                                    with the player palette roles below.
  assets/source/objects.png         128x8, sixteen 8x8 small objects.

Player palette roles (all four team palettes share these indices):
  0 transparent  1 outline  2 skin shadow  3 skin  4 hair
  5 team dark    6 team     7 team light   8 white shadow  9 white
  10 paddle grip 11..15 reserved
"""
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
KEY = {".": 0, "O": 1, "s": 2, "S": 3, "H": 4, "d": 5, "T": 6, "L": 7,
       "g": 8, "W": 9, "h": 10}

# Preview palette (blue team); the build owns the real palettes.
PREVIEW = [(255, 0, 255), (16, 20, 36), (176, 104, 72), (240, 168, 120),
           (96, 56, 32), (24, 48, 152), (48, 96, 224), (120, 168, 248),
           (168, 176, 192), (248, 248, 248), (56, 40, 32)]

HEAD = [
    "...OOOOO.....",
    "..OdTTTLO....",
    ".OdTTTTTLO...",
    ".OddTTTTTOOO.",
    ".OHdddddTTTTO",
    ".OHHSSSSOOOO.",
    ".OHSSSOSO....",
    ".OHSSSSSSO...",
    "..OsSSSSO....",
    "...OOsSO.....",
]

# Each frame: (head_x, head_y, rows from y=12 down to y=29). Rows are drawn
# under the head, so '.' in the body never erases head pixels.
IDLE = (10, 3, [
    "................................",
    "............OOOOOOOOO...........",
    "...........OdTTTTTTTLO..........",
    "..........OdTTTTTTTTTLO.........",
    "..........OdTTTTTTTTTsSO...OOO..",
    "..........OdTTTTTTTOOsSSOOOdTTO.",
    "..........OsSdTTTTTTOSSShhhdTTTO",
    "..........OSSOddddddOOOOOOOdTTTO",
    "...........OOgWWWWWWWO....OdTTO.",
    "...........OgWWWWWWWWWO....OOO..",
    "..........OgWWWOOOOgWWO.........",
    "..........OsSSO....OsSSO........",
    ".........OsSSO......OsSSO.......",
    ".........OsSO........OsSO.......",
    ".........OgWO........OgWO.......",
    "........OgWWO........OgWWO......",
    "........OWWWWO.......OWWWWO.....",
    "........OOOOOO.......OOOOOO.....",
])

RUN_TORSO = [
    "................................",
    "............OOOOOOOOO...........",
    "...........OdTTTTTTTLO..........",
    "..........OdTTTTTTTTTLO..OOO....",
    "..........OdTTTTTTTTTsSOOdTTO...",
    "..........OdTTTTTTTOOsSShdTTTO..",
    "..........OsSdTTTTTTOOOOOdTTTO..",
    "..........OSSOddddddO...OdTTO...",
    "...........OOgWWWWWWO....OOO....",
]
RUN_LEGS = [
    [  # contact: stride wide
        "...........OgWWWWWWWWO..........",
        "..........OgWWWOOOgWWWO.........",
        ".........OsSSO.....OsSSO........",
        "........OsSSO.......OsSSO.......",
        ".......OsSO..........OsSO.......",
        "......OgWO............OgWO......",
        ".....OWWWO............OgWWO.....",
        ".....OOOOO............OOOOOO....",
        "................................",
    ],
    [  # recoil: legs closing
        "...........OgWWWWWWWWO..........",
        "...........OgWWOOgWWWO..........",
        "..........OsSSO.OsSSO...........",
        ".........OsSSO..OsSSO...........",
        ".........OsSO....OsSO...........",
        ".........OgWO....OgWWO..........",
        "........OWWWO....OOOOO..........",
        "........OOOOO...................",
        "................................",
    ],
    [  # passing: back foot lifted
        "...........OgWWWWWWWWO..........",
        "............OgWWWWWWO...........",
        "............OsSSOsSSO...........",
        "...........OsSSOOsSO............",
        "...........OsSO.OOgWO...........",
        "...........OgWO..OWWWO..........",
        "..........OWWWO..OOOOO..........",
        "..........OOOOO.................",
        "................................",
    ],
    [  # push off: front leg reaching
        "...........OgWWWWWWWWO..........",
        "...........OgWWWOOgWWWO.........",
        "..........OsSSO...OsSSO.........",
        "..........OsSO.....OsSSO........",
        ".........OgWO.......OsSO........",
        "........OWWWO........OgWO.......",
        "........OOOOO........OgWWO......",
        ".....................OOOOOO.....",
        "................................",
    ],
]

WINDUP = (9, 4, [
    "................................",
    "................................",
    ".OOO.......OOOOOOOOO............",
    "OdTTO.....OdTTTTTTTLO...........",
    "OdTTTO...OdTTTTTTTTTLO..........",
    "OdTTTOhhOdTTTTTTTTTTLO..........",
    "OdTTTOSSSOdTTTTTTTsSO...........",
    ".OdTOOSSSOOdTTTTTTOSSO..........",
    "..OO..OOOOOddddddOSSSO..........",
    "..........OgWWWWWWOOOO..........",
    ".........OgWWWWWWWWWO...........",
    ".........OgWWWOOOgWWWO..........",
    "........OsSSO.....OsSSO.........",
    ".......OsSSO.......OsSSO........",
    "......OsSO..........OsSO........",
    "......OgWO..........OgWO........",
    ".....OgWWO..........OgWWO.......",
    ".....OOOOO..........OOOOOO......",
])

SWING = (11, 3, [
    "................................",
    "............OOOOOOOOO...........",
    "...........OdTTTTTTTLOOOO...OOO.",
    "..........OdTTTTTTTTTLsSSOOOdTTO",
    "..........OdTTTTTTTTTOSSSShhdTTT",
    "..........OdTTTTTTTTTOOOOOOOdTTT",
    "..........OsSdTTTTTTTO......OdTO",
    "..........OSSOdddddddO.......OO.",
    "...........OOgWWWWWWWO..........",
    "...........OgWWWWWWWWWO.........",
    "..........OgWWWOOOOgWWO.........",
    ".........OsSSO.....OsSSO........",
    "........OsSSO.......OsSSO.......",
    ".......OsSO..........OsSO.......",
    "......OgWO............OgWO......",
    ".....OgWWO............OgWWO.....",
    ".....OWWWO............OWWWWO....",
    ".....OOOOO............OOOOOO....",
])

FOLLOW = (11, 3, [
    "......................OOO.......",
    "............OOOOOOOOOOdTTO......",
    "...........OdTTTTTTTLOdTTTO.....",
    "..........OdTTTTTTTTTLOdTTO.....",
    "..........OdTTTTTTTTTOOhOOO.....",
    "..........OdTTTTTTTTSSOhO.......",
    "..........OsSdTTTTTOSSSSO.......",
    "..........OSSOddddddOOOO........",
    "...........OOgWWWWWWWO..........",
    "...........OgWWWWWWWWWO.........",
    "..........OgWWWOOOOgWWO.........",
    "..........OsSSO....OsSSO........",
    ".........OsSSO......OsSSO.......",
    ".........OsSO........OsSO.......",
    ".........OgWO........OgWO.......",
    "........OgWWO........OgWWO......",
    "........OWWWWO.......OWWWWO.....",
    "........OOOOOO.......OOOOOO.....",
])

TOSS = (10, 3, [
    "........................OO......",
    "............OOOOOOOOO..OSSO.....",
    "...........OdTTTTTTTLOOSSO......",
    "..........OdTTTTTTTTTLSSO.......",
    "..........OdTTTTTTTTTLOO........",
    "..OOO.....OdTTTTTTTTTO..........",
    ".OdTTO...OsSdTTTTTTTTO..........",
    ".OdTTTOhhSSOddddddddO...........",
    ".OdTTTOOSSOOgWWWWWWWO...........",
    "..OdTO..OO.OgWWWWWWWWO..........",
    "...OO.....OgWWWOOOgWWO..........",
    "..........OsSSO....OsSSO........",
    ".........OsSSO......OsSSO.......",
    ".........OsSO........OsSO.......",
    ".........OgWO........OgWO.......",
    "........OgWWO........OgWWO......",
    "........OWWWWO.......OWWWWO.....",
    "........OOOOOO.......OOOOOO.....",
])

CHEER = (10, 4, [
    "..OOO...................OOO.....",
    ".OdTTO.................OSSO.....",
    ".OdTTTO...............OSSO......",
    ".OdTTTO..............OSSO.......",
    "..OdTOO.............OSSO........",
    "...OOhO..OOOOOOOOO.OSSO.........",
    ".....OSSOdTTTTTTTLOSSO..........",
    "......OSSdTTTTTTTTLSO...........",
    ".......OOdTTTTTTTTLO............",
    "........OdTTTTTTTTTO............",
    "........OddddddddddO............",
    "........OgWWWWWWWWWO............",
    "........OgWWWOOOgWWO............",
    ".........OsSO...OsSO............",
    ".........OsSO...OsSO............",
    ".........OgWO...OgWO............",
    "........OgWWO...OgWWO...........",
    "........OOOOO...OOOOOO..........",
])


def blank():
    return [[0] * 32 for _ in range(32)]


def stamp(grid, rows, x0, y0, opaque_only=True):
    for dy, row in enumerate(rows):
        for dx, ch in enumerate(row):
            if ch == ".":
                continue
            x, y = x0 + dx, y0 + dy
            if 0 <= x < 32 and 0 <= y < 32:
                grid[y][x] = KEY[ch]


def body_frame(spec, bob=0):
    head_x, head_y, rows = spec
    grid = blank()
    stamp(grid, rows, 0, 12 + bob)
    stamp(grid, HEAD, head_x, head_y + bob)
    return grid


def run_frame(legs, bob):
    """Feet stay on the anchor row; torso and head bob over them."""
    grid = blank()
    stamp(grid, RUN_TORSO, 0, 13 + bob)
    stamp(grid, legs[:8], 0, 22)
    stamp(grid, HEAD, 10, 4 + bob)
    return grid


def idle_bob():
    """Idle cel 2: upper body sinks one pixel, feet stay planted."""
    head_x, head_y, rows = IDLE
    grid = blank()
    stamp(grid, rows[10:], 0, 22)
    stamp(grid, rows[:10], 0, 13)
    stamp(grid, HEAD, head_x, head_y + 1)
    return grid


FRAMES = [
    body_frame(IDLE),                 # 0 idle 0
    idle_bob(),                       # 1 idle 1
    run_frame(RUN_LEGS[0], 0),        # 2 run 0
    run_frame(RUN_LEGS[1], 1),        # 3 run 1
    run_frame(RUN_LEGS[2], 0),        # 4 run 2
    run_frame(RUN_LEGS[3], 1),        # 5 run 3
    body_frame(WINDUP),               # 6 windup
    body_frame(SWING),                # 7 swing (active)
    body_frame(FOLLOW),               # 8 follow-through (recovery)
    body_frame(TOSS),                 # 9 serve toss
    body_frame(CHEER),                # 10 cheer
]

# 8x8 small objects, palette roles: 1 outline, 2 ball, 3 ball light,
# 4 shadow, 5 spark, 6 spark core.
OBJECTS = [
    [  # 0 ball
        "........",
        "..OOOO..",
        ".OLLBBO.",
        ".OLBBBO.",
        ".OBBBBO.",
        ".OBBBBO.",
        "..OOOO..",
        "........",
    ],
    [  # 1 ball shadow
        "........",
        "........",
        "........",
        "..XXXX..",
        ".XXXXXX.",
        "..XXXX..",
        "........",
        "........",
    ],
    [  # 2 player shadow left half (right half is the h-flip)
        "........",
        "........",
        "....XXXX",
        "..XXXXXX",
        ".XXXXXXX",
        "..XXXXXX",
        "....XXXX",
        "........",
    ],
    [  # 3 spark
        "...*....",
        "...*..*.",
        "*..#.*..",
        ".*###...",
        "..###*.*",
        "*.*#....",
        "...*.*..",
        "..*.....",
    ],
    [  # 4 spark small
        "........",
        "...*....",
        "..*#*...",
        ".*###*..",
        "..*#*...",
        "...*....",
        "........",
        "........",
    ],
]
OBJ_KEY = {".": 0, "O": 1, "B": 2, "L": 3, "X": 4, "*": 5, "#": 6}
OBJ_PREVIEW = [(255, 0, 255), (40, 40, 16), (224, 232, 48), (255, 255, 200),
               (0, 0, 0), (255, 220, 64), (255, 255, 255)]


def save_indexed(path, pixels, palette):
    height, width = len(pixels), len(pixels[0])
    img = Image.new("P", (width, height))
    img.putdata([v for row in pixels for v in row])
    flat = []
    for color in palette:
        flat.extend(color)
    flat.extend([0] * (768 - len(flat)))
    img.putpalette(flat)
    img.save(path, optimize=False)


def main():
    sheet = [[0] * 128 for _ in range(96)]
    for i, frame in enumerate(FRAMES):
        ox, oy = (i % 4) * 32, (i // 4) * 32
        for y in range(32):
            for x in range(32):
                sheet[oy + y][ox + x] = frame[y][x]
    save_indexed(ROOT / "assets/source/player_sprites.png", sheet, PREVIEW)

    objects = [[0] * 128 for _ in range(8)]
    for i, rows in enumerate(OBJECTS):
        for y, row in enumerate(rows):
            for x, ch in enumerate(row):
                objects[y][i * 8 + x] = OBJ_KEY[ch]
    save_indexed(ROOT / "assets/source/objects.png", objects, OBJ_PREVIEW)
    print("wrote assets/source/player_sprites.png and objects.png")


if __name__ == "__main__":
    main()
