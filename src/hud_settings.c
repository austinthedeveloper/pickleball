#include "audio_events.h"
#include "court_style.h"
#include "hud.h"
#include "hud_internal.h"
#include "hud_scene.h"
#include "hud_tiles.h"
#include "modifiers.h"
#include "options.h"
#include "practice.h"
#include "save.h"
#include "text.h"

/* The open settings page's rows on screen (one page shows at a time):
 * value | cursor (bit 7); 0xff not drawn. */
#define SETTINGS_ROWS_MAX MODIFIERS_ROWS
static u8 s_setting_drawn[SETTINGS_ROWS_MAX];
static u8 s_practice_best_drawn;   /* the zone whose best is shown */

/* ---- The settings pages: COURT, OPTIONS, PRACTICE, MODIFIERS (R13) ----
 * One box each: a header, a gap, then a "<VALUE   >" row per setting
 * (PRACTICE adds a gap and its best streak). Drawn a part per frame; once
 * drawn, a changed row (Left/Right, Up/Down) redraws, one per frame. */

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

/* The open page's rows (TITLE_PAGE_*), and its editor's cursor row. */
static u8 hud_settings_rows(u8 page)
{
    if (page == TITLE_PAGE_COURT)
        return COURT_ROWS;
    if (page == TITLE_PAGE_OPTIONS)
        return OPTIONS_ROWS;
    if (page == TITLE_PAGE_PRACTICE)
        return PRACTICE_ROWS;
    return MODIFIERS_ROWS;
}

static u8 hud_settings_cursor(u8 page)
{
    if (page == TITLE_PAGE_COURT)
        return g_court_edit.row;
    if (page == TITLE_PAGE_OPTIONS)
        return g_options_edit.row;
    if (page == TITLE_PAGE_PRACTICE)
        return g_practice_edit.row;
    return g_modifiers_edit.row;
}

/* A row's value as drawn (under bit 7, the cursor's): the saved setting,
 * or for OPTIONS what its row shows of it. */
static u8 hud_settings_value(u8 page, u8 index)
{
    if (page == TITLE_PAGE_COURT) {
        if (index == COURT_ROW_VENUE)
            return g_save.settings.venue;
        if (index == COURT_ROW_TIME)
            return g_save.settings.time;
        return g_save.settings.court[index - COURT_ROW_PARTS];
    }
    if (page == TITLE_PAGE_PRACTICE)
        return g_save.settings.practice[index];
    if (page != TITLE_PAGE_OPTIONS)
        return modifiers_row_on(index, g_save.settings.modifiers,
            g_save.modifiers2);
    if (index == OPTIONS_ROW_POINTS)
        return g_save.settings.points;
    if (index == OPTIONS_ROW_SCORING)
        return g_save.settings.modifiers & MODIFIER_WIN_BY_2;
    if (index == OPTIONS_ROW_SOUND)
        return g_save.settings.sound;
    if (index == OPTIONS_ROW_CAMERA)
        return options_camera_on(g_save.camera);
    if (index == OPTIONS_ROW_AMOUNT)
        return options_camera_amount(g_save.camera);
    return g_save.settings.difficulty;
}

/* An OPTIONS row's value name (ADR-025, ADR-065): POINTS and AMOUNT are
 * numbers. */
static const char *hud_options_name(u8 index, u8 value)
{
    static char text[5];

    if (index == OPTIONS_ROW_POINTS) {
        text_count(text, value);
        return text;
    }
    if (index == OPTIONS_ROW_SCORING)
        return options_scoring_name(value);
    if (index == OPTIONS_ROW_SOUND)
        return audio_setting_name(value);
    if (index == OPTIONS_ROW_CAMERA)
        return value ? "ON" : "OFF";
    if (index == OPTIONS_ROW_AMOUNT) {
        /* ADR-050: "85%", "100%". */
        text_append(text_count(text, g_options_cameras[value]), "%");
        return text;
    }
    return options_difficulty_name(value);
}

