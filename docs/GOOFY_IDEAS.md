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
- **GLOW IN THE DARK:** the lights go out and the outlines glow. The
  user's picks: the players' black outlines turn green (or another neon),
  the ball's too, the paddles change, on the night court palette.
  - *Outlines:* every player palette has the outline in entry 1 (the
    shared role, ADR-013; contract table in docs/03), so a glow match
    composes each palette with a neon there instead
    (`match_compose_palette`, and the career and opponent looks it copies).
    No new palettes: the OBJ palettes are all in use, but these are the
    same palettes with different colours.
  - *Ball:* entry 1 of the objects palette is the ball's dark outline
    (2 is its yellow, 3 its light). A work-RAM copy of `objects_palette`
    with entry 1 neon. The sparks and the practice machine share that
    palette; check they still look right.
  - *Paddles:* entries 11-13 (face dark, face light, edge). A neon set
    (pink, cyan, or a per-team pick) in glow matches.
  - *Court:* NIGHT's palette (ADR-045), or a darker "lights out" time in
    `court_colors.json` `times`, which the asset build already contrast-
    checks. Forced on for a glow match whatever the COURT page's TIME.
  - *Maybe:* darken the skin and shirt ramps a step so the outline pops,
    like a blacklight.
- **INVERSE:** the user's pick is colours: every colour inverted (white is
  black), like a photo negative.
  - Each colour is `0x7FFF ^ c`. The SNES colour math can't do it (it adds
    or subtracts, never "white minus"), so it's done to the palettes as
    they're composed: the court ground and backdrop (with LAVA and TIME),
    the player and objects palettes, the flash, and the HUD's team colours.
    All of those upload in VBlank or forced blank already; one more pass
    each, nothing per frame.
  - The ball's shadow is black averaged with the court in singles; keep it
    dark (not inverted) or it becomes a bright spot.
  - Check the HUD text stays readable, and that the asset build's contrast
    checks still hold inverted (they should: inversion keeps differences).
- **MULTI BALL:** the user's pick: a machine spits out several balls, and
  the scoring stays the same, so the first to the winning score wins.
  - *Scoring:* each ball scores on its own when it dies (out, net, two
    bounces, a fault), by the usual rules for whoever hit it last. A match
    can end mid-rally: when a side reaches the winning score, the match is
    over, balls still in the air or not.
  - *Open questions:* how many balls at once (two, three, more?); all at
    the serve, or the machine (ADR-028's practice machine) feeding a new
    one every few seconds or each time one dies; does the two-bounce rule
    apply to every ball, or only the served one; does the server keep
    serving or does the point winner (with several points a rally).
  - *Cost:* a `Ball` each, its physics step, its OBJ pair (ball and
    shadow), and every CPU choosing which ball to chase and running the
    swing lookahead on it. The lookahead is the doubles budget's worst
    cost (KNOWN_ISSUES #22), so probably singles only at first, and
    measured ball by ball.
  - The rules engine, the CPU's landing cache, the close-call replay, and
    the camera all read `g_ball` directly; each needs a "which ball".
