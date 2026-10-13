#!/usr/bin/env python3
"""M3-ASSET-02: deliberately invalid assets must fail with useful messages.

Each case copies assets/source to a temporary directory, breaks one rule,
runs the generator with --source --validate-only, and checks both the
failure and the message. Stdlib only.
"""
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import generate_assets  # noqa: E402


def write_png(path, rows, palette):
    def chunk(kind, body):
        return (struct.pack(">I", len(body)) + kind + body +
                struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF))
    height, width = len(rows), len(rows[0])
    raw = b"".join(b"\0" + bytes(row) for row in rows)
    plte = b"".join(bytes(c) for c in palette)
    data = (b"\x89PNG\r\n\x1a\n" +
            chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 3, 0, 0, 0)) +
            chunk(b"PLTE", plte) + chunk(b"IDAT", zlib.compress(raw)) +
            chunk(b"IEND", b""))
    path.write_bytes(data)


def mutate(src, name, fn):
    w, h, rows, pal = generate_assets.read_png(src / name)
    rows = [list(r) for r in rows]
    rows = fn(rows)
    while len(pal) < 256:
        pal.append((0, 0, 0))
    write_png(src / name, rows, pal)


def frame_origin(slot):
    return (slot % 4) * 32, (slot // 4) * 32


def shift_idle_down(rows):
    x0, y0 = frame_origin(0)
    block = [rows[y0 + y][x0:x0 + 32] for y in range(32)]
    block = [[0] * 32] + block[:-1]
    for y in range(32):
        rows[y0 + y][x0:x0 + 32] = block[y]
    return rows


def put_pixel(x, y, value):
    def fn(rows):
        rows[y][x] = value
        return rows
    return fn


def scene_tile_palette0(rows):
    for y in range(8):
        for x in range(8):
            rows[y][x] = 1          # palette 0, colour 1
    return rows


def erase_left_baseline(rows):
    import court_projection
    c = court_projection.court_constants()
    table = court_projection.scale_table(c)
    y = (c["COURT_SIDELINE_TOP"] + c["COURT_SIDELINE_BOTTOM"]) // 2
    x = court_projection.project_x(c["COURT_BASELINE_LEFT"], y, table)
    rows[y][x] = 32 + 4             # court green over the line pixel
    rows[y][x + 1] = 32 + 4
    return rows


def grow_sprite_sheet(rows):
    # Two more 32-px frame rows push OBJ data past one 256-tile name table.
    return rows + [[0] * 128 for _ in range(64)]


def crop_scene(rows):
    return [row[:248] for row in rows]


CASES = [
    ("scene must be 256x224", "court_scene.png", crop_scene,
     "court_scene.png must be 256x224"),
    ("frame foot anchor", "player_sprites.png", shift_idle_down,
     "frame idle0 foot row 30 != 29"),
    ("frame colour index above 15", "player_sprites.png",
     put_pixel(16, 16, 20), "uses color index > 15"),
    ("BG1 palette 0 (ADR-016)", "court_scene.png", scene_tile_palette0,
     "uses 4bpp palette 0"),
    ("court line missing (PROJ-04)", "court_scene.png", erase_left_baseline,
     "PROJ-04"),
    ("OBJ tile budget", "player_sprites.png", grow_sprite_sheet,
     "OBJ uses"),
]


def main():
    failures = 0
    for title, name, fn, expected in CASES:
        with tempfile.TemporaryDirectory() as tmp:
            src = Path(tmp) / "source"
            shutil.copytree(ROOT / "assets" / "source", src)
            mutate(src, name, fn)
            result = subprocess.run(
                [sys.executable, str(ROOT / "tools" / "generate_assets.py"),
                 "--source", str(src), "--validate-only"],
                capture_output=True, text=True)
            output = result.stdout + result.stderr
            ok = result.returncode != 0 and expected in output
            print(f"{'PASS' if ok else 'FAIL'}: {title}")
            if not ok:
                failures += 1
                print("  expected failure containing:", expected)
                print("  got:", output.strip().splitlines()[-1:] or "(none)")
    if failures:
        raise SystemExit(f"{failures} asset validation fixture(s) did not fail")
    print("asset validation fixtures passed")


if __name__ == "__main__":
    main()
