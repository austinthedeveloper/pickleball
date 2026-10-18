# Repository Bootstrap

**Historical:** the POC plan. Current behaviour is in the ADRs ([docs/decisions/](decisions/)) and [docs/MANUAL.md](MANUAL.md); where this doc disagrees, they win.

Status: **Historical** (executed for M1)

## Repository tree

```text
/
  README.md
  LICENSE
  Makefile
  .gitignore
  docs/
    00_PRODUCT_BRIEF.md
    01_GAMEPLAY_SPEC.md
    02_TECHNICAL_PLAN.md
    03_SPRITE_ASSET_PIPELINE.md
    04_INPUT_MULTITAP_PLAN.md
    05_TEST_PLAN.md
    06_REPO_BOOTSTRAP.md
    07_DECISIONS_AND_RISKS.md
    08_SOURCES.md
    09_M1_VRAM_AND_BUILD.md
    10_MOCKUP_DECISIONS.md
    decisions/        # future standalone ADRs
  src/
  include/
  assets/
    source/
    export/
    generated/
    manifest/
  tools/
  test/
    fixtures/
    hardware-log.md
  build/
  dist/
  toolchain/
    README.md
  milestones/
```

`build/`, `dist/`, and `assets/generated/` are disposable unless the team intentionally checks generated assets in. Pick one generated-asset policy in M1 and document it.

## Toolchain decision

Use a pinned PVSnesLib release. It supplies the 65C816 compiler/linker, SNES library, asset tools, examples, controller support, and ROM build path. C is the default language because it gets the POC playable quickly; hardware-facing wrappers keep a later optimization or library change contained.

The initial repository commit records:

- PVSnesLib version or commit;
- host operating systems supported by the team;
- exact install path environment variable expected by the Makefile;
- commands for clean, debug, release, and hardware-test builds;
- emulator launch/debug steps;
- asset converter versions;
- produced ROM name and where it appears.

Do not commit a proprietary commercial ROM, Nintendo SDK material, emulator BIOS, or copyrighted reference-game assets.

## Minimum build targets

| Target | Result |
|---|---|
| `all` | Debug ROM for normal development |
| `release` | Diagnostics-disabled ROM |
| `hardware-test` | Test screen and build ID enabled |
| `assets` | Regenerate binary graphics and manifest report |
| `check` | Host checks plus asset validation |
| `clean` | Remove only known generated paths |
| `dist` | Release ROM, checksum text, map/symbol file, build report |

The specific command syntax may follow PVSnesLib conventions, but these named outcomes must exist.

## First commit sequence

1. Copy this documentation pack into the repository.
2. Add license and contribution policy.
3. Pin the toolchain and compile an untouched minimal sample.
4. Replace the sample with project boot/main loop.
5. Add build ID and hardware-test screen.
6. Add court constants and placeholder assets.
7. Begin M1 tasks in order.

## Coding conventions

- C filenames and symbols: `lower_snake_case`.
- Types include unit when ambiguous: `x_fp`, `timer_frames`, `speed_fp`.
- No magic coordinates outside court/animation data.
- No heap allocation.
- No recursion.
- Explicit-width SNES/PVSnesLib integer types at hardware and serialized boundaries.
- State transitions occur in one owning module.
- Functions that mutate state use verb-first names.
- Hardware/library calls are wrapped by input/render/platform modules.
- Warnings are treated as defects; new warnings are not accepted.

## Commit and checklist rule

Use a task ID in commits, for example `M2-BALL-03: add swept net crossing`. The commit that completes the acceptance checks also changes the relevant box from `[ ]` to `[x]`. If later regression invalidates it, reopen the box and link the defect in the task note.

Recommended commit size: one task or one inseparable group of adjacent tasks. A task is complete only when its code, checks, and documentation agree.

## Definition of done for any implementation task

- Acceptance checks on the milestone sheet pass.
- Debug and release builds compile from a clean checkout.
- No new compiler warnings.
- Relevant deterministic/emulator test added or updated.
- No asset, OAM, VRAM, or frame budget is silently exceeded.
- Documentation reflects changed behavior.
- Task checkbox is marked in the same commit.

## Versioning

- `0.1.0`: M1 bootable court.
- `0.2.0`: M2 playable rally.
- `0.3.0`: M3 complete POC/hardware candidate.

Development builds append configuration and commit ID. Do not call a build `0.3.0` until every M3 exit criterion is checked.
