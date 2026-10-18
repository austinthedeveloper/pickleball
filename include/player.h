#ifndef PLAYER_H
#define PLAYER_H

#include "game_types.h"
#include "input.h"

#define FP_SHIFT 4
#define FP_ONE (1 << FP_SHIFT)
#define TO_FP(value) ((fx)((value) << FP_SHIFT))
#define FROM_FP(value) ((s16)((value) >> FP_SHIFT))

typedef enum {
    PLAYER_STATE_IDLE = 0,
    PLAYER_STATE_MOVE = 1,
    PLAYER_STATE_WINDUP = 2,
    PLAYER_STATE_ACTIVE = 3,
    PLAYER_STATE_RECOVERY = 4
} PlayerState;

/* Presentation-only overrides chosen by the match flow. */
typedef enum {
    PLAYER_POSE_NONE = 0,
    PLAYER_POSE_TOSS = 1,
    PLAYER_POSE_CHEER = 2
} PlayerPose;

enum {
    /* A swing: windup, then the frames it can meet the ball, then
     * recovery. */
    PLAYER_WINDUP_FRAMES = 4,
    PLAYER_ACTIVE_FRAMES = 3,
    /* The standard paddle's; g_paddle_recovery holds each paddle's
     * (paddle SPEED, ADR-020). */
    PLAYER_RECOVERY_FRAMES = 7,

    /* 12.4 units per frame: accel 0.25 px/frame/frame (0.1875 on a
     * diagonal), top speed 1.5 px/frame, friction 0.375 px/frame/frame.
     * A diagonal's speed limit is the straight one x 181/256
     * (g_player_diagonal_limit). */
    PLAYER_ACCEL_FP = 4,
    PLAYER_DIAGONAL_ACCEL_FP = 3,
    PLAYER_MAX_SPEED_FP = 24,
    PLAYER_FRICTION_FP = 6,
    /* ICE (ADR-053): half the acceleration and a sixth of the friction, so
     * a run takes 12 frames to reach top speed and slides about 18 px. */
    PLAYER_ICE_ACCEL_FP = 2,
    PLAYER_ICE_DIAGONAL_ACCEL_FP = 2,
    PLAYER_ICE_FRICTION_FP = 1,
    /* Top speed while swinging, 0.75 px/frame, for the standard paddle;
     * g_paddle_swing_limit holds each paddle's. */
    PLAYER_SWING_MAX_SPEED_FP = 12,
    /* The default CPU's top speed, 80% of a human's; each opponent's is
     * its AiStyle.speed (opponents.json). */
    PLAYER_CPU_MAX_SPEED_FP = 19,

    PLAYER_SEPARATION_X = 16,
    PLAYER_SEPARATION_Y = 12,

    PLAYER_VARIANT_MALE = 0,
    PLAYER_VARIANT_FEMALE = 1,

    PLAYER_IDLE_CEL_FRAMES = 20,
    PLAYER_RUN_CEL_FRAMES = 6
};

typedef struct {
    u8 active;
    u8 team;
    u8 logical_input_slot;
    u8 cpu_controlled;
    fx x_fp;
    fx y_fp;
    s16 vx_fp;
    s16 vy_fp;
    s16 max_speed_fp;
    u8 facing_left;
    u8 animation_frame;
    u8 animation_timer;
    u8 state;
    u8 state_frames_remaining;
    u8 contact_consumed;
    u8 frozen;
    u8 pose;
    u8 palette;
    u8 flash_timer;
    u8 shot_type;       /* ShotType latched when the swing starts */
    u8 variant;         /* PLAYER_VARIANT_*: ponytail accessory (ADR-014) */
    u8 paddle;          /* PADDLE_* stats (ADR-020) */
    u8 reserved;        /* keeps the size even on host and SNES */
} Player;

extern Player g_players[MAX_PLAYERS];

void players_initialize(void);
/* This match's footing (ICE or not): g_player_accel and the rest. Words,
 * for src/asm/player_step.asm. players_initialize sets the normal ones. */
extern s16 g_player_accel;
extern s16 g_player_diagonal_accel;
extern s16 g_player_friction;
void players_set_ice(u8 ice);
void players_update(const PlayerIntent intents[MAX_PLAYERS], u8 paused);
/* Doubles (ADR-018): nudge crowded teammates 1 px apart in y. */
void players_separate_teammates(void);
void player_place(Player *player, s16 x, s16 y);
u8 player_is_swinging(const Player *player);
s16 player_diagonal_limit(s16 limit);

#endif
