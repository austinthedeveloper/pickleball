#include <assert.h>
#include <stdio.h>
#include "input.h"
#include "menu.h"

/* The shared menu steps (R27) and the settings pages' editor keys (R13). */
int main(void)
{
    u8 row;
    u8 i;

    /* Wrapping steps, at both ends and in between. */
    assert(menu_prev(0, 5) == 4);
    assert(menu_prev(3, 5) == 2);
    assert(menu_next(4, 5) == 0);
    assert(menu_next(3, 5) == 4);
    assert(menu_prev(0, 1) == 0 && menu_next(0, 1) == 0);
    assert(menu_step(0, 3, PAD_LEFT) == 2);
    assert(menu_step(2, 3, PAD_RIGHT) == 0);
    assert(menu_step(1, 3, PAD_LEFT | PAD_RIGHT) == 0);     /* Left wins */
    assert(menu_row(0, 4, PAD_UP) == 3);
    assert(menu_row(3, 4, PAD_DOWN) == 0);
    assert(menu_row(2, 4, PAD_LEFT) == 2);                  /* not a row key */
    /* A full lap comes back. */
    for (row = 2, i = 0; i < 7; ++i)
        row = menu_next(row, 7);
    assert(row == 2);

    /* Editor keys: Y before B/Start before Up/Down before Left/Right. */
    row = 1;
    assert(menu_edit_keys(&row, 4, PAD_Y | PAD_B) == MENU_EDIT_CANCEL);
    assert(menu_edit_keys(&row, 4, PAD_B) == MENU_EDIT_DONE);
    assert(menu_edit_keys(&row, 4, PAD_START | PAD_UP) == MENU_EDIT_DONE);
    assert(row == 1);
    assert(menu_edit_keys(&row, 4, PAD_UP | PAD_LEFT) == MENU_EDIT_STAY);
    assert(row == 0);
    assert(menu_edit_keys(&row, 4, PAD_UP) == MENU_EDIT_STAY && row == 3);
    assert(menu_edit_keys(&row, 4, PAD_DOWN) == MENU_EDIT_STAY && row == 0);
    assert(menu_edit_keys(&row, 4, PAD_LEFT) == MENU_EDIT_STEP && row == 0);
    assert(menu_edit_keys(&row, 4, PAD_RIGHT) == MENU_EDIT_STEP);
    assert(menu_edit_keys(&row, 4, PAD_A | PAD_X) == MENU_EDIT_STAY);
    assert(menu_edit_keys(&row, 4, 0) == MENU_EDIT_STAY && row == 0);
    printf("menu host checks passed\n");
    return 0;
}
