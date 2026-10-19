# ADR-076 — The Push: Hold Toward the Net Longer to Hit Deeper

Status: **Accepted 2026-10-19**
Task: `M4-SHOT-04`. Amends: ADR-019 (shot targets).

## Why

The D-pad aims at contact, and only its last direction counts: toward the
net is deep, nothing is neutral, away is short (ADR-019). Depth was three
fixed spots, and the deep one, 12 px inside the baseline, could never go
long, so hitting deep had no risk and no skill in it. A push that builds
the longer it's held makes depth something you judge: a little for a
mid-court ball, more to pin them back, too much and it sails out.

## Decision

- **The push** is the number of frames the D-pad has stayed toward the
  player's own net (Right on the left team, Left on the right), counted
  by the input layer into `PlayerIntent.push` (to 255). Any other frame
  resets it: no direction, away, or L/R's short hold. Up or Down with it
  still counts, so a deep shot can still aim at a sideline.
- **A standard shot or drive aimed toward the net** lands at
  `HIT_PUSH_START_X` (212, 8 px past the neutral spot) plus one pixel for
  every 2 frames of push, up to `HIT_PUSH_MAX_X` (252, 12 px past the
  baseline). For the left-team hitter, mirrored for the right:

| Push (frames) | Lands at x | Where |
|---|---|---|
| 1 (a tap) | 212 | mid-deep |
| 32 (`HIT_PUSH_DEEP`, about half a second) | 228 | the old deep spot |
| 57 | 240 | on the baseline: in |
| 58 (`HIT_PUSH_OUT_FRAMES`) and up | 241 to 252 | out |

- A stretched contact still sprays long on top (ADR-019), so a deep push
  off a stretch goes out sooner.
- **Unchanged:** lobs (Y toward the net stays at the deep lob spot),
  dinks, slices, short shots, and serves (the charged serve has its own
  meter, ADR-044).
- **The CPU pushes `HIT_PUSH_DEEP`** on a deep shot, so its shots land
  where they always did and it never pushes one out. `make stats` is
  identical before and after in singles and both doubles bots (ladder
  order: yes).
- Holding toward the net also moves you forward, as it always did: a long
  push is a run at the net. Whether that's too punishing is for play to
  say; the constants are in `include/collision.h`.

## Code

- `include/input.h`: `PlayerIntent` gains `push` and a `reserved` byte, so
  it is 14 bytes on the host and the SNES (the asm reads only the first
  10). `input_configure` zeroes them; WRAM isn't cleared at power-on.
- `src/input_state.c`: `build_intent` counts the push; slots without a
  human pad get 0.
- `src/collision.c`: the deep target from the push.
- `src/ai.c`: the CPU's push.
- `TUNING_VERSION` 5 (`docs/TUNING.md`: the row, and port rule 9, how the
  push counts). The conformance trace prints each slot's push.
- `tools/mesen_probe.py` recorded 48 bytes of intents for the lockstep,
  4 × the old 12. With 14-byte intents, slots 1-3 read misaligned and the
  doubles run "differed" on a CPU's `aim_y`. It records 56 now, and
  `tools/lockstep.c` refuses a record whose intent size isn't the host's.

## Tests

- `test_input.c: test_push`: counts toward each team's own net, keeps
  counting with Up or Down, resets on release, away, or L/R, and
  saturates at 255.
- `test_collision.c: test_push_depth`: standard and drive land further
  with every push (212 to 252), the old deep spot at 32, in at 57, out
  from 58, capped; the right team mirrors; a lob ignores the push. The
  other deep-aim tests use the CPU's push.
- On the ROM: a probe of `push.timeline` counts 1 to 20 over a 20-frame
  hold and resets on release; the generated asm for the target reads the
  byte at offset 12, halves it, adds 212, and caps at 252.
- `make lockstep` adds `push.timeline`: a singles match with Right held
  in windows of 20 to 120 frames, identical frame for frame. Its P1 never
  reaches a ball during a push, so the ROM check above covers the target.
- Golden trace identical: its P1 never holds a direction. Zero overruns
  in `make perf` (the worst frames rise about 2 scanlines).
