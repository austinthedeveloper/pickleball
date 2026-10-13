#!/usr/bin/env python3
"""Deterministic, stdlib-only asset build.

Inputs (authored, committed):
  assets/source/court_scene.png    256x224 indexed; palette slots 32..127 are
                                   BG palettes 2..7, one sub-palette per tile.
  assets/source/player_sprites.png 128x96 indexed, 32x32 frame slots.
  assets/source/objects.png        128x8 indexed, 8x8 small objects.
  assets/manifest/player_frames.json  frame order/names and team palettes.

Outputs (disposable, assets/generated/): court.pic/.map/.pal, hud.pic/.pal,
obj.pic, objects.pal, asset_frames.h, hud_tiles.h, palette_parts.h,
court_projection.h, sizes.json.

Validation (fails the build): tile budgets, frame anchors and palette roles,
BG1 never using 4bpp palette 0 (ADR-016), and PROJ-01..04 (ADR-011).
"""
import argparse
import hashlib
import json
import struct
import zlib
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import court_projection  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "assets" / "source"
OUT = ROOT / "assets" / "generated"
MANIFEST = ROOT / "assets" / "manifest" / "player_frames.json"
REPORT = OUT / "sizes.json"

COURT_TILE_BUDGET = 896      # BG1 tiles at VRAM word 0x1000..0x47ff
HUD_TILE_BUDGET = 512        # BG3 tiles at VRAM word 0x5000..0x5fff
OBJ_TILE_BUDGET = 256        # first OBJ name table at VRAM word 0x6000


class AssetError(SystemExit):
    pass


