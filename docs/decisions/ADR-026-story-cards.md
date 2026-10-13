# ADR-026 — Story Cards

Status: **Accepted 2026-10-13**
Starts the story wrapper from ADR-023 (`M4-STORY-01`, `M4-STORY-02`).
Depends on: ADR-021 (save), ADR-023 (opponents), ADR-025 (options).

## Cards

There is no portrait art yet, so the cards use the HUD font over the live
court, where the opponent already stands in their own colours.

- **Intro card:** shown when a ladder match starts from Player Select. A
  rematch or a 2P match has no card.
  - The opponent's name in big text, in the team colour. Singles shows the
    captain; doubles shows the team.
  - Their intro line in a panel, then `LADDER n OF 8`, then the prompt
    `@ PLAY`.
  - The match waits underneath: no simulation and no AI. B, A, or Start
    ends the card after half a second, and it ends by itself after 5 s. The
    frame that closes the card keeps its input from the match, because the
    serve buffers early presses and the closing B would otherwise serve.
- **Match result**, under `YOU WIN!` / `CPU WINS`:
  - The opponent's line in their colour: the taunt if they won, the
    concession if you did.
  - After a win, `NEXT UP: <team>`.
- **Ladder end:** beating CHAMPS shows `CHAMPION!` instead of `YOU WIN!`,
  then `LADDER CLEARED!`. It records a **ladder clear** (a new record,
  SaveData v5, on the records page), and the ladder starts over.

Lines live in `opponents.json` (`intro`, `taunt`, `concede`). The build
checks that each is 1–28 characters from the HUD font (capitals, digits,
space, and `.,!?'-:`), with no `@`, which draws the B button.

## Frame budget

`hud_center_prompt` drew its panel border tile by tile in C. 816-tcc spent
about 60 scanlines on a wide panel, and two panels in one frame (the intro
line plus the prompt) overran.

- `hud_panel` now fills its rows with the assembly fill (`hud_fill_box`).
  This speeds up every prompt in the game.
- The cards draw one panel per frame, using two extra HUD steps.

Results:

- Zero overruns on every timeline: golden with cards, idle, records, court,
  venue, options, the 3,000-frame soak, and **doubles**. Doubles dropped a
  frame before (`M4-UI-02`).

## Tests

- `test/test_story.c` (host):
  - the intro shows only with an opponent, and never in attract
  - B can't skip it before the minimum
  - it ends by itself
  - ladder clears are recorded and saved
  - every line fits
- `test/smoke/story.timeline`: DAD's intro card, then B dismisses it without
  serving.
- The golden timeline now opens on the intro card. Its first B press closes
  the card, and traces were re-recorded.
