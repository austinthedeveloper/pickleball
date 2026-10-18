# Pickleball '94

An arcade pickleball game for the Super Nintendo: a fixed court seen from the
side, chunky movement, committed swings, and short matches. The name is a
working title.

It runs on a real NTSC SNES/Super Famicom: a 1 MiB LoROM FastROM cart
(ADR-017) with 2 KiB of battery-backed SRAM for records and settings
(ADR-021), no enhancement chip. It is written in C for 816-tcc (PVSnesLib)
plus 65816 assembly in `src/asm/`; the simulation and rules also build on the
host for tests.

## What's in the game

- Singles and doubles against CPU teams, or with a friend: 2P on controller 2;
  controllers 3 and 4 join through a Super Multitap but sit out until
  four-player matches arrive (`M4-MP-01`).
- A ladder of eight CPU teams with their own play styles, story cards, and
  portraits, kept per mode.
- Open play at the park, a practice ball machine, and career players with
  stats and cosmetic unlocks.
- Venues, court colours, night courts, options, and modifiers.
- Pickleball rules with rally scoring: the serve, the two-bounce rule, and
  kitchen volleys.

Controls and rules are in [docs/PLAYER_GUIDE.md](docs/PLAYER_GUIDE.md) and the
booklet draft in [docs/MANUAL.md](docs/MANUAL.md).

## Read this first

1. [CLAUDE.md](CLAUDE.md): how the code is built, the rules of the code, and
   the checks before calling something done.
2. [docs/MANUAL.md](docs/MANUAL.md) and
   [docs/PLAYER_GUIDE.md](docs/PLAYER_GUIDE.md): the game as it plays.
3. [docs/decisions/](docs/decisions/): the ADRs from ADR-018 on; ADR-001 to
   ADR-017 are in [07_DECISIONS_AND_RISKS.md](docs/07_DECISIONS_AND_RISKS.md)
   and [10_MOCKUP_DECISIONS.md](docs/10_MOCKUP_DECISIONS.md).
4. [milestones/04_GAME_FEATURES.md](milestones/04_GAME_FEATURES.md): the
   feature tasks, done and open.
5. [docs/KNOWN_ISSUES.md](docs/KNOWN_ISSUES.md): defects, open and fixed.
6. [docs/REFACTOR_PLAN.md](docs/REFACTOR_PLAN.md): refactors and
   optimization passes.
7. [docs/NEXT_GAME_PLAYBOOK.md](docs/NEXT_GAME_PLAYBOOK.md): lessons for the
   next project.

Also current: the VRAM/OAM/ROM record in
[09_M1_VRAM_AND_BUILD.md](docs/09_M1_VRAM_AND_BUILD.md), unplanned ideas in
[IDEAS.md](docs/IDEAS.md), and the release process in
[RELEASING.md](docs/RELEASING.md).

**Historical:** docs 00–06 and 10 are the proof-of-concept plan (one player
against a CPU, one court, first to five). Where they disagree with the ADRs
or the manual, the ADRs and the manual win. Milestones M1–M3 are that plan's
sheets; their open boxes wait on real-hardware runs
([test/hardware-log.md](test/hardware-log.md)).

## Build and play

With the pinned PVSnesLib 4.6.0 sibling install described in
`toolchain/README.md`:

```sh
make check          # host tests, asset validation, 816-tcc pitfall check
make release        # build/release/pickleball-<version>-release-<id>.sfc
make debug          # adds the debug overlay and the OBJ stress scene
make dist           # dist/: ROM, SHA-256, build report, player guide, hardware log
make repro          # two clean builds must produce identical hashes
make smoke SMOKE=m2 # headless MesenCE run of test/smoke/m2.timeline
make perf           # dropped frames, hangs, and sound on every timeline (test/perf.json)
make golden-record  # then golden-compare: the simulation trace before and after
make stats          # host bot matches against the ladder (see CLAUDE.md)
make release PROFILE=1  # per-stage scanline profiler (see include/profile.h)
```

`<version>` is `VERSION_NAME` in the `Makefile`. The ROM boots into a
CPU-vs-CPU demo behind the title menu. In debug and hardware-test builds,
pausing shows the build ID and Multitap status, and Select opens the OBJ
stress scene. Build artifacts are written below `build/`; the canonical ROM
is the release `.sfc` in `dist/`. Releases are tagged GitHub Releases
([docs/RELEASING.md](docs/RELEASING.md)).

Art sources live in `assets/source/` as indexed PNGs plus palette part files
([assets/source/README.md](assets/source/README.md)). Normal builds need only
the Python standard library.

## Sources

Hardware and toolchain claims are linked in [Sources](docs/08_SOURCES.md).
Treat those links, not emulator folklore, as the starting point when a
low-level assumption is disputed.
