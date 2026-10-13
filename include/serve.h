#ifndef SERVE_H
#define SERVE_H

#include "ball.h"
#include "court.h"
#include "game.h"
#include "input.h"
#include "player.h"

enum {
    SERVE_TOSS_FRAMES = 5,
    SERVE_TOSS_RISE_FP = 512,
    SERVE_TARGET_LANE_TOP = 72,
    SERVE_TARGET_LANE_BOTTOM = 168,
    SERVE_TARGET_SHORT_RIGHT = COURT_KITCHEN_RIGHT_X + 12,
    SERVE_TARGET_NEUTRAL_RIGHT = 200,
    SERVE_TARGET_DEEP_RIGHT = COURT_BASELINE_RIGHT - 12,
    SERVE_TARGET_SHORT_LEFT = COURT_KITCHEN_LEFT_X - 12,
    SERVE_TARGET_NEUTRAL_LEFT = 56,
    SERVE_TARGET_DEEP_LEFT = COURT_BASELINE_LEFT + 12,
    SERVE_FLIGHT_FRAMES = 34
};

typedef struct {
    u8 server_index;
    u8 landing_pending;
    s16 aim_x;
    s16 aim_y;
    s32 toss_x_fp;
    s32 toss_y_fp;
} ServeState;

extern ServeState g_serve_state;

void serve_initialize(void);
void serve_update(const PlayerIntent intents[MAX_PLAYERS], u8 paused);
u8 serve_validate_landing(s32 x_fp, s32 y_fp, u8 server_team,
    s32 server_y_fp);
void serve_process_ball_events(u8 events);

#endif
