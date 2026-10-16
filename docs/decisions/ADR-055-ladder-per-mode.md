# ADR-055 — A Ladder per Mode, and Quitting Starts It Over

Status: **Accepted 2026-10-16**
Milestone: `M4-OPP-05`. Amends: ADR-023 (the ladder). Depends on: ADR-030
(pause menu), ADR-032 (save migration).

## Why

The user quit a doubles ladder match (pause, MAIN MENU), started doubles
again, and was back on the second round. They expected quitting to give
up their place. Before this, the rung moved only on a win; a loss and a
quit both kept it, across power-off too. Singles and doubles also shared
one rung, so a singles win moved doubles along.

## Decision (the user's picks)

- **Quitting starts over.** Pause, MAIN MENU on a ladder match sets that
  mode's ladder back to the first opponent and writes the save (leaving a
  menu is a safe point). `game_quit_match` calls `save_quit_ladder`; it
  writes only when the rung moved.
- **Everything else keeps the rung:** a loss, RESTART, Start on the match
  result (after a win the next rung is already saved), power-off, and the
  matches that never move the ladder (2P versus, the default CPU, open
  play, practice, the attract demo).
- **Singles and doubles each have their own rung.** `save_ladder(mode)`
  reads it (out of range reads as the first), `save_advance_ladder(mode)`
  moves it. A clear in either mode counts as a ladder clear.

## Save (layout 14)

`SaveData.reserved`, the byte before the checksum, is now
`ladder_doubles`; `settings.ladder` is the singles rung. The size is
unchanged (214 bytes). Layout 13 is frozen as `SaveV13`; its migration
copies everything before the checksum. Every migration then sets the
doubles rung to the old shared one, so nobody loses their place in either
mode.

## HUD

- The records page's LADDER row shows both rungs in the W and L columns
  of the lines above: `LADDER  S 5/8  D 3/8`. The next team's name no
  longer fits; the intro card and the match result still name it.
- The result's NEXT UP line names the next team in the match's mode.

## KNOWN_ISSUES #31

Found with this: after pause, MAIN MENU on a doubles match, the title kept
the match's `VS <team>` sub-panel (rows 4-6). The title's first redraw
cleared from row 6 down, and the sub-panel refresh runs only in matches.
The title's clear now starts at the sub-panel's row and forgets it. The
`DOUBLES` scoreboard in that report is the doubles demo's own, as usual.

## Tests

- `test_save.c: test_migrate_v13`: a v13 save keeps everything, and
  doubles starts on the shared rung; the doubles rung saves on its own.
- `test_opponents.c: test_select_uses_ladder`: Player Select takes each
  mode's rung. `test_ladder_per_mode_and_quit`: doubles climbs alone;
  quitting resets only that mode and is saved; 2P, open play, and the
  demo never reset it.
- `test_story.c: test_ladder_clear_record`: a doubles clear counts.
- `test/smoke/quit_ladder.timeline`: a doubles ladder match, pause, MAIN
  MENU; the title shows no VS panel. Probed with the doubles rung poked to
  3: 0 after the quit, singles unchanged.
