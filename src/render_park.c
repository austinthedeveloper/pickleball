#include <snes.h>
#include "asset_frames.h"
#include "hud.h"
#include "hud_tiles.h"
#include "match.h"
#include "open_play.h"
#include "opponents.h"
#include "paddle_stats.h"
#include "render.h"
#include "render_internal.h"

/*
 * The open play park (ADR-034). BG1 holds the park, BG3 the message panel
 * and the scores over the courts, OBJ the people (8x16: two small objects),
 * the balls, and the paddles in the racks.
 */

extern char park_tiles, park_tiles_end;
extern char park_map, park_map_end;
extern char park_palette, park_palette_end;
extern char objects_palette;

/* OBJ palettes: 0 you, 1-6 the teams on court (court * 2 + side), 7 the
 * objects (balls, greys for the paddles). */
#define PARK_OBJPAL_YOU 0
#define PARK_OBJPAL_TEAMS 1
#define PARK_OBJPAL_OBJECTS 7

/* OAM: you 0-1, court people 2-25 (court * 8 + person * 2), balls 26-28,
 * their shadows 29-31, rack paddles 32-43 (court * 4 + place). */
#define PARK_OAM_YOU 0
#define PARK_OAM_PEOPLE 2
#define PARK_OAM_BALLS 26
#define PARK_OAM_SHADOWS 29
#define PARK_OAM_PADDLES 32
#define PARK_OAM_COUNT 44

/* Priority 2 (OBJ_ATTR): above BG1, below the BG3 HUD. */
#define PARK_ATTR(palette, flip) OBJ_ATTR(palette, flip)

#define PARK_PANEL_ROW 1            /* the message panel, rows 1-3 */
#define PARK_LEVEL_ROW 4            /* each court's skill level */
#define PARK_SCORE_ROW 5            /* over the courts (court top 48) */
#define PARK_RESULT_FRAMES 240      /* "YOU WON" after a match */

static u16 s_pal_you[16];
static u16 s_pal_teams[OPEN_COURTS * 2][16];
static u8 s_team_shown[OPEN_COURTS * 2];
static u8 s_palettes_dirty;
/* What the HUD shows, so it is redrawn only on a change. */
static u8 s_message;
static u8 s_message_arg;
static u8 s_score_shown[OPEN_COURTS][3];
static u16 s_result_timer;
static u8 s_anim;
/* Rally animation, precomputed so a frame does no division (816-tcc calls
 * a routine for each): the ball's travel from end to end over 32 frames,
 * its arc, and where it lands on each court this rally. */
#define PARK_TOP_Y (OPEN_COURT_TOP + 24)
#define PARK_BOTTOM_Y (OPEN_COURT_TOP + OPEN_COURT_H - 10)
static s16 s_track[32];
static s16 s_arc[32];
static s16 s_ball_x[OPEN_COURTS];
static u8 s_ball_end[OPEN_COURTS];      /* rally >> 5 for s_ball_x */

enum {
    PARK_MSG_NONE = 0xff,
    PARK_MSG_HELLO = 0,
    PARK_MSG_STACK = 1,         /* @ STACK YOUR PADDLE */
    PARK_MSG_TAKE = 2,          /* @ TAKE YOUR PADDLE */
    PARK_MSG_FULL = 3,
    PARK_MSG_NEXT = 4,          /* arg: court */
    PARK_MSG_WAIT = 5,          /* arg: court */
    PARK_MSG_PEEK = 6,          /* arg: court */
    PARK_MSG_CALLED = 7,
    PARK_MSG_WON = 8,
    PARK_MSG_LOST = 9
};

static const char *park_team_name(u8 entry)
{
    if (entry == OPEN_YOU)
        return "YOU";
    if (entry >= OPPONENT_COUNT)
        return "";
    return g_opponent_team[entry];
}

