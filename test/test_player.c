#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "court.h"
#include "game.h"
#include "player.h"

static PlayerIntent intents[MAX_PLAYERS];

static void reset_test(void)
{
    memset(intents, 0, sizeof(intents));
    players_initialize();
}

static void test_initialization(void)
{
    u8 i;
    reset_test();
    assert(g_players[0].active == 1);
    assert(g_players[2].active == 0);
    assert(g_players[2].cpu_controlled == 1);
    for (i = 1; i < MAX_PLAYERS; ++i)
        assert(g_players[i].active == 0);
}

static void test_zero_input_and_pause(void)
{
    s32 x;
    reset_test();
    x = g_players[0].x_fp;
    players_update(intents, 0);
    assert(g_players[0].x_fp == x);
    intents[0].move_x = 1;
    players_update(intents, 1);
    assert(g_players[0].x_fp == x);
}

static void test_diagonal_normalization(void)
{
    reset_test();
    intents[0].move_x = 1;
    intents[0].move_y = 1;
    players_update(intents, 0);
    assert(g_players[0].vx_fp == 17);
    assert(g_players[0].vy_fp == 17);
}

static void test_sign_change_and_friction(void)
{
    reset_test();
    intents[0].move_x = 1;
    players_update(intents, 0);
    assert(g_players[0].vx_fp > 0);
    intents[0].move_x = 0;
    players_update(intents, 0);
    assert(g_players[0].vx_fp >= 0);
    intents[0].move_x = -1;
    players_update(intents, 0);
    players_update(intents, 0);
    assert(g_players[0].vx_fp < 0);
}

static void test_all_clamps(void)
{
    reset_test();
    g_players[0].x_fp = TO_FP(PLAYER_CLAMP_MIN_X) - 100;
    g_players[0].y_fp = TO_FP(PLAYER_CLAMP_MIN_Y) - 100;
    players_update(intents, 0);
    assert(g_players[0].x_fp == TO_FP(PLAYER_CLAMP_MIN_X));
    assert(g_players[0].y_fp == TO_FP(PLAYER_CLAMP_MIN_Y));

    g_players[0].x_fp = TO_FP(PLAYER_CLAMP_MAX_X) + 100;
    g_players[0].y_fp = TO_FP(PLAYER_CLAMP_MAX_Y) + 100;
    players_update(intents, 0);
    assert(g_players[0].x_fp == TO_FP(PLAYER_CLAMP_MAX_X));
    assert(g_players[0].y_fp == TO_FP(PLAYER_CLAMP_MAX_Y));
}

static void test_input_edges(void)
{
    PadSnapshot snapshot = {0, 0, 0};
    input_update_snapshot(&snapshot, 0x8000);
    assert(snapshot.held == 0x8000);
    assert(snapshot.pressed == 0x8000);
    assert(snapshot.released == 0);

    input_update_snapshot(&snapshot, 0x8000);
    assert(snapshot.pressed == 0);
    assert(snapshot.released == 0);

    input_update_snapshot(&snapshot, 0x1000);
    assert(snapshot.held == 0x1000);
    assert(snapshot.pressed == 0x1000);
    assert(snapshot.released == 0x8000);
}

static void test_twenty_pause_toggles(void)
{
    u8 i;
    reset_test();
    game_initialize();
    for (i = 0; i < 20; ++i) {
        intents[0].pause_pressed = 1;
        game_update_top_level(intents);
        assert(g_game_state.paused == ((i + 1) & 1));
    }
    assert(g_game_state.paused == 0);
}

int main(void)
{
    test_initialization();
    test_zero_input_and_pause();
    test_diagonal_normalization();
    test_sign_change_and_friction();
    test_all_clamps();
    test_input_edges();
    test_twenty_pause_toggles();
    puts("player host checks passed");
    return 0;
}

