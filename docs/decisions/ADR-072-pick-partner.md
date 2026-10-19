# ADR-072 — Pick Your Partner

Status: **Accepted 2026-10-18**
Task: `M4-OPEN-08`. Amends: ADR-034 (open play), ADR-071 (winners stay).

## Why

At the park your partner is whoever's team waits next in line at that
court (ADR-034, M4-OPEN-03). At a real park you often go in with someone
you know. The ladder already records who you have beaten
(`CareerPlayer.beaten[]`, ADR-047): beating a team on the ladder earns
their captain's respect, and now their company at the park.

## Decision

- **Who you may pick:** the captains of the ladder teams the active
  career player has beaten (`beaten[team] > 0`). No career player: nobody
  (the park is as before). `main.c`'s `park_rate` builds the bit mask
  (bit n: ladder team n) and passes it to `open_play_partners`, at park
  entry and after each game, where it sets the rating. `open_play.c`
  stays pure.
- **The pick** is `OpenPlay.pick`: a team, or `OPEN_NONE` for "next in
  line" (the old behaviour, the default). WRAM only, reset by
  `open_play_begin`; not saved. A pick that leaves the mask goes back to
  next in line (it can't today: beaten[] only grows).
- **Y picks** (`open_play_pick_next`), only while you stand at the rack
  holding your paddle (`your_court` is that rack), with teams to pick and
  without 2P (2P is your partner). It cycles next in line, each team you
  may pick in ladder order, next in line again. Anywhere else Y does
  nothing (`open_pick_ready`). `update_park` calls it on Y with a blip.
- **Your call:** a fresh game's partner is the picked captain (else the
  captain next in line, as before). Their team is never your opponent:
  - In a fresh game (`court_start_game`), when you are first or second in
    the court's stack, both draws pass over the picked team (`next_entry`
    with `skip`, as winners stay passes over your partner's team).
  - If you would come on as the challenger against winners who are the
    picked team, the game is a fresh one instead (the winners come off and
    the fresh draw passes over them).
  - Nobody else to draw: the not-enough-players path, as before.
- **Winners stay** keeps the partner (ADR-071, unchanged): the picked
  team is skipped as the challenger. If their team was taken by another
  court during the changeover, the partner is "chosen afresh", which is
  the pick again.
- **The HUD** (BG3, the staged panel, 28 characters at most): at your
  rack with teams to pick, `@ TAKE  Y PARTNER: NEXT UP` (26) or
  `@ TAKE  Y PARTNER: RAVEN` (24 at the longest captain, five letters);
  `@` is the B button glyph, so taking your paddle back stays on the line.
  Without teams to pick, or with 2P, `@ TAKE YOUR PADDLE` as before. The
  call line already names the partner: `WITH MARGE VS GYM BROS`.
- **Unchanged:** the match setup (`match_config_partner` with the team,
  the court's difficulty, career and park points), the simulation, the
  save layout, the tuning export (`open_play.h` is not exported).

## Consequences

- The picked captain partners you even when their team is playing on
  another court at the time; for the length of your match the captain is
  in two places. Nobody minds (the park doesn't move during your match).
- A pick on a short court (CHAMP has two teams) can leave nobody to draw
  against you there; the court waits a changeover, as when nobody waits.
- Picking a team that is about to win against you only makes the game
  fresh: you still play the next game.

## Verification

- `test_open_play.c`: `test_pick_cycle` (only teams in the mask, wraps to
  next in line, nothing without teams, with 2P, at another rack, away, or
  with your paddle taken back; a pick leaving the mask resets),
  `test_pick_called` (144 calls: six seeds, every court, every pick; the
  partner is the pick, the opponent never their team; the pick first in
  line passed over; winners stay keeps it), `test_pick_vs_winners` (you
  first in line against winners who are the pick: a fresh game),
  `test_pick_soak` (30 minutes picking at random: 81 games, 73 with a
  pick, the population checked every frame).
- MesenCE, `test/smoke/pick_partner.timeline` with the pokes below on the
  release ROM (ACE on 14 points, having beaten SNOWBIRDS and GOTH CLUB:
  `beaten[]` is `g_save+95` to `+102`): at court 1's rack
  `@ TAKE  Y PARTNER: NEXT UP`, Y: `MARGE`, `RAVEN`, `NEXT UP`, `MARGE`;
  the call `WITH MARGE VS GYM BROS` (GYM BROS won and stayed; SNOWBIRDS,
  MARGE's team, lost and waits), and the match has MARGE (her look) on
  your side against GYM BROS (`g_match_config.partner` 1).

      python3 tools/mesen_probe.py probe release \
        test/smoke/pick_partner.timeline 2020 pick=g_open_play+73:1 \
        partner=g_open_play+59:1 opp=g_open_play+58:1 \
        mc_partner=g_match_config+28:1 \
        --poke "@60 g_save+80=1" --poke "@60 g_save+81=65" \
        --poke "@60 g_save+82=67" --poke "@60 g_save+83=69" \
        --poke "@60 g_save+84=32" ... --poke "@60 g_save+88=32" \
        --poke "@60 g_save+103=15" --poke "@60 g_save+208=0" \
        --poke "@60 g_save+96=2" --poke "@60 g_save+99=1" \
        --poke "@500 g_open_play+3=6" --poke "@500 g_open_play+4=6" \
        --poke "@500- once if g_match_config+27==1 g_game_state+6=10" \
        --shots "$PWD/build/smoke/pick_partner"

  (`--shots` wants an absolute path to an existing directory: MesenCE's
  working directory is not the repository's.)
- `make perf` zero overruns (`open_play`, `clubhouse`); golden trace and
  lockstep identical.
