#include "game.h"

GameState g_game_state;

void game_initialize(void)
{
    g_game_state.paused = 0;
}

void game_update_top_level(const PlayerIntent intents[MAX_PLAYERS])
{
    if (intents[0].pause_pressed)
        g_game_state.paused = !g_game_state.paused;
}

