# ADR-065 — Menu Cleanup

Status: **Accepted 2026-10-16**
Milestone: `M4-UI-05`. Amends: ADR-020 (Player Select), ADR-025 (options),
ADR-033 (modifiers), ADR-040 (initials), ADR-047 (career), ADR-049 (ball
speed), ADR-050 (camera).

## Why

Features arrived one at a time, and the title collected them in the order
they were built. A review of every menu screen found:

- The title menu mixed ways to play, settings, and CAREER, with PRACTICE
  last, below the settings.
- The title showed three competing things: the demo's scoreboard
  (`P1 0 · ARCADE RULES · CPU 0`), the menu, and `PRESS START`, while
  every other page shows a button hint.
- RECORDS was hidden as a row of the CAREER page, under NO PLAYER.
- The career card said `UNLOCKS 0/16` (hats and hair) and the HAT MUSEUM
  `1 OF 12` (hats held, the cap included): two numbers that read as one.
- Player Select still offered a career player's body and shirt, which the
  career editor owns.
- Two settings did one job: OPTIONS' BALL SPEED and the FAST BALL
  modifier. ADR-049 left BALL SPEED's default "until the speed is chosen
  by play", which never happened.
- WIN BY 2, a scoring rule, sat with the modifiers, away from POINTS.
- CAMERA was one row of eleven values (OFF, 50% to 100%).
- A career player has a name, yet a record still asked for initials.

## Decision

- **Title menu**, top to bottom: SINGLES, DOUBLES, OPEN PLAY, PRACTICE;
  CAREER, RECORDS; COURT, OPTIONS, MODIFIERS. Nine entries, rows 12-20;
  the 2P line moves to row 21. The HUD's title view keeps the choice in
  bits 0-3 (it was 0-2) and 2P in bit 4.
- **The title's demo has no scoreboard** (ADR-066 puts the join counter
  there): while `g_attract` the score
  panels' rows stay clear and score changes draw nothing. A match redraws
  them on its full redraw, as before. The menu's prompt is `@ SELECT`.
- **RECORDS** is a title entry. It opens the CAREER page on its RECORDS
  view (`career_page_begin_records`); B, Y, or Start goes back to the
  title. The CAREER page's slot list loses its RECORDS row.
- **Unlock counts**: the card shows `HATS 1/12` and `HAIR 5/10`, what the
  player may wear out of all there are (`career_hats_held`,
  `career_hair_held`); the museum says `ACE  1 OF 12 HATS`.
- **Player Select**: with a career player active, 1P starts on the paddle
  row and cannot go above it (Up stays, Y backs out). 2P still chooses
  everything.
- **BALL SPEED is cut.** Matches play at 100% (`OPTIONS_BALL_SPEED`); the
  saved byte is unused, and a save with another speed plays at 100%.
  `make stats SPEED=` still tries other speeds for tuning.
- **SCORING moves to OPTIONS**, under POINTS: FIRST TO or WIN BY 2. It is
  still `MODIFIER_WIN_BY_2` in `settings.modifiers`, so saves keep it and
  the rules are unchanged; the modifiers page maps rows to bits
  (`modifiers_row_bit`) instead of row n being bit n.
- **CAMERA** is OFF or ON, and **AMOUNT** (50% to 100%, 85% first) is the
  next row; the user tunes with it. Both live in `g_save.camera`: 2-12 is
  ON at an amount, as every save since ADR-050 has it; 0 and 1 are OFF;
  `0x40 | amount` is OFF keeping an amount. No layout change.
- **Records credit a career player by name**: with one active, a beaten
  record takes the first three letters of their name, with no card.
  Without one, the card asks at a game over, as ADR-040 (amended) says.

The options page is POINTS, SCORING, DIFFICULTY, SOUND, CAMERA, AMOUNT;
the modifiers page BALL, SPEED, KITCHEN, VISION, RALLY, FOOTING, LIGHTS,
COLORS.

## Not done

- COURT, OPTIONS, and MODIFIERS stay three pages. A single settings page
  with tabs would shorten the menu by two rows but hide more.
- The records table keeps the cartridge's best across players; the card
  keeps each player's own. They answer different questions.

## Verification

- `make check`: `test_options.c` (the camera's values, old saves read as
  before, SCORING keeps the other modifiers, AMOUNT wraps),
  `test_modifiers.c` (row bits; no row reads WIN BY 2),
  `test_career_page.c` (no RECORDS row; RECORDS opens and closes to the
  title; the hat and hair counts), `test_select.c` (a career player picks
  only a paddle; 2P picks everything), `test_story.c` (a career player's
  record takes their name without a card).
- `make perf`: zero overruns on every timeline; the menu timelines walk
  the new order.
- `make golden-compare`: identical.
- Screenshots (debug ROM): the title, RECORDS, OPTIONS, MODIFIERS, COURT,
  PRACTICE, the career card, the museum, and Player Select with a career
  player.
