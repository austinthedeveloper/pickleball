#include "court.h"
#include "input.h"
#include "menu.h"
#include "modifiers.h"
#include "options.h"

ModifiersEdit g_modifiers_edit;

u8 modifiers_valid(u8 saved)
{
    return saved & MODIFIER_ALL;
}

/* One channel's distance below the brightest, stretched: 2 (halves: 4 is
 * twice, 3 one and a half, 2 the same). */
static u16 neon_channel(s16 v, s16 top, s16 lifted, u8 halves)
{
    s16 d = top - v;

    if (halves == 4)
        d <<= 1;
    else if (halves == 3)
        d += d >> 1;
    return (u16)(lifted - d);
}

u16 modifiers_neon(u16 color)
{
    s16 r = color & 31;
    s16 g = (color >> 5) & 31;
    s16 b = (color >> 10) & 31;
    s16 top = r > g ? r : g;
    s16 low = r < g ? r : g;
    s16 lifted;
    s16 d;
    u8 halves;

    if (b > top)
        top = b;
    if (b < low)
        low = b;
    lifted = top + MODIFIER_NEON_LIFT > 31 ? 31 : top + MODIFIER_NEON_LIFT;
    /* As far as the darkest channel stays at 0 or more: clipping it would
     * turn the hue (an orange went red). */
    d = top - low;
    halves = (d << 1) <= lifted ? 4 : (d + (d >> 1)) <= lifted ? 3 : 2;
    return neon_channel(r, top, lifted, halves) |
        (neon_channel(g, top, lifted, halves) << 5) |
        (neon_channel(b, top, lifted, halves) << 10);
}

u8 modifiers2_valid(u8 saved)
{
    return saved & MODIFIER2_ALL;
}

void modifiers_ball_pace(u8 *pace, u8 ticks, u8 difficulty, u8 modifiers,
    u8 percent)
{
    u8 per_cycle = options_ball_per_cycle(difficulty, percent);
    u8 share = 0;
    u8 tick = 0;
    u8 i;

    /* Even spreading: a step each time the running share passes a cycle. */
    for (i = 0; i < ticks; ++i) {
        pace[i] = 0;
        share += per_cycle;
        while (share >= OPTIONS_BALL_CYCLE) {
            share -= OPTIONS_BALL_CYCLE;
            ++pace[i];
        }
        if ((modifiers & MODIFIER_FAST_BALL) && tick % 3 == 2)
            ++pace[i];
        if (++tick >= OPTIONS_BALL_CYCLE)
            tick = 0;
    }
}

/* The tick each popcorn level starts at, in its stride. None is 2 mod 3,
 * FAST BALL's extra step (tick % 3 == 2): a tick gains one or the other. */
static const u8 s_popcorn_offset[MODIFIER_POPCORN_LEVELS] = {
    0, 6, 3, 9, 1, 7
};

/* GAME_BALL_PACE_TICKS is a multiple of the stride, and so is the cycle:
 * the lookahead entries past the cycle's end get the same steps. */
typedef char popcorn_stride_fits[
    (OPTIONS_BALL_CYCLE % MODIFIER_POPCORN_STRIDE == 0 &&
     (OPTIONS_BALL_CYCLE + 12) % MODIFIER_POPCORN_STRIDE == 0) ? 1 : -1];

void modifiers_popcorn_add(u8 *pace, u8 level)
{
    u8 *p = pace + s_popcorn_offset[level];
    u8 i;

    for (i = 0; i < (OPTIONS_BALL_CYCLE + 12) / MODIFIER_POPCORN_STRIDE;
            ++i, p += MODIFIER_POPCORN_STRIDE)
        ++*p;
}

void modifiers_popcorn_remove(u8 *pace, u8 level)
{
    u8 *p = pace + s_popcorn_offset[level];
    u8 i;

    for (i = 0; i < (OPTIONS_BALL_CYCLE + 12) / MODIFIER_POPCORN_STRIDE;
            ++i, p += MODIFIER_POPCORN_STRIDE)
        --*p;
}

