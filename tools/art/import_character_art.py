#!/usr/bin/env python3
"""Authoring tool: the athletic player models from art/characters (ADR-078).

NOT part of the normal build; run by hand when the art changes. Standard
library only. Reads the text grids in art/characters/sprites/ (one
character per pixel: '.' transparent, then the palette contract v2 roles
as hex digits, ADR-013) and writes into the committed sources:

  assets/source/player_sprites.png  every body frame at its manifest slot,
      and the hats given as {"sheet": k} in the hat_slot (ADR-060); the
      BIG BALL, the pickle, and the sheet objects are kept.
  assets/source/objects.png         the ponytail cell, and the hats given
      as objects.png cells.
  assets/manifest/player_frames.json  each frame's accessory (ponytail)
      offset and the hat_offset, from the head boxes in heads.json.

It replaces import_kit_sprites.py and the body frames of
import_wishlist.py, which would put the old big-headed art back.

Usage: python tools/art/import_character_art.py
"""
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "tools" / "art"))
from assetgen.common import read_png  # noqa: E402
from import_kit_sprites import write_png  # noqa: E402

ART = ROOT / "art" / "characters" / "sprites"
DIGITS = ".123456789ABCDEF"
# The head box is 7 wide; the ponytail's 8x8 cell and the 16x8 hat cell
# sit at fixed places around it (heads.json).
PONY_FROM_HEAD = (-5, 1)
HAT_FROM_HEAD = (-4, -4)


def read_grid(path, width, height):
    rows = path.read_text("utf-8").split()
    if len(rows) != height or any(len(r) != width for r in rows):
        raise SystemExit(f"{path.name}: not {width}x{height}")
    return [[DIGITS.index(ch) for ch in row] for row in rows]


def main():
    manifest_path = ROOT / "assets" / "manifest" / "player_frames.json"
    manifest = json.loads(manifest_path.read_text("utf-8"))
    heads = json.loads((ART / "heads.json").read_text("utf-8"))["heads"]

    sheet_path = ROOT / "assets" / "source" / "player_sprites.png"
    _, _, sheet, palette = read_png(sheet_path)
    sheet = [list(r) for r in sheet]
    for name, info in manifest["frames"].items():
        grid = read_grid(ART / "frames" / f"{name}.txt", 32, 32)
        slot = info["slot"]
        x0, y0 = (slot % 4) * 32, (slot // 4) * 32
        for y in range(32):
            sheet[y0 + y][x0:x0 + 32] = grid[y]
        top, left = heads[name]
        info["accessory"] = [left + PONY_FROM_HEAD[0], top + PONY_FROM_HEAD[1]]
    manifest["hat_offset"] = [HAT_FROM_HEAD[0] - PONY_FROM_HEAD[0],
                              HAT_FROM_HEAD[1] - PONY_FROM_HEAD[1]]

    objects_path = ROOT / "assets" / "source" / "objects.png"
    _, _, objects, opal = read_png(objects_path)
    objects = [list(r) for r in objects]

    def stamp(cell, width, pixels):
        for y in range(8):
            for x in range(width):
                cx = (cell + x // 8) % 16 * 8 + x % 8
                cy = (cell + x // 8) // 16 * 8 + y
                objects[cy][cx] = pixels[y][x]

    stamp(manifest["objects"]["ponytail"], 8,
          read_grid(ART / "ponytail.txt", 8, 8))
    slot = manifest["hat_slot"]
    for name, place in manifest["hats"].items():
        if place is None:
            continue
        hat = read_grid(ART / "hats" / f"{name}.txt", 16, 8)
        if isinstance(place, int):
            stamp(place, 16, hat)
            continue
        k = place["sheet"]
        x0 = (slot % 4) * 32 + (k % 2) * 16
        y0 = (slot // 4) * 32 + (k // 2) * 8
        for y in range(8):
            sheet[y0 + y][x0:x0 + 16] = hat[y]

    write_png(sheet_path, sheet, palette)
    write_png(objects_path, objects, opal)
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", "utf-8")
    print(f"wrote {sheet_path.relative_to(ROOT)}, "
          f"{objects_path.relative_to(ROOT)}, and "
          f"{manifest_path.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