/* "TEXT n" into a buffer, n a digit or a two-digit score. */
static char *park_append(char *at, const char *text)
{
    while (*text)
        *at++ = *text++;
    return at;
}

static char *park_number(char *at, u8 value)
{
    if (value >= 10) {
        *at++ = (char)('0' + value / 10);
        value %= 10;
    }
    *at++ = (char)('0' + value);
    return at;
}

static u8 park_message(u8 *arg)
{
    u8 rack = open_rack_near();
    u8 court;
    u8 wait;

    *arg = 0;
    if (g_open_play.called != OPEN_NONE) {
        *arg = g_open_play.called;
        return PARK_MSG_CALLED;
    }
    /* Back from your game: its result first, for a few seconds. */
    if (s_result_timer && g_open_play.last_won != OPEN_NONE)
        return g_open_play.last_won ? PARK_MSG_WON : PARK_MSG_LOST;
    if (rack != OPEN_NONE) {
        *arg = rack;
        if (g_open_play.your_court == rack)
            return PARK_MSG_TAKE;
        return g_open_play.court[rack].stack_count >= OPEN_STACK_MAX ?
            PARK_MSG_FULL : PARK_MSG_STACK;
    }
    court = open_court_near();
    if (court != OPEN_NONE && court != g_open_play.your_court &&
            g_open_play.court[court].state == OPEN_COURT_PLAYING) {
        *arg = court;
        return PARK_MSG_PEEK;
    }
    wait = open_your_wait();
    if (wait != OPEN_NONE) {
        *arg = g_open_play.your_court;
        return wait == 0 ? PARK_MSG_NEXT : PARK_MSG_WAIT;
    }
    return PARK_MSG_HELLO;
}

/* A message changes in steps on separate frames (each panel draw costs
 * about 100 scanlines): clear, the panel, then the call's big text. */
enum { PARK_STEP_CLEAR = 0, PARK_STEP_PANEL = 1, PARK_STEP_BIG = 2,
       PARK_STEP_DONE = 3 };
static u8 s_message_step;
static u8 s_big_shown;              /* "YOUR GAME!" is on rows 11-12 */

static void park_draw_message(u8 message, u8 arg)
{
    static char text[29];
    const OpenCourt *court = &g_open_play.court[arg < OPEN_COURTS ? arg : 0];
    char *at = text;

    if (s_message_step == PARK_STEP_CLEAR) {
        hud_clear_rows(PARK_PANEL_ROW, 3);
        if (s_big_shown) {
            hud_clear_rows(10, 4);
            s_big_shown = 0;
        }
        return;
    }
    if (s_message_step == PARK_STEP_BIG) {
        if (message == PARK_MSG_CALLED) {
            hud_center_big(11, HUD_PAL_YELLOW, "YOUR GAME!");
            s_big_shown = 1;
        }
        return;
    }
    switch (message) {
    case PARK_MSG_STACK:
        at = park_append(at, "@ STACK YOUR PADDLE");
        break;
    case PARK_MSG_TAKE:
        at = park_append(at, "@ TAKE YOUR PADDLE");
        break;
    case PARK_MSG_FULL:
        at = park_append(at, "THIS STACK IS FULL");
        break;
    case PARK_MSG_NEXT:
        at = park_append(at, "YOU'RE NEXT ON COURT ");
        at = park_number(at, arg + 1);
        break;
    case PARK_MSG_WAIT:
        at = park_append(at, "WAITING ON COURT ");
        at = park_number(at, arg + 1);
        break;
    case PARK_MSG_PEEK:
        at = park_append(at, park_team_name(court->playing[0]));
        at = park_append(at, " VS ");
        at = park_append(at, park_team_name(court->playing[1]));
        break;
    case PARK_MSG_CALLED:
        /* "WITH ACE VS TWINS": the captain next in line partners you. */
        if (g_open_play.partner < OPPONENT_COUNT) {
            at = park_append(at, "WITH ");
            at = park_append(at, g_opponent_captain[g_open_play.partner]);
        } else {
            at = park_append(at, "COURT ");
            at = park_number(at, arg + 1);
        }
        at = park_append(at, " VS ");
        at = park_append(at, park_team_name(g_open_play.opponent));
        break;
    case PARK_MSG_WON:
    case PARK_MSG_LOST:
        at = park_append(at, message == PARK_MSG_WON ? "YOU WON " :
            "YOU LOST ");
        at = park_number(at, g_open_play.last_score[0]);
        *at++ = '-';
        at = park_number(at, g_open_play.last_score[1]);
        break;
    default:
        at = park_append(at, "OPEN PLAY  START EXIT");
        break;
    }
    *at = 0;
    hud_center_prompt(PARK_PANEL_ROW, message == PARK_MSG_CALLED ?
        HUD_PAL_YELLOW : HUD_PAL_WHITE, text);
}

