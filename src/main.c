#include <snes.h>
#include "ai.h"
#include "game.h"
#include "input.h"
#include "match.h"
#include "profile.h"
#include "render.h"

/* Human and CPU intents for this frame (frame pipeline steps 1-3). The pads
 * were already captured this frame. */
static void update_controllers(void)
{
    if (g_attract) {
        /* Title: any Start/B press begins a human match. */
        if (g_pad_snapshots[0].pressed & (PAD_START | PAD_B)) {
            game_start_match();
            /* Consume the press: it must not also pause or swing. */
            g_player_intents[0].pause_pressed = 0;
            g_player_intents[0].hit_pressed = 0;
        } else if (g_loop_count & 1) {
            ai_update(0, &g_player_intents[0]);
        } else {
            ai_hold_intent(0, &g_player_intents[0]);
        }
        PROFILE_MARK(6);
    }
    ai_update(2, &g_player_intents[2]);
    PROFILE_MARK(0);
}

int main(void)
{
    input_initialize();
    match_config_defaults();
#ifdef BUILD_DEBUG
    /* ADR-010: every active player has a unique palette. */
    if (!match_config_palettes_unique())
        while (1)
            ;
#endif
    /* Before the first render: the debug overlay reads the counters. */
    profile_initialize();
    g_attract = 0;
    game_start_attract();
    render_initialize();
    /* Restart timing after the forced-blank uploads. */
    profile_initialize();

    /* docs/02_TECHNICAL_PLAN.md frame pipeline. */
    while (1) {
        profile_frame_begin();
        input_capture_frame();
#ifdef RENDER_DIAGNOSTICS
        /* Select toggles the OBJ stress scene, which freezes the match. */
        if (g_pad_snapshots[0].pressed & PAD_SELECT)
            render_stress_toggle();
        if (!render_stress_active())
#endif
        {
            update_controllers();
            game_update(g_player_intents);
            PROFILE_MARK(1);
        }
        render_prepare();
        PROFILE_MARK(7);
        profile_work_done();
        WaitForVBlank();
        render_commit();
        profile_frame_end();
    }

    return 0;
}
