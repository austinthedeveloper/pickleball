# ADR-069 — The ROM and the Host in Lockstep

Status: **Accepted 2026-10-18**
Task: `M4-PORT-02`. Resolves: KNOWN_ISSUES #36, #39. Records: #37, #38.
Follows: ADR-068 (the tuning reference and conformance traces).

## Why

The host tests, `make stats`, and the conformance traces for ports all
run the C on the host. The ROM runs something else:
- `player_step` in asm (`src/asm/player_step.asm`);
- the AI's ball probe in asm (`src/asm/ai_probe.asm`);
- the rest of the C as 816-tcc compiles it, a compiler known to
  miscompile (KNOWN_ISSUES #25).

Nothing compared the two. CLAUDE.md said the golden trace did, but the
golden trace compares the ROM with an earlier ROM: a change to the C twin
alone passed every check. So "the ports match the host" did not yet mean
"the ports match the game".

## Decision

- **One schedule definition.** `CPU_THINKS(slot)` in
  `include/cpu_schedule.h` holds the rule for when a CPU thinks (a lane
  cover on one frame in four, staggered by slot). It is used by `main.c`
  (`UPDATE_CPU`, the demo's P1), `test_step_scheduled`, `match_stats`,
  `conformance` and `lockstep`, which each had their own copy. It counts
  `g_loop_count`. On the SNES that is the main loop's count since
  power-on; host programs set it: the harnesses to their frame, and the
  lockstep to the ROM's.
- **`make lockstep`.**
  - `mesen_probe.py lockstep-record` runs a timeline on the release ROM and
    records, every frame, the match config, the input owners, the loop
    count, the intents, and the simulation state (game, players, ball,
    serve, serve charge, AI slots).
  - `tools/lockstep.c` starts at the timeline's first match frame and
    replays the match on the host. Humans play the ROM's recorded intents;
    the CPUs think on `CPU_THINKS` with the ROM's loop count. After every
    update it compares every field, the CPUs' intents included, and stops
    at the first difference with its frame, struct and field.
  - It runs `golden` (singles), `doubles_ladder` (three CPUs), and
    `modifiers_fun` (doubles with FAST BALL, GHOST BALL, POPCORN and ICE).
- **It found a bug at once (#36).** A lane cover's held intent was never
  initialized, so its first skipped frames replayed power-on RAM:
  `move_x` -25829. `ai_initialize` now clears it.
- **Which exposed a frame overrun (#39).** With the held intent fixed,
  `doubles_ladder` plays a different match, and one of its points shows
  a quip. The quip's text was drawn in both halves on one frame: 242 of
  225 scanlines. It is now drawn a half per frame, as the point's text
  is.

## Rejected: counting the schedule from the match

Keying the stagger to `g_game_state.frame_count`, so that a match's phase
no longer depends on menu time (#37), was built and measured. It made
the ROM reproducible, but:
- **Perf.** It changed every doubles match and moved which frames carry
  a lane cover's think. `modifiers_doubles` overran once (the doubles
  budget is that tight, ADR-067), as did `doubles_ladder` (#39, fixed
  since), and the attract demo played a different match.
- **Balance.** The phase alone, offset 0 to 3, moves `MODE=1` TWINS from
  10 to 22 wins of 60 and CHAMPS from 5 to 11. Every fixed phase reads
  `ladder order: NO` in `MODE=1` (#38).

Fixing the phase is a balance and frame-budget change, not a cleanup.
The lockstep doesn't need it, because it reads the ROM's count. The
ports don't either: a port's phase is arbitrary in the same way, and the
conformance traces count from their first frame.

## Not done

- **R25 (`ai_think` on a static copy).** It needs signature changes across
  about eight helpers for a few scanlines a think. The doubles budget
  passes since ADR-067.
- **R14 (one frame-budget defer helper).** It touches the HUD files that
  the `feature/cld-cleanup` branch splits; it goes after that branch.

## Verification

- `make lockstep`: the singles, doubles, and modifiers matches are
  identical on every frame.
- A one-character change to the host C `player_step` alone (a 1/16 px
  drift) fails at match frame 130 with `g_players[0].x_fp: host 209,
  ROM 208`.
- `make check`; `make stats` identical in `MODE` 0, 1 and 3; golden
  trace and `make perf` as reported in the branch's commit.
