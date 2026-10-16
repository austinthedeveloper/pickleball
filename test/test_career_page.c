#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "career_page.h"
#include "input.h"
#include "palette_parts.h"
#include "test_support.h"

/* ADR-047 (M4-CAREER-02): the CAREER page's input. */

static u8 press(u16 pressed)
{
    return career_page_input(pressed, pressed);
}

static void fresh(void)
{
    memset(g_host_sram, 0, sizeof(g_host_sram));
    save_initialize();
    career_page_begin();
}

static void test_create_and_name(void)
{
    CareerPlayer *p;
    u8 i;

    fresh();
    assert(g_career_page.view == CAREER_VIEW_SLOTS && g_career_page.row == 0);
    /* Slot 2: a new player, typing the name at once. */
    press(PAD_DOWN);
    press(PAD_DOWN);
    assert(press(PAD_B) == CAREER_PAGE_STAY);
    assert(g_career_page.view == CAREER_VIEW_EDIT && g_career_page.naming);
    assert(g_career_page.creating && g_save.career_active == 2);
    p = &g_save.career[2];
    /* "ACE": A is there; B to the next, Up twice (B, C), B, Up 4 times. */
    press(PAD_B);
    press(PAD_UP);
    press(PAD_UP);
    assert(p->name[1] == 'B');
    press(PAD_UP);
    assert(p->name[1] == 'C');
    press(PAD_LEFT);
    press(PAD_DOWN);
    assert(p->name[0] == ' ');                  /* A back to a space */
    press(PAD_UP);
    press(PAD_RIGHT);
    press(PAD_RIGHT);
    for (i = 0; i < 4; ++i)
        press(PAD_UP);
    assert(p->name[2] == 'D');
    press(PAD_DOWN);
    assert(memcmp(p->name, "ACC     ", CAREER_NAME) == 0);
    /* A held Up repeats after the delay. */
    for (i = 0; i < CAREER_REPEAT_DELAY; ++i)
        career_page_input(0, PAD_UP);
    assert(p->name[2] == 'D');
    press(PAD_START);                           /* done typing */
    assert(!g_career_page.naming);
    /* B on another row keeps: the card, and a save. */
    press(PAD_DOWN);
    assert(press(PAD_B) == CAREER_PAGE_SAVE);
    assert(g_career_page.view == CAREER_VIEW_CARD && !g_career_page.creating);
    assert(memcmp(p->name, "ACD     ", CAREER_NAME) == 0);
}

static void test_cancel_new_player(void)
{
    fresh();
    press(PAD_B);                               /* slot 0: new */
    press(PAD_START);                           /* stop typing */
    press(PAD_Y);                               /* never mind */
    assert(g_career_page.view == CAREER_VIEW_SLOTS);
    assert(g_save.career[0].used == 0 && career_active() == 0);
    /* A name of spaces is no name. */
    press(PAD_B);
    press(PAD_DOWN);                            /* A to a space */
    press(PAD_START);
    assert(g_save.career[0].name[0] == 'A');
}

static void test_edit_look(void)
{
    CareerPlayer *p;

    fresh();
    career_create(1, "MAX");
    career_page_begin();
    assert(g_career_page.row == 1);             /* on the active player */
    assert(press(PAD_B) == CAREER_PAGE_SAVE);   /* the card */
    press(PAD_B);                               /* edit */
    assert(g_career_page.view == CAREER_VIEW_EDIT && !g_career_page.naming);
    p = &g_save.career[1];
    g_career_page.look_changed = 0;
    press(PAD_DOWN);                            /* BODY */
    press(PAD_RIGHT);
    assert(p->variant == PLAYER_VARIANT_FEMALE && g_career_page.look_changed);
    press(PAD_DOWN);                            /* SKIN */
    press(PAD_LEFT);
    assert(p->skin == CAREER_SKINS - 1);
    press(PAD_DOWN);                            /* HAIR: the locked skipped */
    press(PAD_LEFT);
    assert(career_hair_unlocked(p, p->hair) && p->hair != 0);
    press(PAD_DOWN);                            /* SHIRT */
    press(PAD_RIGHT);
    assert(p->shirt == PAL_RED);
    press(PAD_DOWN);                            /* HAT: nothing won yet */
    press(PAD_RIGHT);
    assert(p->hat == CAREER_HATS - 1);          /* past the locked: NONE */
    press(PAD_RIGHT);
    assert(p->hat == 0);
    /* Y puts it all back. */
    press(PAD_Y);
    assert(g_career_page.view == CAREER_VIEW_CARD);
    assert(p->variant == PLAYER_VARIANT_MALE && p->skin == 0 && p->hair == 0);
    assert(p->shirt == PAL_BLUE);
    /* An unlocked hat can be worn. */
    p->unlocked = 1;                            /* VISOR */
    press(PAD_B);
    for (g_career_page.row = 0; g_career_page.row != CAREER_EDIT_HAT;)
        press(PAD_DOWN);
    press(PAD_RIGHT);
    assert(p->hat == 1);
    assert(press(PAD_B) == CAREER_PAGE_SAVE && p->hat == 1);
}

static void test_delete_and_records(void)
{
    fresh();
    career_create(0, "ACE");
    career_page_begin();
    press(PAD_B);                               /* the card */
    /* X alone does nothing; L+R+X deletes. */
    press(PAD_X);
    assert(g_save.career[0].used);
    assert(career_page_input(PAD_X, PAD_L | PAD_R | PAD_X) ==
        CAREER_PAGE_SAVE);
    assert(g_save.career[0].used == 0 && career_active() == 0);
    assert(g_career_page.view == CAREER_VIEW_SLOTS);
    /* RECORDS, erase, and back. */
    g_save.records.wins[0] = 9;
    press(PAD_UP);                              /* wraps to RECORDS */
    assert(g_career_page.row == CAREER_ROW_RECORDS);
    press(PAD_B);
    assert(g_career_page.view == CAREER_VIEW_RECORDS);
    career_page_input(PAD_X, PAD_L | PAD_R | PAD_X);
    assert(g_save.records.wins[0] == 0);
    press(PAD_Y);
    assert(g_career_page.view == CAREER_VIEW_SLOTS);
    assert(press(PAD_Y) == CAREER_PAGE_CLOSE);
}

static void test_card_helpers(void)
{
    CareerPlayer p;

    memset(&p, 0, sizeof(p));
    assert(career_next_unlock(&p) == 0 && career_unlock_count(&p) == 0);
    p.unlocked = 0x0b;                          /* 0, 1, 3 */
    assert(career_next_unlock(&p) == 2 && career_unlock_count(&p) == 3);
    p.unlocked = (1 << CAREER_UNLOCKS) - 1;
    assert(career_next_unlock(&p) == CAREER_UNLOCKS);
    assert(strcmp(career_paddle_name(0), "WOOD") == 0);
    assert(strcmp(career_body_name(1), "FEMALE") == 0);
}

int main(void)
{
    test_create_and_name();
    test_cancel_new_player();
    test_edit_look();
    test_delete_and_records();
    test_card_helpers();
    puts("career page host checks passed");
    return 0;
}
