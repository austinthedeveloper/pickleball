#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "asset_frames.h"
#include "events.h"
#include "flair.h"
#include "input.h"
#include "match.h"
#include "serve.h"
#include "test_support.h"

/* ADR-064: at each point result, winners celebrate and losers slump, but a
 * loser running for a ball that bounced twice dives, gets up, then slumps;
 * nothing shows during play. Plays singles matches against the default
 * CPU with the bot. */
static void test_point_reactions(void)
{
    PlayerIntent in[MAX_PLAYERS];
    unsigned long f;
    int points = 0;
    int dives = 0;
    int dive_frames[3] = { 0, 0, 0 };
    u8 m = 0;
    u8 slot;
    u8 frame;

    match_config_defaults();
    g_attract = 0;
    game_new_match();
    for (f = 0; f < 60UL * 60 * 20; ++f) {
        test_bot_skill(4, m);
        memset(in, 0, sizeof(in));
        ai_update(0, &in[0]);
        if (g_game_state.phase == GAME_PHASE_SERVE_SETUP &&
                g_serve_state.server_index == 0 && (f & 31) == 0)
            in[0].hit_pressed = 1;
        if (g_game_state.phase == GAME_PHASE_MATCH_RESULT) {
            ++m;
            game_new_match();
        }
        test_step(in);
        if (g_game_state.phase == GAME_PHASE_LIVE) {
            for (slot = 0; slot < MAX_PLAYERS; ++slot)
                assert(flair_body_frame(slot) == FLAIR_NONE);
            continue;
        }
        if (g_game_state.phase != GAME_PHASE_POINT_RESULT)
            continue;
        if (g_flair.result_frames == 1)
            ++points;
        for (slot = 0; slot < MAX_PLAYERS; slot += 2) {
            frame = flair_body_frame(slot);
            if (g_players[slot].team == g_game_state.point_winner) {
                assert(frame == FRAME_CHEER_INDEX);     /* the default CPU */
                continue;
            }
            if (g_flair.react[slot] != FLAIR_REACT_DIVE) {
                assert(frame == FRAME_SLUMP_INDEX);
                continue;
            }
            assert(g_game_state.last_fault == GAME_FAULT_SECOND_BOUNCE);
            if (g_flair.result_frames == 1)
                ++dives;
            if (frame == FRAME_DIVE_INDEX) {
                assert(g_flair.result_frames < FLAIR_DIVE_FRAMES);
                ++dive_frames[0];
            } else if (frame == FRAME_SCRAMBLE_INDEX) {
                assert(g_flair.result_frames >= FLAIR_DIVE_FRAMES);
                ++dive_frames[1];
            } else {
                assert(frame == FRAME_SLUMP_INDEX &&
                    g_flair.result_frames >= FLAIR_SCRAMBLE_END);
                ++dive_frames[2];
            }
        }
    }
    printf("flair: %d points, %d dives (dive %d, scramble %d, slump %d "
        "frames)\n", points, dives, dive_frames[0], dive_frames[1],
        dive_frames[2]);
    assert(points > 50 && dives > 0 && dives < points);
    assert(dive_frames[0] && dive_frames[1] && dive_frames[2]);
    match_config_defaults();
}

/* Ladder players celebrate their own way; humans and the others cheer. */
static void test_celebrations(void)
{
    static const u8 expect[4] = {
        FRAME_THUMBS_UP_INDEX, FRAME_CHEER_INDEX, FRAME_FLEX_INDEX,
        FRAME_EN_GARDE_INDEX
    };
    u8 opponent;
    u8 i;

    for (opponent = 0; opponent < 4; ++opponent) {
        match_config_doubles();
        match_config_opponent(opponent);
        game_new_match();
        g_game_state.phase_timer = FLAIR_LOOKUP_DELAY;
        for (i = 0; i < MAX_PLAYERS; ++i)
            flair_tick();
        assert(g_flair.celebrate[0] == FRAME_CHEER_INDEX);  /* a human */
        assert(g_flair.celebrate[2] == expect[opponent]);
        assert(g_flair.celebrate[3] == expect[opponent]);
    }
    match_config_defaults();
    game_new_match();
    g_game_state.phase_timer = FLAIR_LOOKUP_DELAY;
    for (i = 0; i < MAX_PLAYERS; ++i)
        flair_tick();
    assert(g_flair.celebrate[2] == FRAME_CHEER_INDEX);
}

