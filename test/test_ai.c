#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ai.h"
#include "court.h"
#include "game.h"
#include "serve.h"
#include "test_support.h"

static PlayerIntent intents[MAX_PLAYERS];
static int verbose;

static void new_match(void)
{
    memset(intents, 0, sizeof(intents));
    players_initialize();
    ball_initialize();
    serve_initialize();
    ai_initialize();
    game_new_match();
}

/* One frame exactly as main.c runs it, with P1 driven by `p1`. */
static void frame(const PlayerIntent *p1)
{
    intents[0] = *p1;
    test_step(intents);
    if (verbose)
        printf("ph %d ball %d,%d,%d cpu %d,%d st %d hit %d bc %d\n",
            g_game_state.phase, FROM_FP(g_ball.x_fp), FROM_FP(g_ball.y_fp),
            FROM_FP(g_ball.z_fp), FROM_FP(g_players[2].x_fp),
            FROM_FP(g_players[2].y_fp), g_players[2].state,
            intents[2].hit_pressed, g_ball.bounce_count);
}

static void test_prediction_matches_simulation(void)
{
    Ball ball;
    Ball sim;
    s16 land_x;
    s16 land_y;
    u8 steps;
    u8 n;

    memset(&ball, 0, sizeof(ball));
    ball.active = 1;
    ball.rally_state = BALL_RALLY_LIVE;
    ball.x_fp = TO_FP(60);
    ball.y_fp = TO_FP(120);
    ball.z_fp = TO_FP(10);
    ball_launch(&ball, 200, 150, 34);
    steps = ai_predict_landing(&ball, &land_x, &land_y);
    assert(steps <= AI_PREDICT_MAX_STEPS);
    sim = ball;
    for (n = 0; n < steps; ++n)
        ball_simulate_step(&sim);
    assert(sim.bounce_count == 1);
    assert(FROM_FP(sim.x_fp) == land_x);
    assert(FROM_FP(sim.y_fp) == land_y);

    /* A ball that never comes down still terminates at the cap. */
    ball.vz_fp = 400;
    assert(ai_predict_landing(&ball, &land_x, &land_y) ==
        AI_PREDICT_MAX_STEPS);
}

/* The AI's lightweight probe must match ball_simulate_step exactly. */
static void test_probe_matches_ball(void)
{
    static const s16 targets[6][2] = {
        { 200, 104 }, { 228, 156 }, { 176, 130 }, { 28, 104 }, { 80, 156 },
        { 250, 180 }
    };
    Ball ball;
    u8 t;
    u8 n;

    for (t = 0; t < 6; ++t) {
        memset(&ball, 0, sizeof(ball));
        ball.active = 1;
        ball.rally_state = BALL_RALLY_LIVE;
        ball.x_fp = TO_FP(t < 3 ? 60 : 200);
        ball.y_fp = TO_FP(120);
        ball.z_fp = TO_FP(6 + t * 3);
        ball_launch(&ball, targets[t][0], targets[t][1], (u8)(26 + t * 4));
        ai_probe_load(&ball);
        for (n = 0; n < 160; ++n) {
            ball_simulate_step(&ball);
            ai_probe_step();
            assert(g_probe.x == ball.x_fp && g_probe.y == ball.y_fp);
            assert(g_probe.z == ball.z_fp && g_probe.vz == ball.vz_fp);
            assert(g_probe.vx == ball.vx_fp && g_probe.vy == ball.vy_fp);
            assert(g_probe.bounces == ball.bounce_count);
        }
    }
}

static void test_cpu_uses_shared_player_path_and_bounds(void)
{
    PlayerIntent idle;
    u16 n;

    memset(&idle, 0, sizeof(idle));
    new_match();
    assert(g_players[2].active && g_players[2].cpu_controlled);
    for (n = 0; n < 600; ++n) {
        frame(&idle);
        assert(g_players[2].x_fp >= TO_FP(PLAYER_RIGHT_CLAMP_MIN_X));
        assert(g_players[2].x_fp <= TO_FP(PLAYER_RIGHT_CLAMP_MAX_X));
        assert(g_players[2].y_fp >= TO_FP(PLAYER_CLAMP_MIN_Y));
        assert(g_players[2].y_fp <= TO_FP(PLAYER_CLAMP_MAX_Y));
    }
}

/* Serve with the given aim and report whether the CPU returned it. */
static u8 cpu_returns_serve(s16 aim_y)
{
    PlayerIntent p1;
    u16 n;

    new_match();
    memset(&p1, 0, sizeof(p1));
    p1.hit_pressed = 1;
    frame(&p1);
    p1.hit_pressed = 0;
    p1.aim_y = aim_y;
    for (n = 0; n < 400; ++n) {
        frame(&p1);
        if (g_ball.last_hitter == 2)
            return g_game_state.phase == GAME_PHASE_LIVE;
        if (g_game_state.phase == GAME_PHASE_POINT_RESULT)
            return 0;
    }
    return 0;
}

static void test_cpu_returns_standard_serves(void)
{
    static const s16 aims[10] = { 0, 0, 0, 0, -1, -1, 1, 1, 0, 0 };
    u8 returned = 0;
    u8 i;

    for (i = 0; i < 10; ++i)
        returned += cpu_returns_serve(aims[i]);
    printf("cpu returned %d/10 serves\n", returned);
    assert(returned >= 8);
}

int main(int argc, char **argv)
{
    (void)argv;
    verbose = argc > 1;
    if (verbose) {
        cpu_returns_serve(0);
        return 0;
    }
    test_prediction_matches_simulation();
    test_probe_matches_ball();
    test_cpu_uses_shared_player_path_and_bounds();
    test_cpu_returns_standard_serves();
    puts("ai host checks passed");
    return 0;
}
