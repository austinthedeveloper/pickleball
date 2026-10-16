"""BG1 court scene: tiles, map, palettes, and the projection checks."""
import json
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


GROUND_PALETTE = 2      # shared by every venue (court colours, ADR-022)
VENUE_NAME_CELLS = 8


def scene_tiles(name, rows):
    """Per 8x8 tile of a 256x224 scene: (palette, 4bpp tile bytes, flip
    variants). Validates the ADR-016 palette rules."""
    out = []
    for ty in range(28):
        for tx in range(32):
            px = block(rows, tx * 8, ty * 8)
            subs = {v // 16 for row in px for v in row}
            if len(subs) != 1:
                raise AssetError(f"{name} tile {tx},{ty} mixes palettes {subs}")
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
                (encode_tile(local, 4), 0),
                (encode_tile([row[::-1] for row in local], 4), 0x4000),
                (encode_tile(local[::-1], 4), 0x8000),
                (encode_tile([row[::-1] for row in local[::-1]], 4), 0xC000),
            )
            out.append((sub, variants))
    return out


class TileSet:
    """Deduplicated tiles (with flips); entries index from `base`."""

    def __init__(self, base=0):
        self.base = base
        self.tiles = []
        self.lookup = {}

    def entry(self, variants, add=True):
        for key, flip in variants:
            if key in self.lookup:
                return (self.base + self.lookup[key]) | flip
        if not add:
            return None
        key = variants[0][0]
        self.lookup[key] = len(self.tiles)
        self.tiles.append(key)
        return self.base + len(self.tiles) - 1


def court_assets():
    """Venues (ADR-024): shared ground tiles and palette 2, plus each venue's
    backdrop tiles, map, and palettes 3-7."""
    from .court_colors import load_court_colors
    spec = json.loads((paths.src / "venues.json").read_text("utf-8"))
    venues = spec["venues"]
    if not 1 <= len(venues) <= 9:
        raise AssetError("venues.json: 1..9 venues")
    color_parts, _ = load_court_colors()

    scenes = []
    for venue in venues:
        name = venue["name"]
        if not 1 <= len(name) <= VENUE_NAME_CELLS:
            raise AssetError(f"venue {name!r}: 1..{VENUE_NAME_CELLS} cells")
        width, height, rows, palette = read_png(paths.src / venue["scene"])
        if (width, height) != (256, 224):
            raise AssetError(f"{venue['scene']} must be 256x224")
        check_court_lines(rows)
        scenes.append((venue, rows, palette, scene_tiles(venue["scene"], rows)))

    # The ground palette is shared: every venue paints it identically.
    first = GROUND_PALETTE * 16
    base_palette = scenes[0][2]
    ground_colors = [tuple(base_palette[first + i]) for i in range(16)]
    for venue, _, palette, _ in scenes[1:]:
        if [tuple(palette[first + i]) for i in range(16)] != ground_colors:
            raise AssetError(f"venue {venue['name']}: palette 2 differs from "
                             f"{venues[0]['scene']} (it is shared)")

    ground = TileSet()
    for _, _, _, tiles in scenes:
        for sub, variants in tiles:
            if sub == GROUND_PALETTE:
                ground.entry(variants)

    files = {"court.pic": b"".join(ground.tiles),
             "court.pal": struct.pack("<16H", *[rgb8_to_15(c)
                                               for c in ground_colors])}
    most = 0
    header_rows = []
    for i, (venue, _, palette, tiles) in enumerate(scenes):
        own = TileSet(base=len(ground.tiles))
        tilemap = []
        for sub, variants in tiles:
            if sub == GROUND_PALETTE:
                entry = ground.entry(variants, add=False)
            else:
                entry = own.entry(variants)
            tilemap.append(entry | (sub << 10))
        total = len(ground.tiles) + len(own.tiles)
        if total > COURT_TILE_BUDGET:
            raise AssetError(f"venue {venue['name']} uses {total} tiles > "
                             f"{COURT_TILE_BUDGET}")
        most = max(most, total)
        pal = [rgb8_to_15(palette[slot]) if slot < len(palette) else 0
               for slot in range(48, 128)]
        files[f"venue_{i}.pic"] = b"".join(own.tiles)
        files[f"venue_{i}.map"] = struct.pack("<2048H", *camera_map(tilemap))
        files[f"venue_{i}.pal"] = struct.pack("<80H", *pal)
        defaults = []
        for part, _, presets in color_parts:
            names = [n for n, _ in presets]
            want = venue["ground"][part]
            if want not in names:
                raise AssetError(f"venue {venue['name']}: {part} {want!r} is "
                                 f"not a court colour preset")
            defaults.append(names.index(want))
        watcher = venue.get("watcher")
        if not (isinstance(watcher, list) and len(watcher) == 3 and
                8 <= watcher[0] < 248 and 16 <= watcher[1] < 224 and
                watcher[2] in ("sit", "stand")):
            raise AssetError(f"venue {venue['name']}: watcher is [x, y, "
                             "sit|stand] on screen (ADR-042)")
        header_rows.append((venue["name"], defaults,
                            len(files[f"venue_{i}.pic"]),
                            len(files[f"venue_{i}.map"]),
                            (watcher[0], watcher[1],
                             1 if watcher[2] == "sit" else 0)))

    files["venues.h"] = venues_header(header_rows).encode()
    files["venues.inc"] = venues_include(len(venues)).encode()
    return files, most, len(ground.tiles)


