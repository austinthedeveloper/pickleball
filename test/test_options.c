#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ai.h"
#include "game.h"
#include "match.h"
#include "audio_events.h"
#include "options.h"
#include "rules.h"
#include "save.h"
#include "select.h"
#include "serve.h"
#include "test_support.h"

/* ADR-025 options: points to win, difficulty, slow ball, KID swing assist. */

static void test_values(void)
{
    assert(options_points(5) == 5 && options_points(21) == 21);
    assert(options_points(4) == OPTIONS_POINTS_DEFAULT);
    assert(options_points(0) == OPTIONS_POINTS_DEFAULT);
    assert(options_difficulty(DIFFICULTY_HARD) == DIFFICULTY_HARD);
    assert(options_difficulty(9) == DIFFICULTY_MEDIUM);
    assert(strcmp(options_difficulty_name(DIFFICULTY_KID), "KID") == 0);

    /* A fresh save holds the defaults. */
    memset(g_host_sram, 0, sizeof(g_host_sram));
    save_initialize();
    assert(g_save.settings.points == OPTIONS_POINTS_DEFAULT);
    assert(g_save.settings.difficulty == DIFFICULTY_MEDIUM);
}

static void test_difficulty_styles(void)
{
    AiStyle style[DIFFICULTY_COUNT];
    u8 d;

    for (d = 0; d < DIFFICULTY_COUNT; ++d) {
        style[d] = g_ai_default_style;
        options_apply_difficulty(&style[d], d);
    }
    assert(memcmp(&style[DIFFICULTY_MEDIUM], &g_ai_default_style,
        sizeof(AiStyle)) == 0);
    for (d = 1; d < DIFFICULTY_COUNT; ++d) {
        assert(style[d].late_period > style[d - 1].late_period);
        assert(style[d].reaction <= style[d - 1].reaction);
        assert(style[d].speed >= style[d - 1].speed);
    }
    assert(style[DIFFICULTY_KID].late_period >= 2);
    assert(style[DIFFICULTY_HARD].speed <= PLAYER_MAX_SPEED_FP);
}

static void test_ball_pattern(void)
{
    u8 counts[DIFFICULTY_COUNT] = { 0 };
    u8 d;
    u8 t;

    for (d = 0; d < DIFFICULTY_COUNT; ++d)
        for (t = 0; t < OPTIONS_BALL_CYCLE; ++t)
            counts[d] += options_ball_moves(d, t);
    assert(counts[DIFFICULTY_KID] == 8);        /* 2/3 speed */
    assert(counts[DIFFICULTY_EASY] == 9);       /* 3/4 speed */
    assert(counts[DIFFICULTY_MEDIUM] == 12);
    assert(counts[DIFFICULTY_HARD] == 12);
}

static void test_editor(void)
{
    u8 points = 4;          /* invalid */
    u8 difficulty = 7;      /* invalid */
    u8 sound = 9;           /* invalid */

    options_edit_begin(&points, &difficulty, &sound);
    assert(sound == AUDIO_ALL);
    assert(points == OPTIONS_POINTS_DEFAULT);
    assert(difficulty == DIFFICULTY_MEDIUM);
    options_edit_input(&points, &difficulty, &sound, PAD_RIGHT);
    assert(points == 7);
    options_edit_input(&points, &difficulty, &sound, PAD_LEFT);
    options_edit_input(&points, &difficulty, &sound, PAD_LEFT);
    options_edit_input(&points, &difficulty, &sound, PAD_LEFT);
    assert(points == 21);                       /* wraps */
    options_edit_input(&points, &difficulty, &sound, PAD_DOWN);
    options_edit_input(&points, &difficulty, &sound, PAD_RIGHT);
    options_edit_input(&points, &difficulty, &sound, PAD_RIGHT);
    assert(difficulty == DIFFICULTY_KID);       /* wraps */
    assert(options_edit_changed(points, difficulty, sound));
    assert(options_edit_input(&points, &difficulty, &sound, PAD_Y) ==
        OPTIONS_EDIT_CANCEL);
    assert(points == 5 && difficulty == DIFFICULTY_MEDIUM);
    /* Rows wrap through SOUND: Up from POINTS. */
    options_edit_input(&points, &difficulty, &sound, PAD_UP);
    options_edit_input(&points, &difficulty, &sound, PAD_UP);
    assert(g_options_edit.row == OPTIONS_ROW_SOUND);
    options_edit_input(&points, &difficulty, &sound, PAD_LEFT);
    assert(sound == AUDIO_OFF);
    options_edit_input(&points, &difficulty, &sound, PAD_UP);
    options_edit_input(&points, &difficulty, &sound, PAD_RIGHT);
    assert(options_edit_input(&points, &difficulty, &sound, PAD_B) ==
        OPTIONS_EDIT_DONE);
    assert(options_edit_changed(points, difficulty, sound));
}

static void test_select_and_styles(void)
{
    memset(g_host_sram, 0, sizeof(g_host_sram));
    save_initialize();
    g_save.settings.points = 11;
    g_save.settings.difficulty = DIFFICULTY_HARD;
    select_begin(MATCH_DOUBLES, 1);
    select_apply();
    assert(g_match_config.points_to_win == 11);
    assert(g_match_config.difficulty == DIFFICULTY_HARD);
    game_new_match();
    /* The humans' CPU partner plays as shipped; the opponents are harder. */
    assert(g_ai_styles[1].late_period ==
        g_ai_default_style.late_period);
    assert(g_ai_styles[2].late_period >
        g_opponent_styles[g_match_config.opponent][0].late_period);
}

