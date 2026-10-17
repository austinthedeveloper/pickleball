#include "asset_frames.h"
#include "events.h"
#include "flair.h"
#include "game.h"
#include "input.h"
#include "match.h"
#include "player.h"
#include "serve.h"

Flair g_flair;

static const u8 s_celebration_frames[CELEBRATION_COUNT] = CELEBRATION_FRAMES;
/* The phase flair_tick saw last: emotes held for a phase end with it. */
static u8 s_phase;
/* Emotes up and reactions set: flair_tick skips its loops without. */
static u8 s_emotes_on;
static u8 s_emotes_counting;            /* one counts down (not held) */
/* The next slot whose celebration flair_tick looks up: one a frame, after
 * the match's first frames, which have no time to spare (make perf). */
static u8 s_celebrate_next;

/* Constant stores, no memset or loops: it runs on a new match's frame,
 * the game's heaviest (make perf). */
void flair_initialize(void)
{
    g_flair.active = 0;
    g_flair.fresh = 0;
    g_flair.wake = 1;
    ++g_flair.react_serial;
    g_flair.reacting = 0;
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
    s16 d;

    for (slot = 0; slot < MAX_PLAYERS; ++slot, ++player) {
        if (!player->active) {
            g_flair.react[slot] = FLAIR_REACT_NONE;
            continue;
        }
        if (player->team == winner) {
            g_flair.react[slot] = FLAIR_REACT_CELEBRATE;
            if (g_flair.rally_shots >= FLAIR_STAR_RALLY)
                flair_emote(slot, FLAIR_EMOTE_STAR,
                    FLAIR_RESULT_EMOTE_FRAMES);
            continue;
        }
        g_flair.react[slot] = FLAIR_REACT_SLUMP;
        if (g_game_state.last_fault != GAME_FAULT_SECOND_BOUNCE ||
                player->state != PLAYER_STATE_MOVE)
            continue;
        d = flair_abs(FROM_FP(player->x_fp) - ball_x) +
            flair_abs(FROM_FP(player->y_fp) - ball_y);
        if (d < nearest) {
            nearest = d;
            diver = slot;
        }
    }
    if (diver != 0xff)
        g_flair.react[diver] = FLAIR_REACT_DIVE;
    g_flair.result_frames = 0;
    g_flair.reacting = 1;
    ++g_flair.react_serial;
    g_flair.active = 1;
}

void flair_on_event(u8 kind, s16 x, s16 y, u8 detail)
{
    u8 slot;

    g_flair.active = 1;                 /* flair_tick settles it */
    g_flair.fresh = 1;
    switch (kind) {
    case EVENT_SERVE:
        g_flair.rally_shots = 1;
        g_flair.soft_run = 0;
        g_flair.shot = 0xff;
        break;
    case EVENT_HIT:
        if (g_flair.rally_shots < 0xff)
            ++g_flair.rally_shots;
        g_flair.shot = detail >> 4;
        slot = detail & 3;
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
    for (slot = 0; slot < MAX_PLAYERS; ++slot, ++player)
        if (player->active && player->team == losing_team)
            flair_emote(slot, emote, FLAIR_RESULT_EMOTE_FRAMES);
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

    s_phase = phase;
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
