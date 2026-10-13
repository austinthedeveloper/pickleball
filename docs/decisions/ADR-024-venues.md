# ADR-024 — Venues

Status: **Accepted 2026-10-13**
Starts the courts/locations idea in [IDEAS.md](../IDEAS.md).
Depends on: ADR-016 (BG1 budget), ADR-022 (court colours), ADR-023
(opponents).

## What a venue is

A venue changes the **backdrop** and the **default ground colours**. The
court, lines, net, sand, and bottom fence keep the same tiles everywhere, so
geometry, projection, and the PROJ-04 line check are untouched.

| Venue | Scene | Mock | Ground when DEFAULT |
|---|---|---|---|
| PARK | `court_scene.png` | `level-01` | painted teal / dirt / cream |
| LAKESIDE | `venue_lake.png` | `level-02` | GREEN / GRASS / WHITE |
| SUNSET | `venue_sunset.png` | `level-03` | painted teal / BRICK / cream |

`tools/art/prep_court_scene.py [PARK|LAKESIDE|SUNSET]` makes each scene (it
needs Pillow and numpy and is not part of the build):

1. Resample the mock and in-paint its HUD panels.
2. Paint the court, net, and fence procedurally from `court.h`.
3. Overlay the mock's corner bushes.
4. Quantize the rest into BG1 palettes 3–7.

The PARK output is byte-identical to the committed `court_scene.png`.

## Data

`assets/source/venues.json` lists the venues. The asset build:

- **Shares the ground:** every BG1 palette-2 tile, from any venue, goes into
  one deduplicated set (`court.pic`, 257 tiles, 8 KiB) at the start of the
  BG1 tile region. Palette 2 (`court.pal`) must be identical in every scene,
  which the build checks.
- **Gives each venue its own data:**
  - tiles (`venue_N.pic`, ~370 tiles, ~12 KiB), placed after the ground
    tiles
  - a 32 × 28 map (`venue_N.map`, 1,792 bytes)
  - palettes 3–7 (`venue_N.pal`, 160 bytes)
  - its own 32 KiB-bank-safe ROM section (`venues.inc`, generated)
- **Enforces the tile budget:** ground + venue ≤ 896 tiles (ADR-016). The
  largest venue uses 653.
- **Generates `venues.h`:**
  - `g_venue_names`
  - `g_venue_ground`, the court colour presets that DEFAULT means at each
    venue
  - `g_venue_data`, the ROM pointers and sizes, emitted as literals. It is
    defined only in the renderer (`VENUE_DATA_DEFINE`), since host tests
    have no ROM labels.
- **Validates:** names are ≤ 8 cells (the COURT page), ground presets exist,
  and there are at most 9 venues.

ROM after three venues: about 157 KiB used of 256, so about 6 more venues fit
before the cart needs to grow.

## Choosing a venue

- `SaveSettings.venue` is `0` for AUTO, or the venue index + 1. SaveData goes
  to version 3, and older saves reset.
- **AUTO** is the opponent's home court: each opponents.json team has a
  `venue`. With no opponent (the attract demo, 2P versus), AUTO is PARK.
- **Homes:**
  - HENDERSON, GYM BROS: PARK
  - SNOWBIRDS, PIRATES, SURF CREW: LAKESIDE
  - GOTH CLUB, TWINS, CHAMPS: SUNSET
- The COURT page has a **VENUE** row first (`AUTO`, `PARK`, `LAKESIDE`,
  `SUNSET`). Colour rows still preview live. A venue change applies when the
  page closes with B or Start: the court reloads with the same fade as
  returning from Player Select.
- **DEFAULT colours** now mean the venue's ground (`g_venue_ground`) rather
  than the painted colours. Explicit presets still win.
- `render_load_court` resolves the venue on every court load (boot, after
  Player Select, after a VENUE change). It uploads the ground tiles, the
  venue tiles, the map, palettes 3–7, and the composed palette 2. After a
  match the title keeps that match's venue until the next load.

## Frame budget on the title

The attract demo alone reaches about 200 scanlines on its heavy frames. A
title-area row or the title prompt can cost 60+ scanlines. Title drawing (the
staged steps and the area parts) now waits for a later frame when the frame's
work is past scanline 140 (`profile_scanline`, `HUD_TITLE_SCANLINE_LIMIT`).

Results:

- Zero overruns on the idle, records, court, venue, doubles, and golden
  timelines.
- Zero overruns in a 3,000-frame title soak, where the worst frame ends at
  scanline 216.

## Tests

- `test/test_court_style.c` (host):
  - DEFAULT at each venue equals that venue's ground presets
  - an explicit preset wins over the venue's
  - AUTO resolves to the opponent's home, else PARK
  - a saved venue wins
  - the VENUE row wraps through AUTO and the venues, and is restored by Y
- Asset fixtures: an unknown ground preset, and a venue name over 8 cells.
- Smoke:
  - `test/smoke/venue.timeline`: SUNSET chosen on the title, then a match
    there
  - `test/smoke/lake.timeline`: LAKESIDE
- `test/smoke/records.timeline` now presses Down three times (COURT sits
  before RECORDS).
- Golden: the court load does more DMA, so the match appears a frame later.
  Frames before the first serve differ only in timers, and everything from
  the serve on is identical. Re-recorded.
