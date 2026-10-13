#include "ball.h"
#include "court.h"
#include "player.h"

Ball g_ball;

static s16 retain_percent(s16 value, s16 percent)
{
    return (s16)(((s32)value * percent) / 100);
}

void ball_initialize(void)
{
    g_ball.active = 1;
    g_ball.x_fp = TO_FP(BALL_START_X);
    g_ball.y_fp = TO_FP(BALL_START_Y);
    g_ball.z_fp = TO_FP(BALL_START_Z);
    g_ball.vx_fp = 0;
    g_ball.vy_fp = 0;
    g_ball.vz_fp = BALL_START_VZ_FP;
    g_ball.last_hitter = BALL_NO_HITTER;
    g_ball.bounce_count = 0;
    g_ball.rally_state = BALL_RALLY_LIVE;
}

u8 ball_net_crossing_height(s32 previous_x_fp, s32 previous_z_fp,
    s32 current_x_fp, s32 current_z_fp, s32 *height_fp)
{
    s32 net_x_fp = TO_FP(COURT_NET_X);
    s32 x_distance;
    s32 distance_to_net;
    s32 z_distance;

    if (previous_x_fp < net_x_fp) {
        if (current_x_fp < net_x_fp)
            return 0;
    } else if (previous_x_fp > net_x_fp) {
        if (current_x_fp > net_x_fp)
            return 0;
    } else {
        return 0;
    }

    x_distance = current_x_fp - previous_x_fp;
    distance_to_net = net_x_fp - previous_x_fp;
    z_distance = current_z_fp - previous_z_fp;
    *height_fp = previous_z_fp +
        (s32)((z_distance * distance_to_net) / x_distance);
    return 1;
}

u8 ball_simulate_step(Ball *ball)
{
    u8 events = BALL_EVENT_NONE;
    s32 previous_x;
    s32 previous_z;
    s32 crossing_height;
    s16 impact_vz;

    if (!ball->active)
        return BALL_EVENT_NONE;
    if (ball->rally_state != BALL_RALLY_LIVE)
        return BALL_EVENT_NONE;

    if (ball->z_fp == 0) {
        if (ball->vz_fp <= 0) {
            ball->vx_fp = 0;
            ball->vy_fp = 0;
            ball->vz_fp = 0;
            return BALL_EVENT_NONE;
        }
    }

    previous_x = ball->x_fp;
    previous_z = ball->z_fp;
    ball->x_fp += ball->vx_fp;
    ball->y_fp += ball->vy_fp;
    ball->z_fp += ball->vz_fp;
    ball->vz_fp += BALL_GRAVITY_FP;

    if (ball_net_crossing_height(previous_x, previous_z,
            ball->x_fp, ball->z_fp, &crossing_height)) {
        if (crossing_height < TO_FP(COURT_NET_HEIGHT))
            events |= BALL_EVENT_NET_FAULT;
    }

    if (previous_z > 0) {
        if (ball->z_fp <= 0) {
            impact_vz = ball->vz_fp - BALL_GRAVITY_FP;
            ball->z_fp = 0;
            ball->vx_fp = retain_percent(ball->vx_fp,
                BALL_BOUNCE_PLANAR_PERCENT);
            ball->vy_fp = retain_percent(ball->vy_fp,
                BALL_BOUNCE_PLANAR_PERCENT);
            ball->vz_fp = -retain_percent(impact_vz,
                BALL_BOUNCE_VERTICAL_PERCENT);
            ++ball->bounce_count;
            events |= BALL_EVENT_BOUNCE;
            if (!court_contains_ground_fp(ball->x_fp, ball->y_fp))
                events |= BALL_EVENT_OUT;
            if (ball->vz_fp <= BALL_MIN_BOUNCE_VZ_FP) {
                ball->vx_fp = 0;
                ball->vy_fp = 0;
                ball->vz_fp = 0;
            }
        }
    }

    return events;
}

u8 ball_update(u8 paused)
{
    if (paused)
        return 0;
    return ball_simulate_step(&g_ball);
}