/* "3-5" over a court; NEXT between games; YOU for yours. */
static void park_draw_score(u8 c)
{
    const OpenCourt *court = &g_open_play.court[c];
    char text[6];
    char *at = text;
    u8 col = (u8)((open_court_left(c) + OPEN_COURT_W / 2) / 8);

    if (court->state == OPEN_COURT_PLAYING) {
        at = park_number(at, court->score[0]);
        *at++ = '-';
        at = park_number(at, court->score[1]);
    } else {
        at = park_append(at, court->state == OPEN_COURT_YOURS ? "YOU" :
            "NEXT");
    }
    while (at < text + 5)
        *at++ = ' ';
    *at = 0;
    hud_text(col - 2, PARK_SCORE_ROW, HUD_PAL_YELLOW, text);
    s_score_shown[c][0] = court->state;
    s_score_shown[c][1] = court->score[0];
    s_score_shown[c][2] = court->score[1];
}

/*
 * OAM writes cost 816-tcc about three scanlines each, and most park objects
 * stand still from one frame to the next: each entry's last values are
 * kept, and only a change is written. Macros, as a call with this many
 * arguments costs about as much again.
 */
#define PARK_HIDDEN 0x7fff
static s16 s_ox[PARK_OAM_COUNT];
static s16 s_oy[PARK_OAM_COUNT];
static u8 s_ot[PARK_OAM_COUNT];
static u8 s_oa[PARK_OAM_COUNT];
/* g_open_play.lineup when the team palettes were last composed, and
 * .stacks when the racks were last drawn. */
static u8 s_lineup_shown;
static u8 s_stacks_shown;
/* The court whose people and ball move this frame: each court animates on
 * every third frame, a third of the cost (it was most of the frame). */
static u8 s_court_turn;

#define PARK_OBJ(id, x, y, tile, attr) \
    do { \
        if (s_ox[id] != (x) || s_oy[id] != (y) || s_ot[id] != (tile) || \
                s_oa[id] != (attr)) { \
            s_ox[id] = (x); \
            s_oy[id] = (y); \
            s_ot[id] = (tile); \
            s_oa[id] = (attr); \
            obj_put(id, s_ox[id], s_oy[id], tile, attr, 0); \
        } \
    } while (0)

#define PARK_HIDE(id) \
    do { \
        if (s_oy[id] != PARK_HIDDEN) { \
            s_oy[id] = PARK_HIDDEN; \
            render_obj_hide(id); \
        } \
    } while (0)

/* One person (feet at x, y): the top (head and shirt) and the legs. */
#define PARK_PERSON(oam, x, y, palette, step, flip) \
    do { \
        PARK_OBJ(oam, (x) - 4, (y) - 16, OBJTILE_PERSON_TOP, \
            PARK_ATTR(palette, flip)); \
        PARK_OBJ((oam) + 1, (x) - 4, (y) - 8, \
            (step) ? OBJTILE_PERSON_STEP : OBJTILE_PERSON_LEGS, \
            PARK_ATTR(palette, flip)); \
    } while (0)

