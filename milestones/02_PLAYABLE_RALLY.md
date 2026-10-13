# M2 — Playable Rally

Target version: **0.2.0**  
Outcome: one human and one CPU can complete a serve and rally ending in one correctly awarded point.

## Ball and collision

- [x] `M2-BALL-01` Add deterministic 8.8 fixed-point ball state and tunable constants.
  - Accept: identical input sequence produces identical per-frame ball state after reset.
- [x] `M2-BALL-02` Implement x/y/z flight, gravity, bounce, ball object, and ground shadow.
  - Accept: shadow stays on ground; ball visibly separates by z; bounce fires once per ground crossing.
- [x] `M2-BALL-03` Implement swept net-plane crossing and height test.
  - Accept: PHY-01 and PHY-02 pass at the one-unit boundary.
- [x] `M2-BALL-04` Implement inclusive court line and out tests at ground contact.
  - Accept: PHY-03 and PHY-04 pass on every boundary.
- [x] `M2-HIT-01` Implement player windup/active/recovery state machine.
  - Accept: state durations match spec; movement penalty applies; no retrigger during recovery.
- [x] `M2-HIT-02` Implement one-contact-per-swing hit volume.
  - Accept: HIT-01 and HIT-02 pass; an overlapping ball cannot be hit repeatedly.
- [x] `M2-HIT-03` Implement directional target selection and deterministic outgoing velocity.
  - Accept: neutral/up/down/deep/short inputs land in their intended legal lanes from fixtures.

## Serve, rules, and CPU

- [x] `M2-SRV-01` Implement serve setup, toss, contact, and diagonal service-box validation.
  - Accept: P1 can begin with B; legal serve reaches opposite diagonal box; wrong box/kitchen/net are faults.
- [ ] `M2-RULE-01` Implement rally state and one-point resolution.
  - Accept: net, out, and second bounce identify one winner and stop live simulation.
- [ ] `M2-RULE-02` Implement required serve bounce and required return bounce.
  - Accept: RUL-01 and RUL-02 pass; debug HUD exposes remaining required bounce state.
- [ ] `M2-AI-01` Activate P3 as CPU through `PlayerIntent`.
  - Accept: human and CPU movement share the same player update path.
- [ ] `M2-AI-02` Add capped landing prediction and readable recovery position.
  - Accept: prediction loop caps at 60 steps; CPU does not cross bounds or hang.
- [ ] `M2-AI-03` Add windup-aware CPU swing decision.
  - Accept: CPU returns a standard center-lane fixture at least 8 of 10 times with no physics cheating.

## Presentation and asset contract

- [ ] `M2-ASSET-01` Add manifest-driven player frame and palette metadata.
  - Accept: code uses named frame offsets; invalid frame dimension/order fails asset validation.
- [ ] `M2-ASSET-02` Replace geometry with temporary but contract-correct indexed player/ball/shadow art.
  - Accept: 32×32/8×8 dimensions, index-0 transparency, fixed anchors, and horizontal flip all pass review.
- [ ] `M2-ASSET-03` Prove P1/P3 palette reuse.
  - Accept: two different visible player palettes reference byte-identical player tiles.
- [ ] `M2-FX-01` Add hit stop, contact flash, and point/fault banner.
  - Accept: state remains deterministic; each cause is legible without audio.
- [ ] `M2-HUD-01` Display score placeholder and `ARCADE RULES` label.
  - Accept: point result is understandable to a new tester.

## Verification

- [ ] `M2-TEST-01` Automate PHY-01..05, HIT-01..02, and RUL-01..03 fixtures.
  - Accept: all pass from a clean `check` run or deterministic debug harness.
- [ ] `M2-TEST-02` Add frame-overrun latch and exercise worst normal rally.
  - Accept: latch remains clear for two complete emulator matches.
- [ ] `M2-TEST-03` Run two-emulator rally smoke and one real-hardware rally session.
  - Accept: five points total are played without crash, corruption, stuck input, or sprite dropout.

## Exit criteria

- [ ] All required tasks above are complete.
- [ ] A new player can serve and complete a rally against CPU using D-pad + B.
- [ ] Net, in/out, required bounces, and point ownership are deterministic.
- [ ] Ball remains readable on real hardware.
- [ ] Tag/version `0.2.0` is ready.
