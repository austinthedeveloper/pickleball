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


def select_layout():
    text = (ROOT / "include" / "select_layout.h").read_text("utf-8")
    return {k: int(v) for k, v in re.findall(r"#define (SEL_\w+) (\d+)", text)}


def indexed_scene(name, rows, palette, budget, first_sub=1, last_sub=6):
    """A 256x224 indexed picture as deduplicated BG tiles (flips included),
    a 32x32 map, and BG palettes first_sub..last_sub (slots 16..111 for
    1-6). Each tile uses one sub-palette (index // 16)."""
    if len(rows) != 224 or any(len(row) != 256 for row in rows):
        raise AssetError(f"{name} must be 256x224")
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
                    f"{name} tile {tx},{ty} mixes palettes {subs}")
            sub = subs.pop()
            if not first_sub <= sub <= last_sub:
                raise AssetError(f"{name} tile {tx},{ty} uses palette {sub}; "
                                 f"BG1 may use {first_sub}-{last_sub} (0 is "
                                 "the HUD's)")
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
    return b"".join(tiles), struct.pack("<1024H", *tilemap), \
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
