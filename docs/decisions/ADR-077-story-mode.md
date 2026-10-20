# ADR-077 — Story Mode: Save the Park

Status: **Accepted 2026-10-19**
Task: `M4-STORY-04`. Extends: ADR-026 (story cards), ADR-023 (the
ladder). Amends: ADR-065 (the title menu, eleven entries), ADR-032 (save
layout 15).

## Why

The ladder already has a story's bones: eight teams in order, an intro
card, a taunt or a concession after the match, and an ending. What it
doesn't have is a reason to climb. The first story mode is the ladder
with a plot wrapped around it: a scene before each match, a partner who
is in it with you, and an ending.

## Decision

**The plot.** Mr. Grimsby of Grimsby Parking Co. is going to pave the
park for a 400-space parking lot, unless someone beats the Champs. The
Champs only play the best, so you and **PEPPER**, a park regular, take on
the other seven teams first, and each team you beat signs the petition.
Along the way, the Gym Bros turn out to be on Grimsby's payroll, and the
Twins are his niece and nephew. The Champs play with the park as the bet,
and beating them saves it. Grimsby is never on screen: he speaks on scene
pages, with no sprite. Dale Dawson stays out of the ROM (the bible, §3).

**How it plays.**

- **STORY** is the first title entry, above SINGLES. The cursor still
  starts on SINGLES, so the title's Start goes where it always did.
- Player Select opens for 1P in doubles; you pick a colour and a paddle.
  Slot 1 is PEPPER, a CPU with the human partner's style
  (`g_ai_partner_style`) and her own look: a backwards cap, red hair, a
  lime shirt, and the Gold paddle. 2P can't join; the story is 1P.
- **A chapter is a ladder team**, in ladder order, played on that team's
  home court whatever the COURT setting. It opens on its **scene**: an
  automatic `CHAPTER n` page with the chapter's title, then four to six
  pages. Each page has the speaker's name in big letters and one or two
  lines. Then comes the normal intro card, with the captain's bust, its
  line, and `CHAPTER n OF 8`, and then the match.
- A page turns on B or A after 20 frames (so a held B can't race
  through), and Start skips the rest of the scene. Nothing turns by
  itself: people read at different speeds.
- **Win** and the result says `NEXT UP: <the next chapter's team>`. B
  goes on to that chapter. **Lose** and B is a rematch, as on the ladder.
  **Quit** from the pause menu and the chapter is kept. That's unlike the
  ladder, which quitting resets: a story is meant to be put down and
  picked up.
- **Beating the Champs** shows `CHAMPION!` and `PARK SAVED!` (not LADDER
  CLEARED!). B plays the ending over the result screen, closing on
  `THE END` and `@ FINISH`, then returns to the title. The story starts
  over at chapter 1.
- **Records and career** count a story match as a doubles match against
  the CPU, like any other: wins, streaks, and `beaten[]`. Beating a team
  in the story lets you pick its captain at the park (ADR-072). It isn't
  a ladder clear: neither `records.ladder_clears` nor a career player's
  moves, and the ladders' rungs don't move either.

## Data

`assets/source/story.json` holds the partner (the look schema of
`opponents.json`'s players), the villain's name, eight chapters (a title
and pages) and the ending. A page is `who` plus one or two lines.
`who` is `narrator` (no name), `partner`, `captain`, `mate` (the team's
second player), or `villain`. The asset build (`tools/assetgen/story.py`
writes `story_data.h`) checks:

- one chapter per ladder team;
- every line is 1-28 HUD characters, and a page has one or two lines;
- the partner's name is 1-9 cells and no ladder player's, and her hat,
  paddle, and celebration exist;
- at most 255 pages (53 today).

## Code

- `MatchConfig.story`, and `partner` = `MATCH_PARTNER_STORY` (0xfe).
  `match_config_story(chapter)` applies both after Player Select.
  `match.c` gives slot 1 the partner's palette, hat, and celebration.
- `SaveData.story_chapter` plus a reserved byte: **layout 15**. Layout 14
  is frozen as `SaveV14` and migrates with everything kept, its doubles
  rung included, and the chapter at 0. The old "copy the singles rung to
  doubles" step now runs only for layouts before 14; it would otherwise
  have reset a v14 save's doubles ladder. `save_story_chapter` and
  `save_advance_story` (1 when the park is saved).
- `story.c`: `STORY_CARD_SCENE`. `story_begin_intro` opens a story
  chapter's scene, and the scene opens the intro card when it ends.
  `story_begin_ending` plays the ending, which ends in
  `game_start_attract`. `g_story.cleared` is set by the win that saves
  the park, and `start_next_opponent` (`main.c`) plays the ending instead
  of a next match.
- `hud_scene.c`: `HUD_KEY_SCENE | page`, so each page is a staged
  redraw: the name, line 1, line 2, then the prompt, a panel a frame. Its
  clear runs down to the banner, over the match result too.
- `render.c`: a chapter's court loads the bust under the scene with BG2
  off, and turns it on when the intro card opens.
- **The title menu** (amends ADR-065): eleven entries, rows 11-21, the
  join line on row 22. The settings pages and RECORDS start a row
  higher, and the title area is 12 rows.
- `tools/mesen_probe.py` records 32 bytes of `MatchConfig` for the
  lockstep, and `tools/lockstep.c` now also refuses a mismatched match
  config size (ADR-076 added the intent check).
- No simulation or tuning value changed. The tuning export gains
  `MATCH_PARTNER_STORY`; version 5.

## Tests

- `test_story.c: test_story_mode`: the config and the partner's look and
  style; every chapter opens on CHAPTER and the ending closes on THE END,
  and every named page has a speaker. It reads chapter 1's scene page by
  page into the intro card, and Start skips a scene. A win moves the
  story and not the ladders, and quitting keeps it. The last chapter
  saves the park with no ladder clear, and the ending returns to the
  title.
- `test_save.c: test_migrate_v14`: a v14 save keeps everything (the
  doubles rung too) and starts the story at 0; the chapter is kept after.
- `test/smoke/story_mode.timeline`: STORY on the title, Player Select,
  chapter 1's pages, and the intro card. Zero overruns in `make perf`.
  MesenCE frames of the title, CHAPTER 1, Grimsby, PEPPER, DAD, and the
  intro card.
- On the ROM, with a poked chapter 8 and a held 4-0 score: the win, then
  the initials card (records beaten), then the ending over the result
  (THE END, @ FINISH), then the title with the story back at chapter 1.
- Eleven timelines that wrap Up past SINGLES, or go Down from MODIFIERS,
  press once more for STORY. Probes of attract, mode, opponent,
  modifiers, and the title page every 100 frames match main's ROM on
  the old timelines in all fourteen menu timelines. Golden trace and
  lockstep are identical.

## Later

- A story partner for 2P, and the partner's own bust on her pages. The
  match leaves one BG palette free, the captain's (ADR-026).
- A cleared-story mark on the records page, and lines that change on a
  second time through.
