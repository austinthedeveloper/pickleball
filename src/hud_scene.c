#include <string.h>
#include "game.h"
#include "hud_tiles.h"
#include "input.h"
#include "serve.h"
#include "hud.h"
#include "flair.h"
#include "hud_scene.h"
#include "match.h"
#include "profile.h"
#include "render.h"
#include "render_internal.h"
#include "hud_internal.h"
#include "court_style.h"
#include "audio_events.h"
#include "career.h"
#include "career_page.h"
#include "modifiers.h"
#include "museum.h"
#include "open_play.h"
#include "options.h"
#include "paddle_stats.h"
#include "pause_menu.h"
#include "practice.h"
#include "save.h"
#include "story.h"
#include "text.h"
#include "replay.h"

/*
 * Score panels and scene overlays (title logo and prompt, serve prompt,
 * point and match results, story cards, pause) on BG3. The BG3 map is
 * rebuilt only when what it shows changes, and overlays are drawn in steps
 * across frames (see below). The title area under the logo is
 * src/hud_title.c, with its pages in hud_settings.c and hud_career.c.
 */

/* Last HUD inputs, so the BG3 map is only rebuilt when something changes. */
static u8 s_hud_score[2];
static u16 s_hud_scene;
static u8 s_hud_valid;
static u8 s_sub_panel;             /* 0 none, 1 ARCADE RULES, 2 MATCH POINT */

/* A match's staged steps wait the same way once the frame's work passes
 * this scanline: the heaviest (the point banner) costs about 86, and a
 * doubles rally's contact or point frame already reaches 130-140 with
 * three CPUs thinking (KNOWN_ISSUES #35). A step a frame late shows
 * nothing; a dropped frame does. */
#define HUD_MATCH_SCANLINE_LIMIT 136
/* ...for at most this many frames in a row (a third of a second): forced
 * sooner, a waited step landed on a heavy frame itself. */
#define HUD_MATCH_MAX_WAITS 20
static u8 s_match_waits;


/* The title prompt's rows (BANNER_ROW - 1 and its panel) as last drawn, for
 * TITLE_PAGE_* s_prompt_page (0xff: none yet). Its blink brings it back
 * every 64 frames; redrawing cost about 100 scanlines, which the doubles
 * demo rarely had, so it was forced onto a heavy frame after
 * HUD_TITLE_MAX_WAITS: most of its dropped frames (M4-UI-03). */
#define HUD_PROMPT_ROWS 4
static u16 s_prompt_rows[HUD_PROMPT_ROWS * 32];
static u8 s_prompt_page;
/* A new prompt's panel is up and its text comes next (EXTRA2, EXTRA3). */
static u8 s_prompt_text;

/* Score-panel labels: at most 4 characters before the score digits. */
static const char *const s_palette_names[PAL_COUNT] = {
    "BLUE", "RED", "GRN", "GOLD"
};

static u8 hud_doubles(void)
{
    return g_match_config.mode == MATCH_DOUBLES;
}

static u8 hud_versus_2p(void)
{
    return g_match_config.human[2];
}

static u8 hud_opponent_team(u8 team)
{
    return team && g_match_config.opponent != OPPONENT_NONE;
}

static u8 hud_practice(void)
{
    return g_match_config.practice && !g_attract;
}

/* The active career player's name for the score panel: four cells. */
static const char *hud_career_short_name(void)
{
    static char name[5];
    const CareerPlayer *player = career_active();
    u8 i;

    for (i = 0; i < 4; ++i)
        name[i] = (char)player->name[i];
    name[4] = 0;
    while (i && name[i - 1] == ' ')
        name[--i] = 0;
    return name;
}

/* Singles: P1 / CPU (or P2). Doubles: the team's palette name
 * (ADR-010/018), or CPU for a ladder opponent (ADR-023). */
static const char *team_name(u8 team)
{
    /* Practice (ADR-028): P1's streak and the best streak. */
    if (hud_practice())
        return team ? "BEST" : "P1";
    /* ADR-047: the career player's name, its first four letters. */
    if (team == 0 && g_match_config.career_p1 && career_active())
        return hud_career_short_name();
    if (hud_doubles() && !hud_opponent_team(team))
        return s_palette_names[match_team_palette(team)];
    if (team)
        return hud_versus_2p() ? "P2" : "CPU";
    return "P1";
}

void hud_scene_invalidate(void)
{
    s_hud_valid = 0;
    /* No previous scene: the staged redraw starts from scratch (a title
     * logo is redrawn, not assumed to be on screen). */
    s_hud_scene = 0xff;
    hud_title_area_forget();
}

void hud_scene_refresh_title(void)
{
    hud_title_area_forget();
}

static const char *fault_word(u8 fault)
{
    switch (fault) {
    case GAME_FAULT_NET:
        return "NET!";
    case GAME_FAULT_KITCHEN:
        return "KITCHEN!";
    case GAME_FAULT_WRONG_BOX:
        return "FAULT!";
    case GAME_FAULT_OUT:
        return "OUT!";
    case GAME_FAULT_SECOND_BOUNCE:
        return "2 BOUNCES";
    case GAME_FAULT_EARLY_VOLLEY:
        return "VOLLEY!";
    case GAME_FAULT_LAVA:
        return "LAVA!";
    default:
        return "POINT!";
    }
}

static u8 team_hud_palette(u8 team)
{
    return team ? HUD_PAL_TEAM_RIGHT : HUD_PAL_TEAM_LEFT;
}

/* Score digits only; tens are drawn as a leading digit or a blank. The
 * title's demo keeps no scoreboard (ADR-065). */
