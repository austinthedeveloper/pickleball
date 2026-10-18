# ADR-067 — Doubles Frame Headroom

Status: **Accepted 2026-10-17**
Resolves: KNOWN_ISSUES #35. Follows: KNOWN_ISSUES #14, #22 (the doubles
budget), ADR-064 (flair's cost).

## Why

Since 0.23.0 `doubles_ladder` and `modifiers_fun` were allowed one
overrun: their heaviest frames sat at 222-224 of 225 scanlines, so the
build ID baked into the ROM (which moves code) decided whether a frame
dropped. The fix had to change nothing in play: the golden trace and the
ladder stats are the check.

## What the profile showed

`make release PROFILE=1` (after a `make clean`: the Makefile does not
rebuild objects when only `PROFILE` changes) and the stage maxima, worst
scanlines in a doubles frame:

| Work | Worst |
|---|---|
| Drawing the four players (camera and bodies) | 98 |
| The HUD's staged step | 86 |
| One CPU's swing lookahead and movement | 55-72 |
| A new match set up in `game_update` (a rematch) | 159 |

No single routine dominates: the drops came from a HUD step landing on a
frame the CPUs had already filled, and from rematches.

## Decision

- **A match's HUD steps wait for a lighter frame**, as the title's
  already did (M4-UI-03): past scanline 136 (225 less the heaviest step,
  about 86, less a margin), for at most 20 frames in a row (forced
  sooner, a waited step landed on a heavy frame itself). Display only.
- **`ai_slide` is cached** per slot and axis: within a match it depends
  only on the speed (the paddle's limit and the friction are the
  match's), and a running player's speed holds. On ICE it was a long
  loop every think. Identical results.
- **The match's save write follows a frame later** (`save_flush`, from
  the match result's next frame): the records are in RAM at once, so the
  initials and unlock cards open on the same frame as before.
- **A rematch's frame is a scene switch**, like every new match's
  (`g_match_started`): `game_new_match` and the first draw run past one
  VBlank, which main.c already excused when the config changed.
- **The profiler** records the loop count of each stage's maximum
  (`g_stage_max_loop`) and of the first and last overrun
  (`g_overrun_first_loop`, `g_overrun_loop`, `g_overrun_first_work`):
  the stage maxima alone could not say which frame they came from.

## Tried and rejected

- One intercept plan a frame (a second CPU waiting a frame): singles
  unchanged, but doubles reshuffled (PIRATES 42 to 32 of 60 with the
  regular bot, TWINS 26 to 49 with the net bot, both ladders out of
  order). A retune, not a perf fix.
- Deferring all of the match's bookkeeping two frames: the initials card
  opened later, and the golden run's timed presses went elsewhere.

## Result

`make perf`: zero overruns on every timeline; worst `modifiers_fun` 214,
`modifiers_doubles` 212, `doubles_ladder` about 203 (were 222-224). The
budgets of 1 are gone. Ladder stats identical in all three modes. The
golden trace is identical but for the frame timers (the phase timer and
the players' animation counters) at the rematches, where 0.23.0 dropped
a frame and this build does not: masked, every frame matches.
