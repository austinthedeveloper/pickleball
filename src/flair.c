#include "asset_frames.h"
#include "collision.h"
#include "events.h"
#include "flair.h"
#include "game.h"
#include "input.h"
#include "match.h"
#include "player.h"
#include "serve.h"

Flair g_flair;
u16 g_flair_hit[MAX_PLAYERS];

static const u8 s_celebration_frames[CELEBRATION_COUNT] = CELEBRATION_FRAMES;
/* The phase flair_tick saw last: emotes held for a phase end with it. */
static u8 s_phase;
/* Emotes up and reactions set: flair_tick skips its loops without. */
static u8 s_emotes_on;
static u8 s_emotes_counting;            /* one counts down (not held) */
/* The next slot whose celebration flair_tick looks up: one a frame, after
 * the match's first frames, which have no time to spare (make perf). */
static u8 s_celebrate_next;
#define FLAIR_RESULT(phase) \
    ((phase) == GAME_PHASE_POINT_RESULT || (phase) == GAME_PHASE_MATCH_RESULT)
/* The point's losing player nearest the ball (a quip's face), and each
 * team's last hitter this rally (a star's winner): 0xff for none. */
static u8 s_quip_slot;
static u8 s_quip_team;
static u8 s_change_waited;              /* a phase change waited its frame */
static u8 s_last_hit[2];

/* Constant stores, no memset or loops: it runs on a new match's frame,
 * the game's heaviest (make perf). */
void flair_initialize(void)
{
    g_flair.active = 0;
    g_flair.fresh = 0;
    g_flair.wake = 1;
    ++g_flair.react_serial;
    g_flair.reacting = 0;
    g_flair.repaint = 1;
    g_flair.quip_team = 0xff;
    g_flair.react[0] = FLAIR_REACT_NONE;
    g_flair.react[1] = FLAIR_REACT_NONE;
    g_flair.react[2] = FLAIR_REACT_NONE;
    g_flair.react[3] = FLAIR_REACT_NONE;
    g_flair.emote[0] = FLAIR_EMOTE_NONE;
    g_flair.emote[1] = FLAIR_EMOTE_NONE;
    g_flair.emote[2] = FLAIR_EMOTE_NONE;
    g_flair.emote[3] = FLAIR_EMOTE_NONE;
    g_flair.celebrate[0] = FRAME_CHEER_INDEX;
    g_flair.celebrate[1] = FRAME_CHEER_INDEX;
    g_flair.celebrate[2] = FRAME_CHEER_INDEX;
    g_flair.celebrate[3] = FRAME_CHEER_INDEX;
    g_flair.result_frames = 0;
    g_flair.rally_shots = 0;
    g_flair.soft_run = 0;
    g_flair.shot = 0xff;
    g_flair.squash_timer = 0;
    g_flair.dust_timer = 0;
    g_flair.cord_timer = 0;
    s_phase = 0xff;
    s_emotes_on = 0;
    s_emotes_counting = 0;
    s_celebrate_next = 0;
    s_quip_slot = 0xff;
    s_last_hit[0] = 0xff;
    s_last_hit[1] = 0xff;
    s_change_waited = 0;
    g_flair_hit[0] = 0xffff;
    g_flair_hit[1] = 0xffff;
    g_flair_hit[2] = 0xffff;
    g_flair_hit[3] = 0xffff;
}

/* ADR-074: an overhead above the band, else a stretched contact's lunge,
 * else a low one's scoop. */
u16 flair_hit_frame(const Player *player, const Ball *ball)
{
    fx z = ball->z_fp;

    if (z >= TO_FP(FLAIR_OVERHEAD_Z))
        return FRAME_OVERHEAD_CONTACT_INDEX;
    if (collision_ball_in_sweet_spot(player, ball) == 0)
        return FRAME_REACH_INDEX;
    if (z < TO_FP(FLAIR_SCOOP_Z))
        return FRAME_SCOOP_INDEX;
    return 0xffff;
}

/* A rally contact's pose (ADR-074). Constant slots: &g_players[slot] is a
 * multiply on 816-tcc. The ball machine's feeds (practice) come from slot
 * 2, which is empty there. */
