# Ideas

Candidate features that have not been planned yet. When one is picked up, it
gets an ADR in [decisions/](decisions/) and this entry links to it.

Budget snapshot (2026-10-13, release build of `d8b3dd5`): the ROM is 256 KiB
(8 LoROM banks); about 115 KiB is used and 140 KiB is free (banks 3–7). Per-screen
limits are in [09_M1_VRAM_AND_BUILD.md](09_M1_VRAM_AND_BUILD.md).

## Locations

A location swaps the **backdrop** (the top of the screen: sky, trees, fence,
bleachers, sign) and **recolours the ground**. The court geometry, lines, and
projection stay the same, so gameplay and collision are untouched.

How `court_scene.png` splits today:

| Part | Tile rows | Unique tiles | BG1 palettes |
|---|---|---:|---|
| Backdrop | 0–9 | 307 | 3–7 |
| Ground (court, kitchen, surround) | 10–27 | 329 | almost all 2; a few edge tiles use 4–5 |

The two parts share no tiles, so a location is:

- **Backdrop:** its own tiles (≤ 567, the 896-tile screen budget minus the
  ground) plus 320 map entries and palettes 3–7. About 10 KiB at today's
  detail.
- **Ground:** palette 2 only, 32 bytes. Same tiles, new colours (clay, blue
  hard court, green, night-lit).

That's about 10 KiB per location, so free ROM fits about 12 of them.

Constraints to resolve:

- The few ground-edge tiles on palettes 4–5 would also change colour with the
  backdrop. Either move them to palette 2 or check them against each backdrop.
- Palette 2 holds the line colours (entries 6–7, checked by the asset build).
  Any ground recolour must keep the lines readable.
- Loading happens in forced blank, the same way Player Select already swaps
  the court out and back.

Starter list: Riverside Park (current), Beach Boardwalk, Indoor Rec Center,
Rooftop at Night, Pirate Dock, Gothic Courtyard, Suburban Driveway, Championship
Stadium. Each story opponent can have a home court (see below).

### Custom court colours

A config screen where the player picks the ground colours (surface, kitchen,
outer area; maybe the lines), and the choice is used for later matches.

- Small: a few colour values in palette 2. SNES colours are 5 bits per channel
  (32 levels each of R, G, B), so a picker can be three sliders, or a grid of
  preset swatches, which is simpler and harder to make ugly.
- Guardrail: refuse or adjust a choice whose lines don't contrast with the
  surface, so the ball and lines stay visible.
- **Remembering it across power-off needs battery save** (below). Without it,
  the choice lasts until the console is turned off.

## Story mode: CPU personalities

A ladder of themed opponents. Each one is **data, not new code**: a look, a
home court, and a set of play-style numbers fed into the existing CPU.

### What the CPU can vary today

These knobs already exist in [src/ai.c](../src/ai.c) as fixed constants
(ADR-019). Per-opponent values would come from a table:

| Knob | Today | Personality use |
|---|---|---|
| Dink frequency | every 4th shot | Soft-game players dink constantly; bangers never |
| Lob trigger | opponent within 44 px of the net | Lob-happy players lob from farther out |
| Put-away appetite | drive if ball ≥ 14 px high and CPU within 48 px of net | Aggressive players drive lower balls from farther back |
| Mistake rate | mistimes every 7th shot | Main difficulty dial |
| Aim pattern | away from the opponent; alternating depth | Body shots, always deep, always cross-court, random |
| Paddle stats | POWER / CONTROL / SPEED (ADR-020) | Already per-player |

New knobs worth adding: **home position** (net rusher vs baseline camper) and
**reaction delay** (frames before moving to the predicted landing spot).

### Look

Cheap: palette swaps of the shared body (palette parts, ADR-020) plus one
accessory sprite (the ponytail slot, ADR-014): hats, hair, bandanas. Plus a
select-style portrait (about 3.5 KiB each). New body animations are the
expensive option: the OBJ tile budget is 208 of 256.

### Roster draft

Roughly easiest to hardest. Doubles teams are marked.

