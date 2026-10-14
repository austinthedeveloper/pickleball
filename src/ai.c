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
/* The teammate (set with s_has_partner by ai_update). */
static const Player *s_mate;

static s16 s_impact_vz;

/* The planned contact is a volley (set by ai_swing_connects), and the
 * slot's swing is a block volley: neutral and down the middle, held
 * through its active frames (ADR-035). Outside AiState (golden traces). */
static u8 s_contact_volley;
static u8 s_block_volley[MAX_PLAYERS];
/* A new shot's volley check waits for the slot's next think: the frame a
 * shot is planned already pays for its landing prediction (ADR-035). */
static u8 s_volley_check[MAX_PLAYERS];

/* Net play (ADR-035): volleys are legal once both bounces are done. */
#define AI_VOLLEYS_LEGAL() \
    (g_game_state.required_bounce == GAME_REQUIRED_BOUNCE_NONE)
#define AI_VOLLEYS(style) ((style)->volley && AI_VOLLEYS_LEGAL())
/* A volley is played between these heights (12.4): lower is a half-volley
 * the player lets bounce, higher is over their head. */
#define AI_VOLLEY_MIN_Z TO_FP(4)

/*
 * Landing cache: a flight is fixed from a hit or a bounce to the next one,
 * so its landing is predicted once and shared. Every CPU on the receiving
 * team plans the same flight within a few frames of the shot, and each
 * prediction steps up to AI_PREDICT_MAX_STEPS (M4-UI-03/04). The cached
 * landing is exact: x and y are linear in steps, and the steps left are
 * the cached count less the steps flown since (g_ball_steps).
 */
static u8 s_land_valid;
static u8 s_land_serial;        /* g_game_state.shot_serial */
static u8 s_land_bounces;       /* g_ball.bounce_count */
static u16 s_land_step;         /* g_ball_steps at the landing */
static s16 s_land_x;
static s16 s_land_y;
static s16 s_land_impact_vz;

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
    s_land_valid = 0;
    /* RAM is not cleared at power-on: a stale block volley would centre
     * the first serve. */
    s_contact_volley = 0;
    for (i = 0; i < MAX_PLAYERS; ++i) {
        s_block_volley[i] = 0;
        s_volley_check[i] = 0;
    }
}

/*
 * Ground contact prediction using the ball's own constants. x/y are linear
 * until the bounce, so only z is stepped; the loop is capped so a bad state
 * can never hang the frame.
 *
 * Eight steps at a time while the ball stays up: after k steps z is
 * z + k*vz + g*k(k-1)/2 and vz is vz + k*g, exactly. A falling parabola is
 * below zero only past its one crossing, so a block that ends above ground
 * never touched it, and the block that lands is stepped singly: the same
 * result as one step at a time, in about an eighth of the loop (a single
 * step cost 816-tcc about a scanline; M4-UI-03/04).
 */
#define AI_PREDICT_BLOCK 8
#define AI_PREDICT_BLOCK_FALL (28 * BALL_GRAVITY_FP)    /* g * 8 * 7 / 2 */

