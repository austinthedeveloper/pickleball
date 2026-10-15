# ADR-044 — Charged Serve

Status: **Accepted 2026-10-15**
Milestone: `M4-SERVE-01`. From [IDEAS.md](../IDEAS.md), "Charged serve".
Depends on: ADR-021/025 (KID), ADR-029 (HUD prompts).

## Why

The serve was one press: the same 40-frame serve every time. A meter makes
it a small skill test with a risk: more power, or a dud into the net.

## What

- **Who:** a human server, not on KID, outside the attract demo
  (`serve_charged`). CPUs and KID serve as before, at the classic power.
- **Hold:** from the serve's ready frame, holding B, A, or Y runs a meter
  from level 0 up to 30 and back down, one level a frame (one second up and
  down). A button held before the ready frame starts it at the ready frame;
  a press released before it does nothing (the old early-press buffer is
  for CPUs and KID only).
- **Release** tosses and serves at the level shown:
  - **0-3: a dud.** The ball goes low and slow to just past the net
    (`SERVE_DUD_PAST_NET`, 30 frames) and meets the net: a NET fault
    through the existing `BALL_EVENT_NET_FAULT` path. A tap is a dud.
  - **4-30:** the flight shortens a frame for every two levels, from 40
    frames (the classic serve, level 4) to 27 at the top. Every level, aim,
    box, and side lands in (`test_charged_serves_land`): the fastest serve
    still clears the net.
- **Aim** is as before: the D-pad at contact.
- **HUD:** the prompt reads `HOLD @ TO SERVE`; once the meter runs, a
  15-cell bar replaces the text, one cell for two levels. The first cell is
  white (lit there or nothing lit: a dud); the rest fill yellow. Only the
  changed cells redraw, on idle HUD frames (one tile most frames); VRAM
  follows the level one frame behind.

## State

`ServeCharge g_serve_charge` (charging, level, rising, power), apart from
`ServeState`, which the golden trace captures at a fixed size.
`PlayerIntent` gains `hit_held` at the end (the assembly reads only the
fields before it).

## Bots and tests

A host test or `make stats` bot plays P1's human-owned slot through the
AI. When it serves with the meter, it holds from the ready frame to the
classic level and lets go, so its serve is the CPU's and its timing is the
old early press's (close, not frame-exact). Singles ladder bot wins are
unchanged. Doubles moved, from that serve timing alone (CPUs serve as
before), and every table keeps its ladder order. Bot wins of 60:

| Team | Regular: before | Now | Net: before | Now |
|---|---:|---:|---:|---:|
| HENDERSON | 59 | 60 | 60 | 60 |
| SNOWBIRDS | 57 | 56 | 60 | 60 |
| GYM BROS | 44 | 40 | 54 | 56 |
| PIRATES | 22 | 26 | 42 | 39 |
| GOTH CLUB | 18 | 12 | 24 | 33 |
| SURF CREW | 5 | 8 | 11 | 17 |
| TWINS | 2 | 5 | 5 | 14 |
| CHAMPS | 0 | 3 | 2 | 5 |

The smoke timelines that serve for P1 (`doubles_ladder`, `modifiers`,
`modifiers_doubles`) hold B for 15 frames. `golden.timeline` taps B, so P1's
serves there are now duds: the trace differs from frame 339 and is
re-recorded.

## Not yet

- CPU serve power from personality (a risky CHAMPS, a safe SNOWBIRDS).
  CPUs serve at the classic power, so a strong human serve is an edge.
- A sound for the meter's top and bottom.

## Tests

- `test_serve.c: test_charged_meter`: waits for the ready frame, rises and
  falls a level a frame, the flight table, no meter for CPU, KID, or the
  demo.
- `test_serve.c: test_charged_serves_land`: both teams, both boxes, every
  level and aim: duds are NET faults, the rest land in; a release on the
  way down serves at the level shown.
- MesenCE: a tap goes in the net (NET); a full charge (level 30) lands in;
  the bar's cells in VRAM follow the level.
- `make perf`: zero overruns.
