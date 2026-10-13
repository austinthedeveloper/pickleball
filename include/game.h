#ifndef GAME_H
#define GAME_H

#include "game_types.h"
#include "input.h"

typedef enum {
    GAME_PHASE_SERVE_SETUP = 0,
    GAME_PHASE_SERVE_TOSS = 1,
    GAME_PHASE_LIVE = 2,
    GAME_PHASE_SERVE_FAULT = 3,
    GAME_PHASE_POINT_RESULT = 4
} GamePhase;

typedef enum {
    GAME_FAULT_NONE = 0,
    GAME_FAULT_NET = 1,
    GAME_FAULT_KITCHEN = 2,
    GAME_FAULT_WRONG_BOX = 3,
    GAME_FAULT_OUT = 4,
    GAME_FAULT_SECOND_BOUNCE = 5,
    GAME_FAULT_EARLY_VOLLEY = 6
} GameFault;

typedef enum {
    GAME_REQUIRED_BOUNCE_NONE = 0,
    GAME_REQUIRED_BOUNCE_SERVE = 1,
    GAME_REQUIRED_BOUNCE_RETURN = 2
} GameRequiredBounce;

typedef struct {
    u8 paused;
    u8 phase;
    u8 phase_timer;
    u8 last_fault;
    u8 point_winner;
    u8 team_score[2];
    u8 required_bounce;
} GameState;

extern GameState g_game_state;

void game_initialize(void);
void game_update_top_level(const PlayerIntent intents[MAX_PLAYERS]);

#endif

