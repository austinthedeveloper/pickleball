#include "collision.h"

static fx absolute_fp(fx value)
{
    return value < 0 ? -value : value;
}

/*
 * Target and flight for a rally shot (ADR-019). Standard and drive choose
 * a deep/neutral/short target from the aim; soft is a lob (deep, high) or,
 * aimed away from the net, a dink into the opponent's kitchen. Targets are
 * chosen for a left-team hitter and mirrored. A stretched (off-centre)
 * contact sprays wide and long, more so for a drive.
 */
static void collision_apply_targeted_shot(Ball *ball, const Player *player,
    const PlayerIntent *intent)
{
    s16 target_x;
    s16 target_y = HIT_TARGET_LANE_CENTER;
    s16 flight_frames;
    s16 distance;
    s16 pace;
    s16 min_frames = HIT_FLIGHT_MIN_FRAMES;
    s16 max_frames = HIT_FLIGHT_MAX_FRAMES;
    s16 wide = HIT_STRETCH_WIDE;
    s16 longer = HIT_STRETCH_LONG;
    s16 depth = court_aim_depth(player->team, intent->aim_x);
    u8 shot = player->shot_type;

    if (intent->aim_y < 0)
        target_y = HIT_TARGET_LANE_TOP;
    else if (intent->aim_y > 0)
        target_y = HIT_TARGET_LANE_BOTTOM;

    if (shot == SHOT_SOFT && depth < 0) {
        target_x = HIT_TARGET_DINK_RIGHT;
        pace = HIT_PACE_DINK_FP;
    } else if (shot == SHOT_SOFT) {
        target_x = HIT_TARGET_DEEP_RIGHT;
        pace = HIT_PACE_LOB_FP;
        min_frames = HIT_LOB_MIN_FRAMES;
        max_frames = HIT_LOB_MAX_FRAMES;
    } else {
        if (depth > 0) {
            target_x = HIT_TARGET_DEEP_RIGHT;
            pace = HIT_PACE_DEEP_FP;
        } else if (depth < 0) {
            target_x = HIT_TARGET_SHORT_RIGHT;
            pace = HIT_PACE_SHORT_FP;
        } else {
            target_x = HIT_TARGET_NEUTRAL_RIGHT;
            pace = HIT_PACE_NEUTRAL_FP;
        }
        if (shot == SHOT_DRIVE) {
            pace = HIT_PACE_DRIVE_FP;
            min_frames = HIT_DRIVE_MIN_FRAMES;
            max_frames = HIT_DRIVE_MAX_FRAMES;
            wide = HIT_DRIVE_STRETCH_WIDE;
            longer = HIT_DRIVE_STRETCH_LONG;
        }
    }

    if (!collision_ball_in_sweet_spot(player, ball)) {
        /* Stretched: the shot sprays toward its sideline and long. */
        if (target_y < HIT_TARGET_LANE_CENTER ||
            (target_y == HIT_TARGET_LANE_CENTER && ball->y_fp < player->y_fp))
            target_y -= wide;
        else
            target_y += wide;
        target_x += longer;
    }
    target_x = court_target_x(player->team, target_x);

    distance = absolute_fp(TO_FP(target_x) - ball->x_fp);
    flight_frames = distance / pace;
    if (flight_frames < min_frames)
        flight_frames = min_frames;
    else if (flight_frames > max_frames)
        flight_frames = max_frames;

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
