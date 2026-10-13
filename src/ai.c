#include "ai.h"
#include "collision.h"
#include "court.h"
#include "game.h"
#include "serve.h"

AiState g_ai;

static s16 s_impact_vz;

void ai_initialize(void)
{
    g_ai.has_target = 0;
    g_ai.target_x = AI_RECOVER_X;
    g_ai.target_y = AI_RECOVER_Y;
    g_ai.predicted_land_x = 0;
    g_ai.predicted_land_y = 0;
    g_ai.predicted_steps = 0;
    g_ai.shot_counter = 0;
    g_ai.plan_key = 0xff;
}

/*
 * Ground contact prediction using the ball's own constants. x/y are linear
 * until the bounce, so only z is stepped; the loop is capped so a bad state
 * can never hang the frame.
 */
u8 ai_predict_landing(const Ball *ball, s16 *land_x, s16 *land_y)
{
    fx z = ball->z_fp;
    s16 vz = ball->vz_fp;
    u8 steps = 0;

    s_impact_vz = 0;
    if (z <= 0 && vz <= 0) {
        *land_x = FROM_FP(ball->x_fp);
        *land_y = FROM_FP(ball->y_fp);
        return 0;
    }
    while (steps < AI_PREDICT_MAX_STEPS) {
        z += vz;
        vz += BALL_GRAVITY_FP;
        ++steps;
        if (z <= 0)
            break;
    }
    s_impact_vz = vz - BALL_GRAVITY_FP;
    *land_x = FROM_FP(ball->x_fp + ball->vx_fp * steps);
    *land_y = FROM_FP(ball->y_fp + ball->vy_fp * steps);
    return steps;
}

static s16 move_toward(s16 from, s16 to)
{
    s16 delta = to - from;
    if (delta > AI_DEAD_ZONE)
        return 1;
    if (delta < -AI_DEAD_ZONE)
        return -1;
    return 0;
}

static u8 ball_on_team_side(fx x_fp, u8 team)
{
    if (team == 0)
        return x_fp < TO_FP(COURT_NET_X);
    return x_fp > TO_FP(COURT_NET_X);
}

static u8 ball_incoming(const Player *player)
{
    if (g_ball.rally_state != BALL_RALLY_LIVE)
        return 0;
    if (g_ball.last_hitter < MAX_PLAYERS &&
        g_players[g_ball.last_hitter].team == player->team)
        return 0;
    return 1;
}

/* Aim: away from the opponent's y, mostly deep, a drop shot every 4th. */
static void ai_choose_aim(const Player *player, PlayerIntent *intent)
{
    const Player *opponent = &g_players[0];
    s16 toward_opponent = player->team ? -1 : 1;

    intent->move_y = (opponent->y_fp < TO_FP(COURT_CENTERLINE_Y)) ? 1 : -1;
    if ((g_ai.shot_counter & 3) == 3)
        intent->move_x = -toward_opponent;
    else if (g_ai.shot_counter & 1)
        intent->move_x = toward_opponent;
    else
        intent->move_x = 0;
}

/* Would a swing started now meet the ball during its active frames? */
static u8 ai_swing_connects(const Player *player)
{
    Ball probe;
    u8 step;
    s16 dx = FROM_FP(g_ball.x_fp) - FROM_FP(player->x_fp);

    /* The ball moves < 8 px/frame, so beyond this it cannot arrive in time. */
    if (dx > 64 || dx < -64)
        return 0;
    probe = g_ball;

    /* Pressed now, the swing is ACTIVE on frames +4..+6, and each of those
     * frames' collision check sees the ball after that many updates. */
    for (step = 1; step < PLAYER_WINDUP_FRAMES + PLAYER_ACTIVE_FRAMES;
            ++step) {
        ball_simulate_step(&probe);
        if (step < PLAYER_WINDUP_FRAMES)
            continue;
        /* Let every incoming ball bounce once: legal and readable. */
        if (probe.bounce_count == 0)
            continue;
        if (!ball_on_team_side(probe.x_fp, player->team))
            continue;
        if (collision_ball_in_player_reach(player, &probe))
            return 1;
    }
    return 0;
}

