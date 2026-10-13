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
| ADR-006 | 12.4 fixed-point deterministic simulation in s16 (amended from 8.8 on 2026-10-12) | Sufficient court range and 1/16 px precision without floating point; 816-tcc's 32-bit code cost ~30% of the frame | Overflow/range tests fail |
| ADR-007 | Arcade rally scoring, first to 5 | Short POC loop and easier validation | Vertical slice rules pass begins |
| ADR-008 | P1 human vs P3 CPU entity slots | Exercises future doubles layout now | Never; IDs are stable |
| ADR-009 | No audio requirement through M3 | Gameplay and hardware proof first | Immediately after POC certification |
| [ADR-010](10_MOCKUP_DECISIONS.md#adr-010--palette-decoupled-from-player-slot) | Palette is per-player data, decoupled from slot | Keeps every mockup valid; Player Select needs it | — |
| [ADR-011](10_MOCKUP_DECISIONS.md#adr-011--perspective-court-with-world-to-screen-projection) | Perspective court: rules in world space, rendering projects | Matches the mockups without changing any rule or test | — |
| [ADR-012](10_MOCKUP_DECISIONS.md#adr-012--depth-sorted-player-bodies-in-oam) | Player bodies depth-sorted in OAM each frame | Teammates overlap in doubles | — |
| [ADR-013](10_MOCKUP_DECISIONS.md#adr-013--palette-contract-v2-and-runtime-palette-composition) | Palette contract v2 with runtime palette composition | Hair role and independent paddle colors | — |
| [ADR-014](10_MOCKUP_DECISIONS.md#adr-014--body-variants-via-accessory-object) | Body variants via an 8×8 accessory object | Female variant without a second body tile set | Distinct silhouette becomes a readability need |
| [ADR-015](10_MOCKUP_DECISIONS.md#adr-015--paddle-selection-with-gameplay-stats-post-poc) | Paddle selection with gameplay stats (post-POC) | Player Select mockup; gated by a rules ADR | — |
| [ADR-016](10_MOCKUP_DECISIONS.md#adr-016--scenery-hud-palettes-and-contact-effects) | Scenery, HUD palettes, and contact effects budget | CGRAM overlap and worst-row budget | BG1 exceeds its character budget |

## Risk register

| Risk | Likelihood | Impact | Mitigation | Trigger |
|---|---|---|---|---|
| Ball unreadable on CRT | Medium | High | 8×8 high-contrast ball, shadow, test on hardware in M1/M2 | Tester loses ball twice in one match |
| Object dropout in doubles | Low with plan | High | 30-sliver planned maximum, stress scene, backgrounds for HUD | Missing pixels/overflow flag |
| C simulation misses frame | Medium | High | Integer math, capped AI prediction, overrun latch, profile before assembly | Overrun latch in normal match |
| Asset converter changes output | Medium | Medium | Pin tool versions, manifest sizes, clean rebuild check | Binary diff without source change |
| Emulator hides hardware bug | Medium | High | Two emulator families plus real console gates | Hardware result differs |
| Multitap library semantics misunderstood | Medium | Medium | Input wrapper, library example, dedicated detection screen, physical test | Slot mapping mismatch |
| Scope expands into full rules/art | High | Medium | Locked exclusions and milestone exit criteria | Task lacks an M1–M3 acceptance link |
| Coordinate art and rules diverge | Medium | High | Generator and game share one generated projection table; PROJ-04 enforced in `make check` | Line call disagrees with visible line |
| PVSnesLib API changes | Medium | Medium | Pin release, wrap platform calls | Clean build breaks after tool update |
| Player palette swap reduces skin diversity | Low (mitigated) | Medium | Mitigated by ADR-013: skin is an independent palette part chosen at select | Variants are hard to distinguish |

## Deferred rule decisions

- Traditional side-out scoring and win-by-two.
- Full doubles serve order and side switching.
- Non-volley-zone foot position and momentum faults.
- Let serve policy.
- Difficulty levels and intentional randomness.
- Lob, dink, drive, charge, and special-shot controls.
- Paddle stat effects (ADR-015; rules ADR required).

Deferred means “do not accidentally implement inside M1–M3.” Write a new decision entry before changing the POC rule set.

## Questions that do not block M1

- Final title and character theme.
- Whether the court is indoor, outdoor, or fantastical.
- Music engine/content.
- Save data and long-term modes.
- PAL release plan.
- Physical cartridge manufacturing.
