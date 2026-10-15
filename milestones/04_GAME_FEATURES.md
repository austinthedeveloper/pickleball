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
- [x] `M4-SAVE-03` Initials entry for new records, arcade style.
  - Accept: a new best streak or longest rally asks for three letters, which the records page shows; the save layout change bumps `SAVE_VERSION`.
  - Verified: [ADR-040](../docs/decisions/ADR-040-record-initials.md), save layout 9 (48 bytes; 4, 7, 8 migrate), `test_save.c: test_migrate_v8, test_initials`, `test_story.c: test_initials_card`; the golden run ends its first match on the card; zero overruns in `make perf`.
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
- [x] `M4-VENUE-02` Preview a venue on the COURT page before leaving it.
  - Accept: scrolling VENUE shows the backdrop (reload with a short fade, or a thumbnail) without dropped frames; Y still restores the original.
  - Verified: [ADR-024](../docs/decisions/ADR-024-venues.md) (amended): a rested VENUE reloads behind the page in about 25 frames (the ambience waits for the page to close); `venue_preview.timeline` with zero overruns in `make perf`; golden trace identical.
- [ ] `M4-VENUE-03` More venues from new mocks (indoor rec center, rooftop at night, beach boardwalk, championship stadium).
  - Accept: each passes the shared-ground and 896-tile checks; about 14 KiB of ROM each (the 512 KiB cart has about 300 KiB free).

## Opponents and story

- [x] `M4-OPP-01` Eight CPU personalities with looks, play styles, and a saved ladder.
  - Verified: [ADR-023](../docs/decisions/ADR-023-cpu-personalities.md), `test/test_opponents.c`, `make stats OPPONENT=n`.
- [x] `M4-OPP-02` Tune the doubles partners.
  - Accept: a doubles balance run (`tools/match_stats.c` is singles only today) shows each team's difficulty in ladder order.
  - Verified: [ADR-038](../docs/decisions/ADR-038-doubles-balance.md) (amended): `make stats LATE=0 OPPONENT=-2 MODE=1` (and `MODE=3`, the net bot) prints the doubles ladder in order for both bots (bot wins of 60: 59, 44, 36, 26, 21, 9, 3, 2 and 60, 59, 42, 29, 20, 17, 7, 2, after the singles pass); four partners retuned, singles unchanged.
- [x] `M4-OPP-03` Give BRAD a distinct difficulty.
  - Accept: his matches stop being a coin flip of balls hit out; his bot win rate sits between MARGE's and PETE's in the ADR-023 table.
  - Verified: [ADR-023](../docs/decisions/ADR-023-cpu-personalities.md) (amended): late every 4th (was 2nd), slower to react and move; bot wins of 180 MARGE 123, BRAD 96 (was 80), PETE 79, and better bots now beat him more (11 → 20 of 30, was flat); the doubles ladder stays in order (CHAD late 7).
- [x] `M4-AI-01` Net play: CPUs volley once the bounces are done and come up to the kitchen line, scaled by ladder rung; a human's CPU partner plays it too.
  - Verified: [ADR-035](../docs/decisions/ADR-035-net-play.md), `test_ai.c: test_net_play`, the ADR-035 bot table (no early-volley faults), golden trace identical.
- [x] `M4-AI-02` Situational shots: drops and dinks against net players, put-aways, dink rallies with speed-ups.
  - Verified: [ADR-037](../docs/decisions/ADR-037-situational-shots.md), `test_ai.c: test_tactics`; against a net-playing bot every net team is tougher in singles (CHAMPS 24 → 7 bot wins of 60). Not done: speed-ups out of a dink rally (put-aways of high balls stay).
- [x] `M4-AI-05` Balance doubles: TWINS are easy for a net-playing human (57-59 of 60 bot wins), GOTH CLUB over-dink, HENDERSON and GYM BROS are near-certain wins.
  - Verified: [ADR-038](../docs/decisions/ADR-038-doubles-balance.md), `test_ai.c: test_footwork, test_middle_aim`; the doubles ladder gets harder each rung (average bot wins of 60: 60, 52, 49, 37, 22, 20, 18, 4; TWINS against the net bot 59 → 21). The cause was CPU mis-hits from sliding into the swing (`footwork`).
