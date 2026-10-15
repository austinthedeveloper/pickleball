#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "serve.h"

static PlayerIntent intents[MAX_PLAYERS];

static void reset_fixture(u8 serving_team)
{
    memset(intents, 0, sizeof(intents));
    players_initialize();
    g_players[2].active = 1;
    ball_initialize();
    game_initialize();
    serve_initialize();
    serve_setup(serving_team);
}

/* Mirrors game_update(): the phase timer advances before serve_update. */
static void step(void)
{
    ++g_game_state.phase_timer;
    serve_update(intents, 0);
}

static void serve_now(u8 slot)
{
    while (g_game_state.phase_timer < GAME_SERVE_READY_FRAMES)
        step();
    intents[slot].hit_pressed = 1;
    step();
    intents[slot].hit_pressed = 0;
    while (g_game_state.phase == GAME_PHASE_SERVE_TOSS)
        step();
}

static u8 fly_to_first_bounce(void)
{
    u8 events = BALL_EVENT_NONE;
    u8 frame;

    for (frame = 0; frame < 80; ++frame) {
        events = ball_simulate_step(&g_ball);
        serve_process_ball_events(events);
        if (events & (BALL_EVENT_BOUNCE | BALL_EVENT_NET_FAULT))
            break;
    }
    assert(events & BALL_EVENT_BOUNCE);
    return frame;
}

static void test_setup_positions(void)
{
    reset_fixture(0);
    assert(g_game_state.phase == GAME_PHASE_SERVE_SETUP);
    assert(g_serve_state.server_index == 0);
    assert(g_serve_state.receiver_index == 2);
    /* Even score: the left server stands on its right (bottom) half. */
    assert(g_players[0].x_fp ==
        TO_FP(COURT_BASELINE_LEFT - SERVE_BASELINE_GAP));
    assert(g_players[0].y_fp > TO_FP(COURT_CENTERLINE_Y));
    assert(g_players[2].y_fp < TO_FP(COURT_CENTERLINE_Y));
    /* Both stand behind their baselines, not on them. */
    assert(g_players[2].x_fp ==
        TO_FP(COURT_BASELINE_RIGHT + SERVE_RECEIVER_GAP));
    assert(g_players[0].frozen == 1);
    assert(g_players[2].frozen == 0);
    assert(g_ball.rally_state == BALL_RALLY_INACTIVE);
    assert(g_ball.x_fp == g_players[0].x_fp + TO_FP(SERVE_HAND_OFFSET_X));
    assert(g_ball.z_fp == TO_FP(SERVE_HAND_Z));

    g_game_state.team_score[0] = 1;
    serve_setup(0);
    assert(g_players[0].y_fp < TO_FP(COURT_CENTERLINE_Y));
    assert(g_players[2].y_fp > TO_FP(COURT_CENTERLINE_Y));

    reset_fixture(1);
    assert(g_serve_state.server_index == 2);
    /* The right team faces -x, so its right-hand court is the top half. */
    assert(g_players[2].y_fp < TO_FP(COURT_CENTERLINE_Y));
    assert(g_players[2].x_fp ==
        TO_FP(COURT_BASELINE_RIGHT + SERVE_BASELINE_GAP));
    assert(g_players[0].x_fp ==
        TO_FP(COURT_BASELINE_LEFT - SERVE_RECEIVER_GAP));
    assert(g_ball.x_fp == g_players[2].x_fp - TO_FP(SERVE_HAND_OFFSET_X));
    /* The ball is struck behind the baseline too. */
    assert(g_ball.x_fp > TO_FP(COURT_BASELINE_RIGHT));
}

static void test_early_press_is_buffered_and_toss_runs(void)
{
    reset_fixture(0);
    intents[0].hit_pressed = 1;
    step();
    intents[0].hit_pressed = 0;
    assert(g_game_state.phase == GAME_PHASE_SERVE_SETUP);
    while (g_game_state.phase == GAME_PHASE_SERVE_SETUP)
        step();
    assert(g_game_state.phase == GAME_PHASE_SERVE_TOSS);
    assert(g_players[0].pose == PLAYER_POSE_TOSS);
    while (g_game_state.phase == GAME_PHASE_SERVE_TOSS) {
        assert(g_ball.z_fp > 0);
        step();
    }
    assert(g_game_state.phase == GAME_PHASE_LIVE);
    assert(g_ball.rally_state == BALL_RALLY_LIVE);
    assert(g_ball.last_hitter == 0);
    assert(g_serve_state.landing_pending == 1);
    assert(g_game_state.required_bounce == GAME_REQUIRED_BOUNCE_SERVE);
    assert(g_players[0].frozen == 0);
}

