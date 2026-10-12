# Decisions and Risks

Status: **Living record**

## Accepted decisions

| ID | Decision | Why | Revisit when |
|---|---|---|---|
| ADR-001 | PVSnesLib + C | Fastest path to a real SNES ROM with maintained input/graphics examples | Profiling shows an unsolved limitation |
| ADR-002 | Static 256×224 Mode 1 court | Readable, inexpensive, no scrolling/camera edge cases | A later mode truly needs scrolling |
| ADR-003 | 8×8 / 32×32 object pair | One object per player plus small ball/effects | Character silhouette cannot fit 32×32 |
| ADR-004 | Shared player tiles + palette variants | Directly solves reuse and preserves VRAM | A character requires gameplay-readable silhouette |
| ADR-005 | Preload animation frames | Predictable frame timing and simple M1–M3 renderer | Content outgrows OBJ VRAM |
| ADR-006 | 8.8 fixed-point deterministic simulation | Sufficient court range and subpixel movement without floating point | Overflow/range tests fail |
| ADR-007 | Arcade rally scoring, first to 5 | Short POC loop and easier validation | Vertical slice rules pass begins |
| ADR-008 | P1 human vs P3 CPU entity slots | Exercises future doubles layout now | Never; IDs are stable |
| ADR-009 | No audio requirement through M3 | Gameplay and hardware proof first | Immediately after POC certification |

## Risk register

| Risk | Likelihood | Impact | Mitigation | Trigger |
|---|---|---|---|---|
| Ball unreadable on CRT | Medium | High | 8×8 high-contrast ball, shadow, test on hardware in M1/M2 | Tester loses ball twice in one match |
| Object dropout in doubles | Low with plan | High | 26-sliver planned maximum, stress scene, backgrounds for HUD | Missing pixels/overflow flag |
| C simulation misses frame | Medium | High | Integer math, capped AI prediction, overrun latch, profile before assembly | Overrun latch in normal match |
| Asset converter changes output | Medium | Medium | Pin tool versions, manifest sizes, clean rebuild check | Binary diff without source change |
| Emulator hides hardware bug | Medium | High | Two emulator families plus real console gates | Hardware result differs |
| Multitap library semantics misunderstood | Medium | Medium | Input wrapper, library example, dedicated detection screen, physical test | Slot mapping mismatch |
| Scope expands into full rules/art | High | Medium | Locked exclusions and milestone exit criteria | Task lacks an M1–M3 acceptance link |
| Coordinate art and rules diverge | Medium | High | One court definition feeds validation and asset guide | Line call disagrees with visible line |
| PVSnesLib API changes | Medium | Medium | Pin release, wrap platform calls | Clean build breaks after tool update |
| Player palette swap reduces skin diversity | Medium | Medium | Reserve independent skin ramp; accessories later | Variants are hard to distinguish |

## Deferred rule decisions

- Traditional side-out scoring and win-by-two.
- Full doubles serve order and side switching.
- Non-volley-zone foot position and momentum faults.
- Let serve policy.
- Difficulty levels and intentional randomness.
- Lob, dink, drive, charge, and special-shot controls.

Deferred means “do not accidentally implement inside M1–M3.” Write a new decision entry before changing the POC rule set.

## Questions that do not block M1

- Final title and character theme.
- Whether the court is indoor, outdoor, or fantastical.
- Music engine/content.
- Save data and long-term modes.
- PAL release plan.
- Physical cartridge manufacturing.
