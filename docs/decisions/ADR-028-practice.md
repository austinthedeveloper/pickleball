# ADR-028 — Practice Mode: the Ball Machine

Status: **Accepted 2026-10-13**
Depends on: ADR-019 (shot paces), ADR-020 (Player Select), ADR-021 (save),
ADR-025 (difficulty).

## Flow

Choose **PRACTICE** (the last title entry; Up from SINGLES wraps to it).

1. **The page:** sets SHOT, TARGET, and RATE, and shows the BEST STREAK. B or
   Start saves any change and continues; Y goes back.
2. **Player Select (1P):** your look and paddle (its stats apply).
3. **The session:** you alone on the left. A ball machine on the far
   baseline (x 232, centred) feeds balls from its nozzle, 8 px up. Start
   pauses, and the pause menu's MAIN MENU leaves (*amended by ADR-030*; it
   used to leave at once).

| Setting | Values | Default |
|---|---|---|
| SHOT | DRIVE, LOB, DINK, MIX | MIX |
| TARGET | KITCHEN, DEEP, FOREHAND, BACKHAND, AT YOU, RANDOM | RANDOM |
| RATE | SLOW (2 s), NORMAL (1.25 s), FAST (0.67 s) between feeds | NORMAL |

- **Speed and swinging** follow the OPTIONS difficulty: a slow ball on KID
  and EASY, and on KID the swing assist.
- **Targets:** forehand is the lower lane (facing the net, the right hand is
  +y). A DINK always lands in the kitchen. AT YOU aims where you stand,
  clamped to the court. MIX and RANDOM come from a fixed 8-bit pattern, the
  same every session.

## Rules

- There are no serves, points, or faults. Each feed is a `ball_launch` with
  the players' shot paces and flight limits (ADR-019), launched as the right
  team's shot (last hitter slot 2), so the KID assist treats it as incoming.
- **A return counts** when its first bounce is in on the machine's side.
- **A miss resets the streak:** a return into the net, out, or on your own
  side; a feed you let bounce twice; or a feed that runs 5 s without ending.
- **On screen:** the score panels read `P1 <streak>`, `PRACTICE`, and
  `BEST <best>` (up to 99), with a `START  MENU` banner. A spark marks
  each feed. Each feed also emits a HIT event, so it will sound once audio
  exists (`M4-AUDIO-01`); the game has no sound yet (ADR-009).
- **The machine** (`M4-PRACTICE-02`) is a 16 × 16 sprite: a grey body with a
  hopper of yellow balls, a nozzle facing P1, and wheels.
  - It is four 8 × 8 tiles in the small-object sheet (slots 7–10) and four
    OAM entries (18–21). The objects palette gains three greys (entries
    7–9). The OBJ tile count is unchanged (208).
  - Feeds leave the nozzle, so the launch height is 8 px. A flat drive from
    there can't clear the net and still land in the kitchen, so a DRIVE to
    the kitchen is fed as a firm standard shot. The every-shot × every-target
    landing test covers this.
- **Aiming** (*amended 2026-10-14*): the nozzle used to face P1 whatever the
  lane, so a feed's direction was a guess. Now each feed is planned when
  the wait before it starts (`practice_plan`), and the nozzle turns toward
  its lane: **up** (a lane more than 12 px toward the far sideline),
  **centre**, or **down**.
  - It turns one pose every 6 frames, so up to down passes the centre. The
    machine feeds only once it points at the lane, which costs no time
    except on AT YOU, which follows P1 through the wait and locks when the
    wait ends.
  - Each pose has its own left-half tiles (slots 22-25 of the small-object
    sheet for up and down; the OBJ tile count is unchanged). A feed leaves
    that pose's muzzle (4 px toward the far sideline for up, 3 px toward
    the near one for down), and the machine kicks back 2 px, then 1, over
    6 frames.
  - MIX and RANDOM draw from the pattern in the same order as before, so
    the same feeds come in the same order.
  - Not shown: depth (KITCHEN or DEEP) and the shot. The ball's flight
    shows those.
- **Saving:** the best streak is saved when you leave, as
  `practice_best`. The three settings are kept in `SaveSettings.practice`
  (SaveData v7).

## Code

- `src/practice.c`: settings, the page editor, feeds, and streaks.
- `game.c`:
  - starts a session instead of a serve
  - runs `practice_update` instead of the phase logic
  - `game_step_ball` (the slow-ball step) and `game_contact` (contact
    effects) are shared with matches
  - Start leaves practice
- `match_config_practice`: singles with slot 2 inactive and no opponent.
- The HUD has its own practice key (`0x400`, clear of the title's bit 8).

## Tests

- `test/test_practice.c`:
  - settings and the editor
  - **every shot to every target, 6 feeds each:** the feed clears the net,
    first bounces in on P1's side (in the kitchen for KITCHEN and DINK, deep
    for DEEP), and an unreturned feed ends with the streak at 0
  - the bot builds a streak of 8 on MIX/RANDOM; a KID-assist-only P1 builds
    32 on DINK/KITCHEN
  - the best is saved on leaving and never lowered
  - Start returns to the title
- `test_practice.c: test_machine_aim`: the nozzle turns one pose at a time,
  points at each feed's lane when it leaves (all three poses seen over 30
  RANDOM feeds), and follows P1 on AT YOU.
- `test/smoke/machine.timeline`: centre, up, the feed's kick, and a turn
  through the centre to down.
- `test/smoke/practice.timeline`: the page, Player Select, a session with a
  forehand return, and Start back to the title.

## Found while testing

The **doubles attract demo** drops about 1 frame in 40. It was already true
in v0.4.0, but the soaks never reached it: the singles demo fills the first
~4,300 frames. It averages about 197 of 225 scanlines a frame (four AI
players, four bodies, and the title). Human doubles matches run one fewer AI
and have zero drops. This is logged as `M4-UI-03`.
