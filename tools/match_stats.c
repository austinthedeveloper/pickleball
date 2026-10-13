/*
 * Bot-vs-CPU balance statistics (docs/REFACTOR_PLAN.md R1, M3-MATCH-04).
 *
 * Plays N full matches on the host using the real simulation sources: the
 * CPU (slot 2) at its shipped difficulty against a stand-in P1 driven by the
 * same AI with a human skill level. Prints match length, win rate, and how
 * points ended.
 *
 *   make stats                     # defaults
 *   build/host/match_stats 4 20    # P1 mistimes every 4th shot, 20 matches
 *   build/host/match_stats 4 20 3  # ... against opponent 3 (ADR-023)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ai.h"
#include "game.h"
#include "match.h"
#include "serve.h"
#include "test_support.h"

static PlayerIntent intents[MAX_PLAYERS];
static const char *fault_names[] = {
    "none", "net", "kitchen", "wrong box", "out", "2 bounces", "volley"
};

int main(int argc, char **argv)
{
    int late = argc > 1 ? atoi(argv[1]) : 4;
    int matches = argc > 2 ? atoi(argv[2]) : 20;
    int opponent = argc > 3 ? atoi(argv[3]) : -1;
    int faults[2][7] = {{0}};
    int match = 0;
    int bot_wins = 0;
    int points = 0;
    unsigned long frames = 0;
    unsigned long cap = 60UL * 60 * 60;
    u8 previous = 0xff;
    u8 counted = 0;
    int team;
    int i;

    match_config_defaults();
    if (opponent >= 0)
        match_config_opponent((u8)opponent);
    game_new_match();
    while (match < matches && frames < cap) {
        test_bot_skill((u8)late, (u8)match);
        memset(intents, 0, sizeof(intents));
        ai_update(0, &intents[0]);
        if (g_game_state.phase == GAME_PHASE_SERVE_SETUP &&
            g_serve_state.server_index == 0 && (frames & 31) == 0)
            intents[0].hit_pressed = 1;
        if (g_game_state.phase == GAME_PHASE_MATCH_RESULT) {
            intents[0].hit_pressed = 1;
            if (!counted) {
                counted = 1;
                ++match;
                bot_wins += g_game_state.match_winner == 0;
            }
        } else {
            counted = 0;
        }
        test_step(intents);
        ++frames;
        if (g_game_state.phase == GAME_PHASE_POINT_RESULT &&
            previous != GAME_PHASE_POINT_RESULT) {
            ++faults[g_game_state.point_winner][g_game_state.last_fault % 7];
            ++points;
        }
        previous = g_game_state.phase;
    }

    printf("P1 stand-in mistimes every %d shot(s); %d matches", late, match);
    if (opponent >= 0 && opponent < OPPONENT_COUNT)
        printf(" vs %s", g_opponent_captain[opponent]);
    printf("\n");
    printf("  P1 won %d of %d matches; average match %.1f s; %d points\n",
        bot_wins, match, match ? frames / 60.0 / match : 0.0, points);
    for (team = 0; team < 2; ++team) {
        printf("  points won by %s:", team ? "CPU" : "P1 ");
        for (i = 0; i < 7; ++i)
            if (faults[team][i])
                printf(" %s %d", fault_names[i], faults[team][i]);
        printf("\n");
    }
    return 0;
}
