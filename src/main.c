#include <snes.h>
#include "ai.h"
#include "audio_events.h"
#include "court_style.h"
#include "game.h"
#include "hud_scene.h"
#include "serve.h"
#include "input.h"
#include "match.h"
#include "player.h"
#include "profile.h"
#include "render.h"
#include "save.h"
#include "select.h"

/* The court (title, attract, match) or Player Select (ADR-020). */
enum { SCENE_MATCH = 0, SCENE_SELECT = 1 };
static u8 s_scene;
static u8 s_open_select;

/* Records page (ADR-021): Start, B, or Y closes it; holding L+R and
 * pressing X erases the records (court colours stay). */
static void update_title_records(u16 pressed)
{
    if ((g_pad_snapshots[0].held & (PAD_L | PAD_R)) == (PAD_L | PAD_R) &&
            (pressed & PAD_X)) {
        save_clear_records();
        hud_scene_refresh_title();
    } else if (pressed & (PAD_START | PAD_B | PAD_Y)) {
        g_title_page = TITLE_PAGE_MENU;
    }
}

/* Court page (ADR-022/024): colour changes show on the court behind it at
 * once; B or Start keeps them (and saves), Y puts them back. A new venue
 * loads when the page closes. */
static void update_title_court(u16 pressed)
{
    SaveSettings *settings = &g_save.settings;
    u8 result = court_edit_input(settings->court, &settings->venue, pressed);

    if (g_court_edit.recolor) {
        g_court_edit.recolor = 0;
        render_apply_court_colors();
    }
    if (result == COURT_EDIT_STAY)
        return;
    g_title_page = TITLE_PAGE_MENU;
    if (result != COURT_EDIT_DONE ||
            !court_edit_changed(settings->court, settings->venue))
        return;
    save_write();
    if (settings->venue != g_court_edit.original_venue) {
        render_open_match();
        profile_resync();
    }
}

/* Title: Up/Down picks an entry, Start on pad 2 toggles 2P, and Start/B
 * opens Player Select (singles, doubles) or a page (court, records). */
static void update_title(void)
{
    u16 pressed = g_pad_snapshots[0].pressed;

    if (g_title_page == TITLE_PAGE_RECORDS)
        update_title_records(pressed);
    else if (g_title_page == TITLE_PAGE_COURT)
        update_title_court(pressed);
    else {
        if (pressed & PAD_UP)
            g_title_mode = g_title_mode ? g_title_mode - 1 :
                TITLE_ENTRIES - 1;
        if (pressed & PAD_DOWN)
            g_title_mode = g_title_mode == TITLE_ENTRIES - 1 ? 0 :
                g_title_mode + 1;
        if (g_pad_snapshots[1].pressed & PAD_START)
            g_title_2p ^= 1;
        if (pressed & (PAD_START | PAD_B)) {
            if (g_title_mode == TITLE_RECORDS) {
                g_title_page = TITLE_PAGE_RECORDS;
            } else if (g_title_mode == TITLE_COURT) {
                court_edit_begin(g_save.settings.court,
                    &g_save.settings.venue);
                g_title_page = TITLE_PAGE_COURT;
            } else {
                select_begin(g_title_mode == TITLE_DOUBLES ? MATCH_DOUBLES :
                    MATCH_SINGLES, g_title_2p ? 2 : 1);
                s_open_select = 1;
                return;
            }
        }
    }
    if (g_loop_count & 1)
        ai_update(0, &g_player_intents[0]);
    else
        ai_hold_intent(0, &g_player_intents[0]);
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

/* One Player Select frame: 1P reads port 1, 2P port 2. */
static void update_select(void)
{
    u8 result = select_input(g_pad_snapshots[g_select.current].pressed);

    if (result == SELECT_STAY) {
        render_select_prepare();
        WaitForVBlank();
        render_select_commit();
        return;
    }
    if (result == SELECT_DONE) {
        select_apply();
        game_start_match();
    } else {
        game_start_attract();
    }
    /* render_open_match applies the new config itself. */
    g_match_config_changed = 0;
    render_open_match();
    s_scene = SCENE_MATCH;
    profile_resync();
}

int main(void)
{
    input_initialize();
    audio_events_initialize();
    save_initialize();
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
    g_title_mode = TITLE_SINGLES;
    g_title_page = TITLE_PAGE_MENU;
    g_title_2p = 0;
    s_scene = SCENE_MATCH;
    s_open_select = 0;
    g_match_config_changed = 0;
    g_demo_mode = 0;
    game_start_attract();
    render_initialize();
    /* render_initialize applied the attract config already. */
    g_match_config_changed = 0;
    /* Restart timing after the forced-blank uploads. */
    profile_initialize();

    /* docs/02_TECHNICAL_PLAN.md frame pipeline. */
    while (1) {
        profile_frame_begin();
        input_capture_frame();
        if (s_scene == SCENE_SELECT) {
            update_select();
            profile_frame_end();
            continue;
        }
        if (s_open_select) {
            s_open_select = 0;
            render_select_open();
            s_scene = SCENE_SELECT;
            profile_resync();
            continue;
        }
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
        /* A new attract demo or a return to the title: palettes, ponytails
         * and HUD names follow the new config. */
        if (g_match_config_changed) {
            render_apply_match_config();
            hud_scene_invalidate();
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