/* A court's two teams and its rally: the ball goes end to end every 32
 * frames, landing at a new x each time; the players nearest it step. */
static void park_draw_court(u8 c)
{
    const OpenCourt *court = &g_open_play.court[c];
    s16 left = open_court_left(c);
    s16 top_y = PARK_TOP_Y;
    s16 bottom_y = PARK_BOTTOM_Y;
    u8 oam = PARK_OAM_PEOPLE + c * 8;
    u8 phase = court->rally & 31;
    u8 down = (court->rally & 32) == 0;
    s16 ball_x;
    s16 ball_y;
    u8 side;
    u8 p;
    u8 near;
    u8 i;
    u8 walk;
    s16 x;
    s16 y;
    s16 gate_x;
    s16 gate_y;

    /* A new landing spot at each end (once per 32 frames). */
    if (s_ball_end[c] != (u8)(court->rally >> 5)) {
        s_ball_end[c] = (u8)(court->rally >> 5);
        s_ball_x[c] = left + 16 + (s16)((s_ball_end[c] * 13 + c * 7) % 25);
    }
    ball_x = s_ball_x[c];
    /* Walking: off to the gate in the first 64 frames of a changeover,
     * on from it in the first 64 of a game. walk is 0 at the players'
     * spots, 64 at the gate. */
    walk = 0;
    if (court->state == OPEN_COURT_CHANGEOVER) {
        walk = (u8)(OPEN_CHANGEOVER_FRAMES - court->timer);
        if (walk >= OPEN_WALK_FRAMES)
            walk = 0xff;                    /* off: nobody on court */
        else if (!walk)
            walk = 1;
    } else if (court->state == OPEN_COURT_PLAYING &&
            court->age < OPEN_WALK_FRAMES) {
        walk = (u8)(OPEN_WALK_FRAMES - court->age);
    } else if (court->state != OPEN_COURT_PLAYING) {
        walk = 0xff;
    }
    if (walk == 0xff) {
        for (i = 0; i < 8; ++i, ++oam)
            PARK_HIDE(oam);
        PARK_HIDE(PARK_OAM_BALLS + c);
        PARK_HIDE(PARK_OAM_SHADOWS + c);
        return;
    }
    if (walk) {
        gate_x = left + OPEN_COURT_W / 2;
        gate_y = OPEN_COURT_TOP + OPEN_COURT_H + 6;
        for (side = 0; side < 2; ++side) {
            for (p = 0; p < 2; ++p, oam += 2) {
                if (court->playing[side] >= OPPONENT_COUNT) {
                    PARK_HIDE(oam);
                    PARK_HIDE(oam + 1);
                    continue;
                }
                x = left + (p ? 40 : 16);
                y = side ? bottom_y : top_y;
                x += (s16)(((gate_x - x) * (s16)walk) >> 6);
                y += (s16)(((gate_y - y) * (s16)walk) >> 6);
                PARK_PERSON(oam, x, y, PARK_OBJPAL_TEAMS + c * 2 + side,
                    (s_anim >> 3) & 1, p);
            }
        }
        PARK_HIDE(PARK_OAM_BALLS + c);
        PARK_HIDE(PARK_OAM_SHADOWS + c);
        return;
    }
    for (side = 0; side < 2; ++side) {
        /* The receiving side follows the ball. */
        near = down == side;
        for (p = 0; p < 2; ++p) {
            x = left + (p ? 40 : 16);
            if (near && (ball_x < left + OPEN_COURT_W / 2) == !p)
                x = (s16)((u16)(x + ball_x) >> 1);
            y = side ? bottom_y : top_y;
            PARK_PERSON(oam, x, y, PARK_OBJPAL_TEAMS + c * 2 + side,
                near && ((s_anim >> 3) & 1), p);
            oam += 2;
        }
    }
    ball_y = down ? top_y + s_track[phase] : bottom_y - s_track[phase];
    PARK_OBJ(PARK_OAM_SHADOWS + c, ball_x - 4, ball_y - 6,
        OBJTILE_MINI_SHADOW, PARK_ATTR(PARK_OBJPAL_OBJECTS, 0));
    PARK_OBJ(PARK_OAM_BALLS + c, ball_x - 4, ball_y - 10 - s_arc[phase],
        OBJTILE_MINI_BALL, PARK_ATTR(PARK_OBJPAL_OBJECTS, 0));
}

