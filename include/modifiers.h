#ifndef MODIFIERS_H
#define MODIFIERS_H

#include "game_types.h"
#include "player.h"

/*
 * Play modifiers (docs/decisions/ADR-033-modifiers.md): rule and physics
 * toggles for a match, one bit each in MatchConfig.modifiers, kept in the
 * save; past the eighth, in MatchConfig.modifiers2 (ADR-054). Pure
 * helpers; game.c, rules.c, ai.c, and render.c apply them.
 */

enum {
    MODIFIER_BIG_BALL = 1,      /* a 14 px ball, reach and sweet spot +4 px */
    MODIFIER_FAST_BALL = 2,     /* one extra ball step every 3rd frame */
    MODIFIER_WIN_BY_2 = 4,      /* first to POINTS, but by two */
    MODIFIER_LAVA = 8,          /* stepping into your kitchen loses the point */
    MODIFIER_GHOST_BALL = 16,   /* high in flight, the ball only blinks */
    MODIFIER_POPCORN = 32,      /* every hit in a rally adds pace */
    MODIFIER_ICE = 64,          /* players slide */
    MODIFIER_GLOW = 128,        /* lights out: neon outlines, a night court */
    MODIFIER_ALL = 255,
    MODIFIER_COUNT = 8
};

/* The second byte (ADR-054): presentation only, read when a match loads. */
enum {
    MODIFIER2_INVERSE = 1,      /* every colour inverted */
    MODIFIER2_ALL = 1,
    MODIFIER2_COUNT = 1
};

enum {
    /* Big ball: the hit box grows by the ball's extra radius. */
    MODIFIER_BIG_REACH = 4,
    /* Lava: CPUs keep their feet this far behind their kitchen line. */
    MODIFIER_LAVA_MARGIN = 8,
    /* Ghost ball: at or above this height (px) the ball shows only on
     * MODIFIER_GHOST_SHOWN frames in every 16. */
    MODIFIER_GHOST_Z = 8,
    MODIFIER_GHOST_SHOWN = 2,
    /* Popcorn: each level adds a ball step on one tick in every
     * MODIFIER_POPCORN_STRIDE (5 a cycle, about +8%), up to the cap (+50%). */
    MODIFIER_POPCORN_STRIDE = 12,
    MODIFIER_POPCORN_LEVELS = 6
};

/* GLOW (ADR-054): a shirt colour (15-bit) made neon: brighter by
 * MODIFIER_NEON_LIFT, and each channel twice as far below the brightest
 * (one and a half, or the same, where that would clip the darkest and
 * turn the hue), so a ramp keeps its light and dark. */
enum { MODIFIER_NEON_LIFT = 6 };
u16 modifiers_neon(u16 color);

/* A saved value with unknown bits cleared. */
u8 modifiers_valid(u8 saved);
u8 modifiers2_valid(u8 saved);

/* The ball's steps on each of `ticks` frames from a cycle's start: the
 * difficulty's slow ball (ADR-025) times BALL SPEED (ADR-049), spread
 * evenly, plus one more every 3rd frame with FAST BALL. 0 to 3 each. */
void modifiers_ball_pace(u8 *pace, u8 ticks, u8 difficulty, u8 modifiers,
    u8 percent);

/* POPCORN: adds (or takes back) level `level`'s steps (0-based) in a pace
 * table of GAME_BALL_PACE_TICKS. A few increments: it runs on a hit. */
void modifiers_popcorn_add(u8 *pace, u8 level);
void modifiers_popcorn_remove(u8 *pace, u8 level);

/* Nonzero when `score` against `other` wins a match to `points`: with WIN
 * BY 2, only by two. A macro: the frames that score and end a point run
 * within a scanline or two of the budget, and 816-tcc calls cost that. */
#define MODIFIERS_SCORE_WINS(score, other, points, modifiers) \
    ((score) >= (points) && \
     (!((modifiers) & MODIFIER_WIN_BY_2) || (score) >= (other) + 2))

/* Nonzero when a ground point is in that team's kitchen: past its kitchen
 * line toward the net, between the sidelines. */
u8 modifiers_in_kitchen(fx x_fp, fx y_fp, u8 team);
/* A CPU's target x moved behind its kitchen line (LAVA). */
s16 modifiers_lava_target(s16 x, u8 team);

/* Modifiers page editor (like the options page, ADR-025). */
enum { MODIFIERS_ROW_BALL = 0, MODIFIERS_ROW_SPEED = 1,
       MODIFIERS_ROW_SCORING = 2, MODIFIERS_ROW_KITCHEN = 3,
       MODIFIERS_ROW_VISION = 4, MODIFIERS_ROW_RALLY = 5,
       MODIFIERS_ROW_FOOTING = 6, MODIFIERS_ROW_LIGHTS = 7,
       MODIFIERS_ROW_COLORS = 8,
       MODIFIERS_ROWS = MODIFIER_COUNT + MODIFIER2_COUNT };
enum { MODIFIERS_EDIT_STAY = 0, MODIFIERS_EDIT_DONE = 1,
       MODIFIERS_EDIT_CANCEL = 2 };

typedef struct {
    u8 row;
    u8 original;
    u8 original2;
} ModifiersEdit;

extern ModifiersEdit g_modifiers_edit;

/* Row labels and the value names (off, on) of each row. Rows 0-7 are
 * modifiers' bits, rows 8 on modifiers2's. */
const char *modifiers_row_label(u8 row);
u8 modifiers_row_on(u8 row, u8 modifiers, u8 modifiers2);
const char *modifiers_value_name(u8 row, u8 modifiers, u8 modifiers2);

/* Opens the editor on the saved values (made valid in place). */
void modifiers_edit_begin(u8 *modifiers, u8 *modifiers2);
/* Up/Down rows, Left/Right toggle (both wrap), B or Start keeps, Y
 * cancels. */
u8 modifiers_edit_input(u8 *modifiers, u8 *modifiers2, u16 pressed);
u8 modifiers_edit_changed(u8 modifiers, u8 modifiers2);

#endif
