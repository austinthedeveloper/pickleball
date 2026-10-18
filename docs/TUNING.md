# Tuning reference

The SNES game is where play is tuned; the ports (the Godot remaster in
`../pickleball-port-kit`) copy its numbers. This page says where those
numbers are, what units they are in, which ones a port must copy exactly,
and how a port finds out that they changed (ADR-068).

## The files

| File | What it is |
|---|---|
| `docs/tuning/tuning.json` | Every simulation constant and table, with the value the game compiles and the header comment as a note. Generated, committed. **Ports read this, not the C.** |
| `include/tuning.h` | `TUNING_VERSION`. Bumped whenever a value in the export changes. |
| `make tuning-export` | Rewrites `tuning.json` (`tools/tuning_export.py`, `tools/tuning_dump.c`). |
| `make tuning-check` | Part of `make check`: fails while `tuning.json` is stale, or a value changed without a version bump, or the version has no row below. |
| `make conformance` | `build/conformance/<scenario>.trace`: fixed match scenarios played frame by frame on the host, as plain integers (`tools/conformance.c`). Not committed; generate them at the commit a port is matching. |
| `make stats` | The ladder tables (`tools/match_stats.c`). A port that can't be bit-exact compares its own ladder to these. |

`tuning.json` has five parts: `units`; `constants`, grouped by source file
(`include/ball.h`, ...), each `{value, note}`; `tables`, each `{note,
value}`: the arrays and derived tables (paddle profiles, opponent styles
before and after each difficulty, the ball's step patterns); and `sources`,
short hashes of `opponents.json`, `paddles.json` and `career.json` so a
port can tell its copies are current.

## The workflow

**On the SNES side, to retune:**

1. Change the header constant or `assets/source/*.json`.
2. Measure it: `make stats` for anything the AI or rules touch (CLAUDE.md
   step 4), `make golden-compare` (it will differ: say so).
3. Bump `TUNING_VERSION` in `include/tuning.h` and add a row to the
   versions table below: what changed, and the ADR.
4. `make tuning-export` and commit `docs/tuning/tuning.json` with the
   change. `make check` fails until you do; `tuning-export` refuses to write
   changed values under the old version.

A refactor that changes no value leaves `tuning.json` byte-identical. A
comment change rewrites a note, and naming a value the code already had
adds an entry; both need a re-export but no bump. A changed or removed
value needs the bump.

**On the port side, to catch up:**

1. Compare the port's recorded tuning version with `TUNING_VERSION`.
2. `git diff <old commit> -- docs/tuning/tuning.json` lists every value
   that moved; the versions table says why.
3. Apply them, then replay the conformance scenarios from that commit.
   An integer-exact port (the Godot one is) should match every line; the
   first line that differs is the first frame it went wrong.
4. A port that isn't exact compares `make stats` ladders instead (same
   ladder order, win rates within a few matches).

## Units

| | |
|---|---|
| Time | One frame is one simulation step, 60 a second (NTSC). There is no PAL timing. |
| Distance | Ground pixels on the flat court: x along it (baselines 16 and 240, net 128, kitchen lines 92 and 164), y across it (sidelines 90 and 170). Rules, physics and AI all run here; the 3/4 view is a display projection (ADR-011). |
| Fixed point | 12.4. A name ending `_FP` is in 1/16 px: a position, a speed in 1/16 px per frame, or gravity in 1/16 px per frame². |
| Fractions | `_256` is n/256; the paddle multipliers are n/64. |
| Scale | x: 224 px for 44 ft (5.09 px/ft). y: 80 px for 20 ft (4 px/ft). |

**The court is squashed across.** The same speed covers about 27% more
feet in y than in x. Every distance, speed and AI threshold was tuned on
these units. Keep them in a port and scale the *picture*, not the
simulation; a true-proportion court will play differently.

## Port rules

These are behaviours, not constants, so they aren't in `tuning.json`.
The feel depends on each of them.

1. **Integer math, C semantics.** Integer `/` truncates toward zero; `>>`
   floors. `ball_retain` scales the magnitude and restores the sign, so
   +v and -v lose the same speed (`src/ball.c`). `ball_launch` rounds vx
   and vy half away from zero and *floors* vz, so the ball lands on
   exactly the frame asked for. u8 and u16 counters wrap. Floats will
   drift off the conformance trace within a rally.
2. **The frame order.** The CPUs think first (`ai_update`, or
   `ai_hold_intent` on a frame they skip), then `game_update`:
   `frame_count` counts up; on a hit-stop frame nothing else moves; then
   `phase_timer` counts, players move, teammates separate, and the phase
   runs. In a live rally that is contact (against where the ball is
   before it moves), then the rules, then the ball's steps, then the
   bounce, net and out events (`src/game.c`).
