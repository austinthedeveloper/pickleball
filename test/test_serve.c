#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "serve.h"

static PlayerIntent intents[MAX_PLAYERS];

static void reset_fixture(void)
{
    memset(intents, 0, sizeof(intents));
    players_initialize();
    ball_initialize();
    game_initialize();
    serve_initialize();
}

static void test_setup_and_toss(void)
{
    u8 frame;

    reset_fixture();
    assert(g_game_state.phase == GAME_PHASE_SERVE_SETUP);
    assert(g_ball.rally_state == BALL_RALLY_INACTIVE);
    assert(g_ball.x_fp == g_players[0].x_fp + TO_FP(12));
    assert(g_ball.y_fp == g_players[0].y_fp);

    intents[0].hit_pressed = 1;
    serve_update(intents, 0);
    assert(g_game_state.phase == GAME_PHASE_SERVE_TOSS);
    assert(g_ball.z_fp == SERVE_TOSS_RISE_FP);
    intents[0].hit_pressed = 0;
    for (frame = 1; frame < SERVE_TOSS_FRAMES; ++frame)
        serve_update(intents, 0);

    assert(g_game_state.phase == GAME_PHASE_LIVE);
    assert(g_ball.rally_state == BALL_RALLY_LIVE);
    assert(g_ball.last_hitter == 0);
    assert(g_serve_state.landing_pending == 1);
}

static void test_neutral_serve_lands_diagonal(void)
{
    u8 events = BALL_EVENT_NONE;
    u8 frame;

    reset_fixture();
    intents[0].hit_pressed = 1;
    serve_update(intents, 0);
    intents[0].hit_pressed = 0;
    for (frame = 1; frame < SERVE_TOSS_FRAMES; ++frame)
        serve_update(intents, 0);

    for (frame = 0; frame < 60; ++frame) {
        events = ball_simulate_step(&g_ball);
        serve_process_ball_events(events);
        if (events & BALL_EVENT_BOUNCE)
            break;
    }

    assert(frame == SERVE_FLIGHT_FRAMES - 1);
    assert(g_ball.x_fp > TO_FP(SERVE_TARGET_NEUTRAL_RIGHT) - 64);
    assert(g_ball.x_fp < TO_FP(SERVE_TARGET_NEUTRAL_RIGHT) + 64);
    assert(g_ball.y_fp > TO_FP(SERVE_TARGET_LANE_BOTTOM) - 64);
    assert(g_ball.y_fp < TO_FP(SERVE_TARGET_LANE_BOTTOM) + 64);
    assert(g_game_state.last_fault == GAME_FAULT_NONE);
    assert(g_game_state.phase == GAME_PHASE_LIVE);
    assert(g_serve_state.landing_pending == 0);
}

static void test_service_box_faults(void)
{
    assert(serve_validate_landing(TO_FP(200), TO_FP(168), 0,
        TO_FP(112)) == GAME_FAULT_NONE);
    assert(serve_validate_landing(TO_FP(200), TO_FP(72), 0,
        TO_FP(112)) == GAME_FAULT_WRONG_BOX);
    assert(serve_validate_landing(TO_FP(COURT_KITCHEN_RIGHT_X), TO_FP(168),
        0, TO_FP(112)) == GAME_FAULT_KITCHEN);
    assert(serve_validate_landing(TO_FP(COURT_BASELINE_RIGHT) + 1,
        TO_FP(168), 0, TO_FP(112)) == GAME_FAULT_OUT);

    assert(serve_validate_landing(TO_FP(56), TO_FP(72), 1,
        TO_FP(168)) == GAME_FAULT_NONE);
    assert(serve_validate_landing(TO_FP(56), TO_FP(168), 1,
        TO_FP(168)) == GAME_FAULT_WRONG_BOX);
    assert(serve_validate_landing(TO_FP(COURT_KITCHEN_LEFT_X), TO_FP(72),
        1, TO_FP(168)) == GAME_FAULT_KITCHEN);
}

static void test_net_fault_stops_serve(void)
{
    reset_fixture();
    g_game_state.phase = GAME_PHASE_LIVE;
    g_serve_state.landing_pending = 1;
    g_ball.rally_state = BALL_RALLY_LIVE;
    serve_process_ball_events(BALL_EVENT_NET_FAULT);
    assert(g_game_state.phase == GAME_PHASE_SERVE_FAULT);
    assert(g_game_state.last_fault == GAME_FAULT_NET);
    assert(g_serve_state.landing_pending == 0);
    assert(g_ball.rally_state == BALL_RALLY_INACTIVE);
}

static void test_pause_freezes_toss(void)
{
    reset_fixture();
    intents[0].hit_pressed = 1;
    serve_update(intents, 0);
    serve_update(intents, 1);
    assert(g_game_state.phase_timer == 1);
    assert(g_ball.z_fp == SERVE_TOSS_RISE_FP);
}

int main(void)
{
    test_setup_and_toss();
    test_neutral_serve_lands_diagonal();
    test_service_box_faults();
    test_net_fault_stops_serve();
    test_pause_freezes_toss();
    puts("serve host checks passed");
    return 0;
}