static void hud_draw_score_digits(void)
{
    static const char digits[] = "0123456789";
    char text[3];
    u8 team;
    u8 score;

    for (team = 0; g_attract == 0 && team < 2; ++team) {
        score = g_game_state.team_score[team];
        text[0] = ' ';
        while (score >= 10) {      /* no 816-tcc division */
            score -= 10;
            text[0] = text[0] == ' ' ? '1' : text[0] + 1;
        }
        text[1] = digits[score];
        text[2] = 0;
        hud_text(team ? RIGHT_SCORE_COL : LEFT_SCORE_COL, SCORE_TEXT_ROW,
            HUD_PAL_WHITE, text);
    }
    s_hud_score[0] = g_game_state.team_score[0];
    s_hud_score[1] = g_game_state.team_score[1];
}

/* "VS NAME" centred in a 12-cell panel interior (ADR-023): singles names
 * the captain, doubles the team. */
static void hud_versus(u8 row)
{
    const char *name = hud_doubles() ?
        g_opponent_team[g_match_config.opponent] :
        g_opponent_captain[g_match_config.opponent];
    u8 length = 0;
    u8 col;

    while (name[length])
        ++length;
    col = CENTER_PANEL_COL + 1 + (CENTER_PANEL_WIDTH - 3 - length) / 2;
    hud_text(col, row, HUD_PAL_WHITE, "VS");
    hud_text(col + 3, row, HUD_PAL_TEAM_RIGHT, name);
}

/* Practice: PRACTICE, or the return zone the streak counts (centred). */
static void hud_practice_title(void)
{
    const char *name = practice_zone_name(g_practice.zone);
    u8 length = 0;

    if (g_practice.zone == PRACTICE_ZONE_ANY) {
        hud_text(CENTER_PANEL_COL + 1, SCORE_TEXT_ROW, HUD_PAL_YELLOW,
            "  PRACTICE");
        return;
    }
    while (name[length])
        ++length;
    hud_text(CENTER_PANEL_COL + 1 + (CENTER_PANEL_WIDTH - 2 - length) / 2,
        SCORE_TEXT_ROW, HUD_PAL_YELLOW, name);
}

/* Panels and labels never change during a match; drawn on full redraws. */
static void hud_draw_scores(void)
{
    hud_clear_rows(SCORE_PANEL_ROW, 3);
    /* The title (ADR-065): the demo behind the menu has no scoreboard; the
     * join counter takes its place (ADR-066). */
    if (g_attract) {
        s_sub_panel = 0xff;
        hud_title_join_forget();
        hud_draw_score_digits();
        return;
    }
    hud_panel(LEFT_PANEL_COL, SCORE_PANEL_ROW, SIDE_PANEL_WIDTH);
    hud_text(LEFT_PANEL_COL + 1, SCORE_TEXT_ROW, team_hud_palette(0),
        team_name(0));
    hud_panel(CENTER_PANEL_COL, SCORE_PANEL_ROW, CENTER_PANEL_WIDTH);
    if (hud_practice())
        hud_practice_title();
    else if (!g_attract && !hud_doubles() && hud_opponent_team(1))
        hud_versus(SCORE_TEXT_ROW);
    else
        hud_text(CENTER_PANEL_COL + 1, SCORE_TEXT_ROW, HUD_PAL_YELLOW,
            hud_doubles() ? "  DOUBLES" : "ARCADE RULES");
    hud_panel(RIGHT_PANEL_COL, SCORE_PANEL_ROW, SIDE_PANEL_WIDTH);
    hud_text(RIGHT_PANEL_COL + 1, SCORE_TEXT_ROW, team_hud_palette(1),
        team_name(1));
    s_sub_panel = 0xff;     /* re-evaluate the sub-panel */
    hud_draw_score_digits();
}

/*
 * Scene keys: what the overlay currently shows. A key change starts a
 * staged redraw. Game phases use their own value (with winner/fault bits for
 * results); these special keys sit above that range.
 */
#define HUD_KEY_PAUSED 0x80        /* | the menu choice (ADR-030) */
#define HUD_IS_PAUSED(k) (((k) & ~3u) == HUD_KEY_PAUSED)
#define PAUSE_BOX_COL 9
#define PAUSE_BOX_ROW 9             /* header, gap, 3 items: rows 9-13 */
#define PAUSE_BOX_WIDTH 14
#define HUD_KEY_TITLE 0x100
#define HUD_KEY_TITLE_BLINK 0x101
#define HUD_KEY_INTRO 0x200         /* the story intro card (ADR-026) */
#define HUD_KEY_PRACTICE 0x400      /* a practice session (ADR-028); not bit 8 */
#define HUD_KEY_INITIALS 0x800      /* the initials card (ADR-040) */
#define HUD_KEY_UNLOCK 0x1000       /* the career unlock card (ADR-047) */
/* The initials card: three big letters on rows 16-17, four columns apart
 * and centred, the chosen one underlined on row 18. */
#define INITIALS_ROW 16
#define INITIALS_COL 11
#define INITIALS_STEP 4
#define INITIALS_LAST_ROW (INITIALS_ROW + 2)

/* WIN BY 2 (ADR-033): one more point wins from a lead. */
static u8 hud_win_by_2_match_point(void)
{
    u8 left = g_game_state.team_score[0];
    u8 right = g_game_state.team_score[1];
    u8 points = g_match_config.points_to_win;

    return (left + 1 >= points && left > right) ||
        (right + 1 >= points && right > left);
}

/* Doubles shows ARCADE RULES under DOUBLES; either mode shows MATCH POINT
 * when a team is one point from winning (the doubles mockup). Runs every
 * match frame, close to the budget: without WIN BY 2 it is the original
 * test plus one bit. */
