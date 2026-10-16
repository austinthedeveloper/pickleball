# Goofy Ideas

A running list of ideas that are **not** needed for the MVP and are not fully
baked. These are for fun and inspiration, not commitments. When one grows
up, it moves to [IDEAS.md](IDEAS.md) and gets an ADR.

## 5-player Multitap: the line judge

With a Super Multitap, four players play doubles and a fifth is the **line
judge**. They call balls in or out, and maybe the game keeps score of how
often they get it right. They could be bribed? Heckled? They could have a
whistle button.

- Notes: the Multitap gives five pads (port 1 plus four on port 2;
  docs/04_INPUT_MULTITAP_PLAN.md), so the fifth pad is free in doubles.

## Pickleball manager

Run the club behind open play (ADR-034): the courts, the players, and the
money.

- **Smallest version:**
  - a day cycle
  - a club ledger: member dues and court fees in, upkeep out
  - upgrades that change the park: lights for night play (see Night
    courts in IDEAS.md), a snack bar, more courts, a ball machine that
    unlocks practice
- **You still play:** you own the club and play on court 1; managing
  happens between days.
- **More ideas:** hire a coach, run a weekend tournament, members who leave
  when the wait at the paddle stack is too long, weather.
- Needs its own ADR before any code: the economy loop, what a day is, and
  what the park scene already supports (M4-OPEN-05).

## Quips

**Done:** [ADR-048](decisions/ADR-048-quips.md). The notes below are the
original idea.

Players say something when they miss or fault, in the point-result banner
(where `KITCHEN!` already shows). PG only: no cursing, no put-downs.

- **Per opponent:** a `quips` list in `opponents.json`, next to `intro`,
  `taunt`, and `concede`. Your player gets a generic list.
  - HENDERSON: `WELL, THAT'S A PICKLE.`
  - SNOWBIRDS: `OH, FIDDLESTICKS.`
  - GYM BROS: `BRO. NO.`
  - PIRATES: `ARRRGH!`, `SHIVER ME PADDLES!`
  - GOTH CLUB: `HOW TRAGIC.`
  - SURF CREW: `WIPEOUT!`
  - TWINS: `THAT WAS HIS FAULT.`
  - CHAMPS: `...THAT NEVER HAPPENS.`
- **Not every time:** at most one miss in three, never the same line
  twice in a row.
- **Cost:** a few hundred bytes of text.

## More modifiers

More rows or choices for the MODIFIERS page (ADR-033), next to BIG BALL,
FAST BALL, WIN BY 2, and LAVA. The user likes LAVA as it is; these sit
beside it. Each is mostly a constant or a flag, so the ROM cost is small,
but each needs a test and a doubles `make perf` run (ball-step costs are
the usual budget breaker, KNOWN_ISSUES #22).

- **MOON:** low gravity. Lobs hang forever, and drives sail long. The
  ball's gravity constant, halved; the AI's landing prediction already
  reads it (check the exact-block maths in `ai_swing_connects`).
- **BOUNCY:** the ball keeps more height on each bounce (the restitution
  in `ball_update`). Dinks pop up, which is great for put-aways.
- **GHOST BALL:** **Done:** [ADR-053](decisions/ADR-053-fun-modifiers.md).
  In flight, the ball blinks out; the shadow stays.
- **POPCORN:** **Done:** [ADR-053](decisions/ADR-053-fun-modifiers.md).
  Every hit adds a little pace; reset on the serve.
- **DINK OR DIE:** only soft shots (dinks, drops, lobs) are legal; a drive
  loses the point (**DRIVE!**, a new banner). A rules-engine modifier,
  like LAVA.
- **TINY PADDLE / HUGE PADDLE:** the reach box shrinks or grows (BIG BALL
  already grows it, so this is the same knob on its own).
- **ICE:** **Done:** [ADR-053](decisions/ADR-053-fun-modifiers.md).
  Players slide.
- **SUDDEN DEATH:** one point, winner takes the match. Pure scoring.
- **WIND** is already in IDEAS.md under Play modifiers; a venue flag (the
  beach, the lake) could pick its direction.
- **GLOW IN THE DARK:** the lights go out. The court, the crowd, and the
  backdrop fade to near black; the lines, the ball, and the players glow.
  - Night courts (ADR-045) already swap the venue palettes, so this could
    be one more palette set, built by the asset build like the LAVA
    kitchen's colours (and checked for line and ball contrast).
  - The OBJ palettes are all in use, so the players can't get new glowing
    colours; brighten their existing palettes' entries instead, or leave
    them and let the court go dark around them.
  - A trail behind the ball would sell it, but needs OBJ slots.
- **INVERSE:** two readings; pick one (or both, as two choices):
  - *Colours:* every colour inverted (white is black), like a photo
    negative. One pass over the palettes when the match loads, nothing per
    frame. Check the HUD stays readable.
  - *Controls:* the D-pad is reversed (up is down, toward the net is
    away). A flag in the input mapping, humans only. Brutal with aiming.
- **MULTI BALL:** two balls in play at once. Needs real thought, since the
  rules assume one ball:
  - *Rules options:* the first fault on either ball ends the point
    (simplest, and keeps rally scoring), or each ball scores its own point
    and the rally ends when both are dead (two points a rally; WIN BY 2
    and match point get odd).
  - *When:* the second ball enters after the two-bounce rule is done (a
    second serve from the machine, or a ball dropped from the sky), so the
    serve and two-bounce rules stay one-ball.
  - *Cost:* a second `Ball`, its physics step, its OBJ pair (ball and
    shadow), and every CPU choosing which ball to chase and running the
    swing lookahead on it. The lookahead is the doubles budget's worst
    cost (KNOWN_ISSUES #22), so this is probably singles only at first.
  - The rules engine, the CPU's landing cache, the close-call replay, and
    the camera all read `g_ball` directly; each needs a "which ball".
