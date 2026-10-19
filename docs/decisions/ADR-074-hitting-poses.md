# ADR-074 — The Hitting Poses in Play

Status: **Accepted 2026-10-18**
Task: `M4-ART-04`. Amends: ADR-059 (character models: the four hitting
poses, drawn but never shown). Extends: ADR-064 (flair).

## Why

ADR-059 put four hitting poses on the player sheet (slots 12-15:
`overhead_windup`, `overhead_contact`, `reach`, `scoop`) for the reach and
overhead mechanics in IDEAS.md ("Hitting: reach, height, and overheads").
Those mechanics are a rules change with a retune; the poses waited on
them, so the only frames on the sheet that nothing drew were these four.
Every swing looked the same whatever the ball did. A swing can show what
its contact already is, with play unchanged.

## Decision

**Presentation only.** The pose follows the contact the simulation already
makes; nothing in the simulation reads it, so the golden trace and the
ROM-host lockstep are unchanged.

- **At a rally contact** (`EVENT_HIT`), flair picks the hitter's pose
  (`flair_hit_frame`) from the ball's height and the hit box:
  - at or above **18 px** (`FLAIR_OVERHEAD_Z`; reach ends at 24): the
    **overhead** contact, paddle straight up;
  - else **stretched** (outside the sweet spot, the contact that sprays,
    `collision_ball_in_sweet_spot`): the **reach**, a low lunge. It shows
    the player why the shot went wide;
  - else below **4 px** (`FLAIR_SCOOP_Z`): the **scoop**;
  - else the usual swing and follow-through.
  It is kept per slot in `g_flair_hit` (frame index words, `$FFFF` for
  none). A point's end clears all four and a serve its server's, so a
  serve's swing (marked met from its windup, `serve.c`) never shows one.
- **`render_bodies`**: a swing that has met the ball (`contact_consumed`)
  shows its slot's pose through the rest of the active frames and the
  recovery. Before contact, while the ball is at or above 18 px, the
  windup is the **overhead windup** (paddle cocked behind the head) and
  the active frames the overhead contact; a miss follows through as
  usual. The swing's frame choice is out of line (`_rb_swing`): the
  extra tests pushed the reaction and pose branches past 8-bit range, and
  idle and running bodies, most of them, take one more branch than
  before.
- **Thresholds** from 20 matches a rung against the ladder with the
  stats bot: contacts at 18 px or more are about 6% in singles and 14% in
  doubles (net play), below 4 px about 12%, stretched about 23%.

## Not done

- The mechanics in IDEAS.md: a hit box in front on the paddle side, the
  D-pad's reach, height bands, and the overhead smash as its own shot.
  With them the poses' rules would follow the new bands.
- Handedness under flip (KNOWN_ISSUES #7): left-facing poses are mirrors,
  as every frame is.

## Verification

- `test_flair.c`: `flair_hit_frame` at each band edge, stretched across
  and along the court; ten minutes of bot play in singles and doubles
  (577 and 563 contacts; reaches and scoops in both, 74 overheads in
  doubles, none in singles against the default CPU, which stays back),
  each pose the one its contact earned, and every slot clear through the
  serve's setup and toss.
- `test_layout.c`: `ASM_BALL_Z` and `ASM_FLAIR_OVERHEAD_Z_FP` match C.
- MesenCE (title demo, debug ROM): the overhead windup and contact
  (frames 5007-5022), a reach (665), and a scoop on the right side,
  flipped (2771).
- `make check`; `make golden-compare` identical (4230 frames); `make
  lockstep` identical; `make perf` zero overruns on every timeline.
