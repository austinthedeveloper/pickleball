# M3 — Complete Hardware POC

Target version: **0.3.0**  
Outcome: a complete first-to-five arcade match and rematch is certified on real NTSC hardware.

## Match loop

- [x] `M3-MATCH-01` Implement rally scoring, first to 5, and point-winner serves.
  - Accept: every fault path awards exactly one point; SCR-01 passes.
  - Verified: `test/test_match_flow.c` SCR-01 (first to 5 reaches the match result; winner serves via `game_begin_serve`).
- [x] `M3-MATCH-02` Add point result, next-serve setup, and match result transitions.
  - Accept: state timers and B-to-advance behavior match the gameplay spec.
  - Verified: result timers (20-frame minimum, 90-frame auto-advance, B to skip) and MATCH_RESULT waiting for B are covered in `test_first_to_five`.
- [x] `M3-MATCH-03` Add rematch reset without console reset.
  - Accept: RST-01 passes and 20 consecutive rematches do not accumulate state.
  - Verified: RST-01 compares every simulation struct against a fresh match after 20 consecutive bot-played rematches.
- [ ] `M3-MATCH-04` Tune CPU and core constants for a two-to-four-minute typical match.
  - Accept: three novice test matches finish; CPU can return but remains beatable; final constants are documented.
  - Progress: CPU difficulty is deterministic (late-shot period 8, contact timing sweet spot; see `docs/01_GAMEPLAY_SPEC.md`). Simulated bots that mistime every 2nd shot win 1/20 matches; every 10th, 19/20 (`test_cpu_is_beatable_but_not_trivial`). Simulated matches run 45–85 s; three human novice matches still required.

## Reusable asset pipeline

- [x] `M3-ASSET-01` Make asset conversion non-interactive and part of `assets`/normal builds.
  - Accept: clean regeneration succeeds with pinned tools.
  - Verified: `make` runs `tools/generate_assets.py` (stdlib only) before every build; Pillow is needed only for the authoring tools under `tools/art/`.
- [ ] `M3-ASSET-02` Validate indexed color roles, dimensions, frame order, anchors, and tile budget.
  - Accept: deliberate invalid fixtures fail with useful messages.
- [x] `M3-ASSET-03` Export four player palettes against one player tile set.
  - Accept: stress scene cycles four clearly distinguishable variants without duplicate graphics bytes.
  - Verified: the stress scene composes all four palettes from parts onto OBJ palettes 0-3 and rotates them over the single player tile set.
- [ ] `M3-ASSET-04` Produce asset manifest/build report.
  - Accept: report lists each generated file’s byte size, tiles/frames, palettes, and total OBJ budget.
- [ ] `M3-ASSET-05` Verify clean-build reproducibility.
  - Accept: two clean builds from identical sources produce identical generated asset hashes and ROM hash, or documented non-gameplay metadata is the only difference.

## Four-player-ready seams

- [x] `M3-IN-01` Add boot-time Multitap detection through the input wrapper.
  - Accept: hardware-test screen shows detected/not detected; standard port-2 pad still works when absent.
  - Verified (MesenCE, no Multitap): `detectMPlay5()` at boot; pause screen of debug/hardware-test builds shows `MULTITAP: NO`; port-2 pad still reads (`test_poc_ownership_without_multitap`).
- [x] `M3-IN-02` Validate all four logical player intents with injected pad fixtures.
  - Accept: simultaneous directions and hit edges remain independent across four slots.
  - Verified: `test/test_input.c` drives four human slots from injected pad words.
- [x] `M3-OBJ-01` Implement four-player object stress scene.
  - Accept: 4 bodies + 4 shadows + ball + ball shadow + 4 effects display on shared rows without dropout.
  - Verified (MesenCE): Select in debug/hardware-test builds opens the stress scene; the PPU range/time-over flags ($213E) stayed clear (`OBJ OVERFLOW: NO`). Real-hardware run pending under M3-QA-03.
- [x] `M3-OBJ-02` Record OAM order and worst-row 30-sliver plan beside the stress test.
  - Accept: effects cannot consume unreserved object IDs in review/build assertions.
  - Verified: row plan in the comment above `render_stress()` in `src/render.c`; compile-time checks there fail the build if OAM ranges overlap.
- [ ] `M3-IN-03` **OPTIONAL if hardware unavailable:** Validate physical Super Multitap detection and four pads.
  - Accept: each of four game slots displays independent held/pressed state on a real console.

## Robustness and packaging

- [ ] `M3-QA-01` Complete all deterministic scenarios in the test plan.
  - Accept: results are recorded against the candidate build ID.
- [ ] `M3-QA-02` Run two complete matches on each of two accuracy-focused emulator families.
  - Accept: no blocker/major defects and frame-overrun latch remains clear.
- [ ] `M3-QA-03` Run NTSC real-hardware certification.
  - Accept: 15-minute soak, at least 3 matches, reset, pause, rematch, and cold boot pass.
- [ ] `M3-QA-04` Review object, VRAM, ROM, stack/static-RAM, and generated-asset budgets.
  - Accept: actual values appear in `dist/build-report.md`; no overlap/overflow warning.
- [ ] `M3-QA-05` Confirm internal ROM header and checksum/complement.
  - Accept: ROM inspection tool and hardware both recognize and boot the image.
- [ ] `M3-QA-06` Close or document every defect.
  - Accept: zero blockers/majors; every accepted minor has reproduction and reason to defer.
- [ ] `M3-DIST-01` Produce distribution bundle.
  - Accept: versioned ROM, checksum, build report, controls/rules readme, source revision, and hardware log are present.

## Exit criteria

- [ ] All required tasks above are complete.
- [ ] POC success test in the root README passes with a new player.
- [ ] One tile set visibly supports all four player palettes.
- [ ] Four-player input/entity/OAM seams are demonstrated even though gameplay remains 1P vs CPU.
- [ ] Candidate passes two emulator families and an NTSC real SNES/SFC.
- [ ] Master cross-cutting gates are complete.
- [ ] Tag/version `0.3.0` is ready.

## Handoff after M3

The next planning pass decides whether to build fuller pickleball rules, two-player versus, or full four-player doubles first. It must not begin until M3 exposes measured frame, VRAM, OAM, and ROM headroom.
