#!/usr/bin/env python3
"""Authoring tool: build a venue's court scene from a mock (ADR-024).

PARK (the default) writes assets/source/court_scene.png from the level-01
mock; LAKESIDE and SUNSET write venue_lake.png / venue_sunset.png from the
level-02 / level-03 mocks. The court, lines, net, and bottom fence are
painted procedurally and identically for every venue, so their tiles are
shared (generate_assets.py checks this); only the backdrop and the corner
bushes come from the mock.

This is NOT part of the normal build. It needs Pillow and numpy and is run by
hand when the scene art changes. Its output is a committed 256x224 indexed PNG
whose palette is laid out as SNES BG palettes 2..7 (16 entries each, entry 0
unused). Every 8x8 tile uses exactly one of those sub-palettes, which is what
tools/generate_assets.py validates and converts.

Composition:
  * rows 0..75   : sky/trees/fence/bleachers resampled from the mock, with the
                   mock's HUD panels in-painted (BG3 draws live HUD panels).
  * rows 76..185 : sand, court, lines, and net painted procedurally from the
                   constants in include/court.h using the same perspective
                   projection as src/render.c.
  * rows 186..223: procedural chain-link fence with the mock's corner bushes.

Usage: python tools/art/prep_court_scene.py [PARK|LAKESIDE|SUNSET]
"""
import re
import sys
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
INSPIRATION = ROOT / "assets" / "inspiration"
SOURCE = ROOT / "assets" / "source"
# Venue: mock, output, and the mock's HUD panels (in 256x224 pixels, x0 y0 x1
# y1) to in-paint; BG3 draws live HUD panels over that area.
VENUES = {
    "PARK": (INSPIRATION / "level-01", SOURCE / "court_scene.png",
             [(11, 11, 69, 30), (75, 10, 181, 30), (187, 11, 245, 30)]),
    "LAKESIDE": (INSPIRATION / "level-02.png", SOURCE / "venue_lake.png",
                 [(7, 6, 57, 25), (81, 6, 175, 25), (199, 6, 250, 25)]),
    "SUNSET": (INSPIRATION / "level-03.png", SOURCE / "venue_sunset.png",
               [(15, 6, 77, 24), (82, 3, 180, 33), (185, 6, 246, 24)]),
}
W, H = 256, 224
SAND_TOP = 76
FENCE_TOP = 186

# Fixed court palette (5-bit RGB). Index 0 is never used by BG1 pixels.
COURT_PALETTE = [
    (0, 0, 0),
    (26, 20, 12),   # 1 sand
    (24, 17, 10),   # 2 sand speckle dark
    (28, 22, 14),   # 3 sand speckle light
    (6, 17, 16),    # 4 court green
    (4, 12, 12),    # 5 kitchen in the net's shadow (ADR-027)
    (31, 29, 24),   # 6 line
    (21, 21, 18),   # 7 line in shadow
    (4, 7, 8),      # 8 net mesh
    (2, 2, 3),      # 9 post / rail
    (15, 16, 18),   # 10 post highlight
    (28, 28, 26),   # 11 net tape
    (6, 17, 16),    # 12 kitchen (painted like the court; ADR-027)
    (21, 16, 11),   # 13 fence mesh
    (12, 9, 6),     # 14 fence mesh shadow
    (8, 13, 12),    # 15 court edge dark
]
SAND, SAND_D, SAND_L, GREEN, KITCHEN_S, LINE, LINE_S, MESH, POST, POST_H, \
    TAPE, KITCHEN, FMESH, FMESH_S, EDGE = range(1, 16)
# Sand in shadow under the far fence: only in backdrop tiles (quantized into
# palettes 3-7), so it needs no palette 2 entry.
SAND_S = 16
SAND_S_COLOR = (19, 14, 9)


sys.path.insert(0, str(ROOT / "tools"))
import court_projection  # noqa: E402

TABLE = court_projection.scale_table()


def court_constants():
    return court_projection.court_constants()