/* A quip's emote says how the point was lost; it lasts the result. */
static void test_quip_emotes(void)
{
    static const u8 faults[5] = {
        GAME_FAULT_NET, GAME_FAULT_OUT, GAME_FAULT_SECOND_BOUNCE,
        GAME_FAULT_KITCHEN, GAME_FAULT_EARLY_VOLLEY
    };
    static const u8 emotes[5] = {
        FLAIR_EMOTE_EXCLAIM, FLAIR_EMOTE_QUESTION, FLAIR_EMOTE_SWEAT,
        FLAIR_EMOTE_ANGER, FLAIR_EMOTE_ANGER
    };
    u8 i;
    u8 t;

    match_config_defaults();
    game_new_match();
    for (i = 0; i < 5; ++i) {
        game_set_phase(GAME_PHASE_POINT_RESULT);
        flair_tick();
        g_game_state.last_fault = faults[i];
        flair_quip(1);
        assert(g_flair.emote[2] == emotes[i]);
        assert(g_flair.emote[0] == FLAIR_EMOTE_NONE);   /* the winner */
        for (t = 0; t < 100; ++t)
            flair_tick();
        assert(g_flair.emote[2] == emotes[i]);          /* held */
        game_set_phase(GAME_PHASE_SERVE_SETUP);
        flair_tick();
        assert(g_flair.emote[2] == FLAIR_EMOTE_NONE);   /* the next serve */
        /* A quip picked late, in the next serve's setup: no emote. */
        flair_quip(1);
        assert(g_flair.emote[2] == FLAIR_EMOTE_NONE);
    }
}

/* Three soft shots in a row: a note over the third's hitter, for a while. */
static void test_dink_note(void)
{
    u8 t;

    match_config_defaults();
    game_new_match();
    game_set_phase(GAME_PHASE_LIVE);
    flair_tick();
    flair_on_event(EVENT_SERVE, 0, 0, 0);
    flair_on_event(EVENT_HIT, 0, 0, 2 | (SHOT_SOFT << 4));
    flair_on_event(EVENT_HIT, 0, 0, 0 | (SHOT_SOFT << 4));
    assert(g_flair.emote[0] == FLAIR_EMOTE_NONE);
    flair_on_event(EVENT_HIT, 0, 0, 2 | (SHOT_STANDARD << 4));
    flair_on_event(EVENT_HIT, 0, 0, 0 | (SHOT_SOFT << 4));
    flair_on_event(EVENT_HIT, 0, 0, 2 | (SHOT_SOFT << 4));
    assert(g_flair.emote[2] == FLAIR_EMOTE_NONE);       /* the run reset */
    flair_on_event(EVENT_HIT, 0, 0, 0 | (SHOT_SOFT << 4));
    assert(g_flair.emote[0] == FLAIR_EMOTE_NOTE);
    for (t = 1; t < FLAIR_NOTE_FRAMES; ++t)
        flair_tick();
    assert(g_flair.emote[0] == FLAIR_EMOTE_NOTE);
    flair_tick();
    assert(g_flair.emote[0] == FLAIR_EMOTE_NONE);
    /* A drive squashes the ball; a bounce ends its streak. */
    flair_on_event(EVENT_HIT, 0, 0, 0 | (SHOT_DRIVE << 4));
    assert(g_flair.squash_timer == FLAIR_SQUASH_FRAMES &&
        g_flair.shot == SHOT_DRIVE);
    flair_on_event(EVENT_BOUNCE, 90, 70, 1);
    assert(g_flair.shot == 0xff && g_flair.dust_timer == FLAIR_DUST_FRAMES &&
        g_flair.dust_x == 90 && g_flair.dust_y == 70);
    flair_on_event(EVENT_FAULT, 128, 60, GAME_FAULT_NET);
    assert(g_flair.cord_timer == FLAIR_CORD_FRAMES && g_flair.cord_y == 60);
    for (t = 0; t < FLAIR_CORD_FRAMES; ++t)
        flair_tick();
    assert(!g_flair.dust_timer && !g_flair.cord_timer &&
        !g_flair.squash_timer);
}

/* A human server slow to serve: the receivers nod off, until the serve. */
static void test_waiting_zzz(void)
{
    u16 t;

    match_config_defaults();
    g_attract = 0;
    game_new_match();
    g_input_owners[0] = INPUT_OWNER_HUMAN;
    game_set_phase(GAME_PHASE_SERVE_SETUP);
    flair_tick();
    for (t = 0; t < FLAIR_ZZZ_WAIT - 1; ++t) {
        ++g_game_state.phase_timer;
        flair_tick();
    }
    assert(g_flair.emote[2] == FLAIR_EMOTE_NONE);
    ++g_game_state.phase_timer;
    flair_tick();
    assert(g_flair.emote[2] == FLAIR_EMOTE_ZZZ);
    assert(g_flair.emote[0] == FLAIR_EMOTE_NONE);       /* not the server */
    game_set_phase(GAME_PHASE_SERVE_TOSS);
    flair_tick();
    assert(g_flair.emote[2] == FLAIR_EMOTE_NONE);
    /* A CPU server takes its time without them. */
    g_input_owners[0] = INPUT_OWNER_CPU;
    game_set_phase(GAME_PHASE_SERVE_SETUP);
    g_game_state.phase_timer = FLAIR_ZZZ_WAIT * 2;
    flair_tick();
    assert(g_flair.emote[2] == FLAIR_EMOTE_NONE);
}

int main(void)
{
    test_point_reactions();
    test_celebrations();
    test_quip_emotes();
    test_dink_note();
    test_waiting_zzz();
    printf("flair host checks passed\n");
    return 0;
}