/* The paddles hanging on a court's rack, next first (on the left). */
static void park_draw_rack(u8 c)
{
    const OpenCourt *court = &g_open_play.court[c];
    s16 x = open_rack_x(c) - 8;
    u8 oam = PARK_OAM_PADDLES + c * OPEN_STACK_MAX;
    u8 i;

    for (i = 0; i < OPEN_STACK_MAX; ++i, ++oam, x += 4) {
        if (i < court->stack_count)
            PARK_OBJ(oam, x, OPEN_RACK_Y - 11, OBJTILE_MINI_PADDLE,
                PARK_ATTR(court->stack[i] == OPEN_YOU ? PARK_OBJPAL_YOU :
                    PARK_OBJPAL_OBJECTS, 0));
        else
            PARK_HIDE(oam);
    }
}

/* Team palettes follow who is on court (captains' colours). */
static void park_team_palettes(void)
{
    u8 c;
    u8 side;
    u8 slot;
    u8 entry;
    u8 i;
    const u16 *src;
    u16 *dst;

    /* Only when someone came on or off a court. */
    if (s_lineup_shown == g_open_play.lineup)
        return;
    s_lineup_shown = g_open_play.lineup;
    for (c = 0; c < OPEN_COURTS; ++c) {
        for (side = 0; side < 2; ++side) {
            slot = c * 2 + side;
            entry = g_open_play.court[c].playing[side];
            if (entry == s_team_shown[slot])
                continue;
            s_team_shown[slot] = entry;
            /* Walking pointers: an index per colour cost a multiply each. */
            src = entry < OPPONENT_COUNT ? g_opponent_palettes[entry][0] :
                s_pal_you;
            dst = s_pal_teams[slot];
            for (i = 0; i < 16; ++i)
                *dst++ = *src++;
            s_palettes_dirty = 1;
        }
    }
}

void render_park_prepare(void)
{
    s16 x;
    s16 y;
    u8 message;
    u8 arg;
    u8 c;
    const OpenCourt *court;

    ++s_anim;
    if (s_result_timer)
        --s_result_timer;
    x = FROM_FP(g_open_play.x_fp);
    y = FROM_FP(g_open_play.y_fp);
    PARK_PERSON(PARK_OAM_YOU, x, y, PARK_OBJPAL_YOU,
        g_open_play.walking && ((g_open_play.step >> 3) & 1),
        g_open_play.facing_left);
    /* One heavy job per frame (each is up to ~100 scanlines): new team
     * palettes, a message step, or a court's animation with the racks and
     * a score. A court skips a beat now and then; nobody sees it. */
    if (s_lineup_shown != g_open_play.lineup) {
        park_team_palettes();
        return;
    }
    message = park_message(&arg);
    if (message != s_message || arg != s_message_arg) {
        s_message = message;
        s_message_arg = arg;
        s_message_step = PARK_STEP_CLEAR;
    }
    if (s_message_step < PARK_STEP_DONE) {
        park_draw_message(s_message, s_message_arg);
        ++s_message_step;
        return;
    }
    park_draw_court(s_court_turn);
    if (++s_court_turn >= OPEN_COURTS)
        s_court_turn = 0;
    if (s_stacks_shown != g_open_play.stacks) {
        s_stacks_shown = g_open_play.stacks;
        for (c = 0; c < OPEN_COURTS; ++c)
            park_draw_rack(c);
        return;
    }
    for (c = 0; c < OPEN_COURTS; ++c) {
        court = &g_open_play.court[c];
        if (s_score_shown[c][0] != court->state ||
                s_score_shown[c][1] != court->score[0] ||
                s_score_shown[c][2] != court->score[1]) {
            park_draw_score(c);
            return;
        }
    }
}

