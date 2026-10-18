#include <assert.h>
#include <stdio.h>
#include "match.h"
#include "player.h"

/* M4-MP-01: up to four humans, by controller: 1P and 3P on the left, 2P
 * and 4P on the right; singles takes 1P and 2P only. */
static void test_four_players(void)
{
    static const u8 all[4] = { 0, 1, 2, 3 };
    static const u8 one_three[2] = { 0, 2 };
    static const u8 one_four[2] = { 0, 3 };
    PlayerChoice choices[4] = {
        { PAL_BLUE, PLAYER_VARIANT_MALE, PADDLE_WOOD },
        { PAL_RED, PLAYER_VARIANT_FEMALE, PADDLE_BLUE },
        { PAL_GREEN, PLAYER_VARIANT_MALE, PADDLE_RED },
        { PAL_GOLD, PLAYER_VARIANT_FEMALE, PADDLE_GOLD }
    };
    u8 active[MAX_PLAYERS];
    u8 controllers[MAX_PLAYERS];
    u8 slot;

    assert(match_controller_slot(MATCH_DOUBLES, 0) == 0);
    assert(match_controller_slot(MATCH_DOUBLES, 1) == 2);
    assert(match_controller_slot(MATCH_DOUBLES, 2) == 1);
    assert(match_controller_slot(MATCH_DOUBLES, 3) == 3);
    assert(match_controller_slot(MATCH_SINGLES, 1) == 2);
    assert(match_controller_slot(MATCH_SINGLES, 2) == MATCH_CPU);
    assert(match_controller_slot(MATCH_SINGLES, 3) == MATCH_CPU);

    /* Four humans: each choice on its controller's slot, no CPUs. */
    match_config_select(MATCH_DOUBLES, choices, all, 4);
    assert(g_match_config.palette_id[0] == PAL_BLUE);
    assert(g_match_config.palette_id[2] == PAL_RED);    /* 2P, right */
    assert(g_match_config.palette_id[1] == PAL_GREEN);  /* 3P, left */
    assert(g_match_config.palette_id[3] == PAL_GOLD);   /* 4P, right */
    assert(g_match_config.paddle_id[1] == PADDLE_RED);
    for (slot = 0; slot < MAX_PLAYERS; ++slot) {
        assert(g_match_config.human[slot]);
        controllers[slot] = match_slot_controller(slot);
        active[slot] = match_slot_active(slot);
    }
    assert(controllers[0] == 0 && controllers[1] == 2);
    assert(controllers[2] == 1 && controllers[3] == 3);
    assert(!match_vs_cpu() && match_config_palettes_unique());

    /* With a Multitap each slot reads its controller's pad. */
    input_configure(1);
    input_assign_match(active, controllers);
    for (slot = 0; slot < MAX_PLAYERS; ++slot)
        assert(g_input_owners[slot] == INPUT_OWNER_HUMAN);
    assert(g_input_pad_for_player[0] == 0 && g_input_pad_for_player[1] == 2);
    assert(g_input_pad_for_player[2] == 1 && g_input_pad_for_player[3] == 3);
    /* Without one (KNOWN_ISSUES #13: port 2 is a plain pad), 3P and 4P
     * have no pad: their slots fall back to the CPU. */
    input_configure(0);
    input_assign_match(active, controllers);
    assert(g_input_owners[2] == INPUT_OWNER_HUMAN &&
        g_input_pad_for_player[2] == 1);
    assert(g_input_owners[1] == INPUT_OWNER_CPU &&
        g_input_pad_for_player[1] == INPUT_NO_PAD);
    assert(g_input_owners[3] == INPUT_OWNER_CPU);

    /* 1P and 3P against two CPUs: still against the CPU (the ladder). */
    match_config_select(MATCH_DOUBLES, choices, one_three, 2);
    assert(g_match_config.human[1] && !g_match_config.human[2]);
    assert(match_slot_controller(1) == 2 && match_vs_cpu());
    assert(match_slot_controller(2) == MATCH_CPU);
    assert(match_config_palettes_unique());
    /* 1P and 4P: 4P is across the net, beside a CPU. */
    match_config_select(MATCH_DOUBLES, choices, one_four, 2);
    assert(g_match_config.human[3] && !match_vs_cpu());
    assert(match_slot_controller(3) == 3);
    assert(match_slot_controller(1) == MATCH_CPU);

    /* Open play (ADR-034): your side first, slot n is controller n. */
    match_config_doubles();
    g_match_config.open_play = 1;
    g_match_config.human[1] = 1;
    g_match_config.human[2] = 1;
    g_match_config.human[3] = 1;
    for (slot = 0; slot < MAX_PLAYERS; ++slot)
        assert(match_slot_controller(slot) == slot);
    match_config_defaults();
}

