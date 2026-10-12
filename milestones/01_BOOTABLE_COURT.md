# M1 — Bootable Court

Target version: **0.1.0**  
Outcome: a stable ROM boots on real hardware and lets one player move around a correctly marked court.

## Foundation

- [x] `M1-BOOT-01` Create the repository tree, license, ignore rules, and top-level README.
  - Accept: clean checkout contains every documented authored directory; generated paths are clearly identified.
- [x] `M1-BOOT-02` Pin PVSnesLib and document host setup.
  - Accept: a second shell/session can identify the exact version and expected environment variable without oral instructions.
- [x] `M1-BOOT-03` Add `debug`, `release`, `hardware-test`, `check`, `clean`, and `dist` build outcomes.
  - Accept: clean build emits named ROM(s), map/symbol output, checksum text, and no warnings.
- [x] `M1-BOOT-04` Implement boot, forced-blank initialization, main loop, VBlank sync, and screen enable.
  - Accept: 100 resets in an emulator reach the same valid screen; no uninitialized visual garbage appears.
- [ ] `M1-BOOT-05` Add build ID/configuration to hardware-test screen.
  - Accept: tester can identify the exact ROM from an on-screen value.

## Court and rendering

- [x] `M1-COURT-01` Add one authoritative court constants module.
  - Accept: baseline, sideline, net, kitchen, centerline, spawn, and clamp coordinates exist once.
- [x] `M1-COURT-02` Create placeholder Mode 1 court and HUD palettes/tiles/map.
  - Accept: all required pickleball lines are visible and match court constants within one pixel/tile design tolerance.
- [x] `M1-COURT-03` Load static background assets during initialization.
  - Accept: no gameplay-frame bulk VRAM upload is needed to keep the court visible.
- [ ] `M1-COURT-04` Add BG3 debug text/markers without gameplay objects.
  - Accept: build ID, player coordinates, and pause state can be displayed in debug/hardware-test builds.
- [x] `M1-COURT-05` Record initial VRAM map and actual asset byte sizes.
  - Accept: regions do not overlap and total fits the plan.

## Player and input

- [x] `M1-IN-01` Capture pad state once per frame and derive held/pressed/released.
  - Accept: a held B/Start produces one pressed edge; debug view shows stable bits.
- [x] `M1-IN-02` Introduce `PlayerIntent[4]` and ownership data; activate P1 only.
  - Accept: player movement code contains no direct pad API calls.
- [x] `M1-PLY-01` Add four-slot player data with P1 active and P3 reserved for CPU.
  - Accept: all slots reset deterministically; inactive slots emit no OAM or collision.
- [x] `M1-PLY-02` Implement fixed-point acceleration, diagonal normalization, friction, and clamps.
  - Accept: P1 moves eight directions, stops, and cannot cross court/player bounds.
- [ ] `M1-PLY-03` Render P1 as one 32×32 placeholder object and one 8×8 shadow.
  - Accept: both remain stable at every court edge and after 5 minutes of movement.
- [x] `M1-PLY-04` Implement idle/run animation metadata and horizontal flip.
  - Accept: no mirrored graphics are stored; facing changes via OAM attributes.
- [x] `M1-STATE-01` Implement pause/unpause on Start.
  - Accept: positions and timers do not change while paused; 20 toggles work.

## Verification

- [x] `M1-TEST-01` Add player clamp and fixed-point host checks.
  - Accept: min/max corners, zero input, diagonals, and sign changes pass.
- [ ] `M1-TEST-02` Run fast and accuracy-emulator smoke tests.
  - Accept: boot, movement, pause, reset, and visual court checks pass in both.
- [ ] `M1-TEST-03` Run M1 real-hardware smoke and add the hardware log entry.
  - Accept: every M1 item in the test plan passes on an NTSC console.

## Exit criteria

- [ ] All required tasks above are complete.
- [ ] Debug and release ROMs build cleanly from a clean checkout.
- [ ] P1 can move and pause on a correctly lined static court on real hardware.
- [ ] No observed sprite dropout, corrupt tiles, stuck input, or frame-overrun latch.
- [ ] Tag/version `0.1.0` is ready.
