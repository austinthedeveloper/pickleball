#ifndef TEST_SUPPORT_H
#define TEST_SUPPORT_H

/*
 * Shared host-test helpers (docs/REFACTOR_PLAN.md R7). Header-only so each
 * test binary stays a single translation unit plus the game sources.
 */
#include <assert.h>
#include <string.h>
#include "ai.h"
#include "career.h"
#include "cpu_schedule.h"
#include "game.h"
#include "player.h"
#include "input.h"
#include "save.h"
#include "serve.h"

/* One simulation frame the way main.c runs it once the pads are read: the
 * CPU (slot 2) thinks, then the game updates. */
static inline void test_step(PlayerIntent intents[MAX_PLAYERS])
{
    ai_update(2, &intents[2]);
    game_update(intents);
}

/*
 * One frame with the CPUs on main.c's schedule (CPU_THINKS, ADR-018,
 * ADR-069). Slot 0 (the P1 stand-in) is the caller's.
 */
static inline void test_step_scheduled(PlayerIntent intents[MAX_PLAYERS])
{
    u8 slot;

    for (slot = 1; slot < MAX_PLAYERS; ++slot) {
        if (!g_players[slot].active ||
                g_input_owners[slot] != INPUT_OWNER_CPU)
            continue;
        if (CPU_THINKS(slot))
            ai_update(slot, &intents[slot]);
        else
            ai_hold_intent(slot, &intents[slot]);
    }
    game_update(intents);
}

/*
 * P1 stand-in with a human skill level: the same AI, mistiming every
 * `late`th shot, with reaction delay and foot speed varied by match index.
 * Used by test_match_flow.c and tools/match_stats.c so balance numbers in
 * tests and tooling come from one definition.
 */
static inline void test_bot_skill(u8 late, u8 match_index)
{
    g_ai_slots[0].late_period = late;
    g_ai_slots[0].reaction_frames = (u8)((match_index * 7) % 20);
    g_players[0].max_speed_fp = 16 + (match_index % 3) * 4;
}

/*
 * The bot P1's buttons beside its AI, as every bot-vs-CPU loop presses
 * them: B on every 32nd frame of its own serve setup (the AI does not
 * serve for a human slot), and B on the match result to play again.
 */
static inline u8 test_bot_serve_due(unsigned long frame)
{
    return g_game_state.phase == GAME_PHASE_SERVE_SETUP &&
        g_serve_state.server_index == 0 && (frame & 31) == 0;
}

static inline void test_bot_serve(PlayerIntent *p1, unsigned long frame)
{
    if (test_bot_serve_due(frame))
        p1->hit_pressed = 1;
}

/* B on the match result; nonzero on the result's first frame, a match to
 * count. *counted (0 to start) remembers that it was counted. */
static inline u8 test_bot_match_over(PlayerIntent *p1, u8 *counted)
{
    if (g_game_state.phase != GAME_PHASE_MATCH_RESULT) {
        *counted = 0;
        return 0;
    }
    p1->hit_pressed = 1;
    if (*counted)
        return 0;
    *counted = 1;
    return 1;
}

/* Both: the serve press, then the result's. */
static inline u8 test_bot_buttons(PlayerIntent *p1, unsigned long frame,
    u8 *counted)
{
    test_bot_serve(p1, frame);
    return test_bot_match_over(p1, counted);
}

/* A blank battery save, loaded. */
static inline void test_blank_save(void)
{
    memset(g_host_sram, 0, sizeof(g_host_sram));
    save_initialize();
}

/* The career unlock bit with this name (CAREER_UNLOCK_*). */
static inline u8 test_unlock_bit(const char *name)
{
    u8 i;

    for (i = 0; i < CAREER_UNLOCKS; ++i)
        if (strcmp(g_career_unlock_names[i], name) == 0)
            return i;
    assert(0);
    return 0;
}

#endif
