#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ai.h"
#include "collision.h"
#include "court.h"
#include "game.h"
#include "match.h"
#include "options.h"
#include "practice.h"
#include "save.h"
#include "test_support.h"

/* ADR-028 practice: settings, feeds that land where asked, streaks. */

static PlayerIntent intents[MAX_PLAYERS];

static void start(u8 shot, u8 target, u8 rate, u8 difficulty)
{
    memset(g_host_sram, 0, sizeof(g_host_sram));
    save_initialize();
    g_save.settings.practice[PRACTICE_ROW_SHOT] = shot;
    g_save.settings.practice[PRACTICE_ROW_TARGET] = target;
    g_save.settings.practice[PRACTICE_ROW_RATE] = rate;
    match_config_defaults();
    match_config_practice();
    g_match_config.difficulty = difficulty;
    g_attract = 0;
    game_new_match();
}

static void test_settings(void)
{
    u8 values[PRACTICE_ROWS] = { 9, 9, 9 };

    assert(practice_shot(9) == PRACTICE_SHOT_MIX);
    assert(strcmp(practice_target_name(PRACTICE_TARGET_AT_YOU), "AT YOU") == 0);
    practice_edit_begin(values);
    assert(values[0] == PRACTICE_SHOT_MIX &&
        values[1] == PRACTICE_TARGET_RANDOM &&
        values[2] == PRACTICE_RATE_NORMAL);
    practice_edit_input(values, PAD_RIGHT);
    assert(values[0] == PRACTICE_SHOT_DRIVE);           /* wraps */
    practice_edit_input(values, PAD_UP);
    assert(g_practice_edit.row == PRACTICE_ROW_RATE);
    practice_edit_input(values, PAD_LEFT);
    assert(values[2] == PRACTICE_RATE_SLOW);
    assert(practice_edit_changed(values));
    assert(practice_edit_input(values, PAD_Y) == PRACTICE_EDIT_CANCEL);
    assert(!practice_edit_changed(values));
    assert(practice_edit_input(values, PAD_B) == PRACTICE_EDIT_DONE);
}

/* Every shot to every target: the machine's ball clears the net and first
 * bounces in on P1's side, where asked (no one swings). */
static void test_feeds_land(void)
{
    u8 shot;
    u8 target;
    u16 f;
    u8 feeds;

    for (shot = 0; shot < PRACTICE_SHOTS; ++shot)
        for (target = 0; target < PRACTICE_TARGETS; ++target) {
            start(shot, target, PRACTICE_RATE_FAST, DIFFICULTY_MEDIUM);
            memset(intents, 0, sizeof(intents));
            assert(!g_players[2].active);
            for (feeds = 0; feeds < 6; ++feeds) {
                for (f = 0; f < 400 && g_practice.state == PRACTICE_WAIT; ++f)
                    game_update(intents);
                assert(g_practice.state == PRACTICE_FEED);
                for (f = 0; f < 200 && g_ball.bounce_count == 0; ++f)
                    game_update(intents);
                assert(g_ball.bounce_count == 1);
                assert(court_x_on_side(g_ball.x_fp, 0));
                assert(court_contains_ground_fp(g_ball.x_fp, g_ball.y_fp));
                if (target == PRACTICE_TARGET_KITCHEN ||
                        shot == PRACTICE_SHOT_DINK)
                    assert(g_ball.x_fp > TO_FP(COURT_KITCHEN_LEFT_X - 8));
                if (target == PRACTICE_TARGET_DEEP &&
                        shot != PRACTICE_SHOT_DINK)
                    assert(g_ball.x_fp < TO_FP(COURT_BASELINE_LEFT + 40));
                /* Unreturned, it bounces twice and the streak stays 0. */
                for (f = 0; f < 300 && g_practice.state != PRACTICE_WAIT; ++f)
                    game_update(intents);
                assert(g_practice.state == PRACTICE_WAIT);
                assert(g_practice.streak == 0);
            }
        }
}

/* The bot P1 (the CPU's own AI on slot 0) returns feeds: streaks build,
 * the best is kept and saved on leaving, and the score panels show them. */
static int run_bot(u8 shot, u8 target, u8 difficulty, u8 kid, u16 frames)
{
    u16 f;

    start(shot, target, PRACTICE_RATE_FAST, difficulty);
    for (f = 0; f < frames; ++f) {
        test_bot_skill(9, 0);
        memset(intents, 0, sizeof(intents));
        ai_update(0, &intents[0]);
        if (kid) {
            intents[0].hit_pressed = 0;
            if (ai_assist_swing(0))
                intents[0].hit_pressed = 1;
        }
        game_update(intents);
    }
    return g_practice.best;
}

