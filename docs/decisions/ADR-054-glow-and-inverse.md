# ADR-054 — Glow and Inverse

Status: **Accepted 2026-10-16**
Milestone: `M4-MOD-03`. Extends: ADR-033, ADR-053 (modifiers). Depends on:
ADR-013 (palette roles), ADR-032 (save migration), ADR-045 (night courts).

## Why

More modifiers from GOOFY_IDEAS.md, with the user's picks: GLOW IN THE
DARK turns the players' black outlines (and the ball's) neon, changes the
paddles, and plays on the night court; INVERSE inverts the colours, like a
photo negative.

## Title page

| Row | Off | On | What it does |
|---|---|---|---|
| LIGHTS | NORMAL | GLOW | Neon outlines and paddles on a NIGHT court |
| COLORS | NORMAL | INVERSE | Every court and player colour inverted |

They combine with each other (an inverted glow match: magenta outlines on
a light court) and with every other modifier. The page box is eleven rows
now (rows 12-22), still clear of the prompt; a tenth modifier needs a
second page or column.

## Save (layout 13)

The modifiers byte was full (GLOW is its last bit, 128). INVERSE is the
first bit of a second byte, `SaveData.modifiers2` (and
`MatchConfig.modifiers2`, `MODIFIER2_*`), with a reserved byte after it to
keep the checksum at an even offset: 214 bytes.

- At the end of `SaveData`, after `camera`: `SaveSettings` and every
  older frozen layout are untouched. Layout 12 is frozen as `SaveV12`
  (212 bytes); its migration copies everything before its checksum and
  starts the second byte off.
- The editor takes both bytes: rows 0-7 are `modifiers`' bits, row 8 on
  `modifiers2`'s (`modifiers_row_on`). B saves if either changed; Y puts
  both back.

## GLOW

- **Outlines:** entry 1 of every player palette is the shared outline
  (ADR-013), so after a player's palette is composed (any look: custom,
  ladder captain, partner, career player) entry 1 becomes neon green,
  `RGB5(4, 31, 8)`. The objects palette's entry 1 is the ball's outline;
  the same green, in a work-RAM copy of `objects_palette`.
- **Paddles:** entries 11-13 (face dark, face light, edge) become purple,
  hot pink, and cyan.
- **Outfits** (the user's follow-up): the shirt, cap, and shorts entries
  (5-9) go neon (`modifiers_neon`): 6 brighter (to 31 at most), and each
  channel's distance below the brightest doubled, so a colour is more
  saturated and a ramp keeps its light and dark. Where doubling would
  push the darkest channel below 0, which turns the hue (an orange went
  red), it stretches by one and a half, or not at all. The shoe colour
  (10) is also the visors' and headbands' (`author_hats.py`), and a white
  one stayed white; it takes the neon shirt's mid colour, so every hat
  glows with its shirt.
- **Court:** composed at NIGHT (`RENDER_TIME`), whatever the COURT page's
  TIME, with the LINES preset LIME (`COURT_LINES_LIME`, generated from
  `court_colors.json`; the asset build fails without a LIME). The NIGHT
  ambience and the NIGHT crowd watcher too.
- No new palettes or tiles: the same OBJ palettes with other colours.
- The hit spark, the practice machine, and the hats share those palettes
  and take the green outline too.

## INVERSE

- Each colour is `c ^ 0x7FFF`: every channel 31 - itself. The SNES colour
  math only adds or subtracts, so it is done to the composed palettes, as
  the last step: the ground (after LAVA and TIME), the backdrop, the
  players, the objects, and the impact flash.
- **Not inverted:** the HUD (BG3) and the story card's bust, which stay
  readable as they are; and the shadows (a black shadow inverted would be
  a bright spot; they stay dark, averaged with the inverted court in
  singles).

## When it applies

- In a match: `RENDER_LOOK()` reads the match's bits, and 0 in the attract
  demo. Each composed palette records the look it was composed with
  (`s_ground_look`, `s_backdrop_look`, `s_objects_look`); a match with
  another look, or the title after one, recomposes it. The ground and
  backdrop go through the existing pending recolour (on a light frame).
  The objects' copy is made only when the look changes, so the demo's
  restart frame (KNOWN_ISSUES #29) is unchanged.
- No preview on the MODIFIERS page (LAVA has one): the title's demo plays
  without modifiers.

## Measured

- `make perf`: every timeline 0 overruns, including the two new ones
  (`modifiers_glow.timeline` worst 168 scanlines, `modifiers_inverse`
  190, which ends back on the title). `court.timeline` 216 (214 before:
  the ground's compose copies the court choices to swap in LIME).
- The golden trace and `make stats` (singles, doubles, net bot) are
  identical: presentation only.
- MesenCE: the GLOW page and match (green outlines on the players and the
  ball, pink paddles, the night park); the INVERSE doubles match; the title
  after it in its own colours.

## Tests

- `test_modifiers.c: test_values_and_editor`: the new rows and values,
  LIGHTS and COLORS as their bits, Up from BALL wraps to COLORS, a change
  to the second byte alone counts, Y puts both back.
  `test_select_copies`: Player Select copies the second byte (unknown bits
  cleared) and the demo clears it.
- `test_modifiers.c: test_neon`: the blue ramp stretches by one and a
  half and keeps its order, a saturated orange keeps its hue, a dull green
  doubles, greys stay grey.
- `test_court_style.c: test_glow_lines`: `COURT_LINES_LIME` is LIME.
- `test_save.c: test_migrate_v12`: a v12 save keeps its records,
  settings, initials, career players, and camera; the second byte starts
  off and then saves.
- `test/smoke/modifiers_glow.timeline`, `modifiers_inverse.timeline` (both
  in `make perf`).

## 816-tcc

`names[row][modifiers_row_on(...)]` (a call inside a two-dimensional
index) stops 816-tcc with "ICE 2"; the call goes in a local first.
