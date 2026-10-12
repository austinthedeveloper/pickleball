#ifndef GAME_H
#define GAME_H

#include "game_types.h"
#include "input.h"

typedef struct {
    u8 paused;
} GameState;

extern GameState g_game_state;

void game_initialize(void);
void game_update_top_level(const PlayerIntent intents[MAX_PLAYERS]);

#endif

