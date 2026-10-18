# ADR-058 — CPU Serve Power

Status: **Accepted 2026-10-16**
Milestone: `M4-SERVE-02`. Extends: ADR-044 (the charged serve), ADR-023
(CPU styles).

Amended by [ADR-062](ADR-062-ladder-rebalance.md): a CPU short serve goes at the classic power, whatever `serve_power` says.

## Why

Since ADR-044 a human can charge a serve; CPUs all served at the classic
power and never missed. Personalities should show in the serve too: the
bangers serve hard and sometimes dump one into the net.

## Decision

Two style knobs:

- `serve_power` (4-30): the charge level the CPU serves at, the same scale
  as the meter (4 is the classic 40-frame serve, 30 the fastest, 27
  frames). The CPU writes it to `g_serve_charge.power` as it presses;
  `serve_update` keeps it, since a CPU never runs the meter.
- `serve_dud` (0-30): about one serve in N is a dud into the net (power
  0, ADR-044's dud), 0 never. The game has no RNG: a serve is a dud when
  the frame count is a multiple of N. The count at a serve depends on how
  long the rallies before it ran, so duds land unpredictably, and a match
  still replays the same.

The default style and a human's partner serve classic, never a dud.

| Team | Player | serve_power | serve_dud |
|---|---|---|---|
| HENDERSON | KIDDO (doubles) | 18 | 10 |
| GYM BROS | BRAD, CHAD | 30 | 12 |
| CHAMPS | ACE, JADE | 30 | 20 |
| everyone else | | 4 | 0 |

DAD serves classic, so the golden trace (his match) is unchanged.

## What the tuning found

- **A hard serve isn't always better for the CPU.** It brings the return
  back sooner, before the server has recovered. In singles it helped
  (BRAD), but in doubles it often hurt: GOTH CLUB at power 8 went from 18
  to 30 bot wins of 60 against the net bot; SURF CREW at 16 from 11 to
  20, and at 10 to 30. So the soft-game and deep teams serve classic.
- **Duds weigh more in doubles,** where both players serve: the TWINS
  with one dud in 20 went from 5 to 17.
- **Two rejected dud rules:** every Nth shot of the CPU's shot counter
  (rallies included) made duds far rarer than the knob said (2 in 123 for
  12); every Nth serve of the match never fired, since a CPU serves about
  four times in a 5-point match and the AI resets each match.
- Small changes reshuffle the deterministic matches by up to ±10 wins, so
  each step was measured in all three modes.

## Balance

`make stats LATE=0 MATCHES=20 OPPONENT=-2 DIFFICULTY=2`, bot wins of 60,
v0.20.0 → now (unchanged rungs omitted); every table keeps
`ladder order: yes`:

| Mode | GYM BROS | CHAMPS | HENDERSON |
|---|---|---|---|
| Singles (`MODE=0`) | BRAD 26 → 19 | ACE 0 → 0 | |
| Doubles, regular bot (`MODE=1`) | 44 → 33 | 1 → 2 | 59 → 60 |
| Doubles, net bot (`MODE=3`) | 54 → 54 | 1 → 4 | |

The GYM BROS got harder; the CHAMPS give away a few points to duds.

## Tests

- `test_ai.c: test_cpu_serve_power`: the default style is classic with no
  duds; BRAD's serves over 20 minutes of singles are all at full power or
  duds (111 and 10 of 121).
- MesenCE: singles against BRAD, his serves read power 30, 30, then a dud.
- Golden trace identical; `make perf` zero overruns.