static u8 hud_sub_panel_wanted(void)
{
    if (g_attract || g_match_config.practice)
        return 0;
    if (g_game_state.phase != GAME_PHASE_MATCH_RESULT &&
        ((g_match_config.modifiers & MODIFIER_WIN_BY_2) ?
            hud_win_by_2_match_point() :
            (g_game_state.team_score[0] == g_match_config.points_to_win - 1 ||
             g_game_state.team_score[1] == g_match_config.points_to_win - 1)))
        return 2;
    return hud_doubles() ? 1 : 0;
}

static void hud_draw_sub_panel(u8 wanted)
{
    hud_clear_rows(SUB_PANEL_ROW, 3);
    if (wanted == 1) {
        hud_panel(CENTER_PANEL_COL, SUB_PANEL_ROW, CENTER_PANEL_WIDTH);
        if (hud_opponent_team(1))
            hud_versus(SUB_PANEL_ROW + 1);
        else
            hud_text(CENTER_PANEL_COL + 1, SUB_PANEL_ROW + 1, HUD_PAL_WHITE,
                "ARCADE RULES");
    } else if (wanted == 2) {
        hud_panel(CENTER_PANEL_COL, SUB_PANEL_ROW, CENTER_PANEL_WIDTH);
        hud_text(CENTER_PANEL_COL + 1, SUB_PANEL_ROW + 1, HUD_PAL_WHITE,
            "MATCH POINT");
    }
    s_sub_panel = wanted;
}

static const char *hud_point_text(u8 winner)
{
    static const char *const doubles[PAL_COUNT] = {
        "BLUE POINT", "RED POINT", "GREEN POINT", "GOLD POINT"
    };
    if (hud_doubles() && !hud_opponent_team(winner))
        return doubles[match_team_palette(winner)];
    if (winner == 0)
        return "P1 POINT";
    return hud_versus_2p() ? "P2 POINT" : "CPU POINT";
}

static const char *hud_match_winner_text(void)
{
    /* ADR-026: beating the last ladder opponent. */
    if (g_game_state.match_winner == 0 && !g_attract &&
            !g_match_config.open_play &&
            g_match_config.opponent == OPPONENT_COUNT - 1)
        return "CHAMPION!";
    if (hud_versus_2p())
        return g_game_state.match_winner == 0 ? "P1 WINS" : "P2 WINS";
    return g_game_state.match_winner == 0 ? "YOU WIN!" : "CPU WINS";
}

/* The title prompt (bits 1-4 of a title key): the page, or for the
 * CAREER page (ADR-047) its sub-view, so each has its own. */
#define HUD_PROMPT_CAREER 8
#define HUD_PROMPT_CAREER_NAMING 12
#define HUD_PROMPT_CAREER_NAMING_EMPTY 13   /* nothing to erase: Y backs out */
static u8 hud_title_prompt_id(void)
{
    if (g_title_page != TITLE_PAGE_CAREER)
        return g_title_page;
    if (g_career_page.view == CAREER_VIEW_EDIT && g_career_page.naming)
        return g_career_page.cursor ? HUD_PROMPT_CAREER_NAMING :
            HUD_PROMPT_CAREER_NAMING_EMPTY;
    return HUD_PROMPT_CAREER + g_career_page.view;
}

static u16 hud_scene_key(void)
{
    u8 phase = g_game_state.phase;

    if (g_story.card == STORY_CARD_INTRO)
        return HUD_KEY_INTRO;
    if (g_story.card == STORY_CARD_INITIALS)
        return HUD_KEY_INITIALS;
    if (g_story.card == STORY_CARD_UNLOCK)
        return HUD_KEY_UNLOCK;
    if (g_game_state.paused && !g_attract)
        return HUD_KEY_PAUSED | g_pause_choice;
    if (hud_practice())
        return HUD_KEY_PRACTICE;

    if (g_attract)
        return ((g_game_state.frame_count & 32) ? HUD_KEY_TITLE_BLINK :
            HUD_KEY_TITLE) | (hud_title_prompt_id() << 1);
    if (phase == GAME_PHASE_SERVE_TOSS || phase == GAME_PHASE_SERVE_FAULT)
        phase = GAME_PHASE_LIVE;
    if (phase == GAME_PHASE_SERVE_SETUP)
        return phase | (g_serve_state.server_index << 4);
    if (phase == GAME_PHASE_POINT_RESULT || phase == GAME_PHASE_MATCH_RESULT)
        return phase | (g_game_state.point_winner << 4) |
            ((g_game_state.last_fault & 7) << 5);
    return phase;
}

/* Title keys: 0x100 | blink (bit 0) | page (bits 1-3, for its prompt).
 * Bit 8 keeps them apart from phase keys: a kitchen fault point for P1 is
 * 0x44. The menu choice and 2P are drawn by hud_title_area_update. */
#define HUD_IS_TITLE(k) (((k) & HUD_KEY_TITLE) != 0)

/*
 * Scene overlays are drawn in three steps on consecutive frames (clear, big
 * text, prompt). Doing all three on the frame a point ends made that one
 * frame overrun; spread out, no frame does more than one step.
 */
enum { HUD_STEP_CLEAR = 0, HUD_STEP_BIG = 1, HUD_STEP_PROMPT = 2,
       /* Story cards (intro, match result): one panel per frame. */
       HUD_STEP_EXTRA = 3, HUD_STEP_EXTRA2 = 4,
       /* The match result only: a panel and its text on separate frames
        * (other scenes go from EXTRA2 to DONE). */
       HUD_STEP_EXTRA3 = 5, HUD_STEP_EXTRA4 = 6, HUD_STEP_EXTRA5 = 7,
       HUD_STEP_EXTRA6 = 8, HUD_STEP_DONE = 9 };

