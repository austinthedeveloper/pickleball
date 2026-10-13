#include "game.h"
#include "hud_tiles.h"
#include "input.h"
#include "serve.h"
#include "hud.h"
#include "hud_scene.h"
#include "match.h"
#include "profile.h"
#include "render.h"
#include "render_internal.h"
#include "court_style.h"
#include "options.h"
#include "save.h"
#include "story.h"

/*
 * Score panels and scene overlays (title, serve prompt, point and match
 * results, pause) on BG3. The BG3 map is rebuilt only when what it shows
 * changes, and overlays are drawn in steps across frames (see below).
 */

/*
 * BG3 layout in tile rows/columns (32x28 map). Panels are three rows tall
 * (border, text, border); see hud_panel().
 */
#define SCORE_PANEL_ROW 1           /* text on row 2 */
#define SCORE_TEXT_ROW (SCORE_PANEL_ROW + 1)
#define LEFT_PANEL_COL 1            /* "P1    n" */
#define CENTER_PANEL_COL 9          /* "ARCADE RULES" */
#define RIGHT_PANEL_COL 23          /* "CPU   n" */
#define SIDE_PANEL_WIDTH 6
#define CENTER_PANEL_WIDTH 12
#define LEFT_SCORE_COL 6            /* two digit cells, right-aligned */
#define RIGHT_SCORE_COL 28
#define TITLE_ROW 7                 /* PICKLEBALL / '94, two big rows each */
#define CENTER_BIG_ROW 8            /* fault word or match result */
#define CENTER_PANEL_ROW 11         /* "P1 POINT", "PAUSED" */
#define BANNER_ROW 24               /* serve / rematch / title prompt */
#define SUB_PANEL_ROW 4             /* ARCADE RULES / MATCH POINT */
#define TITLE_MENU_ROW 12           /* SINGLES ... OPTIONS (rows 12-16) */
#define TITLE_MENU_COL 12
#define TITLE_2P_ROW 18
#define TITLE_2P_COL 9              /* 14-character 2P join line */
#define TITLE_AREA_ROWS 11          /* menu or a page: rows 12-22 */
#define RECORDS_COL 4               /* the records box: columns 4-27 */
#define RECORDS_WIDTH 24
#define RECORDS_ROWS 11
#define RECORDS_COUNT_COL 23        /* four digits, one cell from the edge */
#define COURT_PAGE_ROWS 6           /* the court box: header, gap, 4 rows */
#define COURT_FIRST_ROW (TITLE_MENU_ROW + 2)
#define COURT_LABEL_COL 6
#define COURT_VALUE_COL 16          /* "<NAME    >" */
#define OPTIONS_PAGE_ROWS 4         /* header, gap, points, difficulty */

/* Last HUD inputs, so the BG3 map is only rebuilt when something changes. */
static u8 s_hud_score[2];
static u16 s_hud_scene;
static u8 s_hud_valid;
static u8 s_sub_panel;             /* 0 none, 1 ARCADE RULES, 2 MATCH POINT */

/* Title menu choice (TITLE_*), the open page (TITLE_PAGE_*), and 2P join;
 * owned by main.c's title input. */
u8 g_title_mode;
u8 g_title_page;
u8 g_title_2p;
/*
 * Title area (rows 12-22: the menu or a page), drawn a part per
 * idle frame by hud_title_area_update, apart from the staged scene steps so
 * the prompt blink never interrupts it. s_title_view is the view on screen
 * or being drawn (hud_title_view), HUD_TITLE_CLEARED after a clear, or
 * HUD_TITLE_UNKNOWN; s_title_part is the next part to draw.
 */
static u8 s_title_view;
static u8 s_title_part;
/* Court and options page rows on screen: value | cursor (bit 7); 0xff not
 * drawn. */
static u8 s_court_row_drawn[COURT_ROWS];
static u8 s_options_row_drawn[OPTIONS_ROWS];
#define HUD_TITLE_CLEARED 0xfe
#define HUD_TITLE_UNKNOWN 0xff
/* Title drawing (staged steps and area parts) waits for a later frame once
 * the frame's work has reached this scanline: a row or the prompt can cost
 * 60+ scanlines, VBlank starts at 225, and the attract demo alone reaches
 * about 200 on its heavy frames. */
