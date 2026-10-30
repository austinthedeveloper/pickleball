# Character art refresh: what to generate

The in-game players read as "E.T.": a head almost as wide as the shoulders,
about 2.5 heads tall, no neck, short arms and legs. This brief asks for a
replacement set with athletic proportions, at the same size and in the same
format, so it drops into the existing pipeline (ADR-059) with no code
changes beyond a few offsets.

Only the **in-game 32×32 player frames** (and the ponytail and hats that sit
on them) are in scope. The Player Select portraits and opponent busts are
already the look we want; they are the style reference.

## Where to put things

Drop everything into `art/characters/` next to this file:

```text
art/characters/
  model_sheet.png        step 1, the look to approve
  frames/<name>.png      step 2, one image per frame (names below)
  ponytail.png           step 3
  hats/<name>.png        step 3
  notes.md               optional: anything you changed or couldn't get
```

Claude reviews it, cleans it to the exact palette and grid, and imports it
through the port kit (`pickleball-port-kit/art/sprites/bare/`, then
`tools/art/import_kit_sprites.py`). Nothing in `assets/source/` should be
edited by hand.

## Format, two ways

**A. Native pixel art (best, e.g. Aseprite):** each frame exactly 32×32,
indexed PNG, the 16-colour palette below, index 0 transparent.

**B. Image generator output (fine):** each frame drawn on a **32×32 pixel
grid upscaled 8× = 256×256**, every "pixel" a clean 8×8 block, flat colours
only, background solid magenta `#FF00FF`. Claude downsamples and snaps it to
the palette. Blurry edges, gradients, or blocks that don't line up with the
grid are what make method B fail, so ask the generator for that explicitly.

## Rules every frame must follow

1. Canvas 32×32. Character faces **right**. Nothing crosses the edge.
2. **Feet end on row 29** (the bottom of the shoes is the 30th row from the
   top); rows 30–31 are empty. Same baseline in every frame.
3. No ground shadow (the game draws its own), no ball, no effects.
4. Bareheaded, short hair close to the skull (hats are separate objects).
5. A one-pixel dark outline around the silhouette. No anti-aliasing,
   no dithering, no semi-transparent pixels.
6. Right-handed, paddle in the hand nearest the viewer, as in the current
   frames. One body serves every team: only the colours swap.
7. Draw in the **blue team colours** below. Every pixel must be one of them.

| Role (palette index) | Colour |
|---|---|
| Transparent (0) | `#FF00FF` magenta in method B |
| Outline (1) | `#101031` |
| Skin shadow / skin / skin light (2–4) | `#B56B4A` `#F7AD7B` `#FFD6A5` |
| Shirt dark / shirt / shirt light (5–7) | `#19319C` `#3163E6` `#7BADFF` |
| Shorts shadow / shorts (8–9) | `#ADB5CE` `#FFFFFF` |
| Shoes (10) | `#FFFFFF` |
| Paddle face dark / light (11–12) | `#213AA5` `#4A7BF7` |
| Paddle edge and grip (13) | `#3A2921` |
| Highlight (14) | `#FFFFFF` (a pixel or two: paddle glint, eye) |
| Hair (15) | `#633A21` |

Shorts, shoes, and highlight share white here; Claude assigns them by where
they are, so don't worry about telling them apart.

## The proportions (the actual fix)

Target **about 3.5–4 heads tall**, like the court mockup
(`assets/inspiration/level-01`) and the Player Select portrait. In a 32×32
frame standing at idle that works out to roughly:

| Part | Rows (idle0) | Size |
|---|---|---|
| Head with hair | 3–10 | 7 px wide, 8 px tall at most (today ~11×11) |
| Neck | 11 | visible, 2–3 px wide |
| Shirt (shoulders to hem) | 12–19 | shoulders ~10 px wide, wider than the head |
| Shorts | 19–23 | |
| Legs | 23–27 | knees bent in a ready stance |
| Shoes | 27–29 | 3–4 px long each |

- Arms reach mid-thigh when hanging, with a visible elbow in the swing poses.
- Face: one dark eye pixel, ear and jaw suggested by skin shadow. **No big
  eyes, no round forehead.** The head should be slightly taller than wide.
- Paddle about 8 px long (face ~5×6 plus a 2–3 px handle): a little bigger
  than real life so it reads on court, but smaller than the head was.
- Head stacked over the hips, hips centred over the feet (ADR-017: the old
  set leaned forward).
- Keep the top of the head in the **same place in the two idle frames and
  the four run frames, ±1 row** (the bob is fine). Hats ride on it.

## Step 1: model sheet (approve before the frames)

