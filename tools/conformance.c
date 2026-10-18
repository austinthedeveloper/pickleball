/*
 * Conformance traces for ports (ADR-068, docs/TUNING.md).
 *
 * Plays a few fixed scenarios on the host with the real simulation sources,
 * from game_new_match, and prints every frame as plain integers: the
 * intents each active slot played that frame, then the state after it. A
 * port that is integer-exact replays the scenario and diffs line by line;
 * the first differing line is the first frame it went wrong. A port can
 * also feed the recorded intents to its sim alone, to test movement and
 * rules apart from its AI.
 *
 *   make conformance   # build/conformance/<scenario>.trace
 *
 * The CPUs run on main.c's schedule (TEST_CPU_THINKS); P1 is the bot from
 * tools/match_stats.c. The traces are generated, not committed: rebuild
 * them from the commit a port is matching.
 */
#include <stdio.h>
#include <string.h>
#include "ai.h"
#include "game.h"
#include "match.h"
#include "modifiers.h"
#include "options.h"
#include "serve.h"
#include "test_support.h"
#include "tuning.h"

enum { FRAMES = 5400 };

typedef struct {
    const char *name;
    const char *about;
    u8 doubles;
    u8 opponent;
    u8 difficulty;
    u8 modifiers;
    u8 bot_late;
} Scenario;

static const Scenario s_scenarios[] = {
    { "singles", "singles, P1 bot (late every 4th) vs PETE, MEDIUM",
      0, 3, DIFFICULTY_MEDIUM, 0, 4 },
    { "doubles", "doubles, P1 bot + CPU partner vs the TWINS, MEDIUM",
      1, 6, DIFFICULTY_MEDIUM, 0, 4 },
    { "kid", "singles, P1 bot vs the default CPU, KID (ball step pattern)",
      0, OPPONENT_NONE, DIFFICULTY_KID, 0, 5 },
    { "modifiers", "singles vs KAI, HARD, BIG BALL + FAST BALL + LAVA + "
      "POPCORN + ICE", 0, 5, DIFFICULTY_HARD,
      MODIFIER_BIG_BALL | MODIFIER_FAST_BALL | MODIFIER_LAVA |
      MODIFIER_POPCORN | MODIFIER_ICE, 3 }
};

static PlayerIntent s_intents[MAX_PLAYERS];

static void start(const Scenario *s)
{
    if (s->doubles)
        match_config_doubles();
    else
        match_config_defaults();
    if (s->opponent != OPPONENT_NONE)
        match_config_opponent(s->opponent);
    g_match_config.difficulty = s->difficulty;
    g_match_config.modifiers = s->modifiers;
    g_attract = 0;
    game_new_match();
    test_bot_skill(s->bot_late, 0);
}

static void print_frame(FILE *out, u16 frame)
{
    const Ball *b = &g_ball;
    u8 slot;

    fprintf(out, "%u G %u %u %u %u %u %u %u B %u %d %d %d %d %d %d %u %u",
        frame, g_game_state.phase, g_game_state.phase_timer,
        g_game_state.team_score[0], g_game_state.team_score[1],
        g_game_state.serving_team, g_serve_state.server_index,
        g_game_state.hit_stop,
        b->active, b->x_fp, b->y_fp, b->z_fp, b->vx_fp, b->vy_fp, b->vz_fp,
        b->bounce_count, b->last_hitter);
    for (slot = 0; slot < MAX_PLAYERS; ++slot) {
        const Player *p = &g_players[slot];
        const PlayerIntent *i = &s_intents[slot];

        if (!p->active)
            continue;
        fprintf(out, " I%u %d %d %d %d %u %u %u P%u %d %d %d %d %u %u %u",
            slot, i->move_x, i->move_y, i->aim_x, i->aim_y, i->hit_pressed,
            i->shot, i->hit_held,
            slot, p->x_fp, p->y_fp, p->vx_fp, p->vy_fp, p->state,
            p->state_frames_remaining, p->shot_type);
    }
    fputc('\n', out);
}

static void run(const Scenario *s, FILE *out)
{
    u16 frame;
    u8 slot;

    fprintf(out, "# Pickleball '94 conformance trace (tools/conformance.c, "
        "ADR-068)\n# scenario %s: %s\n# tuning version %d, %d frames\n",
        s->name, s->about, TUNING_VERSION, FRAMES);
    fprintf(out, "# frame G phase phase_timer score0 score1 serving_team "
        "server hit_stop\n#   B active x y z vx vy vz bounces last_hitter "
        "(12.4)\n#   per active slot: I<slot> move_x move_y aim_x aim_y "
        "hit_pressed shot hit_held\n#   P<slot> x y vx vy state "
        "state_frames shot_type (12.4)\n");
    start(s);
    for (frame = 0; frame < FRAMES; ++frame) {
        memset(s_intents, 0, sizeof(s_intents));
        ai_update(0, &s_intents[0]);
        if (g_game_state.phase == GAME_PHASE_SERVE_SETUP &&
                g_serve_state.server_index == 0 && (frame & 31) == 0)
            s_intents[0].hit_pressed = 1;
        if (g_game_state.phase == GAME_PHASE_MATCH_RESULT &&
                (frame & 63) == 0)
            s_intents[0].hit_pressed = 1;
        for (slot = 1; slot < MAX_PLAYERS; ++slot) {
            if (!g_players[slot].active ||
                    g_input_owners[slot] != INPUT_OWNER_CPU)
                continue;
            if (TEST_CPU_THINKS(slot, frame))
                ai_update(slot, &s_intents[slot]);
            else
                ai_hold_intent(slot, &s_intents[slot]);
        }
        game_update(s_intents);
        print_frame(out, frame);
        if (g_next_opponent)
            start(s);
    }
}

int main(int argc, char **argv)
{
    unsigned i;

    if (argc != 2) {
        fprintf(stderr, "usage: conformance OUT_DIR\n");
        return 2;
    }
    for (i = 0; i < sizeof(s_scenarios) / sizeof(s_scenarios[0]); ++i) {
        char path[512];
        FILE *out;

        snprintf(path, sizeof(path), "%s/%s.trace", argv[1],
            s_scenarios[i].name);
        out = fopen(path, "w");
        if (!out) {
            perror(path);
            return 1;
        }
        run(&s_scenarios[i], out);
        fclose(out);
        printf("wrote %s\n", path);
    }
    return 0;
}
