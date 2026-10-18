# ADR-062 — Ladder Rebalance and Soft Short Serves

Status: **Accepted 2026-10-17**
Milestone: `M4-OPP-06`. Amends: ADR-023 (CPU styles), ADR-058 (CPU serve
power).

## Why

Playtesting on MEDIUM: rungs 2 and 3 (SNOWBIRDS, GYM BROS) were barely
beatable and nobody past rung 3 was. The bot agreed: in singles it won
21 of 60 against MARGE, 15 against BRAD, and single digits from KAI up.
The higher rungs' short serves were also nearly unreturnable: every 4th
CPU serve aims short (`SERVE_TARGET_SHORT_*`, just past the kitchen) and,
by the style's aim, 12 pixels toward a sideline, at the style's power.
At power 30 (GYM BROS, CHAMPS) it came in flat and angled off the court
before a receiver at the baseline could get there.

## Decision

- **A CPU short serve is soft and central.** It goes to the middle of the
  box (no lane shift) at the classic power, whatever `serve_power` says;
  a dud is still a dud. It stays a change of pace that draws the receiver
  in, not a winner. The depth is now picked once, before the press, and
  kept through the toss: it used to be picked again on the toss frames
  from the already advanced shot counter, so the serve that landed was
  not the one the press decided.
- **The ladder is retuned** with the style knobs (no new knobs). Mostly
  `late_period` (the strongest lever: every Nth return swung late), with
  `reaction` and `speed` to place a rung between its neighbours. The
  captains set singles; the second players (LANI, MIA, JADE) were tuned
  for doubles alone.

| Player | Knob | Was | Now |
|---|---|---|---|
| MARGE | late_period, reaction, speed | 6, 5, 15 | 3, 3, 14 |
| BRAD | late_period, reaction, serve_dud | 4, 16, 12 | 2, 20, 8 |
| PETE | late_period, reaction | 5, 2 | 3, 1 |
| RAVEN | late_period | 6 | 3 |
| KAI | late_period, reaction | 5, 0 | 3, 3 |
| LANI | late_period | 5 | 3 |
| MAX | late_period, speed | 7, 21 | 5, 20 |
| MIA | late_period | 9 | 5 |
| ACE | late_period | 8 | 5 |
| JADE | late_period | 30 | 12 |

## Balance

`make stats LATE=0 MATCHES=20 OPPONENT=-2 DIFFICULTY=2`, bot wins of 60,
before → after; every table says `ladder order: yes` after (singles and
regular-bot doubles did not before, with the short-serve change alone):

| Rung | Singles (`MODE=0`) | Doubles, regular bot (`MODE=1`) | Doubles, net bot (`MODE=3`) |
|---|---|---|---|
| 1 | DAD 48 → 54 | HENDERSON 60 → 60 | 60 → 60 |
| 2 | MARGE 32 → 48 | SNOWBIRDS 57 → 60 | 60 → 60 |
| 3 | BRAD 19 → 44 | GYM BROS 33 → 51 | 54 → 58 |
| 4 | PETE 16 → 37 | PIRATES 22 → 42 | 42 → 57 |
| 5 | RAVEN 13 → 27 | GOTH CLUB 14 → 23 | 18 → 54 |
| 6 | KAI 4 → 22 | SURF CREW 5 → 19 | 11 → 40 |
| 7 | MAX 2 → 14 | TWINS 2 → 14 | 5 → 26 |
| 8 | ACE 0 → 8 | CHAMPS 2 → 8 | 4 → 12 |

DAD (unchanged style) wins more for the bot only from the soft central
short serves.

*Amended 2026-10-18 (KNOWN_ISSUES #38):* at 60 matches a rung these
tables were in order by luck in places. With `make stats ... PHASES=4`
(the doubles CPU schedule's four phases added up) and `MATCHES=80`, and
the tool's hour-long cap replaced by ten minutes a match (it had cut the
upper rungs to 576-700 of 960), the doubles regular-bot table had GOTH
CLUB, SURF CREW, and TWINS within about one standard error of each
other, and singles had MARGE easier than DAD. The tool now also compares
rungs by win rate, not wins. Retuned (`TUNING_VERSION` 2): VLAD reaction 0
(was 4; `reaction` steps by four frames for a lane cover, and more of it
made VLAD harder, not easier), LANI late every 4th (was 3rd), MIA every
7th (was 5th), DAD every 2nd (was 3rd). Bot wins, before → after:

| Rung | Singles (of 240) | Doubles, regular bot (of 960) | Doubles, net bot (of 960) |
|---|---|---|---|
| 1 | DAD 186 → 220 | HENDERSON 960 → 960 | 960 → 960 |
| 2 | MARGE 206 → 206 | SNOWBIRDS 960 → 960 | 960 → 960 |
| 3 | BRAD 185 → 185 | GYM BROS 783 → 783 | 939 → 939 |
| 4 | PETE 128 → 128 | PIRATES 701 → 701 | 888 → 888 |
| 5 | RAVEN 74 → 74 | GOTH CLUB 313 → 409 | 772 → 675 |
| 6 | KAI 69 → 69 | SURF CREW 298 → 337 | 663 → 611 |
| 7 | MAX 40 → 40 | TWINS 275 → 186 | 542 → 406 |
| 8 | ACE 37 → 37 | CHAMPS 115 → 115 | 214 → 214 |

At 1,920 a rung the doubles gaps from GOTH CLUB down are 114 and 286
(regular bot) and 126 and 435 (net bot), five or more standard errors.
Singles RAVEN/KAI and MAX/ACE remain ties (KNOWN_ISSUES #40): separating
them moves captains, and so doubles, with ACE already at M4-OPP-06's
floor.

## Consequences

- EASY and HARD scale the new styles by the same rules (ADR-025): EASY
  halves `late_period`, and a CPU late every 2nd shot is a pushover
  (singles: the bot wins 45-60 of 60 on every rung); HARD singles is
  still hard (31 against DAD, 0-6 from MARGE up). Neither was in ladder
  order before; only MEDIUM is tuned.
- The golden trace differs (DAD's short serves): re-record after merging.

## Tests

- `test_ai.c: test_cpu_serve_power`: BRAD's serves are full power, soft
  (his short serves), or duds; some of each, mostly full power.
- `make stats` in all three modes; `make perf` zero overruns.
