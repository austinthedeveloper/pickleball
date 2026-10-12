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

