# Known Issues

Open defects and accepted minors for the M3 candidate (`M3-QA-06`). None
are blockers. Items 1 and 2 need a decision before M3 sign-off.

| # | Severity | Issue | Reproduction | Status / reason |
|---|---|---|---|---|
| 1 | Major (if sustained) | Occasional single-frame CPU overruns: about 1.5% of frames in a match and about 5% in the attract demo on SlowROM. Spikes come from hit frames and HUD redraws. | Release ROM, play rallies; the debug overlay's `O` counter climbs. Measure with `make release PROFILE=1`. | Mitigated by 12.4 math, scratch structs, and the asm OAM writer. A FastROM build measured about 1.8% in the demo. Decide: FastROM (cartridge must support 120 ns ROM) or more assembly. |
| 2 | Minor | BG1 scene needs 625 tiles, against ADR-016's 512-tile plan. | `dist/build-report.md`. | VRAM has room (docs/09). Decide: raise the budget, move scenery to BG2, or merge tiles. |
| 3 | Minor | Toss frame is rough (cheer redrawn 2026-10-12). | Watch the serve toss. | Placeholder art, allowed through M3 (docs/03). |
| 4 | Minor | Doubles needs a new OBJ palette plan: only OBJ palettes 0–3 are exempt from shadow colour math. | — | Recorded in docs/09; vertical-slice work. |
| 5 | Minor | A ball passing behind a far player still draws in front of them. | — | Accepted by ADR-012 (readability). |
| 6 | Minor | Mesen reports reads of compiler scratch registers (`$16–$42`) before they are written. | `make smoke`, watch stderr. | tcc prologues save these registers; no game state is involved. |