static void flair_note_hit(u8 slot)
{
    const Player *player;

    if (slot == 0)
        player = &g_players[0];
    else if (slot == 1)
        player = &g_players[1];
    else if (slot == 2)
        player = &g_players[2];
    else
        player = &g_players[3];
    if (player->active)
        g_flair_hit[slot] = flair_hit_frame(player, &g_ball);
}

static void flair_emote(u8 slot, u8 emote, u8 frames)
{
    g_flair.emote[slot] = emote;
    g_flair.emote_timer[slot] = frames;
    s_emotes_on = 1;
    if (frames != FLAIR_RESULT_EMOTE_FRAMES)
        s_emotes_counting = 1;
    g_flair.active = 1;
}

/* How a lost point looks on the losers' faces. */
static u8 flair_fault_emote(u8 fault)
{
    if (fault == GAME_FAULT_NET)
        return FLAIR_EMOTE_EXCLAIM;
    if (fault == GAME_FAULT_OUT)
        return FLAIR_EMOTE_QUESTION;
    if (fault == GAME_FAULT_SECOND_BOUNCE || fault == GAME_FAULT_NONE)
        return FLAIR_EMOTE_SWEAT;
    return FLAIR_EMOTE_ANGER;           /* kitchen, box, volley, lava */
}

static s16 flair_abs(s16 v)
{
    return v < 0 ? -v : v;
}

/* The point is over: winners celebrate, losers slump; a loser running for a
 * ball that bounced twice past dives for it (the nearest one). */
static void flair_point(u8 winner, s16 ball_x, s16 ball_y)
{
    const Player *player = g_players;
    u8 slot;
    u8 diver = 0xff;
    s16 nearest = 0x7fff;
    s16 nearest_runner = 0x7fff;
    s16 d;

    s_quip_slot = 0xff;
    for (slot = 0; slot < MAX_PLAYERS; ++slot, ++player) {
        if (!player->active) {
            g_flair.react[slot] = FLAIR_REACT_NONE;
            continue;
        }
        if (player->team == winner) {
            g_flair.react[slot] = FLAIR_REACT_CELEBRATE;
            continue;
        }
        g_flair.react[slot] = FLAIR_REACT_SLUMP;
        /* The loser nearest the ball: the one who missed it, the face of
         * a quip; running for a second bounce, they dive. */
        d = flair_abs(FROM_FP(player->x_fp) - ball_x) +
            flair_abs(FROM_FP(player->y_fp) - ball_y);
        if (d < nearest) {
            nearest = d;
            s_quip_slot = slot;
            s_quip_team = player->team;
        }
        if (g_game_state.last_fault != GAME_FAULT_SECOND_BOUNCE ||
                player->state != PLAYER_STATE_MOVE)
            continue;
        if (d < nearest_runner) {
            nearest_runner = d;
            diver = slot;
        }
    }
    if (diver != 0xff)
        g_flair.react[diver] = FLAIR_REACT_DIVE;
    /* A long rally's star for the winner who hit last (one emote, not a
     * team's: a doubles result has no time for four, make perf). */
    if (g_flair.rally_shots >= FLAIR_STAR_RALLY &&
            s_last_hit[winner] != 0xff)
        flair_emote(s_last_hit[winner], FLAIR_EMOTE_STAR,
            FLAIR_RESULT_EMOTE_FRAMES);
    g_flair.result_frames = 0;
    g_flair.reacting = 1;
    ++g_flair.react_serial;
    g_flair.active = 1;
    /* The point result begins (rules_resolve_point sets it next): the
     * emotes just set belong to it, so that change must not clear them. */
    s_phase = GAME_PHASE_POINT_RESULT;
}

