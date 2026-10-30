# ADR-078 — Athletic Player Models

Status: **Accepted 2026-10-30**
Milestone: `M4-ART-05`. Amends: ADR-059 (character models), ADR-060 (hats),
ADR-064 (the wish list's body frames).

## Why

The in-game players read as "E.T.": about 2.5 heads tall, the head nearly
as wide as the shoulders, no neck, short limbs. The Player Select
portraits and the court mockup (`assets/inspiration/level-01`) are about
four heads tall; the sprites should match them.

## Decision

- **New bodies, same contract.** All 21 body frames are redrawn at about
  four heads tall: a 7×6 head with a one-pixel eye, a visible neck,
  shoulders wider than the head. Same 32×32 frame, palette contract v2
  (ADR-013), feet on row 29, slot order, and one object per body, so
  nothing in the renderer changes.
- **Source of truth is `art/characters/`.** The brief is
  `art/CHARACTER_ART_PROMPT.md`. The image generator's renders of nine
  frames (`frames/*.png`, kept for reference) were reduced to the 32×32
  grid and the palette; the other twelve poses, follow's arms, the
  ponytail, and every hat were drawn by hand on the same head. The art is
  text grids in `art/characters/sprites/` (one hex role per pixel), easy
  to diff and to edit.
- **One head everywhere.** Every frame draws the same head, and
  `sprites/heads.json` records where it is. `tools/art/import_character_art.py`
  (run by hand) writes the frames, hats, and ponytail into
  `player_sprites.png` and `objects.png` and derives each frame's
  accessory offset (head left − 5, head top + 1) and `hat_offset` (1, −5)
  from it.
- **Hats and ponytail redrawn** for the smaller head: same twelve hats,
  same ids, cells, and colour roles.
- **Retired importers.** `import_kit_sprites.py` and the body frames of
  `import_wishlist.py` would put the old art back; the kit importer now
  refuses to run without `--force`, and the wish list importer no longer
  writes body frames.

## Costs

- None at run time: same tiles, objects, and tables. The hat can now sit
  one row above the frame's top in the tallest poses (hat y = accessory
  y − 5), which the renderer already handles as a signed offset.
- The port kit's bare set no longer matches the SNES art; the Godot port
  draws its own HD players, so nothing reads it.

## Not done

- Handedness under flip (KNOWN_ISSUES #7) is unchanged.
- The idle bob is two rows (the generated art's), not one.

## Verification

- `make check`: passes, no warnings (feet on row 29 in every frame).
- To do on the Mac: `make perf`, `make golden-compare` (art only, should
  be identical), and screenshots of a doubles match with hats and
  ponytails both facing ways, Player Select thumbnails, and the museum.
