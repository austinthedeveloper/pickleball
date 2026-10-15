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

Contract v2 ([ADR-013](10_MOCKUP_DECISIONS.md#adr-013--palette-contract-v2-and-runtime-palette-composition)):

| Index | Role | Source part |
|---:|---|---|
| 0 | Transparent | — |
| 1 | Outline | Shared |
| 2–4 | Skin ramp | Skin part |
| 5–7 | Shirt/team ramp (also cap) | Team part |
| 8–9 | Shorts ramp (was 3 colors) | Team part |
| 10 | Shoe accent | Team part |
| 11–13 | Paddle face dark / face light / edge-grip | Paddle part |
| 14 | Highlight and impact-flash light (was 13 + 14) | Shared |
| 15 | Hair | Skin part |

Each player's 16-color OBJ palette is composed at match start from the shared, team, skin, and paddle parts and uploaded once during the match-setup forced blank. The POC uses one skin part and one standard paddle part per team color, so its output equals fixed palettes.

The first four variants are `PAL_BLUE`, `PAL_RED`, `PAL_GREEN`, and `PAL_GOLD`. Palettes are assigned per match; slot number never implies color ([ADR-010](10_MOCKUP_DECISIONS.md#adr-010--palette-decoupled-from-player-slot)). Each palette is 16 SNES colors/32 bytes and shares transparency at index 0.

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
      team_*.json
      skin_*.json
      paddle_*.json
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
| 2–13 | A group of three per player, sorted nearest-first each frame ([ADR-012](10_MOCKUP_DECISIONS.md#adr-012--depth-sorted-player-bodies-in-oam)): the hat's two halves, then the body (ADR-014 hats amendment, M4-OPP-04) |
| 14–17 | Player shadows 1–4 |
| 18–21 | Impact effects reserve (18 spark, 19 debug landing marker) |
| 22–25 | Accessories 1–4 ([ADR-014](10_MOCKUP_DECISIONS.md#adr-014--body-variants-via-accessory-object)) |
| 26–29 | Practice ball machine (ADR-028) |
| 30–31 | Close call replay: the ball and its shadow ([ADR-041](decisions/ADR-041-close-call-replay.md)) |
| 32–33 | The opponent's partner watching a singles match ([ADR-042](decisions/ADR-042-sitting-out.md)) |
| 34–127 | Hidden/reserved |

Confirm whether the selected PVSnesLib API expects byte offsets (`0, 4, 8...`) or logical indices at each call site. Wrap library calls in `render_obj_*` helpers so the game never mixes conventions.

## Animation architecture

Gameplay produces an animation ID, frame index, facing, palette ID, and screen position. Rendering maps those values through generated metadata into OAM. A frame change changes the tile offset; it does not copy a new frame into VRAM.

For later characters, prefer this order:

1. Palette-only identity.
2. Shared body plus small accessory object.
3. Selective alternate frames.
4. Entirely separate tile set only when silhouette/gameplay demands it.

## Asset review checklist

- [x] Player is legible at 32×32 on a nearest-neighbor 2× preview.
- [x] Ball remains visible over every court tile and line color.
- [x] Shadow is visibly distinct from the ball and uses one 8×8 object.
- [x] Four palettes work with the exact same player tile bytes.
- [ ] Flipped racket/pose does not create an anatomical or handedness problem. (Open: a left-facing player holds the paddle in the other hand; see KNOWN_ISSUES #7.)
- [x] Every frame uses only declared palette roles.
- [x] Frame anchors do not jitter.
- [x] Worst-row object count remains at or below 30 planned slivers.
- [ ] Each accessory is one 8×8 object in its owner's palette (hair index 15) at OAM 22–25, with a per-frame offset that mirrors around the body anchor under horizontal flip.
- [ ] Each hat is 16×8 (two objects) in its owner's palette, covers every cap pixel (`tools/art/author_hats.py` checks), and sits at the accessory offset plus `hat_offset`, mirrored as 16 − x with its halves swapped.
- [x] Asset conversion can be repeated from a clean checkout.
- [x] Manifest report records tile, palette, and byte totals.

## Placeholder policy

M1 may use geometric placeholder tiles. M2 must use the final dimensions and palette-role contract. M3 must prove the complete conversion/reuse pipeline, but art quality can remain temporary.
