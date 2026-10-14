"""Player Select art (ADR-020): BG1 scene and BG2 portraits."""
import re
import struct

from .common import (ROOT, AssetError, block, encode_tile, paths, read_png,
                     rgb8_to_15)

SELECT_TILE_BUDGET = 768     # BG1 tiles at VRAM word 0x1000..0x3fff
PORTRAIT_TILE_BUDGET = 256   # BG2 tiles at VRAM word 0x4000..0x4fff
PORTRAIT_CELL = 96
PORTRAIT_PALETTE = 7
BG_PRIORITY = 0x2000
BUST_TILE_BUDGET = 128       # BG2 tiles at VRAM word 0x4800..0x4fff (match)
BUST_PALETTE = 1             # the one BG palette a match leaves free
# BG2's tile base steps by 0x1000 words: the map counts from 0x4000, where
# 128 4bpp tiles in, at 0x4800, the busts start (below 0x4800 is BG1's).
BUST_TILE_INDEX = 128
# Roles the bust art must not use: the shorts and paddle face don't follow
# the captain's look (prep_opponent_busts.py remaps them).
BUST_FORBIDDEN_ROLES = (8, 9, 11, 12)


def select_layout():
    text = (ROOT / "include" / "select_layout.h").read_text("utf-8")
    return {k: int(v) for k, v in re.findall(r"#define (SEL_\w+) (\d+)", text)}


def indexed_scene(name, rows, palette, budget, first_sub=1, last_sub=6):
    """A 256x224 (or 512x224: two screens, a 64x32 map) indexed picture as
    deduplicated BG tiles (flips included), a 32x32 map per screen, and BG
    palettes first_sub..last_sub (slots 16..111 for 1-6). Each tile uses
    one sub-palette (index // 16)."""
    screens = len(rows[0]) // 256 if rows else 0
    if len(rows) != 224 or screens not in (1, 2) or \
            any(len(row) != 256 * screens for row in rows):
        raise AssetError(f"{name} must be 256x224 or 512x224")
    tiles, lookup, tilemap = [], {}, []
    for screen in range(screens):
        for ty in range(32):
            for tx in range(32):
                if ty >= 28:
                    tilemap.append(0)
                    continue
                px = block(rows, screen * 256 + tx * 8, ty * 8)
                subs = {v // 16 for row in px for v in row}
                if len(subs) != 1:
                    raise AssetError(
                        f"{name} tile {tx},{ty} mixes palettes {subs}")
                sub = subs.pop()
                if not first_sub <= sub <= last_sub:
                    raise AssetError(
                        f"{name} tile {tx},{ty} uses palette {sub}; BG1 "
                        f"may use {first_sub}-{last_sub} (0 is the HUD's)")
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
    if len(tiles) > budget:
        raise AssetError(f"{name} uses {len(tiles)} tiles > {budget}")
    count = (last_sub - first_sub + 1) * 16
    pal = [rgb8_to_15(palette[slot]) if slot < len(palette) else 0
           for slot in range(first_sub * 16, first_sub * 16 + count)]
    return b"".join(tiles), struct.pack(f"<{len(tilemap)}H", *tilemap), \
        struct.pack(f"<{count}H", *pal), len(tiles)


def scene_assets():
    width, height, rows, palette = read_png(paths.src / "select_scene.png")
    if (width, height) != (256, 224):
        raise AssetError("select_scene.png must be 256x224")
    return indexed_scene("select_scene.png", rows, palette,
                         SELECT_TILE_BUDGET)


def portrait_assets():
    width, height, rows, _ = read_png(paths.src / "select_portraits.png")
    cell = PORTRAIT_CELL
    if (width, height) != (cell * 2, cell):
        raise AssetError("select_portraits.png must be 192x96 "
                         "(male, female)")
    lay = select_layout()
    col0, row0 = lay["SEL_PORTRAIT_COL"], lay["SEL_PORTRAIT_ROW"]
    if col0 + cell // 8 > 32 or row0 + cell // 8 > 28:
        raise AssetError("portrait cell does not fit the screen")
    blank = encode_tile([[0] * 8 for _ in range(8)], 4)
    tiles, lookup, maps = [blank], {blank: 0}, []
    for variant in range(2):
        tilemap = [0] * 1024
        for ty in range(cell // 8):
            for tx in range(cell // 8):
                px = block(rows, variant * cell + tx * 8, ty * 8)
                if any(v > 15 for row in px for v in row):
                    raise AssetError("portrait uses colour index > 15")
                key = encode_tile(px, 4)
                if key not in lookup:
                    lookup[key] = len(tiles)
                    tiles.append(key)
                index = lookup[key]
                if index:
                    tilemap[(row0 + ty) * 32 + col0 + tx] = (
                        index | (PORTRAIT_PALETTE << 10) | BG_PRIORITY)
        maps.append(struct.pack("<1024H", *tilemap))
    if len(tiles) > PORTRAIT_TILE_BUDGET:
        raise AssetError(f"portraits use {len(tiles)} tiles > "
                         f"{PORTRAIT_TILE_BUDGET}")
    return b"".join(tiles), maps[0], maps[1], len(tiles)


def story_layout():
    text = (ROOT / "include" / "story.h").read_text("utf-8")
    return {k: int(v) for k, v in
            re.findall(r"#define (STORY_BUST_\w+) (\d+)", text)}


def bust_assets():
    """The intro card's opponent busts (M4-STORY-03): male and female
    framed cells as BG2 tiles and one 32x32 map per variant, in BG palette
    1 with priority (over the court and the players, under the HUD). Every
    other map cell is the busts' own blank tile, never a tile of BG1's."""
    width, height, rows, _ = read_png(paths.src / "opponent_busts.png")
    lay = story_layout()
    tiles_wide = lay["STORY_BUST_TILES"]
    cell = tiles_wide * 8
    if (width, height) != (cell * 2, cell):
        raise AssetError(f"opponent_busts.png must be {cell * 2}x{cell} "
                         "(male, female)")
    col0, row0 = lay["STORY_BUST_COL"], lay["STORY_BUST_ROW"]
    if col0 + tiles_wide > 32 or row0 + tiles_wide > 28:
        raise AssetError("bust cell does not fit the screen")
    blank = encode_tile([[0] * 8 for _ in range(8)], 4)
    tiles, lookup, maps = [blank], {blank: 0}, []
    for variant in range(2):
        tilemap = [BUST_TILE_INDEX] * 1024
        for ty in range(tiles_wide):
            for tx in range(tiles_wide):
                px = block(rows, variant * cell + tx * 8, ty * 8)
                if any(v > 15 for row in px for v in row):
                    raise AssetError("bust uses colour index > 15")
                used = {v for row in px for v in row}
                bad = used.intersection(BUST_FORBIDDEN_ROLES)
                if bad:
                    raise AssetError(f"bust uses role {min(bad)} "
                                     "(shorts/paddle face)")
                key = encode_tile(px, 4)
                if key not in lookup:
                    lookup[key] = len(tiles)
                    tiles.append(key)
                tilemap[(row0 + ty) * 32 + col0 + tx] = (
                    (BUST_TILE_INDEX + lookup[key]) | (BUST_PALETTE << 10) |
                    BG_PRIORITY)
        maps.append(struct.pack("<1024H", *tilemap))
    if len(tiles) > BUST_TILE_BUDGET:
        raise AssetError(f"busts use {len(tiles)} tiles > {BUST_TILE_BUDGET}")
    return b"".join(tiles), maps[0], maps[1], len(tiles)
