#!/usr/bin/env python3
import argparse
import hashlib
import json
import re
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "assets" / "generated"
REPORT = OUT / "sizes.json"

def court_constants():
    text = (ROOT / "include" / "court.h").read_text(encoding="utf-8")
    names = (
        "COURT_BASELINE_LEFT", "COURT_BASELINE_RIGHT",
        "COURT_SIDELINE_TOP", "COURT_SIDELINE_BOTTOM", "COURT_NET_X",
        "COURT_KITCHEN_LEFT_X", "COURT_KITCHEN_RIGHT_X",
        "COURT_CENTERLINE_Y",
    )
    result = {}
    for name in names:
        match = re.search(rf"\b{name}\s*=\s*(\d+)", text)
        if not match:
            raise SystemExit(f"missing court constant: {name}")
        result[name] = int(match.group(1))
    return result

GLYPHS = {
    "A": ["01110","10001","10001","11111","10001","10001","10001"],
    "B": ["11110","10001","10001","11110","10001","10001","11110"],
    "C": ["01111","10000","10000","10000","10000","10000","01111"],
    "D": ["11110","10001","10001","10001","10001","10001","11110"],
    "E": ["11111","10000","10000","11110","10000","10000","11111"],
    "F": ["11111","10000","10000","11110","10000","10000","10000"],
    "G": ["01111","10000","10000","10111","10001","10001","01111"],
    "H": ["10001","10001","10001","11111","10001","10001","10001"],
    "I": ["11111","00100","00100","00100","00100","00100","11111"],
    "J": ["00111","00010","00010","00010","10010","10010","01100"],
    "K": ["10001","10010","10100","11000","10100","10010","10001"],
    "L": ["10000","10000","10000","10000","10000","10000","11111"],
    "M": ["10001","11011","10101","10101","10001","10001","10001"],
    "N": ["10001","11001","10101","10011","10001","10001","10001"],
    "O": ["01110","10001","10001","10001","10001","10001","01110"],
    "P": ["11110","10001","10001","11110","10000","10000","10000"],
    "Q": ["01110","10001","10001","10001","10101","10010","01101"],
    "R": ["11110","10001","10001","11110","10100","10010","10001"],
    "S": ["01111","10000","10000","01110","00001","00001","11110"],
    "T": ["11111","00100","00100","00100","00100","00100","00100"],
    "U": ["10001","10001","10001","10001","10001","10001","01110"],
    "V": ["10001","10001","10001","10001","10001","01010","00100"],
    "W": ["10001","10001","10001","10101","10101","10101","01010"],
    "X": ["10001","10001","01010","00100","01010","10001","10001"],
    "Y": ["10001","10001","01010","00100","00100","00100","00100"],
    "Z": ["11111","00001","00010","00100","01000","10000","11111"],
    "0": ["01110","10001","10011","10101","11001","10001","01110"],
    "1": ["00100","01100","00100","00100","00100","00100","01110"],
    "2": ["01110","10001","00001","00010","00100","01000","11111"],
    "3": ["11110","00001","00001","01110","00001","00001","11110"],
    "4": ["00010","00110","01010","10010","11111","00010","00010"],
    "5": ["11111","10000","10000","11110","00001","00001","11110"],
    "6": ["01110","10000","10000","11110","10001","10001","01110"],
    "7": ["11111","00001","00010","00100","01000","01000","01000"],
    "8": ["01110","10001","10001","01110","10001","10001","01110"],
    "9": ["01110","10001","10001","01111","00001","00001","01110"],
    "-": ["00000","00000","00000","11111","00000","00000","00000"],
    ".": ["00000","00000","00000","00000","00000","00110","00110"],
    ",": ["00000","00000","00000","00000","00110","00100","01000"],
    ":": ["00000","00110","00110","00000","00110","00110","00000"],
    "?": ["01110","10001","00001","00010","00100","00000","00100"],
}

def bgr15(r, g, b):
    return (r & 31) | ((g & 31) << 5) | ((b & 31) << 10)

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