def read_png(path):
    """Minimal indexed-PNG reader (color type 3, bit depth 1/2/4/8)."""
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise AssetError(f"{path}: not a PNG")
    pos, idat, palette = 8, b"", []
    width = height = depth = ctype = None
    while pos < len(data):
        length, kind = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if kind == b"IHDR":
            width, height, depth, ctype, _, _, interlace = \
                struct.unpack(">IIBBBBB", body)
            if ctype != 3 or interlace:
                raise AssetError(f"{path}: must be non-interlaced indexed PNG")
        elif kind == b"PLTE":
            palette = [tuple(body[i:i + 3]) for i in range(0, len(body), 3)]
        elif kind == b"IDAT":
            idat += body
        elif kind == b"IEND":
            break
    raw = zlib.decompress(idat)
    stride = (width * depth + 7) // 8
    bpp = max(1, depth // 8)
    rows, prev, i = [], bytearray(stride), 0
    for _ in range(height):
        ftype = raw[i]
        line = bytearray(raw[i + 1:i + 1 + stride])
        i += 1 + stride
        for x in range(stride):
            a = line[x - bpp] if x >= bpp else 0
            b = prev[x]
            c = prev[x - bpp] if x >= bpp else 0
            if ftype == 1:
                line[x] = (line[x] + a) & 255
            elif ftype == 2:
                line[x] = (line[x] + b) & 255
            elif ftype == 3:
                line[x] = (line[x] + (a + b) // 2) & 255
            elif ftype == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pred = a if pa <= pb and pa <= pc else (b if pb <= pc else c)
                line[x] = (line[x] + pred) & 255
        prev = line
        if depth == 8:
            rows.append(list(line[:width]))
        else:
            per = 8 // depth
            mask = (1 << depth) - 1
            row = []
            for x in range(width):
                byte = line[x // per]
                shift = 8 - depth * (x % per + 1)
                row.append((byte >> shift) & mask)
            rows.append(row)
    return width, height, rows, palette


def bgr15(r, g, b):
    return (r & 31) | ((g & 31) << 5) | ((b & 31) << 10)


def rgb8_to_15(color):
    r, g, b = color
    return bgr15(round(r * 31 / 255), round(g * 31 / 255), round(b * 31 / 255))


def encode_tile(pixels, bpp):
    data = bytearray()
    for plane_pair in range(0, bpp, 2):
        for row in pixels:
            low = high = 0
            for x, value in enumerate(row):
                low |= ((value >> plane_pair) & 1) << (7 - x)
                high |= ((value >> (plane_pair + 1)) & 1) << (7 - x)
            data.extend((low, high))
    return bytes(data)


def block(rows, x0, y0, w=8, h=8):
    return [rows[y][x0:x0 + w] for y in range(y0, y0 + h)]


# --------------------------------------------------------------------------
# BG1 court scene
# --------------------------------------------------------------------------

LINE_COLORS = {32 + 6, 32 + 7}   # court palette: line, line in shadow


def check_court_lines(rows):
    """PROJ-01..03 on the table and PROJ-04 on the painted art."""
    c = court_projection.court_constants()
    table = court_projection.scale_table(c)
    errors = court_projection.check(table, c)
    top, bottom = c["COURT_SIDELINE_TOP"], c["COURT_SIDELINE_BOTTOM"]
    left, right = c["COURT_BASELINE_LEFT"], c["COURT_BASELINE_RIGHT"]
    for xw in (left, right - 1, c["COURT_KITCHEN_LEFT_X"],
               c["COURT_KITCHEN_RIGHT_X"] - 1):
        for y in range(top, bottom + 1):
            sx = court_projection.project_x(xw, y, table)
            if rows[y][sx] not in LINE_COLORS:
                errors.append(f"PROJ-04: line x={xw} missing at row {y} "
                              f"(screen x {sx})")
    for y in (top, bottom):
        for xw in range(left, right + 1, 4):
            sx = court_projection.project_x(xw, y, table)
            if abs(sx - c["COURT_NET_X"]) <= 5:
                continue   # the net posts are painted over the sideline
            if rows[y][sx] not in LINE_COLORS:
                errors.append(f"PROJ-04: sideline y={y} missing at x {sx}")
    if errors:
        raise AssetError("court projection check failed:\n  " +
                         "\n  ".join(errors[:10]))


def court_assets():
    width, height, rows, palette = read_png(SRC / "court_scene.png")
    if (width, height) != (256, 224):
        raise AssetError("court_scene.png must be 256x224")
    tiles, lookup, tilemap = [], {}, []
    for ty in range(32):
        for tx in range(32):
            if ty >= 28:
                tilemap.append(0)
                continue
            px = block(rows, tx * 8, ty * 8)
            subs = {v // 16 for row in px for v in row}
            if len(subs) != 1:
                raise AssetError(
                    f"court_scene.png tile {tx},{ty} mixes palettes {subs}")
            sub = subs.pop()
            if sub == 0:
                raise AssetError(f"court tile {tx},{ty} uses 4bpp palette 0, "
                                 "which BG3 HUD palettes overlap (ADR-016)")
            if not 2 <= sub <= 7:
                raise AssetError(f"court tile {tx},{ty} uses palette {sub}")
            if any(v % 16 == 0 for row in px for v in row):
                raise AssetError(f"court tile {tx},{ty} uses color 0")
            local = [[v % 16 for v in row] for row in px]
            variants = (
                (local, 0),
                ([row[::-1] for row in local], 0x4000),
                (local[::-1], 0x8000),
                ([row[::-1] for row in local[::-1]], 0xC000),
            )
            entry = None
            for candidate, flip in variants:
                key = encode_tile(candidate, 4)
                if key in lookup:
                    entry = lookup[key] | flip
                    break
            if entry is None:
                key = encode_tile(local, 4)
                lookup[key] = len(tiles)
                entry = len(tiles)
                tiles.append(key)
            tilemap.append(entry | (sub << 10))
    check_court_lines(rows)
    if len(tiles) > COURT_TILE_BUDGET:
        raise AssetError(f"court uses {len(tiles)} tiles > {COURT_TILE_BUDGET}")
    pal = []
    for slot in range(32, 128):
        pal.append(rgb8_to_15(palette[slot]) if slot < len(palette) else 0)
    return b"".join(tiles), struct.pack("<1024H", *tilemap), \
        struct.pack("<96H", *pal), len(tiles)


# --------------------------------------------------------------------------
# BG3 HUD: chunky font, panel borders, big title glyphs
# --------------------------------------------------------------------------

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


# --------------------------------------------------------------------------
# OBJ sheet and palettes
# --------------------------------------------------------------------------

PALETTE_DIR = SRC / "palettes"


def load_part(name):
    return json.loads((PALETTE_DIR / f"{name}.json").read_text("utf-8"))


def palette_parts_header(colors):
    """Contract v2 parts for runtime composition (ADR-013)."""
    shared = load_part("shared")
    skin = load_part("skin_light")
    lines = ["/* Generated by tools/generate_assets.py from "
             "assets/source/palettes; do not edit. */",
             "#ifndef PALETTE_PARTS_H", "#define PALETTE_PARTS_H", "",
             "#include \"game_types.h\"", ""]
    for i, name in enumerate(colors):
        lines.append(f"#define PAL_{name.upper()} {i}")
    lines += [f"#define PAL_COUNT {len(colors)}", "#define SKIN_COUNT 1",
              "#define PADDLE_COUNT PAL_COUNT", "",
              "#ifdef PALETTE_PARTS_DEFINE"]

    def arr(name, rows):
        body = ",\n    ".join("{ " + ", ".join(str(bgr15(*c)) for c in row)
                               + " }" for row in rows)
        return f"const u16 {name}[{len(rows)}][{len(rows[0])}] = {{\n    {body}\n}};"
    teams = [load_part(f"team_{n}") for n in colors]
    paddles = [load_part(f"paddle_{n}") for n in colors]
    lines.append(arr("g_pal_shared", [[shared["outline"], shared["highlight"]]]))
    lines.append(arr("g_pal_skin", [skin["skin"] + [skin["hair"]]]))
    lines.append(arr("g_pal_team", [t["shirt"] + t["shorts"] + [t["shoe"]]
                                    for t in teams]))
    lines.append(arr("g_pal_paddle", [p["face"] + [p["edge"]] for p in paddles]))
    lines.append(arr("g_pal_hud", [t["hud"] for t in teams]))
    flash = [0] + [bgr15(31, 31, 24)] * 15
    flash[1] = bgr15(31, 24, 8)
    lines.append("const u16 g_pal_flash[16] = { " +
                 ", ".join(str(v) for v in flash) + " };")
    lines += ["#else",
              "extern const u16 g_pal_shared[1][2];",
              "extern const u16 g_pal_skin[SKIN_COUNT][4];",
              "extern const u16 g_pal_team[PAL_COUNT][6];",
              "extern const u16 g_pal_paddle[PADDLE_COUNT][3];",
              "extern const u16 g_pal_hud[PAL_COUNT][2];",
              "extern const u16 g_pal_flash[16];",
              "#endif", "", "#endif", ""]
    return "\n".join(lines)


# Contract v2: every index 1..15 is a declared role (ADR-013).
V2_ROLES = set(range(1, 16))

OBJECT_PALETTE = [0, bgr15(6, 6, 2), bgr15(28, 29, 6), bgr15(31, 31, 24),
                  bgr15(0, 0, 0), bgr15(31, 27, 8), bgr15(31, 31, 31)] + [0] * 9
# Shadows draw with OBJ palette 4 so color math halves the court behind them.
SHADOW_PALETTE = [0, 0, 0, 0, bgr15(0, 0, 0)] + [0] * 11


def obj_assets(manifest):
    width, height, rows, _ = read_png(SRC / "player_sprites.png")
    if width != 128 or height % 32:
        raise AssetError("player_sprites.png must be 128 wide, rows of 32")
    frames = manifest["frames"]
    slots = (width // 32) * (height // 32)
    if len(frames) > slots:
        raise AssetError("more manifest frames than sheet slots")
    for name, info in frames.items():
        slot = info["slot"]
        x0, y0 = (slot % 4) * 32, (slot // 4) * 32
        px = block(rows, x0, y0, 32, 32)
        if any(v > 15 for row in px for v in row):
            raise AssetError(f"frame {name} uses color index > 15")
        if not any(v for row in px for v in row):
            raise AssetError(f"frame {name} is empty")
        foot = max(y for y in range(32) if any(px[y]))
        if foot != manifest["foot_row"]:
            raise AssetError(f"frame {name} foot row {foot} != "
                             f"{manifest['foot_row']}")
    ow, oh, orows, _ = read_png(SRC / "objects.png")
    if (ow, oh) != (128, 8):
        raise AssetError("objects.png must be 128x8")
    tiles = []
    for ty in range(height // 8):
        for tx in range(16):
            tiles.append(encode_tile(block(rows, tx * 8, ty * 8), 4))
    object_base = len(tiles)
    for tx in range(16):
        tiles.append(encode_tile(block(orows, tx * 8, 0), 4))
    if len(tiles) > OBJ_TILE_BUDGET:
        raise AssetError(f"OBJ uses {len(tiles)} tiles > {OBJ_TILE_BUDGET}")
    for name, info in frames.items():
        slot = info["slot"]
        x0, y0 = (slot % 4) * 32, (slot // 4) * 32
        used = {v for row in block(rows, x0, y0, 32, 32) for v in row} - {0}
        undeclared = used - V2_ROLES
        if undeclared:
            raise AssetError(f"frame {name} uses undeclared palette roles "
                             f"{sorted(undeclared)}")
    obj_pal = OBJECT_PALETTE + SHADOW_PALETTE
    header = ["/* Generated by tools/generate_assets.py; do not edit. */",
              "#ifndef ASSET_FRAMES_H", "#define ASSET_FRAMES_H", ""]
    for name, info in frames.items():
        slot = info["slot"]
        tile = (slot // 4) * 64 + (slot % 4) * 4
        header.append(f"#define FRAME_{name.upper()} {tile}")
    for name, index in manifest["objects"].items():
        header.append(f"#define OBJTILE_{name.upper()} {object_base + index}")
    header += ["", "#endif", ""]
    return b"".join(tiles), struct.pack(f"<{len(obj_pal)}H", *obj_pal), \
        "\n".join(header), len(tiles)


def generated_files():
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    court_pic, court_map, court_pal, court_tiles = court_assets()
    hud_pic, hud_pal, big_chars, hud_tiles = hud_assets()
    obj_pic, objects_pal, header, obj_tiles = obj_assets(manifest)
    projection = court_projection.header(court_projection.scale_table())
    hud_header = [
        "/* Generated by tools/generate_assets.py; do not edit. */",
        "#ifndef HUD_TILES_H", "#define HUD_TILES_H",
        f"#define HUD_TILE_CLEAR {HUD_TILE_CLEAR}",
        f"#define HUD_TILE_PANEL_TL {HUD_TILE_PANEL_TL}",
        f"#define HUD_TILE_PANEL_T {HUD_TILE_PANEL_T}",
        f"#define HUD_TILE_PANEL_L {HUD_TILE_PANEL_L}",
        f"#define HUD_TILE_BUTTON_B {HUD_TILE_BUTTON_B}",
        f"#define HUD_TILE_CURSOR {HUD_TILE_CURSOR}",
        f"#define HUD_TILE_BAR_ON {HUD_TILE_BAR_ON}",
        f"#define HUD_TILE_BAR_OFF {HUD_TILE_BAR_OFF}",
        f"#define HUD_TILE_BALL_ICON {HUD_TILE_BALL_ICON}",
        f"#define HUD_TILE_BIG_BASE {HUD_TILE_BIG_BASE}",
        f"#define HUD_BIG_CHARS \"{big_chars}\"",
        "#endif", ""]
    files = {
        "court.pic": court_pic,
        "court.map": court_map,
        "court.pal": court_pal,
        "hud.pic": hud_pic,
        "hud.pal": hud_pal,
        "obj.pic": obj_pic,
        "objects.pal": objects_pal,
        "asset_frames.h": header.encode(),
        "hud_tiles.h": "\n".join(hud_header).encode(),
        "palette_parts.h": palette_parts_header(manifest["palettes"]).encode(),
        "court_projection.h": projection.encode(),
    }
    budget = {"court_tiles": court_tiles, "hud_tiles": hud_tiles,
              "obj_tiles": obj_tiles}
    return files, budget


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    expected, budget = generated_files()
    OUT.mkdir(parents=True, exist_ok=True)
    if args.check:
        failures = [name for name, data in expected.items()
                    if not (OUT / name).exists() or
                    (OUT / name).read_bytes() != data]
        if failures:
            raise SystemExit("asset validation failed: " + ", ".join(failures))
        print("asset validation passed")
        return
    for name, data in expected.items():
        path = OUT / name
        if not path.exists() or path.read_bytes() != data:
            path.write_bytes(data)
    report = {name: {"bytes": len(data),
                     "sha256": hashlib.sha256(data).hexdigest()}
              for name, data in expected.items()}
    report["_budget"] = budget
    REPORT.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n",
                      encoding="utf-8")
    print("generated assets: court %(court_tiles)d tiles, hud %(hud_tiles)d "
          "tiles, obj %(obj_tiles)d tiles" % budget)


if __name__ == "__main__":
    main()
