# Refactor Plan

Status: **In progress** (branch `feature/cld-polish`)

The POC grew quickly, much of it while fighting 816-tcc's code generation
for performance. These refactors make the code easier to change before the
vertical slice (doubles, Player Select, audio). None of them may change
gameplay.

## Ground rules

1. **Behavior-preserving.** Each refactor keeps `make check` green and
   leaves a recorded golden emulator trace byte-identical (R0). A ROM hash
   may change because code moves; simulation state may not.
2. **No CPU-time regressions.** Compare `make release PROFILE=1` stage
   averages before and after any change that touches per-frame code. Hot
   paths keep the 816-tcc rules from `02_TECHNICAL_PLAN.md`: static scratch
   structs, constant indices, no 32-bit math.
3. **One refactor per commit**, with the R-number in the subject.

## Items

| ID | Refactor | Why | Priority | Status |
|---|---|---|---|---|
| R0 | Golden trace tool: record per-frame simulation state from a scripted MesenCE run and diff it after each change | Makes every later item provably behavior-preserving on the real ROM, not just in host tests | High | Done |
| R1 | Promote the scratchpad tools into the repo: `tools/mesen_probe.py` (symbol-resolved memory probes) and `tools/match_stats.c` (bot-vs-CPU balance stats) | Profiling and CPU tuning were only possible from a private scratchpad | High | Done |
| R2 | Split `src/render.c` (~750 lines) into `render.c` (OAM/sprites/palettes), `hud_scene.c` (score panels and scene overlays), and `diagnostics.c` (debug overlay, stress scene) | One file mixes three concerns; the HUD state machine is hard to follow | High | Done |
| R3 | Move scanline/overrun/profiler code from `main.c` to `src/profile.c` | `main.c` should read as the frame pipeline from `02_TECHNICAL_PLAN.md` | Medium | Done |
| R4 | One shared aim → target function for serves and rally shots, defined for the left team and mirrored for the right (the court is symmetric about x = 128) | `serve.c` and `collision.c` each repeat team-mirrored deep/neutral/short branches | Medium | Done |
| R5 | Name the HUD scene keys (`0x80` pause, `0x40` title) and the HUD layout rows and columns | Magic numbers in the HUD code | Medium | Open |
| R6 | Court-side helpers in `court.h` (`court_side_of_x`, `court_toward_net`) used by AI, rules, and collision | Three private copies of "which side is this x on" | Medium | Done |
| R7 | Shared host-test support header (`test/test_support.h`: frame stepping, bot P1, snapshots) | `test_ai.c`, `test_match_flow.c`, and `test_serve.c` each reimplement the frame loop | Low | Open |
| R8 | Split `tools/generate_assets.py` into modules (PNG I/O, scene, HUD font, OBJ/palettes) | 600+ lines in one file; the font tables dominate it | Low | Open |
| R9 | Move presentation timers (`hit_stop`, `spark_*`) out of `GameState` into an FX struct | Match state should hold only rules state; simplifies RST-01 snapshots | Low | Open |
| R10 | Doubles readiness: drop the hard-coded opponent slot in `ai_choose_aim` and the slot 0/2 assumptions in serve/render | Required before activating slots 1 and 3 | Vertical slice | Open |

## Verification recipe

```sh
make check            # host tests + asset fixtures
make golden-record    # once, before a refactor (build/golden/trace.txt)
make golden-compare   # after; must report "identical"
make stats            # bot-vs-CPU balance; LATE=n MATCHES=n
make release PROFILE=1  # per-stage timing (probe g_stage_scanline_sum)
```

The golden trace records game, player, ball, serve, and AI state on every
frame of `test/smoke/golden.timeline`: about 4,200 frames, 18 points, and 3
match results with rematches. Frames 0–1 (MesenCE's randomized power-on RAM)
and `frame_count` are excluded. Everything else is byte-identical across
runs, which also confirms that no game state depends on uninitialized RAM.
The scripted P1 never wins a point, so the P1-scoring paths are covered by
the host tests rather than the trace.
