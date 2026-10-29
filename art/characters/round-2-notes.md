# Round 2: core animation reference frames

Nine individual PNGs generated with the built-in image generator: idle0, idle1, run0, run1, run2, run3, windup, swing, follow.

These are image-generator source renders for the review/cleanup workflow in CHARACTER_ART_PROMPT.md, not indexed production sprites. Each is a single bareheaded player on magenta, referenced to the approved model sheet. No hats or ponytails are baked into the body.

Prompts request 2–3 logical pixels less height through shorter legs, head near row 4, feet on row 29, two empty bottom rows, exact 32×32 grid, flat palette colours, and no noise. The renderer does not reliably enforce the grid, palette, or baseline. Native output resolution is larger than the requested 256×256. Keep the originals for cleanup rather than treating them as import-ready.

Before import: simplify each pose to a real 32×32 grid and the brief's palette; align head positions and soles; check one-pixel idle dip and the alternating run-leg phases; ensure a consistent right-handed paddle arm through windup/contact/follow. Normalize head size, paddle size, and the space above the head across the set. No game sources or runtime assets were changed.

This batch covers the requested idle, run, and swing priority. Serve, reactions, overhead/lunge/dive poses, and the separate accessory assets remain for a later batch.

See prompts.json for the full base direction and individual poses. Subsequent frames also used idle0 as a consistency reference; swing/follow additionally referenced windup.
