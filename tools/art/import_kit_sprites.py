#!/usr/bin/env python3
"""Authoring tool: the player models from the port kit's bareheaded set.

NOT part of the normal build; run by hand when the kit's art changes.
Standard library only. Reads the kit's indexed strips (palette contract v2,
ADR-013; every team's strip has the same indices, so the blue one is read)
and writes into the committed sources:

  assets/source/player_sprites.png  slots 0-10 the bareheaded frames, 12-15
      the hitting poses (name table 2, R17); slot 11 (the BIG BALL) and any
      other slot are kept.
  assets/source/objects.png         the ponytail cell, and the 16x8 hats at
      their manifest cells (the cap is a hat now, ADR-059). Hats given as
      {"sheet": k} go in player_sprites.png's hat_slot instead (ADR-060).

The kit lives beside this repository (pickleball-port-kit,
art/sprites/bare/README.md); pass another path as the only argument.

Usage: python tools/art/import_kit_sprites.py [KIT_ROOT]
"""
import json
import struct
import sys
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from assetgen.common import read_png  # noqa: E402

# Kit strip order: the 11 frames, the 4 poses, then the ponytail's cell.
KIT_FRAMES = ["idle0", "idle1", "run0", "run1", "run2", "run3", "windup",
              "swing", "follow", "toss", "cheer", "overhead_windup",
              "overhead_contact", "reach", "scoop"]
KIT_PONYTAIL_CELL = 15
# hats_<team>.png order, 16x8 each.
KIT_HATS = ["cap", "visor", "headband", "tricorn", "bandana", "beanie",
            "backcap", "bucket", "crown", "whirly", "chef", "viking"]


def write_png(path, rows, palette):
    def chunk(kind, body):
        return (struct.pack(">I", len(body)) + kind + body +
                struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF))
    height, width = len(rows), len(rows[0])
    raw = b"".join(b"\0" + bytes(row) for row in rows)
    plte = b"".join(bytes(c) for c in palette)
    path.write_bytes(
        b"\x89PNG\r\n\x1a\n" +
        chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 3, 0, 0, 0)) +
        chunk(b"PLTE", plte) + chunk(b"IDAT", zlib.compress(raw, 9)) +
        chunk(b"IEND", b""))


def main():
    kit = Path(sys.argv[1]) if len(sys.argv) > 1 else \
        ROOT.parent / "pickleball-port-kit"
    bare = kit / "art" / "sprites" / "bare"
    manifest = json.loads((ROOT / "assets" / "manifest" /
                           "player_frames.json").read_text("utf-8"))
    offsets = json.loads((bare / "offsets.json").read_text("utf-8"))
    _, _, strip, _ = read_png(bare / "player_bare_blue.png")
    _, _, hats, _ = read_png(bare / "hats_blue.png")

    # The kit's ponytail offsets are the manifest's accessory offsets.
    kit_acc = {f["frame"]: f["accessory"] for f in offsets["frames"]}
    if offsets["hat_offset"] != manifest["hat_offset"]:
        raise SystemExit("kit hat_offset differs from the manifest's")
    for name, info in manifest["frames"].items():
        if kit_acc.get(name) != info["accessory"]:
            raise SystemExit(f"{name}: kit accessory {kit_acc.get(name)} != "
                             f"manifest {info['accessory']}")

    sheet_path = ROOT / "assets" / "source" / "player_sprites.png"
    _, _, sheet, palette = read_png(sheet_path)
    sheet = [list(r) for r in sheet]
    rows_needed = max(info["slot"] for info in
                      manifest["frames"].values()) // 4 + 1
    while len(sheet) < rows_needed * 32:
        sheet.append([0] * 128)
    for i, name in enumerate(KIT_FRAMES):
        slot = manifest["frames"][name]["slot"]
        x0, y0 = (slot % 4) * 32, (slot // 4) * 32
        for y in range(32):
            sheet[y0 + y][x0:x0 + 32] = strip[y][i * 32:i * 32 + 32]
    # Hats on the sheet (ADR-060): place k of slot hat_slot.
    slot = manifest.get("hat_slot")
    for i, name in enumerate(KIT_HATS):
        place = manifest["hats"][name]
        if not isinstance(place, dict):
            continue
        k = place["sheet"]
        x0 = (slot % 4) * 32 + (k % 2) * 16
        y0 = (slot // 4) * 32 + (k // 2) * 8
        while len(sheet) < (slot // 4 + 1) * 32:
            sheet.append([0] * 128)
        for y in range(8):
            sheet[y0 + y][x0:x0 + 16] = hats[y][i * 16:i * 16 + 16]
    write_png(sheet_path, sheet, palette)

    objects_path = ROOT / "assets" / "source" / "objects.png"
    _, _, objects, opal = read_png(objects_path)
    objects = [list(r) for r in objects]

    def stamp(cell, width, pixels):
        for y in range(8):
            for x in range(width):
                cx = (cell + x // 8) % 16 * 8 + x % 8
                cy = (cell + x // 8) // 16 * 8 + y
                objects[cy][cx] = pixels[y][x]

    px = KIT_PONYTAIL_CELL * 32
    stamp(manifest["objects"]["ponytail"], 8,
          [row[px:px + 8] for row in strip[:8]])
    for i, name in enumerate(KIT_HATS):
        if isinstance(manifest["hats"][name], int):
            stamp(manifest["hats"][name], 16,
                  [row[i * 16:i * 16 + 16] for row in hats])
    write_png(objects_path, objects, opal)
    print(f"wrote {sheet_path.relative_to(ROOT)} and "
          f"{objects_path.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
