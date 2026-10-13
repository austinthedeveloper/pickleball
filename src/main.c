#include <snes.h>
#include "ball.h"
#include "collision.h"
#include "game.h"
#include "input.h"
#include "player.h"
#include "render.h"
#include "rules.h"
#include "serve.h"

int main(void)
{
    input_initialize();
    players_initialize();
    ball_initialize();
    game_initialize();
    serve_initialize();
    render_initialize();

    while (1) {
        input_capture_frame();
        game_update_top_level(g_player_intents);
        players_update(g_player_intents, g_game_state.paused);
        if (g_game_state.phase == GAME_PHASE_SERVE_SETUP) {
            serve_update(g_player_intents, g_game_state.paused);
        } else if (g_game_state.phase == GAME_PHASE_SERVE_TOSS) {
            serve_update(g_player_intents, g_game_state.paused);
        } else if (g_game_state.phase == GAME_PHASE_LIVE) {
            u8 ball_events;
            if (!g_game_state.paused)
                collision_resolve_player_hits(&g_ball, g_players,
                    g_player_intents, MAX_PLAYERS);
            ball_events = ball_update(g_game_state.paused);
            serve_process_ball_events(ball_events);
            rules_process_ball_events(ball_events);
        }
        render_prepare_players();
        render_prepare_ball();
        WaitForVBlank();
        render_commit_hud(g_game_state.paused);
    }

    return 0;
}

