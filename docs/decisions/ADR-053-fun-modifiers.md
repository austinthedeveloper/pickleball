# ADR-053 — Ghost Ball, Popcorn, and Ice

Status: **Accepted 2026-10-16**
Milestone: `M4-MOD-02`. Extends: ADR-033 (modifiers). Depends on: ADR-025
(ball pace), ADR-038 (footwork), ADR-044/049 (serve, ball speed).

## Why

The user liked LAVA and asked for more fun modifiers. From the list in
GOOFY_IDEAS.md they picked these three to start.

## Title page

Three rows join the MODIFIERS page, one bit each, like the first four. They
combine freely with each other and with the old ones.

| Row | Off | On | What it does |
|---|---|---|---|
| VISION | NORMAL | GHOST | High in a live rally, the ball shows only in a blink; its shadow always |
| RALLY | NORMAL | POPCORN | Every hit in a rally speeds the ball up a little, up to +50%; the next serve starts over |
| FOOTING | NORMAL | ICE | Players speed up slowly and slide when they let go |

- **Save:** the bits are 16, 32, and 64 in the same `modifiers` byte
  (`MODIFIER_ALL` is now 127). Old saves only ever stored bits 0-3
  (`modifiers_valid`), so they load with the new rows off. No layout
  change, no migration.
- The page box grew to nine rows; it still clears the prompt.

## GHOST BALL

- `render_ball`: when the ball is 8 px up or more (`MODIFIER_GHOST_Z`) in
  a live rally, its object shows on 2 frames in 16 and is hidden otherwise.
  The shadow always draws. So a ball is visible as it's hit low, at the
  bounce, and in a brief flash mid-flight; the rest is reading the shadow.
- The hit spark is drawn at the ball's height, so on a ghost frame it's
  hidden too. Otherwise it showed where the ball was for its 10 frames.
- Presentation only: the simulation, the golden trace, and the CPUs are
  unchanged (the CPUs read the ball's state, so they're not fooled).
- Not in the close-call replay, which is a close-up of the landing.

## POPCORN

- The ball's pace is the per-frame step table from ADR-025/033
  (`g_ball_pace`). A POPCORN match is always paced. Each hit in a rally
  (`game_contact`) adds a level, up to six (`g_popcorn_level`): one more
  ball step on one tick in every 12, 5 a cycle, about +8% a level and +50%
  at the cap (`modifiers_popcorn_add`).
  - Arcs and landing spots are unchanged, as with FAST BALL: the ball only
    takes its steps sooner.
  - The levels' ticks are never 2 mod 3, FAST BALL's extra step, so one
    tick gains one or the other, never both.
  - A hit adds six increments to the table: no rebuild on the busy hit
    frame.
- On the point result's second frame, every level is taken back
  (`modifiers_popcorn_remove`), so each serve starts at the match's pace.
  A new match rebuilds the table anyway.
- **The CPUs' early-out:** `predicted_frames` (the frames until the
  landing, for the swing lookahead's skip) must never be more than the real
  count. With POPCORN up it is cut to 3/4 of the steps (levels 1-4), 5/8
  (5-6), or 1/2 with FAST BALL too.
- Practice doesn't speed up (it has no points to reset on).

## ICE

- `player_step`'s acceleration and friction are now variables
  (`g_player_accel`, `g_player_diagonal_accel`, `g_player_friction`), set
  at match start (`players_set_ice`) and to normal by
  `players_initialize`. On ICE they are 2, 2, and 1 (normally 4, 3, and 6),
  in 1/16 px a frame: a run takes 12 frames to reach top speed instead of
  6, and letting go from it slides 17 px instead of 3.
  - Both `player_step`s change, the C one (host) and
    `src/asm/player_step.asm` (SNES). The asm reads the three words with
    long loads; its friction is now a subtraction and a sign test
    (`v - f`, `v + f`), the same result as the C. The golden trace (ICE
    off) is identical; in MesenCE, P1's run and slide on ICE match the host
    test to the sixteenth of a pixel.
  - The `ASM_ACCEL`, `ASM_DIAGONAL_ACCEL`, and `ASM_FRICTION` constants
    are gone from `asm_shared.h`.
- **CPUs:** on ice, a CPU lets go of the pad once its slide would carry it
  to its target (`move_toward_ice`, a table of slide distances by speed),
  so it coasts onto its spot instead of sliding past and back. The swing's
  slide prediction (`ai_slide`, ADR-038) uses the match's friction.

## Measured

`test_modifiers.c`, bot (the CPU AI, mistiming every 5th shot) against the
default CPU, 12 singles matches each:

| Modifier | Bot wins | Points | Shots per point | POPCORN level reached |
|---|---:|---:|---:|---:|
| none | 6 | 104 | 6.4 | 0 |
| POPCORN | 3 | 82 | 4.9 | 6 |
| ICE | 5 | 86 | 4.4 | 0 |
| POPCORN + FAST BALL | 5 | 84 | 3.7 | 5 |

- Faster rallies end sooner, as with FAST BALL (4.6).
- On ice the CPUs still rally, a little less well: about 4.4 shots a point.
- `make stats` (singles, doubles, net bot) is identical with the modifiers
  off, and so is the golden trace.

## Frame budget

`make perf`, release ROM: every timeline 0 overruns. The new
`modifiers_fun.timeline` (FAST BALL, GHOST BALL, POPCORN, and ICE; a
doubles ladder match) peaks at 198 scanlines of 225, against 196 for
`modifiers_doubles.timeline`.

## Tests

- `test_modifiers.c`:
  - `test_values_and_editor`: the new rows' labels and values, each row is
    its bit, Up from BALL wraps to FOOTING.
  - `test_popcorn_pace`: five steps a level a cycle, never on FAST BALL's
    ticks, the lookahead entries match the cycle's start, and removing
    every level gives back the table.
  - `test_ice`: top speed in 6 frames and a slide under 4 px normally; 12
    frames and 276/16 px on ICE; a new match without it is back to normal.
  - `test_matches`: POPCORN reaches the cap (and nearly, with FAST BALL),
    no serve starts with pace left over, rallies still happen on ICE.
- `test_layout.c`: the removed asm constants.
- `test/smoke/modifiers_fun.timeline` (in `make perf`): the page with the
  three on, and a doubles match. MesenCE: the ball hidden over the net
  with its shadow showing, POPCORN's level rising with each hit.