H_FLIP = 0x4000
V_FLIP = 0x8000


def camera_map(tilemap):
    """A 64x32 BG1 map (two 32x32 screens) for the camera (ADR-050).

    The 32x28 scene is screen 0. The camera pans up to a tile past each
    edge, so the scene's edge column or row is mirrored once beyond it by
    the map's flip bits (no new tiles): column 32 (screen 1, column 0) and
    column -1 (column 63, where a negative scroll wraps), row 28 below and
    row -1 (row 31) above."""
    rows, cols = 28, 32
    out = [0] * 2048

    def put(col, row, entry):
        col %= 64
        row %= 32
        base = 1024 if col >= 32 else 0
        out[base + row * 32 + col % 32] = entry

    def at(col, row):
        return tilemap[row * cols + col]

    for row in range(-1, rows + 1):
        src_row = min(max(row, 0), rows - 1)
        vflip = V_FLIP if row != src_row else 0
        for col in range(-1, cols + 1):
            src_col = min(max(col, 0), cols - 1)
            hflip = H_FLIP if col != src_col else 0
            put(col, row, at(src_col, src_row) ^ hflip ^ vflip)
    return out


def venues_header(rows):
    lines = ["/* Generated by tools/generate_assets.py from "
             "assets/source/venues.json; do not edit. */",
             "#ifndef VENUES_H", "#define VENUES_H", "",
             "#include \"game_types.h\"", "#include \"court_colors.h\"", "",
             f"#define VENUE_COUNT {len(rows)}", "",
             "/* One venue's BG1 data in ROM (venues.inc). */",
             "typedef struct {",
             "    const u8 *tiles;          /* after the shared ground tiles */",
             "    u16 tiles_size;",
             "    const u8 *map;            /* 32x28 entries */",
             "    u16 map_size;",
             "    const u8 *palettes;       /* BG1 palettes 3-7 */",
             "} VenueData;", "",
             "/* The ROM data table refers to venues.inc labels: defined only in",
             " * the SNES renderer. */",
             "#ifdef VENUE_DATA_DEFINE"]
    for i in range(len(rows)):
        lines.append(f"extern char venue{i}_tiles, venue{i}_map, "
                     f"venue{i}_palettes;")
    lines.append("const VenueData g_venue_data[VENUE_COUNT] = {")
    for i, (_, _, tiles_size, map_size, _) in enumerate(rows):
        lines.append(f"    {{ (const u8 *)&venue{i}_tiles, {tiles_size}, "
                     f"(const u8 *)&venue{i}_map, {map_size}, "
                     f"(const u8 *)&venue{i}_palettes }},")
    lines.append("};")
    lines += ["#else", "extern const VenueData g_venue_data[VENUE_COUNT];",
              "#endif", "", "#ifdef VENUES_DEFINE"]
    lines.append("const char *const g_venue_names[VENUE_COUNT] = { " +
                 ", ".join(f'"{r[0]}"' for r in rows) + " };")
    lines.append("/* Court colour preset a DEFAULT choice means here. */")
    lines.append("const u8 g_venue_ground[VENUE_COUNT][COURT_PARTS] = { " +
                 ", ".join("{ " + ", ".join(map(str, r[1])) + " }"
                           for r in rows) + " };")
    lines.append("/* Where the opponent's partner watches a singles match "
                 "(ADR-042): x, y of the feet, and 1 to sit. */")
    lines.append("const u8 g_venue_watcher[VENUE_COUNT][3] = { " +
                 ", ".join("{ %d, %d, %d }" % r[4] for r in rows) + " };")
    lines += ["#else",
              "extern const char *const g_venue_names[VENUE_COUNT];",
              "extern const u8 g_venue_ground[VENUE_COUNT][COURT_PARTS];",
              "extern const u8 g_venue_watcher[VENUE_COUNT][3];",
              "#endif", "", "#endif", ""]
    return "\n".join(lines)


def venues_include(count):
    """One ROM section per venue (each fits a 32 KiB bank)."""
    lines = ["; Generated by tools/generate_assets.py; do not edit."]
    for i in range(count):
        lines += [f".section \".rodata_venue{i}\" superfree",
                  f"venue{i}_tiles:",
                  f".incbin \"assets/generated/venue_{i}.pic\"",
                  f"venue{i}_map:",
                  f".incbin \"assets/generated/venue_{i}.map\"",
                  f"venue{i}_palettes:",
                  f".incbin \"assets/generated/venue_{i}.pal\"",
                  ".ends", ""]
    return "\n".join(lines)
