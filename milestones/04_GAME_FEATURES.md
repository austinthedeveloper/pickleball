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
- [x] `M4-COURT-02` Two-tone court: the kitchen gets its own colour part (see IDEAS.md, "Two-tone court").
  - Verified: [ADR-027](../docs/decisions/ADR-027-two-tone-kitchen.md), `test_court_style.c`, `court.timeline`.
  - Accept: the kitchen paints with its own palette-2 entries; a KITCHEN row (with SAME as an option) previews live; every kitchen preset passes the line contrast check; venues can set a default kitchen.
- [x] `M4-VENUE-01` Venues PARK, LAKESIDE, and SUNSET with shared ground tiles and home courts.
  - Verified: [ADR-024](../docs/decisions/ADR-024-venues.md), `venue.timeline`, `lake.timeline`.
- [ ] `M4-VENUE-02` Preview a venue on the COURT page before leaving it.
  - Accept: scrolling VENUE shows the backdrop (reload with a short fade, or a thumbnail) without dropped frames; Y still restores the original.
- [ ] `M4-VENUE-03` More venues from new mocks (indoor rec center, rooftop at night, beach boardwalk, championship stadium).
  - Accept: each passes the shared-ground and 896-tile checks; about 14 KiB of ROM each (the 512 KiB cart has about 300 KiB free).

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

## Pause

- [x] `M4-PAUSE-01` Pause menu: RESUME, RESTART, MAIN MENU in matches and practice.
  - Verified: [ADR-030](../docs/decisions/ADR-030-pause-menu.md), `test/test_pause_menu.c`, `pause.timeline`.

## Open play

- [x] `M4-OPEN-01` Open play (from GOOFY_IDEAS.md): a park with three courts, paddle stacks, and your turn as a doubles match.
  - Accept: walk the park, stack and take back your paddle, watch the games rotate, get called and play, come back; host tests for the rules (nobody lost or doubled in a long soak); the park holds the frame budget.
  - Verified: [ADR-034](../docs/decisions/ADR-034-open-play.md), `test/test_open_play.c`, `open_play.timeline` (zero overruns in `make perf`).
- [x] `M4-OPEN-02` A livelier park: people walk off to the gate after a game and the next four walk on before the first serve; a crowd watches from the benches.
  - Verified: ADR-034 (amended), `test_open_play.c`, MesenCE frames of a changeover; `open_play.timeline` still has zero overruns.
- [ ] `M4-OPEN-05` A bigger, scrolling park (more courts or a clubhouse). All eight OBJ palettes are in use (you, six teams on court, objects) and the scores are BG3 text, so it needs sprite scores or BG-baked labels, and palettes shared or swapped by what is on screen.
- [x] `M4-OPEN-03` Partners from the stack: play with the person stacked with you (their looks and style), not a fixed regular.
  - Verified: ADR-034 (amended), `test_opponents.c: test_partner`, `test_open_play.c`; `open_play.timeline` shows "WITH PETE VS SURF CREW" and PETE's look in the match.
- [x] `M4-OPEN-04` A second player in the park: 2P joins and plays as your partner.
  - Verified: ADR-034 (amended), `test_open_play.c: test_two_players` (co-op pad mapping, versus unchanged). The headless runner cannot press port 2 (KNOWN_ISSUES #16): check by hand with a second pad.

## Practice

- [x] `M4-PRACTICE-01` Practice mode with a ball machine (see IDEAS.md, "Practice mode").
  - Verified: [ADR-028](../docs/decisions/ADR-028-practice.md), `test/test_practice.c`, `practice.timeline`.
- [x] `M4-PRACTICE-02` A ball machine sprite on the far baseline (a few OBJ tiles; 208 of 256 used).
  - Verified: ADR-028 (amended), `test/smoke/machine.timeline`.
- [ ] `M4-PRACTICE-03` Return targets: a zone to aim returns at (for example, dink into the kitchen), with its own streak.
  - Accept: a PRACTICE title entry; the machine feeds the chosen shot to the chosen target at the chosen rate and speed; the HUD shows the return streak, and the best streak is saved; no points are scored; host tests cover feeds and streaks.

## Sound

The game is silent today (ADR-009). `src/audio_events.c` is a seam that
counts the simulation's events (SERVE, HIT with the shot type, BOUNCE,
FAULT, POINT) for the tests. A sound driver replaces its body without
touching the simulation.

