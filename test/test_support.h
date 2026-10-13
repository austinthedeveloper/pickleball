#ifndef TEST_SUPPORT_H
#define TEST_SUPPORT_H

/*
 * Shared host-test helpers (docs/REFACTOR_PLAN.md R7). Header-only so each
 * test binary stays a single translation unit plus the game sources.
 */
#include "ai.h"
#include "game.h"
#include "player.h"

/* One simulation frame the way main.c runs it once the pads are read: the
 * CPU (slot 2) thinks, then the game updates. */
static inline void test_step(PlayerIntent intents[MAX_PLAYERS])
{
    ai_update(2, &intents[2]);
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

#endif