u8 modifiers_in_kitchen(fx x_fp, fx y_fp, u8 team)
{
    if (y_fp < TO_FP(COURT_SIDELINE_TOP) || y_fp > TO_FP(COURT_SIDELINE_BOTTOM))
        return 0;
    if (team)
        return x_fp < TO_FP(COURT_KITCHEN_RIGHT_X) &&
            x_fp > TO_FP(COURT_NET_X);
    return x_fp > TO_FP(COURT_KITCHEN_LEFT_X) && x_fp < TO_FP(COURT_NET_X);
}

s16 modifiers_lava_target(s16 x, u8 team)
{
    if (team)
        return x < COURT_KITCHEN_RIGHT_X + MODIFIER_LAVA_MARGIN ?
            COURT_KITCHEN_RIGHT_X + MODIFIER_LAVA_MARGIN : x;
    return x > COURT_KITCHEN_LEFT_X - MODIFIER_LAVA_MARGIN ?
        COURT_KITCHEN_LEFT_X - MODIFIER_LAVA_MARGIN : x;
}

const char *modifiers_row_label(u8 row)
{
    static const char *const labels[MODIFIERS_ROWS] = {
        "BALL", "SPEED", "KITCHEN", "VISION", "RALLY", "FOOTING", "LIGHTS",
        "COLORS"
    };
    return labels[row < MODIFIERS_ROWS ? row : 0];
}

u8 modifiers_row_bit(u8 row)
{
    static const u8 bits[MODIFIERS_ROWS] = {
        MODIFIER_BIG_BALL, MODIFIER_FAST_BALL, MODIFIER_LAVA,
        MODIFIER_GHOST_BALL, MODIFIER_POPCORN, MODIFIER_ICE, MODIFIER_GLOW, 0
    };
    return bits[row < MODIFIERS_ROWS ? row : 0];
}

u8 modifiers_row_on(u8 row, u8 modifiers, u8 modifiers2)
{
    if (row == MODIFIERS_ROW_COLORS)
        return modifiers2 & MODIFIER2_INVERSE;
    return (modifiers & modifiers_row_bit(row)) != 0;
}

const char *modifiers_value_name(u8 row, u8 modifiers, u8 modifiers2)
{
    static const char *const names[MODIFIERS_ROWS][2] = {
        { "NORMAL", "BIG" }, { "NORMAL", "FAST" }, { "NORMAL", "LAVA" },
        { "NORMAL", "GHOST" }, { "NORMAL", "POPCORN" }, { "NORMAL", "ICE" },
        { "NORMAL", "GLOW" }, { "NORMAL", "INVERSE" }
    };
    u8 on;

    if (row >= MODIFIERS_ROWS)
        row = 0;
    /* A local: the call inside the index stops 816-tcc ("ICE 2"). */
    on = modifiers_row_on(row, modifiers, modifiers2);
    return names[row][on];
}

void modifiers_edit_begin(u8 *modifiers, u8 *modifiers2)
{
    *modifiers = modifiers_valid(*modifiers);
    *modifiers2 = modifiers2_valid(*modifiers2);
    g_modifiers_edit.row = MODIFIERS_ROW_BALL;
    g_modifiers_edit.original = *modifiers;
    g_modifiers_edit.original2 = *modifiers2;
}

u8 modifiers_edit_changed(u8 modifiers, u8 modifiers2)
{
    return modifiers != g_modifiers_edit.original ||
        modifiers2 != g_modifiers_edit.original2;
}

u8 modifiers_edit_input(u8 *modifiers, u8 *modifiers2, u16 pressed)
{
    u8 row = g_modifiers_edit.row;
    u8 keys;

    /* Most frames press nothing: skip the call (R33). */
    if (pressed == 0)
        return MENU_EDIT_STAY;
    keys = menu_edit_keys(&g_modifiers_edit.row, MODIFIERS_ROWS, pressed);
    if (keys == MENU_EDIT_CANCEL) {
        *modifiers = g_modifiers_edit.original;
        *modifiers2 = g_modifiers_edit.original2;
        return MODIFIERS_EDIT_CANCEL;
    }
    if (keys != MENU_EDIT_STEP)
        return keys;
    if (row == MODIFIERS_ROW_COLORS)
        *modifiers2 ^= MODIFIER2_INVERSE;
    else
        *modifiers ^= modifiers_row_bit(row);
    return MODIFIERS_EDIT_STAY;
}
