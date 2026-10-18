# Refactor Plan

Status: **R0–R11, R15–R20, R22, R23, R26 done (R16–R23 merged from `feature/cld-optimize`); R12–R14, R25 planned; cleanup round R27+ on `feature/cld-cleanup`**

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
| R12 | Split `src/hud_scene.c` (1,064 lines) into the match HUD (score panels, banners, pause menu) and the title (logo, menu, COURT/OPTIONS/PRACTICE/RECORDS pages); move title input (`update_title*`, ~120 lines of `main.c`) into a host-tested `title.c` | The title grew a page per feature; its input is the only menu logic without host tests | Medium | Partly done: `hud_scene.c` (2,242 lines) is now `hud_scene.c` (match HUD, story cards, pause, the staged steps, the title logo and prompt), `hud_title.c` (menu, join counter, RECORDS, the area's part drawing), `hud_settings.c`, and `hud_career.c`, sharing `src/hud_internal.h`; page headers centre through `hud_page_header` (same columns; MODIFIERS keeps its one-cell-right column). `hud.c`'s never-built `HOST_TEST` branches are gone (it needs `snes.h` and the assembly fills). Left: title input (`update_title*` in `main.c`) into a host-tested `title.c`; it drives render previews, fades, and the ambience directly, so it needs an action-result interface first |
| R13 | One settings-page component: rows of (label, value names, count) with Up/Down rows, Left/Right wrapping values, B/Start keep, Y restore, and one HUD row renderer | `court_edit_input`, `options_edit_input`, and `practice_edit_input` repeat the same editor; their HUD pages repeat the row drawing | Medium | Done: `menu_edit_keys` (`src/menu.c`, `test_menu.c`) is the four editors' keys; the HUD's four pages are one `hud_settings_*` component with one row cache. Each editor keeps its own restore and changed test (their values have different shapes), and OPTIONS its camera packing and points table |
| R14 | One frame-budget helper: "defer if past scanline N, at most M frames in a row" | The same wait logic is written three times: `hud_title_wait` (hud_scene.c), `render_update_ground` (render.c), and `audio_update` (audio_events.c) | Low | Planned |
| R15 | Doubles attract demo budget (`M4-UI-03`): bring its ~197 of 225 scanlines under budget (e.g. stagger the four AIs' decisions across frames) and set its `test/perf.json` budget to 0 | The only timeline allowed overruns (about 1 frame in 30–45) | Optional | Done: 160 → 0 overruns in the doubles demo (see R15 notes) |
| R16 | `make perf` prints each timeline's worst frame (`g_work_scanline_max`, of 225) | Overruns alone hide headroom: a saving or a cost shows only when it crosses the line | High | Done |
| R17 | OBJ name table 2 (VRAM word `0x7000`, 256 tiles, free) for player frames past sheet row 3; the renderer's frame tables and the animation loop lengths generated from the manifest | Character models need frames: table 1 is 249 of 256 tiles, and every frame was hand-listed in `render_bodies.asm` | High | Done |
| R18 | VBlank headroom: the scanline where `render_commit`'s uploads end, its worst in `make perf` | Streaming frames into VRAM (a character-model option) needs the free VBlank measured first | High | Done: VBlank used 23–32 → 13–22 of 37 lines (see R16–R22 notes) |
| R19 | 1 MiB ROM (32 banks) | 114 KiB free; character frames, body sets, and venues need room. The user allows a bigger cart | Medium | Done: 1 MiB, ~624 KiB free; `build_report.py` checks the header size byte |
| R20 | `oam_write`: high-table bits from a table, not a shift loop | ~280 cycles an object; better models add objects per player | High | Done |
| R21 | No per-frame hides of objects already hidden (debug marker in release, spark, inactive ball) | ~2–3 scanlines a frame for nothing | Medium | Dropped: after R20 a hide costs under a scanline, and other scenes (select, park, the stress scene) reuse those OAM entries, so per-entry hidden flags would be fragile |
| R22 | Small 816-tcc fixes on per-frame and hit-frame paths: multiplies by ±1 and by the player size (`ai_choose_aim`, `ai_plan_intercept`, `ai_opponent_at_net`, `rules`' last hitter team), the collision loop by pointer | Each a `tcc__mul` (16 iterations for a negative factor) | Medium | Done |
| R23 | The swing lookahead skips its probe when the ball's x/y range over the window cannot reach the swing box | The lookahead is the largest single AI cost (R15 notes) | Medium | Done (steady pace only; see the notes) |
| R24 | Metasprite OAM writer: a per-frame object list per player, written in one pass | Required for multi-object character models; ~4× cheaper per object | High | Partly overtaken: `render_bodies.asm` already writes each player's objects in one pass; a general object list waits for the character models |
| R25 | `ai_think` on a static scratch copy of the player and style | ~4–6 scanlines per think, 36 pointer accesses | Low | Later |
| R26 | `oam_write_pair`: a hat's two halves in one call (one camera and bounds test, the second entry copied from the first) | Since ADR-059 every player wears a hat object, two `oam_write` calls each | Medium | Done (see the notes) |
| R27 | One wrap-step helper (`menu_prev`/`menu_next`/`menu_step`/`menu_row`, `src/menu.c`) for every menu and settings cursor | `step_value` (court) and `page_step` (career) were identical, and ~20 inline `v ? v - 1 : n - 1` copies were spread over seven files | Low | Done |
| R28 | One text module (`src/text.c`, `test_text.c`): `text_append`, `text_count` (no leading zeros), `text_count4` (four cells, right-aligned), all without division; `hud_center_text` shared from `hud.c` | Three copies of the no-division decimal formatter (`hud_count_text`, `hud_count`, `park_count`), two string appends, and two-digit `/ 10` `% 10` formatting in the career card, the museum, the park, and OPTIONS (each a `tcc__udiv` call); the museum had its own `center_text` | Low | Done. The score digits (`hud_draw_score_digits`) keep their inline loop: they run on match frames |
| R29 | The bot P1's buttons in `test/test_support.h` (`test_bot_serve`, `test_bot_match_over`, `test_bot_buttons`, `test_bot_serve_due`), with `test_blank_save` and `test_unlock_bit` | "B every 32nd serve-setup frame, B on the result, count each result once" was written into eleven bot-vs-CPU loops (`test_ai.c`, `test_opponents.c`, `test_options.c`, `test_modifiers.c`, `test_match_flow.c`, `test_doubles.c`, `test_events.c`, `test_flair.c`, `tools/match_stats.c`); the blank save and the unlock lookup were copied between the career, career page, and museum tests | Low | Done: every host test prints the same numbers, and `make stats` (singles and both doubles bots) is byte-identical |
| R30 | One player-palette composer, `match_compose_look`, with named `PAL_SLOT_*` entries (outline 1, skin 2-4, team 5-10, paddle 11-13, highlight 14, hair 15) | `career_compose_with` and `match_compose_parts` were the same loop with magic indices | Low | Done |
| R31 | `enter_scene` in `main.c`: open a scene's screen, set the scene, resync the overrun latch (the match screen also clears `g_match_config_changed`) | `render_open_match(); s_scene = SCENE_MATCH; profile_resync();` was written five times, and the museum, select, and park openings the same way | Low | Done |

### R16–R22 notes

Release build, `make perf` (worst work scanline of 225 / worst VBlank lines
used of 37), `main` → `feature/cld-optimize` (R16–R23):

| Timeline | Worst frame | VBlank used |
|---|---:|---:|
| idle | 146 → 140 | 23 → 13 |
| golden | 199 → 199 | 24 → 14 |
| doubles_ladder | 192 → 180 | 24 → 14 |
| modifiers (singles) | 193 → 182 | 24 → 14 |
| modifiers_doubles | 197 → 190 | 24 → 14 |
| options | 178 → 163 | 24 → 14 |
| records / career | 210 / 203 → 208 / 196 | 32 → 22 |
| clubhouse | 199 → 194 | 23 → 13 |
| open_play | 219 → 219 | 23 → 13 |
| demo | 191 → 189 | 28 → 18 |

The worst frame is mostly a one-off (a page draw, a scene step); averages
moved more. Profiler build (`PROFILE=1`), `doubles_ladder.timeline` to frame
2,390, stage sums and worst frames:

| Stage | Sum | Worst |
|---|---:|---:|
| 0: intents and the CPUs' thinks | 22,602 → 18,654 (−17%) | 22 → 22 |
| 2: player bodies (`render_bodies`) | 16,507 → 12,063 (−27%) | 59 → 37 |
| 3: ball, effects, machine, ground | 35,483 → 34,910 | 29 → 27 |

The open play park (219) is now the tightest timeline. Its frames are the
park's own one-heavy-job-per-frame draws, which the character models don't
touch.

- **VBlank (R18).** The wait for VBlank returns within 5 lines; `render_commit`
  then took up to 20 more, almost all in `hud_commit`: each band went through
  `dmaCopyVram` with two `tcc__mul` calls for its offsets, about four lines a
  band for 256 bytes (1.5 lines of DMA). `src/asm/hud_commit.asm` programs
  DMA channel 0 directly. The spare VBlank (about 15 lines in the worst
  case) is what a streamed character frame would use: 512 bytes a player is
  about 3 lines of DMA, so four players fit only with an uploader like this
  one, and not on a frame that also sends three HUD bands.
- **OAM (R20).** The high-table pair is placed from two small tables, with
  8-bit index registers, instead of a shift loop of up to three passes
  (the first table version, in 16-bit, was slower than the loop for
  entries with `id & 3 == 0`, the ball among them).
- **Lookahead (R23).** Over the window's seven steps the ball's x and y stay
  between where it is and where seven steps of its velocity carry it (a
  bounce shrinks |v| without flipping it); when that range misses the reach
  box the probe is skipped. Steady pace only: a first version also summed a
  paced ball's steps through a helper with six arguments, and FAST BALL
  doubles' worst frame went from 197 to 213, because a paced window rarely
  misses. `make stats` is identical in seven configurations (singles, both
  doubles bots, KID, EASY, 70% and 120% ball speed).
- **Multiplies (R22).** `court_aim_depth`, the spark lead, `ai_choose_aim`'s
  depth, the AI's stand-behind offset and opponent pointers, `rules`' last
  hitter and server team, and the collision loop. `make stats` is identical
  for singles and both doubles bots (`LATE=0 MATCHES=20 OPPONENT=-2`), and
  the golden trace is identical.
- **Checked on the way:** emulator traces of doubles and practice timelines
  are not repeatable run to run even on one ROM (MesenCE's randomized
  power-on state reaches the attract demo, and the timelines start from
  boot), so `make stats` and the golden trace (which skips boot) are the
  identity checks. Screenshots differ only where the base ROM also differs
  from itself, except a records frame where the new ROM shows the menu's
  finished cursor a frame sooner.

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

Third pass (`feature/cld-frame-headroom`, KNOWN_ISSUES #22). Doubles with
all four modifiers dropped a frame as a point ended: that loop ran to 223-226
of 225 scanlines (four CPUs thinking with the ball still live, the second
bounce, the point, the score). Measured with temporary scanline stamps in a
release build (the profiler build's own marks overran), then:

| Change | Saved on that loop |
|---|---:|
| The swing lookahead's first three frames as one exact block for a paced ball too (FAST BALL, the slow difficulties), as at a steady pace | ~3 |
| The lookahead's ball step in assembly for its common cases (airborne, rising, resting; `src/asm/ai_probe.asm`); a landing falls back to the C | ~2 |
| The LAVA check by constant slot, not a pointer walk and three-argument calls | ~8 |
| The paddle collision only when a swing is in its active frames (most frames none is) | ~6 |

Every change keeps the simulation identical: eight full-match host traces
(singles and doubles, every difficulty, FAST BALL, LAVA, all modifiers) hash
the same before and after, and the golden trace is identical (the probe
runs in every swing check, so it also checks the assembly against the C).
`modifiers_doubles.timeline` worst frame 221 → 209 with no overrun at any
input alignment from -2 to +2 frames (one alignment dropped a frame in
v0.13.0 and v0.14.0). A forced sound handover is never started past
scanline 190 either: it takes 60-110 scanlines.

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

### R26 notes

Every player draws a hat since ADR-059 (the cap is hat 0), which raised the
worst frames by up to 13 scanlines (the doubles demo to 222 of 225). The
pair writer (`src/asm/oam_write.asm`) places both halves exactly as two
`oam_write` calls did, parking the second at x 256 and up as `oam_write`
does (an object at x 256 counts toward the PPU's per-line range). Checked
on the debug ROM by probing OAM over a doubles match with the camera
pushed past both screen edges: 230 hats on screen, every one at its
frame's accessory offset, its second half 8 px right with tile ^ 1.

Profiler build, `doubles_ladder.timeline` to frame 460 (four hats):
`render_bodies` 11,690 → 11,095 scanlines (−5%, about 1.3 a frame).
`make perf` worst frames: demo 222 → 220, doubles ladder 192 → 189,
modifiers doubles 206 → 204. Smaller than hoped: each half still updates
the high table on its own. Merging the two (the pair shares a high-table
byte for three of the four players' OAM groups) is the next saving, with
R24's metasprite writer.
