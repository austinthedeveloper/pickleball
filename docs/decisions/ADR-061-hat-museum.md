# ADR-061 — Hat Museum

Status: **Accepted 2026-10-16**
Milestone: `M4-CAREER-05`. Extends: ADR-047 (career), ADR-059/060 (hats).

## Why

Twelve hats to earn, and the only place to see them was the career
editor's HAT row, one name at a time, skipping the locked ones. Players
should see what there is to earn and how.

## Decision

A **HAT MUSEUM** per career player, opened with **X** on the player's card
(CAREER page; the prompt reads `@ EDIT  X HATS  Y BACK`).

- **Its own scene,** like Player Select: the career page is a BG3 panel
  over the court, and BG3's priority tiles cover every object, so hats
  can't be drawn inside it. The museum turns BG1 and BG2 off. The
  backdrop (CGRAM 0) is a dark wall; each case is a block of the font's
  space tile *without* the priority bit, which sits behind every object,
  in BG3 palette 3 with its fill lit to a gallery grey. The museum writes
  no text in palette 3, and closing puts the fill and the backdrop back.
- **Twelve cases,** 4 × 3, every hat but the bare head, in hat id order
  (the cap first). The player stands in each in their saved look, wearing
  that case's hat. A locked hat is a black silhouette (its own OBJ
  palette, outlined in grey so it reads over dark hair). The picked case
  idles; the others stand still.
- **Under the cases:** the picked hat's name, and WEARING if it's worn; a
  locked one reads `???` and the unlock's hint (`BEAT THE GYM BROS`).
  Over the cases: the player's name and `n OF 12`.
- **Input** (`src/museum.c`, pure): the D-pad over the grid, wrapping; B
  puts an unlocked hat on; Y or Start goes back to the card over a fresh
  title demo. The save is written on leaving, and only if a hat was put
  on: a safe point.
- **Objects:** cursor corners 0-3, hats 4-27, bodies 28-39, ponytails
  40-51. A row of cases is 4 bodies, 4 hats, 4 ponytails, and the cursor:
  30 of 34 slivers.

## Not done

- Hair colours, the other unlockables, aren't shown. A second room (L/R
  to switch) would fit them: they are palettes, so a case would show the
  player with that hair.
- The cases don't show the hats' gear (ADR-047 stage 4); the editor does.

## Verification

- `make check`: `test_museum.c` (the cases, moves, wearing, the save on
  leaving), `test_career_page.c` (X opens it from the card).
- `make perf` with `museum.timeline` (opens, moves, back): zero overruns,
  worst frame 198. `make golden-compare`: identical.
- Screenshots (debug ROM): a new player (the cap only), and eight hats
  unlocked by poking the unlock mask, brown and black hair.
