# ADR-047 — Career Players

Status: **Accepted 2026-10-15** (stages 1-2: the data, the page)
Milestone: `M4-CAREER-01` to `-04`. From [IDEAS.md](../IDEAS.md), "Career
mode". Depends on: ADR-021/032 (save and migration), ADR-013/014 (palette
parts, hats), ADR-020 (Player Select), ADR-023 (the ladder), ADR-040
(initials entry).

## The idea

Create a player, name them, and play as them: they keep lifetime stats,
and playing unlocks cosmetics (hats, hair colours). Gear that changes
stats comes later (stage 4), with its own balance pass.

## Decisions

- **Three save slots** (a family or friends sharing the cart). One is the
  active player, who is 1P.
- **The title's RECORDS entry becomes CAREER**: the active player's card
  (name, look, record, stats, unlocks), switching or creating players, and
  the old records table as a section. The title menu is full, so no new
  entry.
- **Unlocks are cosmetic** for now; skin tones are never locked.
- Stats count **matches against the CPU** (no 2P on the other team),
  outside the attract demo and practice, like the records (ADR-021).

## Stages

1. **Data** (this branch): the player record, save layout 12, stats from
   points and matches, unlock rules, tests. No screens yet.
2. **The CAREER page** (`M4-CAREER-02`): slot picker, player card, and the
   editor (name with the initials entry of ADR-040 widened to 8 letters;
   body, skin, hair, shirt, hat, paddle; locked items shown locked).
3. **Into matches** (`M4-CAREER-03`): 1P's look and paddle from the active
   player (Player Select starts on them), the name on the HUD, and
   "UNLOCKED: VISOR!" on the result screen (`g_career_new_unlocks`).
4. **Gear stats** (`M4-CAREER-04`): small trade-offs like the paddles'.

## The CAREER page (stage 2)

The title's RECORDS entry reads CAREER and opens a page with four views
(`src/career_page.c`, pure input logic like the court and options
editors; `hud_scene.c` draws, `main.c` writes the save and previews):

- **SLOTS**: the three players (name, yellow for the active one, and
  total wins) or NEW PLAYER, then RECORDS. B opens a player, which makes
  them active (a save write), or creates one and opens EDIT on the name.
- **CARD**: SINGLES and DOUBLES W/L, aces, shutouts, best streak, longest
  rally, UNLOCKS n/9, and the next unlock with its hint (`how` in
  career.json, 18 cells), or ALL UNLOCKED!. B edits; L+R+X deletes (the
  records erase's guard); Y back.
- **EDIT**: NAME, BODY, SKIN, HAIR, SHIRT, HAT, PADDLE. B on NAME types
  it (Up/Down letters A-Z and space, held to repeat; B/Right next; Y/Left
  back; Start done; the letter at the cursor is yellow, a space shows as
  `_`; a name of spaces becomes A). Left/Right change a look; locked hair
  and hats are skipped. B elsewhere keeps (a save write); Y restores the
  look, or drops a player just created.
- **RECORDS**: the old table; L+R+X erases.

Each view has its own prompt: title keys carry a 4-bit prompt id (was the
3-bit page), ids 8-12 for the CAREER views. The views draw a part (one or
two rows) per frame and then redraw only changed rows, like the others.

**The preview:** while a player's CARD or EDIT is open, the title demo's
P1 wears their look: `MatchConfig.career_p1` makes slot 0's palette
(`career_compose_palette`: the career skin ramp and hair, the shirt's team
colours, the paddle) and hat come from the active player, and
`render_apply_p1_look` recomposes slot 0 alone. It is re-applied if the
demo restarts under the page. Stage 3 uses the same flag in matches.

## The player record

`CareerPlayer` (`save.h`), 52 bytes: fourteen u16 counters first, then the
bytes, an even size, so the layout is the same on the host and the SNES
(816-tcc does not pad):

- wins and losses by mode, points won and lost, aces, shutouts, the
  current and best win streak, longest rally, ladder clears, open play
  games, and the unlocked mask (u16);
- used, the name (8 letters, space-padded), variant, skin, hair, shirt
  (a PAL_* team colour: 1P's palette must stay unique, ADR-010), hat,
  paddle, and wins over each ladder team (u8 each, saturating).

An **ace** is a point 1P's team won on 1P's serve with one shot in the
rally. A **ladder clear** is a win over the last ladder team on the
ladder (not at the park).

## Unlocks

`assets/source/career.json` lists the skins (opponents.json's four, in
order), the hair colours, the hats (in HAT id order), and the unlocks
(bit n of the mask) with a rule and a value: WINS, RALLY, ACES, SHUTOUTS,
STREAK, BEAT (a ladder team, by name), CLEARS, OPEN. A hair colour or hat
names the unlock that frees it. The asset build checks names, rules,
teams, hat order, and that the first hair colour and the cap are free,
and writes `career_data.h`.

| Unlock | Rule |
|---|---|
| VISOR | win a match |
| HEADBAND | win 10 |
| BANDANA | a 20-shot rally |
| TRICORN | beat the PIRATES |
| BLUE HAIR | 5 aces |
| PINK HAIR | a shutout |
| GREEN HAIR | 5 open play games |
| PURPLE HAIR | a 5-win streak |
| GOLD HAIR | clear the ladder |

Unlocks are checked after every finished match;
`g_career_new_unlocks` (WRAM) holds the ones just earned.

## Save (layout 12)

`SaveData` gains `career[3]` and `career_active` (CAREER_NONE: nobody)
after the initials: 212 bytes. Layout 11 (v0.15.0) is frozen as `SaveV11`
and migrates whole with no players (its kitchen colours are already
current, so the layout-10 remap skips it). The match hooks record into
`g_save` and `save_record_match`'s write keeps both, one write a match.

### Cost

Every save write sums the block, which grew four times. The 816-tcc sum
loop made save writes (leaving a page, a match end) overrun a frame in
eight `make perf` runs; the sum is now `src/asm/save_sum.asm` (host
builds keep the C). Boot reads the bigger block, which shifts the title
demo slightly: heavy demo frames then met COURT page recolours (and a
LAVA one), so a recolour now waits for scanline 96 (was 110), up to 6
frames (was 3). Zero overruns in every run.

## Tests

`test/test_career.c`: slots (create, name cut and padding, taken and
missing slots, delete, select, a save round trip); points and matches
(aces, rallies, streaks, shutouts, team wins, ladder clears on the ladder
only, nothing without a player); unlocks (free items, first win, earned
once, several at once, a loss earns nothing); layout 11 migrating with
no players. MesenCE: a save written and read back across a reboot
(layout 12, the court choices kept).
