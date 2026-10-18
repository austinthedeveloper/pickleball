# ADR-068 — A Tuning Reference for Ports

Status: **Accepted 2026-10-18**
Task: `M4-PORT-01`. Reference: `docs/TUNING.md`.

## Why

The SNES game is where play is tuned, and the Godot remaster
(`../pickleball-port-kit`) copies its numbers integer for integer. The
port was copied from the SNES repo at 0.18.1. Since then the SNES ladder
was rebalanced (ADR-062), and slice (ADR-056, 057), CPU serve power and
duds (ADR-058), and three modifiers (ADR-053) were added. Nothing told
the port.

The numbers were documented where they are defined, in header comments
and ADRs, but:

- About 70% of them are C enums and `#define`s across a dozen headers,
  and two tools read them by regex.
- Some of the feel is in derived tables (paddle profiles, opponent styles
  after a difficulty, the ball's step pattern) that exist only once the
  code runs.
- Nothing separated a retune from a refactor, so a port couldn't tell
  which commits mattered.
- The golden trace is raw SNES memory from boot, so a port can't use it.
- Some behaviour that sets the feel isn't a constant at all: the frame
  order, the CPU think schedule, the integer rounding. That was written
  nowhere as a rule a port must follow.

## Decision

- **`docs/tuning/tuning.json`, generated and committed.** `make
  tuning-export` lists every enum member and object-like `#define` in the
  simulation headers, plus the `#define`s at the top of `src/ai.c`. A
  host program built from the game's own sources prints their values and
  the derived tables, so no number is typed twice. Each constant carries
  its header comment as a note. Its git history is the tuning changelog.
- **`TUNING_VERSION`** (`include/tuning.h`), bumped with any value change,
  with a row in `docs/TUNING.md`. `make check` runs `tuning-check`: the
  export must be current, a changed value needs a new version, and the
  version needs its row.
- **`make conformance`:** fixed match scenarios (singles, doubles, KID, a
  stack of modifiers) played on the host from `game_new_match`. Each
  frame is printed as plain integers: every active slot's intents, then
  the state. An exact port diffs against them line by line. The traces
  are generated, not committed (about 3 MB); they are rebuilt from the
  commit being matched.
- **`docs/TUNING.md`:** the units, the port rules (integer semantics,
  frame order, hit stop, tie priority, the step-pattern ball speed, the
  CPU schedule, frame-clock periods instead of randomness), the values
  that are SNES limits, and where each group lives.
- `g_ai_ice_coast` (was `s_ice_coast`) is visible so the export can print
  it. The comments on the swing and diagonal constants in `player.h` were
  out of place; they were moved. Neither changes the ROM's behaviour.

## Not done

- **Moving every tunable into a `gameplay.json` that generates the
  headers** (as `opponents.json` and `paddles.json` do). The export gives
  ports the same single file without touching the 816-tcc hot paths or
  the asm's shared constants. Revisit if hand-editing headers becomes the
  bottleneck.
- **Committing the conformance traces.** They are large, and any tuning
  change rewrites all of them. They are deterministic, so regenerating
  them is enough.
- **Hashing the traces, as the Godot golden test does.** A hash says
  *that* a port differs, not where. Plain lines give the first bad frame
  and field.

## Consequences and verification

- Every retune is a commit that touches `docs/tuning/tuning.json`, and
  the port follows by diffing it.
- The ROM's CPU schedule counts from boot, while the traces count from
  the match's start; `docs/TUNING.md` says to follow the traces.
- Verified: `make check` (including `tuning-check`), golden trace
  identical, `make perf` unchanged.