static u8 s_scene_step;
static u16 s_prev_scene;
/* The close call's frame is up (rows 14-23, ADR-041): the next clear takes
 * those rows too. */
static u8 s_replay_drawn;
/* The point result's quip (story_quip), or 0, and whether its panel is up
 * (the next clear covers its rows). */
static const char *s_quip;
static u8 s_quip_drawn;
#define QUIP_PANEL_ROW (CENTER_PANEL_ROW + 3)

u8 hud_replay_framed(void)
{
    return s_replay_drawn;
}

/* The opponent speaking on the story cards: singles names the captain,
 * doubles the team. */
static const char *hud_opponent_name(void)
{
    return hud_doubles() ? g_opponent_team[g_match_config.opponent] :
        g_opponent_captain[g_match_config.opponent];
}

/* The records just beaten: "BEST STREAK 5", "LONGEST RALLY 12", or both
 * as "STREAK 5  RALLY 12". */
static const char *hud_new_records_text(void)
{
    static char text[29];
    char *end = text;
    u8 both = g_save_new_records == (SAVE_NEW_STREAK | SAVE_NEW_RALLY);

    if (g_save_new_records & SAVE_NEW_STREAK) {
        end = text_append(end, both ? "STREAK " : "BEST STREAK ");
        end = text_count(end, g_save.records.best_streak);
        if (both)
            end = text_append(end, "  ");
    }
    if (g_save_new_records & SAVE_NEW_RALLY) {
        end = text_append(end, both ? "RALLY " : "LONGEST RALLY ");
        text_count(end, g_save.records.longest_rally);
    }
    return text;
}

/* The charged serve's meter (ADR-044): a bar over the serve prompt's
 * text, one cell for two levels. The first cell is the dud zone, white;
 * the rest fill yellow. */
#define SERVE_PROMPT_CHARGED "HOLD @ TO SERVE"
#define METER_CELLS 15
#define METER_COL (16 - (METER_CELLS + 2) / 2 + 1)
#define METER_UNDRAWN 0xff
static u8 s_meter_cells;            /* cells lit as drawn; METER_UNDRAWN */

/* Lights or clears the cells that changed; nonzero if it drew. On idle
 * frames: one cell a frame once the bar is up. */
static u8 hud_serve_meter_refresh(void)
{
    u8 cells = g_serve_charge.level >> 1;
    u8 i;
    u8 from;
    u8 to;

    if (!g_serve_charge.charging || cells == s_meter_cells)
        return 0;
    if (s_meter_cells == METER_UNDRAWN) {
        from = 0;
        to = METER_CELLS;
    } else if (cells > s_meter_cells) {
        from = s_meter_cells;
        to = cells;
    } else {
        from = cells;
        to = s_meter_cells;
    }
    for (i = from; i < to; ++i)
        hud_tile(METER_COL + i, BANNER_ROW + 1, i >= cells ?
            hud_entry(HUD_TILE_BAR_OFF, HUD_PAL_WHITE) :
            hud_entry(HUD_TILE_BAR_ON,
                i < (SERVE_CHARGE_DUD >> 1) ? HUD_PAL_WHITE : HUD_PAL_YELLOW));
    s_meter_cells = cells;
    return 1;
}

/* Letters and cursor as drawn; 0 forces a redraw. */
static u8 s_initials_drawn[3];
static u8 s_initials_cursor;

/* The letters whose letter or underline changed: on idle frames. */
static void hud_initials_refresh(void)
{
    static char letter[2];
    u8 i;
    u8 col;
    u8 chosen;
    u8 was = s_initials_cursor;

    for (i = 0; i < 3; ++i) {
        chosen = g_story.cursor == i;
        if (s_initials_drawn[i] == g_story.letters[i] && (was == i) == chosen)
            continue;
        col = INITIALS_COL + i * INITIALS_STEP;
        /* Statics are not zeroed at power-on: terminate it here. */
        letter[0] = (char)g_story.letters[i];
        letter[1] = 0;
        hud_big_text(col, INITIALS_ROW, chosen ? HUD_PAL_YELLOW :
            HUD_PAL_WHITE, letter);
        hud_tile(col, INITIALS_LAST_ROW, chosen ?
            hud_entry(HUD_TILE_BAR_ON, HUD_PAL_YELLOW) : HUD_TILE_CLEAR);
        hud_tile(col + 1, INITIALS_LAST_ROW, chosen ?
            hud_entry(HUD_TILE_BAR_ON, HUD_PAL_YELLOW) : HUD_TILE_CLEAR);
        s_initials_drawn[i] = g_story.letters[i];
    }
    s_initials_cursor = g_story.cursor;
}

/* Initials card (ADR-040): NEW RECORD!, the records, and the prompt, one
 * per frame; the letters draw on the idle frames after. */
static void hud_initials_step(u8 step)
{
    if (step == HUD_STEP_BIG) {
        hud_center_big(CENTER_BIG_ROW, HUD_PAL_YELLOW, "NEW RECORD!");
    } else if (step == HUD_STEP_PROMPT) {
        hud_center_prompt(CENTER_PANEL_ROW, HUD_PAL_WHITE,
            hud_new_records_text());
    } else if (step == HUD_STEP_EXTRA) {
        hud_center_prompt(BANNER_ROW, HUD_PAL_WHITE,
            "UP DOWN  @ NEXT  START OK");
    }
}

/* The close call (ADR-041): a frame around the BG2 close-up, then the
 * caption under it ("OUT BY 2 INCHES!"). */
