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
#define TITLE_MENU_ROW 12           /* SINGLES / DOUBLES */
#define TITLE_MENU_COL 12

/* Last HUD inputs, so the BG3 map is only rebuilt when something changes. */
static u8 s_hud_score[2];
static u8 s_hud_scene;
static u8 s_hud_valid;
static u8 s_sub_panel;             /* 0 none, 1 ARCADE RULES, 2 MATCH POINT */

/* Title menu choice (0 singles, 1 doubles); owned by main.c's title input. */
u8 g_title_mode;

static const char *const s_palette_names[PAL_COUNT] = {
    "BLUE", "RED", "GREEN", "GOLD"
};

static u8 hud_doubles(void)
{
    return g_match_config.mode == MATCH_DOUBLES;
}

/* Singles: P1 / CPU. Doubles: the team's palette name (ADR-010/018). */
static const char *team_name(u8 team)
{
    if (hud_doubles())
        return s_palette_names[match_team_palette(team)];
    return team ? "CPU" : "P1";
}

void hud_scene_invalidate(void)
{
    s_hud_valid = 0;
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

/* Panels and labels never change during a match; drawn on full redraws. */
static void hud_draw_scores(void)
{
    hud_clear_rows(SCORE_PANEL_ROW, 3);
    hud_panel(LEFT_PANEL_COL, SCORE_PANEL_ROW, SIDE_PANEL_WIDTH);
    hud_text(LEFT_PANEL_COL + 1, SCORE_TEXT_ROW, team_hud_palette(0),
        team_name(0));
    hud_panel(CENTER_PANEL_COL, SCORE_PANEL_ROW, CENTER_PANEL_WIDTH);
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
#define HUD_KEY_TITLE 0x40
#define HUD_KEY_TITLE_BLINK 0x41

/* Doubles shows ARCADE RULES under DOUBLES; either mode shows MATCH POINT
 * when a team is one point from winning (the doubles mockup). */
static u8 hud_sub_panel_wanted(void)
{
    if (g_attract)
        return 0;
    if (g_game_state.phase != GAME_PHASE_MATCH_RESULT &&
        (g_game_state.team_score[0] == GAME_POINTS_TO_WIN - 1 ||
         g_game_state.team_score[1] == GAME_POINTS_TO_WIN - 1))
        return 2;
    return hud_doubles() ? 1 : 0;
}

static void hud_draw_sub_panel(u8 wanted)
{
    hud_clear_rows(SUB_PANEL_ROW, 3);
    if (wanted == 1) {
        hud_panel(CENTER_PANEL_COL, SUB_PANEL_ROW, CENTER_PANEL_WIDTH);
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
    if (hud_doubles())
        return doubles[match_team_palette(winner)];
    return winner == 0 ? "P1 POINT" : "CPU POINT";
}

static u8 hud_scene_key(void)
{
    u8 phase = g_game_state.phase;

    if (g_attract)
        return ((g_game_state.frame_count & 32) ? HUD_KEY_TITLE_BLINK :
            HUD_KEY_TITLE) | (g_title_mode << 1);

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

/* Title keys: 0x40 | blink (bit 0) | menu choice (bit 1). */
#define HUD_IS_TITLE(k) (((k) & 0xF0) == HUD_KEY_TITLE)

/*
 * Scene overlays are drawn in three steps on consecutive frames (clear, big
 * text, prompt). Doing all three on the frame a point ends made that one
 * frame overrun; spread out, no frame does more than one step.
 */
enum { HUD_STEP_CLEAR = 0, HUD_STEP_BIG = 1, HUD_STEP_PROMPT = 2,
       HUD_STEP_DONE = 3 };

static u8 s_scene_step;
static u8 s_prev_scene;

static void hud_scene_step(u8 key, u8 step)
{
    u8 winner = g_game_state.point_winner;
    u8 from_title = HUD_IS_TITLE(s_prev_scene);

    if (HUD_IS_TITLE(key)) {
        if (step == HUD_STEP_CLEAR && !from_title)
            hud_clear_rows(CENTER_BIG_ROW - 2, 9);
        else if (step == HUD_STEP_BIG && !from_title) {
            hud_center_big(TITLE_ROW, HUD_PAL_YELLOW, "PICKLEBALL");
            hud_center_big(TITLE_ROW + 2, HUD_PAL_YELLOW, "'94");
        } else if (step == HUD_STEP_PROMPT) {
            /* Only the prompt blinks; the logo stays. */
            hud_clear_rows(BANNER_ROW - 1, 4);
            if (!(key & 1))
                hud_center_prompt(BANNER_ROW, HUD_PAL_WHITE, "PRESS START");
            /* The menu redraws only when entering the title or when the
             * choice changes, not on every prompt blink. */
            if (!from_title || ((key ^ s_prev_scene) & 2)) {
                hud_text(TITLE_MENU_COL, TITLE_MENU_ROW,
                    g_title_mode ? HUD_PAL_WHITE : HUD_PAL_YELLOW,
                    "  SINGLES ");
                hud_text(TITLE_MENU_COL, TITLE_MENU_ROW + 1,
                    g_title_mode ? HUD_PAL_YELLOW : HUD_PAL_WHITE,
                    "  DOUBLES ");
                hud_tile(TITLE_MENU_COL, TITLE_MENU_ROW + g_title_mode,
                    hud_entry(HUD_TILE_CURSOR, HUD_PAL_YELLOW));
            }
        }
        return;
    }

    if (step == HUD_STEP_CLEAR) {
        hud_clear_rows(from_title ? CENTER_BIG_ROW - 2 : CENTER_BIG_ROW,
            from_title ? 9 : 7);
        hud_clear_rows(BANNER_ROW - 1, 4);
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
                g_game_state.match_winner == 0 ? "YOU WIN!" : "CPU WINS");
        else
            hud_center_prompt(BANNER_ROW, HUD_PAL_WHITE,
                "PRESS @ TO REMATCH");
        break;
    default:
        break;
    }
}

void hud_scene_update(void)
{
    u8 key;

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
    if (s_scene_step < HUD_STEP_DONE && !busy) {
        PROFILE_MARK(15);
        hud_scene_step(key, s_scene_step);
        ++s_scene_step;
    } else if (!busy && hud_sub_panel_wanted() != s_sub_panel) {
        /* Only on otherwise idle frames: keeps each frame to one redraw. */
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
}
