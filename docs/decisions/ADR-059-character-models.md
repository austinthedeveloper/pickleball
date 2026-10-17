# ADR-059 — Character Models

Status: **Accepted 2026-10-16**
Milestone: `M4-ART-01`. Amends: ADR-014 (accessories, the hats amendment),
M4-OPP-04 (hats). Fixes: KNOWN_ISSUES #32.

## Why

The player frames were placeholder art with a stubby paddle held at arm's
length, and every frame had the cap drawn in, so a player without a hat
(HAIR) wore a hair-shaped cap, brim and all (KNOWN_ISSUES #32). The hitting
work (IDEAS.md, "Hitting: reach, height, and overheads") waits on poses the
sheet didn't have.

## Decision

- **New art from the port kit.** The bodies come from the bareheaded
  32×32 set in `pickleball-port-kit/art/sprites/bare/`: same frame size,
  same slot order, same palette contract v2 (ADR-013), feet on row 29, and
  the same ponytail offsets, so nothing in the renderer's tables changes.
  The kit composes frames from shared parts (`src/compose.py`); the game
  imports its indexed strips with `tools/art/import_kit_sprites.py`, run by
  hand. It replaces `author_sprites.py` and `author_hats.py`, which would
  overwrite the new art.
- **Bareheaded bodies; the cap is a hat.** Hair is index 15 and the
  outline only, short and close to the skull. The cap is a 16×8 hat like
  the others, drawn by the same rule (accessory offset plus `hat_offset`).
  Over the bare head it reproduces the old capped frames pixel for pixel,
  except where the paddle grip (follow) or the fist (cheer) crosses the
  brim: the hat draws in front, so the brim covers them.
- **Hat ids are unchanged** (0 cap, 1-4 visor, headband, tricorn,
  bandana, 5 hair), so saves, `opponents.json`, and `career.json` keep
  their values. The manifest's `hats` now lists the cap first; `hair` is
  `null`, which draws nothing: the bare head is the hair look. The cap
  takes the HAIR hat's old `objects.png` cells (26-27); no new tiles.
- **The other hats** are redrawn by the kit for the bare head; they no
  longer have to hide a cap.
- **Four hitting poses** in OBJ name table 2 (R17), slots 12-15:
  `overhead_windup`, `overhead_contact`, `reach`, `scoop`, with ponytail
  offsets. Nothing draws them yet; the reach and overhead mechanics will
  add their selection rules in `render_bodies.asm`.

## Costs

- **Every player wears a hat object now** (unless bareheaded), so a match
  has up to four hats instead of three. The worst row at head height is
  bodies 16, hats 8, ponytails 4, effects 4, ball and shadow 2: **34 of
  34**. The OBJ stress scene draws four hats. The close-call replay
  (ADR-041) could reach 35 when four ponytailed, hatted players' heads and
  the replay ball share one row; the highest OAM index then loses a
  sliver on that line. Accepted: rare, one line.
- **CPU:** a hat is two `oam_write` calls where the cap was two parked Y
  bytes, about 3 scanlines a hat. Every timeline still has zero overruns,
  but the worst frames rose by up to 13 lines; the doubles title demo,
  four hats, from 210 to 222 of 225. If a later change needs the room, a
  hat writer that fills both halves in one call (one camera and bounds
  test, the second half copied from the first) is the first saving.
- **VRAM/ROM:** name table 2 holds 64 tiles (2 KiB) plus padding of table
  1's 7 free tiles. Table 1 is unchanged at 249 of 256.

## Not done

- Handedness under flip (KNOWN_ISSUES #7): a left-facing player is still a
  mirror, paddle in the other hand. Drawn left-facing frames would take 11+
  of name table 2's 12 remaining slots.
- The HD 64×64 kit set: four objects per player don't fit the scanline
  budget.

## Verification

- `make check`: passes, no warnings. The asset build checks that the cap
  is hat 0 and that every frame (the new poses too) has its feet on row 29.
- `make golden-compare`: identical (art only).
- `make perf`: zero overruns on every timeline. Worst frames before → after:
  demo 210 → 222, open play 220 → 220, court 217 → 215, modifiers inverse
  202 → 215, modifiers doubles 199 → 206, doubles ladder 182 → 191.
- Screenshots (debug ROM): the doubles ladder match with `rb_hat` poked to
  the cap, visor, bare head, and tricorn, both facing directions and with
  ponytails; Player Select (thumbnails in caps); the OBJ stress scene draws
  four hats on the head row, 34/34, with no overflow.
