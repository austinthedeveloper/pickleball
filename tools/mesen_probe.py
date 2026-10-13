#!/usr/bin/env python3
"""Headless MesenCE probes and golden traces (docs/REFACTOR_PLAN.md R0/R1).

  probe CONFIG TIMELINE FRAMES name=symbol[+offset]:size ...
      Runs build/CONFIG's ROM with the timeline and prints the probes on
      every "shot" frame. Symbols come from the ROM's .sym file.

  golden-record [--config release]
  golden-compare [--config release]
      Records (or compares against) build/golden/trace.txt: the whole game,
      player, ball, serve, and AI state on every frame of
      test/smoke/golden.timeline. A behavior-preserving refactor must keep
      the trace identical even though variables may move in RAM.

Environment: MESEN_BIN overrides /Applications/Mesen.app/.../Mesen.
"""
import argparse
import glob
import os
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MESEN = os.environ.get("MESEN_BIN",
                       "/Applications/Mesen.app/Contents/MacOS/Mesen")
GOLDEN_TIMELINE = ROOT / "test" / "smoke" / "golden.timeline"
GOLDEN_TRACE = ROOT / "build" / "golden" / "trace.txt"
GOLDEN_FRAMES = 4200

# Simulation state captured by golden traces. Sizes are the SNES layouts
# (identical to the host's sizeof); each run checks them against .sym gaps.
GOLDEN_STATE = [
    ("game", "g_game_state", 16),
    ("players", "g_players", 96),
    ("ball", "g_ball", 18),
    ("serve", "g_serve_state", 10),
    ("ai", "g_ai_slots", 80),
]
# Frames before initialization hold MesenCE's randomized power-on RAM.
GOLDEN_SKIP_FRAMES = 2
# Frame counters move with code size/timing, not behavior; mask them.
GAME_MASK_BYTES = (14, 15)   # GameState.frame_count (offsetof)


def rom_for(config):
    roms = sorted(glob.glob(str(ROOT / "build" / config / "*.sfc")),
                  key=os.path.getmtime)
    if not roms:
        raise SystemExit(f"no ROM in build/{config}; run make {config}")
    return Path(roms[-1])


def symbols(rom):
    table = {}
    for line in rom.with_suffix(".sym").read_text(errors="replace").splitlines():
        parts = line.split()
        if len(parts) == 2 and not line.startswith(";"):
            table[parts[1]] = int(parts[0], 16)
    return table


def run(rom, timeline, frames, probes, every=False, shots_dir=None):
    env = dict(os.environ, SMOKE_TIMELINE=str(timeline),
               SMOKE_FRAMES=str(frames), SMOKE_PROBES=",".join(probes),
               SMOKE_OUT=shots_dir or tempfile.gettempdir(),
               SMOKE_NOSHOT="0" if shots_dir else "1",
               SMOKE_PROBE_EVERY="1" if every else "0")
    result = subprocess.run(
        [MESEN, "--testRunner", "--doNotSaveSettings", "--noAudio",
         "--enableStdout", "--debug.scriptWindow.allowIoOsAccess=true",
         str(ROOT / "tools" / "mesen_smoke.lua"), str(rom)],
        env=env, capture_output=True, text=True)
    return [line for line in result.stdout.splitlines()
            if line.startswith("frame ")]


def resolve(table, spec):
    name, rest = spec.split("=")
    sym, size = rest.rsplit(":", 1)
    offset = 0
    if "+" in sym:
        sym, off = sym.split("+")
        offset = int(off)
    if sym not in table:
        raise SystemExit(f"symbol {sym} not found (static symbols are not "
                         "exported; probe a global)")
    return f"{name}={(table[sym] & 0xFFFF) + offset:x}:{size}"


def golden_probes(rom):
    table = symbols(rom)
    ordered = sorted(addr for addr in table.values() if addr >> 16 == 0x7E)
    probes = []
    for name, sym, size in GOLDEN_STATE:
        addr = table[sym]
        following = [a for a in ordered if a > addr]
        if following and following[0] - addr < size:
            raise SystemExit(f"{sym}: {size} bytes overlaps the next symbol; "
                             "update GOLDEN_STATE sizes")
        probes.append(f"{name}={addr & 0xFFFF:x}:{size}")
    return probes


def golden_lines(config):
    rom = rom_for(config)
    lines = run(rom, GOLDEN_TIMELINE, GOLDEN_FRAMES, golden_probes(rom),
                every=True)
    cleaned = []
    for line in lines:
        if int(line.split()[1]) < GOLDEN_SKIP_FRAMES:
            continue
        fields = dict(part.split("=", 1) for part in line.split()[2:])
        game = bytearray.fromhex(fields["game"])
        for i in GAME_MASK_BYTES:
            game[i] = 0
        fields["game"] = game.hex()
        cleaned.append(line.split()[1] + " " + " ".join(
            f"{k}={fields[k]}" for k, _, _ in GOLDEN_STATE))
    if len(cleaned) < GOLDEN_FRAMES - GOLDEN_SKIP_FRAMES - 5:
        raise SystemExit(f"emulator produced only {len(cleaned)} frames")
    return cleaned


def main():
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("probe")
    p.add_argument("config")
    p.add_argument("timeline", type=Path)
    p.add_argument("frames", type=int)
    p.add_argument("specs", nargs="+")
    p.add_argument("--shots", help="directory for screenshots")
    for name in ("golden-record", "golden-compare"):
        g = sub.add_parser(name)
        g.add_argument("--config", default="release")
        g.add_argument("--fields", help="compare only these comma-separated "
                       "fields (for a refactor that changes one struct's "
                       "layout); then re-record")
    args = parser.parse_args()

    if args.cmd == "probe":
        rom = rom_for(args.config)
        table = symbols(rom)
        for line in run(rom, args.timeline.resolve(), args.frames,
                        [resolve(table, s) for s in args.specs],
                        shots_dir=args.shots):
            print(line)
        return

    lines = golden_lines(args.config)
    if args.cmd == "golden-record":
        GOLDEN_TRACE.parent.mkdir(parents=True, exist_ok=True)
        GOLDEN_TRACE.write_text("\n".join(lines) + "\n")
        print(f"recorded {len(lines)} frames to "
              f"{GOLDEN_TRACE.relative_to(ROOT)}")
        return
    if not GOLDEN_TRACE.exists():
        raise SystemExit("no golden trace; run golden-record first")
    expected = GOLDEN_TRACE.read_text().splitlines()
    if args.fields:
        keep = set(args.fields.split(","))

        def only(line):
            frame, *parts = line.split()
            return frame + " " + " ".join(
                p for p in parts if p.split("=", 1)[0] in keep)
        expected = [only(line) for line in expected]
        lines = [only(line) for line in lines]
    for i, (want, got) in enumerate(zip(expected, lines)):
        if want != got:
            print(f"golden trace differs first at line {i + 1}:")
            print("  expected:", want[:200])
            print("  actual:  ", got[:200])
            sys.exit(1)
    if len(expected) != len(lines):
        sys.exit(f"golden trace length differs: {len(expected)} vs "
                 f"{len(lines)}")
    scope = f" for {args.fields}" if args.fields else ""
    print(f"golden trace identical{scope} ({len(lines)} frames)")


if __name__ == "__main__":
    main()
