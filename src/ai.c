#include <string.h>
#include "ram_copy.h"
#include "ai.h"
#include "profile.h"
#include "collision.h"
#include "court.h"
#include "match.h"
#include "modifiers.h"
#include "options.h"
#include "game.h"
#include "input.h"
#include "serve.h"

AiState g_ai;
AiState g_ai_slots[MAX_PLAYERS];
AiStyle g_ai_styles[MAX_PLAYERS];
/* Style of the slot being updated (set by ai_update). */
static const AiStyle *s_style;

/* Slot being updated, and whether it shares its half with an active
 * teammate (doubles lanes, ADR-018). Set by ai_update. */
static u8 s_slot;
static u8 s_has_partner;
/* The teammate is a human: never swing at a ball in their lane. */
static u8 s_partner_human;

static s16 s_impact_vz;

void ai_initialize(void)
{
    u8 i;

    memset(&g_ai, 0, sizeof(g_ai));
    g_ai.plan_key = 0xff;
    for (i = 0; i < MAX_PLAYERS; ++i) {
        memcpy(&g_ai_slots[i], &g_ai, sizeof(g_ai));
        g_ai_styles[i] = g_ai_default_style;
    }
    s_style = &g_ai_styles[0];
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

static u8 ball_incoming(const Player *player)
{
    if (g_ball.rally_state != BALL_RALLY_LIVE)
        return 0;
    /* Team from the slot (slots 2-3 are the right team), not
     * g_players[last_hitter].team, which costs 816-tcc a multiply. */
    if (g_ball.last_hitter < MAX_PLAYERS &&
        (g_ball.last_hitter >= 2) == player->team)
        return 0;
    return 1;
}

/* Doubles: does this point (y) belong to our lane? Singles: always. */
static u8 ai_owns_y(s16 y)
{
    if (!s_has_partner)
        return 1;
    return (y < COURT_CENTERLINE_Y) == match_slot_lane_top(s_slot);
}

static s16 abs_s16(s16 value)
{
    return value < 0 ? -value : value;
}

/* Nonzero when an active opponent stands within `distance` of the net. */
static u8 ai_opponent_at_net(u8 team, s16 distance)
{
    const Player *a = &g_players[team ? 0 : 2];
    const Player *b = a + 1;

    if (a->active && abs_s16(FROM_FP(a->x_fp) - COURT_NET_X) < distance)
        return 1;
    return b->active && abs_s16(FROM_FP(b->x_fp) - COURT_NET_X) < distance;
}

/*
 * Shot choice at the swing press (ADR-019), deterministic: put-away drive
 * on a high ball near the net, lob over an opponent at the net, a dink as
 * every Nth shot, otherwise a standard shot that alternates deep and
 * neutral (or is always deep). The thresholds and periods are the slot's
 * style (ADR-023); the default style is 14 px / 48 px, 44 px, every 4th.
 * contact_z is the ball height at contact.
 */
static void ai_plan_shot(const Player *player, fx contact_z)
{
    u8 n = g_ai.shot_counter;
    u8 dink = s_style->dink_period;
    s16 to_net = abs_s16(FROM_FP(player->x_fp) - COURT_NET_X);

    if (contact_z >= TO_FP(s_style->putaway_z) &&
            to_net < s_style->putaway_distance) {
        g_ai.planned_shot = SHOT_DRIVE;
        g_ai.planned_depth = 0;
    } else if (s_style->lob_distance &&
            ai_opponent_at_net(player->team, s_style->lob_distance)) {
        g_ai.planned_shot = SHOT_SOFT;
        g_ai.planned_depth = 1;
    } else if (dink && (n % dink) == dink - 1) {
        g_ai.planned_shot = SHOT_SOFT;
        g_ai.planned_depth = -1;
    } else {
        g_ai.planned_shot = SHOT_STANDARD;
        g_ai.planned_depth = (s_style->depth == AI_DEPTH_DEEP || (n & 1)) ?
            1 : 0;
    }
}

/* Aim (singles, by style): away from the opponent's y, at them, or a mixed
 * pattern; depth from the planned shot. */
static void ai_choose_aim(const Player *player, PlayerIntent *intent)
{
    const Player *opponent = &g_players[player->team ? 0 : 2];

    /* First active player on the other team (slots 0-1 left, 2-3 right). */
    if (!opponent->active)
        ++opponent;
    if (s_has_partner)
        /* Doubles: both opponents get the ball. Aiming away from the first
         * one would starve slot 0 (the human). Lanes go bottom, top, top,
         * bottom by shot count. */
        intent->aim_y = ((g_ai.shot_counter ^ (g_ai.shot_counter >> 1)) & 1) ?
            -1 : 1;
    else if (s_style->aim == AI_AIM_MIX)
        intent->aim_y = ((g_ai.shot_counter ^ (g_ai.shot_counter >> 2)) & 1) ?
            -1 : 1;
    else {
        intent->aim_y = (opponent->y_fp < TO_FP(COURT_CENTERLINE_Y)) ? 1 : -1;
        if (s_style->aim == AI_AIM_BODY)
            intent->aim_y = -intent->aim_y;
    }
    intent->aim_x = g_ai.planned_depth * court_toward_net(player->team);
    intent->shot = g_ai.planned_shot;
}

/*
 * Swing decision. Pressed now, the swing is ACTIVE on frames +4..+6 and each
 * of those frames' collision check sees the ball after that many updates;
 * contact happens on the first of them with the ball in reach. Swing when
 * that first contact is in the sweet spot, or when it is the last chance
 * (the ball is out of reach one frame after the window).
 */
/*
 * Swing lookahead probe: the ball's flight and bounce from
 * ball_simulate_step() on static fields, without the net/line event checks
 * the lookahead does not need. test_ai.c checks it against the real step
 * frame by frame.
 */
AiProbe g_probe;

void ai_probe_load(const Ball *ball)
{
    g_probe.x = ball->x_fp;
    g_probe.y = ball->y_fp;
    g_probe.z = ball->z_fp;
    g_probe.vx = ball->vx_fp;
    g_probe.vy = ball->vy_fp;
    g_probe.vz = ball->vz_fp;
    g_probe.bounces = ball->bounce_count;
}

void ai_probe_step(void)
{
    fx previous_z;
    s16 impact_vz;

    if (g_probe.z == 0 && g_probe.vz <= 0) {
        g_probe.vx = 0;
        g_probe.vy = 0;
        g_probe.vz = 0;
        return;
    }
    previous_z = g_probe.z;
    g_probe.x += g_probe.vx;
    g_probe.y += g_probe.vy;
    g_probe.z += g_probe.vz;
    g_probe.vz += BALL_GRAVITY_FP;
    if (previous_z > 0 && g_probe.z <= 0) {
        impact_vz = g_probe.vz - BALL_GRAVITY_FP;
        g_probe.z = 0;
        g_probe.vx = ball_retain(g_probe.vx, BALL_BOUNCE_PLANAR_256);
        g_probe.vy = ball_retain(g_probe.vy, BALL_BOUNCE_PLANAR_256);
        g_probe.vz = -ball_retain(impact_vz, BALL_BOUNCE_VERTICAL_256);
        ++g_probe.bounces;
        if (g_probe.vz <= BALL_MIN_BOUNCE_VZ_FP) {
            g_probe.vx = 0;
            g_probe.vy = 0;
            g_probe.vz = 0;
        }
    }
}

static fx s_contact_z;   /* ball height at the planned contact */
static fx s_reach_x;
static fx s_reach_y;

/* collision_ball_in_player_reach / _sweet_spot on the probe. */
static u8 probe_in_reach(void)
{
    if (!HIT_WITHIN(g_probe.x - s_reach_x, g_hit_box.reach_x,
            g_hit_box.reach_x_span))
        return 0;
    if (!HIT_WITHIN(g_probe.y - s_reach_y, g_hit_box.reach_y,
            g_hit_box.reach_y_span))
        return 0;
    /* 0 <= z <= reach: one unsigned test. */
    return (u16)g_probe.z <= (u16)g_hit_box.reach_z;
}

static u8 probe_in_sweet_spot(void)
{
    if (!HIT_WITHIN(g_probe.x - s_reach_x, g_hit_box.sweet_x,
            g_hit_box.sweet_x_span))
        return 0;
    return HIT_WITHIN(g_probe.y - s_reach_y, g_hit_box.sweet_y,
        g_hit_box.sweet_y_span);
}

static u8 ai_swing_connects(const Player *player)
{
    u8 step;
    u8 n;
    s16 dx = FROM_FP(g_ball.x_fp) - FROM_FP(player->x_fp);

    /* The ball moves < 8 px/frame, so beyond this it cannot arrive in time. */
    if (dx > 64 || dx < -64)
        return 0;
    /* Contact needs a bounced ball; skip the lookahead while the bounce is
     * still beyond the swing window (the common case, and the costly one). */
    if (g_ball.bounce_count == 0 &&
        g_ai.predicted_frames > g_ai.plan_age + PLAYER_WINDUP_FRAMES +
            PLAYER_ACTIVE_FRAMES)
        return 0;
    if (!g_ball.active || g_ball.rally_state != BALL_RALLY_LIVE)
        return 0;
    ai_probe_load(&g_ball);
    s_reach_x = player->x_fp;
    s_reach_y = player->y_fp;
    /* Probe step n is the ball at frame n's contact check: it moved on
     * frames 0..n-1 (a slow ball skips some, ADR-025; a fast one takes
     * extra steps, ADR-033). At a steady pace the pace check is skipped:
     * two calls per step cost 816-tcc real frame time. */
    for (step = 1; step <= PLAYER_WINDUP_FRAMES + PLAYER_ACTIVE_FRAMES;
            ++step) {
        if (!g_ball_paced)
            ai_probe_step();
        else
            for (n = GAME_BALL_STEPS_AHEAD(step - 1); n; --n)
                ai_probe_step();
        if (step < PLAYER_WINDUP_FRAMES)
            continue;
        if (!probe_in_reach())
            continue;
        /* This frame would be the contact. Let every incoming ball bounce
         * once first (legal and readable), and never reach over the net. */
        s_contact_z = g_probe.z;
        if (g_probe.bounces == 0 ||
            !court_x_on_side(g_probe.x, player->team))
            return 0;
        /* A human partner's lane is theirs, even when we could reach. */
        if (s_partner_human && !ai_owns_y(FROM_FP(g_probe.y)))
            return 0;
        if (step == PLAYER_WINDUP_FRAMES + PLAYER_ACTIVE_FRAMES)
            return 0;       /* first reachable after the window: wait */
        if (probe_in_sweet_spot())
            return !g_ai.late_this_ball;   /* a late player waits it out */
        /* First contact would be stretched; take it only as a last chance. */
        while (step < PLAYER_WINDUP_FRAMES + PLAYER_ACTIVE_FRAMES) {
            if (!g_ball_paced)
                ai_probe_step();
            else
                for (n = GAME_BALL_STEPS_AHEAD(step); n; --n)
                    ai_probe_step();
            ++step;
        }
        return !probe_in_reach();
    }
    return 0;
}

/* LAVA (ADR-033): an intercept never asks a CPU into its own kitchen (the
 * home targets are behind it already). Tested inline: no call without it. */
#define AI_AVOID_LAVA(player) \
    do { \
        if (g_match_config.modifiers & MODIFIER_LAVA) \
            g_ai.target_x = modifiers_lava_target(g_ai.target_x, \
                (player)->team); \
    } while (0)

static void ai_plan_intercept(const Player *player)
{
    u8 steps;
    s16 land_x;
    s16 land_y;
    s16 bounce_vz;
    s16 vx;
    s16 vy;
    u8 rise;
    s16 behind = -court_toward_net(player->team) * AI_CONTACT_OFFSET_X;

    if (g_ball.bounce_count > 0 &&
        court_x_on_side(g_ball.x_fp, player->team)) {
        /* Already bounced on our side: chase where it is heading. */
        g_ai.target_x = FROM_FP(g_ball.x_fp + g_ball.vx_fp * 6) + behind;
        g_ai.target_y = FROM_FP(g_ball.y_fp + g_ball.vy_fp * 6);
        g_ai.has_target = ai_owns_y(FROM_FP(g_ball.y_fp));
        AI_AVOID_LAVA(player);
        return;
    }

    steps = ai_predict_landing(&g_ball, &land_x, &land_y);
    /* FAST BALL takes an extra step every 3rd frame, so N steps take at
     * least N - N/4 - 1 frames: the swing early-out stays safe. A slow ball
     * (ADR-025) only takes longer, so its steps are a safe count. */
    if ((g_match_config.modifiers & MODIFIER_FAST_BALL) && steps > 1)
        steps -= (steps >> 2) + 1;
    g_ai.predicted_frames = steps;
    g_ai.predicted_land_x = land_x;
    g_ai.predicted_land_y = land_y;

    if (!court_contains_ground_fp(TO_FP(land_x), TO_FP(land_y)) ||
        !court_x_on_side(TO_FP(land_x), player->team)) {
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
    AI_AVOID_LAVA(player);
    /* Doubles: only the teammate whose lane holds the landing plays it. */
    g_ai.has_target = ai_owns_y(land_y);
}

static void ai_think(u8 player_index, PlayerIntent *intent);
static PlayerIntent s_held_intent[MAX_PLAYERS];

/*
 * KID swing assist (ADR-025): nonzero when a swing pressed now would meet
 * the ball, by the CPU's own lookahead. It never swings at a ball that
 * must bounce first (the lookahead waits for the bounce) and never lets a
 * contact sail past: the human only has to be in the ball's path.
 */
u8 ai_assist_swing(u8 player_index)
{
    const Player *player = &g_players[player_index];
    u8 connects;

    if (g_game_state.phase == GAME_PHASE_SERVE_SETUP)
        return g_serve_state.server_index == player_index &&
            g_game_state.phase_timer >= AI_ASSIST_SERVE_FRAMES;
    if (g_game_state.phase != GAME_PHASE_LIVE || player_is_swinging(player) ||
            !ball_incoming(player))
        return 0;
    RAM_COPY(&g_ai, &g_ai_slots[player_index], sizeof(g_ai));
    g_ai.late_this_ball = 0;
    g_ai.predicted_frames = 0;      /* no early-out on a stale plan */
    s_slot = player_index;
    s_has_partner = 0;
    s_partner_human = 0;
    connects = ai_swing_connects(player);
    return connects;
}

/* Attract demo budget: the demo P1 thinks every other frame and repeats its
 * last movement in between (the one-shot swing press is not repeated). */
void ai_hold_intent(u8 player_index, PlayerIntent *intent)
{
    *intent = s_held_intent[player_index];
    intent->hit_pressed = 0;
}

void ai_update(u8 player_index, PlayerIntent *intent)
{
    s_slot = player_index;
    /* Constant indices (816-tcc multiplies for g_players[variable]). */
    if (player_index == 0) {
        s_has_partner = g_players[1].active;
        s_style = &g_ai_styles[0];
    } else if (player_index == 1) {
        s_has_partner = g_players[0].active;
        s_style = &g_ai_styles[1];
    } else if (player_index == 2) {
        s_has_partner = g_players[3].active;
        s_style = &g_ai_styles[2];
    } else {
        s_has_partner = g_players[2].active;
        s_style = &g_ai_styles[3];
    }
    s_partner_human = s_has_partner &&
        g_input_owners[player_index ^ 1] == INPUT_OWNER_HUMAN;
    RAM_COPY(&g_ai, &g_ai_slots[player_index], sizeof(g_ai));
    PROFILE_MARK(8);
    ai_think(player_index, intent);
    PROFILE_MARK(11);
    RAM_COPY(&g_ai_slots[player_index], &g_ai, sizeof(g_ai));
    s_held_intent[player_index] = *intent;
}

static void ai_think(u8 player_index, PlayerIntent *intent)
{
    Player *player = &g_players[player_index];
    s16 x;
    s16 y;
    u8 key;
    u8 incoming;

    intent->move_x = 0;
    intent->move_y = 0;
    intent->aim_x = 0;
    intent->aim_y = 0;
    intent->hit_pressed = 0;
    intent->pause_pressed = 0;

    if (!player->active || g_game_state.paused)
        return;

    if (g_game_state.phase == GAME_PHASE_SERVE_SETUP ||
        g_game_state.phase == GAME_PHASE_SERVE_TOSS) {
        if (g_serve_state.server_index != player_index)
            return;
        /* Serves keep the original depth pattern: a short serve every 4th,
         * otherwise alternating deep and neutral. */
        g_ai.planned_shot = SHOT_STANDARD;
        if ((g_ai.shot_counter & 3) == 3)
            g_ai.planned_depth = -1;
        else
            g_ai.planned_depth = (g_ai.shot_counter & 1) ? 1 : 0;
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

    /* Committed to a swing: plant the feet (the swing check assumed the
     * current position) and hold the aim through the active frames. */
    if (player_is_swinging(player)) {
        ai_choose_aim(player, intent);
        return;
    }

    PROFILE_MARK(9);
    incoming = ball_incoming(player);
    if (!incoming)
        g_ai.incoming_frames = 0;
    else if (g_ai.incoming_frames < 255)
        ++g_ai.incoming_frames;

    if (incoming && g_ai.incoming_frames > g_ai.reaction_frames) {
        /* Replan only when a hit or bounce changed the trajectory, or while
         * chasing a bounced ball (cheap linear lookahead). */
        key = (u8)((g_ball.last_hitter << 4) | (g_ball.bounce_count & 15));
        if (g_game_state.shot_serial != g_ai.seen_shot) {
            /* A new incoming shot: decide once whether we are late. */
            g_ai.seen_shot = g_game_state.shot_serial;
            ++g_ai.returns;
            g_ai.late_this_ball = g_ai.late_period &&
                (g_ai.returns % g_ai.late_period) == 0;
        }
        if (key != g_ai.plan_key || g_ball.bounce_count > 0) {
            ai_plan_intercept(player);
            g_ai.plan_key = key;
            g_ai.plan_age = 0;
        } else if (g_ai.plan_age < 255) {
            ++g_ai.plan_age;
        }
        PROFILE_MARK(10);
        if (!player_is_swinging(player) && ai_swing_connects(player)) {
            ai_plan_shot(player, s_contact_z);
            ai_choose_aim(player, intent);
            intent->hit_pressed = 1;
            ++g_ai.shot_counter;
            return;
        }
    } else {
        g_ai.has_target = 0;
        g_ai.plan_key = 0xff;
    }

    if (!g_ai.has_target) {
        /* Home: the style's distance behind the net (default 80, x 208). */
        g_ai.target_x = player->team ? COURT_NET_X + s_style->recover :
            COURT_NET_X - s_style->recover;
        if (!s_has_partner)
            g_ai.target_y = AI_RECOVER_Y;
        else if (match_slot_lane_top(player_index))
            g_ai.target_y = AI_RECOVER_Y_TOP;
        else
            g_ai.target_y = AI_RECOVER_Y_BOTTOM;
    }
    intent->move_x = move_toward(x, g_ai.target_x);
    intent->move_y = move_toward(y, g_ai.target_y);
}