static void test_neutral_serves_land_diagonal(void)
{
    u8 frames;

    reset_fixture(0);
    serve_now(0);
    frames = fly_to_first_bounce();
    /* Integer vz truncation may land the serve one frame early or late. */
    assert(frames + 2 >= SERVE_FLIGHT_FRAMES);
    assert(frames <= SERVE_FLIGHT_FRAMES);
    assert(g_ball.x_fp > TO_FP(SERVE_TARGET_NEUTRAL_RIGHT - 3));
    assert(g_ball.x_fp < TO_FP(SERVE_TARGET_NEUTRAL_RIGHT + 3));
    assert(g_ball.y_fp < TO_FP(COURT_CENTERLINE_Y));
    assert(g_game_state.last_fault == GAME_FAULT_NONE);
    assert(g_game_state.phase == GAME_PHASE_LIVE);

    reset_fixture(1);
    serve_now(2);
    fly_to_first_bounce();
    assert(g_ball.x_fp < TO_FP(COURT_KITCHEN_LEFT_X));
    assert(g_ball.y_fp > TO_FP(COURT_CENTERLINE_Y));
    assert(g_game_state.last_fault == GAME_FAULT_NONE);

    reset_fixture(0);
    intents[0].aim_x = 1;
    intents[0].aim_y = -1;
    serve_now(0);
    fly_to_first_bounce();
    assert(g_ball.x_fp > TO_FP(SERVE_TARGET_DEEP_RIGHT - 3));
    assert(g_game_state.last_fault == GAME_FAULT_NONE);
}

static void test_service_box_faults(void)
{
    fx bottom_server = TO_FP(COURT_CENTERLINE_Y + SERVE_STANCE_OFFSET_Y);
    fx top_server = TO_FP(COURT_CENTERLINE_Y - SERVE_STANCE_OFFSET_Y);

    assert(serve_validate_landing(TO_FP(200), TO_FP(110), 0,
        bottom_server) == GAME_FAULT_NONE);
    assert(serve_validate_landing(TO_FP(200), TO_FP(150), 0,
        bottom_server) == GAME_FAULT_WRONG_BOX);
    assert(serve_validate_landing(TO_FP(COURT_KITCHEN_RIGHT_X), TO_FP(110),
        0, bottom_server) == GAME_FAULT_KITCHEN);
    assert(serve_validate_landing(TO_FP(COURT_BASELINE_RIGHT) + 1,
        TO_FP(110), 0, bottom_server) == GAME_FAULT_OUT);

    assert(serve_validate_landing(TO_FP(56), TO_FP(150), 1,
        top_server) == GAME_FAULT_NONE);
    assert(serve_validate_landing(TO_FP(56), TO_FP(110), 1,
        top_server) == GAME_FAULT_WRONG_BOX);
    assert(serve_validate_landing(TO_FP(COURT_KITCHEN_LEFT_X), TO_FP(150),
        1, top_server) == GAME_FAULT_KITCHEN);
}

static void test_net_fault_stops_serve(void)
{
    reset_fixture(0);
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
    fx z;

    reset_fixture(0);
    serve_now(0);
    reset_fixture(0);
    while (g_game_state.phase_timer < GAME_SERVE_READY_FRAMES)
        step();
    intents[0].hit_pressed = 1;
    step();
    intents[0].hit_pressed = 0;
    step();
    z = g_ball.z_fp;
    serve_update(intents, 1);
    assert(g_ball.z_fp == z);
    assert(g_game_state.phase == GAME_PHASE_SERVE_TOSS);
}

int main(void)
{
    test_setup_positions();
    test_early_press_is_buffered_and_toss_runs();
    test_neutral_serves_land_diagonal();
    test_service_box_faults();
    test_net_fault_stops_serve();
    test_pause_freezes_toss();
    puts("serve host checks passed");
    return 0;
}
