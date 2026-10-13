# Hardware Test Log

Use an unmodified NTSC SNES/Super Famicom and record console revision, flash
cartridge, ROM checksum, duration, and every result from `docs/05_TEST_PLAN.md`.

No M1 hardware run has been recorded yet.

## Emulator observations

### 2026-10-12 — MesenCE — `0.1.0-release-40fa1d2-dirty`

- Source: user-supplied screenshot of the canonical `dist/` ROM.
- Cold boot/render: PASS (single observed boot).
- Mode 1 court: PASS; baselines, sidelines, kitchen lines, centerlines, and net
  are visible and aligned with the placeholder layout.
- P1 placeholder object: PASS at the initial spawn position.
- Not established by this observation: movement at every bound, pause toggles,
  reset repetitions, five-minute soak, second-emulator accuracy pass, or real
  hardware behavior.

### 2026-10-12 — MesenCE — boundary review

- User exercised the release ROM at bottom-right, top-left, and bottom-left.
- Input and directional facing were responsive.
- DEFECT: the right clamp stopped at the kitchen line and the top/left clamps
  allowed the body outside the court. Root cause was applying top-left-style
  limits to a feet/center sprite anchor.
- FIX: clamp the complete 32×32 body between the left baseline, net, and
  sidelines; exact relationships are covered by host assertions.
- Retest required with a newly built ROM before closing `M1-PLY-03`.

### 2026-10-12 — MesenCE — corrected release and reset pass

- ROM: `0.1.0-release-228a4e0-dirty`.
- Automated 100-reset pass: PASS. Every reset completed and the final settled
  frame contained the complete court and player with no uninitialized tiles.
- Corrected top/net corner: PASS; the complete 32×32 body remained inside the
  sideline and P1 side of the net.
- Horizontal facing: PASS across the user boundary captures; the same graphics
  are flipped through the OAM horizontal-flip attribute.
- BG3 diagnostics were initially hidden behind the opaque BG1 because their map
  entries lacked the priority bit. The map entries and explicit BG enables were
  corrected; visual retest of the hardware-test profile remains required.
- Full four-corner retest and five-minute movement soak remain required before
  closing `M1-PLY-03`.

### 2026-10-12 — MesenCE — foot-anchor boundary adjustment

- User review found the containment rule too restrictive for gameplay: the top
  limit stopped the character's head at the sideline instead of the feet.
- Top limit changed to stop the bottom of the visible feet on the top sideline;
  the character artwork may extend into the surrounding runoff area.
- Bottom limit changed to permit the bottom of the visible feet approximately
  three court feet (11 pixels) beyond the bottom sideline.
- Corrected release requires a visual boundary retest before `M1-PLY-03` closes.

### 2026-10-12 — MesenCE — left-side runoff adjustment

- User review confirmed the vertical limits look correct.
- Left limit changed to permit the visible player body 22 pixels beyond the
  baseline, twice the bottom runoff allowance.
- Net-side and vertical limits remain unchanged.

### 2026-10-12 — MesenCE — left-edge sprite-wrap correction

- User testing found the player disappearing at the left edge and reappearing
  on the right, caused by a negative SNES OAM X coordinate wrapping around.
- Left baseline inset from X=16 to X=24, making both court halves 104 pixels
  wide around the net at X=128.
- The full 22-pixel left runoff remains available, while the player's visible
  left edge now stops at screen X=2 and cannot wrap.

### 2026-10-12 — hardware-test HUD build verification

- Clean commit build produced `0.1.0-HW-1c8112b` in the hardware-test ROM.
- ROM inspection confirms the exact build/configuration string and `PAUSED`
  label are embedded; renderer code places build ID, live P1 coordinates, and
  pause state on BG3 only in debug/hardware-test configurations.
- `M1-BOOT-05` and `M1-COURT-04` implementation acceptance is complete.
- Visual HUD readability and live-value behavior remain in the manual smoke
  checklist below; they are not prerequisites for rebuilding the ROM.

### 2026-10-12 — missing pause/HUD text retest

- User confirmed pause logic works but reported no visible words.
- Reopened `M1-BOOT-05` and `M1-COURT-04` pending visual confirmation.
- Release builds now display `PAUSED`; diagnostic builds additionally display
  build ID and live P1 coordinates.