/* First to the configured points; MATCH POINT follows it too. */
static void test_points_to_win(void)
{
    PlayerIntent intents[MAX_PLAYERS];
    u8 point;
    u16 n;

    memset(intents, 0, sizeof(intents));
    match_config_defaults();
    g_match_config.points_to_win = 11;
    g_attract = 0;
    game_new_match();
    for (point = 0; point < 11; ++point) {
        assert(g_game_state.phase != GAME_PHASE_MATCH_RESULT);
        g_game_state.phase = GAME_PHASE_LIVE;
        g_ball.rally_state = BALL_RALLY_LIVE;
        rules_resolve_point(1, GAME_FAULT_OUT);
        for (n = 0; n < 400 && g_game_state.phase == GAME_PHASE_POINT_RESULT;
                ++n)
            game_update(intents);
    }
    assert(g_game_state.phase == GAME_PHASE_MATCH_RESULT);
    assert(g_game_state.team_score[0] == 11);
    match_config_defaults();
}

/* Bot-vs-CPU wins at a difficulty (tools/match_stats.c, smaller). With
 * `kid_style`, P1 only moves: the KID assist does all the swinging. */
static int bot_wins(u8 difficulty, u8 late, u8 matches, u8 kid_style,
    int *p1_hits)
{
    PlayerIntent intents[MAX_PLAYERS];
    unsigned long frames = 0;
    u8 match = 0;
    u8 counted = 0;
    u8 serial = 0;
    int wins = 0;

    match_config_defaults();
    g_match_config.difficulty = difficulty;
    game_new_match();
    *p1_hits = 0;
    while (match < matches && frames < 60UL * 60 * 60) {
        test_bot_skill(late, match);
        memset(intents, 0, sizeof(intents));
        ai_update(0, &intents[0]);
        if (kid_style) {
            intents[0].hit_pressed = 0;
            if (ai_assist_swing(0))
                intents[0].hit_pressed = 1;
        }
        /* A KID P1 presses nothing at all: the serve goes by itself. */
        if (!kid_style && g_game_state.phase == GAME_PHASE_SERVE_SETUP &&
            g_serve_state.server_index == 0 && (frames & 31) == 0)
            intents[0].hit_pressed = 1;
        if (g_game_state.phase == GAME_PHASE_MATCH_RESULT) {
            intents[0].hit_pressed = 1;
            if (!counted) {
                counted = 1;
                ++match;
                wins += g_game_state.match_winner == 0;
            }
        } else {
            counted = 0;
        }
        test_step(intents);
        if (g_game_state.shot_serial != serial) {
            serial = g_game_state.shot_serial;
            *p1_hits += g_ball.last_hitter == 0;
        }
        ++frames;
    }
    match_config_defaults();
    return wins;
}

/* The slow ball: arcs unchanged, but it steps on only some frames. */
static void test_slow_ball(void)
{
    PlayerIntent intents[MAX_PLAYERS];
    u8 moves = 0;
    u8 f;
    fx x;

    memset(intents, 0, sizeof(intents));
    match_config_defaults();
    g_match_config.difficulty = DIFFICULTY_EASY;
    game_new_match();
    g_game_state.phase = GAME_PHASE_LIVE;
    g_ball.rally_state = BALL_RALLY_LIVE;
    g_ball.active = 1;
    g_ball.z_fp = TO_FP(20);
    ball_launch(&g_ball, 200, 130, 40);
    for (f = 0; f < 24; ++f) {
        x = g_ball.x_fp;
        game_update(intents);
        moves += g_ball.x_fp != x;
    }
    assert(moves == 18);                        /* 3 of every 4 frames */
    match_config_defaults();
}

static void test_difficulty_and_kid(void)
{
    int wins[DIFFICULTY_COUNT];
    int hits;
    int kid_hits;
    int kid_wins;
    u8 d;

    for (d = 0; d < DIFFICULTY_COUNT; ++d)
        wins[d] = bot_wins(d, 5, 16, 0, &hits);
    printf("options: bot (late 5) wins of 16 by difficulty: KID %d, EASY %d, "
        "MEDIUM %d, HARD %d\n", wins[0], wins[1], wins[2], wins[3]);
    assert(wins[DIFFICULTY_KID] >= wins[DIFFICULTY_EASY]);
    assert(wins[DIFFICULTY_EASY] > wins[DIFFICULTY_MEDIUM]);
    assert(wins[DIFFICULTY_MEDIUM] > wins[DIFFICULTY_HARD]);

    /* KID: a P1 that never presses a button still serves, returns the
     * ball, and wins. */
    kid_wins = bot_wins(DIFFICULTY_KID, 5, 8, 1, &kid_hits);
    printf("options: KID P1 that only moves (no buttons): %d hits, %d of 8 "
        "wins\n",
        kid_hits, kid_wins);
    assert(kid_hits > 40);
    assert(kid_wins >= 4);
}

int main(void)
{
    test_values();
    test_difficulty_styles();
    test_ball_pattern();
    test_editor();
    test_select_and_styles();
    test_points_to_win();
    test_slow_ball();
    test_difficulty_and_kid();
    printf("options host checks passed\n");
    return 0;
}
