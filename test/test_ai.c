#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ai.h"
#include "court.h"
#include "game.h"
#include "match.h"
#include "opponents.h"
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

/* The blocked landing prediction matches stepping one frame at a time
 * (steps and landing; the impact speed follows from the same steps), over
 * every height and climb a shot can have, and past the cap. */
static void test_predict_blocks_exact(void)
{
    Ball ball;
    s16 land_x;
    s16 land_y;
    fx z;
    s16 vz;
    fx sz;
    s16 svz;
    u8 steps;
    u8 want;

    memset(&ball, 0, sizeof(ball));
    ball.x_fp = TO_FP(60);
    ball.y_fp = TO_FP(120);
    ball.vx_fp = 37;
    ball.vy_fp = -11;
    for (z = 0; z <= TO_FP(64); z += 3) {
        for (vz = -120; vz <= 260; vz += 1) {
            ball.z_fp = z;
            ball.vz_fp = vz;
            steps = ai_predict_landing(&ball, &land_x, &land_y);
            if (z <= 0 && vz <= 0) {
                assert(steps == 0);
                continue;
            }
            sz = z;
            svz = vz;
            for (want = 0; want < AI_PREDICT_MAX_STEPS; ) {
                sz += svz;
                svz += BALL_GRAVITY_FP;
                ++want;
                if (sz <= 0)
                    break;
            }
            assert(steps == want);
            assert(land_x == FROM_FP(ball.x_fp + ball.vx_fp * want));
            assert(land_y == FROM_FP(ball.y_fp + ball.vy_fp * want));
        }
    }
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

/* A flat ball from P1 passing the CPU (at its kitchen-line spot) at
 * about 15 px, in a rally at the given bounce stage. Returns 1 if the CPU
 * hit it before it bounced, 0 if after, -1 if not at all; *fault is the
 * point's fault against the CPU's team, if any. */
static int volley_case(const AiStyle *style, u8 required, u8 serving,
    u8 *fault)
{
    PlayerIntent idle;
    u8 serial;
    u8 bounces;
    u16 n;

    memset(&idle, 0, sizeof(idle));
    match_config_defaults();
    new_match();
    g_ai_styles[2] = *style;
    g_game_state.phase = GAME_PHASE_LIVE;
    g_game_state.required_bounce = required;
    g_game_state.serving_team = serving;
    g_players[2].x_fp = TO_FP(COURT_NET_X + 44);
    g_players[2].y_fp = TO_FP(130);
    g_players[2].state = PLAYER_STATE_IDLE;
    g_players[0].x_fp = TO_FP(40);
    g_ball.active = 1;
    g_ball.rally_state = BALL_RALLY_LIVE;
    g_ball.last_hitter = 0;
    g_ball.bounce_count = 0;
    g_ball.x_fp = TO_FP(60);
    g_ball.y_fp = TO_FP(130);
    g_ball.z_fp = TO_FP(10);
    ball_launch(&g_ball, 230, 130, 24);
    ++g_game_state.shot_serial;
    serial = g_game_state.shot_serial;
    *fault = GAME_FAULT_NONE;
    for (n = 0; n < 120; ++n) {
        bounces = g_ball.bounce_count;
        frame(&idle);
        if (g_game_state.phase == GAME_PHASE_POINT_RESULT) {
            if (g_game_state.point_winner == 0)
                *fault = g_game_state.last_fault;
            return -1;
        }
        if (g_game_state.shot_serial != serial && g_ball.last_hitter == 2)
            return bounces == 0;
    }
    return -1;
}

/* ADR-035 net play: a volleying style volleys a ball passing at a good
 * height once the bounces are done, never before (no early-volley
 * faults), and the default CPU never volleys. */
static void test_net_play(void)
{
    const AiStyle *twins = &g_opponent_styles[6][0];
    u8 fault;
    int r;

    assert(twins->volley && twins->net == 44);
    r = volley_case(twins, GAME_REQUIRED_BOUNCE_NONE, 0, &fault);
    assert(r == 1);
    /* The return still has to bounce for the serving team. */
    r = volley_case(twins, GAME_REQUIRED_BOUNCE_RETURN, 1, &fault);
    assert(r != 1 && fault != GAME_FAULT_EARLY_VOLLEY);
    /* The default CPU lets it bounce. */
    r = volley_case(&g_ai_default_style, GAME_REQUIRED_BOUNCE_NONE, 0,
        &fault);
    assert(r != 1);
    /* Coming in: with the ball going away, home is the style's net spot;
     * while the serving team must still let the return bounce, it is not. */
    match_config_defaults();
    new_match();
    g_ai_styles[2] = *twins;
    g_game_state.phase = GAME_PHASE_LIVE;
    g_game_state.required_bounce = GAME_REQUIRED_BOUNCE_NONE;
    g_ball.last_hitter = 2;
    g_ball.rally_state = BALL_RALLY_LIVE;
    ai_update(2, &intents[2]);
    assert(g_ai_slots[2].target_x == COURT_NET_X + 44);
    g_game_state.required_bounce = GAME_REQUIRED_BOUNCE_RETURN;
    g_game_state.serving_team = 1;
    ai_update(2, &intents[2]);
    assert(g_ai_slots[2].target_x == COURT_NET_X + twins->recover);
    match_config_defaults();
}

/* Doubles, the right team incoming: a ball from slot 0 landing at
 * (x, y), with the left team placed; returns whether `slot` plans to play
 * it after its think. */
static u8 owns_landing(u8 slot, s16 x, s16 y)
{
    g_game_state.phase = GAME_PHASE_LIVE;
    g_game_state.required_bounce = GAME_REQUIRED_BOUNCE_NONE;
    g_ball.active = 1;
    g_ball.rally_state = BALL_RALLY_LIVE;
    g_ball.bounce_count = 0;
    g_ball.last_hitter = slot < 2 ? 2 : 0;
    g_ball.x_fp = TO_FP(slot < 2 ? 200 : 56);
    g_ball.y_fp = TO_FP(130);
    g_ball.z_fp = TO_FP(20);
    ball_launch(&g_ball, x, y, 36);
    ++g_game_state.shot_serial;
    g_ai_slots[slot].incoming_frames = 30;
    g_ai_slots[slot].plan_key = 0xff;
    ai_update(slot, &intents[slot]);
    return g_ai_slots[slot].has_target;
}

/* ADR-036 teamwork. */
static void test_teamwork(void)
{
    const AiStyle *pirates = &g_opponent_styles[3][0];

    assert(pirates->team && !g_ai_default_style.team);
    assert(g_ai_partner_style.team);
    /* Two CPUs with teamwork: a middle ball goes to the nearer one, even
     * across the centreline; without it, by halves. */
    match_config_doubles();
    match_config_opponent(3);
    new_match();
    g_players[2].x_fp = TO_FP(200);
    g_players[2].y_fp = TO_FP(100);         /* slot 2: top lane */
    g_players[3].x_fp = TO_FP(200);
    g_players[3].y_fp = TO_FP(160);
    /* Landing just above the centreline (slot 2's half), nearer slot 3. */
    g_players[3].y_fp = TO_FP(135);
    assert(owns_landing(3, 200, 126));
    assert(!owns_landing(2, 200, 126));
    g_ai_styles[2].team = 0;
    g_ai_styles[3].team = 0;
    assert(!owns_landing(3, 200, 126));
    assert(owns_landing(2, 200, 126));
    /* A human's partner (slot 1, bottom lane) keeps a middle ball in its
     * own half even with the human nearer, poaches one in the human's
     * half when clearly nearer, and leaves one deep in it. */
    match_config_doubles();
    new_match();
    g_players[0].x_fp = TO_FP(60);          /* slot 0: bottom lane */
    g_players[0].y_fp = TO_FP(134);
    g_players[1].x_fp = TO_FP(60);
    g_players[1].y_fp = TO_FP(100);         /* slot 1: top lane */
    assert(owns_landing(1, 60, 126));       /* its half; the human nearer */
    g_players[0].y_fp = TO_FP(165);
    g_players[1].y_fp = TO_FP(128);         /* clearly nearer the middle */
    assert(owns_landing(1, 60, 140));       /* the human's half: a poach */
    assert(!owns_landing(1, 60, 160));      /* deep in the human's half */
    /* The pair: the partner comes in only when the human is up. */
    g_ball.last_hitter = 0;
    g_ball.x_fp = TO_FP(200);
    g_game_state.required_bounce = GAME_REQUIRED_BOUNCE_NONE;
    g_players[0].x_fp = TO_FP(30);
    g_ai_slots[1].has_target = 0;
    ai_update(1, &intents[1]);
    assert(g_ai_slots[1].target_x == COURT_NET_X - g_ai_partner_style.recover);
    g_players[0].x_fp = TO_FP(COURT_NET_X - 50);
    ai_update(1, &intents[1]);
    assert(g_ai_slots[1].target_x == COURT_NET_X - g_ai_partner_style.net);
    /* The shift: home moves toward the ball's side across the net. */
    g_ball.y_fp = TO_FP(160);
    ai_update(1, &intents[1]);
    assert(g_ai_slots[1].target_y > AI_RECOVER_Y_TOP);
    g_ball.y_fp = TO_FP(100);
    ai_update(1, &intents[1]);
    assert(g_ai_slots[1].target_y < AI_RECOVER_Y_TOP);
    match_config_defaults();
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
    test_predict_blocks_exact();
    test_probe_matches_ball();
    test_cpu_uses_shared_player_path_and_bounds();
    test_cpu_returns_standard_serves();
    test_net_play();
    test_teamwork();
    puts("ai host checks passed");
    return 0;
}
