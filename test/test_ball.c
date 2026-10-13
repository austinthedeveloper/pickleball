#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ball.h"
#include "court.h"
#include "player.h"

static void test_initial_state(void)
{
    ball_initialize();
    assert(g_ball.active == 1);
    assert(g_ball.x_fp == TO_FP(BALL_START_X));
    assert(g_ball.y_fp == TO_FP(BALL_START_Y));
    assert(g_ball.z_fp == TO_FP(BALL_START_Z));
    assert(g_ball.vz_fp == BALL_START_VZ_FP);
    assert(g_ball.last_hitter == BALL_NO_HITTER);
    assert(g_ball.bounce_count == 0);
    assert(g_ball.rally_state == BALL_RALLY_LIVE);
}

static void test_deterministic_flight(void)
{
    Ball first;
    Ball second;
    u16 frame;

    ball_initialize();
    first = g_ball;
    second = g_ball;
    first.vx_fp = 700;
    first.vy_fp = -311;
    second.vx_fp = 700;
    second.vy_fp = -311;

    for (frame = 0; frame < 240; ++frame) {
        assert(ball_simulate_step(&first) == ball_simulate_step(&second));
        assert(memcmp(&first, &second, sizeof(first)) == 0);
    }
}

static void test_position_and_gravity_order(void)
{
    Ball ball;

    memset(&ball, 0, sizeof(ball));
    ball.active = 1;
    ball.rally_state = BALL_RALLY_LIVE;
    ball.x_fp = TO_FP(10);
    ball.y_fp = TO_FP(20);
    ball.z_fp = TO_FP(3);
    ball.vx_fp = 64;
    ball.vy_fp = -32;
    ball.vz_fp = 128;

    assert(ball_simulate_step(&ball) == 0);
    assert(ball.x_fp == TO_FP(10) + 64);
    assert(ball.y_fp == TO_FP(20) - 32);
    assert(ball.z_fp == TO_FP(3) + 128);
    assert(ball.vz_fp == 128 + BALL_GRAVITY_FP);
}

static void test_single_bounce_event(void)
{
    Ball ball;

    memset(&ball, 0, sizeof(ball));
    ball.active = 1;
    ball.rally_state = BALL_RALLY_LIVE;
    ball.x_fp = TO_FP(COURT_NET_X - 16);
    ball.y_fp = TO_FP(COURT_CENTERLINE_Y);
    ball.z_fp = 32;
    ball.vx_fp = 100;
    ball.vy_fp = -100;
    ball.vz_fp = -64;

    assert(ball_simulate_step(&ball) == BALL_EVENT_BOUNCE);
    assert(ball.z_fp == 0);
    assert(ball.vx_fp == 92);
    assert(ball.vy_fp == -92);
    assert(ball.vz_fp == 61);
    assert(ball.bounce_count == 1);

    assert(ball_simulate_step(&ball) == 0);
    assert(ball.bounce_count == 1);
}

static void test_swept_net_height_boundaries(void)
{
    s32 height;
    s32 net = TO_FP(COURT_NET_X);
    s32 threshold = TO_FP(COURT_NET_HEIGHT);

    assert(ball_net_crossing_height(net - 256, threshold + 1,
        net + 256, threshold + 1, &height) == 1);
    assert(height == threshold + 1);
    assert(height >= threshold);

    assert(ball_net_crossing_height(net - 256, threshold - 1,
        net + 256, threshold - 1, &height) == 1);
    assert(height == threshold - 1);
    assert(height < threshold);

    assert(ball_net_crossing_height(net + 256, threshold + 1,
        net - 256, threshold + 1, &height) == 1);
    assert(height == threshold + 1);

    assert(ball_net_crossing_height(net - 512, threshold,
        net - 1, threshold, &height) == 0);
}

static void test_net_fault_event(void)
{
    Ball ball;

    memset(&ball, 0, sizeof(ball));
    ball.active = 1;
    ball.rally_state = BALL_RALLY_LIVE;
    ball.x_fp = TO_FP(COURT_NET_X) - 1;
    ball.z_fp = TO_FP(COURT_NET_HEIGHT) - 1;
    ball.vx_fp = 2;
    assert(ball_simulate_step(&ball) & BALL_EVENT_NET_FAULT);

    ball.x_fp = TO_FP(COURT_NET_X) - 1;
    ball.z_fp = TO_FP(COURT_NET_HEIGHT) + 1;
    ball.vx_fp = 2;
    ball.vz_fp = 0;
    assert((ball_simulate_step(&ball) & BALL_EVENT_NET_FAULT) == 0);
}

static u8 ground_event_at(s32 x_fp, s32 y_fp)
{
    Ball ball;

    memset(&ball, 0, sizeof(ball));
    ball.active = 1;
    ball.rally_state = BALL_RALLY_LIVE;
    ball.x_fp = x_fp;
    ball.y_fp = y_fp;
    ball.z_fp = 1;
    ball.vz_fp = -2;
    return ball_simulate_step(&ball);
}

static void test_inclusive_court_boundaries(void)
{
    s32 left = TO_FP(COURT_BASELINE_LEFT);
    s32 right = TO_FP(COURT_BASELINE_RIGHT);
    s32 top = TO_FP(COURT_SIDELINE_TOP);
    s32 bottom = TO_FP(COURT_SIDELINE_BOTTOM);
    u8 events;

    events = ground_event_at(left, top);
    assert(events & BALL_EVENT_BOUNCE);
    assert((events & BALL_EVENT_OUT) == 0);
    assert((ground_event_at(right, top) & BALL_EVENT_OUT) == 0);
    assert((ground_event_at(left, bottom) & BALL_EVENT_OUT) == 0);
    assert((ground_event_at(right, bottom) & BALL_EVENT_OUT) == 0);

    assert(ground_event_at(left - 1, top) & BALL_EVENT_OUT);
    assert(ground_event_at(right + 1, top) & BALL_EVENT_OUT);
    assert(ground_event_at(left, top - 1) & BALL_EVENT_OUT);
    assert(ground_event_at(left, bottom + 1) & BALL_EVENT_OUT);
}

static void test_pause_and_inactive(void)
{
    Ball before;

    ball_initialize();
    before = g_ball;
    assert(ball_update(1) == 0);
    assert(memcmp(&before, &g_ball, sizeof(before)) == 0);

    g_ball.active = 0;
    before = g_ball;
    assert(ball_update(0) == 0);
    assert(memcmp(&before, &g_ball, sizeof(before)) == 0);
}

int main(void)
{
    test_initial_state();
    test_deterministic_flight();
    test_position_and_gravity_order();
    test_single_bounce_event();
    test_swept_net_height_boundaries();
    test_net_fault_event();
    test_inclusive_court_boundaries();
    test_pause_and_inactive();
    puts("ball host checks passed");
    return 0;
}
