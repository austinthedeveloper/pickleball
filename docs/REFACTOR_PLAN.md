# Refactor Plan

Status: **R0–R11 done; R12–R15 planned** (R11 on `feature/cld-perf`)

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
| R5 | Name the HUD scene keys (`0x80` pause, `0x40` title) and the HUD layout rows and columns | Magic numbers in the HUD code | Medium | Done |
| R6 | Court-side helpers in `court.h` (`court_side_of_x`, `court_toward_net`) used by AI, rules, and collision | Three private copies of "which side is this x on" | Medium | Done |
| R7 | Shared host-test support header (`test/test_support.h`: frame stepping, bot P1, snapshots) | `test_ai.c`, `test_match_flow.c`, and `test_serve.c` each reimplement the frame loop | Low | Done |
| R8 | Split `tools/generate_assets.py` into modules (PNG I/O, scene, HUD font, OBJ/palettes) | 600+ lines in one file; the font tables dominate it | Low | Done |
| R9 | Move the spark out of `GameState` behind an event seam: `events_emit` feeds `fx` (spark) and an `audio_events` stub. `hit_stop` stays, because it pauses the simulation. | Match state holds only simulation state. This also delivers the spec's required hooks for serve, hit, bounce, fault, and point sounds. | Low | Done |
| R10 | Doubles readiness: drop the hard-coded opponent slot in `ai_choose_aim` and the slot 0/2 assumptions in serve/render | Required before activating slots 1 and 3 | Vertical slice | Done (ADR-018) |
| R11 | `make perf`: every timeline in `test/perf.json` on the release ROM, checking dropped frames against a budget (0 except the doubles demo), that the main loop keeps running (hangs), and that SPC700 voices sound (`tools/mesen_voices.lua`, promoted from the scratchpad) | The boot hang (`spcFlush`), the late whistle, and the doubles demo drops were each found by hand-run probes | High | Done |
| R12 | Split `src/hud_scene.c` (1,064 lines) into the match HUD (score panels, banners, pause menu) and the title (logo, menu, COURT/OPTIONS/PRACTICE/RECORDS pages); move title input (`update_title*`, ~120 lines of `main.c`) into a host-tested `title.c` | The title grew a page per feature; its input is the only menu logic without host tests | Medium | Planned |
| R13 | One settings-page component: rows of (label, value names, count) with Up/Down rows, Left/Right wrapping values, B/Start keep, Y restore, and one HUD row renderer | `court_edit_input`, `options_edit_input`, and `practice_edit_input` repeat the same editor; their HUD pages repeat the row drawing | Medium | Planned |
| R14 | One frame-budget helper: "defer if past scanline N, at most M frames in a row" | The same wait logic is written three times: `hud_title_wait` (hud_scene.c), `render_update_ground` (render.c), and `audio_update` (audio_events.c) | Low | Planned |
| R15 | Doubles attract demo budget (`M4-UI-03`): bring its ~197 of 225 scanlines under budget (e.g. stagger the four AIs' decisions across frames) and set its `test/perf.json` budget to 0 | The only timeline allowed overruns (about 1 frame in 30–45) | Optional | Done: 160 → 0 overruns in the doubles demo (see R15 notes) |

### R15 notes

First pass (`feature/cld-doubles-perf`), all results unchanged (host
outcomes, the golden trace, and a step-by-step check of the blocked
prediction in `test_ai.c`). Release build, frames 4,300-9,400 from boot
(the doubles demo), and a 2,400-frame doubles ladder match:

| Change | Demo overruns | Ladder match |
|---|---:|---:|
| Before | 160 | 17 |
| Only the ball's lane owner runs the swing lookahead | 150 | 7 |
| A server thinks every frame only while serving; the demo P1 on its own fourth frame | 103 | 5 |
| One landing prediction per flight, shared (cache keyed by shot and bounce) | 59 | 6 |
| The title prompt's rows are kept and restored on each blink, not redrawn | 18 | 6 |
| The logo in two steps; no wait for steps that draw nothing | 21 | 6 |
| The landing prediction steps 8 frames at a time (exact) | 4-9 | 0 |

- How it was found: per-frame AI counters in a scratch release build,
  lined up with `g_work_scanline`: each CPU think cost about 17 scanlines
  and a swing lookahead 35-40 more. The prompt blink and the logo were
  ~100-scanline draws that waited for a light frame the doubles demo
  rarely has, then were forced onto a heavy one.
- Left: frames where a hit, a point, or the owner's lookahead coincide
  with a second think. Four player updates (~56 scanlines) and the body
  render (~33) are the largest fixed costs; an assembly `player_step` is
  the next lever.

Second pass (`feature/cld-frame-budget`, M4-UI-01/03/04). Release build,
worst frame (work scanline, of 225) and overruns:

| Change | Doubles demo | All-modifier doubles | Menu |
|---|---:|---:|---:|
| Before | 4-12 | 7-14 | 3-6 |
| `player_step` in assembly | 0 (219) | 0 (214) | 0 |
| The step loop and teammate separation by constant slots | 0 (214) | 0 (204) | 0 (197) |
| `hud_text`'s loop in assembly | 0 (190) | 0 (208) | 0 (179) |

- **`player_step`** (`src/asm/player_step.asm`) does what the C one does;
  the host tests run the C (`#ifdef HOST_TEST` in `player.c`), so a change
  goes in both. The golden trace and a doubles trace (every player, ball,
  and AI byte, every frame of `doubles.timeline`) are identical. The
  attract demo's trace is not: its CPU schedule follows `g_loop_count`,
  and a faster scene switch loses one frame fewer.
- **`hud_text`** (`src/asm/hud_text.asm`) reads the string through the
  compiler's `tcc__r9` (`[dp],y`, any bank). The BG3 map is identical at
  every shot of nine HUD timelines.
- **Found on the way:** with lighter demo frames, a new title prompt (106
  scanlines in one step) waited out its six frames on `court.timeline` and
  dropped one. It now draws as a panel and two halves of text, like the
  match result's (`M4-UI-04`).

### R11 notes

- **Budgets:** `test/perf.json` lists each run (timeline, frames, allowed
  overruns, and optionally `min_loops`). The main loop must run on at least
  30% of frames: a hang runs ~0, and fades and screen loads skip some.
- **Sound:** the voice probe reports each SPC700 voice that starts (its
  envelope rises from 0). The practice and golden runs must start at least
  10 voices, including the ambience (voice 0, and calls on 1) and the
  effects (7).
- **First catch:** the first run found one dropped frame whenever a match
  ended back at the title or a demo gave way to the next. `game_new_match`
  takes about 180 scanlines and the first draw follows in the same frame.
  That frame is a scene switch, so the main loop now resyncs the overrun
  latch after it, as fades do (`s_switched`, main.c). The screen holds one
  frame; nothing tears.
- **Golden trace:** boot now ends near frame 10 (the sound CPU and the
  two-bank soundbank), so the trace skips 30 frames, not 2.

### R10 progress

Done: the AI aims away from the first *active* opponent rather than slot 0. The main loop runs the AI for every CPU-owned active slot (per `g_input_owners`) rather than slot 2 only.

Then completed with doubles (`feature/cld-doubles`, ADR-018): lane-based serve rotation, the doubles OBJ palette layout, teammate separation, lane homes, and per-match slot activation from the match config.

## Verification recipe

```sh
make check            # host tests + asset fixtures
make golden-record    # once, before a refactor (build/golden/trace.txt)
make golden-compare   # after; must report "identical"
make perf             # frame budget, hangs, and sound (test/perf.json)
make stats            # bot-vs-CPU balance; LATE=n MATCHES=n
make release PROFILE=1  # per-stage timing (probe g_stage_scanline_sum)
```

The golden trace records game, player, ball, serve, and AI state on every
frame of `test/smoke/golden.timeline`: about 4,200 frames, 18 points, and 3
match results with rematches. Frames 0–29 (boot, over MesenCE's randomized power-on RAM)
and `frame_count` are excluded. Everything else is byte-identical across
runs, which also confirms that no game state depends on uninitialized RAM.
The scripted P1 never wins a point, so the P1-scoring paths are covered by
the host tests rather than the trace.