- [x] `M4-AUDIO-01` Sound driver and effects: paddle hits (standard, drive, and soft sound different), bounces, the net, the serve, and fault and point stings.
  - Accept: an ADR picks the driver (PVSnesLib's SNESMOD, or a small custom SPC700 sound-effect player) and the ARAM budget (64 KiB: driver, BRR samples, echo buffer). Effects play from `audio_on_event` with no frame overruns (the main CPU only sends commands). The ROM cost of BRR samples is recorded. Effects are tested on hardware.
  - Verified: [ADR-029](../docs/decisions/ADR-029-sound-effects.md), `test/test_audio.c`, MesenCE voice probe. Hardware listening is `M4-AUDIO-04`.
- [x] `M4-AUDIO-02` Crowd: light applause after points, louder for long rallies and match point, a cheer on the match result, and maybe a quiet murmur loop.
  - Accept: rally length (already tracked for records) scales the reaction; a crowd volume or off option on the OPTIONS page.
  - Verified: ADR-029 (applause scales with the rally, loudest on the match point; OPTIONS SOUND: ALL / NO CROWD / OFF). The cheer and murmur loop are left for later.
- [ ] `M4-AUDIO-04` Listen on real hardware (levels and mix through a TV), and tune the effects (`make sfx-preview` writes WAVs).
- [x] `M4-AUDIO-03` Subtle background sound instead of music: a venue ambience loop (murmur with birds, gulls, or crickets).
  - Verified: [ADR-031](../docs/decisions/ADR-031-ambience.md), `test_audio.c`, MesenCE voice probe. A real tune stays optional.

## Play modifiers

- [x] `M4-MOD-01` A modifiers menu and the first modifiers (big ball, fast ball, first to 11, kitchen is lava).
  - Accept: each modifier has a rules or physics test; the choice lives in `MatchConfig` and is optionally saved.
  - Verified: [ADR-033](../docs/decisions/ADR-033-modifiers.md), `test/test_modifiers.c`, `test_save.c: test_migrate_v7`, `modifiers.timeline`. First to 11 was already a POINTS choice (ADR-025), so the scoring modifier is WIN BY 2. Doubles with all four drops frames (budgeted; see `M4-UI-04`).

## Polish

- [ ] `M4-UI-03` Doubles attract demo frame budget: about 1 dropped frame in 40 (since the doubles demo, ADR-018; found in ADR-028). It averages about 197 of 225 scanlines: four AIs, four player updates (about 49), and the title. `make perf` holds it to its budget; see `docs/REFACTOR_PLAN.md` R15.
  - Progress: 4-12 overruns over the demo (was 160), by power-on state and build: the CPU schedule (ADR-018, amended), a shared and blocked landing prediction, and the title prompt and logo drawn in cheaper steps. `demo.timeline` budget 16, `menu.timeline` 10.
  - Accept: zero overruns over a full doubles demo (about frames 4,300-9,300 from boot), without changing human matches or the golden trace.

- [x] `M4-UI-02` Doubles frame headroom: the doubles timeline peaks at scanline 223-226 of 225 (one dropped frame on the options branch, two on the old `main`).
  - Verified: ADR-026; `hud_panel` now uses the assembly fill, and doubles has zero overruns.
  - Accept: zero overruns on the doubles timeline and a 3,000-frame doubles soak, with a few scanlines of margin.

- [ ] `M4-UI-04` Doubles ladder matches drop frames: 17 in a 2,400-frame match against DAD and MOM before the modifiers, 26-48 with all four modifiers (most from FAST BALL's longer swing lookahead; ADR-033).
  - Progress: the plain ladder match drops none (`doubles_ladder.timeline`, budget 0); all four modifiers drop 7-14 (`modifiers_doubles.timeline`, budget 20). See `docs/REFACTOR_PLAN.md` R15.
  - Accept: zero overruns on that timeline, without changing the golden trace. Pairs with `M4-UI-03` (the same four-player AI cost).

- [ ] `M4-UI-01` Draw the title pages faster.
  - Accept: the records page fills in under about 0.25 s (today about 0.5 s, because rows wait for frames with time left while the demo runs) with zero overruns. One option is an assembly text writer like `hud_fill`.