def project_x(x, y, c):
    """Same table and rounding as the game (tools/court_projection.py)."""
    return court_projection.project_x(x, y, TABLE)


def paint_court(c):
    """Returns an HxW array of court-palette indices (0 = not painted)."""
    idx = np.zeros((H, W), dtype=np.int32)
    rng = np.random.RandomState(94)
    top, bottom = c["COURT_SIDELINE_TOP"], c["COURT_SIDELINE_BOTTOM"]
    left, right = c["COURT_BASELINE_LEFT"], c["COURT_BASELINE_RIGHT"]
    net = c["COURT_NET_X"]

    # Sand with sparse speckles.
    idx[SAND_TOP:FENCE_TOP, :] = SAND
    noise = rng.randint(0, 100, size=(H, W))
    band = np.zeros((H, W), dtype=bool)
    band[SAND_TOP:FENCE_TOP, :] = True
    idx[band & (noise < 6)] = SAND_D
    idx[band & (noise >= 97)] = SAND_L
    # Soft shadow under the far fence/bleachers.
    idx[SAND_TOP:SAND_TOP + 2, :] = SAND_S

    # Court surface.
    for y in range(top, bottom + 1):
        x0 = project_x(left, y, c)
        x1 = project_x(right, y, c)
        idx[y, x0:x1 + 1] = GREEN
        # The kitchen (non-volley zone) has its own entries (ADR-027).
        k0 = project_x(c["COURT_KITCHEN_LEFT_X"], y, c)
        k1 = project_x(c["COURT_KITCHEN_RIGHT_X"], y, c)
        idx[y, k0:k1 + 1] = KITCHEN
    # Dark lip under the near sideline gives the slab a little depth.
    for x in range(project_x(left, bottom + 1, c),
                   project_x(right, bottom + 1, c) + 1):
        idx[bottom + 1, x] = EDGE

    # Net shadow: a thin diagonal falling down-left from the net on the court.
    for y in range(top + 1, bottom):
        for dx in range(4, 7):
            x = net - dx - (y - top) // 16
            if idx[y, x] == KITCHEN:
                idx[y, x] = KITCHEN_S

    # Lines: sidelines/centerline are 1px rows; baselines/kitchen are 2px.
    def hline(y, xa, xb):
        for x in range(project_x(xa, y, c), project_x(xb, y, c) + 1):
            idx[y, x] = LINE_S if idx[y, x] in (KITCHEN_S, SAND_S) else LINE

    def vline(xw):
        for y in range(top, bottom + 1):
            sx = project_x(xw, y, c)
            for x in (sx, sx + 1):
                idx[y, x] = LINE_S if idx[y, x] in (KITCHEN_S, SAND_S) else LINE

    hline(top, left, right)
    hline(bottom, left, right)
    hline(c["COURT_CENTERLINE_Y"], left, c["COURT_KITCHEN_LEFT_X"])
    hline(c["COURT_CENTERLINE_Y"], c["COURT_KITCHEN_RIGHT_X"], right)
    for xw in (left, right - 1, c["COURT_KITCHEN_LEFT_X"],
               c["COURT_KITCHEN_RIGHT_X"] - 1):
        vline(xw)

    # Net: mesh strip from the tape (net height above the ground line) down
    # to the ground, posts at both sidelines.
    height = c["COURT_NET_HEIGHT"]
    for y in range(top - height, bottom + 1):
        for x in range(net - 3, net + 2):
            if (x + y) % 2 == 0 or x == net - 3:
                idx[y, x] = MESH
        idx[y, net + 2] = MESH if y % 2 else idx[y, net + 2]
    for y in range(top - height, bottom - height + 1):
        idx[y, net - 3] = TAPE
    # Posts.
    for post_ground in (top, bottom):
        for y in range(post_ground - height - 3, post_ground + 2):
            idx[y, net - 4] = POST
            idx[y, net - 3] = POST_H
            idx[y, net - 2] = POST
        for x in range(net - 5, net):
            idx[post_ground + 2, x] = POST
    return idx


