#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "museum.h"
#include "input.h"
#include "test_support.h"

/* ADR-061: the HAT MUSEUM's input. */

static u8 unlock_bit(const char *name)
{
    u8 i;

    for (i = 0; i < CAREER_UNLOCKS; ++i)
        if (strcmp(g_career_unlock_names[i], name) == 0)
            return i;
    assert(0);
    return 0;
}

/* The case holding a named hat. */
static u8 case_of(const char *name)
{
    u8 i;

    for (i = 0; i < MUSEUM_HATS; ++i)
        if (strcmp(g_career_hat_names[museum_hat(i)], name) == 0)
            return i;
    assert(0);
    return 0;
}

/* A blank save with a new player in slot 1 (made active). */
static CareerPlayer *fresh(void)
{
    memset(g_host_sram, 0, sizeof(g_host_sram));
    save_initialize();
    assert(career_create(0, "MAX"));
    return career_active();
}

static void test_cases(void)
{
    u8 i;

    fresh();
    museum_begin(0);
    /* Every hat but the bare head, in id order: the cap first. */
    assert(MUSEUM_HATS == CAREER_HATS - 1);
    assert(museum_hat(0) == 0);
    for (i = 0; i < MUSEUM_HATS; ++i)
        assert(museum_hat(i) != CAREER_HAT_NONE && museum_hat(i) < CAREER_HATS);
    assert(museum_hat(MUSEUM_HATS - 1) == CAREER_HATS - 1);
    /* A new player holds the cap only. */
    assert(museum_unlocked(0) && museum_unlocked_count() == 1);
    assert(!museum_unlocked(case_of("CROWN")));
    assert(g_museum.cursor == 0 && g_museum.changed);
}

static void test_moves(void)
{
    u8 last = MUSEUM_HATS - 1;

    fresh();
    museum_begin(0);
    museum_input(PAD_LEFT);                     /* wraps to the last */
    assert(g_museum.cursor == last);
    museum_input(PAD_RIGHT);
    assert(g_museum.cursor == 0);
    museum_input(PAD_DOWN);
    assert(g_museum.cursor == MUSEUM_COLS);
    museum_input(PAD_UP);
    museum_input(PAD_UP);                       /* wraps to the bottom row */
    assert(g_museum.cursor == (MUSEUM_ROWS - 1) * MUSEUM_COLS);
    museum_input(PAD_DOWN);                     /* and back to the top */
    assert(g_museum.cursor == 0);
    /* Every case is reachable, and moves stay in the grid. */
    for (g_museum.cursor = 0; g_museum.cursor <= last; ++g_museum.cursor) {
        u8 at = g_museum.cursor;
        u16 pads[4] = { PAD_UP, PAD_DOWN, PAD_LEFT, PAD_RIGHT };
        u8 k;

        for (k = 0; k < 4; ++k) {
            g_museum.cursor = at;
            museum_input(pads[k]);
            assert(g_museum.cursor <= last);
        }
        g_museum.cursor = at;
    }
}

static void test_wear(void)
{
    CareerPlayer *p = fresh();
    u8 crown = case_of("CROWN");

    museum_begin(0);
    /* A locked hat can't be worn. */
    g_museum.cursor = crown;
    assert(museum_input(PAD_B) == MUSEUM_STAY && p->hat == 0);
    /* Unlocked, it can; the save is written on leaving. */
    p->unlocked |= (u16)(1 << unlock_bit("CROWN"));
    assert(museum_unlocked(crown) && museum_unlocked_count() == 2);
    g_museum.changed = 0;
    assert(museum_input(PAD_B) == MUSEUM_STAY);
    assert(p->hat == museum_hat(crown) && g_museum.changed && g_museum.worn);
    assert(museum_input(PAD_Y) == MUSEUM_CLOSE_SAVE);
    /* Again: nothing to do. */
    assert(museum_input(PAD_B) == MUSEUM_STAY);
    /* It opens on the worn hat; bareheaded, on the cap. */
    museum_begin(0);
    assert(g_museum.cursor == crown);
    p->hat = CAREER_HAT_NONE;
    museum_begin(0);
    assert(g_museum.cursor == 0);
    /* Y or Start leaves. */
    assert(museum_input(PAD_Y) == MUSEUM_CLOSE);
    assert(museum_input(PAD_START) == MUSEUM_CLOSE);
}

int main(void)
{
    test_cases();
    test_moves();
    test_wear();
    puts("museum host checks passed");
    return 0;
}
