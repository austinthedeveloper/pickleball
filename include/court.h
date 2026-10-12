#ifndef COURT_H
#define COURT_H

#include "game_types.h"

enum {
    COURT_BASELINE_LEFT = 16,
    COURT_BASELINE_RIGHT = 232,
    COURT_SIDELINE_TOP = 40,
    COURT_SIDELINE_BOTTOM = 200,
    COURT_NET_X = 128,
    COURT_KITCHEN_LEFT_X = 88,
    COURT_KITCHEN_RIGHT_X = 168,
    COURT_CENTERLINE_Y = 120,

    PLAYER_SPAWN_X = 56,
    PLAYER_SPAWN_Y = 112,
    PLAYER_CLAMP_MIN_X = 20,
    PLAYER_CLAMP_MAX_X = 92,
    PLAYER_CLAMP_MIN_Y = 44,
    PLAYER_CLAMP_MAX_Y = 168
};

s32 court_clamp_x_fp(s32 x_fp);
s32 court_clamp_y_fp(s32 y_fp);

#endif

