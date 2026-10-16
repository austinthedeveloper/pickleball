"""Shared asset-build helpers: paths, PNG reading, SNES tile/colour encoding."""
import struct
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class Paths:
    """Source locations; --source swaps in validation fixtures."""

    def __init__(self):
        self.src = ROOT / "assets" / "source"
        self.out = ROOT / "assets" / "generated"
        self.manifest = ROOT / "assets" / "manifest" / "player_frames.json"

    @property
    def palettes(self):
        return self.src / "palettes"


paths = Paths()

COURT_TILE_BUDGET = 896      # BG1: 28 KiB at VRAM word 0x1000 (ADR-016)
HUD_TILE_BUDGET = 512        # BG3 tiles at VRAM word 0x5000..0x5fff
OBJ_TILE_BUDGET = 256        # each OBJ name table: 0x6000 and 0x7000 (words)
OBJ_TABLE1_SLOTS = 12        # player sheet slots (rows 0-2) in name table 1


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