static void test_streaks(void)
{
    int best = run_bot(PRACTICE_SHOT_MIX, PRACTICE_TARGET_RANDOM,
        DIFFICULTY_MEDIUM, 0, 6000);
    int kid_best;

    printf("practice: bot best streak (MIX, RANDOM) %d in 100 s\n", best);
    assert(best >= 3);
    assert(g_game_state.team_score[1] == g_practice.best);
    assert(g_game_state.team_score[0] == g_practice.streak);

    kid_best = run_bot(PRACTICE_SHOT_DINK, PRACTICE_TARGET_KITCHEN,
        DIFFICULTY_KID, 1, 6000);
    printf("practice: KID assist only (DINK, KITCHEN) best streak %d\n",
        kid_best);
    assert(kid_best >= 3);

    /* Leaving saves a new best; a lower one does not overwrite it. */
    practice_finish();
    assert(g_save.records.practice_best == (u16)kid_best);
    save_initialize();
    assert(g_save.records.practice_best == (u16)kid_best);
    g_practice.best = 1;
    practice_finish();
    assert(g_save.records.practice_best == (u16)kid_best);
}

/* The nozzle shows where each feed goes: it turns one pose at a time
 * toward the next lane during the wait, points there when the ball leaves
 * (up for the far sideline, down for the near one), and kicks back. */
static void test_machine_aim(void)
{
    u8 seen[3] = { 0, 0, 0 };
    u8 pose;
    u16 f;
    u8 feeds;

    start(PRACTICE_SHOT_MIX, PRACTICE_TARGET_RANDOM, PRACTICE_RATE_FAST,
        DIFFICULTY_MEDIUM);
    memset(intents, 0, sizeof(intents));
    pose = g_practice.pose;
    for (feeds = 0; feeds < 30; ++feeds) {
        for (f = 0; f < 400 && g_practice.state != PRACTICE_FEED; ++f) {
            game_update(intents);
            assert(g_practice.pose <= PRACTICE_AIM_DOWN);
            assert(g_practice.pose + 1 >= pose && g_practice.pose <= pose + 1);
            pose = g_practice.pose;
        }
        assert(g_practice.state == PRACTICE_FEED);
        assert(g_practice.pose == g_practice.aim);
        assert(g_practice.kick == PRACTICE_KICK_FRAMES);
        if (g_practice.aim == PRACTICE_AIM_UP)
            assert(g_ball.vy_fp < 0);
        else if (g_practice.aim == PRACTICE_AIM_DOWN)
            assert(g_ball.vy_fp > 0);
        else
            assert(g_practice.next_ty >= PRACTICE_MACHINE_Y -
                PRACTICE_AIM_SPREAD &&
                g_practice.next_ty <= PRACTICE_MACHINE_Y + PRACTICE_AIM_SPREAD);
        seen[g_practice.aim] = 1;
        /* Unreturned, it ends and the machine turns toward the next. */
        for (f = 0; f < 300 && g_practice.state != PRACTICE_WAIT; ++f)
            game_update(intents);
        pose = g_practice.pose;
    }
    assert(seen[0] && seen[1] && seen[2]);

    /* AT YOU follows P1: stand by the far sideline and it turns up. */
    start(PRACTICE_SHOT_LOB, PRACTICE_TARGET_AT_YOU, PRACTICE_RATE_SLOW,
        DIFFICULTY_MEDIUM);
    g_players[0].y_fp = TO_FP(HIT_TARGET_LANE_TOP);
    for (f = 0; f < 400 && g_practice.state != PRACTICE_FEED; ++f)
        game_update(intents);
    assert(g_practice.aim == PRACTICE_AIM_UP);
    assert(g_practice.pose == PRACTICE_AIM_UP);
    assert(g_ball.vy_fp < 0);
}

/* Leaving through the pause menu's MAIN MENU (ADR-030) saves the best:
 * main.c calls practice_finish, then returns to the title. */
static void test_exit(void)
{
    start(PRACTICE_SHOT_DRIVE, PRACTICE_TARGET_AT_YOU, PRACTICE_RATE_FAST,
        DIFFICULTY_MEDIUM);
    g_practice.best = 7;
    practice_finish();
    game_start_attract();
    assert(g_attract);
    assert(!g_match_config.practice);
    assert(g_save.records.practice_best == 7);
    g_attract = 0;
    match_config_defaults();
}

int main(void)
{
    test_settings();
    test_feeds_land();
    test_streaks();
    test_machine_aim();
    test_exit();
    printf("practice host checks passed\n");
    return 0;
}
