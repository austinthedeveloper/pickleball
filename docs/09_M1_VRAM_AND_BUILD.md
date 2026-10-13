# VRAM, CGRAM, OAM, and Build Record

Originally the M1 record; updated 2026-10-12 for the M2 art and the
[mockup decisions](10_MOCKUP_DECISIONS.md). PVSnesLib APIs express VRAM
addresses in words; byte ranges make overlap checks explicit. Static assets
are uploaded once during forced blank.

## VRAM

| Region | Word address | Reserved byte range | Actual bytes |
|---|---:|---:|---:|
| BG1 scene map | `0x0000` | `0x0000–0x07ff` | 2,048 |
| BG3 HUD map | `0x0400` | `0x0800–0x0fff` | 2,048 (rows 0–27 used) |
| BG1 scene tiles | `0x1000` | `0x2000–0x8fff` | 20,000 (625 tiles) |
| BG3 HUD font, panels, big glyphs | `0x5000` | `0xa000–0xbfff` | 4,480 (280 tiles) |
| OBJ tiles (name base) | `0x6000` | `0xc000–0xffff` | 6,656 (208 tiles) |

Regions do not overlap. `assets/generated/sizes.json` records every generated
file's size, hash, and the tile counts above on each build.

**Open budget item.** ADR-016 plans 16 KiB (512 tiles) of BG1 character data.
The mock-derived scene needs 625 unique tiles (19.5 KiB) after flip-aware
deduplication. VRAM has room for it, so nothing overlaps, but the plan and
the art disagree. The options are to raise the BG1 budget, to move scenery to
BG2 (which saves no VRAM), or to merge near-duplicate scene tiles.

## CGRAM

| Entries | Use |
|---:|---|
| 0–15 | BG3 HUD palettes 0–3 (ADR-016): 0 neutral, 1 title gold / B button, 2 left-team label, 3 right-team label. Palettes 2–3 are rewritten from the match config. |
| 16–31 | Unused (BG3 palettes 4–7 / BG 4bpp palette 1) |
| 32–127 | BG1 palettes 2–7. BG1 never references 4bpp palette 0; the asset build fails if it does. |
| 128–143 | OBJ 0: slot 0 player, composed at match start (ADR-013) |
| 144–159 | OBJ 1: slot 2 player, composed at match start |
| 160–175 | OBJ 2: ball, spark, debug marker |
| 176–191 | OBJ 3: hit flash |
| 192–207 | OBJ 4: shadows (black, colour math) |
| 208–255 | OBJ 5–7: unused |

Shadows use colour math: `CGWSEL = $02` (add subscreen), `CGADSUB = $50`
(half, OBJ). BG1 is also on the subscreen, so a black OBJ-palette-4 pixel
renders as the court at half brightness. Only OBJ palettes 4–7 take part in
colour math.

**Doubles note.** Only OBJ palettes 0–3 are exempt from colour math. Four
players plus objects and flash do not fit in them. Before doubles, either
compose flash by rewriting the hitter's palette (ADR-010 permits this),
which frees OBJ 3, or draw shadows without colour math.

## OAM

Matches `03_SPRITE_ASSET_PIPELINE.md`: 0 ball, 1 ball shadow, 2–5 bodies
sorted nearest-first, 6–9 shadows by slot (one 8×8 each), 10 spark, 11 debug
landing marker (debug builds), 14–17 accessories (unused in the POC),
18–127 hidden. Objects are written by `src/asm/oam_write.asm`, which sets the
high-table X bit for negative x and parks hidden objects at y = 224 so a
32×32 object cannot wrap to the top of the screen.

## ROM

LoROM, SlowROM, NTSC, no SRAM; internal checksum and complement sum to
`0xffff`. `make dist` records the full ROM SHA-256 in the distribution report.
