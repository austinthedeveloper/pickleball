#include "game.h"

GameState g_game_state;

void game_initialize(void)
{
    g_game_state.paused = 0;
    g_game_state.phase = GAME_PHASE_SERVE_SETUP;
    g_game_state.phase_timer = 0;
    g_game_state.last_fault = GAME_FAULT_NONE;
    g_game_state.point_winner = 0xff;
    g_game_state.team_score[0] = 0;
    g_game_state.team_score[1] = 0;
    g_game_state.required_bounce = GAME_REQUIRED_BOUNCE_NONE;
}

void game_update_top_level(const PlayerIntent intents[MAX_PLAYERS])
{
    if (intents[0].pause_pressed)
        g_game_state.paused = !g_game_state.paused;
}

