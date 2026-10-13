"""BG1 court scene: tiles, map, palettes, and the projection checks."""
import struct
import sys

from .common import (COURT_TILE_BUDGET, ROOT, AssetError, block, encode_tile,
                     paths, read_png, rgb8_to_15)

sys.path.insert(0, str(ROOT / "tools"))
import court_projection  # noqa: E402

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
    width, height, rows, palette = read_png(paths.src / "court_scene.png")
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
