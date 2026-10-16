# ADR-052 — The Kitchen Fault

Status: **Accepted 2026-10-16**
Milestone: `M4-RULES-01`. Amends: the gameplay spec's POC rule 9 (no
non-volley-zone faults). Depends on: ADR-033 (LAVA), ADR-035 (net play).

## Why

On playing, the user found that walking in the kitchen was allowed
(correct) but so was volleying from it (wrong). In pickleball the kitchen
is the non-volley zone: you may stand in it, and play a ball there after it
bounces, but a volley with your feet in it (or on its line) is a fault.

## What

- **The rule:** a contact on a ball that has not bounced since the last hit
  (a volley), with the hitter's ground point in their own kitchen or on its
  line, loses the point for their team. The banner is **KITCHEN!**, the
  serve-in-the-kitchen call: the same word, and the same rule family.
  - The zone runs between the sidelines; past them is not the kitchen.
  - Walking in the kitchen, and hitting a bounced ball there, stay legal.
  - The early-volley check (two-bounce rule) runs first. Both lose the
    point for the same team; VOLLEY! is the more specific call.
- **Where:** `game_update_live` reads `g_ball.bounce_count == 0` before
  the contact (which resets it) and then calls `rules_process_volley`. The
  zone test is `COURT_IN_KITCHEN_FP` in `court.h`, a macro so the contact
  frame makes no extra call.
- **LAVA is unchanged** (the user's choice). It keeps its own test, where
  the line is safe; with LAVA on, the kitchen fault can only happen on the
  line, since standing inside is already LAVA.
- **CPUs:** a CPU never plans a volley (`ai_plan_volley`) or swings at one
  (`ai_swing_connects`) with its feet in its kitchen; it plays that ball
  off the bounce. Their net spots (44 px and up, ADR-035) are already
  behind the line.
- **Practice** has no rules and calls nothing.

## Not done

- The momentum rule (a volley that carries you into the kitchen after the
  hit is a fault too) isn't called. It's in IDEAS.md.

## Measured

- `make stats` (singles, doubles, net bot; 20 matches, MEDIUM): every
  table identical to before, `ladder order: yes`. Neither the CPUs nor the
  stats bot volleyed from the kitchen before, so in play the fault is
  called on humans.
- `make perf`: every timeline 0 overruns. The golden trace is identical.
- MesenCE, debug ROM: P1 walks to x = 120 (in the kitchen) without a
  fault; a ball poked into the air beside them and swung at ends the point
  with KITCHEN! and a CPU POINT.

## Tests

- `test_rules.c: test_kitchen_volley_loses_point`: in the kitchen and on
  its line it's a fault, a pixel behind it isn't, past the top sideline
  isn't, and the bottom sideline is; mirrored for the right team; nothing
  after the point is over.
- `test_ai.c: test_net_play`: TWINS standing 20 px from the net don't
  volley the ball they volley from 44 px. From the kitchen line they never
  draw a kitchen fault (they may drift back behind the line and volley).
