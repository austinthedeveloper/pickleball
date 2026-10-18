# ADR-073 — Four Players

Status: **Accepted 2026-10-18**
Task: `M4-MP-01`. Amends: ADR-020 (Player Select), ADR-034 (open play),
ADR-066 (the join counter: 3P and 4P no longer sit out).

## Why

The title has let 3P and 4P join since ADR-066, the rings show their
colours, and the manual describes four players on a Super Multitap, but
matches took 1P and 2P only. The input layer was built for four from the
start (docs/04_INPUT_MULTITAP_PLAN.md): intents per slot, ownership as
data, team separate from pad.

## Decision

- **Teams by controller.** From the title, in doubles: 1P and 3P on the
  left (slots 0 and 1), 2P and 4P on the right (slots 2 and 3), a CPU in
  any empty slot. Singles is 1P and 2P only (slots 0 and 2).
  `match_controller_slot(mode, controller)`.
- **Who plays a slot** is one function, `match_slot_controller(slot)`:
  the title's layout above, or in open play your side first (slot n is
  controller n: 2P partners you, 3P and 4P across the net). The match
  start passes it to `input_assign_match`, which reads each human slot's
  pad (a controller's pad position: 1P port 1, 2P port 2 or the
  Multitap's first pad, 3P and 4P the Multitap's next two; without a
  Multitap 3P and 4P have none and their slots are CPUs). The rings use
  it too, so input and colour can't disagree.
- **Against the CPU** means no human on the right team
  (`match_vs_cpu`): the ladder opponent, the records, career stats, and
  the park rating. 1P and 3P against the ladder is a ladder match; 4P on
  the right is a match between players.
- **Player Select** takes everyone who joined, in turn: 1P, 2P, 3P, 4P,
  each on their own controller, the tag naming whose turn it is
  (`1P`-`4P`). A colour taken by anyone before is a dark silhouette;
  each later player starts on the first free colour. B or Start on the
  paddle row passes to the next player, Y on the top row back to the one
  before. There are four colours for four players.
- **Open play.** 3P and 4P who joined on the title play the team you
  drew, in its looks (the call still says `VS SURF CREW`). A game with a
  human across the net isn't against the CPU, so it isn't rated or
  recorded. The scoreboard names that side by its human (`P3`, `P3
  POINT`, `P3 WINS`), as it says `P2` in a versus match.
- **Pause** stays 1P's (the plan's central arbitration).

## Testing controllers 2-4 headless

The runner presses port 1 only (KNOWN_ISSUES #16), and a poke of the
library's `pad_keys` is overwritten by its VBlank read. On the debug ROM,
`g_input_inject_on` makes pad positions 1-4 read `g_input_inject[]`,
which pokes can set (without a Multitap those positions hold power-on
RAM, so the hook replaces them rather than adding to them). Release
builds have no such path.

## Frame budget

A human slot runs no AI, so a four-human frame does less work than a
doubles frame with CPUs (the bodies, rings, and ball are the same).
Measured on the debug ROM over frames 380-2400 (the injection hook is
debug-only): four humans 0 overruns, the doubles ladder 3 (the debug
overlay overruns more, KNOWN_ISSUES #8). The humans stood still there,
so it is not a rally under load; `make perf` (release) is unchanged,
zero overruns.

## Not done

- **Career players in other slots.** The career is one active player,
  1P's (ADR-047); 2P-4P choose a look on Player Select as before.
- **Joining 3P and 4P in the park.** They join on the title and come
  along; Start on their controllers in the park does nothing yet.
- **The real-hardware checklist** with a Multitap (`M3-IN-03`, the plan's
  activation checklist).

## Verification

- `test_match.c: test_four_players`: the controller-to-slot layout in both
  modes; four humans' choices on their slots; with a Multitap each slot
  reads its controller's pad, without one 3P and 4P fall back to CPUs;
  1P and 3P is against the CPU, 1P and 4P is not; open play's layout.
  `test_open_play.c: test_two_players` with the park's layout.
  `test_select.c: test_four_players`: four turns in controller order, a
  colour taken before is skipped (and a later default moved off it), Y
  back to the player before, the config on the right slots; 1P and 3P
  get the ladder's next team; singles takes 1P and 2P only.
- Debug ROM, `test/smoke/four_players.timeline` with these pokes:

      --poke "@100 g_multitap_present=1" --poke "@100 g_input_inject_on=1"
      --poke "@200 g_input_inject+2:2=4096" --poke "@201 g_input_inject+2:2=0"
      --poke "@202 g_input_inject+4:2=4096" --poke "@203 g_input_inject+4:2=0"
      --poke "@204 g_input_inject+6:2=4096" --poke "@205 g_input_inject+6:2=0"
      --poke "@310 g_input_inject+2:2=4096" --poke "@311 g_input_inject+2:2=0"
      --poke "@330 g_input_inject+4:2=4096" --poke "@331 g_input_inject+4:2=0"
      --poke "@350 g_input_inject+6:2=4096" --poke "@351 g_input_inject+6:2=0"
      --poke "@420 g_input_inject+2:2=256" --poke "@440 g_input_inject+2:2=0"
      --poke "@460 g_input_inject+4:2=256" --poke "@480 g_input_inject+4:2=0"
      --poke "@500 g_input_inject+6:2=256" --poke "@520 g_input_inject+6:2=0"

  All four join (`g_title_joined` 15); Player Select takes four turns
  (the 4P screen shows BLUE, RED, and GREEN taken); in the match every
  slot is human, reading pads 0, 2, 1, 3; 2P's Right moves slot 2, 3P's
  slot 1, 4P's slot 3. The rings: 1P red and 3P yellow on the left, 2P
  blue and 4P green on the right.
- Park (`open_play.timeline`, the same joins): the game at court 2 has
  every slot human on pads 0-3, 3P and 4P as the Goth Club.
- `make check`, `make perf` zero overruns, golden trace and lockstep
  identical.
