#!/usr/bin/env python3
"""Build report for a ROM (M3-ASSET-04, M3-QA-04, M3-QA-05).

Validates the internal header and checksum, then writes a Markdown report with
ROM bank usage, generated-asset sizes and tile budgets, the VRAM/OAM plan, and
static RAM usage from the linker symbol file. Stdlib only.
"""
import argparse
import hashlib
import json
import re
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXPECTED_TITLE = b"PICKLEBALL 94"

parser = argparse.ArgumentParser()
parser.add_argument("--config", required=True)
parser.add_argument("--build-id", required=True)
parser.add_argument("--rom", required=True, type=Path)
parser.add_argument("--output", required=True, type=Path)
args = parser.parse_args()

data = args.rom.read_bytes()
if len(data) < 0x8000:
    raise SystemExit("ROM is too small to contain a LoROM header")
header = data[0x7FC0:0x8000]
title = header[:21].rstrip(b"\0 ")
if title != EXPECTED_TITLE:
    raise SystemExit(f"unexpected internal ROM title {title!r}")
# ADR-021: ROM + RAM + battery ($02) with 2 KiB SRAM ($01).
if header[0x15] != 0x30 or header[0x16] != 0x02:
    raise SystemExit("ROM is not a LoROM FastROM cartridge with battery "
                     "SRAM (map mode $30, type $02)")
if header[0x18] != 0x01 or header[0x19] != 0x01:
    raise SystemExit("ROM must have 2 KiB SRAM and use the NTSC region code")
complement, checksum = struct.unpack_from("<HH", header, 0x1C)
if (complement + checksum) & 0xFFFF != 0xFFFF:
    raise SystemExit("invalid internal checksum/complement pair")
computed = sum(data) & 0xFFFF
if computed != checksum:
    raise SystemExit(f"header checksum {checksum:04x} != computed {computed:04x}")

banks = []
for b in range(len(data) // 0x8000):
    bank = data[b * 0x8000:(b + 1) * 0x8000]
    banks.append(len(bank.rstrip(b"\0")))
rom_used = sum(banks)

sizes = json.loads((ROOT / "assets" / "generated" / "sizes.json").read_text())
budget = sizes.pop("_budget", {})

render_h = (ROOT / "include" / "render.h").read_text()
vram = dict(re.findall(r"#define RENDER_(\w+)_ADDR (0x[0-9a-fA-F]+)", render_h))
oam = dict((k, int(v)) for k, v in
           re.findall(r"RENDER_OAM_(\w+) = (\d+)", render_h)
           if not k.startswith("GROUP_"))

game_ram = []
lib_ram = []
sym = args.rom.with_suffix(".sym")
if sym.exists():
    for line in sym.read_text(errors="replace").splitlines():
        parts = line.split()
        if len(parts) != 2 or line.startswith(";"):
            continue
        addr = int(parts[0], 16)
        if addr >> 16 != 0x7E:
            continue
        (game_ram if 0x2000 <= addr & 0xFFFF < 0x9000 else lib_ram).append(addr)


def span(addrs):
    if not addrs:
        return "n/a"
    lo, hi = min(addrs) & 0xFFFF, max(addrs) & 0xFFFF
    return f"`$7E{lo:04X}`–`$7E{hi:04X}` (≈{hi - lo + 1:,} bytes incl. last symbol start)"


lines = [
    "# Pickleball '94 build report",
    "",
    f"- Configuration: `{args.config}`",
    f"- Build ID: `{args.build_id}`",
    "- PVSnesLib: 4.6.0",
    f"- ROM: `{args.rom.name}`, {len(data):,} bytes",
    "- Header: LoROM / FastROM (map mode $30, 120 ns ROM), NTSC, 2 KiB "
    "battery SRAM (ADR-021), title `PICKLEBALL 94`",
    f"- Internal checksum: `{checksum:04x}` (complement `{complement:04x}`, verified)",
    f"- SHA-256: `{hashlib.sha256(data).hexdigest()}`",
    "",
    "## ROM usage",
    "",
    "| Bank | Used bytes | Free bytes |",
    "|---:|---:|---:|",
]
for i, used in enumerate(banks):
    lines.append(f"| {i} | {used:,} | {0x8000 - used:,} |")
lines += [f"| **Total** | **{rom_used:,}** | **{len(data) - rom_used:,}** |", "",
          "## Generated assets", "",
          "| File | Bytes | SHA-256 (first 16) |", "|---|---:|---|"]
for name in sorted(sizes):
    lines.append(f"| `{name}` | {sizes[name]['bytes']:,} | "
                 f"`{sizes[name]['sha256'][:16]}` |")
lines += ["", "## Tile and palette budgets", "",
          "| Resource | Used | Budget |", "|---|---:|---:|",
          f"| BG1 scene tiles (largest venue) | {budget.get('court_tiles')} | 896 (28 KiB, ADR-016 as amended) |",
          f"| BG1 shared ground tiles | {budget.get('ground_tiles')} | (in the above, ADR-024) |",
          f"| BG3 HUD tiles | {budget.get('hud_tiles')} | 512 |",
          f"| OBJ tiles | {budget.get('obj_tiles')} | 256 (first name table) |",
          "| Player frames | 11 | 14 (docs/03) |",
          "| Player palettes | 4 composed from parts, one tile set | 4 |",
          "| BG3 HUD palettes | 0–3 | 0–3 (ADR-016) |",
          "", "## VRAM map (word addresses)", "",
          "| Region | Address |", "|---|---|"]
for name, addr in vram.items():
    lines.append(f"| {name} | `{addr}` |")
lines += ["", "## OAM plan", "",
          "| First index | Use |", "|---:|---|"]
for name, index in sorted(oam.items(), key=lambda kv: kv[1]):
    lines.append(f"| {index} | {name.lower()} |")
lines += ["", "Worst planned row: 32 of 34 slivers (ADR-014, hats), verified "
          "dropout-free by the OBJ stress scene's PPU range/time-over latch.",
          "", "## Static RAM", "",
          f"- Game variables: {span(game_ram)}",
          f"- PVSnesLib variables: {span(lib_ram)}",
          "- RAM is not cleared at power-on; every module initializes its "
          "own state (see `render_initialize`, `game_new_match`).",
          ""]
args.output.write_text("\n".join(lines), encoding="utf-8")
print(f"wrote {args.output}")
