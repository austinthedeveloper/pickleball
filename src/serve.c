#include "court.h"
#include "serve.h"

ServeState g_serve_state;

static void serve_place_ball_by_server(void)
{
    Player *server = &g_players[g_serve_state.server_index];

    g_ball.active = 1;
    g_ball.x_fp = server->x_fp + TO_FP(12);
    g_ball.y_fp = server->y_fp;
    g_ball.z_fp = 0;
    g_ball.vx_fp = 0;
    g_ball.vy_fp = 0;
    g_ball.vz_fp = 0;
    g_ball.last_hitter = BALL_NO_HITTER;
    g_ball.bounce_count = 0;
    g_ball.rally_state = BALL_RALLY_INACTIVE;
}

static void serve_select_target(const Player *server,
    const PlayerIntent *intent)
{
    if (intent->move_y < 0)
        g_serve_state.aim_y = SERVE_TARGET_LANE_TOP;
    else if (intent->move_y > 0)
        g_serve_state.aim_y = SERVE_TARGET_LANE_BOTTOM;
    else if (server->y_fp < TO_FP(COURT_CENTERLINE_Y))
        g_serve_state.aim_y = SERVE_TARGET_LANE_BOTTOM;
    else
        g_serve_state.aim_y = SERVE_TARGET_LANE_TOP;

    if (server->team == 0) {
        if (intent->move_x > 0)
            g_serve_state.aim_x = SERVE_TARGET_DEEP_RIGHT;
        else if (intent->move_x < 0)
            g_serve_state.aim_x = SERVE_TARGET_SHORT_RIGHT;
        else
            g_serve_state.aim_x = SERVE_TARGET_NEUTRAL_RIGHT;
    } else {
        if (intent->move_x < 0)
            g_serve_state.aim_x = SERVE_TARGET_DEEP_LEFT;
        else if (intent->move_x > 0)
            g_serve_state.aim_x = SERVE_TARGET_SHORT_LEFT;
        else
            g_serve_state.aim_x = SERVE_TARGET_NEUTRAL_LEFT;
    }
}

static void serve_contact(void)
{
    Player *server = &g_players[g_serve_state.server_index];
    s32 gravity_term;

    g_ball.vx_fp = (s16)((TO_FP(g_serve_state.aim_x) - g_ball.x_fp) /
        SERVE_FLIGHT_FRAMES);
    g_ball.vy_fp = (s16)((TO_FP(g_serve_state.aim_y) - g_ball.y_fp) /
        SERVE_FLIGHT_FRAMES);
    gravity_term = (s32)BALL_GRAVITY_FP * SERVE_FLIGHT_FRAMES *
        (SERVE_FLIGHT_FRAMES - 1) / 2;
    g_ball.vz_fp = (s16)((-g_ball.z_fp - gravity_term) /
        SERVE_FLIGHT_FRAMES);
    g_ball.last_hitter = g_serve_state.server_index;
    g_ball.bounce_count = 0;
    g_ball.rally_state = BALL_RALLY_LIVE;
    g_serve_state.landing_pending = 1;
    g_game_state.required_bounce = GAME_REQUIRED_BOUNCE_SERVE;
    server->contact_consumed = 1;
    g_game_state.phase = GAME_PHASE_LIVE;
    g_game_state.phase_timer = 0;
}

void serve_initialize(void)
{
    g_serve_state.server_index = 0;
    g_serve_state.landing_pending = 0;
    g_serve_state.aim_x = SERVE_TARGET_NEUTRAL_RIGHT;
    g_serve_state.aim_y = SERVE_TARGET_LANE_BOTTOM;
    g_serve_state.toss_x_fp = 0;
    g_serve_state.toss_y_fp = 0;
    serve_place_ball_by_server();
}

void serve_update(const PlayerIntent intents[MAX_PLAYERS], u8 paused)
{
    Player *server = &g_players[g_serve_state.server_index];
    const PlayerIntent *intent = &intents[g_serve_state.server_index];

    if (paused)
        return;

    if (g_game_state.phase == GAME_PHASE_SERVE_SETUP) {
        serve_place_ball_by_server();
        if (!intent->hit_pressed)
            return;
        serve_select_target(server, intent);
        g_serve_state.toss_x_fp = g_ball.x_fp;
        g_serve_state.toss_y_fp = g_ball.y_fp;
        g_game_state.phase = GAME_PHASE_SERVE_TOSS;
        g_game_state.phase_timer = 0;
        g_game_state.last_fault = GAME_FAULT_NONE;
    }

    if (g_game_state.phase != GAME_PHASE_SERVE_TOSS)
        return;

    ++g_game_state.phase_timer;
    g_ball.x_fp = g_serve_state.toss_x_fp;
    g_ball.y_fp = g_serve_state.toss_y_fp;
    g_ball.z_fp = (s32)g_game_state.phase_timer * SERVE_TOSS_RISE_FP;
    if (g_game_state.phase_timer >= SERVE_TOSS_FRAMES)
        serve_contact();
}

u8 serve_validate_landing(s32 x_fp, s32 y_fp, u8 server_team,
    s32 server_y_fp)
{
    if (!court_contains_ground_fp(x_fp, y_fp))
        return GAME_FAULT_OUT;

    if (server_team == 0) {
        if (x_fp <= TO_FP(COURT_NET_X))
            return GAME_FAULT_WRONG_BOX;
        if (x_fp <= TO_FP(COURT_KITCHEN_RIGHT_X))
            return GAME_FAULT_KITCHEN;
    } else {
        if (x_fp >= TO_FP(COURT_NET_X))
            return GAME_FAULT_WRONG_BOX;
        if (x_fp >= TO_FP(COURT_KITCHEN_LEFT_X))
            return GAME_FAULT_KITCHEN;
    }

    if (server_y_fp < TO_FP(COURT_CENTERLINE_Y)) {
        if (y_fp < TO_FP(COURT_CENTERLINE_Y))
            return GAME_FAULT_WRONG_BOX;
    } else {
        if (y_fp > TO_FP(COURT_CENTERLINE_Y))
            return GAME_FAULT_WRONG_BOX;
    }

    return GAME_FAULT_NONE;
}

void serve_process_ball_events(u8 events)
{
    Player *server;
    u8 fault;

    if (!g_serve_state.landing_pending)
        return;

    if (events & BALL_EVENT_NET_FAULT) {
        g_game_state.last_fault = GAME_FAULT_NET;
        g_game_state.phase = GAME_PHASE_SERVE_FAULT;
        g_serve_state.landing_pending = 0;
        g_ball.rally_state = BALL_RALLY_INACTIVE;
        return;
    }

    if (!(events & BALL_EVENT_BOUNCE))
        return;

    server = &g_players[g_serve_state.server_index];
    fault = serve_validate_landing(g_ball.x_fp, g_ball.y_fp,
        server->team, server->y_fp);
    g_game_state.last_fault = fault;
    g_serve_state.landing_pending = 0;
    if (fault != GAME_FAULT_NONE) {
        g_game_state.phase = GAME_PHASE_SERVE_FAULT;
        g_ball.rally_state = BALL_RALLY_INACTIVE;
        g_ball.vx_fp = 0;
        g_ball.vy_fp = 0;
        g_ball.vz_fp = 0;
    }
}
