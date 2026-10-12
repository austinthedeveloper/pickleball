# SNES Pickleball — Preproduction Pack

Working title: **Pickleball '94**. The name is a placeholder, not a shipping title.

This pack is the source of truth for a real-hardware Super Nintendo proof of concept. It is deliberately small: one local player against a CPU opponent, one court, one match, and placeholder art. The architecture leaves room for four local players through one Super Multitap connected to controller port 2.

## Product sentence

An arcade pickleball game with the readable, physical feel of 16-bit dodgeball games: a fixed horizontal court, chunky movement, committed swings, exaggerated reactions, and short matches.

## Locked POC scope

- NTSC, 256×224, 60 Hz target.
- Real unmodified SNES/Super Famicom compatibility.
- Standard cartridge hardware only: no enhancement chip and no mandatory SRAM.
- PVSnesLib with C first and assembly only after profiling proves a need.
- One human on the left versus one CPU player on the right.
- Static court with recognizable pickleball lines.
- D-pad movement, one context-sensitive hit button, Start to pause.
- Serve, rally, faults, score, win state, and instant rematch.
- One shared 32×32 player graphics set with palette swaps and horizontal flip.
- Future target: two-versus-two with 1–4 humans; a Super Multitap in port 2 supplies pads 2–4.

## Explicitly out of scope for the first three milestones

- Online play, season mode, character stats, save data, music, announcer, replay, scrolling courts, Mode 7, special chips, and final art.
- Tournament-complete pickleball rules. The POC uses the simplified rules in [Gameplay Spec](docs/01_GAMEPLAY_SPEC.md).
- PAL tuning. The build must not rely on emulator-only behavior, but NTSC is the first certified target.

## Read this first

1. [Product Brief](docs/00_PRODUCT_BRIEF.md)
2. [Gameplay Spec](docs/01_GAMEPLAY_SPEC.md)
3. [Technical Plan](docs/02_TECHNICAL_PLAN.md)
4. [Sprite and Asset Pipeline](docs/03_SPRITE_ASSET_PIPELINE.md)
5. [Input and Multitap Plan](docs/04_INPUT_MULTITAP_PLAN.md)
6. [Repository Bootstrap](docs/06_REPO_BOOTSTRAP.md)
7. [Master Checklist](milestones/00_MASTER_CHECKLIST.md)

## Milestone sheets

- [M1 — Bootable Court](milestones/01_BOOTABLE_COURT.md)
- [M2 — Playable Rally](milestones/02_PLAYABLE_RALLY.md)
- [M3 — Complete Hardware POC](milestones/03_POC_HARDWARE_BUILD.md)

Every implementation task lives on exactly one milestone sheet. Mark a task `[x]` in the same commit that satisfies its acceptance check. Do not mark parent exit criteria complete until every required child task and test is complete.

## Build the M1 ROM

The repository now contains the M1 bootable-court implementation. With the
pinned PVSnesLib 4.6.0 sibling install shown in `toolchain/README.md`:

```sh
make check
make debug
make release
make hardware-test
make dist
```

Debug and hardware-test builds show the build ID, P1 coordinates, and pause
state on BG3. Use the D-pad to move P1 and Start to pause. Build artifacts are
written below `build/`; the canonical ROM to load in an emulator or flash cart
is the release `.sfc` below `dist/`. Root-level ROM files are temporary
PVSnesLib linker outputs and are ignored by Git.

## POC success test

The POC is complete when a new player can power on a real NTSC SNES, move, serve, sustain a rally against the CPU, see an unambiguous fault and score, finish a first-to-five match, and rematch without resetting. No sprite dropout, lockup, input loss, or steadily worsening frame time may appear during a 15-minute hardware session.

## Sources

Hardware and toolchain claims are linked in [Sources](docs/08_SOURCES.md). Treat those links—not emulator folklore—as the starting point when a low-level assumption is disputed.
