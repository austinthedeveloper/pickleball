# VRAM, CGRAM, OAM, and Build Record

Originally the M1 record; updated 2026-10-12 for the M2 art and the
[mockup decisions](10_MOCKUP_DECISIONS.md). PVSnesLib APIs express VRAM
addresses in words; byte ranges make overlap checks explicit. Static assets
are uploaded once during forced blank.

## VRAM

| Region | Word address | Reserved byte range | Actual bytes |
|---|---:|---:|---:|
| BG1 scene map | `0x0000` | `0x0000–0x07ff` | 1,792 (rows 0–27, per venue) |
| BG3 HUD map | `0x0400` | `0x0800–0x0fff` | 2,048 (rows 0–27 used) |
| BG1 scene tiles | `0x1000` | `0x2000–0x8fff` | up to 21,152 (661 tiles: shared ground + the venue's own; ADR-024/027) |
| BG2 intro card bust map (M4-STORY-03) | `0x0800` | `0x1000–0x17ff` | 2,048 (the captain's variant; only while the card is up) |
| BG2 intro card bust tiles | `0x4800` | `0x9000–0x9fff` | 1,632 (51 tiles; budget 128). BG2's tile base is `0x4000` (it steps by `0x1000` words); the map counts from tile 128. |
| BG3 HUD font, panels, big glyphs | `0x5000` | `0xa000–0xbfff` | 4,480 (280 tiles) |
| OBJ tiles (name base) | `0x6000` | `0xc000–0xffff` | 7,968 (249 tiles: the second objects row holds open play's people, ADR-034, and opponent hats, M4-OPP-04; then the park fonts) |

Player Select (ADR-020) replaces the court while it is open and reuses the
BG3 and OBJ regions:

| Region | Word address | Byte range | Actual bytes |
|---|---:|---:|---:|
| BG1 select map | `0x0000` | `0x0000–0x07ff` | 2,048 |
| BG2 portrait maps (male, female) | `0x0800`, `0x0c00` | `0x1000–0x1fff` | 4,096 |
| BG1 select tiles | `0x1000` | `0x2000–0x6dbf` | 19,904 (622 tiles; budget 768) |
| BG2 portrait tiles | `0x4000` | `0x8000–0x8ddf` | 3,552 (111 tiles; budget 256) |

CGRAM during select: BG palettes 1–6 select art, 7 portrait (composed from
palette parts), OBJ 0–3 thumbnails, 4–6 cursors and the taken silhouette;
colour math is off. Leaving the screen reloads the court tiles, map, and
palettes in forced blank.

Regions do not overlap. `assets/generated/sizes.json` records every generated
file's size, hash, and the tile counts above on each build.

**BG1 budget.** ADR-016 (amended 2026-10-12) allows 28 KiB (896 tiles). The
largest venue uses 661 (ADR-024/027). 896 tiles is the most one screen can use, so this region
cannot overflow.

## CGRAM

| Entries | Use |
|---:|---|
| 0–15 | BG3 HUD palettes 0–3 (ADR-016): 0 neutral, 1 title gold / B button, 2 left-team label, 3 right-team label. Palettes 2–3 are rewritten from the match config. |
| 16–31 | BG 4bpp palette 1: the intro card bust (M4-STORY-03), the captain's colours with a frame in roles 10 and 13. Otherwise unused (BG3 palettes 4–7 are not drawn). |
| 32–127 | BG1 palettes 2–7. BG1 never references 4bpp palette 0; the asset build fails if it does. Palette 2 (the ground) is shared by every venue and composed at runtime from the saved court colours (ADR-022); palettes 3–7 are the venue's (ADR-024). |
| 128–255 | OBJ palettes; the layout depends on the match mode (ADR-018). In singles, OBJ 0–1 are the slot 0 and slot 2 players, 2 is the ball and spark, 3 the hit flash, and 4 the shadow (black, colour math). In doubles, OBJ 0–3 are players by slot, 4 the ball, 5 the flash, and 6 the opaque shadow, with colour math off. |

Shadows use colour math: `CGWSEL = $02` (add subscreen), `CGADSUB = $50`
(half, OBJ). BG1 is also on the subscreen, so a black OBJ-palette-4 pixel
renders as the court at half brightness. Only OBJ palettes 4–7 take part in
colour math.

**Doubles.** Only OBJ palettes 0–3 are exempt from colour math, and four
players need all four, so doubles turns colour math off and draws opaque
dark shadows (ADR-018). Singles keeps translucent shadows.

## OAM

Matches `03_SPRITE_ASSET_PIPELINE.md`: 0 ball (a 32×32 large object for BIG
BALL, ADR-033), 1 ball shadow, 2–13 a group of three per player sorted
nearest-first (the hat's two halves, then the body; M4-OPP-04), 14–17 shadows
by slot (one 8×8 each), 18 spark, 19 debug landing marker (debug builds),
22–25 accessories (ponytails, ADR-014), 26–29 the practice ball machine
(ADR-028), 30–127 hidden. Objects are written by `src/asm/oam_write.asm`, which sets the
high-table X bit for negative x and parks hidden objects as small objects at
y = 240. That is below every evaluated line, so they never show and never count
toward the PPU's 32-object range-over limit.

## Open play park (ADR-034)

The park loads in forced blank like Player Select: BG1 park tiles (47) and
map at the same addresses as the court, BG palette 1, BG2 off, colour math
off. OBJ palettes: 0 you, 1–6 the teams on court, 7 the objects palette.
OAM: you 0–1, court people 2–25 (8×16, two small objects each), balls
26–28, ball shadows 29–31, rack paddles 32–43. The match reloads the court
on the way back.

## ROM

LoROM FastROM (map mode `$30`, needs 120 ns ROM; ADR-017), 512 KiB (16 banks,
header ROM size `$09`; 256 KiB until the sound effects filled it), NTSC, cartridge
type `$02` with 2 KiB battery SRAM at `$70:0000` (two 28-byte save copies at
offsets 0 and 1024; ADR-021, v2 since ADR-022);
internal checksum and complement sum to
`0xffff`. `make dist` records the full ROM SHA-256 in the distribution report.
