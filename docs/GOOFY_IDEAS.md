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

## Open play

The real-life open-play experience. Your player arrives at a park with
several courts and has to wait for a match to finish. You might wander the
grounds, peek at other courts, and put your paddle in a court's **paddle
stack**. When your turn comes, you play.

- Notes: needs a walkable overworld (scrolling, or screen-by-screen) and
  courts as places. Venues (ADR-024) and the CPU personalities (ADR-023)
  could populate it.

## Dramatic replay of a long rally that goes out

When a long rally ends with a ball just out, cut to a **close-up camera** on
the line, showing the ball landing just past it (or kissing the paint, and
the call is wrong!). That would be too annoying after short rallies, so it
only happens after long ones.

- Notes: the rally length is already counted (records, ADR-021). A
  close-up could be a separate BG scene, or a zoomed tile set, drawn while
  the point result shows.

## A crowd made of the players who are sitting out

The bleachers show the players who aren't on court. For example, with four
real players and two playing, the other two sit in the stands, cheering or
heckling. They might even throw things.

- Notes: the bleachers are backdrop art today. The waiting players could be
  sprites placed over them, in their own colours, using the same palettes
  as Player Select. Pairs naturally with the crowd sounds (M4-AUDIO-02) and
  rotation-based play.
