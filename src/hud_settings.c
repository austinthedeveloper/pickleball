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

/* Per-page parts. Each page's row is its own one-argument function, and
 * the page is dispatched once per part or refresh: on idle title frames
 * (the demo running behind) every call with arguments costs 816-tcc
 * scanlines (R33). */

/* A row's value as drawn (under bit 7, the cursor's). */
static u8 hud_court_value(u8 index)
{
    if (index == COURT_ROW_VENUE)
        return g_save.settings.venue;
    if (index == COURT_ROW_TIME)
        return g_save.settings.time;
    return g_save.settings.court[index - COURT_ROW_PARTS];
}

static u8 hud_options_value(u8 index)
{
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

static u8 hud_modifiers_value(u8 index)
{
    return modifiers_row_on(index, g_save.settings.modifiers,
        g_save.modifiers2);
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

static void hud_court_row(u8 index)
{
    static const char *const labels[COURT_ROWS] = {
        "VENUE", "TIME", "SURFACE", "KITCHEN", "SURROUND", "LINES"
    };
    u8 value = hud_court_value(index);
    u8 cursor = g_court_edit.row == index;

    hud_setting_row(COURT_FIRST_ROW + index, labels[index],
        index == COURT_ROW_VENUE ? court_style_venue_name(value) :
        index == COURT_ROW_TIME ? court_style_time_name(value) :
        court_style_name(index - COURT_ROW_PARTS, value), cursor);
    s_setting_drawn[index] = value | (cursor << 7);
}

static void hud_options_row(u8 index)
{
    static const char *const labels[OPTIONS_ROWS] = {
        "POINTS", "SCORING", "DIFFICULTY", "SOUND", "CAMERA", "  AMOUNT"
    };
    u8 value = hud_options_value(index);
    u8 cursor = g_options_edit.row == index;

    hud_setting_row(COURT_FIRST_ROW + index, labels[index],
        hud_options_name(index, value), cursor);
    s_setting_drawn[index] = value | (cursor << 7);
}

static void hud_practice_row(u8 index)
{
    static const char *const labels[PRACTICE_ROWS] = {
        "SHOT", "TARGET", "RATE", "RETURN TO"
    };
    u8 value = g_save.settings.practice[index];
    u8 cursor = g_practice_edit.row == index;

    hud_setting_row(COURT_FIRST_ROW + index, labels[index],
        index == PRACTICE_ROW_SHOT ? practice_shot_name(value) :
        index == PRACTICE_ROW_TARGET ? practice_target_name(value) :
        index == PRACTICE_ROW_RATE ? practice_rate_name(value) :
        practice_zone_name(value), cursor);
    s_setting_drawn[index] = value | (cursor << 7);
}

static void hud_modifiers_row(u8 index)
{
    u8 value = hud_modifiers_value(index);
    u8 cursor = g_modifiers_edit.row == index;

    hud_setting_row(COURT_FIRST_ROW + index, modifiers_row_label(index),
        modifiers_value_name(index, g_save.settings.modifiers,
            g_save.modifiers2), cursor);
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

/* Rows and header column per TITLE_PAGE_* (0 for the others). Each header
 * is centred in the box, an odd spare cell to the right, but MODIFIERS,
 * which has always sat a cell further right. */
#define HUD_BOX_CENTRE(text) \
    HUD_HEADER_COL(RECORDS_COL, RECORDS_WIDTH, sizeof(text) - 1)
static const u8 s_page_rows[TITLE_PAGE_MODIFIERS + 1] = {
    0, COURT_ROWS, 0, OPTIONS_ROWS, PRACTICE_ROWS, MODIFIERS_ROWS
};
static const u8 s_header_cols[TITLE_PAGE_MODIFIERS + 1] = {
    0, HUD_BOX_CENTRE("COURT"), 0, HUD_BOX_CENTRE("OPTIONS"),
    HUD_BOX_CENTRE("PRACTICE"), HUD_BOX_CENTRE("MODIFIERS") + 1
};
static const char *const s_headers[TITLE_PAGE_MODIFIERS + 1] = {
    0, "COURT", 0, "OPTIONS", "PRACTICE", "MODIFIERS"
};

/* A settings page: the box, the header, a row per part, and PRACTICE's
 * best; nonzero when the page is complete. */
u8 hud_settings_draw_part(u8 part)
{
    u8 page = g_title_page;
    u8 i;

    /* The rows first: they are most of the parts. */
    if (part >= 2) {
        part -= 2;
        if (page == TITLE_PAGE_COURT) {
            hud_court_row(part);
            return part == COURT_ROWS - 1;
        }
        if (page == TITLE_PAGE_OPTIONS) {
            hud_options_row(part);
            return part == OPTIONS_ROWS - 1;
        }
        if (page == TITLE_PAGE_MODIFIERS) {
            hud_modifiers_row(part);
            return part == MODIFIERS_ROWS - 1;
        }
        if (part < PRACTICE_ROWS) {
            hud_practice_row(part);
            return 0;
        }
        hud_practice_best();
        return 1;
    }
    if (part == 0) {
        /* PRACTICE: a gap and the best under the rows. */
        hud_fill_box(RECORDS_COL, TITLE_MENU_ROW, RECORDS_WIDTH,
            s_page_rows[page] + (page == TITLE_PAGE_PRACTICE ? 4 : 2),
            hud_entry(' ', HUD_PAL_WHITE));
        for (i = 0; i < SETTINGS_ROWS_MAX; ++i)
            s_setting_drawn[i] = 0xff;
        s_practice_best_drawn = 0xff;
        return 0;
    }
    hud_text(s_header_cols[page], TITLE_MENU_ROW, HUD_PAL_YELLOW,
        s_headers[page]);
    return 0;
}

/* After a settings page is drawn: the first row whose value or cursor
 * changed, one per frame, then PRACTICE's best for a new zone. */
void hud_settings_refresh(void)
{
    u8 page = g_title_page;
    u8 index;

    if (page == TITLE_PAGE_COURT) {
        for (index = 0; index < COURT_ROWS; ++index)
            if (s_setting_drawn[index] != (hud_court_value(index) |
                    ((g_court_edit.row == index) << 7))) {
                hud_court_row(index);
                return;
            }
    } else if (page == TITLE_PAGE_OPTIONS) {
        for (index = 0; index < OPTIONS_ROWS; ++index)
            if (s_setting_drawn[index] != (hud_options_value(index) |
                    ((g_options_edit.row == index) << 7))) {
                hud_options_row(index);
                return;
            }
    } else if (page == TITLE_PAGE_MODIFIERS) {
        for (index = 0; index < MODIFIERS_ROWS; ++index)
            if (s_setting_drawn[index] != (hud_modifiers_value(index) |
                    ((g_modifiers_edit.row == index) << 7))) {
                hud_modifiers_row(index);
                return;
            }
    } else {
        for (index = 0; index < PRACTICE_ROWS; ++index)
            if (s_setting_drawn[index] != (g_save.settings.practice[index] |
                    ((g_practice_edit.row == index) << 7))) {
                hud_practice_row(index);
                return;
            }
        if (s_practice_best_drawn !=
                g_save.settings.practice[PRACTICE_ROW_ZONE])
            hud_practice_best();
    }
}
