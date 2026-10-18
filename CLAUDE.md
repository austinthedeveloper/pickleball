# Pickleball '94

A SNES pickleball game in C (816-tcc via PVSnesLib, `PVSNESLIB_HOME`,
default `../pvsneslib`) plus 65816 assembly in `src/asm/`. Simulation and
rules are plain C that also builds on the host for tests. MesenCE runs the
ROM headless for smoke, perf, and golden checks.

## Workflow

- New work goes on `feature/cld-<topic>`, branched off `main`. Stack branches
  only when one depends on another. The user reviews and merges; never push.
- Commit messages start with the milestone task ID when there is one
  (`M4-SAVE-03: ...`); see CONTRIBUTING.md.
- A feature gets an ADR in `docs/decisions/` (next free number after the
  highest there) and a task in `milestones/04_GAME_FEATURES.md`. A defect gets
  a row in `docs/KNOWN_ISSUES.md`, marked Resolved with the branch and test
  when fixed. Unplanned ideas go in `docs/IDEAS.md`; just-for-fun ones in
  `docs/GOOFY_IDEAS.md`.

## Before calling something done

1. `make check`: host tests and asset validation. Warnings are defects.
2. `make perf`: every timeline in `test/perf.json` within its overrun budget
   (almost all are 0). The doubles frame budget is tight (KNOWN_ISSUES #14,
   #22); a per-frame cost in a doubles rally is the usual cause of a failure.
3. `make golden-compare` (after `make golden-record` on the base commit; the
   trace lives in `build/golden/`, not git). Identical means the simulation
   is unchanged. A change that alters play will differ; say so, and the user
   re-records after merging.
4. When AI, rules, positions, or opponent styles change: `make stats` before
   and after, for the ladder in singles and both doubles bots:
   `make stats LATE=0 MATCHES=20 OPPONENT=-2 DIFFICULTY=2 MODE=0` (then
   `MODE=1`, `MODE=3`). It is deterministic. Each table should say
   `ladder order: yes`. Retune with `assets/source/opponents.json`; singles
   plays only each team's captain (first player), so tune the second player
   to change doubles alone.
5. When a simulation value changes (a header constant, a table, or
   `opponents.json`/`paddles.json`): bump `TUNING_VERSION`
   (`include/tuning.h`), add its row to `docs/TUNING.md`, and
   `make tuning-export`. The ports copy `docs/tuning/tuning.json`;
   `make check` fails until it's current (ADR-068).
6. Something visible: screenshot it (below) and look.

## Rules of the code

- **816-tcc is slow at some C.** A multiply is a library call (index arrays
  of structs by walking a pointer or with constant slots, as `STEP_SLOT` and
  `SEPARATE_PAIR` do); pointer dereferences are expensive (hot code copies a
  struct into a static scratch, as `player_step` does). Team is `slot >= 2`.
- **816-tcc miscompiles `a || !b`**: when `a` is true it takes the false
  branch (in `if`s, ternaries, nested; `&&` with `!` is fine). Write
  `b == 0` or split the test. `make check` fails on the pattern
  (`tools/check_tcc_pitfalls.py`; KNOWN_ISSUES #25). Host tests can't catch
  compiler bugs: when the ROM and the host tests disagree, read the
  generated assembly (`make src/<file>.asm`, then delete it).
- **`player_step` exists twice:** C in `src/player.c` (host, under
  `HOST_TEST`) and `src/asm/player_step.asm` (SNES). Change both; the golden
  trace compares them.
- **WRAM is not cleared at power-on.** Initialize every static at boot or
  scene entry.
- **OBJ palettes are all in use** (four players in doubles, objects). New UI
  is BG3 HUD text, drawn by the staged HUD scene: one panel per frame, never
  a whole overlay at once (`src/hud_scene.c`).
- **Save layout changes** follow ADR-032: freeze the old `SaveData` as
  `SaveV<n>`, bump `SAVE_VERSION`, add a migration, and a test that loads the
  old image. Write the save only at safe points (match end, leaving a menu).
- `opponents.json`'s `default_style` must equal the constants in
  `include/ai.h` (the asset build checks).
- Generated files (`assets/generated/`, `build/`, `dist/`) stay out of
  commits.

## Running the ROM headless

- `make smoke SMOKE=<name> SMOKE_FRAMES=<n>` plays `test/smoke/<name>.timeline`
  on the debug ROM and saves screenshots in `build/smoke/<name>/`. Timeline
  lines are `<first> <last> <buttons>` and `shot <frame>`. Runs start from a
  blank battery save unless `KEEP_SAVE=1`.
- `python3 tools/mesen_probe.py probe debug TIMELINE FRAMES name=sym[+off]:size
  ... [--poke SPEC] [--shots DIR]` prints work RAM by symbol on each `shot`
  frame. `--poke` writes RAM to set up a repro (a score, a record) instead of
  playing up to it; the grammar is in the script's docstring. Static symbols
  are not exported: probe a global.
- Pillow is not installed; upscale screenshots with
  `sips -z 672 768 in.png --out out.png`.
- The debug ROM draws a hex status overlay at the top and overruns more than
  release (KNOWN_ISSUES #8); measure timing on release.
- MesenCE keeps saves in `~/Library/Application Support/MesenCE/Saves/`,
  named after the ROM file.