static void hud_replay_step(u8 step)
{
    static char caption[20];
    char *end;

    if (step == HUD_STEP_EXTRA) {
        hud_frame(REPLAY_COL - 1, REPLAY_ROW - 1, REPLAY_COLS, REPLAY_ROWS);
        s_replay_drawn = 1;
    } else if (step == HUD_STEP_EXTRA2) {
        if (!g_replay.inches) {
            text_append(caption, "OUT BY A HAIR!");
        } else {
            end = text_append(caption, "OUT BY ");
            end = text_count(end, g_replay.inches);
            text_append(end, g_replay.inches == 1 ? " INCH!" : " INCHES!");
        }
        hud_center_text(REPLAY_ROW + REPLAY_ROWS + 1, HUD_PAL_YELLOW, caption);
    }
}

/* Intro card (ADR-026): the opponent's name, their line, the prompt, and
 * the rung, one per frame. */
static void hud_intro_step(u8 step)
{
    static char rung[] = "LADDER 0 OF 0";

    if (step == HUD_STEP_BIG) {
        hud_center_big(CENTER_BIG_ROW, HUD_PAL_TEAM_RIGHT, hud_opponent_name());
    } else if (step == HUD_STEP_PROMPT) {
        hud_center_prompt(CENTER_PANEL_ROW, HUD_PAL_WHITE,
            g_opponent_lines[g_match_config.opponent][OPPONENT_LINE_INTRO]);
    } else if (step == HUD_STEP_EXTRA) {
        hud_center_prompt(BANNER_ROW, HUD_PAL_WHITE, "@ PLAY");
    } else if (step == HUD_STEP_EXTRA2) {
        rung[7] = (char)('1' + g_match_config.opponent);
        rung[12] = (char)('0' + OPPONENT_COUNT);
        hud_center_text(CENTER_PANEL_ROW + 3, HUD_PAL_YELLOW, rung);
    }
}

/* The unlocks just earned (ADR-047): "VISOR, PINK HAIR", and " +n" for
 * any that don't fit the panel. */
static const char *hud_unlock_text(void)
{
    static char text[29];
    u16 bit = 1;
    u8 length = 0;
    u8 more = 0;
    u8 i;
    const char *name;
    u8 size;

    for (i = 0; i < CAREER_UNLOCKS; ++i, bit <<= 1) {
        if ((g_career_new_unlocks & bit) == 0)
            continue;
        name = g_career_unlock_names[i];
        for (size = 0; name[size]; ++size)
            ;
        if (length + (length ? 2 : 0) + size > 24) {
            ++more;
            continue;
        }
        if (length) {
            text[length++] = ',';
            text[length++] = ' ';
        }
        while (*name)
            text[length++] = *name++;
    }
    if (more) {
        text[length++] = ' ';
        text[length++] = '+';
        text[length++] = (char)('0' + more);
    }
    text[length] = 0;
    return text;
}

static void hud_unlock_step(u8 step)
{
    if (step == HUD_STEP_BIG)
        hud_center_big(CENTER_BIG_ROW, HUD_PAL_YELLOW, "UNLOCKED!");
    else if (step == HUD_STEP_PROMPT)
        hud_center_prompt(CENTER_PANEL_ROW, HUD_PAL_WHITE, hud_unlock_text());
    else if (step == HUD_STEP_EXTRA)
        hud_center_prompt(BANNER_ROW, HUD_PAL_WHITE, "@ OK");
}

/* Under the match result (ADR-026): the opponent's line (EXTRA), and
 * after a win the next opponent or the ladder cleared (EXTRA2). */
static const char *hud_result_prompt(void)
{
    if (g_match_config.open_play)
        return "@ PARK  START MENU";
    if (g_game_state.match_winner == 0 &&
            g_match_config.opponent != OPPONENT_NONE)
        return "@ NEXT  START MENU";
    return "@ REMATCH  START MENU";
}

/* After a rated park game (ADR-070): "RATED 3.4 TO 3.6", or a court it
 * opened, "3.5-4.5 OPEN! RATED 3.6". */
static void hud_result_rating(void)
{
    static char text[29];
    char *at = text;

    if (g_career_park_before == OPEN_RATING_NONE)
        return;
    if (g_career_park_opened != OPEN_NONE) {
        at = text_append(at, open_court_level_name(g_career_park_opened));
        at = text_append(at, " OPEN! RATED ");
    } else {
        at = text_append(at, "RATED ");
        at = text_tenths(at, g_career_park_before);
        at = text_append(at, " TO ");
    }
    text_tenths(at, g_career_park_after);
    hud_center_text(CENTER_PANEL_ROW + 3, HUD_PAL_YELLOW, text);
}

static void hud_result_story(u8 step)
{
    static char next[] = "NEXT UP: 123456789";
    u8 won = g_game_state.match_winner == 0;
    const char *const *lines;
    const char *team;
    u8 i;

    if (g_match_config.opponent == OPPONENT_NONE)
        return;
    if (step <= HUD_STEP_EXTRA5) {
        /* Separate steps: 816-tcc fails (ICE 2) on a ternary as the index. */
        lines = g_opponent_lines[g_match_config.opponent];
        if (won)
            lines += OPPONENT_LINE_CONCEDE;
        else
            lines += OPPONENT_LINE_TAUNT;
        /* The panel, then the line in halves: each half is a frame. */
        if (step == HUD_STEP_EXTRA3)
            hud_center_prompt_panel(CENTER_PANEL_ROW, *lines);
        else
            hud_center_prompt_half(CENTER_PANEL_ROW, HUD_PAL_TEAM_RIGHT,
                *lines, step == HUD_STEP_EXTRA5);
        return;
    }
    /* Open play (ADR-034) is off the ladder: no next-up line, but the
     * park rating's move (ADR-070). */
    if (g_match_config.open_play) {
        hud_result_rating();
        return;
    }
    if (!won)
        return;
    if (g_match_config.opponent == OPPONENT_COUNT - 1) {
        hud_center_text(CENTER_PANEL_ROW + 3, HUD_PAL_YELLOW,
            "LADDER CLEARED!");
        return;
    }
    team = g_opponent_team[save_ladder(g_match_config.mode)];
    for (i = 0; team[i]; ++i)
        next[9 + i] = team[i];
    next[9 + i] = 0;
    hud_center_text(CENTER_PANEL_ROW + 3, HUD_PAL_YELLOW, next);
}