- [x] `M4-AI-03` Doubles teamwork: the CPU partner moves up and back with you, covers the middle, poaches.
  - Verified: [ADR-036](../docs/decisions/ADR-036-teamwork.md), `test_ai.c: test_teamwork`, `test_doubles.c`; middle balls lost untouched near zero for both sides in the ADR-036 table.
- [x] `M4-AI-04` Smarter aim: the gap between doubles opponents, the open court.
  - Verified: [ADR-039](../docs/decisions/ADR-039-smarter-aim.md), `test_ai.c: test_doubles_aim`; in doubles, smart teams hit at the one back when the opponents split up and back, an open lane, else the middle every other shot (ADR-038). Singles already aimed at the open court (`away`, ADR-037 depth).
- [x] `M4-OPP-04` Opponent accessories (hats, bandanas, a pirate tricorn) using the accessory object (ADR-014).
  - Accept: they fit the OBJ tile budget (208 of 256 used) and the per-row sprite limits.
  - Verified: [ADR-014 hats amendment](../docs/10_MOCKUP_DECISIONS.md) — five hats (visor, headband, tricorn, bandana, bare hair) on 13 of the 16 opponents, drawn in front of their own bodies (OAM groups 2–13); no new OBJ tiles (249 of 256); worst row 32 of 34, the stress scene dropout-free; `test_opponents.c` (hats by slot), `test_asset_validation.py` (unknown hat); zero overruns in `make perf`, golden trace identical.
- [x] `M4-STORY-01` Story cards: an intro card per opponent (name, one line of trash talk, rung) and the opponent's line on the match result.
  - Verified: [ADR-026](../docs/decisions/ADR-026-story-cards.md), `test/test_story.c`, `story.timeline`. Uses the HUD over the live court; portraits are `M4-STORY-03`.
- [x] `M4-STORY-02` A ladder ending after beating CHAMPS (CHAMPION!, plus a "ladder clears" record).
  - Verified: ADR-026, `test_story.c`.
- [x] `M4-STORY-03` Opponent portraits on the intro card (BG2, like Player Select's).
  - Accept: one portrait per team in palette roles; fits the BG2 portrait budget (ADR-020) and VRAM alongside the court.
  - Verified: [ADR-026](../docs/decisions/ADR-026-story-cards.md) (amended), the captain's bust in palette roles through BG palette 1 (51 tiles at `0x4800`, past BG1's reservation; docs/09); `story.timeline` shows DAD's bust and none after B; the asset build rejects off-role busts (`test_asset_validation.py`); zero overruns in `make perf`, golden trace identical.

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
- [x] `M4-OPEN-05` A bigger, scrolling park (more courts or a clubhouse). All eight OBJ palettes are in use (you, six teams on court, objects) and the scores are BG3 text, so it needs sprite scores or BG-baked labels, and palettes shared or swapped by what is on screen.
  - Verified: ADR-034 (amended: the clubhouse), `test_open_play.c: test_clubhouse`, `clubhouse.timeline` (zero overruns; MesenCE frames of the scroll, the board, practice and back, the bench); `open_play.timeline` still zero. The user chose the clubhouse over a fourth court.
- [x] `M4-OPEN-03` Partners from the stack: play with the person stacked with you (their looks and style), not a fixed regular.
  - Verified: ADR-034 (amended), `test_opponents.c: test_partner`, `test_open_play.c`; `open_play.timeline` shows "WITH PETE VS SURF CREW" and PETE's look in the match.