void flair_on_event(u8 kind, s16 x, s16 y, u8 detail)
{
    u8 slot;

    g_flair.active = 1;                 /* flair_tick settles it */
    g_flair.fresh = 1;
    switch (kind) {
    case EVENT_SERVE:
        s_last_hit[0] = 0xff;
        s_last_hit[1] = 0xff;
        s_last_hit[detail >= 2] = detail;
        g_flair_hit[detail & 3] = 0xffff;
        g_flair.rally_shots = 1;
        g_flair.soft_run = 0;
        g_flair.shot = 0xff;
        break;
    case EVENT_HIT:
        if (g_flair.rally_shots < 0xff)
            ++g_flair.rally_shots;
        g_flair.shot = detail >> 4;
        slot = detail & 3;
        s_last_hit[slot >= 2] = slot;
        flair_note_hit(slot);
        if (g_flair.shot == SHOT_DRIVE)
            g_flair.squash_timer = FLAIR_SQUASH_FRAMES;
        if (g_flair.shot != SHOT_SOFT) {
            g_flair.soft_run = 0;
        } else if (++g_flair.soft_run >= FLAIR_NOTE_RUN) {
            flair_emote(slot, FLAIR_EMOTE_NOTE, FLAIR_NOTE_FRAMES);
        }
        break;
    case EVENT_BOUNCE:
        g_flair.dust_timer = FLAIR_DUST_FRAMES;
        g_flair.dust_x = x;
        g_flair.dust_y = y;
        g_flair.shot = 0xff;
        break;
    case EVENT_FAULT:
        if (detail == GAME_FAULT_NET) {
            g_flair.cord_timer = FLAIR_CORD_FRAMES;
            g_flair.cord_y = y;
        }
        break;
    case EVENT_POINT:
        g_flair.shot = 0xff;
        g_flair_hit[0] = 0xffff;
        g_flair_hit[1] = 0xffff;
        g_flair_hit[2] = 0xffff;
        g_flair_hit[3] = 0xffff;
        flair_point(detail, x, y);
        break;
    default:
        break;
    }
}

void flair_quip(u8 losing_team)
{
    const Player *player = g_players;
    u8 emote = flair_fault_emote(g_game_state.last_fault);
    u8 phase = g_game_state.phase;
    u8 slot;

    /* Only on the result it belongs to: a quip the staged HUD picks after
     * the next serve has begun would hold its emote through that serve. */
    if (phase != GAME_PHASE_POINT_RESULT && phase != GAME_PHASE_MATCH_RESULT)
        return;
    /* The loser nearest the ball at the point (flair_point), else the
     * team's first. */
    if (s_quip_slot != 0xff && s_quip_team == losing_team) {
        flair_emote(s_quip_slot, emote, FLAIR_RESULT_EMOTE_FRAMES);
        return;
    }
    for (slot = 0; slot < MAX_PLAYERS; ++slot, ++player)
        if (player->active && player->team == losing_team) {
            flair_emote(slot, emote, FLAIR_RESULT_EMOTE_FRAMES);
            return;
        }
}

/* Z's over the receivers while a human server takes their time. */
static void flair_waiting(void)
{
    const Player *player = g_players;
    u8 server = g_serve_state.server_index;
    u8 team = server >= 2;
    u8 slot;

    if (g_game_state.phase_timer < FLAIR_ZZZ_WAIT || g_attract ||
            g_input_owners[server] != INPUT_OWNER_HUMAN)
        return;
    for (slot = 0; slot < MAX_PLAYERS; ++slot, ++player)
        if (player->active && player->team != team &&
                g_flair.emote[slot] == FLAIR_EMOTE_NONE)
            flair_emote(slot, FLAIR_EMOTE_ZZZ, FLAIR_RESULT_EMOTE_FRAMES);
}

