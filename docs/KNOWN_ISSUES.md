# Known Issues

Open defects and accepted minors for the M3 candidate (`M3-QA-06`). None
are blockers.

| # | Severity | Issue | Reproduction | Status / reason |
|---|---|---|---|---|
| 1 | Resolved | Frame overruns (earlier about 1.5% of match frames, then one per point). | — | FastROM (ADR-017), cheaper HUD drawing, and staged overlay redraws: 0 overruns across 7,200 match frames; about 0.2% in the title demo, where two AIs run. |
| 2 | Resolved | BG1 scene used 625 tiles against the 512-tile plan. | — | BG1 budget raised to 28 KiB / 896 tiles (ADR-016 amended). |
| 3 | Minor | Toss frame is rough (cheer redrawn 2026-10-12). | Watch the serve toss. | Placeholder art, allowed through M3 (docs/03). |
| 4 | Minor | Doubles needs a new OBJ palette plan: only OBJ palettes 0–3 are exempt from shadow colour math. | — | Recorded in docs/09; vertical-slice work. |
| 5 | Minor | A ball passing behind a far player still draws in front of them. | — | Accepted by ADR-012 (readability). |
| 6 | Minor | Mesen reports reads of compiler scratch registers (`$16–$42`) before they are written. | `make smoke`, watch stderr. | tcc prologues save these registers; no game state is involved. |
| 7 | Minor | Horizontal flip mirrors handedness: the left-facing player holds the paddle in the other hand. | Watch the CPU (faces left). | Accepted for the POC; fixing it needs mirrored arm frames, which docs/03 forbids, or a paddle accessory object. Revisit with ADR-014 accessories. |
