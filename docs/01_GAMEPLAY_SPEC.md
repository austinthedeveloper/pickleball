# Gameplay Spec

Status: **POC rules locked**

## Screen and world coordinates

Simulation uses signed 8.8 fixed-point values. Whole-pixel court anchors:

| Element | Value |
|---|---:|
| Visible screen | 256×224 |
| Court left/right baseline | x = 20 / 235 |
| Net plane | x = 128 |
| Left/right kitchen line | x = 84 / 172 |
| Top/bottom sideline | y = 60 / 195 |
| Service centerline | y = 128 |
| Left player legal center | x = 28..78, y = 68..187 |
| Right player legal center | x = 178..227, y = 68..187 |
| Net height | z = 12 px at crossing |

These are initial tuning values. Constants belong in one court-definition module; background construction, player clamps, serve boxes, line calls, and AI must read from the same definitions.

## Controls

| Input | POC behavior | Future-compatible behavior |
|---|---|---|
| D-pad | Move on the court plane; diagonals normalized approximately | Same |
| B | Context hit; serve when holding the ball | Standard hit |
| Y | Reserved; ignored in release POC | Lob/soft shot candidate |
| A | Reserved; ignored in release POC | Power/drive candidate |
| Start | Pause/unpause | Same |
| Select | Toggles diagnostics in debug builds only | Same |

One button is intentional. Pressing B opens a short swing window; direction at contact influences the outgoing target. Holding B is not required.

## Player movement

- Update at one simulation step per displayed frame on NTSC.
- Acceleration: 0.25 px/frame² per axis.
- Maximum cardinal speed: 1.50 px/frame.
- Diagonal input uses a 181/256 multiplier per axis to approximate normalization.
- Friction with no input: subtract 0.375 px/frame toward zero.
- A swing reduces maximum speed to 0.75 px/frame until recovery.
- Clamp the player collision center to its legal half for the POC.
- Players do not collide with each other in M1–M3. Add teammate separation before doubles.

Values are tunables, not scattered literals.

## Player state machine

`IDLE → MOVE → WINDUP → ACTIVE → RECOVERY → IDLE`

- `IDLE` and `MOVE` may enter `WINDUP` on B.
- `WINDUP`: 4 frames; no contact.
- `ACTIVE`: 3 frames; at most one ball contact.
- `RECOVERY`: 7 frames; movement remains reduced.
- A new swing cannot begin during windup, active, or recovery.
- Animation reads this state; animation does not own it.

## Ball model

The ball is a deterministic entity with `x`, `y`, `z`, `vx`, `vy`, `vz`, `last_hitter`, `bounce_count`, and `rally_state`, all integers/fixed point.

Per frame:

1. Apply `x += vx`, `y += vy`, `z += vz`.
2. Apply gravity to `vz`.
3. If `z <= 0`, set `z = 0`, resolve a court bounce, and increment the side’s bounce count.
4. When the x segment crosses the net plane, fault if interpolated z is below net height.
5. When the ground position leaves the court, call out at the first ground contact, not while airborne.
6. Draw the shadow at `(x, y)` and the ball at `(x, y - z)`.

Suggested starting tunables:

| Parameter | Value |
|---|---:|
| Gravity | -0.1875 px/frame² |
| Standard hit horizontal speed | 2.5–3.0 px/frame |
| Standard hit initial vertical speed | 2.5 px/frame |
| Bounce vertical retention | 0.55 |
| Bounce planar retention | 0.92 |
| Hit reach from player center | 22 px x / 18 px y / 18 px z |

Use swept crossing tests for net and ground boundaries. Do not rely on exact coordinate equality.

## Aiming

On contact, the hitter’s opponent-side target is selected from current D-pad input:

- no vertical input: center lane;
- Up/Down: shift target toward the corresponding sideline;
- toward opponent: deeper target and slightly more pace;
- away from opponent: shorter target and slightly more arc.

The physics module derives velocity from start, target, and a fixed flight duration. Randomness is excluded from POC shot physics.

## Simplified POC rules

The POC favors a complete match loop over tournament fidelity.

1. Match is first to 5 points; win by 1.
2. Rally scoring: every rally awards a point.
3. Server begins in a simplified baseline service stance and serves diagonally into the opposite service box. Exact foot-position faults are deferred with the kitchen foot-fault rule.
4. Serve contact is automatic from a short toss after B; the player still controls aim lane.
5. Serve faults: net, outside the diagonal box, or kitchen contact.
6. The two-bounce rule is enforced: the serve and return must each bounce once before volleys are legal.
7. After those two required bounces, one bounce is allowed on each side.
8. A second bounce, net contact, or out ball loses the rally for the last hitter’s side.
9. Non-volley-zone foot faults are **not** simulated in the POC. The kitchen lines and short-ball behavior remain visible.
10. The point winner serves next. Press B after the result banner to begin.

UI must label this mode “ARCADE RULES” so the simplifications are explicit.

## CPU opponent

The CPU is intentionally legible, not optimal.

1. Predict the next ground intersection using the same ball constants, capped at 60 simulated steps.
2. Clamp the target to the CPU half and add a fixed difficulty offset, not random jitter.
3. Move toward the target with 80% of player maximum speed.
4. Swing when the ball is approaching and predicted to enter the active reach volume within the windup horizon.
5. After a hit, recover toward `(206, 128)`.

In debug builds, show predicted landing as one background marker. The CPU must not read future player input or alter ball physics.

## Rally state machine

`BOOT → ATTRACT_READY → SERVE_SETUP → SERVE_TOSS → LIVE → POINT_RESULT → MATCH_RESULT`

- Only `LIVE` accepts ordinary ball contacts.
- All state transitions use frame counters.
- `POINT_RESULT` lasts at least 60 frames and may advance early with B after 20 frames.
- `MATCH_RESULT` requires B for rematch; Start remains pause everywhere except boot.

## Feedback priority for POC

1. Ball/shadow separation.
2. Swing anticipation and contact pose.
3. Two-frame hit stop for a clean non-serve contact; the UI and pause detector continue safely.
4. One-frame palette flash on hitter or ball.
5. Text banner: `NET`, `OUT`, `TWO BOUNCES`, `POINT`.

Audio is not required by M3, but event hooks must exist for future serve, hit, bounce, fault, and point sounds.
