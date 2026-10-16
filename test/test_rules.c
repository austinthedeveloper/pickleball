#include <assert.h>
#include <stdio.h>
#include "player.h"
#include "rules.h"
#include "court.h"
#include "serve.h"

static void reset_live_fixture(void)
{
    players_initialize();
    ball_initialize();
    game_initialize();
    serve_initialize();
    g_game_state.phase = GAME_PHASE_LIVE;
    g_ball.rally_state = BALL_RALLY_LIVE;
    g_ball.last_hitter = 0;
    g_ball.vx_fp = 31;
    g_ball.vy_fp = 6;
    g_ball.vz_fp = 12;
    g_ball.x_fp = TO_FP(200);
    g_ball.bounce_count = 1;
}

static void assert_stopped_with_winner(u8 winner, u8 fault)
{
    assert(g_game_state.phase == GAME_PHASE_POINT_RESULT);
    assert(g_game_state.point_winner == winner);
    assert(g_game_state.last_fault == fault);
    assert(g_game_state.team_score[winner] == 1);
    assert(g_game_state.team_score[winner ^ 1] == 0);
    assert(g_ball.rally_state == BALL_RALLY_INACTIVE);
    assert(g_ball.vx_fp == 0);
    assert(g_ball.vy_fp == 0);
    assert(g_ball.vz_fp == 0);
}

static void test_net_penalizes_last_hitter(void)
{
    reset_live_fixture();
    rules_process_ball_events(BALL_EVENT_NET_FAULT);
    assert_stopped_with_winner(1, GAME_FAULT_NET);
}

static void test_out_penalizes_last_hitter(void)
{
    reset_live_fixture();
    rules_process_ball_events(BALL_EVENT_BOUNCE | BALL_EVENT_OUT);
    assert_stopped_with_winner(1, GAME_FAULT_OUT);
}

static void test_second_bounce_penalizes_court_team(void)
{
    reset_live_fixture();
    g_ball.x_fp = TO_FP(200);
    g_ball.bounce_count = 1;
    rules_process_ball_events(BALL_EVENT_BOUNCE);
    assert(g_game_state.phase == GAME_PHASE_LIVE);
    assert(g_game_state.team_score[0] == 0);

    g_ball.bounce_count = 2;
    rules_process_ball_events(BALL_EVENT_BOUNCE);
    assert_stopped_with_winner(0, GAME_FAULT_SECOND_BOUNCE);

    /* Landing on the hitter's own side is a lost rally for the hitter. */
    reset_live_fixture();
    g_ball.x_fp = TO_FP(56);
    rules_process_ball_events(BALL_EVENT_BOUNCE);
    assert_stopped_with_winner(1, GAME_FAULT_OUT);

    /* A legal first bounce that rolls out is the receiver's second bounce. */
    reset_live_fixture();
    rules_process_ball_events(BALL_EVENT_BOUNCE);
    g_ball.x_fp = TO_FP(COURT_BASELINE_RIGHT + 10);
    g_ball.bounce_count = 2;
    rules_process_ball_events(BALL_EVENT_BOUNCE | BALL_EVENT_OUT);
    assert_stopped_with_winner(0, GAME_FAULT_SECOND_BOUNCE);
}

static void test_dead_ball_ends_rally(void)
{
    reset_live_fixture();
    g_ball.z_fp = 0;
    g_ball.vx_fp = 0;
    g_ball.vy_fp = 0;
    g_ball.vz_fp = 0;
    rules_process_ball_events(BALL_EVENT_NONE);
    assert_stopped_with_winner(0, GAME_FAULT_SECOND_BOUNCE);
}

static void test_serve_fault_resolves_point(void)
{
    reset_live_fixture();
    g_game_state.phase = GAME_PHASE_SERVE_FAULT;
    g_game_state.last_fault = GAME_FAULT_KITCHEN;
    rules_process_ball_events(BALL_EVENT_NONE);
    assert_stopped_with_winner(1, GAME_FAULT_KITCHEN);
}

static void test_resolution_is_idempotent(void)
{
    reset_live_fixture();
    rules_process_ball_events(BALL_EVENT_BOUNCE | BALL_EVENT_OUT);
    rules_process_ball_events(BALL_EVENT_BOUNCE | BALL_EVENT_OUT);
    assert(g_game_state.team_score[1] == 1);
}

