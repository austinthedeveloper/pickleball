#!/usr/bin/env python3
"""Headless MesenCE probes and golden traces (docs/REFACTOR_PLAN.md R0/R1).

  probe CONFIG TIMELINE FRAMES name=symbol[+offset]:size ...
        [--poke SPEC ...] [--shots DIR]
      Runs build/CONFIG's ROM with the timeline and prints the probes on
      every "shot" frame. Symbols come from the ROM's .sym file.
      --poke writes work RAM to set up a bug repro fast (a score, a record)
      instead of playing up to it. SPEC is
        [@FIRST[-LAST]] [once] [if SYM[+OFF][:SIZE]==V] SYM[+OFF][:SIZE]=V
      Without @, every frame; @F, frame F only; @F-, F onward; @F-L, F to L.
      "once"
      fires the first time it applies. SIZE is 1 (default) or 2. Example:
        --poke "@420- once if g_game_state+1==0 g_game_state+6=4"
      puts P1 on 4 points at the first serve setup after frame 420.

  golden-record [--config release]
  golden-compare [--config release]
      Records (or compares against) build/golden/trace.txt: the whole game,
      player, ball, serve, and AI state on every frame of
      test/smoke/golden.timeline. A behavior-preserving refactor must keep
      the trace identical even though variables may move in RAM.

  lockstep-record [--config release] TIMELINE FRAMES OUT
      Writes OUT: on every frame, the match config, the intents, and the
      simulation state (LOCKSTEP_STATE). tools/lockstep.c replays the
      match on the host from it and stops at the first frame where the
      ROM and the host C differ (make lockstep, ADR-069).

Environment: MESEN_BIN overrides /Applications/Mesen.app/.../Mesen.
"""
import argparse
import glob
import json
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
GOLDEN_FRAMES = 4260

# Simulation state captured by golden traces. Sizes are the SNES layouts
# (identical to the host's sizeof); each run checks them against .sym gaps.
GOLDEN_STATE = [
    ("game", "g_game_state", 16),
    ("players", "g_players", 112),
    ("ball", "g_ball", 18),
    ("serve", "g_serve_state", 10),
    ("ai", "g_ai_slots", 96),
]
# The ROM-versus-host lockstep (tools/lockstep.c, ADR-069): what it needs to
# replay a match on the host, then the state it compares.
LOCKSTEP_STATE = [
    ("cfg", "g_match_config", 32),       # sizeof(MatchConfig)
    ("attract", "g_attract", 1),
    ("owners", "g_input_owners", 4),
    ("intents", "g_player_intents", 56),  # 4 x sizeof(PlayerIntent)
    ("charge", "g_serve_charge", 5),
] + GOLDEN_STATE
# Frames before initialization hold MesenCE's randomized power-on RAM. Boot
# (the sound CPU, the two-bank soundbank, the first court load) ends near
# frame 10 since v0.5.0; 30 leaves room.
GOLDEN_SKIP_FRAMES = 30
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


# MesenCE keeps battery saves (ADR-021) per ROM file name. Runs start from
# a blank save: the saved ladder (ADR-023) picks the CPU opponent.
MESEN_SAVES = Path(os.environ.get(
    "MESEN_SAVES",
    Path.home() / "Library" / "Application Support" / "MesenCE" / "Saves"))


