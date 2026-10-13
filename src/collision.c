#include "collision.h"

static fx absolute_fp(fx value)
{
    return value < 0 ? -value : value;
}

static void collision_apply_targeted_shot(Ball *ball, const Player *player,
    const PlayerIntent *intent)
{
    s16 target_x;
    s16 target_y = HIT_TARGET_LANE_CENTER;
    s16 flight_frames;
    s16 distance;
    u8 aiming_deep = 0;
    u8 aiming_short = 0;

    if (intent->aim_y < 0)
        target_y = HIT_TARGET_LANE_TOP;
    else if (intent->aim_y > 0)
        target_y = HIT_TARGET_LANE_BOTTOM;

    if (player->team == 0) {
        if (intent->aim_x > 0)
            aiming_deep = 1;
        else if (intent->aim_x < 0)
            aiming_short = 1;
    } else {
        if (intent->aim_x < 0)
            aiming_deep = 1;
        else if (intent->aim_x > 0)
            aiming_short = 1;
    }

    if (player->team == 0) {
        if (aiming_deep)
            target_x = HIT_TARGET_DEEP_RIGHT;
        else if (aiming_short)
            target_x = HIT_TARGET_SHORT_RIGHT;
        else
            target_x = HIT_TARGET_NEUTRAL_RIGHT;
    } else {
        if (aiming_deep)
            target_x = HIT_TARGET_DEEP_LEFT;
        else if (aiming_short)
            target_x = HIT_TARGET_SHORT_LEFT;
        else
            target_x = HIT_TARGET_NEUTRAL_LEFT;
    }

    if (!collision_ball_in_sweet_spot(player, ball)) {
        /* Stretched: the shot sprays toward its sideline and long. */
        if (target_y < HIT_TARGET_LANE_CENTER ||
            (target_y == HIT_TARGET_LANE_CENTER && ball->y_fp < player->y_fp))
            target_y -= HIT_STRETCH_WIDE;
        else
            target_y += HIT_STRETCH_WIDE;
        target_x += player->team ? -HIT_STRETCH_LONG : HIT_STRETCH_LONG;
    }

    distance = absolute_fp(TO_FP(target_x) - ball->x_fp);
    if (aiming_deep)
        flight_frames = distance / HIT_PACE_DEEP_FP;
    else if (aiming_short)
        flight_frames = distance / HIT_PACE_SHORT_FP;
    else
        flight_frames = distance / HIT_PACE_NEUTRAL_FP;
    if (flight_frames < HIT_FLIGHT_MIN_FRAMES)
        flight_frames = HIT_FLIGHT_MIN_FRAMES;
    else if (flight_frames > HIT_FLIGHT_MAX_FRAMES)
        flight_frames = HIT_FLIGHT_MAX_FRAMES;

    ball_launch(ball, target_x, target_y, (u8)flight_frames);
}

u8 collision_ball_in_player_reach(const Player *player, const Ball *ball)
{
    if (!player->active)
        return 0;
    if (!ball->active)
        return 0;
    if (ball->rally_state != BALL_RALLY_LIVE)
        return 0;
    if (absolute_fp(ball->x_fp - player->x_fp) > TO_FP(HIT_REACH_X))
        return 0;
    if (absolute_fp(ball->y_fp - player->y_fp) > TO_FP(HIT_REACH_Y))
        return 0;
    if (ball->z_fp < 0)
        return 0;
    if (ball->z_fp > TO_FP(HIT_REACH_Z))
        return 0;
    return 1;
}

u8 collision_ball_in_sweet_spot(const Player *player, const Ball *ball)
{
    if (absolute_fp(ball->x_fp - player->x_fp) > TO_FP(HIT_SWEET_X))
        return 0;
    if (absolute_fp(ball->y_fp - player->y_fp) > TO_FP(HIT_SWEET_Y))
        return 0;
    return 1;
}

u8 collision_resolve_player_hits(Ball *ball, Player players[MAX_PLAYERS],
    const PlayerIntent intents[MAX_PLAYERS], u8 player_count)
{
    u8 i;
    Player *player;

    for (i = 0; i < player_count; ++i) {
        player = &players[i];
        if (player->state != PLAYER_STATE_ACTIVE)
            continue;
        if (player->contact_consumed)
            continue;
        if (!collision_ball_in_player_reach(player, ball))
            continue;

        player->contact_consumed = 1;
        ball->last_hitter = i;
        ball->bounce_count = 0;
        collision_apply_targeted_shot(ball, player, &intents[i]);
        return i + 1;
    }

    return 0;
}