static void test_legal_required_bounce_sequence(void)
{
    reset_live_fixture();
    g_game_state.required_bounce = GAME_REQUIRED_BOUNCE_SERVE;

    g_ball.x_fp = TO_FP(200);
    g_ball.bounce_count = 1;
    rules_process_ball_events(BALL_EVENT_BOUNCE);
    assert(g_game_state.required_bounce == GAME_REQUIRED_BOUNCE_RETURN);
    assert(g_game_state.phase == GAME_PHASE_LIVE);

    g_ball.last_hitter = 2;
    rules_process_player_hit(2);
    assert(g_game_state.required_bounce == GAME_REQUIRED_BOUNCE_RETURN);
    assert(g_game_state.phase == GAME_PHASE_LIVE);

    g_ball.x_fp = TO_FP(56);
    g_ball.bounce_count = 1;
    rules_process_ball_events(BALL_EVENT_BOUNCE);
    assert(g_game_state.required_bounce == GAME_REQUIRED_BOUNCE_NONE);
    assert(g_game_state.phase == GAME_PHASE_LIVE);

    g_ball.last_hitter = 0;
    rules_process_player_hit(0);
    assert(g_game_state.phase == GAME_PHASE_LIVE);
}

static void test_serve_volley_before_bounce_loses_point(void)
{
    reset_live_fixture();
    g_game_state.required_bounce = GAME_REQUIRED_BOUNCE_SERVE;
    rules_process_player_hit(2);
    assert_stopped_with_winner(0, GAME_FAULT_EARLY_VOLLEY);
}

static void test_return_volley_before_bounce_loses_point(void)
{
    reset_live_fixture();
    g_game_state.required_bounce = GAME_REQUIRED_BOUNCE_RETURN;
    g_ball.last_hitter = 2;
    rules_process_player_hit(0);
    assert_stopped_with_winner(1, GAME_FAULT_EARLY_VOLLEY);
}

/* The kitchen fault (ADR-052): a volley with the feet in your own kitchen,
 * or on its line, loses the point; behind the line, or past a sideline,
 * it is fine. Mirrored for the right team. */
static void kitchen_volley_case(u8 slot, s16 x, s16 y, u8 faults)
{
    reset_live_fixture();
    g_game_state.required_bounce = GAME_REQUIRED_BOUNCE_NONE;
    g_players[slot].active = 1;
    g_players[slot].x_fp = TO_FP(x);
    g_players[slot].y_fp = TO_FP(y);
    rules_process_volley(slot);
    if (faults)
        assert_stopped_with_winner(g_players[slot].team ^ 1,
            GAME_FAULT_KITCHEN);
    else
        assert(g_game_state.phase == GAME_PHASE_LIVE);
}

static void test_kitchen_volley_loses_point(void)
{
    kitchen_volley_case(0, COURT_NET_X - 20, 130, 1);
    kitchen_volley_case(0, COURT_KITCHEN_LEFT_X, 130, 1);       /* the line */
    kitchen_volley_case(0, COURT_KITCHEN_LEFT_X - 1, 130, 0);
    kitchen_volley_case(0, COURT_NET_X - 20, COURT_SIDELINE_TOP - 2, 0);
    kitchen_volley_case(0, COURT_NET_X - 20, COURT_SIDELINE_BOTTOM, 1);
    kitchen_volley_case(2, COURT_NET_X + 20, 130, 1);
    kitchen_volley_case(2, COURT_KITCHEN_RIGHT_X, 130, 1);
    kitchen_volley_case(2, COURT_KITCHEN_RIGHT_X + 1, 130, 0);
    /* Over the point already: nothing more. */
    reset_live_fixture();
    g_game_state.phase = GAME_PHASE_POINT_RESULT;
    g_players[0].x_fp = TO_FP(COURT_NET_X - 20);
    g_players[0].y_fp = TO_FP(130);
    rules_process_volley(0);
    assert(g_game_state.team_score[1] == 0);
}

int main(void)
{
    test_net_penalizes_last_hitter();
    test_out_penalizes_last_hitter();
    test_second_bounce_penalizes_court_team();
    test_dead_ball_ends_rally();
    test_serve_fault_resolves_point();
    test_resolution_is_idempotent();
    test_legal_required_bounce_sequence();
    test_serve_volley_before_bounce_loses_point();
    test_return_volley_before_bounce_loses_point();
    test_kitchen_volley_loses_point();
    puts("rules host checks passed");
    return 0;
}