def court_assets():
    c = court_constants()
    left = c["COURT_BASELINE_LEFT"]
    right = c["COURT_BASELINE_RIGHT"]
    top = c["COURT_SIDELINE_TOP"]
    bottom = c["COURT_SIDELINE_BOTTOM"]
    net_x = c["COURT_NET_X"]
    kitchen_left = c["COURT_KITCHEN_LEFT_X"]
    kitchen_right = c["COURT_KITCHEN_RIGHT_X"]
    center_y = c["COURT_CENTERLINE_Y"]
    tiles = []
    indices = {}
    tilemap = []
    for ty in range(32):
        for tx in range(32):
            pixels = []
            for py in range(8):
                row = []
                y = ty * 8 + py
                for px in range(8):
                    x = tx * 8 + px
                    inside = left <= x <= right and top <= y <= bottom
                    color = 1 if inside else 4
                    boundary = ((x in (left, right) and top <= y <= bottom) or
                                (y in (top, bottom) and left <= x <= right))
                    kitchen = (x in (kitchen_left, kitchen_right) and
                               top <= y <= bottom)
                    center = (y == center_y and
                              (left <= x <= kitchen_left or
                               kitchen_right <= x <= right))
                    net = x == net_x and top <= y <= bottom
                    if boundary or kitchen or center:
                        color = 2
                    if net:
                        color = 3
                    row.append(color)
                pixels.append(row)
            encoded = encode_tile(pixels, 4)
            if encoded not in indices:
                indices[encoded] = len(tiles)
                tiles.append(encoded)
            tilemap.append(indices[encoded] | (1 << 10))
    return b"".join(tiles), struct.pack("<1024H", *tilemap)

def font_assets():
    result = bytearray()
    for code in range(32, 128):
        pixels = [[0] * 8 for _ in range(8)]
        glyph = GLYPHS.get(chr(code), GLYPHS.get(chr(code).upper()))
        if glyph:
            for y, row in enumerate(glyph):
                for x, bit in enumerate(row):
                    if bit == "1":
                        pixels[y][x + 1] = 1
        result.extend(encode_tile(pixels, 2))
    return bytes(result)

def player_assets():
    canvas = [[0] * 128 for _ in range(32)]
    def paint_frame(origin, running):
        for y in range(3, 11):
            for x in range(11, 21):
                if (x - 16) ** 2 + (y - 7) ** 2 <= 25:
                    canvas[y][origin + x] = 3
        for y in range(11, 24):
            for x in range(9, 23):
                canvas[y][origin + x] = 4 if y < 17 else 5
        for y in range(24, 31):
            left = 8 + (y - 24 if running else 2)
            right = 21 - (y - 24 if running else 0)
            for x in range(left, min(left + 5, 32)):
                canvas[y][origin + x] = 6
            for x in range(max(right, 0), min(right + 5, 32)):
                canvas[y][origin + x] = 6
        for y in range(14, 24):
            canvas[y][origin + 23] = 2
        for x in range(22, 29):
            canvas[13][origin + x] = 2
            canvas[18][origin + x] = 2
        for y in range(13, 19):
            canvas[y][origin + 28] = 2
    paint_frame(0, False)
    paint_frame(32, True)
    for y in range(2, 6):
        for x in range(64, 72):
            if ((x - 68) ** 2) // 4 + (y - 4) ** 2 <= 4:
                canvas[y][x] = 1
    for y in range(1, 7):
        for x in range(97, 103):
            if (x - 100) ** 2 + (y - 4) ** 2 <= 9:
                canvas[y][x] = 2 if y < 3 else 6
    tiles = []
    for ty in range(4):
        for tx in range(16):
            pixels = [row[tx * 8:(tx + 1) * 8] for row in canvas[ty * 8:(ty + 1) * 8]]
            tiles.append(encode_tile(pixels, 4))
    return b"".join(tiles)

def generated_files():
    court_pic, court_map = court_assets()
    return {
        "court.pic": court_pic,
        "court.map": court_map,
        "court.pal": struct.pack("<16H", *[
            bgr15(0, 0, 0), bgr15(4, 18, 12), bgr15(31, 31, 27),
            bgr15(24, 10, 4), bgr15(2, 8, 7), *([0] * 11)]),
        "hud.pic": font_assets(),
        "hud.map": bytes(2048),
        "hud.pal": struct.pack("<4H", bgr15(0, 0, 0), bgr15(31, 31, 31), 0, 0),
        "player.pic": player_assets(),
        "player.pal": struct.pack("<16H", *[
            0, bgr15(4, 5, 5), bgr15(26, 27, 28), bgr15(25, 17, 10),
            bgr15(4, 12, 27), bgr15(7, 20, 31), bgr15(31, 28, 8),
            *([0] * 9)]),
    }

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    expected = generated_files()
    OUT.mkdir(parents=True, exist_ok=True)
    if args.check:
        failures = [name for name, data in expected.items()
                    if not (OUT / name).exists() or (OUT / name).read_bytes() != data]
        if failures:
            raise SystemExit("asset validation failed: " + ", ".join(failures))
        print("asset validation passed")
        return
    for name, data in expected.items():
        (OUT / name).write_bytes(data)
    report = {name: {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}
              for name, data in expected.items()}
    REPORT.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print("generated M1 assets")

if __name__ == "__main__":
    main()

