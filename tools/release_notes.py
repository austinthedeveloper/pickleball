#!/usr/bin/env python3
"""Release notes for `make publish`: an optional hand-written intro, the
commits since the previous release tag, the tuning version (ADR-068), and
the ROM's checksum. Stdlib only."""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path


def git(*args):
    return subprocess.run(["git", *args], capture_output=True, text=True,
                          check=False).stdout.strip()


parser = argparse.ArgumentParser()
parser.add_argument("--tag", required=True)
parser.add_argument("--rom", required=True, type=Path)
parser.add_argument("--intro", type=Path)
args = parser.parse_args()

previous = git("describe", "--tags", "--abbrev=0", "--match", "v*", "HEAD")
span = f"{previous}..HEAD" if previous else "HEAD"
# One line per commit, merges left out.
subjects = git("log", "--no-merges", "--format=- %s (%h)", span).splitlines()

lines = []
if args.intro:
    lines += [args.intro.read_text("utf-8").strip(), ""]
lines += [f"## Changes since {previous}" if previous else "## Changes", ""]
lines += subjects or ["- (no commits)"]


def tuning_version(ref):
    text = git("show", f"{ref}:docs/tuning/tuning.json")
    return json.loads(text)["tuning_version"] if text else None


# Ports follow the tuning version (docs/TUNING.md).
now = tuning_version("HEAD")
before = tuning_version(previous) if previous else None
if now is not None:
    moved = "unchanged" if before == now else \
        f"changed from {before}" if before is not None else "first export"
    lines += ["", "## Tuning", "",
              f"- Tuning version {now} ({moved} since {previous or 'start'}; "
              "`docs/TUNING.md`)."]
lines += ["", "## Download", "",
          f"- `{args.rom.name}`: the ROM. Runs in MesenCE, bsnes, and "
          "flash carts; a physical cart needs 120 ns ROM and 2 KiB battery "
          "SRAM.",
          f"- SHA-256: `{hashlib.sha256(args.rom.read_bytes()).hexdigest()}`",
          "- `README.md`: the player guide. `build-report.md`: ROM and "
          "budget report.", ""]
print("\n".join(lines))
