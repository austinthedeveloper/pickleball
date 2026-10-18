# ADR-066 — Join Counter and Player Rings

Status: **Accepted 2026-10-16**
Milestone: `M4-MP-02`. Prepares: `M4-MP-01` (four humans). Amends:
ADR-020 (2P joins on the title), ADR-065 (the title's top rows).

## Why

The title said `2P PRESS START`, which doesn't grow to four players, and
joining showed only `2P JOINED!` under the menu. In a match nothing said
which player was whose: two humans in doubles, or a human partnered with
a CPU, look alike apart from shirts, and shirts are chosen per match.

## Decision

- **Who joined** is a bitmask, `g_title_joined` (bit n: controller
  n + 1; 1P always). Start on controller 2 toggles 2P as before; with a
  Multitap, controllers 3 and 4 (pad positions 2 and 3) toggle 3P and 4P.
  Matches still take 1P and 2P only: 3P and 4P sit out until `M4-MP-01`.
  *Amended 2026-10-18 (ADR-073):* they play now.
- **The join counter** takes the demo scoreboard's place at the top of
  the title (ADR-065 cleared it): a panel reading `1P 2P 3P 4P`, each
  followed by a dot in that player's colour when joined, or a grey x. It
  draws on idle title frames, the panel first and its text on another
  frame, and only redraws when someone joins or leaves.
- **The menu's line** reads `FRIENDS: PRESS START`.
- **Player colours**: 1P red, 2P blue, 3P yellow, 4P green, CPU grey.
- **Rings**: a 16x8 ellipse round each player's shadow, in their colour
  (`match_slot_controller`: 1P on slot 0, 2P on slot 2 in versus or slot
  1 partnering in open play, else the CPU). OAM 38-45, two halves per
  slot, behind every other object, drawn by `render_bodies.asm` with
  `oam_write_pair` after the shadow. None in the title's demo.

## Palettes and tiles

- OBJ palettes are full (CLAUDE.md), but the objects palette had six
  unused entries: the ring colours take 10-14. It is opaque in singles
  (palette 2, exempt from colour math) and doubles (palette 4, math off).
- Name table 1 is full (250 + 10 would be 260 of 256), so the ten ring
  tiles go after name table 2's rows (from tile 384), as the hats do.
- BG3 has only palettes 0-3. On the title, where the scoreboard no longer
  shows, palettes 2 and 3 (the team colours in a match) carry the ring
  colours: 2 is blue text and a green border shade, 3 red. Yellow is
  palette 1's text. The counter's dots use a text-colour dot tile, a
  border-colour dot (4P green), and a border-colour x (grey).

## Frame budget

Rings cost one `oam_write_pair` per player per frame. `make perf` holds
every timeline at zero overruns, but the doubles peaks rose: worst
scanline 200 (from 189) on `doubles_ladder`, 213 (204) on
`modifiers_doubles`, 218 (204) on `modifiers_fun`, of 225.

## Not done

- 3P and 4P playing (`M4-MP-01`). The headless runner presses port 1
  only (KNOWN_ISSUES #16), so joining is checked by poking
  `g_title_joined`.
- A ring for the park's walk-around sprites (ADR-034): the park draws
  its own scene.

## Verification

- `make check`: `test_match.c` (each slot's controller: 1P, 2P in versus
  and in co-op, CPUs).
- `make perf`: zero overruns (above). `make golden-compare`: identical.
- Screenshots (debug ROM): the title counter (1P alone, and 1P, 2P, 4P
  poked in), a doubles ladder match (a red ring for 1P, grey for the
  CPUs), practice, and the title after the HAT MUSEUM (the counter keeps
  its colours).
