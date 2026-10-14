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
| 8 | Minor | Debug builds overrun more often than release, because the hex status overlay redraws every frame. | `make debug`; the `O` counter climbs faster than in release. | Accepted. Measure timing on release (`make release PROFILE=1`) or hardware-test builds. |
| 9 | Minor | In the singles title demo, the demo P1 thinks every other frame to fit two AIs in the frame, so it reacts a little later than the CPU does. | Watch the attract demo. | Accepted; the demo only. Human matches run one AI at full rate. In the doubles demo P1 follows the other CPUs' rule (ADR-018, amended). |
| 10 | Minor | Impact effects are behind player bodies in OAM order (docs/03: bodies 2–13, effects 18–21). | — | Mitigated: the spark leads 10 px toward the net, in front of the paddle (`FX_SPARK_LEAD_X`). A future swing-arc effect needs its own plan (ADR-016 already rules out a trail). |
| 11 | Minor | The golden trace's scripted P1 never wins a point, so P1-scoring paths are covered only by host tests. | `build/golden/trace.txt`. | Accepted. A smarter input script, or a bot-driven trace mode, would widen emulator coverage. |
| 12 | Minor | Doubles shadows are opaque, not translucent. | Play doubles. | By design (ADR-018): four players use all four colour-math-exempt OBJ palettes. |
| 13 | Resolved | Doubles has one human (P1); port 2 and Multitap humans are not yet assignable. | — | ADR-020: 2P joins on port 2 (Start on the title). Multitap players 3–4 remain deferred. |
| 14 | Resolved | Doubles with all four modifiers drops about 1 frame in 200 (7-14 in 2,400 frames), on frames where a shot, a point, or the owner's swing lookahead meets a second CPU's think. A plain doubles ladder match drops none (it dropped 17 in 2,400 before `M4-UI-04`'s first pass). | `modifiers_doubles.timeline`, `doubles_ladder.timeline` in `make perf`. | Fixed by `M4-UI-03`/`M4-UI-04`: `player_step` in assembly; zero overruns on every `make perf` run (budgets 0). |
| 15 | Minor | One overrun at boot, during the first title frames. | Singles soak: `ovr=1` by frame 60, then none. | Accepted; invisible behind the title fade. |
| 16 | Minor | The headless Mesen runner cannot press port-2 buttons (`emu.setInput` always reaches port 1), so 2P joining is not smoke-tested. | `test/smoke/select.timeline` covers 1P only. | 2P logic is covered by `test/test_select.c`; check 2P by hand with a second pad. |
| 17 | Minor | CPUs always use the standard (blue) paddle, so a red CPU holds a blue paddle. | Play any match. | By ADR-015/020: the CPU's paddle is fixed per difficulty. |
| 18 | Minor | Select-screen labels, stat names, and paddles are baked into BG1; only the bars, tag, and paddle name are live text. | — | The HUD font is too wide for four colour labels in 31-px boxes. |
