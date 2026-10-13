#ifndef GAME_H
#define GAME_H

#include "game_types.h"
#include "input.h"

typedef enum {
    GAME_PHASE_SERVE_SETUP = 0,
    GAME_PHASE_SERVE_TOSS = 1,
    GAME_PHASE_LIVE = 2,
    GAME_PHASE_SERVE_FAULT = 3,
    GAME_PHASE_POINT_RESULT = 4,
    GAME_PHASE_MATCH_RESULT = 5
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

enum {
    GAME_POINTS_TO_WIN = 5,
    GAME_SERVE_READY_FRAMES = 20,
    GAME_POINT_RESULT_MIN_FRAMES = 20,
    GAME_POINT_RESULT_FRAMES = 90,
    GAME_HIT_STOP_FRAMES = 2,
    GAME_HIT_FLASH_FRAMES = 2,
    GAME_NO_TEAM = 0xff
};

typedef struct {
    u8 paused;
    u8 phase;
    u16 phase_timer;
    u8 last_fault;
    u8 point_winner;
    u8 team_score[2];
    u8 required_bounce;
    u8 serving_team;
    u8 match_winner;
    u8 hit_stop;
    /* Increments on every serve and contact; lets controllers see a new shot. */
    u8 shot_serial;
    u16 frame_count;
} GameState;

extern GameState g_game_state;
/* Nonzero while the attract demo (CPU vs CPU behind the title) runs. */
extern u8 g_attract;

enum {
    GAME_ATTRACT_RESULT_FRAMES = 150,
    GAME_ATTRACT_LATE_PERIOD = 5
};

void game_initialize(void);
void game_new_match(void);
/* ATTRACT_READY: demo match with slot 0 driven by the AI. */
void game_start_attract(void);
/* Leaves attract and starts a human match. */
void game_start_match(void);
void game_set_phase(u8 phase);
void game_update(const PlayerIntent intents[MAX_PLAYERS]);

#endif