/* The scanline a title step waits under: the logo steps between two title
 * views draw nothing; every other step (a logo row, a clear, a prompt's
 * panel or half its text, a restored prompt) fits an ordinary frame. A new
 * prompt in one step (about 106 scanlines) needed a light frame, waited
 * them out on the singles demo, and dropped one (court.timeline). */
static u16 hud_title_step_limit(u8 step)
{
    if (step == HUD_STEP_BIG || step == HUD_STEP_EXTRA)
        return HUD_IS_TITLE(s_prev_scene) ? 0xffff : HUD_TITLE_SCANLINE_LIMIT;
    return HUD_TITLE_SCANLINE_LIMIT;
}

static const char *hud_title_prompt(u8 page)
{
    /* The CAREER page's prompts (hud_title_prompt_id). */
    if (page == HUD_PROMPT_CAREER + CAREER_VIEW_SLOTS)
        return "@ OPEN  Y BACK";
    if (page == HUD_PROMPT_CAREER + CAREER_VIEW_CARD)
        return "@ EDIT  X HATS  Y BACK";
    if (page == HUD_PROMPT_CAREER + CAREER_VIEW_EDIT)
        return "@ OK  Y CANCEL";
    if (page == HUD_PROMPT_CAREER_NAMING)
        return "@ TYPE  Y ERASE";
    if (page == HUD_PROMPT_CAREER_NAMING_EMPTY)
        return "@ TYPE  Y BACK";
    if (page == TITLE_PAGE_COURT || page == TITLE_PAGE_OPTIONS ||
            page == TITLE_PAGE_MODIFIERS)
        return "@ OK  Y CANCEL";
    if (page == TITLE_PAGE_PRACTICE)
        return "@ PLAY  Y BACK";
    if (page == HUD_PROMPT_CAREER + CAREER_VIEW_RECORDS)
        return "@ BACK";
    return "@ SELECT";
}

/*
 * Title steps: CLEAR wipes the match overlays (coming from a match), BIG
 * and then EXTRA draw the logo's two rows, and PROMPT the blinking prompt.
 * Only the prompt blinks; the logo stays. The logo in one step (about 100
 * scanlines) fit only a light frame, which the demos rarely have; forced
 * onto an ordinary one after HUD_TITLE_MAX_WAITS, it dropped a frame.
 */
static void hud_title_step(u16 key, u8 step, u8 from_title)
{
    switch (step) {
    case HUD_STEP_CLEAR:
        /* From the sub-panel down: a match's VS or MATCH POINT panel
         * (rows 4-6) never redraws on the title (KNOWN_ISSUES #31). */
        if (!from_title) {
            hud_clear_rows(SUB_PANEL_ROW,
                CENTER_BIG_ROW - 2 - SUB_PANEL_ROW + 6 + TITLE_AREA_ROWS);
            s_sub_panel = 0;
            hud_title_area_cleared();
        }
        break;
    case HUD_STEP_BIG:
        if (!from_title)
            hud_center_big(TITLE_ROW, HUD_PAL_YELLOW, "PICKLEBALL");
        break;
    case HUD_STEP_EXTRA:
        if (!from_title)
            hud_center_big(TITLE_ROW + 2, HUD_PAL_YELLOW, "'94");
        break;
    case HUD_STEP_PROMPT:
        s_prompt_text = 0;
        if (!(key & 1) && s_prompt_page == ((key >> 1) & 15)) {
            hud_restore_rows(BANNER_ROW - 1, HUD_PROMPT_ROWS, s_prompt_rows);
            break;
        }
        hud_clear_rows(BANNER_ROW - 1, HUD_PROMPT_ROWS);
        if (!(key & 1)) {
            hud_center_prompt_panel(BANNER_ROW,
                hud_title_prompt((key >> 1) & 15));
            s_prompt_text = 1;
        }
        break;
    /* A new prompt's text, in halves on the next two steps; then it is
     * kept for the blink. */
    case HUD_STEP_EXTRA2:
    case HUD_STEP_EXTRA3:
        hud_center_prompt_half(BANNER_ROW, HUD_PAL_WHITE,
            hud_title_prompt((key >> 1) & 15), step == HUD_STEP_EXTRA3);
        if (step == HUD_STEP_EXTRA3) {
            hud_save_rows(BANNER_ROW - 1, HUD_PROMPT_ROWS, s_prompt_rows);
            s_prompt_page = (key >> 1) & 15;
            s_prompt_text = 0;
        }
        break;
    default:
        break;
    }
}

/* Pause menu (ADR-030): a box with PAUSED and the three choices. */
static void hud_pause_step(u8 step)
{
    u8 i;
    u8 on;

    if (step == HUD_STEP_PROMPT) {
        hud_fill_box(PAUSE_BOX_COL, PAUSE_BOX_ROW, PAUSE_BOX_WIDTH, 5,
            hud_entry(' ', HUD_PAL_WHITE));
        hud_text(PAUSE_BOX_COL + 4, PAUSE_BOX_ROW, HUD_PAL_YELLOW, "PAUSED");
    } else if (step == HUD_STEP_EXTRA) {
        for (i = 0; i < PAUSE_CHOICES; ++i) {
            on = i == g_pause_choice;
            hud_tile(PAUSE_BOX_COL + 1, PAUSE_BOX_ROW + 2 + i, on ?
                hud_entry(HUD_TILE_CURSOR, HUD_PAL_YELLOW) :
                hud_entry(' ', HUD_PAL_WHITE));
            hud_text(PAUSE_BOX_COL + 3, PAUSE_BOX_ROW + 2 + i,
                on ? HUD_PAL_YELLOW : HUD_PAL_WHITE, pause_choice_name(i));
        }
    }
}

