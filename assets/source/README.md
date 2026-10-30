# Source assets

The art is committed here as indexed PNGs, with palette part files in
`palettes/` and the data files (`*.json`) beside them.
`tools/generate_assets.py` validates them and converts them into
`assets/generated/` on every build; it needs only the Python standard
library.

The PNGs are edited by hand or written by the authoring tools in
`tools/art/`, which are run by hand, never by the build:

- `import_character_art.py`: the player bodies, poses, hats, and ponytail
  from the text grids in `art/characters/sprites/` (ADR-078). It replaces
  `import_kit_sprites.py` (ADR-059), which now needs `--force`.
- `import_wishlist.py`, `wishlist_all.sh`, `wishlist*.lua`: the sprite wish
  list in `wishlist/` (ADR-064; the `.lua` scripts run in Aseprite).
- `prep_court_scene.py`, `prep_select_scene.py`, `prep_opponent_busts.py`:
  venue scenes, Player Select, and the opponent portraits from mocks (some
  need Pillow and numpy).

No third-party or reference-game graphics are included.
