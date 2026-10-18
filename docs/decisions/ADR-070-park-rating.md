# ADR-070 — The Park Rating

Status: **Accepted 2026-10-18**
Task: `M4-OPEN-06`. Amends: ADR-034 (open play), ADR-047 (career
players).

## Why

Open play had no stakes. Any court could be picked, a game was won or
lost, and nothing followed but the career's open play count. The courts
are already labelled as skill levels (<3.5, 3.5-4.5, 4.5+, ADR-034), the
way real parks split open play, so a rating that moves with your games
and opens the higher courts gives the park its own progression, separate
from the ladder, from what is already there.

## Decision

- **A career player has a park rating**, in tenths: 3.0 to start, 2.0 at
  the lowest, 6.0 at the highest. It shows in the park's status line
  (`OPEN PLAY 3.4  START EXIT`) and on the career card
  (`PARK RATING 3.4`).
- **Courts open with it.** The 3.5-4.5 court opens at 3.5 and the 4.5+
  court at 4.5. A court opened stays open (the card keeps the highest
  court reached), so a loss just past a line does not lock you out.
  Stacking at a closed court is refused: `OPENS AT 3.5  YOU'RE 3.2`.
- **A game moves it** (`open_rating_after`). Your level is the court
  whose range holds your rating:
  - at your level: a win +0.2, a loss -0.1;
  - above it (a court reached earlier, after the rating dropped): a win
    +0.3, a loss -0.1;
  - below it: a win nothing, a loss -0.1.
  A win counts for more than a loss so that winning a third of your games
  at your level holds your place and doing better climbs: the 3.5-4.5
  court is MEDIUM, and a player who stalls there at 50% (an even Elo) would
  never see the 4.5+ court. A win below your level counts for nothing, so
  the easy court can't be farmed.
- **The result says so**: under the opponent's line,
  `RATED 3.4 TO 3.6`, or with a court opened, `3.5-4.5 OPEN! RATED 3.6`.
  Back in the park: `YOU WON 11-4  RATED 3.6`.
- **Unrated: every court open**, as before. That is no career player
  (NO PLAYER records nothing, ADR-047), and KID, whose courts are all KID
  (ADR-034).
- **2P in the park** partners you; the rating is 1P's career player's.

## Save

`CareerPlayer.reserved`, always written as 0 (a slot is cleared when it
is made or deleted, and layouts before 12 had no career), is now `park`:
bits 0-5 the rating less 1.9 (2.0 is stored as 1), bits 6-7 the highest
court reached. 0 reads as 3.0 with the first court, which is what a
player saved before this has, so the layout and its size are unchanged
(52 bytes a player, `SaveData` 214) and `SAVE_VERSION` stays 14: nothing
to migrate (ADR-032 asks for a frozen layout and a migration when the
layout changes). An older ROM ignores the byte.

## Code

- `open_play.c` (pure): the rules (`open_rating_court`,
  `open_rating_after`, `open_court_floor`) and the gate
  (`open_play_rate`, `open_court_open`, `OPEN_EVENT_LOCKED`).
- `career.c`: `career_park_rating`, `career_park_court`, and
  `career_rate_park`, which `career_record_match` calls for a park game
  that is not on KID. `MatchConfig.open_play` is now the court + 1 (it was
  1), so the match end knows the court.
- `main.c` sets the park's rating from the career player when the park
  opens and after each game (`park_rate`).
- Not simulation: `open_play.h` is not in the tuning export (ADR-068),
  and the golden trace and lockstep are identical.

## Not done

- No rating without a career player. A cart-wide rating for NO PLAYER
  would need a save layout change; making a player is one screen.
- The court labels over the courts don't show which are closed; the
  message at the rack does.
- The CPU teams' courts don't move with your rating.

## Verification

- `test_open_play.c: test_rating` (the rules, the floor and cap, the
  gate, a reached court staying open), `test_career.c: test_park_rating`
  (the packing, opening a court, KID and the ladder unrated, a save round
  trip), `test_text.c` (`text_tenths`).
- MesenCE, `test/smoke/park_rating.timeline` with the pokes below on the
  release ROM: `OPEN PLAY 3.4`, `OPENS AT 3.5  YOU'RE 3.4` at court 2's
  rack, a game at court 1 won 11-1, `3.5-4.5 OPEN! RATED 3.6` on the
  result, `YOU WON 11-1  RATED 3.6` in the park; `park` went from 15 to
  0x51. The career card reads `PARK RATING 3.6`.

      python3 tools/mesen_probe.py probe release \
        test/smoke/park_rating.timeline 4500 park=g_save+103:1 \
        --poke "@60 g_save+80=1" --poke "@60 g_save+81=65" \
        --poke "@60 g_save+82=67" --poke "@60 g_save+83=69" \
        --poke "@60 g_save+84=32" ... --poke "@60 g_save+88=32" \
        --poke "@60 g_save+103=15" --poke "@60 g_save+208=0" \
        --poke "@300- once if g_match_config+27==1 g_game_state+6=10" \
        --shots build/smoke/park_rating

- `make perf` zero overruns; golden trace and lockstep identical.
