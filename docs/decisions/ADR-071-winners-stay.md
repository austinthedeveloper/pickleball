# ADR-071 — Winners Stay On

Status: **Accepted 2026-10-18**
Task: `M4-OPEN-07`. Amends: ADR-034 (open play).

## Why

At a real park the winners of a game stay on and the losers walk off: the
next pair in the stack challenges the winners. The park (ADR-034) took all
four off after every game, so a win there meant nothing more than a loss
until park points (ADR-070). Staying on after a win is the reward real
parks give, and it gives your games a run of their own. Parks cap the run
so the court keeps turning over; two wins is the common cap.

## Decision

- **A court's game** (the park's own, first to 7, a random side each
  point) ends with a winner: the side that reached `OPEN_GAME_POINTS`.
  `OpenCourt.stay` records that side and `OpenCourt.streak` how many games
  in a row it has won there (`court_won`: a new side starts at 1).
- **The changeover** (`court_start_game`): with a side staying, the losers
  join a stack (`stack_join`, as before) and one entry comes on against
  the winners (`next_entry`), at the other side; the winners keep theirs.
  Winners who have won `OPEN_STAY_MAX` (2) in a row come off too, and the
  game is a fresh one: the two off join stacks and the next two go on, as
  before. The losers join before the challenger is drawn, as the two off
  did before, so a court whose stack is short keeps its own level playing
  (CHAMP, with two teams, plays the same pair until the cap).
- **Nobody to come on:** nobody stays. The winners join their stack and
  the court waits for the next changeover (the old not-enough-players
  path). With eight teams someone always waits, so this needs a hand-made
  state (the host test).
- **Your game** (`open_play_return`): the side that won goes on the
  court's streak. Won, below the cap, you stay on (`OpenPlay.stay_court`)
  and the changeover calls you again (`OPEN_EVENT_CALLED`, the same banner
  and match). Lost, or at the cap, you come off as before; a team that
  beat you stays, like any winner.
- **The same partner:** staying on, you keep `OpenPlay.partner` (the
  captain of the team waiting next when you were first called). Their
  team stays in line: the challenger is the first entry in the stack that
  is not your partner's team (`stack_take`; the other courts' fronts are
  passed over when they are it). If only your partner's team waits, you
  come off. If another court took your partner's team during the
  changeover, the partner is chosen afresh, as for a new game. With 2P as
  your partner there is no team to pass over.
- **B at a rack while you wait to be called again** gives up your place:
  you come off, the court's next game is a fresh one, and the rack does
  what it always does (stacks your paddle).
- **The HUD** (BG3 text, the staged panel; 28 characters at most):
  - Called again: `WINNERS STAY: VS SNOWBIRDS` (26 at the longest team
    name), with `YOUR GAME!` as usual.
  - Back from a normal win: `YOU WON 11-4  16 PTS`, as before (it shows
    through the changeover, then the call replaces it).
  - Off at the cap: `YOU WON 11-4  2 IN A ROW` for the first half of the
    result's four seconds, then `YOU WON 11-4  16 PTS`; unrated, only the
    first (`YOU WON 23-21  2 IN A ROW` is 25).
- **Unchanged:** the match (`MatchConfig.open_play`, the court's
  difficulty, career and park points), the simulation, the save layout,
  the tuning export (`open_play.h` is not exported).

## Consequences

- A game at the park now usually puts on one new team, not two, so you
  wait about as many games but see the winners again. The first game
  after you stack may let the winners stay and you wait one more game.
- The changeover animation still walks all four off and the new four on;
  the winners walk off and back on with the challengers. Nobody minds.
- The courts don't show a streak: the score sprites are five characters
  and every OBJ palette is in use; the call line says it when it's you.

## Verification

- `test_open_play.c`: `test_winners_stay` (the winners keep their side,
  the losers go to the back, the cap sends all four off, another side's
  win restarts the streak), `test_you_stay` (called again on the same
  side with the same partner and an opponent who is neither your
  partner's team nor the team you beat; the cap sends you off; 2P),
  `test_you_lose` (you off, the winners stay; B at a rack gives up the
  stay), `test_stay_nobody_waiting` (the winners off, the court waits),
  `test_stay_soak` (30 minutes with you stacking and winning or losing at
  random: 84 games, 25 stays, 15 capped, the population checked every
  frame). `test_your_turn` now loses its game to come off.
- MesenCE, `test/smoke/winners_stay.timeline` with the pokes below on the
  release ROM: court 1's first game ends and its winners (SNOWBIRDS) stay;
  they win the next too, reach the cap, and the fresh game calls you,
  `WITH BRAD VS HENDERSON` (BRAD is GYM BROS's captain); you win 11-0,
  `YOU WON 11-0  16 PTS`, and the changeover calls you again, `WINNERS
  STAY: VS SNOWBIRDS` (GYM BROS passed over at the front of the stack, the
  beaten HENDERSON at its back), with BRAD again; you win again, `YOU WON
  11-0  2 IN A ROW`, then `YOU WON 11-0  16 PTS`, and the court's next
  game is a fresh one.

      python3 tools/mesen_probe.py probe release \
        test/smoke/winners_stay.timeline 3900 stay=g_open_play+71:2 \
        --poke "@60 g_save+80=1" --poke "@60 g_save+81=65" \
        --poke "@60 g_save+82=67" --poke "@60 g_save+83=69" \
        --poke "@60 g_save+84=32" ... --poke "@60 g_save+88=32" \
        --poke "@60 g_save+103=15" --poke "@60 g_save+208=0" \
        --poke "@1600 g_open_play+3=6" --poke "@1600 g_open_play+4=6" \
        --poke "@2100- once if g_match_config+27==1 g_game_state+6=10" \
        --poke "@3000- once if g_match_config+27==1 g_game_state+6=10" \
        --shots build/smoke/winners_stay

  The run has one overrun, entering a match's result screen; the base
  commit has it too. It is the park rating's (KNOWN_ISSUES #41, fixed on
  `feature/cld-park-overrun`), not this ADR's.
- `make perf` zero overruns (`open_play`, `clubhouse`); golden trace and
  lockstep identical.
