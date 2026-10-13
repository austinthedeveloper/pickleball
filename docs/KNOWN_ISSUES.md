# Known Issues

Open defects and accepted minors for the M3 candidate (`M3-QA-06`). None
are blockers.

| # | Severity | Issue | Reproduction | Status / reason |
|---|---|---|---|---|
| 1 | Minor | One-frame hitch when a point ends or a serve is set up, while the banner and HUD redraw (about 10 per 2,000 match frames). Zero overruns during live rallies; about 0.2% of frames in the title demo. | Release ROM; the debug overlay's `O` counter steps once per point. | Resolved for play by FastROM (ADR-017), the asm HUD fill, and cheaper HUD drawing. The remaining transition hitch is accepted: the screen is changing on that frame anyway. |
| 2 | Resolved | BG1 scene used 625 tiles against the 512-tile plan. | — | BG1 budget raised to 28 KiB / 896 tiles (ADR-016 amended). |
| 3 | Minor | Toss frame is rough (cheer redrawn 2026-10-12). | Watch the serve toss. | Placeholder art, allowed through M3 (docs/03). |
| 4 | Minor | Doubles needs a new OBJ palette plan: only OBJ palettes 0–3 are exempt from shadow colour math. | — | Recorded in docs/09; vertical-slice work. |
| 5 | Minor | A ball passing behind a far player still draws in front of them. | — | Accepted by ADR-012 (readability). |
| 6 | Minor | Mesen reports reads of compiler scratch registers (`$16–$42`) before they are written. | `make smoke`, watch stderr. | tcc prologues save these registers; no game state is involved. |
