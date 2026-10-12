# M3 — Complete Hardware POC

Target version: **0.3.0**  
Outcome: a complete first-to-five arcade match and rematch is certified on real NTSC hardware.

## Match loop

- [ ] `M3-MATCH-01` Implement rally scoring, first to 5, and point-winner serves.
  - Accept: every fault path awards exactly one point; SCR-01 passes.
- [ ] `M3-MATCH-02` Add point result, next-serve setup, and match result transitions.
  - Accept: state timers and B-to-advance behavior match the gameplay spec.
- [ ] `M3-MATCH-03` Add rematch reset without console reset.
  - Accept: RST-01 passes and 20 consecutive rematches do not accumulate state.
- [ ] `M3-MATCH-04` Tune CPU and core constants for a two-to-four-minute typical match.
  - Accept: three novice test matches finish; CPU can return but remains beatable; final constants are documented.

## Reusable asset pipeline

- [ ] `M3-ASSET-01` Make asset conversion non-interactive and part of `assets`/normal builds.
  - Accept: clean regeneration succeeds with pinned tools.
- [ ] `M3-ASSET-02` Validate indexed color roles, dimensions, frame order, anchors, and tile budget.
  - Accept: deliberate invalid fixtures fail with useful messages.
- [ ] `M3-ASSET-03` Export four player palettes against one player tile set.
  - Accept: stress scene cycles four clearly distinguishable variants without duplicate graphics bytes.
- [ ] `M3-ASSET-04` Produce asset manifest/build report.
  - Accept: report lists each generated file’s byte size, tiles/frames, palettes, and total OBJ budget.
- [ ] `M3-ASSET-05` Verify clean-build reproducibility.
  - Accept: two clean builds from identical sources produce identical generated asset hashes and ROM hash, or documented non-gameplay metadata is the only difference.

## Four-player-ready seams

- [ ] `M3-IN-01` Add boot-time Multitap detection through the input wrapper.
  - Accept: hardware-test screen shows detected/not detected; standard port-2 pad still works when absent.
- [ ] `M3-IN-02` Validate all four logical player intents with injected pad fixtures.
  - Accept: simultaneous directions and hit edges remain independent across four slots.
- [ ] `M3-OBJ-01` Implement four-player object stress scene.
  - Accept: 4 bodies + 4 shadows + ball + ball shadow + 4 effects display on shared rows without dropout.
- [ ] `M3-OBJ-02` Record OAM order and worst-row 26-sliver plan beside the stress test.
  - Accept: effects cannot consume unreserved object IDs in review/build assertions.
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