static void hud_scene_step(u16 key, u8 step)
{
    u8 winner = g_game_state.point_winner;
    u8 from_title = HUD_IS_TITLE(s_prev_scene);

    if (HUD_IS_TITLE(key)) {
        hud_title_step(key, step, from_title);
        return;
    }

    if (step == HUD_STEP_CLEAR) {
        /* The initials card also uses rows 15-18 (ADR-040). */
        hud_clear_rows(from_title ? CENTER_BIG_ROW - 2 : CENTER_BIG_ROW,
            from_title ? 6 + TITLE_AREA_ROWS : s_replay_drawn ?
                REPLAY_ROW + REPLAY_ROWS + 1 - CENTER_BIG_ROW :
            (key == HUD_KEY_INITIALS || s_prev_scene == HUD_KEY_INITIALS) ?
                INITIALS_LAST_ROW + 1 - CENTER_BIG_ROW :
            s_quip_drawn ? QUIP_PANEL_ROW + 3 - CENTER_BIG_ROW : 7);
        s_replay_drawn = 0;
        s_quip_drawn = 0;
        hud_clear_rows(BANNER_ROW - 1, 4);
        s_initials_drawn[0] = 0;
        s_initials_drawn[1] = 0;
        s_initials_drawn[2] = 0;
        s_initials_cursor = 0xff;
        return;
    }
    if (key == HUD_KEY_INTRO) {
        hud_intro_step(step);
        return;
    }
    if (key == HUD_KEY_INITIALS) {
        hud_initials_step(step);
        return;
    }
    if (key == HUD_KEY_UNLOCK) {
        hud_unlock_step(step);
        return;
    }
    if (key == HUD_KEY_PRACTICE) {
        if (step == HUD_STEP_PROMPT)
            hud_center_prompt(BANNER_ROW, HUD_PAL_WHITE, "START  PAUSE");
        return;
    }
    if (HUD_IS_PAUSED(key)) {
        hud_pause_step(step);
        return;
    }
    if (g_game_state.phase == GAME_PHASE_MATCH_RESULT &&
            step >= HUD_STEP_PROMPT) {
        /* The prompt's panel, then its text in halves, then the story
         * card (hud_result_story): each on its own frame. Together they
         * overran the doubles result frame. */
        if (step == HUD_STEP_PROMPT)
            hud_center_prompt_panel(BANNER_ROW, hud_result_prompt());
        else if (step <= HUD_STEP_EXTRA2)
            hud_center_prompt_half(BANNER_ROW, HUD_PAL_WHITE,
                hud_result_prompt(), step == HUD_STEP_EXTRA2);
        else if (!g_attract)
            hud_result_story(step);
        return;
    }
    if (g_game_state.phase == GAME_PHASE_POINT_RESULT &&
            step >= HUD_STEP_PROMPT) {
        /* The point's panel, then its text in halves, each on its own
         * frame: drawn whole on the frame after the big banner, with the
         * score, it was the doubles budget's worst frame (as the match
         * result's was, M4-UI-04). Then the close call's frame and caption
         * (ADR-041). */
        if (step == HUD_STEP_PROMPT) {
            hud_center_prompt_panel(CENTER_PANEL_ROW, hud_point_text(winner));
            /* A quip from the side that lost it, now and then; never over
             * a close call's replay (GOOFY_IDEAS.md). */
            s_quip = (g_replay.active && !g_attract) ? 0 :
                story_quip(winner ^ 1, g_game_state.frame_count);
            /* ADR-064: and its players' faces say how it went. */
            if (s_quip)
                FLAIR_QUIP(winner ^ 1);
        } else if (step <= HUD_STEP_EXTRA2) {
            hud_center_prompt_half(CENTER_PANEL_ROW, team_hud_palette(winner),
                hud_point_text(winner), step == HUD_STEP_EXTRA2);
        } else if (g_replay.active && !g_attract) {
            hud_replay_step(step - (HUD_STEP_EXTRA3 - HUD_STEP_EXTRA));
        } else if (step == HUD_STEP_EXTRA3 && s_quip) {
            /* The quip's panel under the point's, then its text. */
            hud_center_prompt_panel(QUIP_PANEL_ROW, s_quip);
            s_quip_drawn = 1;
        } else if (step >= HUD_STEP_EXTRA4 && s_quip) {
            /* Its text in halves on two frames, as the point's: both on
             * one frame overran a doubles point result (ADR-069). */
            hud_center_prompt_half(QUIP_PANEL_ROW, team_hud_palette(winner ^ 1),
                s_quip, step == HUD_STEP_EXTRA5);
        }
        return;
    }
    if (step >= HUD_STEP_EXTRA)
        return;
    switch (g_game_state.phase) {
    case GAME_PHASE_SERVE_SETUP:
        if (step != HUD_STEP_PROMPT)
            break;
        s_meter_cells = METER_UNDRAWN;
        if (serve_charged())
            hud_center_prompt(BANNER_ROW, HUD_PAL_WHITE,
                SERVE_PROMPT_CHARGED);
        else if (g_input_owners[g_serve_state.server_index] ==
                INPUT_OWNER_HUMAN)
            hud_center_prompt(BANNER_ROW, HUD_PAL_WHITE, "PRESS @ TO SERVE");
        else
            hud_center_prompt(BANNER_ROW, HUD_PAL_WHITE, "CPU TO SERVE");
        break;
    case GAME_PHASE_POINT_RESULT:
        if (step == HUD_STEP_BIG)
            hud_center_big(CENTER_BIG_ROW, HUD_PAL_YELLOW,
                fault_word(g_game_state.last_fault));
        break;
    case GAME_PHASE_MATCH_RESULT:
        if (step == HUD_STEP_BIG)
            hud_center_big(CENTER_BIG_ROW, HUD_PAL_YELLOW,
                hud_match_winner_text());
        break;
    default:
        break;
    }
}

