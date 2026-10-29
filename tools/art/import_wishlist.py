#!/usr/bin/env python3
"""Authoring tool: the sprite wish list's art into the player sheet (ADR-064).

NOT part of the normal build; run by hand when a draft in
assets/source/wishlist/ changes (after tools/art/wishlist_all.sh, which
writes the drafts' .png from their grids or .aseprite). Standard library
only. Writes assets/source/player_sprites.png, keeping every other slot:

  frames         the body frames named in WISHLIST_FRAMES, at their
                 manifest slots;
  sheet_objects  the 8x8 and 16x16 cells, from the strips in WISHLIST_CELLS
                 (cell k of a strip is its k-th 8x8 or 16x16 square);
  large_objects  the pickle, a whole 32x32 slot.

Indices are copied as they are: the body frames and the pickle use the
player roles (ADR-013), the cells the objects palette.

Usage: python tools/art/import_wishlist.py
"""
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "tools" / "art"))
from assetgen.common import read_png  # noqa: E402
from import_kit_sprites import write_png  # noqa: E402

WISH = ROOT / "assets" / "source" / "wishlist"
# The body frames come from art/characters since ADR-078
# (tools/art/import_character_art.py).
WISHLIST_FRAMES = []
WISHLIST_LARGE = ["pickle"]
# Sheet object -> (strip, square), squares of the object's own size.
WISHLIST_CELLS = {
    "emote_exclaim": ("emotes", 0), "emote_question": ("emotes", 1),
    "emote_sweat": ("emotes", 2), "emote_note": ("emotes", 3),
    "emote_star": ("emotes", 4), "emote_zzz": ("emotes", 5),
    "emote_anger": ("emotes", 6), "dust0": ("emotes", 7),
    "dust1": ("emotes", 8),
    "cord0": ("ball_fx", 0), "cord1": ("ball_fx", 1),
    "spin0": ("ball_fx", 2), "spin1": ("ball_fx", 3),
    "squash": ("ball_fx", 4), "stretch": ("ball_fx", 5),
    "trophy": ("trophy", 0),
    "gull0": ("critters", 0), "gull1": ("critters", 1),
    "sat0": ("critters", 2), "sat1": ("critters", 3),
    "fish0": ("critters", 4), "fish1": ("critters", 5),
}


def main():
    manifest = json.loads((ROOT / "assets" / "manifest" /
                           "player_frames.json").read_text("utf-8"))
    sheet_path = ROOT / "assets" / "source" / "player_sprites.png"
    _, _, sheet, palette = read_png(sheet_path)
    sheet = [list(r) for r in sheet]
    slots = [manifest["frames"][n]["slot"] for n in WISHLIST_FRAMES] + \
        [manifest["large_objects"][n] for n in WISHLIST_LARGE] + \
        [manifest["sheet_objects"][n][0] for n in WISHLIST_CELLS]
    while len(sheet) < (max(slots) // 4 + 1) * 32:
        sheet.append([0] * 128)

    def put(x0, y0, pixels):
        for y, row in enumerate(pixels):
            sheet[y0 + y][x0:x0 + len(row)] = row

    for name in WISHLIST_FRAMES + WISHLIST_LARGE:
        slot = manifest["frames"][name]["slot"] if name in \
            manifest["frames"] else manifest["large_objects"][name]
        _, _, art, _ = read_png(WISH / f"{name}.png")
        put((slot % 4) * 32, (slot // 4) * 32, art)
    strips = {}
    for name, (strip, square) in WISHLIST_CELLS.items():
        if strip not in strips:
            strips[strip] = read_png(WISH / f"{strip}.png")[2]
        slot, cell, size = manifest["sheet_objects"][name]
        art = [row[square * size:square * size + size]
               for row in strips[strip][:size]]
        put((slot % 4) * 32 + (cell % 4) * 8,
            (slot // 4) * 32 + (cell // 4) * 8, art)
    write_png(sheet_path, sheet, palette)
    print(f"wrote {sheet_path.relative_to(ROOT)}: "
          f"{len(WISHLIST_FRAMES)} frames, {len(WISHLIST_CELLS)} objects, "
          f"{len(WISHLIST_LARGE)} large")


if __name__ == "__main__":
    main()