def paint_bottom_fence(idx):
    """Chain link below the sand with a rail and posts."""
    for y in range(FENCE_TOP, H):
        for x in range(W):
            if idx[y, x]:
                continue
            idx[y, x] = SAND_S if y < FENCE_TOP + 3 else SAND
            if y >= FENCE_TOP + 3 and ((x + y) % 6 == 0 or (x - y) % 6 == 0):
                idx[y, x] = FMESH if y % 2 else FMESH_S
    for y in range(FENCE_TOP, FENCE_TOP + 3):
        idx[y, :] = POST if y != FENCE_TOP + 1 else POST_H
    for px in (46, 124, 202):
        for y in range(FENCE_TOP - 4, H):
            idx[y, px - 2] = POST
            idx[y, px - 1] = POST_H
            idx[y, px] = POST
            idx[y, px + 1] = POST
        idx[FENCE_TOP - 5, px - 1:px + 1] = POST
    return idx


def inpaint(rgb, rects):
    """Diffuse surrounding colors into the given rectangles."""
    rgb = rgb.astype(np.float64)
    mask = np.zeros(rgb.shape[:2], dtype=bool)
    for x0, y0, x1, y1 in rects:
        mask[y0:y1, x0:x1] = True
    rgb[mask] = 0
    for _ in range(400):
        padded = np.pad(rgb, ((1, 1), (1, 1), (0, 0)), mode="edge")
        avg = (padded[:-2, 1:-1] + padded[2:, 1:-1] +
               padded[1:-1, :-2] + padded[1:-1, 2:]) / 4
        rgb[mask] = avg[mask]
    return rgb


def kmeans(points, weights, k, iters=12, seed=0):
    rng = np.random.RandomState(seed)
    uniq = np.unique(points, axis=0)
    if len(uniq) <= k:
        return uniq.astype(np.float64)
    centers = uniq[rng.choice(len(uniq), k, replace=False)].astype(np.float64)
    for _ in range(iters):
        d = ((points[:, None, :] - centers[None, :, :]) ** 2).sum(-1)
        a = d.argmin(1)
        for j in range(k):
            sel = a == j
            if sel.any():
                centers[j] = np.average(points[sel], axis=0, weights=weights[sel])
    return centers


def quantize_tiles(rgb5, tiles, palettes_count, seed=1):
    """Cluster tiles into palettes of 15 colors each.

    rgb5: HxWx3 float image in 5-bit units. tiles: list of (tx, ty).
    Returns (palettes[list of 15x3 int arrays], assignment dict, index image).
    """
    def tile_pixels(tx, ty):
        return rgb5[ty * 8:ty * 8 + 8, tx * 8:tx * 8 + 8].reshape(-1, 3)

    means = np.array([tile_pixels(tx, ty).mean(0) for tx, ty in tiles])
    groups = kmeans(means, np.ones(len(means)), palettes_count, seed=seed)
    d = ((means[:, None, :] - groups[None, :, :]) ** 2).sum(-1)
    assign = d.argmin(1)
    palettes = []
    for _ in range(6):
        palettes = []
        for j in range(palettes_count):
            members = [t for t, a in zip(tiles, assign) if a == j]
            if not members:
                members = tiles[:1]
            px = np.concatenate([tile_pixels(*t) for t in members])
            pts, counts = np.unique(np.round(px).astype(int), axis=0,
                                    return_counts=True)
            cen = kmeans(pts.astype(np.float64), counts.astype(np.float64), 15,
                         seed=seed + j)
            cen = np.clip(np.round(cen), 0, 31).astype(int)
            while len(cen) < 15:
                cen = np.vstack([cen, cen[-1:]])
            palettes.append(cen)
        errors = []
        for t in tiles:
            px = tile_pixels(*t)
            errs = []
            for pal in palettes:
                dd = ((px[:, None, :] - pal[None, :, :]) ** 2).sum(-1)
                errs.append(dd.min(1).sum())
            errors.append(errs)
        assign = np.array(errors).argmin(1)
    index = {}
    for t, a in zip(tiles, assign):
        px = tile_pixels(*t)
        dd = ((px[:, None, :] - palettes[a][None, :, :]) ** 2).sum(-1)
        index[t] = (int(a), dd.argmin(1).reshape(8, 8) + 1)
    return palettes, index


