# ADR-064 — The Sprite Wish List in Play

Status: **Accepted 2026-10-17**
Milestone: `M4-ART-03`. From [IDEAS.md](../IDEAS.md), "Sprite wish list".
Extends: ADR-014 (accessories), ADR-048 (quips), ADR-059/060 (player sheet
slots).

## What

Every sprite on the wish list is in the game, as presentation only:

| Sprite | When |
|---|---|
| Celebrations | A point's winners: DAD and KIDDO a thumbs-up, the GYM BROS a double-biceps flex, the PIRATES en garde; everyone else the cheer. `opponents.json` `celebrate` per player. |
| Slump | A point's losers. |
| Dive, then scramble | The loser nearest the ball when it bounced twice past them, if they were running: 12 frames diving, 12 getting up, then the slump. |
| Emotes | With a quip, over the side that lost: `!` into the net, `?` out, sweat for two bounces, anger for a rules fault (kitchen, box, volley, lava). A star over the winners of a rally of 8+ shots; a note over the hitter of the third soft shot in a row; Z's over the receivers while a human server waits 5 s. |
| Bounce dust | Where the ball lands, 12 frames. |
| Net shiver | Where a ball hits the net, 16 frames. |
| Squash, stretch, spin streak | A drive squashes the ball at contact, then stretches it in flight; spin lines trail drives and slices until they bounce. Not with BIG BALL. |
| Venue critters | LAKESIDE's gull, SPACE's satellite (half speed), UNDERSEA's fish cross under the scoreboard, in screen space, every 2048 frames. |
| Trophy | Over CHAMPION! (beating the last rung). |
| Pickle | Practice: behind the ball machine, in OBJ palette 1 (slot 2's, free in practice) composed green. |

## How

- **`src/flair.c`** decides, from events (like `fx`, R9) and the game state;
  nothing in the simulation reads it, so play is unchanged. `src/render.c`
  draws it; `render_bodies` takes a slot's reaction frame from `rb_react`
  before its pose and state.
- **Art** comes from `assets/source/wishlist/` through
  `tools/art/import_wishlist.py` into the player sheet, now seven rows: the
  six body frames in slots 17-22; 8x8 and 16x16 objects in slots 23-25
  (`sheet_objects` in the manifest: [slot, cell, size]); the pickle a large
  object in 27. OBJ name table 2 is full (256 tiles). The objects palette
  gains a red (10, the anger mark) and a brown (15, the trophy's base).
- **OAM:** dust 20 and the net shiver 21 (the impact reserve's spares);
  emotes 38-41 by slot; the streak 42; the critter 43-46; the trophy
  47-50; the pickle 51. The net shiver draws at priority 3, over the net's
  high-priority tiles.

## Cost

The doubles budget (KNOWN_ISSUES #14, #22) and the new match's frame, the
game's heaviest, set the shape:

- Nothing runs while nothing is up. `FLAIR_TICK()` calls `flair_tick` only
  when `g_flair.active` (a timer, a counting emote, a dive in progress, a
  streak) or `wake` (a phase change, from `game_set_phase`; serve setup for
  the Z's; the celebration lookups) is set; render draws the event objects
  only when flair is active or something is still up.
- Objects are written while they show and parked once (Y = 240, as hats
  are) when they stop. Settled reactions and held emotes need no ticks. A
  frozen player's emote is rewritten only when it, its bob, or the camera
  changes. The reaction table is rebuilt only when `react_serial` moves.
  Critters move on even frames only.
- A first version overran the new match's frame (215 to 226 scanlines,
  three overruns in `golden.timeline`): its tick ran through the whole
  match result. Bisecting each addition found it; the rest cost nothing
  measurable there.

`make perf`: every timeline 0 overruns.

**Amended 2026-10-17** (KNOWN_ISSUES #34): merged with the ladder
rebalance and the quips, the rally's contact and bounce frames ran a few
scanlines over. Flair now draws nothing on an event's own frame (`fresh`:
the effect starts a frame later, unseen); the dust and net shiver are
projected once when they start; the ball keeps its single plain write and
`render_bodies` one test (`rb_react_on`) while flair is idle; one test
gates the event objects, the critter, and the trophy. A quip's emote is
set only on its own result, so a late pick by the staged HUD can't hold
it through the next serve.

## Tests

- `test_flair.c`: point reactions over 20 minutes of bot play (146 points,
  28 dives, each dive's three stages on time, nothing during play);
  celebrations by team; quip emotes by fault and their lifetime; the dink
  note, squash, dust, net timers; the Z's for a human server only.
- MesenCE screenshots: HENDERSON's thumbs-up with the long-rally stars,
  the losers' slump, a dive and its scramble (poked), the sweat emote, the
  dust, a drive's stretch and streak (poked), the net shiver (poked), the
  LAKESIDE gull flapping, the trophy, the practice pickle.
- Golden trace: identical from Player Select on; the title starts a frame
  later (the larger ROM), as with ADR-063.