3. **Players and the ball integrate in opposite orders.** A player
   updates velocity, clamps each axis to its limit on its own (a
   diagonal's limit comes from `player_diagonal_limit`), then moves; a
   court bound stops that axis dead. The ball moves by its velocity,
   then gravity changes vz.
4. **Hit stop.** Every contact freezes play for `GAME_HIT_STOP_FRAMES`
   frames. The CPUs still think on those frames, and `frame_count` still
   counts.
5. **Contact ties go to the lowest slot.** The slots are checked 0 to 3
   and the first eligible swing takes the ball; a team can't hit twice in
   a row (`collision_resolve_player_hits`).
6. **Difficulty and FAST BALL change the step pattern, not the
   velocity.** The `ball_pace_*` tables give the ball's steps on each
   frame (0, 1 or 2): KID plays 2 frames in 3. POPCORN adds steps
   (`popcorn_steps`). The AI predicts with the same pattern. Scaling the
   ball's velocity instead changes every flight time and the AI's timing.
7. **The CPU schedule.** A CPU thinks every frame when it plays the ball,
   is alone on its side, or serves. Otherwise a doubles CPU covering its
   lane thinks only on frames where `(frame & 3) == slot`, and repeats its
   last movement in between (ADR-018, ADR-043). This started as a frame
   budget measure, but the ladder was tuned with it, so copy it. Count
   `frame` from the match's first frame, as the conformance traces and
   the host harnesses do. (The ROM counts from boot, so its phase depends
   on time spent in menus.)
8. **No randomness in a match.** CPU mistakes are periods: late every
   Nth return (`late_period`), a dink every Nth shot, and so on. A CPU's
   serve is a dud when `frame_count % serve_dud == 0` as it serves.
   `frame_count` restarts at 0 with each match. The only real RNG is Open
   Play's park (xorshift16, `src/open_play.c`), outside matches.
9. **Use the tables as given.** `player_diagonal_limit` (x 181/256),
   `ai_ice_coast`, and the paddle multipliers are tables because a
   multiply is costly on the SNES, but their rounded values *are* the
   tuning. Copy them; don't recompute them in floats.

## SNES limits baked into the numbers

These numbers exist because of the SNES. Copy them to keep the same feel.
Change them only on purpose, through an ADR in the port.

| Value | Why it is what it is | If a port changes it |
|---|---|---|
| `AI_PREDICT_MAX_STEPS` = 60 | The AI's prediction cost on the 65816 | Lobs past the horizon become readable; the CPU gets better |
| `HIT_LOB_MAX_FRAMES` = 50, `HIT_FLIGHT_MAX_FRAMES` = 46 | The arc's apex stays below the HUD, and inside the AI's horizon | Higher lobs, and a different AI |
| The lane-cover think every 4th frame | The frame budget in doubles | Doubles CPUs react faster; the ladder needs retuning |
| `PLAYER_CPU_MAX_SPEED_FP` and the speeds in 12.4 | The integer units | Nothing, as long as the units are kept |
| Everything in `src/render*.c`, the projection, camera, palettes, HUD | Presentation | Free to change: none of it feeds the simulation |

## Where each group lives

| Group | Source | ADRs |
|---|---|---|
| Court and player bounds | `include/court.h` | ADR-011 |
| Ball | `include/ball.h`, `src/ball.c` | |
| Shots: reach, sweet spot, targets, paces, flight times | `include/collision.h`, `src/collision.c` | ADR-019, 037, 049, 056 |
| Movement and swing timing | `include/player.h`; `src/player.c` (host) and `src/asm/player_step.asm` (SNES) | ADR-020, 053 |
| Serve | `include/serve.h`, `src/serve.c` | ADR-044, 058 |
| Match phases and timers | `include/game.h` | |
| CPU | `include/ai.h`, the `#define`s at the top of `src/ai.c` | ADR-018, 023, 035–039, 043, 057 |
| CPU personalities and the ladder | `assets/source/opponents.json` | ADR-023, 055, 062 |
| Paddles and hats | `assets/source/paddles.json`, `career.json` gear | ADR-020, 047 |
| Difficulty | `src/options.c` (`options_apply_difficulty`, `options_ball_per_cycle`) | ADR-025 |
| Modifiers | `include/modifiers.h`, `src/modifiers.c` | ADR-033, 053 |

## Versions

One row per `TUNING_VERSION`. `make tuning-check` wants a row for the
current one.

| Version | Release | What changed |
|---|---|---|
| 1 | 0.23.1 | The first export: the game as of 0.23.1, including the ladder rebalance (ADR-062), CPU serve power and duds (ADR-058), slice (ADR-056, 057), and GHOST BALL, POPCORN and ICE (ADR-053). |
