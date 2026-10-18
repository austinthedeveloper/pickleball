#ifndef HUD_INTERNAL_H
#define HUD_INTERNAL_H

/*
 * Shared by the BG3 HUD files only (R12): src/hud_scene.c (score panels,
 * scene overlays, the pause menu, and the staged scene steps),
 * src/hud_title.c (the title area: menu, join counter, RECORDS, and the
 * page drawing), src/hud_settings.c (COURT, OPTIONS, PRACTICE, MODIFIERS),
 * and src/hud_career.c (the CAREER page).
 */

#include "game_types.h"


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
#define TITLE_MENU_ROW 12           /* SINGLES ... MODIFIERS (rows 12-20) */
#define TITLE_MENU_COL 11           /* cursor + 10: columns 11-21, centred */
#define TITLE_JOIN_ROW 21          /* the join prompt under the menu */
#define TITLE_JOIN_TEXT "FRIENDS: PRESS START"
#define TITLE_JOIN_COL ((32 - (sizeof(TITLE_JOIN_TEXT) - 1)) / 2)
/* The join counter (ADR-066): "1P. 2P. 3Px 4Px" in a panel where the
 * match scoreboard goes, on the title only. */
#define JOIN_PANEL_COL 7
#define JOIN_PANEL_INNER 15
#define JOIN_FIRST_COL (JOIN_PANEL_COL + 1)
#define TITLE_AREA_ROWS 11          /* menu or a page: rows 12-22 */
#define RECORDS_COL 4               /* the records box: columns 4-27 */
#define RECORDS_WIDTH 24
#define RECORDS_ROWS 11
#define RECORDS_COUNT_COL 23        /* four digits, one cell from the edge */
/* Settings pages (COURT, OPTIONS, PRACTICE, MODIFIERS): header, gap, rows. */
#define COURT_FIRST_ROW (TITLE_MENU_ROW + 2)
#define COURT_LABEL_COL 6
#define COURT_VALUE_COL 16          /* "<NAME    >" */

/* Title drawing (staged steps and area parts) waits for a later frame once
 * the frame's work has reached this scanline: a row or the prompt can cost
 * 60+ scanlines, VBlank starts at 225, and the attract demo alone reaches
 * about 200 on its heavy frames. */
#define HUD_TITLE_SCANLINE_LIMIT 140

/* The CAREER page's view while typing the name (beside CAREER_VIEW_*). */
#define HUD_CAREER_NAMING 4

/* hud_title.c: the title area, drawn a part per idle title frame. */
/* Nonzero when title drawing should wait for a lighter frame: the frame's
 * work is past `limit`, and it has not waited HUD_TITLE_MAX_WAITS yet. */
u8 hud_title_wait(u16 limit);
void hud_title_area_update(void);
/* The area's view is unknown (redraw it) or its rows were cleared. */
void hud_title_area_forget(void);
void hud_title_area_cleared(void);
/* The join counter's rows were cleared (ADR-066). */
void hud_title_join_forget(void);
/* Power-on state: nothing drawn, no waits. */
void hud_title_reset(void);
/* A four-digit count (spaces for leading zeros), and a records line:
 * " LONGEST RALLY        nnnn ". */
void hud_count(u8 col, u8 row, u16 value);
void hud_record_line(u8 row, const char *label, u16 value);
/* A page's header column: length cells centred in a box of width cells
 * from col (an odd spare cell goes right). A constant for a literal. */
#define HUD_HEADER_COL(col, width, length) ((col) + ((width) - (length)) / 2)
#define HUD_PAGE_HEADER(col, width, text) \
    hud_text(HUD_HEADER_COL(col, width, sizeof(text) - 1), TITLE_MENU_ROW, \
        HUD_PAL_YELLOW, text)

/* hud_settings.c (R13): the open settings page (g_title_page) a part at a
 * time (nonzero when complete), then its changed rows, one per call. The
 * page is read, not passed: every argument costs 816-tcc (R33). */
u8 hud_settings_draw_part(u8 part);
void hud_settings_refresh(void);

/* hud_career.c (ADR-047): a CAREER view (CAREER_VIEW_* or
 * HUD_CAREER_NAMING) a part at a time, and each view's refresh. */
u8 hud_career_draw_part(u8 view, u8 part);
void hud_career_slots_refresh(void);
void hud_career_edit_refresh(void);
void hud_career_naming_refresh(void);

#endif
