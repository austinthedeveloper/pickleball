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
