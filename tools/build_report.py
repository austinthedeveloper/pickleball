#!/usr/bin/env python3
import argparse
import hashlib
import struct
from pathlib import Path

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
if header[:16].rstrip(b"\0 ") != b"PICKLEBALL 94 M1":
    raise SystemExit("unexpected internal ROM title")
if header[0x15] != 0x20 or header[0x16] != 0x00:
    raise SystemExit("ROM is not a standard slow LoROM cartridge")
if header[0x18] != 0x00 or header[0x19] != 0x01:
    raise SystemExit("ROM must have no SRAM and use the NTSC region code")
complement, checksum = struct.unpack_from("<HH", header, 0x1C)
if (complement + checksum) & 0xFFFF != 0xFFFF:
    raise SystemExit("invalid internal checksum/complement pair")
args.output.write_text(
    "Pickleball '94 build report\n"
    f"configuration: {args.config}\n"
    f"build_id: {args.build_id}\n"
    "pvsneslib: 4.6.0\n"
    f"rom_bytes: {len(data)}\n"
    "header: LoROM/SlowROM, NTSC, no SRAM\n"
    f"internal_checksum: {checksum:04x}\n"
    f"sha256: {hashlib.sha256(data).hexdigest()}\n",
    encoding="utf-8",
)

