# Technical Plan

Status: **Implementation baseline**

## Platform profile

| Area | Decision |
|---|---|
| Console | Unmodified NTSC SNES/Super Famicom |
| Toolchain | PVSnesLib, pinned release, C with optional isolated 65C816 assembly |
| ROM | LoROM, no enhancement chip, no SRAM in POC |
| Video | 256×224, Mode 1, non-interlaced |
| Timing | Fixed 60 Hz simulation target; one VBlank-synchronized presentation per frame |
| BG1 | 4bpp court tiles and markings |
| BG2 | 4bpp optional overlays/landing marker; disabled if unused |
| BG3 | 2bpp HUD and debug text |
| Objects | Object size selection 8×8 small / 32×32 large |
| Input | Auto-read pads through PVSnesLib; Multitap detection isolated behind input API |
| Audio | Event API stub only through M3 |

Pin an exact PVSnesLib release or commit in `toolchain/README.md` and CI/container configuration. Never silently build against “latest.”

## Frame pipeline

The order is fixed so behavior remains deterministic:

1. Copy current input snapshot and derive pressed/released edges.
2. Handle pause and top-level game-state transitions.
3. Update human/CPU intent.
4. Update player state and movement.
5. Update serve or live ball physics.
6. Resolve ball/player, net, ground, and court events.
7. Apply score/state transitions.
8. Select animation frames and build shadow/player/ball OAM entries.
9. Queue only required VRAM/CGRAM/OAM work.
10. Wait for VBlank and commit presentation work.

No gameplay mutation occurs from rendering functions.

## Proposed source modules

| Path | Responsibility |
|---|---|
| `src/main.c` | Boot and fixed frame loop |
| `src/game.c/.h` | Top-level game/rally state |
| `src/court.c/.h` | Authoritative geometry and in/out/service-box tests |
| `src/input.c/.h` | Logical slots, pad edges, CPU ownership, Multitap mapping |
| `src/player.c/.h` | Movement, swing state, animation selection |
| `src/ball.c/.h` | 3-axis fixed-point physics and swept crossings |
| `src/collision.c/.h` | Hit volume and contact resolution |
| `src/serve.c/.h` | Serve setup, toss, diagonal validation |
| `src/rules.c/.h` | Two-bounce tracking, faults, scoring |
| `src/ai.c/.h` | Landing prediction and CPU intent |
| `src/render.c/.h` | Background setup and OAM construction |
| `src/hud.c/.h` | Score, state banners, debug overlay |
| `src/audio_events.c/.h` | No-op POC event seam |
| `src/debug.c/.h` | Assertions, counters, visualization flags |

Keep module globals private. Pass compact structures or IDs; do not allocate dynamically.

## Data model

Compile-time maxima are final-target maxima even in the POC:

```text
MAX_PLAYERS = 4
MAX_BALLS = 1
MAX_HUMAN_PADS = 4
```

Each player has an active flag, team, logical input slot, CPU flag, fixed-point position/velocity, facing, state, state timer, palette ID, animation/frame, and “contact already consumed” flag. POC activates player slots 0 and 2, leaving 1 and 3 inactive so doubles positions can be added without renumbering teams.

## Fixed-point and arithmetic rules

- Use signed 16-bit 8.8 values for on-court position/velocity where range permits.
- Promote intermediate multiplies/divides to signed 32-bit.
- Wrap fixed-point helpers in tested macros or inline functions.
- Avoid floating point and standard-library allocation.
- Cap predictive simulations and loops with explicit compile-time limits.
- Seedless/deterministic behavior is required through M3.

## Object budget

The hardware can hold 128 objects but only 32 objects and 34 8-pixel-wide object slivers can survive on one scanline. Design to the sliver limit.

Worst intended gameplay row with four future players aligned:

| Object | Count | Width | Slivers each | Total |
|---|---:|---:|---:|---:|
| Players | 4 | 32 px | 4 | 16 |
| Player shadows | 4 | 8 px | 1 | 4 |
| Ball | 1 | 8 px | 1 | 1 |
| Ball shadow | 1 | 8 px | 1 | 1 |
| Impact effects reserve | 4 | 8 px | 1 | 4 |
| **Worst planned total** | **14** |  |  | **26** |

That leaves eight slivers of safety. HUD and court lines are backgrounds, never objects. Debug overlays use BG3. If an effect needs more than this reserve, it must document its worst-row calculation.

Assign low OAM indices to ball and player bodies before cosmetic effects. Hide unused objects with the library’s supported hide operation; do not park them at the problematic offscreen X value `$100`.

## Graphics memory plan

Initial VRAM addresses are provisional and must be confirmed with the selected PVSnesLib release’s alignment requirements.

| Region | Budget | Content |
|---|---:|---|
| BG1 character data | 16 KiB | Court tiles |
| BG1 map | 2 KiB | 32×32 tilemap |
| BG3 character data | 4 KiB | HUD font/icons |
| BG3 map | 2 KiB | HUD/debug tilemap |
| OBJ character data | 16 KiB | Shared player frames, ball, shadows, effects |
| Reserved/free | 24 KiB | Alignment, later BG2, animation growth |

This is a planning budget, not permission to overlap regions. Record actual linker/export sizes in the build report.

## CPU/VBlank budget policy

- Target a full simulation comfortably within one NTSC frame; correctness must not depend on overclocked emulators.
- Upload static tiles and palettes during initialization with forced blank.
- During play, update OAM every frame and small HUD tilemap regions only when values change.
- Do not stream player graphics every frame in the POC. Preload all shared frames.
- Build a debug frame-overrun indicator before AI refinement.
- If performance fails, profile by subsystem before adding assembly.

## ROM and hardware invariants

- Valid internal header, checksum/complement, map mode, region, and ROM size.
- Startup initializes stack, direct page/library runtime, display, palettes, VRAM, OAM, and input before enabling the screen.
- All arrays have compile-time bounds and all OAM IDs remain within their reserved range.
- No uninitialized RAM is used as gameplay state.
- Warm reset and repeated rematch return to deterministic state.

## Build configurations

| Configuration | Purpose |
|---|---|
| `debug` | Assertions, collision boxes, predicted landing, frame-overrun flag, input display |
| `release` | No diagnostics; same simulation constants |
| `hardware-test` | Release behavior plus build ID and compact test screen |

Every ROM filename includes project, semantic version, configuration, and short commit ID where available.
