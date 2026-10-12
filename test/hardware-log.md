# Hardware Test Log

Use an unmodified NTSC SNES/Super Famicom and record console revision, flash
cartridge, ROM checksum, duration, and every result from `docs/05_TEST_PLAN.md`.

No M1 hardware run has been recorded yet.

## Emulator observations

### 2026-10-12 — MesenCE — `0.1.0-release-40fa1d2-dirty`

- Source: user-supplied screenshot of the canonical `dist/` ROM.
- Cold boot/render: PASS (single observed boot).
- Mode 1 court: PASS; baselines, sidelines, kitchen lines, centerlines, and net
  are visible and aligned with the placeholder layout.
- P1 placeholder object: PASS at the initial spawn position.
- Not established by this observation: movement at every bound, pause toggles,
  reset repetitions, five-minute soak, second-emulator accuracy pass, or real
  hardware behavior.