`model_sheet.png`: the idle0 pose at 8× (256×256), plus the same figure at
1× and at 2× next to the current sprite for scale. This is where we agree
the proportions before 21 frames get drawn.

## Step 2: the frames (21)

One file each in `frames/`, named exactly:

| File | Pose |
|---|---|
| `idle0`, `idle1` | Ready stance, paddle up in front of the chest; idle1 dips one row |
| `run0`–`run3` | Run cycle facing right: contact, pass, contact (other leg), pass |
| `windup` | Forehand drawn back, shoulders turned, paddle behind the hip |
| `swing` | Contact: arm extended forward, paddle face square, out in front |
| `follow` | Follow-through, paddle up past the far shoulder (keep the grip below the head: a hat covers the head area) |
| `toss` | Serve: ball hand forward, paddle back and low (underhand serve) |
| `cheer` | Point won: paddle fist raised (keep the fist beside the head, not over it) |
| `overhead_windup` | Side-on, paddle cocked behind the head, off arm pointing up |
| `overhead_contact` | Arm up and forward, paddle at its highest; paddle top may touch row 0 |
| `reach` | Wide lunge, back leg straight, paddle out low past the front foot |
| `scoop` | Deep knees, paddle flat and face-open just above the ground |
| `slump` | Lost the point: head bowed, shoulders down, paddle hanging |
| `dive` | Airborne horizontal lunge, paddle stretched forward, body along rows ~18–28 |
| `scramble` | Up off a dive, on one knee, pushing up |
| `thumbs_up` | Free hand thumbs-up, paddle down at the side |
| `flex` | Double biceps, paddle dropped at the feet |
| `en_garde` | Paddle held out like a cutlass, other hand on the hip |

If a budget forces a cut, the order of importance is: idle, run, windup,
swing, follow, toss, cheer, then the rest. Anything not delivered keeps
today's art.

## Step 3: ponytail and hats

The girl variant is the same body plus an 8×8 ponytail object; hats are 16×8
objects drawn over the head. Both were drawn for the big head, so they need
redrawing at the new head size.

- `ponytail.png`: 8×8 (64×64 at 8×), hair colour plus outline, tied at the
  back of the crown, hanging to the shoulders. Drawn behind the body.
- `hats/<name>.png`: 16×8 each (128×64 at 8×), drawn to sit on the new
  idle0 head, for: `cap`, `visor`, `headband`, `tricorn`, `bandana`,
  `beanie`, `backcap`, `bucket`, `crown`, `whirly`, `chef`, `viking`.
  Keep each hat's design and colours from today's set (the hat strip at the
  bottom left of `assets/source/player_sprites.png`), only resized for the
  smaller head. Each hat must still look right if the head
  drops or rises one row.

Also include `hats/_on_head.png`: idle0 wearing each hat in a row, so the
fit can be checked.

## What Claude checks on review

- Proportions: head height about a quarter of the figure, neck visible,
  shoulders wider than the head, at 1× and at 2× on the court.
- Every frame 32×32, feet on row 29, nothing outside, palette-only pixels.
- Head positions per frame (Claude measures them and updates
  `player_frames.json` accessory offsets and `hat_offset` to match).
- Animations play cleanly (idle, run, swing) with no jitter.
- Works in all four team palettes (red, green, gold swapped in).
- On court in doubles: four players, hats, and ponytails still legible, and
  the scanline budget unchanged (one 32×32 object per body; no new objects).
- `make check`, `make perf`, and screenshots of a match and Player Select
  thumbnails.

## Copy-paste prompt for an image generator

Use one prompt per frame (or per small group), and generate the model sheet
first.

> 16-bit Super Nintendo sports game sprite, pixel art, a single pickleball
> player, full body, facing right, side view with a slight three-quarter
> turn. Drawn on an exact 32 by 32 pixel grid, shown upscaled 8 times
> (256 by 256 image) so every pixel is a crisp 8 by 8 square. Athletic
> adult proportions, about 4 heads tall: small head, visible neck, broad
> shoulders, long arms and legs, knees bent in a ready stance. NOT chibi,
> NOT big-headed, no big eyes. Short brown hair, no hat. Royal blue polo
> shirt, white shorts, white sneakers, light tan skin, holding a blue
> pickleball paddle in the right hand. One-pixel dark navy outline around
> the whole figure, flat colours with 3-tone shading, no anti-aliasing,
> no gradients, no dithering, no ground shadow, no text. Solid magenta
> #FF00FF background. Feet at the bottom of the grid with two empty pixel
> rows below. Pose: **[pose from the table above]**.

Attach `assets/source/select_portraits.png` (style and face) and a crop of
the court mockup as image references if the generator takes them.
