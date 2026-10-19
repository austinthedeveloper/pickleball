# ADR-075 — The Tutorial: How to Hit, on the Ball Machine

Status: **Accepted 2026-10-18**
Task: `M4-PRACTICE-05`. Extends: ADR-028 (practice, the ball machine).
Amends: ADR-065 (the title menu: ten entries).

## Why

The game has six ways to hit the ball (B, A, Y, Y aimed away from the
net, X, and L or R to hit short) plus the aim, and only the manual says
what they do. A new player gets to the title, picks SINGLES, and learns
by losing. PRACTICE drills a shot, but you have to know what to drill.
A short, guided lesson in the game itself teaches each shot by having you
hit it.

## Decision

**TUTORIAL** is a title entry, fifth, after PRACTICE, with the ways to
play. It opens Player Select (1P), then a practice session on the ball
machine that runs a lesson instead of a drill.

- **Six steps.** Each asks for one shot, and **3 returns** of that shot
  that land in move on (`LESSON_GOAL`). The step's title is in the centre
  panel, its instruction on the banner, and the count is P1's panel
  against **GOAL 3**:

| Step | Banner | Counts when |
|---|---|---|
| 1 SWING | PRESS (B) AS THE BALL ARRIVES | any swing, in |
| 2 AIM | HOLD UP OR DOWN TO AIM | Up or Down held at contact, landing in a sideline zone (LEFT or RIGHT LINE) |
| 3 DRIVE | A  DRIVE: FAST AND FLAT | A, in |
| 4 LOB | Y  LOB: HIGH AND DEEP | Y aimed level or toward the net, in the DEEP zone |
| 5 DINK | HOLD LEFT + Y  DINK IT SHORT | Y aimed away from the net (or held short with L or R), in the KITCHEN zone |
| 6 SLICE | X  SLICE: SLOW, BUYS TIME | X, in |

- **Then WELL DONE!** in big letters, `TUTORIAL` in the centre panel,
  and `PRESS (B) TO FINISH`: the machine stops, and any swing button goes
  back to the title.
- **Forgiving.** A miss, or a return that isn't the step's shot, counts
  for nothing but takes nothing away. AIM wants the pad held, not a
  stretch's spray toward a sideline (a bot mashing B passed it before
  that check).
- **The feeds** are easy and the same for every step: a standard shot
  straight at P1 (AT YOU), at the SLOW rate, at EASY's slow ball whatever
  the OPTIONS difficulty (`match_config_lesson`). KID's swing assist would
  hit standard shots for you, so it is off.
- **The zones.** LOB and DINK show their zone's corners (ADR-028), so you
  see where the shot should land. AIM takes either sideline, so it shows
  none.
- **Nothing is saved.** The tutorial keeps no best streak
  (`practice_finish` returns at once) and earns no unlocks. RESTART on the
  pause menu starts it again from step 1; MAIN MENU leaves.
- **The crowd** cheers each step done, and louder at the end.

## Code

- `MatchConfig.practice` is `MATCH_PRACTICE_DRILL` (1, as before) or
  `MATCH_PRACTICE_LESSON` (2). Everything that tests it for nonzero
  treats the tutorial as practice.
- `src/practice.c`: `PracticeState` gains `lesson`, `step`, and `shot_ok`.
  The shot is judged at contact (`practice_lesson_shot`: the hitter's
  `shot_type` and `court_aim_depth` of the contact's aim, as
  `collision.c` reads a dink), and the landing at the first bounce, as a
  drill's. `streak` is the step's count and `best` the goal, so the score
  panels show them.
- `src/hud_scene.c`: the practice key carries the step
  (`HUD_KEY_PRACTICE | 8 + step`), so each step is a staged redraw: the
  centre panel on the BIG step (with WELL DONE! at the end) and the banner
  on the PROMPT step. The right panel reads GOAL.
- **The title menu** (amends ADR-065): TUTORIAL is entry 4, CAREER to
  MODIFIERS move down one (entries 5-9), the menu takes rows 12-21, and
  the join line moves to row 22, the last row of the title area.
- No simulation change: the golden trace and lockstep are identical, and
  no tuning value changed (the export gains the new constants; version 4).

## Tests

- `test_practice.c: test_lesson`: a scripted P1 (the CPU's movement, the
  KID assist's timing, each step's button and aim) clears all six steps in
  order, about 90 seconds of play; each step starts at 0 with its zone; a
  return that lands in without the step's shot doesn't count; after the
  end no ball is fed, nothing is saved, and B goes to the title. Every
  title fits its panel and every line the banner.
- `test/smoke/tutorial.timeline`: TUTORIAL on the title, Player Select,
  step 1 with the count reaching 2 by pressing B, and MAIN MENU back to
  the title. In `make perf`: zero overruns. MesenCE frames of the menu,
  steps 1 and 5 (the kitchen's corners), and WELL DONE! (the last two by
  `--poke`).
- `career`, `museum`, and `records` timelines press Down once more, past
  TUTORIAL.