def main():
    venue = sys.argv[1].upper() if len(sys.argv) > 1 else "PARK"
    if venue not in VENUES:
        raise SystemExit(f"venue is one of {', '.join(VENUES)}")
    mock_path, out_path, hud_rects = VENUES[venue]
    c = court_constants()
    mock = Image.open(mock_path).convert("RGB").resize((W, H), Image.BOX)
    rgb = np.asarray(mock).astype(np.float64)
    # The mock's HUD panels and banner are replaced by live BG3 panels.
    rgb = inpaint(rgb, hud_rects)
    rgb5 = rgb / 255.0 * 31.0

    court_idx = paint_court(c)
    court_idx = paint_bottom_fence(court_idx)

    # Corner bushes from the mock overlay the procedural bottom fence.
    bush = np.zeros((H, W), dtype=bool)
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    green = (g > r + 12) & (g > b + 4)
    for y in range(FENCE_TOP - 14, H):
        for x in range(W):
            if (x < 70 or x > 186) and green[y, x]:
                bush[y, x] = True
    # Close small holes so bush silhouettes stay solid.
    for _ in range(2):
        grown = bush.copy()
        grown[1:, :] |= bush[:-1, :]
        grown[:-1, :] |= bush[1:, :]
        grown[:, 1:] |= bush[:, :-1]
        grown[:, :-1] |= bush[:, 1:]
        bush = grown & ((np.arange(W)[None, :] < 72) |
                        (np.arange(W)[None, :] > 184)) & \
            (np.arange(H)[:, None] >= FENCE_TOP - 14)
    court_idx[bush] = 0

    court_rgb = np.array(COURT_PALETTE + [SAND_S_COLOR], dtype=np.float64)
    final_rgb5 = rgb5.copy()
    painted = court_idx > 0
    final_rgb5[painted] = court_rgb[court_idx[painted]]

    fixed_tiles, free_tiles = [], []
    for ty in range(H // 8):
        for tx in range(W // 8):
            block = court_idx[ty * 8:ty * 8 + 8, tx * 8:tx * 8 + 8]
            (fixed_tiles if (block > 0).all() else free_tiles).append((tx, ty))

    palettes, index = quantize_tiles(final_rgb5, free_tiles, 5)

    out = np.zeros((H, W), dtype=np.uint8)
    for tx, ty in fixed_tiles:
        if (court_idx[ty * 8:ty * 8 + 8, tx * 8:tx * 8 + 8] > 15).any():
            raise SystemExit(f"tile {tx},{ty}: sand shadow in a court tile")
        out[ty * 8:ty * 8 + 8, tx * 8:tx * 8 + 8] = \
            2 * 16 + court_idx[ty * 8:ty * 8 + 8, tx * 8:tx * 8 + 8]
    for (tx, ty), (pal, block) in index.items():
        out[ty * 8:ty * 8 + 8, tx * 8:tx * 8 + 8] = (3 + pal) * 16 + block

    flat = [0] * (256 * 3)
    def put(slot, col):
        flat[slot * 3:slot * 3 + 3] = [round(v * 255 / 31) for v in col]
    for i, col in enumerate(COURT_PALETTE):
        put(32 + i, col)
    for p, pal in enumerate(palettes):
        for i, col in enumerate(pal):
            put((3 + p) * 16 + 1 + i, col)
    img = Image.fromarray(out, mode="P")
    img.putpalette(flat)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    img.save(out_path, optimize=False)
    print(f"wrote {out_path.relative_to(ROOT)} "
          f"({len(fixed_tiles)} court tiles, {len(free_tiles)} scene tiles)")


if __name__ == "__main__":
    main()
