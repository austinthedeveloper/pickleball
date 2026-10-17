# ADR-063 — More Quips, Shuffled

Status: **Accepted 2026-10-17**
Milestone: `M4-FUN-02`. Amends: ADR-048 (quips).

## Why

Four lines a side repeat within a match: a team that loses ten points
says about three quips, and over a ladder run every line comes back again
and again. The pick (two bits of the frame count) also clustered.

## Decision

- **Sixteen lines a side** (`QUIPS = 16` in `tools/assetgen/opponents.py`,
  `OPPONENT_QUIPS` in the generated header): twelve new ones for each
  ladder team, the human side, and the default CPU, in each team's voice.
  Same rules as ADR-048: PG, no put-downs, up to 28 HUD font characters.
  Sixteen is a power of two, so a pick stays a mask, no divide.
- **A shuffled deck per side** (`story_quip`). A pass starts at a line and
  steps by an odd stride, both taken from the frame count when the pass
  begins; an odd stride visits all sixteen before any again, and the next
  pass gets a new start and stride, so a new order. A pass that would
  start on the line just said starts one step on (that line comes last).
  The game has no RNG: the frame count stands in, as for the CPU's duds
  (ADR-058). Steps are additions; no multiply for 816-tcc.
- How often is unchanged: about one lost point in three.

The deck state (`s_quip_next`, `s_quip_stride`, `s_quip_left`, per side)
is cleared in `story_initialize` with the rest of the story state.

## Cost

About 2.7 KiB more ROM: 2.4 KiB of text and the pointers. Display only:
the simulation is unchanged. The golden trace differs only on the title,
frames 55-239: the title starts one frame later (`phase_timer` and a
player counter one behind), from the larger ROM's load; from Player
Select on it is identical.

## Tests

- `test_story.c: test_quips`: a third of the rolls, the right side's
  lines, never twice in a row, every pass of sixteen says each line once,
  and the second pass is in a different order.
- The asset build checks sixteen lines a side and their characters.
- MesenCE: the golden run's point results show old and new lines
  ("THAT ONE'S ON DAD.", "THAT WAS A PRACTICE SWING."); `make perf` zero
  overruns.
