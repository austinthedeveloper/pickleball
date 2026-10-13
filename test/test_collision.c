#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "collision.h"

static Player players[MAX_PLAYERS];
static Ball ball;
static PlayerIntent intents[MAX_PLAYERS];

static s32 absolute_fp(s32 value)
{
    return value < 0 ? -value : value;
}

static void reset_fixture(void)
{
    memset(players, 0, sizeof(players));
    memset(&ball, 0, sizeof(ball));
    memset(intents, 0, sizeof(intents));
    players[0].active = 1;
    players[0].x_fp = TO_FP(64);
    players[0].y_fp = TO_FP(112);
    ball.active = 1;
    ball.rally_state = BALL_RALLY_LIVE;
    ball.x_fp = players[0].x_fp;
    ball.y_fp = players[0].y_fp;
    ball.z_fp = TO_FP(10);
    ball.last_hitter = BALL_NO_HITTER;
}

static void test_reach_boundaries_are_inclusive(void)
{
    reset_fixture();
    ball.x_fp += TO_FP(HIT_REACH_X);
    ball.y_fp -= TO_FP(HIT_REACH_Y);
    ball.z_fp = TO_FP(HIT_REACH_Z);
    assert(collision_ball_in_player_reach(&players[0], &ball) == 1);

    ball.x_fp += 1;
    assert(collision_ball_in_player_reach(&players[0], &ball) == 0);
    ball.x_fp -= 1;
    ball.y_fp -= 1;
    assert(collision_ball_in_player_reach(&players[0], &ball) == 0);
    ball.y_fp += 1;
    ball.z_fp += 1;
    assert(collision_ball_in_player_reach(&players[0], &ball) == 0);
}

static void test_windup_cannot_contact(void)
{
    reset_fixture();
    players[0].state = PLAYER_STATE_WINDUP;
    players[0].state_frames_remaining = PLAYER_WINDUP_FRAMES;
    assert(collision_resolve_player_hits(&ball, players, intents,
        MAX_PLAYERS) == 0);
    assert(players[0].contact_consumed == 0);
    assert(ball.last_hitter == BALL_NO_HITTER);
}

static void test_active_overlap_contacts_once(void)
{
    u8 frame;
    u8 contacts = 0;

    reset_fixture();
    players[0].state = PLAYER_STATE_ACTIVE;
    players[0].state_frames_remaining = PLAYER_ACTIVE_FRAMES;
    for (frame = 0; frame < PLAYER_ACTIVE_FRAMES; ++frame)
        contacts += collision_resolve_player_hits(&ball, players, intents,
            MAX_PLAYERS);

    assert(contacts == 1);
    assert(players[0].contact_consumed == 1);
    assert(ball.last_hitter == 0);
    assert(ball.bounce_count == 0);
    assert(ball.vx_fp == (TO_FP(HIT_TARGET_NEUTRAL_RIGHT) - ball.x_fp) /
        HIT_FLIGHT_FRAMES_NEUTRAL);
    assert(ball.vy_fp == (TO_FP(HIT_TARGET_LANE_CENTER) - ball.y_fp) /
        HIT_FLIGHT_FRAMES_NEUTRAL);
    assert(ball.vz_fp == 716);
}

static void test_team_direction_and_live_ball_requirement(void)
{
    reset_fixture();
    players[0].team = 1;
    players[0].state = PLAYER_STATE_ACTIVE;
    assert(collision_resolve_player_hits(&ball, players, intents,
        MAX_PLAYERS) == 1);
    assert(ball.vx_fp < 0);

    reset_fixture();
    players[0].state = PLAYER_STATE_ACTIVE;
    ball.rally_state = BALL_RALLY_INACTIVE;
    assert(collision_resolve_player_hits(&ball, players, intents,
        MAX_PLAYERS) == 0);
}

static void assert_lands_near(s16 expected_x, s16 expected_y)
{
    u8 events = BALL_EVENT_NONE;
    u8 frame;

    for (frame = 0; frame < 60; ++frame) {
        events = ball_simulate_step(&ball);
        if (events & BALL_EVENT_BOUNCE)
            break;
    }
    assert(events & BALL_EVENT_BOUNCE);
    assert(absolute_fp(ball.x_fp - TO_FP(expected_x)) < 64);
    assert(absolute_fp(ball.y_fp - TO_FP(expected_y)) < 64);
    assert((events & BALL_EVENT_OUT) == 0);
}

static void test_directional_target_lanes(void)
{
    reset_fixture();
    players[0].state = PLAYER_STATE_ACTIVE;
    assert(collision_resolve_player_hits(&ball, players, intents,
        MAX_PLAYERS) == 1);
    assert_lands_near(HIT_TARGET_NEUTRAL_RIGHT, HIT_TARGET_LANE_CENTER);

    reset_fixture();
    players[0].state = PLAYER_STATE_ACTIVE;
    intents[0].move_y = -1;
    collision_resolve_player_hits(&ball, players, intents, MAX_PLAYERS);
    assert_lands_near(HIT_TARGET_NEUTRAL_RIGHT, HIT_TARGET_LANE_TOP);

    reset_fixture();
    players[0].state = PLAYER_STATE_ACTIVE;
    intents[0].move_y = 1;
    collision_resolve_player_hits(&ball, players, intents, MAX_PLAYERS);
    assert_lands_near(HIT_TARGET_NEUTRAL_RIGHT, HIT_TARGET_LANE_BOTTOM);

    reset_fixture();
    players[0].state = PLAYER_STATE_ACTIVE;
    intents[0].move_x = 1;
    collision_resolve_player_hits(&ball, players, intents, MAX_PLAYERS);
    assert_lands_near(HIT_TARGET_DEEP_RIGHT, HIT_TARGET_LANE_CENTER);

    reset_fixture();
    players[0].state = PLAYER_STATE_ACTIVE;
    intents[0].move_x = -1;
    collision_resolve_player_hits(&ball, players, intents, MAX_PLAYERS);
    assert_lands_near(HIT_TARGET_SHORT_RIGHT, HIT_TARGET_LANE_CENTER);
}

static void test_new_swing_restores_contact(void)
{
    PlayerIntent intents[MAX_PLAYERS];

    memset(intents, 0, sizeof(intents));
    players_initialize();
    g_players[0].contact_consumed = 1;
    intents[0].hit_pressed = 1;
    players_update(intents, 0);
    assert(g_players[0].state == PLAYER_STATE_WINDUP);
    assert(g_players[0].contact_consumed == 0);
}

int main(void)
{
    test_reach_boundaries_are_inclusive();
    test_windup_cannot_contact();
    test_active_overlap_contacts_once();
    test_team_direction_and_live_ball_requirement();
    test_directional_target_lanes();
    test_new_swing_restores_contact();
    puts("collision host checks passed");
    return 0;
}
