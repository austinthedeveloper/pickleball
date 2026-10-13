#ifndef BALL_H
#define BALL_H

#include "game_types.h"

enum {
    BALL_GRAVITY_FP = -48,
    BALL_BOUNCE_VERTICAL_PERCENT = 55,
    BALL_BOUNCE_PLANAR_PERCENT = 92,
    BALL_START_X = 104,
    BALL_START_Y = 112,
    BALL_START_Z = 24,
    BALL_START_VZ_FP = 640,
    BALL_NO_HITTER = 0xff
};

typedef enum {
    BALL_RALLY_INACTIVE = 0,
    BALL_RALLY_LIVE = 1
} BallRallyState;

typedef enum {
    BALL_EVENT_NONE = 0,
    BALL_EVENT_BOUNCE = 1,
    BALL_EVENT_NET_FAULT = 2,
    BALL_EVENT_OUT = 4
} BallEvent;

typedef struct {
    u8 active;
    s32 x_fp;
    s32 y_fp;
    s32 z_fp;
    s16 vx_fp;
    s16 vy_fp;
    s16 vz_fp;
    u8 last_hitter;
    u8 bounce_count;
    u8 rally_state;
} Ball;

extern Ball g_ball;

void ball_initialize(void);
u8 ball_net_crossing_height(s32 previous_x_fp, s32 previous_z_fp,
    s32 current_x_fp, s32 current_z_fp, s32 *height_fp);
u8 ball_simulate_step(Ball *ball);
u8 ball_update(u8 paused);

#endif
