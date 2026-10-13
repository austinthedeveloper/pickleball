#include <snes.h>
#include "ai.h"
#include "audio_events.h"
#include "game.h"
#include "hud_scene.h"
#include "serve.h"
#include "input.h"
#include "match.h"
#include "player.h"
#include "profile.h"
#include "render.h"

/* Title: Up/Down picks singles or doubles; Start/B begins the match. */
static void update_title(void)
{
    u16 pressed = g_pad_snapshots[0].pressed;

    if (pressed & (PAD_UP | PAD_DOWN))
        g_title_mode ^= 1;
    if (pressed & (PAD_START | PAD_B)) {
        game_start_match(g_title_mode ? MATCH_DOUBLES : MATCH_SINGLES);
        /* Consume the press: it must not also pause or swing. */
        g_player_intents[0].pause_pressed = 0;
        g_player_intents[0].hit_pressed = 0;
    } else if (g_loop_count & 1) {
        ai_update(0, &g_player_intents[0]);
    } else {
        ai_hold_intent(0, &g_player_intents[0]);
    }
}

/*
 * A CPU playing the ball (or alone on its side, or serving) thinks every
 * frame; a doubles CPU covering its lane thinks every fourth frame,
 * staggered by slot, and repeats its movement in between (ADR-018).
 * A macro so every index is a constant: 816-tcc multiplies for each
 * variable g_players[i] / g_ai_slots[i] access.
 */
#define UPDATE_CPU(slot) \
    do { \
        if (g_input_owners[slot] == INPUT_OWNER_CPU && \
                g_players[slot].active) { \
            if (!g_players[(slot) ^ 1].active || \
                    g_ai_slots[slot].has_target || \
                    g_serve_state.server_index == (slot) || \
                    ((u8)g_loop_count & 3) == (slot)) \
                ai_update(slot, &g_player_intents[slot]); \
            else \
                ai_hold_intent(slot, &g_player_intents[slot]); \
        } \
    } while (0)

/* Human and CPU intents for this frame (frame pipeline steps 1-3). The pads
 * were already captured this frame. */
static void update_controllers(void)
{
    if (g_attract) {
        update_title();
        PROFILE_MARK(6);
    }
    /* Constant slot numbers keep 816-tcc from multiplying per access. */
    UPDATE_CPU(1);
    UPDATE_CPU(2);
    UPDATE_CPU(3);
    PROFILE_MARK(0);
}

int main(void)
{
    input_initialize();
    audio_events_initialize();
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
    g_title_mode = 0;
    g_match_config_changed = 0;
    g_demo_mode = 0;
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
        /* Re-apply presentation only when the mode really changed: the
         * full redraw costs a frame. Same-mode starts keep everything. */
        if (g_match_config_changed) {
            if (render_match_mode_applied() != g_match_config.mode) {
                render_apply_match_config();
                hud_scene_invalidate();
            }
            g_match_config_changed = 0;
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