/* The busy part of flair_tick: timers, reactions, emotes. */
static void flair_tick_active(u8 phase)
{
    u8 changed = phase != s_phase;
    u8 slot;
    u8 on;

    /* A point's result going on to the match's keeps its emotes. */
    if (changed && FLAIR_RESULT(phase) && FLAIR_RESULT(s_phase))
        changed = 0;

    s_phase = phase;
    g_flair.repaint = 1;
    if (g_flair.dust_timer)
        --g_flair.dust_timer;
    if (g_flair.cord_timer)
        --g_flair.cord_timer;
    if (g_flair.squash_timer)
        --g_flair.squash_timer;
    if (phase == GAME_PHASE_POINT_RESULT || phase == GAME_PHASE_MATCH_RESULT) {
        if (g_flair.result_frames < 0xff)
            ++g_flair.result_frames;
        /* A dive's steps change its frame. */
        if (g_flair.result_frames == FLAIR_DIVE_FRAMES ||
                g_flair.result_frames == FLAIR_SCRAMBLE_END)
            ++g_flair.react_serial;
    } else if (g_flair.reacting) {
        g_flair.reacting = 0;
        for (slot = 0; slot < MAX_PLAYERS; ++slot)
            g_flair.react[slot] = FLAIR_REACT_NONE;
        ++g_flair.react_serial;
    }
    if (s_emotes_on && (changed || s_emotes_counting)) {
        on = 0;
        s_emotes_counting = 0;
        for (slot = 0; slot < MAX_PLAYERS; ++slot) {
            if (g_flair.emote[slot] == FLAIR_EMOTE_NONE)
                continue;
            /* Held emotes last the phase; the others count down. */
            if (g_flair.emote_timer[slot] == FLAIR_RESULT_EMOTE_FRAMES) {
                if (changed)
                    g_flair.emote[slot] = FLAIR_EMOTE_NONE;
            } else if (--g_flair.emote_timer[slot] == 0) {
                g_flair.emote[slot] = FLAIR_EMOTE_NONE;
            }
            if (g_flair.emote[slot] != FLAIR_EMOTE_NONE) {
                on = 1;
                if (g_flair.emote_timer[slot] != FLAIR_RESULT_EMOTE_FRAMES)
                    s_emotes_counting = 1;
            }
        }
        s_emotes_on = on;
    }
    /* Settled reactions (past a dive's last step) and held emotes need no
     * ticks: the next phase change wakes flair (game_set_phase) to clear
     * them. A match result can sit for minutes, and the frame that leaves
     * it is the game's heaviest (make perf). */
    /* A quip's emote, after the clearing above (it belongs to now). */
    if (g_flair.quip_team != 0xff) {
        flair_quip(g_flair.quip_team);
        g_flair.quip_team = 0xff;
    }
    g_flair.active = g_flair.dust_timer | g_flair.cord_timer |
        g_flair.squash_timer | s_emotes_counting |
        (g_flair.reacting && g_flair.result_frames < FLAIR_SCRAMBLE_END) |
        (g_flair.shot == SHOT_DRIVE) | (g_flair.shot == SHOT_SLICE);
}

/* Most frames nothing is up: a few compares (the doubles budget). */
void flair_tick(void)
{
    u8 phase = g_game_state.phase;

    if (s_celebrate_next < MAX_PLAYERS &&
            g_game_state.phase_timer >= FLAIR_LOOKUP_DELAY) {
        g_flair.celebrate[s_celebrate_next] =
            s_celebration_frames[match_slot_celebrate(s_celebrate_next)];
        ++s_celebrate_next;
    }
    if (phase == GAME_PHASE_SERVE_SETUP &&
            g_game_state.phase_timer >= FLAIR_ZZZ_WAIT)
        flair_waiting();
    /* A phase change's clearing (held emotes, reactions) waits a frame:
     * the change's own frame (a result ending, the next serve's setup) is
     * among the game's heaviest (make perf). wake stays set meanwhile. */
    if (phase != s_phase && !s_change_waited) {
        s_change_waited = 1;
        return;
    }
    s_change_waited = 0;
    if (g_flair.active || phase != s_phase)
        flair_tick_active(phase);
    g_flair.wake = (s_celebrate_next < MAX_PLAYERS) |
        (phase == GAME_PHASE_SERVE_SETUP);
}

u8 flair_body_frame(u8 slot)
{
    switch (g_flair.react[slot]) {
    case FLAIR_REACT_CELEBRATE:
        return g_flair.celebrate[slot];
    case FLAIR_REACT_SLUMP:
        return FRAME_SLUMP_INDEX;
    case FLAIR_REACT_DIVE:
        if (g_flair.result_frames < FLAIR_DIVE_FRAMES)
            return FRAME_DIVE_INDEX;
        if (g_flair.result_frames < FLAIR_SCRAMBLE_END)
            return FRAME_SCRAMBLE_INDEX;
        return FRAME_SLUMP_INDEX;
    default:
        return FLAIR_NONE;
    }
}