| # | Opponent | Look | Style | Home court |
|---|---|---|---|---|
| 1 | **The Hendersons** (family, doubles) | Matching vacation tees, dad visor, kid cap | Parent is steady and slow; kid is fast but makes lots of mistakes. Easy warm-up. | Suburban Driveway |
| 2 | **Gym Bro Brad** | Tank top, headband | Drives everything, even low balls. High mistake rate; lob over him. | Indoor Rec Center |
| 3 | **The Snowbirds** (retirees, doubles) | White visors, pastel polos | Slow but accurate; lob specialists who punish anyone at the net. | Retirement community courts |
| 4 | **Salty Pete & Bilge** (pirates, doubles) | Tricorn hat, bandana, striped shirts | Bangers aiming at your body; rarely dink. Drives on anything above knee height. | Pirate Dock |
| 5 | **Raven** (goth) | All black, purple hair, pale palette | Soft-game master: dinks and resets, never speeds up first. Few mistakes, low power. | Gothic Courtyard (night) |
| 6 | **Kai** (surfer) | Board shorts, bleached hair | Fast and loose: chases everything, random aim, medium mistakes. | Beach Boardwalk |
| 7 | **The Twins** (doubles) | Identical outfits, mirrored palettes | Stack at the net together and poach. Tests the doubles lane logic. | Rooftop at Night |
| 8 | **The Champion** | Gold trim, sponsor patches | Mixes every shot well; lowest mistake rate. | Championship Stadium |

Other themes to keep in the pocket: ninja, wizard, robot, mime, cowboy,
food-truck chef.

### Story wrapper

- Intro card per opponent (portrait, name, one line of trash talk), then the
  match, then a win/lose card.
- Progress (which rung you've reached) needs saving: battery save (below),
  a password screen, or a short ladder played in one sitting.

## Play modifiers

Toggles that change the rules or physics for a match, e.g. big ball, fast
ball, wind, first to 5 or 21, rally scoring, "kitchen is lava", or
no-volley-anywhere.

- Mostly code and a few constants, so the ROM cost is small.
- Needs a modifiers menu (could live on Player Select or a new options screen)
  and room in `MatchConfig`.
- Some modifiers interact with the rules engine and its tests; each one needs a
  test case.
- Could double as story flavour: a pirate match with wind, a beach match with a
  fast ball.

## Battery save (SRAM)

**Started:** [ADR-021](decisions/ADR-021-battery-save.md) adds the SRAM, the
save block, and records (no initials entry yet). The notes below are the
original idea.

Add battery-backed save RAM so the game can remember things after power-off.
Today the cart has none (ADR-017: "no SRAM").

### What it unlocks

- Story progress and unlocked opponents and courts.
- Custom court colours.
- Records: high scores, win streak vs CPU, longest rally, fastest win, and
  per-opponent wins and losses.
- Settings (default rules, modifiers, difficulty).
- Initials entry for the records, arcade style.

### Cost

- **Build:** two Makefile settings that the generated `hdr.asm` already
  supports: `CARTRIDGETYPE := 02` (ROM + RAM + battery) and `SRAMSIZE := 01`
  (2 KiB; `03` = 8 KiB). PVSnesLib has `consoleCopySram` and `consoleLoadSram`.
  2 KiB is plenty for everything above.
- **ROM space:** negligible.
- **Emulators and flash carts:** save to an `.srm` file automatically.
- **Physical carts:** need a board with an SRAM chip and a coin battery.
  That costs a little more, and the battery lasts about 10–20+ years.

### Design notes

- Start the save block with a magic value and version number, followed by a
  checksum. A blank, corrupted, or older-format save gets reset to defaults
  instead of crashing.
- Keep two copies and alternate writes, so powering off mid-save loses at most
  the last change.
- Write only at safe points (match end, leaving a menu), never during play.
- Add a "clear save data" option (e.g. hold a button combo on the title screen).
- Tests: round-trip the save struct and corrupt-checksum recovery in the host
  tests; smoke-test with Mesen's `.srm`.