static const char *hud_settings_label(u8 page, u8 index)
{
    static const char *const court[COURT_ROWS] = {
        "VENUE", "TIME", "SURFACE", "KITCHEN", "SURROUND", "LINES"
    };
    static const char *const options[OPTIONS_ROWS] = {
        "POINTS", "SCORING", "DIFFICULTY", "SOUND", "CAMERA", "  AMOUNT"
    };
    static const char *const practice[PRACTICE_ROWS] = {
        "SHOT", "TARGET", "RATE", "RETURN TO"
    };

    if (page == TITLE_PAGE_COURT)
        return court[index];
    if (page == TITLE_PAGE_OPTIONS)
        return options[index];
    if (page == TITLE_PAGE_PRACTICE)
        return practice[index];
    return modifiers_row_label(index);
}

static const char *hud_settings_name(u8 page, u8 index, u8 value)
{
    if (page == TITLE_PAGE_COURT)
        return index == COURT_ROW_VENUE ? court_style_venue_name(value) :
            index == COURT_ROW_TIME ? court_style_time_name(value) :
            court_style_name(index - COURT_ROW_PARTS, value);
    if (page == TITLE_PAGE_OPTIONS)
        return hud_options_name(index, value);
    if (page == TITLE_PAGE_PRACTICE)
        return index == PRACTICE_ROW_SHOT ? practice_shot_name(value) :
            index == PRACTICE_ROW_TARGET ? practice_target_name(value) :
            index == PRACTICE_ROW_RATE ? practice_rate_name(value) :
            practice_zone_name(value);
    return modifiers_value_name(index, g_save.settings.modifiers,
        g_save.modifiers2);
}

/* One row of the open page. */
static void hud_settings_row(u8 page, u8 index)
{
    u8 value = hud_settings_value(page, index);
    u8 cursor = hud_settings_cursor(page) == index;

    hud_setting_row(COURT_FIRST_ROW + index, hud_settings_label(page, index),
        hud_settings_name(page, index, value), cursor);
    s_setting_drawn[index] = value | (cursor << 7);
}

/* The best streak for PRACTICE's RETURN TO zone (M4-PRACTICE-03). */
static void hud_practice_best(void)
{
    u8 zone = g_save.settings.practice[PRACTICE_ROW_ZONE];

    hud_record_line(COURT_FIRST_ROW + PRACTICE_ROWS + 1, "BEST STREAK",
        practice_saved_best(zone));
    s_practice_best_drawn = zone;
}

/* A settings page: the box, the header, a row per part, and PRACTICE's
 * best; nonzero when the page is complete. */
u8 hud_settings_draw_part(u8 page, u8 part)
{
    static const char *const headers[TITLE_PAGE_MODIFIERS + 1] = {
        0, "COURT", 0, "OPTIONS", "PRACTICE", "MODIFIERS"
    };
    u8 rows = hud_settings_rows(page);
    u8 practice = page == TITLE_PAGE_PRACTICE;
    u8 i;

    if (part == 0) {
        /* PRACTICE: a gap and the best under the rows. */
        hud_fill_box(RECORDS_COL, TITLE_MENU_ROW, RECORDS_WIDTH,
            rows + (practice ? 4 : 2), hud_entry(' ', HUD_PAL_WHITE));
        for (i = 0; i < SETTINGS_ROWS_MAX; ++i)
            s_setting_drawn[i] = 0xff;
        s_practice_best_drawn = 0xff;
        return 0;
    }
    if (part == 1) {
        /* MODIFIERS has always sat a cell right of the box's centre. */
        if (page == TITLE_PAGE_MODIFIERS)
            hud_text(12, TITLE_MENU_ROW, HUD_PAL_YELLOW, headers[page]);
        else
            hud_page_header(RECORDS_COL, RECORDS_WIDTH, headers[page]);
        return 0;
    }
    if (part < 2 + rows) {
        hud_settings_row(page, part - 2);
        return practice == 0 && part - 2 == rows - 1;
    }
    hud_practice_best();
    return 1;
}

/* After a settings page is drawn: the first row whose value or cursor
 * changed, one per frame, then PRACTICE's best for a new zone. */
void hud_settings_refresh(u8 page)
{
    u8 rows = hud_settings_rows(page);
    u8 cursor = hud_settings_cursor(page);
    u8 index;

    for (index = 0; index < rows; ++index) {
        if (s_setting_drawn[index] != (hud_settings_value(page, index) |
                ((cursor == index) << 7))) {
            hud_settings_row(page, index);
            return;
        }
    }
    if (page == TITLE_PAGE_PRACTICE &&
            s_practice_best_drawn !=
                g_save.settings.practice[PRACTICE_ROW_ZONE])
        hud_practice_best();
}
