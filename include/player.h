#ifndef PLAYER_H
#define PLAYER_H

#include "game_types.h"
#include "input.h"

#define FP_SHIFT 8
#define FP_ONE (1L << FP_SHIFT)
#define TO_FP(value) ((s32)(value) << FP_SHIFT)
#define FROM_FP(value) ((s16)((value) >> FP_SHIFT))

typedef enum {
    PLAYER_ANIM_IDLE = 0,
    PLAYER_ANIM_RUN = 1
} PlayerAnimationId;

typedef struct {
    u8 active;
    u8 team;
    u8 logical_input_slot;
    u8 cpu_controlled;
    s32 x_fp;
    s32 y_fp;
    s16 vx_fp;
    s16 vy_fp;
    u8 facing_left;
    u8 animation;
    u8 animation_frame;
    u8 animation_timer;
} Player;

typedef struct {
    u16 tile_base[2];
    u8 frame_count;
    u8 frame_delay;
} PlayerAnimation;

extern Player g_players[MAX_PLAYERS];
extern const PlayerAnimation g_player_animations[2];

void players_initialize(void);
void players_update(const PlayerIntent intents[MAX_PLAYERS], u8 paused);

#endif