void render_park_commit(void)
{
    if (s_palettes_dirty) {
        dmaCopyCGram((u8 *)s_pal_teams, CGRAM_OBJ + PARK_OBJPAL_TEAMS * 16,
            OPEN_COURTS * 2 * 32);
        s_palettes_dirty = 0;
    }
    hud_commit();
}

void render_park_open(void)
{
    u8 i;

    render_fade(1);
    setScreenOff();
    for (i = 0; i < 128; ++i)
        render_obj_hide(i);
    for (i = 0; i < PARK_OAM_COUNT; ++i)
        s_oy[i] = PARK_HIDDEN;
    s_lineup_shown = g_open_play.lineup - 1;
    s_stacks_shown = g_open_play.stacks - 1;
    s_court_turn = 0;
    dmaCopyVram((u8 *)&park_tiles, RENDER_BG1_TILES_ADDR,
        (u16)(&park_tiles_end - &park_tiles));
    dmaCopyVram((u8 *)&park_map, RENDER_BG1_MAP_ADDR,
        (u16)(&park_map_end - &park_map));
    dmaCopyCGram((u8 *)&park_palette, 16,
        (u16)(&park_palette_end - &park_palette));
    match_compose_parts(PAL_BLUE, 0, PADDLE_STANDARD, s_pal_you);
    dmaCopyCGram((u8 *)s_pal_you, CGRAM_OBJ + PARK_OBJPAL_YOU * 16, 32);
    dmaCopyCGram((u8 *)&objects_palette,
        CGRAM_OBJ + PARK_OBJPAL_OBJECTS * 16, 32);
    for (i = 0; i < OPEN_COURTS * 2; ++i)
        s_team_shown[i] = OPEN_NONE - 1;    /* none shown: all redrawn */
    park_team_palettes();
    dmaCopyCGram((u8 *)s_pal_teams, CGRAM_OBJ + PARK_OBJPAL_TEAMS * 16,
        OPEN_COURTS * 2 * 32);
    s_palettes_dirty = 0;
    setColorEffect(0x00, 0x00);
    bgSetDisable(1);

    hud_initialize();
    /* The skill levels, centred over their courts; they never change. */
    for (i = 0; i < OPEN_COURTS; ++i) {
        const char *level = open_court_level_name(i);
        u8 length = 0;

        while (level[length])
            ++length;
        hud_text((u8)((open_court_left(i) + OPEN_COURT_W / 2) / 8 -
            length / 2), PARK_LEVEL_ROW, HUD_PAL_WHITE, level);
    }
    s_message = PARK_MSG_NONE;
    s_big_shown = 0;
    for (i = 0; i < OPEN_COURTS; ++i) {
        s_score_shown[i][0] = 0xff;
        park_draw_score(i);
    }
    s_result_timer = g_open_play.last_won != OPEN_NONE ?
        PARK_RESULT_FRAMES : 0;
    s_anim = 0;
    for (i = 0; i < 32; ++i) {
        s_track[i] = (s16)i * (PARK_BOTTOM_Y - PARK_TOP_Y) / 32;
        s_arc[i] = (s16)i * (32 - (s16)i) / 24;     /* up to 10 px */
    }
    for (i = 0; i < OPEN_COURTS; ++i) {
        s_ball_x[i] = open_court_left(i) + 16;
        s_ball_end[i] = 0xff;
    }
    /* Every court drawn once before the screen shows. */
    for (i = 0; i < OPEN_COURTS; ++i)
        render_park_prepare();
    hud_commit_all();
    render_fade(0);
}
