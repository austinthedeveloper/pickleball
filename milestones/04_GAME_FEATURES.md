# M4 — Game Features

Versions: M4 work ships in tagged releases from 0.4.0 (the first GitHub
release) on; the current one is `VERSION_NAME` in the `Makefile` (see
[RELEASING.md](../docs/RELEASING.md)).

Outcome: the POC grows into a game you return to: saved records and
settings, themed opponents on a ladder, and several venues. Design notes for
each idea are in [IDEAS.md](../docs/IDEAS.md); decisions are ADR-021 on, in
[docs/decisions/](../docs/decisions/).

## Save and records

- [x] `M4-SAVE-01` Battery-backed SRAM with a checksummed, double-buffered save block.
  - Accept: a torn or blank save falls back safely; records persist across boots.
  - Verified: [ADR-021](../docs/decisions/ADR-021-battery-save.md), `test/test_save.c`, `records.timeline` with `KEEP_SAVE=1`.
- [x] `M4-SAVE-02` RECORDS page on the title, with an erase combo.
  - Verified: ADR-021; zero frame overruns on the records timeline.
- [x] `M4-SAVE-03` Initials entry for new records, arcade style.
  - Accept: a new best streak or longest rally asks for three letters at the next game over (a loss to the CPU, the ladder cleared, or a 2P match's end), which the records page shows; the save layout change bumps `SAVE_VERSION`.
  - Verified: [ADR-040](../docs/decisions/ADR-040-record-initials.md), save layout 9 (48 bytes; 4, 7, 8 migrate), `test_save.c: test_migrate_v8, test_initials`, `test_story.c: test_initials_card, test_initials_wait_for_game_over`; the golden run ends its first match on the card; zero overruns in `make perf`.
- [ ] `M4-SAVE-04` Verify the save on a physical cart with SRAM and a battery.
  - Accept: records survive a power cycle on real hardware; logged in `test/hardware-log.md`.

## Court colours and venues

- [x] `M4-COURT-01` Court colour presets for surface, surround, and lines, with a live preview on the COURT page.
  - Verified: [ADR-022](../docs/decisions/ADR-022-court-colors.md), contrast checks in the asset build, `court.timeline`.
- [x] `M4-COURT-02` Two-tone court: the kitchen gets its own colour part (see IDEAS.md, "Two-tone court").
  - Verified: [ADR-027](../docs/decisions/ADR-027-two-tone-kitchen.md), `test_court_style.c`, `court.timeline`.
  - Accept: the kitchen paints with its own palette-2 entries; a KITCHEN row (with SAME as an option) previews live; every kitchen preset passes the line contrast check; venues can set a default kitchen.
- [x] `M4-COURT-03` Night courts: a TIME row (DAY, DUSK, NIGHT) on the COURT page for any venue, with a floodlit court and a night ambience (see IDEAS.md, "Night courts").
  - Accept: the backdrop and surround darken while the court and lines stay readable at every time (the contrast checks pass); the change previews live without dropped frames; it is saved, and older saves migrate.
  - Verified: [ADR-045](../docs/decisions/ADR-045-night-courts.md), `test_court_style.c: test_editor, test_times`, `test_save.c: test_migrate_v10`, the asset build's contrast checks per time, `court.timeline` zero overruns in `make perf` (the transform in assembly), MesenCE at every venue; golden trace identical.
- [x] `M4-VENUE-01` Venues PARK, LAKESIDE, and SUNSET with shared ground tiles and home courts.
  - Verified: [ADR-024](../docs/decisions/ADR-024-venues.md), `venue.timeline`, `lake.timeline`.
- [x] `M4-VENUE-02` Preview a venue on the COURT page before leaving it.
  - Accept: scrolling VENUE shows the backdrop (reload with a short fade, or a thumbnail) without dropped frames; Y still restores the original.
  - Verified: [ADR-024](../docs/decisions/ADR-024-venues.md) (amended): a rested VENUE reloads behind the page in about 25 frames (the ambience waits for the page to close); `venue_preview.timeline` with zero overruns in `make perf`; golden trace identical.
- [x] `M4-VENUE-04` Fun venues: DINOSAUR, SPACE, and UNDERSEA, each with its own ambience, default court, and home-court opponent.
  - Verified: [ADR-046](../docs/decisions/ADR-046-fun-venues.md), the asset build's venue checks, `make perf` zero overruns, golden trace identical, MesenCE matches at each.
- [ ] `M4-VENUE-03` More venues from new mocks (indoor rec center, rooftop at night, beach boardwalk, championship stadium). The soundbank now has banks 13-15 (ADR-046, amended), about 30 KiB free for ambience.
  - Accept: each passes the shared-ground and 896-tile checks; about 14 KiB of ROM each (the 1 MiB cart has about 620 KiB free since R19).

## Opponents and story

- [x] `M4-OPP-01` Eight CPU personalities with looks, play styles, and a saved ladder.
  - Verified: [ADR-023](../docs/decisions/ADR-023-cpu-personalities.md), `test/test_opponents.c`, `make stats OPPONENT=n`.
- [x] `M4-OPP-05` A ladder per mode (singles and doubles each keep their rung), and quitting a ladder match from the pause menu starts that mode's ladder over. Save layout 14.
  - Accept: doubles climbs without moving singles; pause, MAIN MENU resets the mode's rung and saves it; a loss, RESTART, and power-off keep it; a v13 save keeps its place in both modes; zero overruns.
  - Verified: [ADR-055](../docs/decisions/ADR-055-ladder-per-mode.md), `test_save.c: test_migrate_v13`, `test_opponents.c: test_ladder_per_mode_and_quit`, MesenCE (doubles rung 3 is 0 after the quit, singles unchanged; the records page reads `S 5/8 D 3/8`); golden trace identical; `make perf` zero overruns.
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
  - Verified: [ADR-038](../docs/decisions/ADR-038-doubles-balance.md), `test_ai.c: test_footwork, test_doubles_aim` (the middle aim; `test_middle_aim` until ADR-039); the doubles ladder gets harder each rung (average bot wins of 60: 60, 52, 49, 37, 22, 20, 18, 4; TWINS against the net bot 59 → 21). The cause was CPU mis-hits from sliding into the swing (`footwork`).
- [x] `M4-AI-03` Doubles teamwork: the CPU partner moves up and back with you, covers the middle, poaches.
  - Verified: [ADR-036](../docs/decisions/ADR-036-teamwork.md), `test_ai.c: test_teamwork`, `test_doubles.c`; middle balls lost untouched near zero for both sides in the ADR-036 table.
- [x] `M4-AI-04` Smarter aim: the gap between doubles opponents, the open court.
  - Verified: [ADR-039](../docs/decisions/ADR-039-smarter-aim.md), `test_ai.c: test_doubles_aim`; in doubles, smart teams hit at the one back when the opponents split up and back, an open lane, else the middle every other shot (ADR-038). Singles already aimed at the open court (`away`, ADR-037 depth).
- [x] `M4-OPP-04` Opponent accessories (hats, bandanas, a pirate tricorn) using the accessory object (ADR-014).
  - Accept: they fit the OBJ tile budget (208 of 256 used) and the per-row sprite limits.
  - Verified: [ADR-014 hats amendment](../docs/10_MOCKUP_DECISIONS.md) — five hats (visor, headband, tricorn, bandana, bare hair) on 13 of the 16 opponents, drawn in front of their own bodies (OAM groups 2–13); no new OBJ tiles (249 of 256); worst row 32 of 34, the stress scene dropout-free; `test_opponents.c` (hats by slot), `test_asset_validation.py` (unknown hat); zero overruns in `make perf`, golden trace identical.
- [x] `M4-OPP-06` Rebalance the ladder: rungs past 3 beatable on MEDIUM, and CPU short serves that can be returned.
  - Accept: a CPU short serve is soft and to the middle of the box; on MEDIUM the bot wins at least 8 of 60 against every rung in every mode, no fewer than before on any rung; every ladder table in order; zero overruns.
  - Verified: [ADR-062](../docs/decisions/ADR-062-ladder-rebalance.md), `test_ai.c: test_cpu_serve_power`, `make stats` in all three modes (ladder order: yes); `make perf` zero overruns; golden trace differs (DAD's short serves), re-record after merging.
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
- [x] `M4-OPT-03` BALL SPEED on the OPTIONS page (70-120%), default 100% until the speed is chosen by play.
- [x] `M4-CAM-01` Camera: the court leans a little toward the ball (up to 8 px across, 4 along), with a CAMERA row on the OPTIONS page (OFF, 50-100%; default OFF since the amendment, 85% before).
  - Verified: [ADR-050](../docs/decisions/ADR-050-camera.md), `test_options.c: test_camera`, `test_layout.c`, golden trace identical, `make perf` zero overruns, MesenCE frames at both extremes of a rally.
- [x] `M4-CAM-02` Parallax: with the camera, the backdrop pans at half its speed and the near fence leans out to 1.5 times it (HDMA).
  - Verified: [ADR-051](../docs/decisions/ADR-051-parallax.md), the asset build's net post check, golden trace identical, `make perf` zero overruns, MesenCE frames of the net, the near fence, and the watcher at both extremes.
  - Verified: [ADR-049](../docs/decisions/ADR-049-ball-speed.md), `test_options.c`, `test_modifiers.c: test_ball_steps`, golden trace identical, `make perf` zero overruns.

## Serves and shots

- [x] `M4-SERVE-01` Charged serve: a press serves safely; holding runs a power meter, and the release serves at its level, with the bottom of the meter on the way down a dud into the net. Not on KID.
  - Accept: a tap (also an early one) is the classic serve; the meter rises and falls while held; more charge is a faster serve that still lands in; a release at the bottom on the way down hits the net; CPUs and KID serve as before; zero overruns.
  - Verified: [ADR-044](../docs/decisions/ADR-044-charged-serve.md), `test_serve.c: test_charged_meter, test_charged_serves_land, test_tap_is_classic`, MesenCE (a tap serves in, a full charge lands in, the bar follows the level); ladder tables unchanged; `make perf` zero overruns; golden trace re-recorded (a tap after the ready frame serves a frame later).
- [x] `M4-SERVE-02` CPU serve power from personality, with a small chance of a dud.
  - Accept: `serve_power` and `serve_dud` style knobs; the default CPU and partner unchanged; GYM BROS and CHAMPS serve hard and sometimes dud; every ladder table in order; zero overruns.
  - Verified: [ADR-058](../docs/decisions/ADR-058-cpu-serve-power.md), `test_ai.c: test_cpu_serve_power`, `make stats` in all three modes (ladder order: yes), MesenCE (BRAD's serves at power 30 and a dud); golden trace identical; `make perf` zero overruns.
- [x] `M4-SHOT-01` Slice on X: a slow shot that buys time, never deep, steady off a stretch.
  - Accept: slower than a standard shot from mid-court and the net, never above a standard shot's longest flight; neutral or short; less stretch spray; its own sound; CPUs unchanged; zero overruns.
  - Verified: [ADR-056](../docs/decisions/ADR-056-slice-and-short-hold.md), `test_collision.c: test_slice_and_fast_drop`, `test_input.c: test_slice_and_short_hold`, MesenCE (ball machine landings per shot); golden trace identical; `make perf` zero overruns.
- [x] `M4-SHOT-02` Hold L or R to hit any shot short (L+A a fast drop), with the D-pad free to move and aim at a sideline; a stretched fast drop flies past the kitchen.
  - Accept: L or R aims away from your own net on either team; pressing away still works; a fast drop met off a stretch lands past neutral; CPUs unchanged.
  - Verified: as `M4-SHOT-01`.
- [x] `M4-SHOT-03` CPUs slice: a per-style choice in `opponents.json` (a soft-game team resets with it), tuned against the ladder order.
  - Accept: a `slice` knob (a slice every Nth rally shot, 0 never), the default and partner styles 0; GOTH CLUB and CHAMPS slice; every ladder table in order; zero overruns.
  - Verified: [ADR-057](../docs/decisions/ADR-057-cpu-slice.md), `test_ai.c: test_slice`, `make stats` in all three modes (ladder order: yes), MesenCE (RAVEN slices on the ROM); golden trace identical; `make perf` zero overruns.

## Rules

- [x] `M4-RULES-01` The kitchen fault: a volley with the feet in your own kitchen, or on its line, loses the point (KITCHEN!). Walking in it, and hitting a bounced ball there, stay legal. CPUs never volley from it. LAVA is unchanged.
  - Accept: a rules test for the zone, its line, and the sidelines on both sides; CPUs draw no kitchen faults; the ladder tables unchanged; zero overruns.
  - Verified: [ADR-052](../docs/decisions/ADR-052-kitchen-fault.md), `test_rules.c: test_kitchen_volley_loses_point`, `test_ai.c: test_net_play`, MesenCE (a poked volley in the kitchen ends the point with KITCHEN!); `make stats` identical in all three modes; golden trace identical.

## Fun

- [x] `M4-FUN-01` Quips: the side that loses a point sometimes says a PG line (from GOOFY_IDEAS.md).
  - Verified: [ADR-048](../docs/decisions/ADR-048-quips.md), `test_story.c: test_quips`, the asset build's line checks, MesenCE screenshots; `make perf` zero overruns, golden trace identical.
- [x] `M4-FUN-02` More quips: sixteen a side, in a shuffled order.
  - Accept: sixteen PG lines for every ladder team, the human side, and the default CPU; a side says all of them before any again, in a new order each pass; display only; zero overruns.
  - Verified: [ADR-063](../docs/decisions/ADR-063-more-quips.md), `test_story.c: test_quips`, the asset build's line checks, MesenCE screenshots of the golden run's point results; `make perf` zero overruns; golden trace identical from Player Select on (the title starts a frame later).
- [x] `M4-FUN-03` Close call replay: a long rally that ends just out shows a close-up of the line, and by how many inches (from GOOFY_IDEAS.md).
  - Verified: [ADR-041](../docs/decisions/ADR-041-close-call-replay.md), `test/test_replay.c`, MesenCE with the thresholds dropped; display only, golden trace identical; `make perf` zero overruns.
- [x] `M4-FUN-04` The partner sitting out: in a singles ladder match the opponent's doubles partner watches from the venue's stands and cheers their teammate's points (from GOOFY_IDEAS.md).
  - Verified: [ADR-042](../docs/decisions/ADR-042-sitting-out.md), the asset build's watcher spot check, MesenCE at PARK, LAKESIDE, and SUNSET (nobody in doubles); display only, golden trace identical; `make perf` zero overruns.

## Career

- [x] `M4-CAREER-01` Career players, the data: three save slots, each a named player with a look, lifetime stats, and unlocks; stats from points and matches against the CPU; save layout 12.
  - Verified: [ADR-047](../docs/decisions/ADR-047-career.md), `test/test_career.c`, the save written and reloaded in MesenCE; `make perf` zero overruns (the save sum in assembly).
- [x] `M4-CAREER-02` The CAREER page (RECORDS becomes CAREER): slot picker, player card with stats and unlocks, and the editor (name, body, skin, hair, shirt, hat, paddle; locked items skipped).
  - Verified: [ADR-047](../docs/decisions/ADR-047-career.md) (stage 2), `test/test_career_page.c`, `career.timeline` and the updated `records.timeline` with zero overruns in `make perf`, MesenCE screenshots of each view and the live preview.
- [x] `M4-CAREER-03` Play as the active player: 1P's look and paddle, the name on the HUD, and new unlocks on the result screen.
  - Verified: [ADR-047](../docs/decisions/ADR-047-career.md) (stage 3), `test_career_page.c: test_no_player`, `test_story.c: test_unlock_card`; MesenCE: Player Select on the player's look, ACE on the score panel, UNLOCKED! VISOR after a win; `make perf` zero overruns (the SRAM copy in assembly).
- [x] `M4-CAREER-04` Gear that changes stats, always a trade-off: a career player's hat moves a paddle stat point.
  - Verified: [ADR-047](../docs/decisions/ADR-047-career.md) (stage 4), `test_career.c: test_gear`, the asset build's budget and 15% checks for every pairing, MesenCE (the editor's stat line, Player Select's bars); golden trace identical, `make perf` zero overruns.
- [x] `M4-CAREER-05` The HAT MUSEUM: every hat in a display case on the career player, locked ones a black silhouette with how to earn them; X on the player's card.
  - Accept: unlocked hats in the player's colours and wearable with B; locked ones dark, named ???, with the unlock's hint; zero overruns; the save written only on leaving.
  - Verified: [ADR-061](../docs/decisions/ADR-061-hat-museum.md), `test/test_museum.c`, `test_career_page.c` (X opens it), `museum.timeline` in `make perf`; screenshots with no hats and with eight unlocked.

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
- [x] `M4-OPEN-06` Park points: a career player's park rating, shown as points, moves with their park games and opens the CLUB and CHAMP courts (the courts were <3.5, 3.5-4.5, and 4.5+).
  - Accept: 3.0 to start; a win at your level or above counts for more than a loss, below it for nothing; a court opens at its level and stays open; a closed court refuses your paddle and says why; the result and the park show the move; unrated (no career player, KID) every court is open; no save layout change; simulation unchanged; zero overruns.
  - Verified: [ADR-070](../docs/decisions/ADR-070-park-rating.md), `test_open_play.c: test_rating`, `test_career.c: test_park_rating`, MesenCE (`park_rating.timeline` with pokes: the locked rack, a court opened on the result, the points in the park and on the card); golden trace and lockstep identical; `make perf` zero overruns.
- [x] `M4-OPEN-07` Winners stay on (from IDEAS.md): the winners of a park game stay on and the next entry challenges them; the losers join a stack; after two wins in a row the winners come off too.
  - Accept: a per-court streak; one new entry against the winners; nobody waiting sends them off; you, winning, are called again with the same partner against a team that isn't your partner's; losing or at the cap you come off; the call says `WINNERS STAY: VS ...`, the cap's result `YOU WON 11-4  2 IN A ROW` (then the points); host tests (a soak with stays, nobody lost or doubled); match, park points, and simulation unchanged; zero overruns.
  - Verified: [ADR-071](../docs/decisions/ADR-071-winners-stay.md), `test_open_play.c` (`test_winners_stay`, `test_you_stay`, `test_you_lose`, `test_stay_nobody_waiting`, `test_stay_soak`), MesenCE (`winners_stay.timeline` with pokes: the first call, the win, `WINNERS STAY: VS SNOWBIRDS`, the second win and `2 IN A ROW`); golden trace and lockstep identical; `make perf` zero overruns.
- [x] `M4-OPEN-08` Pick your partner: a career player chooses their park partner from the captains of the ladder teams they have beaten.
  - Accept: Y at your rack (holding your paddle) cycles next in line, each beaten team's captain in ladder order, and back; nothing elsewhere, with no beaten teams, or with 2P; the HUD shows the choice (`@ TAKE  Y PARTNER: MARGE`); a fresh call partners you with the pick and never draws their team against you; winners stay keeps the pick; the pick is WRAM only (no save change); host tests (cycling, calls, a soak with the population checked); match setup and simulation unchanged; zero overruns.
  - Verified: [ADR-072](../docs/decisions/ADR-072-pick-partner.md), `test_open_play.c` (`test_pick_cycle`, `test_pick_called`, `test_pick_vs_winners`, `test_pick_soak`), MesenCE (`pick_partner.timeline` with pokes: the HUD cycling NEXT UP, MARGE, RAVEN, the call `WITH MARGE VS GYM BROS`, the match with MARGE); golden trace and lockstep identical; `make perf` zero overruns.

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
- [x] `M4-MOD-02` More modifiers: GHOST BALL (the ball blinks out in flight; its shadow stays), POPCORN (every hit speeds the ball up, reset each serve), and ICE (players slide). LAVA unchanged.
  - Accept: each has a test; the CPUs still rally with each; old saves load with the new rows off; zero overruns in a doubles match with all three and FAST BALL; the golden trace unchanged with them off.
  - Verified: [ADR-053](../docs/decisions/ADR-053-fun-modifiers.md), `test_modifiers.c: test_popcorn_pace, test_ice, test_matches`, `modifiers_fun.timeline` (in `make perf`, 0 overruns), MesenCE (the ghost ball over the net, POPCORN's level per hit, the ICE slide on the asm `player_step`); `make stats` and the golden trace identical.
- [x] `M4-MOD-03` GLOW (neon outlines on the players and the ball, neon outfits, hats, and paddles, a night court with lime lines) and INVERSE (every court and player colour inverted). Save layout 13 for the second modifiers byte.
  - Accept: a v12 save loads with everything kept and INVERSE off; the title after a GLOW or INVERSE match shows its own colours; zero overruns; the golden trace unchanged.
  - Verified: [ADR-054](../docs/decisions/ADR-054-glow-and-inverse.md), `test_save.c: test_migrate_v12`, `test_modifiers.c: test_values_and_editor, test_select_copies`, `modifiers_glow.timeline` and `modifiers_inverse.timeline` (in `make perf`, 0 overruns), MesenCE screenshots; `make stats` and the golden trace identical.

## Multiplayer

- [ ] `M4-MP-01` Four humans in doubles with a Super Multitap in port 2 (the M3 seams; [Input and Multitap Plan](../docs/04_INPUT_MULTITAP_PLAN.md)). Players 3 and 4 join with Start on the title (the join counter, `M4-MP-02`), as 2P does. Teams by controller: 1P and 3P on the left, 2P and 4P on the right; a slot with no human is a CPU. Each human picks a colour on Player Select (no two the same), and Career players can sit in any slot. Open play takes up to four humans too. The instruction manual (`docs/MANUAL.md`) already describes this.
  - Accept: host tests for the pad-to-slot mapping with 1-4 humans (with and without a Multitap, KNOWN_ISSUES #13); a doubles match with four humans holds the frame budget; the activation checklist in the Multitap plan, on real hardware with a Multitap (`M3-IN-03`).
  - Built: [ADR-073](../docs/decisions/ADR-073-four-players.md), `test_match.c: test_four_players` (pad-to-slot mapping for 1-4 humans, with and without a Multitap), the debug ROM with injected controllers (`four_players.timeline`: four joins, Player Select in turns, four humans each on their own slot; a park game with 2P, 3P, and 4P); `make perf` zero overruns, golden and lockstep identical. Open: the real-hardware Multitap checklist (`M3-IN-03`), and career players in slots other than 1P.

- [x] `M4-MP-02` Ready for more players: a join counter on the title (`1P` to `4P`, a dot in each player's colour or a grey x), `FRIENDS: PRESS START`, 3P and 4P joining with a Multitap, and a ring at each player's feet in their colour (1P red, 2P blue, 3P yellow, 4P green, CPU grey).
  - Accept: zero overruns on `make perf`; golden trace identical; 3P and 4P sit out until `M4-MP-01`.
  - Verified: [ADR-066](../docs/decisions/ADR-066-join-counter-and-rings.md).

## Polish

- [x] `M4-ART-01` Character models: the port kit's bareheaded player set, the cap as hat 0, and the four hitting poses in name table 2 (ADR-059; fixes KNOWN_ISSUES #32).
  - Accept: same frame size, palette contract, and slot order; HAIR shows real hair; zero overruns on `make perf`; golden trace identical.
  - Verified: ADR-059. The hitting poses wait on the reach and overhead mechanics (IDEAS.md).

- [x] `M4-ART-02` More hats: beanie, bucket, backwards cap, propeller beanie, viking helmet, chef's toque, and a crown, as career unlocks (ADR-060).
  - Accept: every hat reads on all four teams, both facings, with a ponytail; old saves keep their hats; looks only (paddle stats unchanged).
  - Verified: ADR-060.
- [x] `M4-ART-03` The sprite wish list in play: point celebrations by team, the slump, the dive, emotes, bounce dust, the net shiver, the ball's squash, stretch, and spin streak, venue critters, the CHAMPION! trophy, and practice's pickle.
  - Accept: presentation only (play unchanged); each object hidden once when it stops; zero overruns in every `make perf` timeline.
  - Verified: [ADR-064](../docs/decisions/ADR-064-sprite-wishlist-in-play.md), `test_flair.c`, MesenCE screenshots of each; `make perf` zero overruns; golden trace identical from Player Select on (the title starts a frame later, the larger ROM).

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

- [x] `M4-UI-05` Menu cleanup: the title grouped (play, players and records, settings), RECORDS on the title, no demo scoreboard behind it, one speed setting, SCORING with POINTS, CAMERA ON/OFF with an AMOUNT, matching unlock counts, and a career player who picks only a paddle and is never asked for initials.
  - Accept: no save layout change; old saves keep their camera and WIN BY 2; zero overruns on `make perf`; golden trace identical.
  - Verified: [ADR-065](../docs/decisions/ADR-065-menu-cleanup.md).

- [x] `M4-UI-01` Draw the title pages faster.
  - Accept: the records page fills in under about 0.25 s (today about 0.5 s, because rows wait for frames with time left while the demo runs) with zero overruns. One option is an assembly text writer like `hud_fill`.
  - Verified: the records page fills in 4-8 frames after the button (0.07-0.13 s; it was 15-18). `hud_text`'s loop is assembly (`src/asm/hud_text.asm`, a page row about 60 → a few scanlines), the area draws parts back to back while the frame is under its limit, and on the no-op logo steps between title views. The BG3 map is identical at every shot of nine HUD timelines. A new title prompt draws in three steps (panel, two halves): in one step (about 106 scanlines) it dropped a frame on `court.timeline` once the demo got faster.

## Ports

- [x] `M4-PORT-01` A tuning reference for ports: every simulation constant and derived table in one generated, committed file (`docs/tuning/tuning.json`), a `TUNING_VERSION` bumped with any value change, conformance traces a port can diff frame by frame, and `docs/TUNING.md` (units, the port rules, the SNES-limited values).
  - Accept: `make check` fails while the export is stale or a value changes without a version bump; the export prints the values the game compiles (no number typed twice); golden trace identical; zero overruns on `make perf`.
  - Verified: [ADR-068](../docs/decisions/ADR-068-tuning-reference.md).

- [x] `M4-PORT-02` The ROM and the host in lockstep: one CPU schedule definition (`CPU_THINKS`) for the ROM, the harnesses and the traces, and `make lockstep`, which replays matches recorded from the ROM on the host and compares every simulation field every frame (the asm `player_step` and `ai_probe` against their C, and 816-tcc against cc).
  - Accept: `make lockstep` identical on a singles, a doubles, and a modifiers match; a change to the C twin alone fails it; `make stats` identical; zero overruns on `make perf`.
  - Verified: [ADR-069](../docs/decisions/ADR-069-rom-host-lockstep.md). Found and fixed KNOWN_ISSUES #36; #37 and #38 recorded.