#define HUD_TITLE_SCANLINE_LIMIT 140

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

/* Singles: P1 / CPU (or P2). Doubles: the team's palette name
 * (ADR-010/018), or CPU for a ladder opponent (ADR-023). */
static const char *team_name(u8 team)
{
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
    s_title_view = HUD_TITLE_UNKNOWN;
}

void hud_scene_refresh_title(void)
{
    s_title_view = HUD_TITLE_UNKNOWN;
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
    default:
        return "POINT!";
    }
}

static u8 team_hud_palette(u8 team)
{
    return team ? HUD_PAL_TEAM_RIGHT : HUD_PAL_TEAM_LEFT;
}

/* Score digits only; tens are drawn as a leading digit or a blank. */
static void hud_draw_score_digits(void)
{
    static const char digits[] = "0123456789";
    char text[3];
    u8 team;
    u8 score;

    for (team = 0; team < 2; ++team) {
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

/* Panels and labels never change during a match; drawn on full redraws. */
static void hud_draw_scores(void)
{
    hud_clear_rows(SCORE_PANEL_ROW, 3);
    hud_panel(LEFT_PANEL_COL, SCORE_PANEL_ROW, SIDE_PANEL_WIDTH);
    hud_text(LEFT_PANEL_COL + 1, SCORE_TEXT_ROW, team_hud_palette(0),
        team_name(0));
    hud_panel(CENTER_PANEL_COL, SCORE_PANEL_ROW, CENTER_PANEL_WIDTH);
    if (!g_attract && !hud_doubles() && hud_opponent_team(1))
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
#define HUD_KEY_PAUSED 0x80
#define HUD_KEY_TITLE 0x100
#define HUD_KEY_TITLE_BLINK 0x101
#define HUD_KEY_INTRO 0x200         /* the story intro card (ADR-026) */

/* Doubles shows ARCADE RULES under DOUBLES; either mode shows MATCH POINT
 * when a team is one point from winning (the doubles mockup). */
static u8 hud_sub_panel_wanted(void)
{
    if (g_attract)
        return 0;
    if (g_game_state.phase != GAME_PHASE_MATCH_RESULT &&
        (g_game_state.team_score[0] == g_match_config.points_to_win - 1 ||
         g_game_state.team_score[1] == g_match_config.points_to_win - 1))
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
            g_match_config.opponent == OPPONENT_COUNT - 1)
        return "CHAMPION!";
    if (hud_versus_2p())
        return g_game_state.match_winner == 0 ? "P1 WINS" : "P2 WINS";
    return g_game_state.match_winner == 0 ? "YOU WIN!" : "CPU WINS";
}

static u16 hud_scene_key(void)
{
    u8 phase = g_game_state.phase;

    if (g_story.card == STORY_CARD_INTRO)
        return HUD_KEY_INTRO;

    if (g_attract)
        return ((g_game_state.frame_count & 32) ? HUD_KEY_TITLE_BLINK :
            HUD_KEY_TITLE) | (g_title_page << 1);

    if (g_game_state.paused)
        return HUD_KEY_PAUSED;
    if (phase == GAME_PHASE_SERVE_TOSS || phase == GAME_PHASE_SERVE_FAULT)
        phase = GAME_PHASE_LIVE;
    if (phase == GAME_PHASE_SERVE_SETUP)
        return phase | (g_serve_state.server_index << 4);
    if (phase == GAME_PHASE_POINT_RESULT || phase == GAME_PHASE_MATCH_RESULT)
        return phase | (g_game_state.point_winner << 4) |
            ((g_game_state.last_fault & 7) << 5);
    return phase;
}

/* Title keys: 0x100 | blink (bit 0) | page (bits 1-2, for its prompt).
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
       HUD_STEP_DONE = 5 };

static u8 s_scene_step;
static u16 s_prev_scene;

/* The opponent speaking on the story cards: singles names the captain,
 * doubles the team. */
static const char *hud_opponent_name(void)
{
    return hud_doubles() ? g_opponent_team[g_match_config.opponent] :
        g_opponent_captain[g_match_config.opponent];
}

/* A line of text centred on a row, without a panel. */
static void hud_center_text(u8 row, u8 palette, const char *text)
{
    u8 length = 0;

    while (text[length])
        ++length;
    hud_text((u8)(16 - length / 2), row, palette, text);
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

/* Under the match result (ADR-026): the opponent's line (EXTRA), and
 * after a win the next opponent or the ladder cleared (EXTRA2). */
static void hud_result_story(u8 step)
{
    static char next[] = "NEXT UP: 123456789";
    u8 won = g_game_state.match_winner == 0;
    const char *const *lines;
    const char *team;
    u8 i;

    if (g_match_config.opponent == OPPONENT_NONE)
        return;
    if (step == HUD_STEP_EXTRA) {
        /* Separate steps: 816-tcc fails (ICE 2) on a ternary as the index. */
        lines = g_opponent_lines[g_match_config.opponent];
        hud_center_prompt(CENTER_PANEL_ROW, HUD_PAL_TEAM_RIGHT,
            won ? lines[OPPONENT_LINE_CONCEDE] : lines[OPPONENT_LINE_TAUNT]);
        return;
    }
    if (!won)
        return;
    if (g_match_config.opponent == OPPONENT_COUNT - 1) {
        hud_center_text(CENTER_PANEL_ROW + 3, HUD_PAL_YELLOW,
            "LADDER CLEARED!");
        return;
    }
    team = g_opponent_team[g_save.settings.ladder < OPPONENT_COUNT ?
        g_save.settings.ladder : 0];
    for (i = 0; team[i]; ++i)
        next[9 + i] = team[i];
    next[9 + i] = 0;
    hud_center_text(CENTER_PANEL_ROW + 3, HUD_PAL_YELLOW, next);
}

/* Four digits, right-aligned; leading zeros are spaces, which the font
 * draws as dark cells like the box. Records saturate at SAVE_COUNTER_MAX.
 * No 816-tcc division. */
static void hud_count(u8 col, u8 row, u16 value)
{
    static const u16 powers[3] = { 1000, 100, 10 };
    char text[5];
    u8 i;
    u8 leading = 1;

    for (i = 0; i < 3; ++i) {
        text[i] = '0';
        while (value >= powers[i]) {
            value -= powers[i];
            ++text[i];
        }
        if (leading && text[i] == '0')
            text[i] = ' ';
        else
            leading = 0;
    }
    text[3] = (char)('0' + value);
    text[4] = 0;
    hud_text(col, row, HUD_PAL_WHITE, text);
}

/* " LONGEST RALLY        nnnn " */
static void hud_record_line(u8 row, const char *label, u16 value)
{
    hud_text(RECORDS_COL + 1, row, HUD_PAL_WHITE, label);
    hud_count(RECORDS_COUNT_COL, row, value);
}

/* " SINGLES  W nnnn  L nnnn " */
static void hud_record_mode_line(u8 row, const char *label, u8 mode)
{
    hud_text(RECORDS_COL + 1, row, HUD_PAL_WHITE, label);
    hud_text(RECORDS_COUNT_COL - 9, row, HUD_PAL_YELLOW, "W");
    hud_count(RECORDS_COUNT_COL - 7, row, g_save.records.wins[mode]);
    hud_text(RECORDS_COUNT_COL - 2, row, HUD_PAL_YELLOW, "L");
    hud_count(RECORDS_COUNT_COL, row, g_save.records.losses[mode]);
}

/* Two menu items per call (half 0 or 1); the cursor marks the choice. */
/* " LADDER 4/8       PIRATES ": the rung and the team up next (ADR-023). */
static void hud_record_ladder(u8 row)
{
    u8 rung = g_save.settings.ladder;
    const char *team;
    char text[4];
    u8 length = 0;

    if (rung >= OPPONENT_COUNT)
        rung = 0;
    team = g_opponent_team[rung];
    text[0] = (char)('1' + rung);
    text[1] = '/';
    text[2] = (char)('0' + OPPONENT_COUNT);
    text[3] = 0;
    hud_text(RECORDS_COL + 1, row, HUD_PAL_WHITE, "LADDER");
    hud_text(RECORDS_COL + 8, row, HUD_PAL_YELLOW, text);
    while (team[length])
        ++length;
    hud_text(RECORDS_COUNT_COL + 4 - length, row, HUD_PAL_WHITE, team);
}

static void hud_title_menu(u8 choice, u8 half)
{
    static const char *const items[TITLE_ENTRIES] = {
        "  SINGLES ", "  DOUBLES ", "  COURT   ", "  RECORDS ", "  OPTIONS "
    };
    u8 i = half * 2;
    u8 last = i + 2 > TITLE_ENTRIES ? TITLE_ENTRIES : i + 2;

    for (; i < last; ++i) {
        hud_text(TITLE_MENU_COL, TITLE_MENU_ROW + i,
            i == choice ? HUD_PAL_YELLOW : HUD_PAL_WHITE, items[i]);
        if (i == choice)
            hud_tile(TITLE_MENU_COL, TITLE_MENU_ROW + i,
                hud_entry(HUD_TILE_CURSOR, HUD_PAL_YELLOW));
    }
}

/* A court page row's value: the venue choice, or a part's preset. */
static u8 hud_court_value(u8 index)
{
    if (index == COURT_ROW_VENUE)
        return g_save.settings.venue;
    return g_save.settings.court[index - 1];
}

/* A settings page row: cursor, label, and "<VALUE   >". */
static void hud_setting_row(u8 row, const char *label, const char *name,
    u8 cursor)
{
    char text[11];
    u8 i = 1;

    hud_tile(COURT_LABEL_COL - 1, row, cursor ?
        hud_entry(HUD_TILE_CURSOR, HUD_PAL_YELLOW) :
        hud_entry(' ', HUD_PAL_WHITE));
    hud_text(COURT_LABEL_COL, row, cursor ? HUD_PAL_YELLOW : HUD_PAL_WHITE,
        label);
    text[0] = '<';
    while (*name)
        text[i++] = *name++;
    while (i < 9)
        text[i++] = ' ';
    text[9] = '>';
    text[10] = 0;
    hud_text(COURT_VALUE_COL, row, HUD_PAL_WHITE, text);
}

/* One court page row. */
static void hud_court_row(u8 index)
{
    static const char *const labels[COURT_ROWS] = {
        "VENUE", "SURFACE", "SURROUND", "LINES"
    };
    u8 value = hud_court_value(index);
    u8 cursor = g_court_edit.row == index;

    hud_setting_row(COURT_FIRST_ROW + index, labels[index],
        index == COURT_ROW_VENUE ? court_style_venue_name(value) :
        court_style_name(index - 1, value), cursor);
    s_court_row_drawn[index] = value | (cursor << 7);
}

/* An options page value: the points, or the difficulty. */
static u8 hud_options_value(u8 index)
{
    if (index == OPTIONS_ROW_POINTS)
        return g_save.settings.points;
    return g_save.settings.difficulty;
}

/* One options page row (ADR-025). */
static void hud_options_row(u8 index)
{
    u8 value = hud_options_value(index);
    u8 cursor = g_options_edit.row == index;
    char points[3];

    if (index == OPTIONS_ROW_POINTS) {
        points[0] = value >= 20 ? '2' : value >= 10 ? '1' : '0' + value;
        points[1] = value >= 10 ? (char)('0' + value % 10) : 0;
        points[2] = 0;
        hud_setting_row(COURT_FIRST_ROW, "POINTS", points, cursor);
    } else {
        hud_setting_row(COURT_FIRST_ROW + 1, "DIFFICULTY",
            options_difficulty_name(value), cursor);
    }
    s_options_row_drawn[index] = value | (cursor << 7);
}

/* Options page: the box, the header, then a row per frame. */
static u8 hud_options_draw_part(u8 part)
{
    if (part == 0) {
        hud_fill_box(RECORDS_COL, TITLE_MENU_ROW, RECORDS_WIDTH,
            OPTIONS_PAGE_ROWS, hud_entry(' ', HUD_PAL_WHITE));
        s_options_row_drawn[0] = 0xff;
        s_options_row_drawn[1] = 0xff;
        return 0;
    }
    if (part == 1) {
        hud_text(12, TITLE_MENU_ROW, HUD_PAL_YELLOW, "OPTIONS");
        return 0;
    }
    hud_options_row(part - 2);
    return part - 2 == OPTIONS_ROWS - 1;
}

/* After the options page is drawn: a changed row, one per frame. */
static void hud_options_refresh(void)
{
    u8 index;

    for (index = 0; index < OPTIONS_ROWS; ++index) {
        if (s_options_row_drawn[index] != (hud_options_value(index) |
                ((g_options_edit.row == index) << 7))) {
            hud_options_row(index);
            return;
        }
    }
}

/* Title views for the area: the menu choice (bits 0-2) and 2P (bit 3), or
 * a page. */
#define HUD_TITLE_VIEW_COURT 0x80
#define HUD_TITLE_VIEW_RECORDS 0x81
#define HUD_TITLE_VIEW_OPTIONS 0x82

static u8 hud_title_view(void)
{
    if (g_title_page == TITLE_PAGE_COURT)
        return HUD_TITLE_VIEW_COURT;
    if (g_title_page == TITLE_PAGE_RECORDS)
        return HUD_TITLE_VIEW_RECORDS;
    if (g_title_page == TITLE_PAGE_OPTIONS)
        return HUD_TITLE_VIEW_OPTIONS;
    return g_title_mode | (g_title_2p << 3);
}

/* Court page: the box, the header, then a part per frame. */
static u8 hud_court_draw_part(u8 part)
{
    if (part == 0) {
        hud_fill_box(RECORDS_COL, TITLE_MENU_ROW, RECORDS_WIDTH,
            COURT_PAGE_ROWS, hud_entry(' ', HUD_PAL_WHITE));
        s_court_row_drawn[0] = 0xff;
        s_court_row_drawn[1] = 0xff;
        s_court_row_drawn[2] = 0xff;
        s_court_row_drawn[3] = 0xff;
        return 0;
    }
    if (part == 1) {
        hud_text(13, TITLE_MENU_ROW, HUD_PAL_YELLOW, "COURT");
        return 0;
    }
    hud_court_row(part - 2);
    return part - 2 == COURT_ROWS - 1;
}

/* After the court page is drawn: the first row whose value or cursor
 * changed (Left/Right, Up/Down), one per frame. */
static void hud_court_refresh(void)
{
    u8 index;
    u8 want;

    for (index = 0; index < COURT_ROWS; ++index) {
        want = hud_court_value(index) | ((g_court_edit.row == index) << 7);
        if (s_court_row_drawn[index] != want) {
            hud_court_row(index);
            return;
        }
    }
}

/* One part of the area; returns nonzero when the view is complete. One
 * row per part: the title demo leaves little frame time. */
static u8 hud_title_draw_part(u8 view, u8 part)
{
    if (view == HUD_TITLE_VIEW_COURT)
        return hud_court_draw_part(part);
    if (view == HUD_TITLE_VIEW_OPTIONS)
        return hud_options_draw_part(part);
    if (view != HUD_TITLE_VIEW_RECORDS) {
        if (part < 3) {
            hud_title_menu(view & 7, part);
            return 0;
        }
        /* ADR-020: Start on pad 2 joins (or leaves) as 2P. */
        hud_text(TITLE_2P_COL, TITLE_2P_ROW,
            g_title_2p ? HUD_PAL_YELLOW : HUD_PAL_WHITE,
            g_title_2p ? "  2P JOINED!  " : "2P PRESS START");
        return 1;
    }
    switch (part) {
    case 0:
        /* The font's space is a dark cell: a solid box over the court. */
        hud_fill_box(RECORDS_COL, TITLE_MENU_ROW, RECORDS_WIDTH, RECORDS_ROWS,
            hud_entry(' ', HUD_PAL_WHITE));
        return 0;
    case 1:
        hud_text(12, TITLE_MENU_ROW, HUD_PAL_YELLOW, "RECORDS");
        return 0;
    case 2:
        hud_record_mode_line(TITLE_MENU_ROW + 2, "SINGLES", MATCH_SINGLES);
        return 0;
    case 3:
        hud_record_mode_line(TITLE_MENU_ROW + 3, "DOUBLES", MATCH_DOUBLES);
        return 0;
    case 4:
        hud_record_line(TITLE_MENU_ROW + 5, "WIN STREAK",
            g_save.records.streak);
        return 0;
    case 5:
        hud_record_line(TITLE_MENU_ROW + 6, "BEST STREAK",
            g_save.records.best_streak);
        return 0;
    case 6:
        hud_record_line(TITLE_MENU_ROW + 7, "SHUTOUTS",
            g_save.records.shutouts);
        return 0;
    case 7:
        hud_record_line(TITLE_MENU_ROW + 8, "LONGEST RALLY",
            g_save.records.longest_rally);
        return 0;
    case 8:
        hud_record_ladder(TITLE_MENU_ROW + 9);
        return 0;
    default:
        hud_record_line(TITLE_MENU_ROW + 10, "LADDER CLEARS",
            g_save.records.ladder_clears);
        return 1;
    }
}

/* Idle frames on the title: clear the area when the view changed, then
 * draw it a part per frame. */
static void hud_title_area_update(void)
{
    u8 view = hud_title_view();

    if (profile_scanline() > HUD_TITLE_SCANLINE_LIMIT)
        return;

    if (s_title_view == view) {
        if (s_title_part == 0xff) {
            if (view == HUD_TITLE_VIEW_COURT)
                hud_court_refresh();
            else if (view == HUD_TITLE_VIEW_OPTIONS)
                hud_options_refresh();
            return;
        }
        if (hud_title_draw_part(view, s_title_part))
            s_title_part = 0xff;
        else
            ++s_title_part;
        return;
    }
    if (s_title_view != HUD_TITLE_CLEARED)
        hud_clear_rows(TITLE_MENU_ROW, TITLE_AREA_ROWS);
    s_title_view = view;
    s_title_part = 0;
}

static const char *hud_title_prompt(u8 page)
{
    if (page == TITLE_PAGE_COURT || page == TITLE_PAGE_OPTIONS)
        return "@ OK  Y CANCEL";
    if (page == TITLE_PAGE_RECORDS)
        return "@ BACK";
    return "PRESS START";
}

/*
 * Title steps: CLEAR wipes the match overlays (coming from a match), BIG
 * draws the logo, and PROMPT the blinking prompt. Only the prompt blinks;
 * the logo stays.
 */
static void hud_title_step(u16 key, u8 step, u8 from_title)
{
    switch (step) {
    case HUD_STEP_CLEAR:
        if (!from_title) {
            hud_clear_rows(CENTER_BIG_ROW - 2, 6 + TITLE_AREA_ROWS);
            s_title_view = HUD_TITLE_CLEARED;
        }
        break;
    case HUD_STEP_BIG:
        if (!from_title) {
            hud_center_big(TITLE_ROW, HUD_PAL_YELLOW, "PICKLEBALL");
            hud_center_big(TITLE_ROW + 2, HUD_PAL_YELLOW, "'94");
        }
        break;
    case HUD_STEP_PROMPT:
        hud_clear_rows(BANNER_ROW - 1, 4);
        if (!(key & 1))
            hud_center_prompt(BANNER_ROW, HUD_PAL_WHITE,
                hud_title_prompt((key >> 1) & 3));
        break;
    default:
        break;
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
        hud_clear_rows(from_title ? CENTER_BIG_ROW - 2 : CENTER_BIG_ROW,
            from_title ? 6 + TITLE_AREA_ROWS : 7);
        hud_clear_rows(BANNER_ROW - 1, 4);
        return;
    }
    if (key == HUD_KEY_INTRO) {
        hud_intro_step(step);
        return;
    }
    if (step >= HUD_STEP_EXTRA) {
        if (g_game_state.phase == GAME_PHASE_MATCH_RESULT &&
                key != HUD_KEY_PAUSED && !g_attract)
            hud_result_story(step);
        return;
    }
    if (key == HUD_KEY_PAUSED) {
        if (step == HUD_STEP_PROMPT)
            hud_center_prompt(CENTER_PANEL_ROW, HUD_PAL_WHITE, "PAUSED");
        return;
    }
    switch (g_game_state.phase) {
    case GAME_PHASE_SERVE_SETUP:
        if (step != HUD_STEP_PROMPT)
            break;
        if (g_input_owners[g_serve_state.server_index] == INPUT_OWNER_HUMAN)
            hud_center_prompt(BANNER_ROW, HUD_PAL_WHITE, "PRESS @ TO SERVE");
        else
            hud_center_prompt(BANNER_ROW, HUD_PAL_WHITE, "CPU TO SERVE");
        break;
    case GAME_PHASE_POINT_RESULT:
        if (step == HUD_STEP_BIG)
            hud_center_big(CENTER_BIG_ROW, HUD_PAL_YELLOW,
                fault_word(g_game_state.last_fault));
        else
            hud_center_prompt(CENTER_PANEL_ROW, team_hud_palette(winner),
                hud_point_text(winner));
        break;
    case GAME_PHASE_MATCH_RESULT:
        if (step == HUD_STEP_BIG)
            hud_center_big(CENTER_BIG_ROW, HUD_PAL_YELLOW,
                hud_match_winner_text());
        else
            hud_center_prompt(BANNER_ROW, HUD_PAL_WHITE,
                "@ REMATCH  START MENU");
        break;
    default:
        break;
    }
}

void hud_scene_update(void)
{
    u16 key;

    /* Spread big redraws over frames: a score change and a scene change on
     * the same frame (every point) would overrun 816-tcc's budget. */
    u8 busy = 0;

    if (!s_hud_valid)
        hud_draw_scores();
    else if (s_hud_score[0] != g_game_state.team_score[0] ||
             s_hud_score[1] != g_game_state.team_score[1]) {
        hud_draw_score_digits();
        busy = 1;               /* the scene step waits one frame */
    }

    key = hud_scene_key();
    if (!s_hud_valid || key != s_hud_scene) {
        s_prev_scene = s_hud_scene;
        s_hud_scene = key;
        s_scene_step = HUD_STEP_CLEAR;
    }
    /* Title steps also wait for a lighter frame (the demo runs behind). */
    if (s_scene_step < HUD_STEP_DONE && !busy && HUD_IS_TITLE(key) &&
            profile_scanline() > HUD_TITLE_SCANLINE_LIMIT) {
        busy = 1;
    } else if (s_scene_step < HUD_STEP_DONE && !busy) {
        PROFILE_MARK(15);
        hud_scene_step(key, s_scene_step);
        ++s_scene_step;
    } else if (!busy) {
        /* Only on otherwise idle frames: keeps each frame to one redraw. */
        if (HUD_IS_TITLE(key))
            hud_title_area_update();
        else if (hud_sub_panel_wanted() != s_sub_panel)
            hud_draw_sub_panel(hud_sub_panel_wanted());
    }
    s_hud_valid = 1;

#ifdef RENDER_DIAGNOSTICS
    diag_draw_overlay(key == HUD_KEY_PAUSED);
#endif
}


void hud_scene_initialize(void)
{
    s_hud_valid = 0;
    s_sub_panel = 0;
    s_hud_scene = 0xff;
    s_scene_step = HUD_STEP_DONE;
    s_prev_scene = 0xff;
    s_title_view = HUD_TITLE_UNKNOWN;
}
