#include "court.h"
#include "serve.h"

ServeState g_serve_state;

static u8 team_player_index(u8 team)
{
    return team == 0 ? 0 : 2;
}

static void serve_hold_ball(void)
{
    Player *server = &g_players[g_serve_state.server_index];
    s16 offset = server->team ? -SERVE_HAND_OFFSET_X : SERVE_HAND_OFFSET_X;

    g_ball.active = 1;
    g_ball.x_fp = server->x_fp + TO_FP(offset);
    g_ball.y_fp = server->y_fp + TO_FP(1);
    g_ball.z_fp = TO_FP(SERVE_HAND_Z);
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
    s16 box_y;

    /* Diagonal box: a server on the bottom half serves to the top box. */
    if (g_serve_state.server_y_fp >= TO_FP(COURT_CENTERLINE_Y))
        box_y = (COURT_SIDELINE_TOP + COURT_CENTERLINE_Y) / 2;
    else
        box_y = (COURT_CENTERLINE_Y + COURT_SIDELINE_BOTTOM) / 2;
    if (intent->aim_y < 0)
        box_y -= SERVE_TARGET_LANE_SHIFT;
    else if (intent->aim_y > 0)
        box_y += SERVE_TARGET_LANE_SHIFT;
    g_serve_state.aim_y = box_y;

    if (server->team == 0) {
        if (intent->aim_x > 0)
            g_serve_state.aim_x = SERVE_TARGET_DEEP_RIGHT;
        else if (intent->aim_x < 0)
            g_serve_state.aim_x = SERVE_TARGET_SHORT_RIGHT;
        else
            g_serve_state.aim_x = SERVE_TARGET_NEUTRAL_RIGHT;
    } else {
        if (intent->aim_x < 0)
            g_serve_state.aim_x = SERVE_TARGET_DEEP_LEFT;
        else if (intent->aim_x > 0)
            g_serve_state.aim_x = SERVE_TARGET_SHORT_LEFT;
        else
            g_serve_state.aim_x = SERVE_TARGET_NEUTRAL_LEFT;
    }
}

static void serve_contact(const PlayerIntent *intent)
{
    Player *server = &g_players[g_serve_state.server_index];

    serve_select_target(server, intent);
    ball_launch(&g_ball, g_serve_state.aim_x, g_serve_state.aim_y,
        SERVE_FLIGHT_FRAMES);
    g_ball.last_hitter = g_serve_state.server_index;
    g_ball.bounce_count = 0;
    g_ball.rally_state = BALL_RALLY_LIVE;
    ++g_game_state.shot_serial;
    g_serve_state.landing_pending = 1;
    g_game_state.required_bounce = GAME_REQUIRED_BOUNCE_SERVE;
    server->contact_consumed = 1;
    server->pose = PLAYER_POSE_NONE;
    server->frozen = 0;
    game_set_phase(GAME_PHASE_LIVE);
}

void serve_initialize(void)
{
    g_serve_state.server_index = 0;
    g_serve_state.receiver_index = 2;
    g_serve_state.landing_pending = 0;
    g_serve_state.requested = 0;
    g_serve_state.aim_x = SERVE_TARGET_NEUTRAL_RIGHT;
    g_serve_state.aim_y = COURT_SIDELINE_TOP + 20;
    g_serve_state.server_y_fp = TO_FP(COURT_CENTERLINE_Y);
}

void serve_setup(u8 serving_team)
{
    Player *server;
    Player *receiver;
    s16 server_y;
    s16 receiver_y;
    u8 even = (g_game_state.team_score[serving_team] & 1) == 0;

    g_serve_state.server_index = team_player_index(serving_team);
    g_serve_state.receiver_index = team_player_index(serving_team ? 0 : 1);
    g_serve_state.landing_pending = 0;
    g_serve_state.requested = 0;
    server = &g_players[g_serve_state.server_index];
    receiver = &g_players[g_serve_state.receiver_index];

    /* Serve from the right-hand court on an even score. The left team faces
     * +x so its right hand is +y; the right team's right hand is -y. */
    if ((serving_team == 0) == even)
        server_y = COURT_CENTERLINE_Y + SERVE_STANCE_OFFSET_Y;
    else
        server_y = COURT_CENTERLINE_Y - SERVE_STANCE_OFFSET_Y;
    receiver_y = server_y >= COURT_CENTERLINE_Y ?
        COURT_CENTERLINE_Y - SERVE_STANCE_OFFSET_Y :
        COURT_CENTERLINE_Y + SERVE_STANCE_OFFSET_Y;

    if (serving_team == 0) {
        player_place(server, COURT_BASELINE_LEFT - SERVE_BASELINE_GAP,
            server_y);
        player_place(receiver, COURT_BASELINE_RIGHT - SERVE_RECEIVER_GAP,
            receiver_y);
    } else {
        player_place(server, COURT_BASELINE_RIGHT + SERVE_BASELINE_GAP,
            server_y);
        player_place(receiver, COURT_BASELINE_LEFT + SERVE_RECEIVER_GAP,
            receiver_y);
    }
    server->frozen = 1;
    receiver->frozen = 0;
    server->pose = PLAYER_POSE_NONE;
    receiver->pose = PLAYER_POSE_NONE;
    g_serve_state.server_y_fp = server->y_fp;
    g_game_state.serving_team = serving_team;
    g_game_state.required_bounce = GAME_REQUIRED_BOUNCE_NONE;
    serve_hold_ball();
}

void serve_update(const PlayerIntent intents[MAX_PLAYERS], u8 paused)
{
    Player *server = &g_players[g_serve_state.server_index];
    const PlayerIntent *intent = &intents[server->logical_input_slot];

    if (paused)
        return;

    if (g_game_state.phase == GAME_PHASE_SERVE_SETUP) {
        serve_hold_ball();
        /* Buffer an early press so it is not lost to the ready delay. */
        if (intent->hit_pressed)
            g_serve_state.requested = 1;
        if (g_game_state.phase_timer < GAME_SERVE_READY_FRAMES)
            return;
        if (!g_serve_state.requested)
            return;
        g_serve_state.requested = 0;
        g_ball.vz_fp = SERVE_TOSS_VZ_FP;
        server->pose = PLAYER_POSE_TOSS;
        game_set_phase(GAME_PHASE_SERVE_TOSS);
        g_game_state.last_fault = GAME_FAULT_NONE;
        return;
    }

    if (g_game_state.phase != GAME_PHASE_SERVE_TOSS)
        return;

    /* The toss is presentation: the ball rises and falls in the hand lane. */
    g_ball.z_fp += g_ball.vz_fp;
    g_ball.vz_fp += BALL_GRAVITY_FP;
    if (g_game_state.phase_timer == SERVE_TOSS_POSE_FRAMES) {
        server->pose = PLAYER_POSE_NONE;
        server->state = PLAYER_STATE_WINDUP;
        server->state_frames_remaining = PLAYER_WINDUP_FRAMES;
        server->contact_consumed = 1;
    }
    if (g_game_state.phase_timer >= SERVE_TOSS_FRAMES)
        serve_contact(intent);
}

u8 serve_validate_landing(fx x_fp, fx y_fp, u8 server_team,
    fx server_y_fp)
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
        server->team, g_serve_state.server_y_fp);
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
