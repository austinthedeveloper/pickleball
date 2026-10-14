#include "court.h"
#include "input.h"
#include "modifiers.h"
#include "options.h"

ModifiersEdit g_modifiers_edit;

u8 modifiers_valid(u8 saved)
{
    return saved & MODIFIER_ALL;
}

u8 modifiers_ball_steps(u8 difficulty, u8 modifiers, u8 tick)
{
    u8 steps = options_ball_moves(difficulty, tick);

    if ((modifiers & MODIFIER_FAST_BALL) && tick % 3 == 2)
        ++steps;
    return steps;
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
        "BALL", "SPEED", "SCORING", "KITCHEN"
    };
    return labels[row < MODIFIERS_ROWS ? row : 0];
}

const char *modifiers_value_name(u8 row, u8 modifiers)
{
    static const char *const names[MODIFIERS_ROWS][2] = {
        { "NORMAL", "BIG" }, { "NORMAL", "FAST" },
        { "FIRST TO", "WIN BY 2" }, { "NORMAL", "LAVA" }
    };
    if (row >= MODIFIERS_ROWS)
        row = 0;
    return names[row][(modifiers >> row) & 1];
}

void modifiers_edit_begin(u8 *modifiers)
{
    *modifiers = modifiers_valid(*modifiers);
    g_modifiers_edit.row = MODIFIERS_ROW_BALL;
    g_modifiers_edit.original = *modifiers;
}

u8 modifiers_edit_changed(u8 modifiers)
{
    return modifiers != g_modifiers_edit.original;
}

u8 modifiers_edit_input(u8 *modifiers, u16 pressed)
{
    u8 row = g_modifiers_edit.row;

    if (pressed & PAD_Y) {
        *modifiers = g_modifiers_edit.original;
        return MODIFIERS_EDIT_CANCEL;
    }
    if (pressed & (PAD_B | PAD_START))
        return MODIFIERS_EDIT_DONE;
    if (pressed & PAD_UP)
        g_modifiers_edit.row = row ? row - 1 : MODIFIERS_ROWS - 1;
    else if (pressed & PAD_DOWN)
        g_modifiers_edit.row = row == MODIFIERS_ROWS - 1 ? 0 : row + 1;
    else if (pressed & (PAD_LEFT | PAD_RIGHT))
        *modifiers ^= (u8)(1 << row);
    return MODIFIERS_EDIT_STAY;
}
