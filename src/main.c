#include <snes.h>
#include "game.h"
#include "input.h"
#include "player.h"
#include "render.h"

int main(void)
{
    input_initialize();
    players_initialize();
    game_initialize();
    render_initialize();

    while (1) {
        input_capture_frame();
        game_update_top_level(g_player_intents);
        players_update(g_player_intents, g_game_state.paused);
        render_set_paused(g_game_state.paused);
        render_prepare_players();
        WaitForVBlank();
        render_commit_hud();
    }

    return 0;
}