static void ai_plan_intercept(const Player *player)
{
    s16 land_x;
    s16 land_y;
    s16 bounce_vz;
    s16 vx;
    s16 vy;
    u8 rise;
    s16 behind = player->team ? AI_CONTACT_OFFSET_X : -AI_CONTACT_OFFSET_X;

    if (g_ball.bounce_count > 0 &&
        ball_on_team_side(g_ball.x_fp, player->team)) {
        /* Already bounced on our side: chase where it is heading. */
        g_ai.target_x = FROM_FP(g_ball.x_fp + g_ball.vx_fp * 6) + behind;
        g_ai.target_y = FROM_FP(g_ball.y_fp + g_ball.vy_fp * 6);
        g_ai.has_target = 1;
        return;
    }

    g_ai.predicted_steps = ai_predict_landing(&g_ball, &land_x, &land_y);
    g_ai.predicted_land_x = land_x;
    g_ai.predicted_land_y = land_y;

    if (!court_contains_ground_fp(TO_FP(land_x), TO_FP(land_y)) ||
        !ball_on_team_side(TO_FP(land_x), player->team)) {
        /* Out (or short of the net): let it go and hold the middle. */
        g_ai.has_target = 0;
        return;
    }

    /* Meet the ball near the top of its first bounce. */
    bounce_vz = ball_retain(-s_impact_vz, BALL_BOUNCE_VERTICAL_256);
    rise = (u8)(bounce_vz / -BALL_GRAVITY_FP);
    if (rise > 10)
        rise = 10;
    vx = ball_retain(g_ball.vx_fp, BALL_BOUNCE_PLANAR_256);
    vy = ball_retain(g_ball.vy_fp, BALL_BOUNCE_PLANAR_256);
    g_ai.target_x = land_x + FROM_FP(vx * rise) + behind;
    g_ai.target_y = land_y + FROM_FP(vy * rise);
    g_ai.has_target = 1;
}

void ai_update(u8 player_index, PlayerIntent *intent)
{
    Player *player = &g_players[player_index];
    s16 x;
    s16 y;
    u8 key;

    intent->move_x = 0;
    intent->move_y = 0;
    intent->hit_pressed = 0;
    intent->pause_pressed = 0;

    if (!player->active || g_game_state.paused)
        return;

    if (g_game_state.phase == GAME_PHASE_SERVE_SETUP ||
        g_game_state.phase == GAME_PHASE_SERVE_TOSS) {
        if (g_serve_state.server_index != player_index)
            return;
        ai_choose_aim(player, intent);
        if (g_game_state.phase == GAME_PHASE_SERVE_SETUP &&
            g_game_state.phase_timer >= AI_SERVE_DELAY_FRAMES) {
            intent->hit_pressed = 1;
            ++g_ai.shot_counter;
        }
        return;
    }

    if (g_game_state.phase != GAME_PHASE_LIVE)
        return;

    x = FROM_FP(player->x_fp);
    y = FROM_FP(player->y_fp);

    if (player->state == PLAYER_STATE_ACTIVE) {
        ai_choose_aim(player, intent);
        return;
    }

    if (ball_incoming(player)) {
        /* Replan only when a hit or bounce changed the trajectory, or while
         * chasing a bounced ball (cheap linear lookahead). */
        key = (u8)((g_ball.last_hitter << 4) | (g_ball.bounce_count & 15));
        if (key != g_ai.plan_key || g_ball.bounce_count > 0) {
            ai_plan_intercept(player);
            g_ai.plan_key = key;
        }
        if (!player_is_swinging(player) && ai_swing_connects(player)) {
            intent->hit_pressed = 1;
            ++g_ai.shot_counter;
            return;
        }
    } else {
        g_ai.has_target = 0;
        g_ai.plan_key = 0xff;
    }

    if (!g_ai.has_target) {
        g_ai.target_x = AI_RECOVER_X;
        g_ai.target_y = AI_RECOVER_Y;
    }
    intent->move_x = move_toward(x, g_ai.target_x);
    intent->move_y = move_toward(y, g_ai.target_y);
}
