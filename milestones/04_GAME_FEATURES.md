# M4 — Game Features

Target version: **0.4.0** (first GitHub release; see [RELEASING.md](../docs/RELEASING.md))

Outcome: the POC grows into a game you return to: saved records and
settings, themed opponents on a ladder, and several venues. Design notes for
each idea are in [IDEAS.md](../docs/IDEAS.md); decisions are ADR-021 to
ADR-024.

## Save and records

- [x] `M4-SAVE-01` Battery-backed SRAM with a checksummed, double-buffered save block.
  - Accept: a torn or blank save falls back safely; records persist across boots.
  - Verified: [ADR-021](../docs/decisions/ADR-021-battery-save.md), `test/test_save.c`, `records.timeline` with `KEEP_SAVE=1`.
- [x] `M4-SAVE-02` RECORDS page on the title, with an erase combo.
  - Verified: ADR-021; zero frame overruns on the records timeline.
- [ ] `M4-SAVE-03` Initials entry for new records, arcade style.
  - Accept: a new best streak or longest rally asks for three letters, which the records page shows; the save layout change bumps `SAVE_VERSION`.
- [ ] `M4-SAVE-04` Verify the save on a physical cart with SRAM and a battery.
  - Accept: records survive a power cycle on real hardware; logged in `test/hardware-log.md`.

## Court colours and venues

- [x] `M4-COURT-01` Court colour presets for surface, surround, and lines, with a live preview on the COURT page.
  - Verified: [ADR-022](../docs/decisions/ADR-022-court-colors.md), contrast checks in the asset build, `court.timeline`.
- [ ] `M4-COURT-02` Two-tone court: the kitchen gets its own colour part (see IDEAS.md, "Two-tone court").
  - Accept: the kitchen paints with its own palette-2 entries; a KITCHEN row (with SAME as an option) previews live; every kitchen preset passes the line contrast check; venues can set a default kitchen.
- [x] `M4-VENUE-01` Venues PARK, LAKESIDE, and SUNSET with shared ground tiles and home courts.
  - Verified: [ADR-024](../docs/decisions/ADR-024-venues.md), `venue.timeline`, `lake.timeline`.
- [ ] `M4-VENUE-02` Preview a venue on the COURT page before leaving it.
  - Accept: scrolling VENUE shows the backdrop (reload with a short fade, or a thumbnail) without dropped frames; Y still restores the original.
- [ ] `M4-VENUE-03` More venues from new mocks (indoor rec center, rooftop at night, beach boardwalk, championship stadium).
  - Accept: each passes the shared-ground and 896-tile checks; about 14 KiB of ROM each (about 6 more fit in 256 KiB).

## Opponents and story

- [x] `M4-OPP-01` Eight CPU personalities with looks, play styles, and a saved ladder.
  - Verified: [ADR-023](../docs/decisions/ADR-023-cpu-personalities.md), `test/test_opponents.c`, `make stats OPPONENT=n`.
- [ ] `M4-OPP-02` Tune the doubles partners.
  - Accept: a doubles balance run (`tools/match_stats.c` is singles only today) shows each team's difficulty in ladder order.
- [ ] `M4-OPP-03` Give BRAD a distinct difficulty.
  - Accept: his matches stop being a coin flip of balls hit out; his bot win rate sits between MARGE's and PETE's in the ADR-023 table.
- [ ] `M4-OPP-04` Opponent accessories (hats, bandanas, a pirate tricorn) using the accessory object (ADR-014).
  - Accept: they fit the OBJ tile budget (208 of 256 used) and the per-row sprite limits.
- [x] `M4-STORY-01` Story cards: an intro card per opponent (name, one line of trash talk, rung) and the opponent's line on the match result.
  - Verified: [ADR-026](../docs/decisions/ADR-026-story-cards.md), `test/test_story.c`, `story.timeline`. Uses the HUD over the live court; portraits are `M4-STORY-03`.
- [x] `M4-STORY-02` A ladder ending after beating CHAMPS (CHAMPION!, plus a "ladder clears" record).
  - Verified: ADR-026, `test_story.c`.
- [ ] `M4-STORY-03` Opponent portraits on the intro card (BG2, like Player Select's).
  - Accept: one portrait per team in palette roles; fits the BG2 portrait budget (ADR-020) and VRAM alongside the court.

## Options

- [x] `M4-OPT-01` OPTIONS page: points to win (3-21, default 5) and difficulty KID/EASY/MEDIUM/HARD.
  - Verified: [ADR-025](../docs/decisions/ADR-025-options.md), `test/test_options.c`, `options.timeline`.
- [x] `M4-OPT-02` KID auto-serve: serve automatically after a short wait so a kid never needs a button.
  - Verified: ADR-025 (amended), `test_options.c` (a P1 with no button presses serves, returns, and wins 8 of 8).

## Practice

- [ ] `M4-PRACTICE-01` Practice mode with a ball machine (see IDEAS.md, "Practice mode").
  - Accept: a PRACTICE title entry; the machine feeds the chosen shot to the chosen target at the chosen rate and speed; the HUD shows the return streak, and the best streak is saved; no points are scored; host tests cover feeds and streaks.

## Play modifiers

- [ ] `M4-MOD-01` A modifiers menu and the first modifiers (big ball, fast ball, first to 11, kitchen is lava).
  - Accept: each modifier has a rules or physics test; the choice lives in `MatchConfig` and is optionally saved.

## Polish

- [x] `M4-UI-02` Doubles frame headroom: the doubles timeline peaks at scanline 223-226 of 225 (one dropped frame on the options branch, two on the old `main`).
  - Verified: ADR-026; `hud_panel` now uses the assembly fill, and doubles has zero overruns.
  - Accept: zero overruns on the doubles timeline and a 3,000-frame doubles soak, with a few scanlines of margin.

- [ ] `M4-UI-01` Draw the title pages faster.
  - Accept: the records page fills in under about 0.25 s (today about 0.5 s, because rows wait for frames with time left while the demo runs) with zero overruns. One option is an assembly text writer like `hud_fill`.
