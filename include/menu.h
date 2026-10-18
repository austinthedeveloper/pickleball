#ifndef MENU_H
#define MENU_H

#include "game_types.h"

/*
 * Menu cursor steps shared by the title, the pause menu, and the settings
 * and career pages: a value in 0..count-1 that wraps at both ends. Pure.
 */

/* value - 1, or count - 1 from 0. */
u8 menu_prev(u8 value, u8 count);
/* value + 1, or 0 from count - 1. */
u8 menu_next(u8 value, u8 count);
/* Left: menu_prev; anything else: menu_next. */
u8 menu_step(u8 value, u8 count, u16 pressed);
/* Up: menu_prev; Down: menu_next; neither: row. */
u8 menu_row(u8 row, u8 count, u16 pressed);

/*
 * The settings pages' editor keys (COURT, OPTIONS, PRACTICE, MODIFIERS;
 * R13). Every page: Y puts the values back (CANCEL), B or Start keeps them
 * (DONE), Up/Down moves the row (wrapping), Left/Right changes the row's
 * value (STEP: the page steps it). The pages' own results equal these.
 */
enum { MENU_EDIT_STAY = 0, MENU_EDIT_DONE = 1, MENU_EDIT_CANCEL = 2,
       MENU_EDIT_STEP = 3 };
/* One frame of newly pressed buttons; Up/Down moves *row and is STAY. */
u8 menu_edit_keys(u8 *row, u8 rows, u16 pressed);

#endif
