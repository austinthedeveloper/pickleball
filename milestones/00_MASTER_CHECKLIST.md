# Master Checklist

This is the only cross-milestone status sheet. Detailed tasks live in the linked sheets.

## Status key

- `[ ]` not complete
- `[x]` complete and acceptance checks passed
- A blocked task stays unchecked and gets a short `BLOCKED:` note with issue/link.
- A regressed task is reopened immediately.

## Milestones

- [ ] [M1 — Bootable Court](01_BOOTABLE_COURT.md)
  - Exit: real SNES boots and P1 can move/pause within a correctly lined static court.
- [ ] [M2 — Playable Rally](02_PLAYABLE_RALLY.md)
  - Exit: P1 and CPU can serve, hit, bounce, fault, and award a single point deterministically.
- [ ] [M3 — Complete Hardware POC](03_POC_HARDWARE_BUILD.md)
  - Exit: a first-to-five match/rematch passes the emulator matrix and 15-minute real-hardware session.

## Cross-cutting gates

- [x] `GATE-DOCS` Product, gameplay, technical, asset, input, and test documents exist in the repo.
- [x] `GATE-REPRO` A clean checkout produces identical generated asset sizes and a bootable ROM.
- [x] `GATE-REUSE` Four palette variants use one player graphics data set.
  - Verified: OBJ stress scene composes four palettes over one tile set (M3-ASSET-03).
- [x] `GATE-BUDGET` Actual VRAM, ROM, OAM, and generated-asset sizes are recorded for the M3 build.
  - Verified: `dist/build-report.md` (M3-QA-04).
- [ ] `GATE-HW` Hardware log records console, cartridge, controller, build ID, duration, and result.

## Completion procedure

1. Implement one task.
2. Run its acceptance checks.
3. Mark its box in the same commit using the task ID in the commit subject.
4. When every required task on a milestone sheet is checked, run the milestone exit test.
5. Check that milestone here only after the exit test passes.

Optional tasks never block a milestone and are explicitly labeled `OPTIONAL`; all other tasks are required.
