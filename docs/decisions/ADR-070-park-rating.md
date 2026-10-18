# ADR-070 — Park Points

Status: **Accepted 2026-10-18**
Task: `M4-OPEN-06`. Amends: ADR-034 (open play), ADR-047 (career
players).

## Why

Open play had no stakes. Any court could be picked, a game was won or
lost, and nothing followed but the career's open play count. The courts
were already skill levels (ADR-034), the way real parks split open play,
so a score that moves with your games and opens the higher courts gives
the park its own progression, separate from the ladder, from what is
already there.

The courts were labelled `<3.5`, `3.5-4.5`, and `4.5+`: DUPR-style
ratings, which didn't exist in 1994. They are now **ROOKIE**, **CLUB**,
and **CHAMP**, and the rating is shown as whole **park points**.

## Decision

- **A career player has park points**: 10 to start, 0 at the lowest, 40
  at the highest. They show in the park's status line
  (`OPEN PLAY 14 PTS  START EXIT`) and on the career card
  (`PARK POINTS 14`).
- **Courts open with them.** CLUB opens at 15 and CHAMP at 25. A court
  opened stays open (the card keeps the highest court reached), so a loss
  just past a line does not lock you out. Stacking at a closed court is
  refused: `OPENS AT 15 PTS  YOU HAVE 12`.
- **A game moves them** (`open_rating_after`). Your level is the court
  whose range holds your points:
  - at your level: a win +2, a loss -1;
  - above it (a court reached earlier, after the points dropped): a win
    +3, a loss -1;
  - below it: a win nothing, a loss -1.
  A win counts for more than a loss so that winning a third of your games
  at your level holds your place and doing better climbs: CLUB is MEDIUM,
  and a player who stalls there at 50% (an even Elo) would never see
  CHAMP. A win below your level counts for nothing, so ROOKIE can't be
  farmed.
- **The result says so**: under the opponent's line,
  `PARK POINTS 14 TO 16`, or with a court opened,
  `CLUB COURT OPEN! 16 PTS`. Back in the park: `YOU WON 11-4  16 PTS`.
- **Unrated: every court open**, as before. That is no career player
  (NO PLAYER records nothing, ADR-047), and KID, whose courts are all KID
  (ADR-034).
- **2P in the park** partners you; the points are 1P's career player's.

The code keeps the value in tenths, as first planned (a DUPR-like 2.0 to
6.0, start 3.0, courts at 3.5 and 4.5, a win +0.2); the points are that
less 2.0, times ten (`open_rating_points`). Only the display changed.

## Save

`CareerPlayer.reserved`, always written as 0 (a slot is cleared when it
is made or deleted, and layouts before 12 had no career), is now `park`:
bits 0-5 the rating less 1.9 tenths (0 points is stored as 1), bits 6-7
the highest court reached. 0 reads as 10 points with ROOKIE, which is
what a player saved before this has, so the layout and its size are
unchanged (52 bytes a player, `SaveData` 214) and `SAVE_VERSION` stays
14: nothing to migrate (ADR-032 asks for a frozen layout and a migration
when the layout changes). An older ROM ignores the byte.

## Code

- `open_play.c` (pure): the rules (`open_rating_court`,
  `open_rating_after`, `open_court_floor`, `open_rating_points`) and the
  gate (`open_play_rate`, `open_court_open`, `OPEN_EVENT_LOCKED`).
- `career.c`: `career_park_rating`, `career_park_court`, and
  `career_rate_park`, which `career_record_match` calls for a park game
  that is not on KID. `MatchConfig.open_play` is now the court + 1 (it was
  1), so the match end knows the court.
- The rating is applied on the frame after the match ends
  (`career_park_flush`, before the deferred save write): on the
  match-end frame it tipped the frame budget (KNOWN_ISSUES #41).
- `main.c` sets the park's points from the career player when the park
  opens and after each game (`park_rate`).
- The court labels are sprites in the park's white font
  (`tools/assetgen/obj.py`), now the letters of ROOKIE, CLUB, and CHAMP
  in place of `<.+-345`: 15 label sprites, as before.
- Not simulation: `open_play.h` is not in the tuning export (ADR-068),
  and the golden trace and lockstep are identical.

## Not done

- No points without a career player. A cart-wide score for NO PLAYER
  would need a save layout change; making a player is one screen.
- The court labels don't show which courts are closed; the message at
  the rack does.
- The CPU teams' courts don't move with your points.

## Verification

- `test_open_play.c: test_rating` (the rules, the floor and cap, the
  points shown, the gate, a reached court staying open),
  `test_career.c: test_park_rating` (the packing, opening a court, KID and
  the ladder unrated, a save round trip).
- MesenCE, `test/smoke/park_rating.timeline` with the pokes below on the
  release ROM: `OPEN PLAY 14 PTS`, the labels ROOKIE, CLUB, and CHAMP,
  `OPENS AT 15 PTS  YOU HAVE 14` at CLUB's rack, a game at ROOKIE won
  11-1, `CLUB COURT OPEN! 16 PTS` on the result, `YOU WON 11-1  16 PTS`
  in the park; `park` went from 15 to 0x51. The career card reads
  `PARK POINTS 16`.

      python3 tools/mesen_probe.py probe release \
        test/smoke/park_rating.timeline 4500 park=g_save+103:1 \
        --poke "@60 g_save+80=1" --poke "@60 g_save+81=65" \
        --poke "@60 g_save+82=67" --poke "@60 g_save+83=69" \
        --poke "@60 g_save+84=32" ... --poke "@60 g_save+88=32" \
        --poke "@60 g_save+103=15" --poke "@60 g_save+208=0" \
        --poke "@300- once if g_match_config+27==1 g_game_state+6=10" \
        --shots build/smoke/park_rating

- `make perf` zero overruns; golden trace and lockstep identical.
