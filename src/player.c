#include "court.h"
#include "player.h"

#define PLAYER_ACCEL_FP 24
#define PLAYER_DIAGONAL_ACCEL_FP 17
#define PLAYER_MAX_SPEED_FP 384
#define PLAYER_DIAGONAL_MAX_FP 271
#define PLAYER_FRICTION_FP 20

Player g_players[MAX_PLAYERS];

const PlayerAnimation g_player_animations[2] = {
    {{0, 0}, 1, 12},
    {{0, 4}, 2, 8}
};

static s16 approach_zero(s16 value)
{
    if (value > PLAYER_FRICTION_FP)
        return value - PLAYER_FRICTION_FP;
    if (value < -PLAYER_FRICTION_FP)
        return value + PLAYER_FRICTION_FP;
    return 0;
}

static s16 clamp_velocity(s16 value, s16 limit)
{
    if (value > limit)
        return limit;
    if (value < -limit)
        return -limit;
    return value;
}

static u8 player_is_swinging(const Player *player)
{
    return player->state == PLAYER_STATE_WINDUP ||
        player->state == PLAYER_STATE_ACTIVE ||
        player->state == PLAYER_STATE_RECOVERY;
}

static void player_advance_swing(Player *player)
{
    if (--player->state_frames_remaining != 0)
        return;

    if (player->state == PLAYER_STATE_WINDUP) {
        player->state = PLAYER_STATE_ACTIVE;
        player->state_frames_remaining = PLAYER_ACTIVE_FRAMES;
    } else if (player->state == PLAYER_STATE_ACTIVE) {
        player->state = PLAYER_STATE_RECOVERY;
        player->state_frames_remaining = PLAYER_RECOVERY_FRAMES;
    } else {
        player->state = PLAYER_STATE_IDLE;
    }
}

void players_initialize(void)
{
    u8 i;
    for (i = 0; i < MAX_PLAYERS; ++i) {
        g_players[i].active = 0;
        g_players[i].team = (i < 2) ? 0 : 1;
        g_players[i].logical_input_slot = i;
        g_players[i].cpu_controlled = (i == 2);
        g_players[i].x_fp = 0;
        g_players[i].y_fp = 0;
        g_players[i].vx_fp = 0;
        g_players[i].vy_fp = 0;
        g_players[i].facing_left = 0;
        g_players[i].animation = PLAYER_ANIM_IDLE;
        g_players[i].animation_frame = 0;
        g_players[i].animation_timer = 0;
        g_players[i].state = PLAYER_STATE_IDLE;
        g_players[i].state_frames_remaining = 0;
        g_players[i].contact_consumed = 1;
    }

    g_players[0].active = 1;
    g_players[0].x_fp = TO_FP(PLAYER_SPAWN_X);
    g_players[0].y_fp = TO_FP(PLAYER_SPAWN_Y);
}

void players_update(const PlayerIntent intents[MAX_PLAYERS], u8 paused)
{
    u8 i;
    s16 accel;
    s16 limit;
    Player *player;
    const PlayerIntent *intent;
    const PlayerAnimation *animation;

    if (paused)
        return;

    for (i = 0; i < MAX_PLAYERS; ++i) {
        player = &g_players[i];
        if (!player->active)
            continue;

        intent = &intents[player->logical_input_slot];

        if (player_is_swinging(player)) {
            player_advance_swing(player);
        } else if (intent->hit_pressed) {
            player->state = PLAYER_STATE_WINDUP;
            player->state_frames_remaining = PLAYER_WINDUP_FRAMES;
            player->contact_consumed = 0;
        } else {
            player->state = (intent->move_x || intent->move_y) ?
                PLAYER_STATE_MOVE : PLAYER_STATE_IDLE;
        }

        if (intent->move_x && intent->move_y) {
            accel = PLAYER_DIAGONAL_ACCEL_FP;
            limit = PLAYER_DIAGONAL_MAX_FP;
        } else {
            accel = PLAYER_ACCEL_FP;
            limit = PLAYER_MAX_SPEED_FP;
        }

        if (player_is_swinging(player)) {
            if (intent->move_x && intent->move_y)
                limit = PLAYER_SWING_DIAGONAL_MAX_FP;
            else
                limit = PLAYER_SWING_MAX_SPEED_FP;
        }

        if (intent->move_x)
            player->vx_fp += intent->move_x * accel;
        else
            player->vx_fp = approach_zero(player->vx_fp);

        if (intent->move_y)
            player->vy_fp += intent->move_y * accel;
        else
            player->vy_fp = approach_zero(player->vy_fp);

        player->vx_fp = clamp_velocity(player->vx_fp, limit);
        player->vy_fp = clamp_velocity(player->vy_fp, limit);
        player->x_fp = court_clamp_x_fp(player->x_fp + player->vx_fp);
        player->y_fp = court_clamp_y_fp(player->y_fp + player->vy_fp);

        if (player->x_fp == TO_FP(PLAYER_CLAMP_MIN_X) ||
            player->x_fp == TO_FP(PLAYER_CLAMP_MAX_X))
            player->vx_fp = 0;
        if (player->y_fp == TO_FP(PLAYER_CLAMP_MIN_Y) ||
            player->y_fp == TO_FP(PLAYER_CLAMP_MAX_Y))
            player->vy_fp = 0;

        if (intent->move_x < 0)
            player->facing_left = 1;
        else if (intent->move_x > 0)
            player->facing_left = 0;

        player->animation = (intent->move_x || intent->move_y) ?
            PLAYER_ANIM_RUN : PLAYER_ANIM_IDLE;
        animation = &g_player_animations[player->animation];
        ++player->animation_timer;
        if (player->animation_timer >= animation->frame_delay) {
            player->animation_timer = 0;
            ++player->animation_frame;
            if (player->animation_frame >= animation->frame_count)
                player->animation_frame = 0;
        }
    }
}

