#include <string.h>
#include "court.h"
#include "player.h"

Player g_players[MAX_PLAYERS];

s16 player_diagonal_limit(s16 limit)
{
    return (s16)((limit * 181) >> 8);
}

u8 player_is_swinging(const Player *player)
{
    return player->state == PLAYER_STATE_WINDUP ||
        player->state == PLAYER_STATE_ACTIVE ||
        player->state == PLAYER_STATE_RECOVERY;
}

void player_place(Player *player, s16 x, s16 y)
{
    player->x_fp = TO_FP(x);
    player->y_fp = TO_FP(y);
    player->vx_fp = 0;
    player->vy_fp = 0;
    player->state = PLAYER_STATE_IDLE;
    player->state_frames_remaining = 0;
    player->contact_consumed = 1;
    player->animation_frame = 0;
    player->animation_timer = 0;
}

void players_initialize(void)
{
    u8 i;
    Player *player;

    for (i = 0; i < MAX_PLAYERS; ++i) {
        player = &g_players[i];
        player->active = 0;
        player->team = (i < 2) ? 0 : 1;
        player->logical_input_slot = i;
        player->cpu_controlled = (i == 2);
        player->max_speed_fp = player->cpu_controlled ?
            PLAYER_CPU_MAX_SPEED_FP : PLAYER_MAX_SPEED_FP;
        player->facing_left = player->team;
        player->frozen = 0;
        player->pose = PLAYER_POSE_NONE;
        player->palette = player->team;
        player->flash_timer = 0;
        player_place(player, 0, 0);
    }

    g_players[0].active = 1;
    player_place(&g_players[0], PLAYER_SPAWN_X, PLAYER_SPAWN_Y);
}

/* Static scratch copy: one long access per field instead of pointer
 * dereferences, which 816-tcc compiles very expensively. */
static Player s_p;

#define APPROACH_ZERO(v) \
    ((v) > PLAYER_FRICTION_FP ? (v) - PLAYER_FRICTION_FP : \
     (v) < -PLAYER_FRICTION_FP ? (v) + PLAYER_FRICTION_FP : 0)

static void player_step(const PlayerIntent *intent)
{
    s16 accel;
    s16 limit;
    s16 move_x;
    s16 move_y;
    u8 hit;
    u8 was_moving;
    u8 moving;
    u8 cel_frames;
    fx next;

    if (s_p.flash_timer)
        --s_p.flash_timer;

    move_x = intent->move_x;
    move_y = intent->move_y;
    hit = intent->hit_pressed;
    if (s_p.frozen) {
        move_x = 0;
        move_y = 0;
        hit = 0;
    }
    was_moving = s_p.state == PLAYER_STATE_MOVE;

    if (s_p.state >= PLAYER_STATE_WINDUP) {
        if (--s_p.state_frames_remaining == 0) {
            if (s_p.state == PLAYER_STATE_WINDUP) {
                s_p.state = PLAYER_STATE_ACTIVE;
                s_p.state_frames_remaining = PLAYER_ACTIVE_FRAMES;
            } else if (s_p.state == PLAYER_STATE_ACTIVE) {
                s_p.state = PLAYER_STATE_RECOVERY;
                s_p.state_frames_remaining = PLAYER_RECOVERY_FRAMES;
            } else {
                s_p.state = PLAYER_STATE_IDLE;
            }
        }
    } else if (hit) {
        s_p.state = PLAYER_STATE_WINDUP;
        s_p.state_frames_remaining = PLAYER_WINDUP_FRAMES;
        s_p.contact_consumed = 0;
    } else {
        s_p.state = (move_x || move_y) ? PLAYER_STATE_MOVE : PLAYER_STATE_IDLE;
    }

    limit = s_p.state >= PLAYER_STATE_WINDUP ?
        PLAYER_SWING_MAX_SPEED_FP : s_p.max_speed_fp;
    accel = PLAYER_ACCEL_FP;
    if (move_x && move_y) {
        accel = PLAYER_DIAGONAL_ACCEL_FP;
        limit = player_diagonal_limit(limit);
    }

    if (move_x > 0)
        s_p.vx_fp += accel;
    else if (move_x < 0)
        s_p.vx_fp -= accel;
    else
        s_p.vx_fp = APPROACH_ZERO(s_p.vx_fp);
    if (move_y > 0)
        s_p.vy_fp += accel;
    else if (move_y < 0)
        s_p.vy_fp -= accel;
    else
        s_p.vy_fp = APPROACH_ZERO(s_p.vy_fp);

    if (s_p.vx_fp > limit)
        s_p.vx_fp = limit;
    else if (s_p.vx_fp < -limit)
        s_p.vx_fp = -limit;
    if (s_p.vy_fp > limit)
        s_p.vy_fp = limit;
    else if (s_p.vy_fp < -limit)
        s_p.vy_fp = -limit;

    next = s_p.x_fp + s_p.vx_fp;
    s_p.x_fp = court_clamp_x_fp(next, s_p.team);
    if (s_p.x_fp != next)
        s_p.vx_fp = 0;
    next = s_p.y_fp + s_p.vy_fp;
    s_p.y_fp = court_clamp_y_fp(next);
    if (s_p.y_fp != next)
        s_p.vy_fp = 0;

    /* Players always square up to the net; flips come from OAM. */
    s_p.facing_left = s_p.team;

    moving = s_p.state == PLAYER_STATE_MOVE;
    if (moving != was_moving) {
        s_p.animation_frame = 0;
        s_p.animation_timer = 0;
    }
    cel_frames = moving ? PLAYER_RUN_CEL_FRAMES : PLAYER_IDLE_CEL_FRAMES;
    if (++s_p.animation_timer >= cel_frames) {
        s_p.animation_timer = 0;
        if (++s_p.animation_frame >= (moving ? 4 : 2))
            s_p.animation_frame = 0;
    }
}

void players_update(const PlayerIntent intents[MAX_PLAYERS], u8 paused)
{
    u8 i;
    Player *player;

    if (paused)
        return;

    player = g_players;
    for (i = 0; i < MAX_PLAYERS; ++i, ++player) {
        if (!player->active)
            continue;
        memcpy(&s_p, player, sizeof(Player));
        player_step(&intents[s_p.logical_input_slot]);
        memcpy(player, &s_p, sizeof(Player));
    }
}