void hud_scene_update(void)
{
    u16 key;

    /* Spread big redraws over frames: a score change and a scene change on
     * the same frame (every point) would overrun 816-tcc's budget, as
     * would a redraw with the renderer's recolour (g_render_busy). */
    u8 busy = g_render_busy;
    u8 idle;

    g_render_busy = 0;
    if (!s_hud_valid)
        hud_draw_scores();
    else if (s_hud_score[0] != g_game_state.team_score[0] ||
             s_hud_score[1] != g_game_state.team_score[1]) {
        hud_draw_score_digits();
        busy = g_attract == 0;  /* the scene step waits one frame */
    }

    key = hud_scene_key();
    if (!s_hud_valid || key != s_hud_scene) {
        s_prev_scene = s_hud_scene;
        s_hud_scene = key;
        /* Title to title (the prompt blinks, a page opens): only the prompt
         * changes; the logo stays and the area draws itself. One idle step
         * first, so the prompt is not drawn on the frame of the input that
         * changed the page (a recolour or a reload can share it). */
        s_scene_step = (s_hud_valid && HUD_IS_TITLE(key) &&
            HUD_IS_TITLE(s_prev_scene)) ? HUD_STEP_BIG : HUD_STEP_CLEAR;
        /* The pause cursor moved: only the items redraw (no flicker). */
        if (s_hud_valid && HUD_IS_PAUSED(key) && HUD_IS_PAUSED(s_prev_scene))
            s_scene_step = HUD_STEP_EXTRA;
    }
    /* Title steps also wait for a lighter frame (the demo runs behind). */
    idle = 0;
    if (s_scene_step < HUD_STEP_DONE && !busy && HUD_IS_TITLE(key) &&
            hud_title_wait(hud_title_step_limit(s_scene_step))) {
        busy = 1;
    } else if (s_scene_step < HUD_STEP_DONE && !busy && !HUD_IS_TITLE(key) &&
            profile_scanline() > HUD_MATCH_SCANLINE_LIMIT &&
            s_match_waits < HUD_MATCH_MAX_WAITS) {
        ++s_match_waits;
        busy = 1;
    } else if (s_scene_step < HUD_STEP_DONE && !busy) {
        /* The logo steps between two title views draw nothing, so the
         * title area may still draw on their frames (M4-UI-01). */
        idle = HUD_IS_TITLE(key) && HUD_IS_TITLE(s_prev_scene) &&
            (s_scene_step == HUD_STEP_BIG || s_scene_step == HUD_STEP_EXTRA);
        PROFILE_MARK(15);
        s_match_waits = 0;
        hud_scene_step(key, s_scene_step);
        /* The title: CLEAR, BIG, EXTRA (the logo's second row), PROMPT,
         * and a new prompt's text in halves (EXTRA2, EXTRA3). */
        if (HUD_IS_TITLE(key))
            s_scene_step = s_scene_step == HUD_STEP_BIG ? HUD_STEP_EXTRA :
                s_scene_step == HUD_STEP_EXTRA ? HUD_STEP_PROMPT :
                s_scene_step == HUD_STEP_PROMPT ? (s_prompt_text ?
                    HUD_STEP_EXTRA2 : HUD_STEP_DONE) :
                s_scene_step == HUD_STEP_EXTRA3 ? HUD_STEP_DONE :
                s_scene_step + 1;
        else if ((++s_scene_step == HUD_STEP_EXTRA3 &&
                    g_game_state.phase != GAME_PHASE_MATCH_RESULT &&
                    !(g_game_state.phase == GAME_PHASE_POINT_RESULT &&
                      ((g_replay.active && !g_attract) || s_quip))) ||
                (s_scene_step == (s_quip ? HUD_STEP_EXTRA6 :
                        HUD_STEP_EXTRA5) &&
                    g_game_state.phase == GAME_PHASE_POINT_RESULT))
            s_scene_step = HUD_STEP_DONE;
    } else if (!busy) {
        idle = 1;
    }
    /* Only on otherwise idle frames: keeps each frame to one redraw. */
    if (idle) {
        if (HUD_IS_TITLE(key))
            hud_title_area_update();
        else if (key == HUD_KEY_INITIALS)
            hud_initials_refresh();
        else if (g_game_state.phase == GAME_PHASE_SERVE_SETUP &&
                hud_serve_meter_refresh())
            ;
        else if (hud_sub_panel_wanted() != s_sub_panel)
            hud_draw_sub_panel(hud_sub_panel_wanted());
    }
    s_hud_valid = 1;

#ifdef RENDER_DIAGNOSTICS
    diag_draw_overlay(HUD_IS_PAUSED(key));
#endif
}


void hud_scene_initialize(void)
{
    s_hud_valid = 0;
    s_replay_drawn = 0;
    s_quip = 0;
    s_quip_drawn = 0;
    s_meter_cells = METER_UNDRAWN;
    s_sub_panel = 0;
    s_hud_scene = 0xff;
    s_scene_step = HUD_STEP_DONE;
    s_prev_scene = 0xff;
    s_match_waits = 0;
    s_prompt_page = 0xff;
    s_prompt_text = 0;
    hud_title_reset();
}

