# ADR-056 — Slice on X, and Hold L or R to Hit Short

Status: **Accepted 2026-10-16**
Milestone: `M4-SHOT-01`, `M4-SHOT-02`. Extends: ADR-019 (shots), ADR-020
(paddle stats). From IDEAS.md ("Slice on X", "Hold L or R for a drop
shot"), the user's picks.

## Why

X, L, and R did nothing in play. The user asked for a slice on X ("a
slower shot meant to give you some time"), and for L or R as a second way
to hit short, so the D-pad is free to move and aim at a sideline while
dropping; pressing away from the net still works. A fast drop (a drive
hit short) needed a downside so it isn't strictly better than a drive.

## The physics we keep

Gravity is one constant (`BALL_GRAVITY_FP`), and the AI predicts flights
and bounces with it in several places, one of them in assembly
(`ai_probe.asm`). A slice with its own gravity (a float) or its own
bounce (a check) would mean carrying that through every prediction, in
the doubles frame budget. So a shot is still a target and a flight time,
and a longer flight is a higher arc. The slice is built from those.

## Slice (X, `SHOT_SLICE`)

- **Slow:** pace 1.875 px/frame (`HIT_PACE_SLICE_FP` 30) with a floor of
  34 frames, against a standard shot's 2.5-3 px/frame and 24 frames. From
  mid-court and the net it arrives 8-20 frames later than a standard
  shot: time to get back. Its ceiling is the standard shot's (46 frames),
  so it never goes as high as a lob. From the baseline every standard shot
  is already at that ceiling, so there the slice is no slower.
- **Never deep:** neutral, or short when aimed away from the net (or with
  L or R). Aimed toward the net it is still neutral. That is the price:
  it buys time but doesn't push anyone back.
- **Steady:** off a stretch it sprays half as much as a standard shot
  (10 px wide and 8 long, against 20 and 16).
- Sounds duller and quieter than a lob (pitch 2 down, volume 10); the
  small spark.

## Hold L or R: short

`build_intent` sets the aim to "away from your net" while L or R is held
(the left team's -1, the right team's +1); the D-pad still moves and still
aims at a sideline. So every shot does what it does aimed away: B a short
shot, A a fast drop, Y a dink, X a short slice. Serving with L or R held
is a short serve, as pressing away is.

## Fast drop downside

A drive aimed short (L+A, or A pressed away from the net) is fast and
flat. Off a stretch it now flies 34 px long (`HIT_FAST_DROP_STRETCH_LONG`,
against a drive's 22), past the kitchen into the middle of the court,
where it sits up to be attacked. Met in the sweet spot it lands just past
the kitchen line as before. Its flat arc is the other risk: from close to
the net it can catch the tape, and from the back it crosses the net about
20 px up, in a net player's reach.

## CPUs

CPUs don't slice yet, and never drive short, so the ladder and the golden
trace are unchanged (`make golden-compare` identical). They return slices
and fast drops with their usual prediction. A per-style slice choice is a
follow-up (IDEAS.md).

## Cost

`player_step` (C and assembly) copies the shot byte as it is: no change.
`build_intent` gains one test per human pad; `make perf` zero overruns.

## Tests

- `test_input.c: test_slice_and_short_hold`: X is a swing and a slice; L
  or R aims away for either team while the D-pad moves and aims at a
  sideline.
- `test_collision.c: test_slice_and_fast_drop`: a slice is slower than a
  standard shot and no higher than its ceiling, neutral when aimed deep,
  short when aimed away, mirrored, and sprays less; a fast drop lands short
  in the sweet spot and past neutral off a stretch.
- MesenCE (ball machine, the same stretched contact at frame 355): B lands
  at x 219, X 213 (and half as wide), A 227, L+A 210, L+B 193, L+X 184.