- [x] `M4-OPEN-04` A second player in the park: 2P joins and plays as your partner.
  - Verified: ADR-034 (amended), `test_open_play.c: test_two_players` (co-op pad mapping, versus unchanged). The headless runner cannot press port 2 (KNOWN_ISSUES #16): check by hand with a second pad.

## Practice

- [x] `M4-PRACTICE-01` Practice mode with a ball machine (see IDEAS.md, "Practice mode").
  - Verified: [ADR-028](../docs/decisions/ADR-028-practice.md), `test/test_practice.c`, `practice.timeline`.
- [x] `M4-PRACTICE-02` A ball machine sprite on the far baseline (a few OBJ tiles; 208 of 256 used).
  - Verified: ADR-028 (amended), `test/smoke/machine.timeline`.
- [x] `M4-PRACTICE-04` The machine aims: the nozzle turns toward each feed's lane before it leaves, so you can read where the ball is going.
  - Verified: ADR-028 (amended), `test_practice.c: test_machine_aim`, `machine.timeline` (MesenCE frames of the turn and the kick); `practice.timeline` still zero overruns.
- [x] `M4-PRACTICE-03` Return targets: a zone to aim returns at (for example, dink into the kitchen), with its own streak.
  - Verified: ADR-028 (amended: return zones), `test_practice.c: test_zones`, `test_save.c: test_migrate_v8, test_migrate_v9` (save layout 10 after the initials' 9; layout 9 at the time, also on the ROM from a hand-built v8 `.srm`), `zones.timeline` (zero overruns; MesenCE frames of the page and the kitchen's corners). The page, feeds, rate, speed, and saved best were already ADR-028's; this adds RETURN TO and a best per zone.
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

- [x] `M4-UI-03` Doubles attract demo frame budget: about 1 dropped frame in 40 (since the doubles demo, ADR-018; found in ADR-028). It averages about 197 of 225 scanlines: four AIs, four player updates (about 49), and the title. `make perf` holds it to its budget; see `docs/REFACTOR_PLAN.md` R15.
  - Progress: 4-12 overruns over the demo (was 160), by power-on state and build: the CPU schedule (ADR-018, amended), a shared and blocked landing prediction, and the title prompt and logo drawn in cheaper steps. `demo.timeline` budget 16, `menu.timeline` 10.
  - Accept: zero overruns over a full doubles demo (about frames 4,300-9,300 from boot), without changing human matches or the golden trace.
  - Verified: `player_step` in assembly (`src/asm/player_step.asm`; four player updates about 49 → 19 scanlines with the profiler on) and the separation by constant slots; `demo.timeline` and `menu.timeline` budgets 0 (worst frame 190 and 179 of 225). Golden trace and a 1,200-frame doubles trace identical. See `docs/REFACTOR_PLAN.md` R15.

- [x] `M4-UI-02` Doubles frame headroom: the doubles timeline peaks at scanline 223-226 of 225 (one dropped frame on the options branch, two on the old `main`).
  - Verified: ADR-026; `hud_panel` now uses the assembly fill, and doubles has zero overruns.
  - Accept: zero overruns on the doubles timeline and a 3,000-frame doubles soak, with a few scanlines of margin.

- [x] `M4-UI-04` Doubles ladder matches drop frames: 17 in a 2,400-frame match against DAD and MOM before the modifiers, 26-48 with all four modifiers (most from FAST BALL's longer swing lookahead; ADR-033).
  - Progress: the plain ladder match drops none (`doubles_ladder.timeline`, budget 0); all four modifiers drop 7-14 (`modifiers_doubles.timeline`, budget 20). See `docs/REFACTOR_PLAN.md` R15.
  - Progress: a doubles ladder loss dropped 2-3 frames on the result screen: the prompt and the opponent's taunt (up to 28 characters, about 130 scanlines of text) each drew in one step on a frame already at about 115. The result now draws a panel and its text on separate frames, the taunt in halves (`hud_center_prompt_panel`, `_half`). `doubles_ladder.timeline` reaches it now (the CPUs win 5-0) and drops none.
  - Accept: zero overruns on that timeline, without changing the golden trace. Pairs with `M4-UI-03` (the same four-player AI cost).
  - Verified: the `M4-UI-03` changes; `modifiers_doubles.timeline` budget 0 (worst frame 208), `doubles_ladder.timeline` 0 (186).

- [x] `M4-UI-01` Draw the title pages faster.
  - Accept: the records page fills in under about 0.25 s (today about 0.5 s, because rows wait for frames with time left while the demo runs) with zero overruns. One option is an assembly text writer like `hud_fill`.
  - Verified: the records page fills in 4-8 frames after the button (0.07-0.13 s; it was 15-18). `hud_text`'s loop is assembly (`src/asm/hud_text.asm`, a page row about 60 → a few scanlines), the area draws parts back to back while the frame is under its limit, and on the no-op logo steps between title views. The BG3 map is identical at every shot of nine HUD timelines. A new title prompt draws in three steps (panel, two halves): in one step (about 106 scanlines) it dropped a frame on `court.timeline` once the demo got faster.
