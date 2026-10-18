#include "input.h"
#include "menu.h"

u8 menu_prev(u8 value, u8 count)
{
    return value ? value - 1 : count - 1;
}

u8 menu_next(u8 value, u8 count)
{
    return value + 1 >= count ? 0 : value + 1;
}

u8 menu_step(u8 value, u8 count, u16 pressed)
{
    if (pressed & PAD_LEFT)
        return menu_prev(value, count);
    return menu_next(value, count);
}

u8 menu_row(u8 row, u8 count, u16 pressed)
{
    if (pressed & PAD_UP)
        return menu_prev(row, count);
    if (pressed & PAD_DOWN)
        return menu_next(row, count);
    return row;
}

u8 menu_edit_keys(u8 *row, u8 rows, u16 pressed)
{
    if (pressed & PAD_Y)
        return MENU_EDIT_CANCEL;
    if (pressed & (PAD_B | PAD_START))
        return MENU_EDIT_DONE;
    if (pressed & (PAD_UP | PAD_DOWN)) {
        *row = menu_row(*row, rows, pressed);
        return MENU_EDIT_STAY;
    }
    if (pressed & (PAD_LEFT | PAD_RIGHT))
        return MENU_EDIT_STEP;
    return MENU_EDIT_STAY;
}
