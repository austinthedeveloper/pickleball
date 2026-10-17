# ADR-060 — More Hats

Status: **Accepted 2026-10-16**
Milestone: `M4-ART-02`. Extends: ADR-059 (character models), ADR-047
(career unlocks), M4-OPP-04 (hats).

## Why

With bareheaded bodies (ADR-059) a hat no longer has to hide a cap, so
any 16×8 shape works. Career players had four hats to earn.

## Decision

Seven new hats, career unlocks, looks only (no paddle gear):

| Hat | Look | Unlock |
|---|---|---|
| BEANIE | ribbed, shirt ramp, white pom-pom | win 3 matches |
| BUCKET | shirt ramp, band, floppy brim | 10 open play games |
| BACKCAP | the cap worn backwards | win 25 matches |
| WHIRLY | propeller beanie, blades in the shoe accent | a 30-shot rally |
| VIKING | grey (shorts shadow) helmet, white horns, shirt band | beat the GYM BROS |
| CHEF | white toque, grey band | a 10-win streak |
| CROWN | white, shirt-colour jewels | 3 ladder clears |

- **Colours** are the owner's palette only (contract v2): the paddle
  roles change with the paddle, so a hat can't use them, and there is no
  gold. The crown is silver.
- **Art** is in the port kit (`compose.py` `HATS`, `hats_<team>.png`, now
  192×8) and imported with `tools/art/import_kit_sprites.py`.
- **Tiles: name table 2.** Name table 1 has 7 free tiles (three hats). A
  hat in the manifest is either an `objects.png` cell or `{"sheet": k}`,
  the k-th of eight 16×8 places in the free player-sheet slot `hat_slot`
  (16, sheet row 4), in rows of two, so each hat's tiles are an even
  number and the next, as `render_bodies` needs for flipping (tile ^ 1).
  `oam_write` already sets the name-table bit for tiles 256-511. The sheet
  grows to 128×160; name table 2 holds 128 tiles (4 KiB), uploaded with
  the rest at boot. One slot holds one more hat.
- **Ids** append after the bare head: 0-5 unchanged, 6-12 new. Old saves
  keep their hats and unlock bits. `career.json`'s hat list follows the
  manifest's order, the bare head's entry with any name; hat names fit 8
  cells (the career editor's value). `CAREER_HAT_NONE` is generated.
- **Unlocks** take bits 9-15 of the u16 mask: all of them. A further
  unlock needs a wider mask (a save layout change, ADR-032).
- **CPU opponents** keep their hats; any of the new ones can be given to
  one in `opponents.json`.

## Costs

No new per-frame work: a hat costs the same wherever its tiles are.
A veteran save that already qualifies earns several new hats after its
next match, on one unlock card (the names that fit, then "+n").

## Verification

- `make check` passes; `test_career` covers the beanie (with the third
  win), the viking helmet (beating the GYM BROS), and the crown (three
  clears); the career editor still skips locked hats to NONE.
- Screenshots: a doubles match with every new hat poked into `rb_hat`,
  both facing directions and with ponytails.
- `make perf`: zero overruns, worst frames within a scanline of ADR-059's (demo 222, open play 221). `make golden-compare`: identical.
