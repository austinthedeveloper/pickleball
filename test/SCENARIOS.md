# Deterministic Scenario Results

Maps every scenario in `docs/05_TEST_PLAN.md` to the compile-time fixture that
automates it. All of them run in `make check` (host build of the same C
sources the ROM uses).

| ID | Fixture | Result |
|---|---|---|
| PHY-01 | `test_ball.c: test_swept_net_height_boundaries`, `test_net_fault_event` (one unit above) | PASS |
| PHY-02 | same (one unit below → `NET`) | PASS |
| PHY-03 | `test_ball.c: test_inclusive_court_boundaries` (lines are in) | PASS |
| PHY-04 | same (one unit outside → out) | PASS |
| PHY-05 | `test_ball.c: test_single_bounce_event`, `test_fast_ball_eventually_settles` | PASS |
| HIT-01 | `test_collision.c: test_active_overlap_contacts_once` | PASS |
| HIT-02 | `test_collision.c: test_windup_cannot_contact` | PASS |
| RUL-01 | `test_rules.c: test_legal_required_bounce_sequence` | PASS |
| RUL-02 | `test_rules.c: test_return_volley_before_bounce_loses_point` | PASS |
| RUL-03 | `test_rules.c: test_second_bounce_penalizes_court_team` | PASS |
| SCR-01 | `test_match_flow.c: test_first_to_five` | PASS |
| RST-01 | `test_match_flow.c: test_rematch_matches_fresh_state` (20 rematches) | PASS |
| PROJ-01..03 | `test_projection.c` and `tools/court_projection.py` | PASS |
| PROJ-04 | `tools/generate_assets.py` line check, plus a failing fixture in `tools/test_asset_validation.py` | PASS |
| PROJ-05 | Emulator visual check: the ball shadow lands on painted lines | PASS (MesenCE smoke) |
| SAV-01 | `test_save.c`: blank SRAM formats, newer copy wins, torn copy falls back, version resets, sequence wraps (ADR-021) | PASS |
| SAV-02 | `test_save.c: test_records`, `test_game_hooks`: record rules, once per human match, never in attract | PASS |
| CRT-01 | `test_court_style.c`: DEFAULT equals the painted palette, presets touch only their part, editor wrap/cancel/keep (ADR-022) | PASS |
| CRT-02 | Asset build: line and ball contrast on every surface (`tools/assetgen/court_colors.py`) | PASS |
| CRT-03 | `test/smoke/court.timeline`: live preview, keep, cancel; persists into the next boot (`KEEP_SAVE=1`) and a match | PASS (MesenCE smoke) |
| OPP-01 | `test_opponents.c`: default style = shipped constants, config/looks/styles, Player Select takes the rung (ADR-023) | PASS |
| OPP-02 | `test_opponents.c: test_ladder`: advance on a win, stay on a loss, wrap, saved, never in attract | PASS |
| OPP-03 | `test_opponents.c: test_difficulty_order`: the first rung is beaten clearly more than the last | PASS |
| SAV-03 | `test/smoke/records.timeline`: records page, erase; persistence across MesenCE runs with `KEEP_SAVE=1` (`.srm`) | PASS (MesenCE smoke) |

## Candidate

- Build: `0.3.0-release` on branch `feature/cld-milestone-3`
- Host compiler: Apple clang (`cc -std=c99 -Wall -Wextra -Werror`)
- Recorded 2026-10-12. Re-run with `make check`; any failure fails the build.
