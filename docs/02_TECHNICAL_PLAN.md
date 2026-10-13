# Technical Plan

Status: **Implementation baseline**

## Platform profile

| Area | Decision |
|---|---|
| Console | Unmodified NTSC SNES/Super Famicom |
| Toolchain | PVSnesLib, pinned release, C with optional isolated 65C816 assembly |
| ROM | LoROM FastROM (map mode $30, 120 ns ROM; ADR-017), no enhancement chip, 2 KiB battery SRAM for records (ADR-021) |
| Video | 256×224, Mode 1, non-interlaced |
| Timing | Fixed 60 Hz simulation target; one VBlank-synchronized presentation per frame |
| BG1 | 4bpp court tiles and markings |
| BG2 | 4bpp optional overlays/landing marker; disabled if unused |
| BG3 | 2bpp HUD and debug text |
| Objects | Object size selection 8×8 small / 32×32 large |
| Input | Auto-read pads through PVSnesLib; Multitap detection isolated behind input API |
| Audio | SNESMOD sound effects from the event API (ADR-029); no music yet |

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
8. Project world positions to screen space, select animation frames, and build shadow/player/ball OAM entries.
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
| `src/projection.c/.h` | World-to-screen projection over the generated `court_projection.h` table (may be header-only) |
| `src/render.c/.h` | Background setup, palettes, ball/effects OAM |
| `src/asm/render_bodies.asm` | Player bodies and shadows: depth sort, projection on the hardware multiplier, OAM (four players did not fit the frame in C) |
| `src/asm/oam_write.asm`, `ram_copy.asm`, `hud_fill.asm` | OAM entry writer, MVN block copy for scratch structs, BG3 map fill |
| `src/hud.c/.h` | Score, state banners, debug overlay |
| `src/events.c/.h` | Simulation → presentation event seam (serve, hit, bounce, fault, point) |
| `src/fx.c/.h` | Event-driven presentation effects (contact spark) |
| `src/audio_events.c/.h` | Sound effects from events (SNESMOD, ADR-029); counts events and the last effect for tests |
| `src/debug.c/.h` | Assertions, counters, visualization flags |

Keep module globals private. Pass compact structures or IDs; do not allocate dynamically.

## Data model

Compile-time maxima are final-target maxima even in the POC:

```text
MAX_PLAYERS = 4
MAX_BALLS = 1
MAX_HUMAN_PADS = 4
```

Each player has an active flag, team, logical input slot, CPU flag, fixed-point position/velocity, facing, state, state timer, palette ID, animation/frame, and “contact already consumed” flag. The palette is assigned from match config, never derived from slot; uniqueness is asserted in debug builds ([ADR-010](10_MOCKUP_DECISIONS.md#adr-010--palette-decoupled-from-player-slot)). POC activates player slots 0 and 2, leaving 1 and 3 inactive so doubles positions can be added without renumbering teams.

## Fixed-point and arithmetic rules

- Use signed 16-bit 12.4 values (1/16 px) for position and velocity. Every spec tunable is exact in 12.4, and the 256-px court fits with headroom (ADR-006).
- Keep per-frame code in 16-bit math. 816-tcc compiles 32-bit arithmetic, pointer dereferences, and variable struct-array indexing very slowly; hot paths copy a struct into a static scratch copy and use constant indices. Profile with `make release PROFILE=1`.
- Wrap fixed-point helpers in tested macros or inline functions.
- Avoid floating point and standard-library allocation.
- Cap predictive simulations and loops with explicit compile-time limits.
- Seedless/deterministic behavior is required through M3.

## Object budget

The hardware can hold 128 objects but only 32 objects and 34 8-pixel-wide object slivers can survive on one scanline. Design to the sliver limit.

Worst intended gameplay row with four future players aligned ([ADR-014](10_MOCKUP_DECISIONS.md#adr-014--body-variants-via-accessory-object)):

| Object | Count | Slivers |
|---|---:|---:|
| Players | 4 | 16 |
| Player shadows | 4 | 4 |
| Accessories | 4 | 4 |
| Ball + shadow | 2 | 2 |
| Impact reserve | 4 | 4 |
| **Total** | **18** | **30 / 34** |

That leaves four slivers of safety. HUD and court lines are backgrounds, never objects. Debug overlays use BG3. If an effect needs more than this reserve, it must document its worst-row calculation.

Assign low OAM indices to ball and player bodies before cosmetic effects. Hide unused objects with the library’s supported hide operation; do not park them at the problematic offscreen X value `$100`.

## Graphics memory plan

Initial VRAM addresses are provisional and must be confirmed with the selected PVSnesLib release’s alignment requirements.

| Region | Budget | Content |
|---|---:|---|
| BG1 character data | 28 KiB (896 tiles; ADR-016 as amended) | Court and scenery tiles |
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

## Host checks

In addition to the physics, rules, and asset checks in `05_TEST_PLAN.md`, `make check` runs the projection checks from [ADR-011](10_MOCKUP_DECISIONS.md#adr-011--perspective-court-with-world-to-screen-projection):

| ID | Check | Expected |
|---|---|---|
| PROJ-01 | Host: table monotonic, `s(y)` within [0.75, 1.10] | Pass |
| PROJ-02 | Host: net x = 128 maps to 128 on every row | Pass |
| PROJ-03 | Host: every projected court corner within x 8..247 | Pass |
| PROJ-04 | Host: every line pixel in exported court art equals projected constant ±0 px | Pass |
| PROJ-05 | Emulator: PHY-03 ball landing on a line draws its shadow on the painted line | Visual pass |