u8 ai_predict_landing(const Ball *ball, s16 *land_x, s16 *land_y)
{
    fx z = ball->z_fp;
    s16 vz = ball->vz_fp;
    s16 rise;
    u8 steps = 0;

    s_impact_vz = 0;
    if (z <= 0 && vz <= 0) {
        *land_x = FROM_FP(ball->x_fp);
        *land_y = FROM_FP(ball->y_fp);
        return 0;
    }
    while (steps + AI_PREDICT_BLOCK <= AI_PREDICT_MAX_STEPS) {
        rise = vz + vz;                 /* 8 * vz, without a multiply */
        rise += rise;
        rise += rise;
        rise += AI_PREDICT_BLOCK_FALL;
        if (z + rise <= 0)
            break;
        z += rise;
        vz += AI_PREDICT_BLOCK * BALL_GRAVITY_FP;
        steps += AI_PREDICT_BLOCK;
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

/*
 * Teamwork (ADR-036): a ball near the middle goes to whichever teammate
 * is clearly nearer it, not strictly by half; a volley's middle band is
 * wider (a poach). Clearly: by AI_MIDDLE_MARGIN, so teammates thinking on
 * different frames agree; otherwise the halves decide. Without the style
 * knob, the halves alone.
 */
static s16 abs_s16(s16 value);

/* A human is "up" within this of the net: their partner comes in. */
#define AI_PAIR_UP_DISTANCE 80
/* The pair's shift toward the ball's side, at most. */
#define AI_SHIFT_MAX 8
#define AI_MIDDLE_BAND 12
#define AI_POACH_BAND 20
#define AI_MIDDLE_MARGIN 10

static u8 ai_owns_point(const Player *player, s16 x, s16 y, u8 volley)
{
    s16 dy = y - COURT_CENTERLINE_Y;
    s16 me;
    s16 mate;

    if (!s_has_partner || !s_style->team ||
            abs_s16(dy) > (volley ? AI_POACH_BAND : AI_MIDDLE_BAND))
        return ai_owns_y(y);
    me = abs_s16(x - FROM_FP(player->x_fp)) + abs_s16(y - FROM_FP(player->y_fp));
    mate = abs_s16(x - FROM_FP(s_mate->x_fp)) +
        abs_s16(y - FROM_FP(s_mate->y_fp));
    if (me + AI_MIDDLE_MARGIN < mate)
        return 1;
    /* A human may not go for it: their partner never gives up its own
     * half, it only poaches into theirs. */
    if (s_partner_human)
        return ai_owns_y(y);
    if (mate + AI_MIDDLE_MARGIN < me)
        return 0;
    return ai_owns_y(y);
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

    s_block_volley[s_slot] = 0;
    if (contact_z >= TO_FP(s_style->putaway_z) &&
            to_net < s_style->putaway_distance) {
        g_ai.planned_shot = SHOT_DRIVE;
        g_ai.planned_depth = 0;
    } else if (s_contact_volley) {
        /* A volley met off-centre sprays long and wide (ADR-019): block it
         * back with control instead of going for depth and a line. */
        g_ai.planned_shot = SHOT_STANDARD;
        g_ai.planned_depth = 0;
        s_block_volley[s_slot] = 1;
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
    if (s_block_volley[s_slot])
        intent->aim_y = 0;              /* a block volley: down the middle */
    else if (s_has_partner)
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

/* The swing window's first contact check is after this many probe steps;
 * the steps before it are skipped as one block when they can be. */
typedef char ai_windup_skip_is_3[PLAYER_WINDUP_FRAMES - 1 == 3 ? 1 : -1];

static u8 ai_swing_connects(const Player *player)
{
    u8 step = 1;
    u8 n;
    u8 volleys;
    s16 rise;
    s16 dx = FROM_FP(g_ball.x_fp) - FROM_FP(player->x_fp);

    /* The ball moves < 8 px/frame, so beyond this it cannot arrive in time. */
    if (dx > 64 || dx < -64)
        return 0;
    /* Contact needs a bounced ball (unless this CPU volleys, ADR-035);
     * skip the lookahead while the bounce is still beyond the swing window
     * (the common case, and the costly one). */
    volleys = AI_VOLLEYS(s_style);
    if (g_ball.bounce_count == 0 && !volleys &&
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
     * two calls per step cost 816-tcc real frame time.
     *
     * At a steady pace, steps 1-3 only move the ball. When it stays off
     * the ground they are one exact block (as in ai_predict_landing: three
     * steps add 3 vz and 3 g to z): a third of the lookahead, which runs
     * every frame of a swing window (M4-UI-03/04). */
    if (!g_ball_paced && (g_probe.z != 0 || g_probe.vz > 0)) {
        rise = g_probe.vz + g_probe.vz + g_probe.vz + 3 * BALL_GRAVITY_FP;
        if (g_probe.z + rise > 0) {
            g_probe.z += rise;
            g_probe.vz += 3 * BALL_GRAVITY_FP;
            g_probe.x += g_probe.vx + g_probe.vx + g_probe.vx;
            g_probe.y += g_probe.vy + g_probe.vy + g_probe.vy;
            step = PLAYER_WINDUP_FRAMES;
        }
    }
    for (; step <= PLAYER_WINDUP_FRAMES + PLAYER_ACTIVE_FRAMES; ++step) {
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
        s_contact_volley = g_probe.bounces == 0;
        /* A volley only from a volleying style, below the minimum height
         * never, and never at a ball that would land out (ADR-035). */
        if (g_probe.bounces == 0 && (!volleys || g_probe.z < AI_VOLLEY_MIN_Z ||
                !court_contains_ground_fp(TO_FP(g_ai.predicted_land_x),
                    TO_FP(g_ai.predicted_land_y))))
            return 0;
        if (!court_x_on_side(g_probe.x, player->team))
            return 0;
        /* A human partner's lane is theirs, even when we could reach. */
        if (s_partner_human && !ai_owns_point(player, FROM_FP(g_probe.x),
                FROM_FP(g_probe.y), g_probe.bounces == 0))
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

/*
 * Net play (ADR-035). A team may come in once both bounces are done, or the
 * receiving team as soon as it has returned the serve (the serving team must
 * still let the return bounce).
 */

static u8 ai_may_come_in(u8 team)
{
    if (g_game_state.phase != GAME_PHASE_LIVE)
        return 0;
    return g_game_state.required_bounce == GAME_REQUIRED_BOUNCE_NONE ||
        (g_game_state.required_bounce == GAME_REQUIRED_BOUNCE_RETURN &&
         team != g_game_state.serving_team);
}

/*
 * Stand and volley (ADR-035): the ball, coming in, passes the player's x
 * before it would land, at a height they can hit. Then the target is that
 * point and the player stays put. Lobs over the head and balls that would
 * land out are left to the bounce logic. One division per new shot.
 */
static u8 ai_plan_volley(const Player *player, u8 land_steps, s16 land_x)
{
    fx x = player->x_fp;
    s16 px = FROM_FP(x);
    s16 bx = FROM_FP(g_ball.x_fp);
    s16 vx = g_ball.vx_fp;
    s16 t;
    fx z;

    /* A cheap test first (the math below is three library calls, on the
     * frame a shot is planned): the ball passes the player before it would
     * land, so the player stands between the two. */
    if (!vx || (px - bx < 0) != (land_x - px < 0) || px == land_x)
        return 0;
    t = (s16)((x - g_ball.x_fp) / vx);
    if (t <= 0 || t >= (s16)land_steps)
        return 0;
    /* z after t steps: z + t vz + g t (t - 1) / 2, exactly. */
    z = g_ball.z_fp + (fx)(t * g_ball.vz_fp) +
        (fx)(BALL_GRAVITY_FP * (t * (t - 1) / 2));
    if (z < AI_VOLLEY_MIN_Z || z > g_hit_box.reach_z - TO_FP(2))
        return 0;
    g_ai.target_x = FROM_FP(x);
    g_ai.target_y = FROM_FP(g_ball.y_fp + (fx)(t * g_ball.vy_fp));
    g_ai.has_target = ai_owns_point(player, g_ai.target_x, g_ai.target_y, 1);
    return 1;
}

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
        g_ai.has_target = ai_owns_point(player, FROM_FP(g_ball.x_fp),
            FROM_FP(g_ball.y_fp), 0);
        AI_AVOID_LAVA(player);
        return;
    }

    if (s_land_valid && s_land_serial == g_game_state.shot_serial &&
            s_land_bounces == g_ball.bounce_count) {
        steps = (u8)(s_land_step - g_ball_steps);
        land_x = s_land_x;
        land_y = s_land_y;
        s_impact_vz = s_land_impact_vz;
    } else {
        steps = ai_predict_landing(&g_ball, &land_x, &land_y);
        /* Only a found landing: a capped prediction would change. */
        s_land_valid = steps != 0 && steps < AI_PREDICT_MAX_STEPS;
        s_land_serial = g_game_state.shot_serial;
        s_land_bounces = g_ball.bounce_count;
        s_land_step = g_ball_steps + steps;
        s_land_x = land_x;
        s_land_y = land_y;
        s_land_impact_vz = s_impact_vz;
    }
    /* FAST BALL takes an extra step every 3rd frame, so N steps take at
     * least N - N/4 - 1 frames: the swing early-out stays safe. A slow ball
     * (ADR-025) only takes longer, so its steps are a safe count. */
    if ((g_match_config.modifiers & MODIFIER_FAST_BALL) && steps > 1)
        steps -= (steps >> 2) + 1;
    g_ai.predicted_frames = steps;
    g_ai.predicted_land_x = land_x;
    g_ai.predicted_land_y = land_y;

    /* A volley (ADR-035) only at a ball that would land in: checked on
     * the next think (s_volley_check), from the cached landing. */
    s_volley_check[s_slot] = AI_VOLLEYS(s_style) && g_ball.bounce_count == 0 &&
        s_land_valid &&
        court_contains_ground_fp(TO_FP(land_x), TO_FP(land_y)) &&
        court_x_on_side(TO_FP(land_x), player->team);

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
    g_ai.has_target = ai_owns_point(player, land_x, land_y, 0);
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
    s_style = &g_ai_default_style;  /* the assist never volleys */
    s_slot = player_index;
    s_has_partner = 0;
    s_partner_human = 0;
    connects = ai_swing_connects(player);
    return connects;
}

/* Attract demo budget: the demo P1 thinks every other frame and repeats its
 * last movement in between (the one-shot swing press is not repeated). */
/* A slot's held intent by a constant index (816-tcc multiplies for a
 * variable one, and these run for every CPU, every frame). */
static PlayerIntent *ai_held(u8 player_index)
{
    if (player_index == 0)
        return &s_held_intent[0];
    if (player_index == 1)
        return &s_held_intent[1];
    if (player_index == 2)
        return &s_held_intent[2];
    return &s_held_intent[3];
}

void ai_hold_intent(u8 player_index, PlayerIntent *intent)
{
    RAM_COPY(intent, ai_held(player_index), sizeof(PlayerIntent));
    intent->hit_pressed = 0;
}

void ai_update(u8 player_index, PlayerIntent *intent)
{
    AiState *state;

    s_slot = player_index;
    /* Constant indices (816-tcc multiplies for g_players[variable], and
     * for g_ai_slots[variable] twice a think). */
    if (player_index == 0) {
        s_mate = &g_players[1];
        s_style = &g_ai_styles[0];
        state = &g_ai_slots[0];
    } else if (player_index == 1) {
        s_mate = &g_players[0];
        s_style = &g_ai_styles[1];
        state = &g_ai_slots[1];
    } else if (player_index == 2) {
        s_mate = &g_players[3];
        s_style = &g_ai_styles[2];
        state = &g_ai_slots[2];
    } else {
        s_mate = &g_players[2];
        s_style = &g_ai_styles[3];
        state = &g_ai_slots[3];
    }
    s_has_partner = s_mate->active;
    s_partner_human = s_has_partner &&
        g_input_owners[player_index ^ 1] == INPUT_OWNER_HUMAN;
    RAM_COPY(&g_ai, state, sizeof(g_ai));
    PROFILE_MARK(8);
    ai_think(player_index, intent);
    PROFILE_MARK(11);
    RAM_COPY(state, &g_ai, sizeof(g_ai));
    RAM_COPY(ai_held(player_index), intent, sizeof(PlayerIntent));
}

static void ai_think(u8 player_index, PlayerIntent *intent)
{
    Player *player = &g_players[player_index];
    s16 x;
    s16 y;
    u8 key;
    u8 incoming;
    u8 home;
    s16 shift;

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
         * otherwise alternating deep and neutral. Never a block volley's
         * centred aim (ADR-035). */
        s_block_volley[player_index] = 0;
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
        } else {
            if (g_ai.plan_age < 255)
                ++g_ai.plan_age;
            /* The volley check a new shot left for this think. */
            if (s_volley_check[player_index]) {
                s_volley_check[player_index] = 0;
                if (g_ball.bounce_count == 0 && AI_VOLLEYS(s_style) &&
                        s_land_serial == g_game_state.shot_serial)
                    ai_plan_volley(player, (u8)(s_land_step - g_ball_steps),
                        s_land_x);
            }
        }
        PROFILE_MARK(10);
        /* Doubles: only the teammate whose lane holds the ball looks ahead
         * for a swing; the other covers its lane. Two lookaheads on one
         * frame were the doubles frame drops (M4-UI-04). */
        if ((!s_has_partner || g_ai.has_target) &&
                !player_is_swinging(player) && ai_swing_connects(player)) {
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
        /* Home: the style's distance behind the net (default 80, x 208),
         * or its net distance once the rally lets it come in (ADR-035). */
        home = (s_style->net && ai_may_come_in(player->team)) ?
            s_style->net : s_style->recover;
        /* ADR-036: a human's partner comes in only when they are up too
         * (side by side, never split up and back). */
        if (s_style->team && s_partner_human && home == s_style->net &&
                abs_s16(FROM_FP(s_mate->x_fp) - COURT_NET_X) >
                    AI_PAIR_UP_DISTANCE)
            home = s_style->recover;
        g_ai.target_x = player->team ? COURT_NET_X + home :
            COURT_NET_X - home;
        if (!s_has_partner)
            g_ai.target_y = AI_RECOVER_Y;
        else if (match_slot_lane_top(player_index))
            g_ai.target_y = AI_RECOVER_Y_TOP;
        else
            g_ai.target_y = AI_RECOVER_Y_BOTTOM;
        /* ADR-036: the pair shifts toward the ball's side while it is on
         * the other side of the net (covering the angle and the middle). */
        if (s_has_partner && s_style->team &&
                !court_x_on_side(g_ball.x_fp, player->team)) {
            shift = (s16)(FROM_FP(g_ball.y_fp) - COURT_CENTERLINE_Y) >> 2;
            if (shift > AI_SHIFT_MAX)
                shift = AI_SHIFT_MAX;
            else if (shift < -AI_SHIFT_MAX)
                shift = -AI_SHIFT_MAX;
            g_ai.target_y += shift;
        }
    }
    intent->move_x = move_toward(x, g_ai.target_x);
    intent->move_y = move_toward(y, g_ai.target_y);
}
