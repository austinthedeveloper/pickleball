# Sprite and Asset Pipeline

Status: **Required before non-placeholder character art**

## Objective

Solve sprite reuse structurally: one body tile set, palette-defined variants, horizontal flips, fixed frame slots, deterministic conversion, and a build-time budget report. The POC must prove the pipeline with four visible palette variants even though only two players are active.

## Hardware-facing decisions

- Object size pair: 8×8 small and 32×32 large.
- Player: one 32×32 hardware object, 4bpp, color index 0 transparent.
- Ball and shadows: 8×8 objects.
- All player frames are preloaded into OBJ VRAM for the POC.
- Face left/right by horizontal OAM flip; do not export mirrored frames.
- Player identity/team comes from an object palette; do not export recolored tile duplicates.
- Court/HUD elements that do not move belong on background layers.

## Palette contract

Every player frame uses the same indexed color roles:

| Index | Role |
|---:|---|
| 0 | Transparent; never used for visible pixels |
| 1 | Outline |
| 2–4 | Skin ramp |
| 5–7 | Shirt/team ramp |
| 8–10 | Shorts/accent ramp |
| 11–12 | Shoe/racket dark/light |
| 13 | Highlight |
| 14 | Impact flash-compatible light |
| 15 | Reserved |

The first four variants are `P1_BLUE`, `P2_RED`, `P3_GREEN`, and `P4_GOLD`. Skin and equipment indices may remain identical across palettes; team ramps change. Each palette is 16 SNES colors/32 bytes and shares transparency at index 0.

## Required player frames

| Animation | Frames | Reuse rule |
|---|---:|---|
| Idle | 2 | Loop 20 frames per cel |
| Run | 4 | Loop based on movement distance |
| Windup | 1 | Hold for state duration |
| Active hit | 2 | State selects frame |
| Recovery | 2 | State selects frame |
| Serve | 3 | Toss/contact/follow-through |
| Point reaction | 2 | Optional for M3; may reuse idle/recovery |

Target: 14 frames maximum for M3. At 32×32, each 4bpp frame is 512 bytes, so the uncompressed player tile target is at most 7 KiB. Duplicate horizontal or recolored exports fail review.

## Source layout

```text
assets/
  source/
    player_master.aseprite
    court_master.tmx
    palettes/
      players.gpl
  export/
    player_indexed.png
    court_indexed.png
    hud_indexed.png
  generated/
    player.pic
    player.pal
    court.pic
    court.map
    court.pal
    asset_manifest.h
  manifest/
    player_frames.json
    palettes.json
```

Only `source`, `export`, and manifests are authored. `generated` is reproducible and may be cleaned. If the chosen tool produces differently named binary extensions, document the mapping without changing the conceptual split.

## Export rules

1. Canvas/grid stays 32×32 per player frame with a constant foot anchor.
2. Indexed PNG only; no alpha blending, semi-transparency, resampling, or automatic color optimization.
3. Color 0 is the transparent swatch in every object palette.
4. Nearest-neighbor transforms only.
5. Frame order is declared in `player_frames.json`, never inferred from filenames.
6. Conversion is non-interactive and runs from the build.
7. The build fails when dimensions, bit depth, palette roles, tile count, or frame count violate the manifest.
8. Generated headers expose named frame tile offsets; gameplay code never embeds raw tile numbers.

## OAM allocation

Reserve stable IDs so draw order and overflow behavior are reviewable:

| OAM indices | Use |
|---|---|
| 0 | Ball |
| 1 | Ball shadow |
| 2–5 | Player bodies 1–4 |
| 6–9 | Player shadows 1–4 |
| 10–13 | Impact effects reserve |
| 14–127 | Hidden/reserved |

Confirm whether the selected PVSnesLib API expects byte offsets (`0, 4, 8...`) or logical indices at each call site. Wrap library calls in `render_obj_*` helpers so the game never mixes conventions.

## Animation architecture

Gameplay produces an animation ID, frame index, facing, palette ID, and screen position. Rendering maps those values through generated metadata into OAM. A frame change changes the tile offset; it does not copy a new frame into VRAM.

For later characters, prefer this order:

1. Palette-only identity.
2. Shared body plus small accessory object.
3. Selective alternate frames.
4. Entirely separate tile set only when silhouette/gameplay demands it.

## Asset review checklist

- [ ] Player is legible at 32×32 on a nearest-neighbor 2× preview.
- [ ] Ball remains visible over every court tile and line color.
- [ ] Shadow is visibly distinct from the ball and uses one 8×8 object.
- [ ] Four palettes work with the exact same player tile bytes.
- [ ] Flipped racket/pose does not create an anatomical or handedness problem.
- [ ] Every frame uses only declared palette roles.
- [ ] Frame anchors do not jitter.
- [ ] Worst-row object count remains at or below 26 planned slivers.
- [ ] Asset conversion can be repeated from a clean checkout.
- [ ] Manifest report records tile, palette, and byte totals.

## Placeholder policy

M1 may use geometric placeholder tiles. M2 must use the final dimensions and palette-role contract. M3 must prove the complete conversion/reuse pipeline, but art quality can remain temporary.