int main(void)
{
    u16 pal[16];

    match_config_defaults();
    assert(g_match_config.palette_id[0] == PAL_BLUE);
    assert(g_match_config.palette_id[2] == PAL_RED);
    assert(g_match_config.palette_id[1] == MATCH_NO_PALETTE);
    assert(match_config_palettes_unique());
    assert(match_team_palette(0) == PAL_BLUE);
    assert(match_team_palette(1) == PAL_RED);

    /* Contract v2 composition (ADR-013). */
    match_compose_palette(0, pal);
    assert(pal[0] == 0);
    assert(pal[1] == g_pal_shared[0][0]);
    assert(pal[2] == g_pal_skin[0][0] && pal[4] == g_pal_skin[0][2]);
    assert(pal[5] == g_pal_team[PAL_BLUE][0]);
    assert(pal[10] == g_pal_team[PAL_BLUE][5]);
    assert(pal[11] == g_pal_paddle[PADDLE_STANDARD][0]);
    assert(pal[13] == g_pal_paddle[PADDLE_STANDARD][2]);
    assert(pal[14] == g_pal_shared[0][1]);
    assert(pal[15] == g_pal_skin[0][3]);

    /* Colour is per match, not per slot: gold vs green is valid (ADR-010). */
    g_match_config.palette_id[0] = PAL_GOLD;
    g_match_config.palette_id[2] = PAL_GREEN;
    assert(match_config_palettes_unique());
    assert(match_team_palette(1) == PAL_GREEN);
    match_compose_palette(2, pal);
    assert(pal[6] == g_pal_team[PAL_GREEN][1]);

    g_match_config.palette_id[2] = PAL_GOLD;
    assert(!match_config_palettes_unique());

    /* Player Select (ADR-020): CPUs take the unused colours in order. */
    {
        PlayerChoice p1 = { PAL_BLUE, PLAYER_VARIANT_FEMALE, PADDLE_RED };
        PlayerChoice p2 = { PAL_RED, PLAYER_VARIANT_MALE, PADDLE_WOOD };
        PlayerChoice both[2];
        static const u8 one_two[2] = { 0, 1 };

        match_config_select(MATCH_SINGLES, &p1, one_two, 1);
        assert(g_match_config.palette_id[0] == PAL_BLUE);
        assert(g_match_config.variant[0] == PLAYER_VARIANT_FEMALE);
        assert(g_match_config.paddle_id[0] == PADDLE_RED);
        assert(g_match_config.palette_id[2] == PAL_RED);
        assert(g_match_config.paddle_id[2] == PADDLE_STANDARD);
        assert(!match_slot_active(1) && !match_slot_active(3));
        assert(g_match_config.human[0] && !g_match_config.human[2]);
        assert(match_slot_controller(2) == MATCH_CPU);

        p1.palette_id = PAL_RED;
        match_config_select(MATCH_SINGLES, &p1, one_two, 1);
        assert(g_match_config.palette_id[2] == PAL_BLUE);

        p1.palette_id = PAL_GREEN;
        both[0] = p1;
        both[1] = p2;
        match_config_select(MATCH_DOUBLES, both, one_two, 2);
        assert(g_match_config.palette_id[0] == PAL_GREEN);
        assert(g_match_config.palette_id[1] == PAL_BLUE);
        assert(g_match_config.palette_id[2] == PAL_RED);
        assert(g_match_config.palette_id[3] == PAL_GOLD);
        assert(g_match_config.human[2] && !g_match_config.human[1]);
        /* ADR-066: ring colours follow who plays the slot: 1P, 2P (here
         * versus, on slot 2), and the CPUs. */
        assert(match_slot_controller(0) == 0);
        assert(match_slot_controller(2) == 1);
        assert(match_slot_controller(1) == MATCH_CPU);
        assert(match_slot_controller(3) == MATCH_CPU);
        /* 2P partnering 1P (open play, ADR-034) is on slot 1. */
        g_match_config.open_play = 1;
        g_match_config.human[1] = 1;
        g_match_config.human[2] = 0;
        assert(match_slot_controller(1) == 1);
        assert(match_slot_controller(2) == MATCH_CPU);
        g_match_config.human[1] = 0;
        g_match_config.human[2] = 1;
        g_match_config.open_play = 0;
        assert(g_match_config.paddle_id[2] == PADDLE_WOOD);
        assert(g_match_config.variant[1] == PLAYER_VARIANT_FEMALE);
        assert(match_config_palettes_unique());

        /* Composition follows the chosen paddle, not the team colour. */
        match_compose_palette(2, pal);
        assert(pal[11] == g_pal_paddle[PADDLE_WOOD][0]);
    }
    test_four_players();
    puts("match host checks passed");
    return 0;
}
