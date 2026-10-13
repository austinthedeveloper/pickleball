#ifndef AI_H
#define AI_H

#include "ball.h"
#include "input.h"
#include "player.h"

enum {
    AI_PREDICT_MAX_STEPS = 60,
    AI_RECOVER_X = 208,
    AI_RECOVER_Y = 130,
    /* Stand this far behind the predicted contact point (difficulty knob). */
    AI_CONTACT_OFFSET_X = 10,
    AI_DEAD_ZONE = 3,
    AI_SERVE_DELAY_FRAMES = 50,
    /* Look ahead windup + first active frame when deciding to swing. */
    AI_SWING_LOOKAHEAD = PLAYER_WINDUP_FRAMES + 1
};

typedef struct {
    u8 has_target;
    s16 target_x;
    s16 target_y;
    s16 predicted_land_x;
    s16 predicted_land_y;
    u8 predicted_steps;
    u8 shot_counter;
    u8 plan_key;
} AiState;

extern AiState g_ai;

void ai_initialize(void);
/* Predicts the next ground contact; returns steps simulated (<= cap). */
u8 ai_predict_landing(const Ball *ball, s16 *land_x, s16 *land_y);
/* Writes the CPU player's PlayerIntent for this frame. */
void ai_update(u8 player_index, PlayerIntent *intent);

#endif