def run(rom, timeline, frames, probes, every=False, shots_dir=None,
        pokes=()):
    (MESEN_SAVES / (rom.stem + ".srm")).unlink(missing_ok=True)
    env = dict(os.environ, SMOKE_TIMELINE=str(timeline),
               SMOKE_FRAMES=str(frames), SMOKE_PROBES=",".join(probes),
               SMOKE_POKES=";".join(pokes),
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


def symbol_address(table, sym):
    """SYM or SYM+OFF to a work-RAM address."""
    offset = 0
    if "+" in sym:
        sym, off = sym.split("+")
        offset = int(off, 0)
    if sym not in table:
        raise SystemExit(f"symbol {sym} not found (static symbols are not "
                         "exported; use a global)")
    return (table[sym] & 0xFFFF) + offset


def target(table, text):
    """SYM[+OFF][:SIZE] to (address, size)."""
    sym, _, size = text.partition(":")
    return symbol_address(table, sym), int(size or 1)


def resolve_poke(table, spec):
    """A --poke SPEC (see the module docstring) to SMOKE_POKES fields."""
    words = spec.split()
    first, last, once = 0, -1, 0
    caddr, csize, cvalue = 0, 0, 0
    if words and words[0].startswith("@"):
        start, dash, end = words.pop(0)[1:].partition("-")
        first = int(start)
        last = int(end) if end else (-1 if dash else first)
    if words and words[0] == "once":
        words.pop(0)
        once = 1
    if words and words[0] == "if":
        cond = words[1]
        words = words[2:]
        lhs, value = cond.split("==")
        caddr, csize = target(table, lhs)
        cvalue = int(value, 0)
    if len(words) != 1 or "=" not in words[0]:
        raise SystemExit(f"bad --poke {spec!r}")
    lhs, value = words[0].split("=")
    addr, size = target(table, lhs)
    return ",".join(str(n) for n in (first, last, once, caddr, csize, cvalue,
                                     addr, size, int(value, 0)))


def resolve(table, spec):
    name, rest = spec.split("=")
    sym, size = rest.rsplit(":", 1)
    return f"{name}={symbol_address(table, sym):x}:{size}"


def golden_probes(rom, state=GOLDEN_STATE):
    table = symbols(rom)
    ordered = sorted(addr for addr in table.values() if addr >> 16 == 0x7E)
    probes = []
    for name, sym, size in state:
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


PERF_MANIFEST = ROOT / "test" / "perf.json"
PERF_MIN_LOOPS = 0.3            # share of frames the main loop must run


def perf_run(rom, table, timeline, frames):
    """Final overrun count, loop count, worst work scanline (of 225), and
    worst VBlank lines used (of 37) for a timeline (shots removed)."""
    lines = [line for line in timeline.read_text().splitlines()
             if not line.strip().startswith("shot")]
    with tempfile.NamedTemporaryFile("w", suffix=".timeline",
                                     delete=False) as tmp:
        tmp.write("\n".join(lines + [f"shot {frames - 2}"]) + "\n")
    probes = [resolve(table, "ovr=g_overrun_frames:2"),
              resolve(table, "loops=g_loop_count:2"),
              resolve(table, "worst=g_work_scanline_max:2"),
              resolve(table, "vbl=g_vblank_used_max:2")]
    out = run(rom, Path(tmp.name), frames, probes)
    os.unlink(tmp.name)
    if not out:
        return None, None, None, None
    fields = dict(part.split("=", 1) for part in out[-1].split()[2:])
    return (int(fields["ovr"]), int(fields["loops"]), int(fields["worst"]),
            int(fields["vbl"]))


def sound_run(rom, timeline, frames):
    """Voice starts from tools/mesen_voices.lua."""
    (MESEN_SAVES / (rom.stem + ".srm")).unlink(missing_ok=True)
    env = dict(os.environ, SMOKE_TIMELINE=str(timeline),
               SMOKE_FRAMES=str(frames))
    result = subprocess.run(
        [MESEN, "--testRunner", "--doNotSaveSettings", "--noAudio",
         "--enableStdout", "--debug.scriptWindow.allowIoOsAccess=true",
         str(ROOT / "tools" / "mesen_voices.lua"), str(rom)],
        env=env, capture_output=True, text=True)
    starts = [line.split() for line in result.stdout.splitlines()
              if line.endswith(" starts")]
    done = any(line.startswith("done ") for line in result.stdout.splitlines())
    return done, [int(parts[3]) for parts in starts]


def perf(config):
    """make perf: every run in test/perf.json within its overrun budget,
    with the main loop running, and the sound checks passing."""
    rom = rom_for(config)
    table = symbols(rom)
    manifest = json.loads(PERF_MANIFEST.read_text())
    smoke = ROOT / "test" / "smoke"
    failures = []
    print(f"{'timeline':<26} {'frames':>6} {'loops':>6} {'worst':>5} "
          f"{'vblank':>6} {'overruns':>9}")
    for entry in manifest["runs"]:
        name = entry["timeline"]
        frames = entry["frames"]
        budget = entry.get("overruns", 0)
        ovr, loops, worst, vbl = perf_run(rom, table, smoke / name, frames)
        if ovr is None:
            failures.append(f"{name}: no output (emulator or script failed)")
            print(f"{name:<26} {frames:>6} {'-':>6} {'-':>5} {'-':>6} "
                  f"{'-':>9}  FAIL")
            continue
        problems = []
        if loops < frames * entry.get("min_loops", PERF_MIN_LOOPS):
            problems.append(f"main loop ran {loops} of {frames} frames "
                            "(hung or stuck?)")
        if ovr > budget:
            problems.append(f"{ovr} overruns > budget {budget}")
        status = "FAIL" if problems else "ok"
        note = f"  ({entry['note']})" if "note" in entry else ""
        print(f"{name:<26} {frames:>6} {loops:>6} {worst:>5} {vbl:>6} "
              f"{ovr:>5}/{budget:<3} "
              f"{status}{note}")
        failures += [f"{name}: {p}" for p in problems]
    for entry in manifest.get("sound", []):
        name = entry["timeline"]
        done, voices = sound_run(rom, (smoke / name).resolve(),
                                 entry["frames"])
        missing = sorted(set(entry.get("voices", [])) - set(voices))
        problems = []
        if not done:
            problems.append("the sound probe did not finish")
        if len(voices) < entry.get("min_starts", 1):
            problems.append(f"{len(voices)} voice starts < "
                            f"{entry.get('min_starts', 1)}")
        if missing:
            problems.append(f"voices never sounded: {missing}")
        status = "FAIL" if problems else "ok"
        print(f"sound {name:<14} {entry['frames']:>6} {len(voices):>6} starts, "
              f"voices {sorted(set(voices))} {status}")
        failures += [f"sound {name}: {p}" for p in problems]
    if failures:
        print("\nperf failed:")
        for failure in failures:
            print("  " + failure)
        sys.exit(1)
    print("\nperf passed")


def main():
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("probe")
    p.add_argument("config")
    p.add_argument("timeline", type=Path)
    p.add_argument("frames", type=int)
    p.add_argument("specs", nargs="+")
    p.add_argument("--shots", help="directory for screenshots")
    p.add_argument("--poke", action="append", default=[],
                   help="work-RAM write; see the module docstring")
    pf = sub.add_parser("perf")
    pf.add_argument("--config", default="release")
    lr = sub.add_parser("lockstep-record")
    lr.add_argument("--config", default="release")
    lr.add_argument("timeline", type=Path)
    lr.add_argument("frames", type=int)
    lr.add_argument("out", type=Path)
    for name in ("golden-record", "golden-compare"):
        g = sub.add_parser(name)
        g.add_argument("--config", default="release")
        g.add_argument("--fields", help="compare only these comma-separated "
                       "fields (for a refactor that changes one struct's "
                       "layout); then re-record")
    args = parser.parse_args()

    if args.cmd == "perf":
        perf(args.config)
        return

    if args.cmd == "lockstep-record":
        rom = rom_for(args.config)
        lines = run(rom, args.timeline.resolve(), args.frames,
                    golden_probes(rom, LOCKSTEP_STATE), every=True)
        if len(lines) < args.frames - 5:
            raise SystemExit(f"emulator produced only {len(lines)} frames")
        # Sizes 1 and 2 print as decimal; lockstep reads every field as a
        # little-endian hex dump.
        small = {name: size for name, _, size in LOCKSTEP_STATE if size <= 2}

        def as_hex(part):
            name, _, text = part.partition("=")
            if name in small:
                text = int(text).to_bytes(small[name], "little").hex()
            return f"{name}={text}"
        lines = [" ".join(line.split()[:2] + [as_hex(p) for p in
                                             line.split()[2:]])
                 for line in lines]
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text("\n".join(lines) + "\n")
        return

    if args.cmd == "probe":
        rom = rom_for(args.config)
        table = symbols(rom)
        for line in run(rom, args.timeline.resolve(), args.frames,
                        [resolve(table, s) for s in args.specs],
                        shots_dir=args.shots,
                        pokes=[resolve_poke(table, s) for s in args.poke]):
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
