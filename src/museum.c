#include "museum.h"
#include "input.h"

Museum g_museum;

/* The bare head has no case: the cases skip its id. */
u8 museum_hat(u8 index)
{
    return index < CAREER_HAT_NONE ? index : index + 1;
}

CareerPlayer *museum_player(void)
{
    return &g_save.career[g_museum.slot];
}

u8 museum_unlocked(u8 index)
{
    return career_hat_unlocked(museum_player(), museum_hat(index));
}

u8 museum_unlocked_count(void)
{
    u8 count = 0;
    u8 i;

    for (i = 0; i < MUSEUM_HATS; ++i)
        count += museum_unlocked(i);
    return count;
}

void museum_begin(u8 slot)
{
    u8 hat;

    g_museum.slot = slot;
    hat = museum_player()->hat;
    if (hat == CAREER_HAT_NONE || hat >= CAREER_HATS)
        g_museum.cursor = 0;
    else
        g_museum.cursor = hat < CAREER_HAT_NONE ? hat : hat - 1;
    g_museum.changed = 1;
    g_museum.worn = 0;
}

/* The D-pad over the grid: Left/Right along a row, Up/Down along a
 * column, both wrapping; a short last row is skipped going down into it
 * past its end, and entered at its end going up. */
static u8 museum_move(u8 cursor, u16 pressed)
{
    u8 row = cursor / MUSEUM_COLS;
    u8 col = cursor % MUSEUM_COLS;
    u8 last = MUSEUM_HATS - 1;

    if (pressed & PAD_LEFT)
        return cursor ? cursor - 1 : last;
    if (pressed & PAD_RIGHT)
        return cursor < last ? cursor + 1 : 0;
    if (pressed & PAD_UP) {
        if (row)
            return cursor - MUSEUM_COLS;
        row = MUSEUM_ROWS - 1;
        cursor = row * MUSEUM_COLS + col;
        return cursor > last ? cursor - MUSEUM_COLS : cursor;
    }
    if (pressed & PAD_DOWN) {
        cursor += MUSEUM_COLS;
        return cursor > last ? col : cursor;
    }
    return cursor;
}

u8 museum_input(u16 pressed)
{
    u8 moved;
    CareerPlayer *player;

    if (pressed & (PAD_Y | PAD_START))
        return g_museum.worn ? MUSEUM_CLOSE_SAVE : MUSEUM_CLOSE;
    if (pressed & PAD_B) {
        player = museum_player();
        if (museum_unlocked(g_museum.cursor) &&
                player->hat != museum_hat(g_museum.cursor)) {
            player->hat = museum_hat(g_museum.cursor);
            g_museum.changed = 1;
            g_museum.worn = 1;
        }
        return MUSEUM_STAY;
    }
    moved = museum_move(g_museum.cursor, pressed);
    if (moved != g_museum.cursor) {
        g_museum.cursor = moved;
        g_museum.changed = 1;
    }
    return MUSEUM_STAY;
}
