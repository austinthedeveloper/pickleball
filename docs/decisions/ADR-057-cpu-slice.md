# ADR-057 — CPUs Slice

Status: **Accepted 2026-10-16**
Milestone: `M4-SHOT-03`. Extends: ADR-023 (CPU styles), ADR-056 (the
slice).

## Why

ADR-056 gave humans the slice on X; CPUs never hit it. A team with a soft
game should use it too.

## Decision

A new style knob, `slice` (0-8): a slice every Nth rally shot, 0 never.
In `ai_plan_shot` it comes after the dink pattern and before the standard
shot, so put-aways, block volleys, drops against net players, lobs, and
dinks all keep priority, as `dink_period` does. A CPU slice is neutral
(slices are never deep). `default_style` and the partner style are 0, so
the default CPU and a human's partner are unchanged.

| Team | Player | slice |
|---|---|---|
| SNOWBIRDS | WALT (doubles only) | 3 |
| GOTH CLUB | RAVEN, VLAD | 3 |
| CHAMPS | ACE, JADE | 4 |

The others never slice. MARGE stays at 0: in singles a slicing Marge went
from 32 bot wins of 60 to 0 under the first rule tried (below).

## Rejected: slicing a stretch

The first rule sliced a stretched contact, matching what the shot is for.
On the deliberately late returns it gutted the ladder: their spray is the
difficulty, and a slice sprays half as much (MARGE 32 → 0, RAVEN 15 → 0
bot wins). Excluding the late returns left the stretches a CPU really
can't avoid, and with footwork there are almost none: 5 of RAVEN's 152
stretched contacts over the test suite were not late ones, so she never
sliced in 10 matches. The period is visible and tunable.

## Balance

`make stats LATE=0 MATCHES=20 OPPONENT=-2 DIFFICULTY=2`, bot wins of 60,
before → after (unchanged rungs omitted); every table keeps
`ladder order: yes`:

| Mode | GOTH CLUB | CHAMPS |
|---|---|---|
| Singles (`MODE=0`) | RAVEN 15 → 13 | ACE 0 → 0 |
| Doubles, regular bot (`MODE=1`) | 18 → 14 | 0 → 1 |
| Doubles, net bot (`MODE=3`) | 24 → 18 | 2 → 1 |

The slices make GOTH CLUB a little harder, still between PIRATES and SURF
CREW. WALT's slices change nothing measurable: the Snowbirds lob most of
their shots, which come first.

## Tests

- `test_ai.c: test_slice`: RAVEN slices 17 of 127 rally swings in 10
  singles matches against the bot (between a tenth and a third); with the
  knob at 0, none. The default style's knob is 0.
- MesenCE: a singles match against RAVEN on KID (the swing assist keeps
  the rally going); her latched shot goes standard, slice, dink.
- The golden trace (DAD) is identical; `make perf` zero overruns.