- BG3 setup now follows the explicit PVSnesLib Mode 1 pattern: set BG1/BG3 map
  pointers, load their tiles, and copy both maps while the screen is blank.
- Retest found `PAUSED` remained visible after gameplay resumed. The renderer
  now consumes the authoritative game pause state directly, and clears BG3
  with explicit priority-marked space tiles so stale text is overwritten.
- A subsequent retest reported no pause text in either state. Dirty build IDs
  now include a source-diff fingerprint so each changed ROM has a unique name
  and cannot be confused with an emulator-cached build at the same path.
- Final release-path attempt stores a static `PAUSED` map in BG3 and toggles
  the entire BG3 layer with pause state. This avoids runtime text-map clearing
  and per-frame HUD DMA in release builds. If this fails visual retest, retain
  it as an open M1 HUD defect and continue other milestone work.
- PASS: user tested `0.1.0-release-1c8112b-dirty-ecd69676`. `PAUSED` is hidden
  during gameplay, appears when Start pauses the game, and disappears when
  Start resumes it. Closed `M1-BOOT-05` and `M1-COURT-04`.

### 2026-10-12 — MesenCE M1 movement acceptance

- ROM: `0.1.0-release-1c8112b-dirty-ecd69676`.
- User completed a fresh restart and manually confirmed the court, movement
  bounds, facing, and pause behavior remained correct.
- Automated MesenCE test runner supplied continuous input for 18,000 frames
  (five minutes at the 60 Hz target), dividing the run evenly across the four
  cardinal and four diagonal directions.
- Result: PASS. MesenCE reached all 18,000 frames and exited with status 0;
  there was no crash or hang. Combined with the manual boundary review, this
  closes `M1-PLY-03`.
- Diagnostic note: MesenCE reported startup reads in the compiler scratch
  register area (`$00000A–$000013` and `$00003D–$000040`). No visual corruption
  or behavioral failure was observed; revisit if clean-build determinism or a
  second emulator exposes a related symptom.

### 2026-10-12 — M1 reproducible release candidate

- Build: `0.1.0-release-a34a7d0`.
- Debug, hardware-test, and release profiles built successfully.
- Two release builds produced byte-identical 262,144-byte ROMs with SHA-256
  `2031465d6adec163681ef7f1c816e006c98fc9dfdf2d7efaefa45b3a9ed869f3`.
- Both generated asset manifests were byte-identical.
- MesenCE booted the reproduced ROM and completed a 600-frame smoke run with
  exit status 0. This closes `GATE-REPRO`.

### 2026-10-12 — M2 ball physics/render smoke

- Host checks cover deterministic duplicate simulations, update order,
  gravity, pause, inactive state, planar/vertical bounce retention, one bounce
  event per crossing, and both directions of swept net crossing.
- PHY-01/PHY-02 boundary checks pass at one 8.8 fixed-point unit above and
  below the 12-pixel net height.
- PHY-03/PHY-04 cover all four inclusive court boundaries and one 8.8
  fixed-point unit outside each boundary; out is emitted only at ground contact.
- MesenCE completed a 600-frame smoke run with exit status 0.
- Visual review confirmed a contract-sized 8×8 ball and independent 8×8
  ground shadow. An initial OBJ-priority defect behind the court layer was
  corrected; the ball and shadow are now visible and vertically separated.
- User visual confirmation: PASS. On a fresh ROM load, the demonstration toss
  visibly drops, bounces, and settles beside its stationary ground shadow. No
  player interaction is expected before the M2 hit tasks.

### 2026-10-12 — M2 swing state machine

- ROM: `0.1.0-release-a34a7d0-dirty-a77ee346`; MesenCE cold boot and court
  render: PASS.
- P1 enters windup on a B-button press for exactly 4 frames, followed by 3
  active frames and 7 recovery frames.
- Cardinal movement is capped at 0.75 px/frame during all committed swing
  states; diagonal movement uses the normalized per-axis cap.
- Host checks confirm presses during windup, active, and recovery cannot
  retrigger the swing, while a new press after recovery can begin another.
- Pause freezes both the swing state and its remaining-frame timer.
- User acceptance: PASS. Manual review confirmed the temporary movement-speed
  penalty while swinging. Ball contact and visible swing artwork are correctly
  deferred to `M2-HIT-02` and the later asset tasks.

