#include "career_page.h"
#include "hud.h"
#include "hud_internal.h"
#include "hud_scene.h"
#include "hud_tiles.h"
#include "input.h"
#include "match.h"
#include "opponents.h"
#include "profile.h"
#include "save.h"
#include "text.h"

/*
 * The title area (rows 12-22) over the attract demo: the menu and its
 * join line, the join counter, the RECORDS table, and the pages, drawn a
 * part per idle frame (hud_title_area_update); the logo and the prompt
 * are the staged scene steps in hud_scene.c.
 */

/* Title menu choice (TITLE_*), the open page (TITLE_PAGE_*), and who
 * joined (TITLE_JOIN_*); owned by main.c's title input. */
u8 g_title_mode;
u8 g_title_page;
u8 g_title_joined;
/* The join counter on screen: the joined bits | Multitap (bit 4), or
 * JOIN_UNDRAWN (rows cleared) or JOIN_PANEL (the panel, not its text). */
#define JOIN_UNDRAWN 0xff
#define JOIN_PANEL 0xfe
static u8 s_join_drawn;
/*
 * Title area (rows 12-22: the menu or a page), drawn a part per
 * idle frame by hud_title_area_update, apart from the staged scene steps so
 * the prompt blink never interrupts it. s_title_view is the view on screen
 * or being drawn (hud_title_view), HUD_TITLE_CLEARED after a clear, or
 * HUD_TITLE_UNKNOWN; s_title_part is the next part to draw.
 */
static u8 s_title_view;
static u8 s_title_part;
/* A menu item that lost the cursor and still shows it, redrawn on the
 * frame after the move (0xff: none). */
static u8 s_title_stale;
#define HUD_TITLE_CLEARED 0xfe
#define HUD_TITLE_UNKNOWN 0xff
/* ...but for at most this many frames in a row: the doubles demo is busy on
 * nearly every frame (M4-UI-03), and a menu that never finishes drawing is
 * worse than an occasional dropped frame. */
#define HUD_TITLE_MAX_WAITS 6
static u8 s_title_waits;

u8 hud_title_wait(u16 limit)
{
    if (profile_scanline() > limit &&
            s_title_waits < HUD_TITLE_MAX_WAITS) {
        ++s_title_waits;
        return 1;
    }
    s_title_waits = 0;
    return 0;
}

void hud_title_area_forget(void)
{
    s_title_view = HUD_TITLE_UNKNOWN;
}

void hud_title_area_cleared(void)
{
    s_title_view = HUD_TITLE_CLEARED;
}

void hud_title_join_forget(void)
{
    s_join_drawn = JOIN_UNDRAWN;
}

void hud_title_reset(void)
{
    s_title_view = HUD_TITLE_UNKNOWN;
    s_title_part = 0;
    s_title_waits = 0;
    s_title_stale = 0xff;
    s_join_drawn = JOIN_UNDRAWN;
}

/* Four digits, right-aligned; leading zeros are spaces, which the font
 * draws as dark cells like the box. Records saturate at SAVE_COUNTER_MAX. */
void hud_count(u8 col, u8 row, u16 value)
{
    char text[5];

    text_count4(text, value);
    hud_text(col, row, HUD_PAL_WHITE, text);
}

/* " LONGEST RALLY        nnnn " */
void hud_record_line(u8 row, const char *label, u16 value)
{
    hud_text(RECORDS_COL + 1, row, HUD_PAL_WHITE, label);
    hud_count(RECORDS_COUNT_COL, row, value);
}

/* The holder's initials before the count (ADR-040), --- for nobody. */
#define RECORDS_INITIALS_COL (RECORDS_COUNT_COL - 4)
static void hud_record_initials(u8 row, const u8 *initials)
{
    static char text[4];

    text[0] = initials[0] ? (char)initials[0] : '-';
    text[1] = initials[0] ? (char)initials[1] : '-';
    text[2] = initials[0] ? (char)initials[2] : '-';
    text[3] = 0;
    hud_text(RECORDS_INITIALS_COL, row, HUD_PAL_YELLOW, text);
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
/* " LADDER     S  4/8  D  2/8 ": each mode's rung, in the W and L columns
 * of the lines above (ADR-023, ADR-055). */
static void hud_record_rung(u8 col, u8 row, u8 mode)
{
    static char text[] = "1/8";

    text[0] = (char)('1' + save_ladder(mode));
    text[2] = (char)('0' + OPPONENT_COUNT);
    hud_text(col, row, HUD_PAL_WHITE, text);
}

static void hud_record_ladder(u8 row)
{
    hud_text(RECORDS_COL + 1, row, HUD_PAL_WHITE, "LADDER");
    hud_text(RECORDS_COUNT_COL - 9, row, HUD_PAL_YELLOW, "S");
    hud_record_rung(RECORDS_COUNT_COL - 6, row, MATCH_SINGLES);
    hud_text(RECORDS_COUNT_COL - 2, row, HUD_PAL_YELLOW, "D");
    hud_record_rung(RECORDS_COUNT_COL + 1, row, MATCH_DOUBLES);
}

/* One menu item; the choice is yellow with the cursor. Items overwrite
 * their cells (the leading spaces cover the cursor). */
static void hud_title_item(u8 item, u8 choice)
{
    static const char *const items[TITLE_ENTRIES] = {
        "  STORY    ", "  SINGLES  ", "  DOUBLES  ", "  OPEN PLAY",
        "  PRACTICE ",
        "  TUTORIAL ", "  CAREER   ", "  RECORDS  ", "  COURT    ", "  OPTIONS  ",
        "  MODIFIERS"
    };

    hud_text(TITLE_MENU_COL, TITLE_MENU_ROW + item,
        item == choice ? HUD_PAL_YELLOW : HUD_PAL_WHITE, items[item]);
    if (item == choice)
        hud_tile(TITLE_MENU_COL, TITLE_MENU_ROW + item,
            hud_entry(HUD_TILE_CURSOR, HUD_PAL_YELLOW));
}

/* Two menu items per call (half 0 to 4). */
static void hud_title_menu(u8 choice, u8 half)
{
    u8 i = half * 2;
    u8 last = i + 2 > TITLE_ENTRIES ? TITLE_ENTRIES : i + 2;

    for (; i < last; ++i)
        hud_title_item(i, choice);
}

static void hud_title_join_line(void)
{
    /* ADR-066: Start on any other controller joins (or leaves). */
    hud_text(TITLE_JOIN_COL, TITLE_JOIN_ROW, HUD_PAL_WHITE, TITLE_JOIN_TEXT);
}

/* What the join counter shows: who joined, and whether 3P and 4P can. */
static u8 hud_join_state(void)
{
    return g_title_joined | (g_multitap_present ? 16 : 0);
}

/* The join counter (ADR-066): the panel on one frame, then a label and a
 * dot (joined) or a grey x per controller, in the ring colours: 1P red,
 * 2P blue, 3P yellow, 4P green (render.c sets the title's BG3 palettes 2
 * and 3 for them). */
static void hud_join_counter(void)
{
    static const char *const labels[MAX_HUMAN_PADS] = {
        "1P", "2P", "3P", "4P"
    };
    static const u16 dots[MAX_HUMAN_PADS] = {
        hud_entry(HUD_TILE_DOT, HUD_PAL_TEAM_RIGHT),
        hud_entry(HUD_TILE_DOT, HUD_PAL_TEAM_LEFT),
        hud_entry(HUD_TILE_DOT, HUD_PAL_YELLOW),
        hud_entry(HUD_TILE_DOT_ALT, HUD_PAL_TEAM_LEFT)
    };
    u8 joined = g_title_joined;
    u8 col = JOIN_FIRST_COL;
    u8 i;

    if (s_join_drawn == JOIN_UNDRAWN) {
        hud_panel(JOIN_PANEL_COL, SCORE_PANEL_ROW, JOIN_PANEL_INNER);
        s_join_drawn = JOIN_PANEL;
        return;
    }
    for (i = 0; i < MAX_HUMAN_PADS; ++i) {
        hud_text(col, SCORE_TEXT_ROW, HUD_PAL_WHITE, labels[i]);
        hud_tile(col + 2, SCORE_TEXT_ROW, (joined & 1) ? dots[i] :
            hud_entry(HUD_TILE_X_OFF, HUD_PAL_WHITE));
        joined >>= 1;
        col += 4;
    }
    s_join_drawn = hud_join_state();
}

/* Title views for the area: the menu choice, or a page: 0x7f + its
 * TITLE_PAGE_*. Joining draws the counter apart (ADR-066). */
#define HUD_TITLE_VIEW_PAGE(page) (0x7f + (page))
#define HUD_TITLE_VIEW_COURT HUD_TITLE_VIEW_PAGE(TITLE_PAGE_COURT)
/* The CAREER page (ADR-047): a view per sub-view, RECORDS among them. */
#define HUD_TITLE_VIEW_CAREER 0x90
/* COURT, OPTIONS, PRACTICE, MODIFIERS (CAREER's views are apart). */
#define HUD_TITLE_VIEW_SETTINGS(v) \
    ((v) >= HUD_TITLE_VIEW_COURT && (v) < HUD_TITLE_VIEW_CAREER)

/* Runs on every idle title frame behind the demo: one test, not one per
 * page. */
static u8 hud_title_view(void)
{
    if (g_title_page == TITLE_PAGE_CAREER)
        return HUD_TITLE_VIEW_CAREER + (g_career_page.view ==
            CAREER_VIEW_EDIT && g_career_page.naming ? HUD_CAREER_NAMING :
            g_career_page.view);
    if (g_title_page != TITLE_PAGE_MENU)
        return HUD_TITLE_VIEW_PAGE(g_title_page);
    return g_title_mode;
}

static u8 hud_records_draw_part(u8 part);

/* One part of the area; returns nonzero when the view is complete. One
 * row per part: the title demo leaves little frame time. */
static u8 hud_title_draw_part(u8 view, u8 part)
{
    if (HUD_TITLE_VIEW_SETTINGS(view))
        return hud_settings_draw_part(part);
    if (view == HUD_TITLE_VIEW_CAREER + CAREER_VIEW_RECORDS)
        return hud_records_draw_part(part);
    if (view >= HUD_TITLE_VIEW_CAREER)
        return hud_career_draw_part(view - HUD_TITLE_VIEW_CAREER, part);
    /* Parts stay at most two rows: each waits for a frame under
     * HUD_TITLE_SCANLINE_LIMIT, which leaves room for no more. */
    if (part < (TITLE_ENTRIES + 1) / 2) {
        hud_title_menu(view, part);
        return 0;
    }
    hud_title_join_line();
    return 1;
}

/* The records table (ADR-021): the title's RECORDS entry (ADR-065). */
static u8 hud_records_draw_part(u8 part)
{
    switch (part) {
    case 0:
        /* The font's space is a dark cell: a solid box over the court. */
        hud_fill_box(RECORDS_COL, TITLE_MENU_ROW, RECORDS_WIDTH, RECORDS_ROWS,
            hud_entry(' ', HUD_PAL_WHITE));
        return 0;
    case 1:
        HUD_PAGE_HEADER(RECORDS_COL, RECORDS_WIDTH, "RECORDS");
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
        hud_record_initials(TITLE_MENU_ROW + 6, g_save.initials.best_streak);
        return 0;
    case 6:
        hud_record_line(TITLE_MENU_ROW + 7, "SHUTOUTS",
            g_save.records.shutouts);
        return 0;
    case 7:
        hud_record_line(TITLE_MENU_ROW + 8, "LONGEST RALLY",
            g_save.records.longest_rally);
        hud_record_initials(TITLE_MENU_ROW + 8,
            g_save.initials.longest_rally);
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
void hud_title_area_update(void)
{
    u8 view = hud_title_view();

    /* A drawn menu whose cursor or 2P line changed: redraw only what
     * changed, in place, at once (two rows are cheap, so this skips the
     * wait for a lighter frame; no clear, so nothing flickers). */
    /* The item the cursor left, a frame after the move: the two items on
     * one frame cost about 140 scanlines, which the title demo's frames do
     * not always have (M4-UI-03). */
    if (s_title_stale != 0xff) {
        /* Coloured by the choice now, so another move since is fine. */
        if (s_title_part == 0xff && s_title_view < HUD_TITLE_VIEW_COURT &&
                view < HUD_TITLE_VIEW_COURT)
            hud_title_item(s_title_stale, view);
        s_title_stale = 0xff;
        return;
    }
    if (s_title_view != view && s_title_part == 0xff &&
            s_title_view < HUD_TITLE_VIEW_COURT &&
            view < HUD_TITLE_VIEW_COURT) {
        hud_title_item(view, view);
        s_title_stale = s_title_view;
        s_title_view = view;
        return;
    }
    if (hud_title_wait(HUD_TITLE_SCANLINE_LIMIT))
        return;
    /* The join counter (ADR-066), between the area's parts. */
    if (s_title_part == 0xff && s_join_drawn != hud_join_state()) {
        hud_join_counter();
        return;
    }

    if (s_title_view == view) {
        if (s_title_part == 0xff) {
            /* A drawn page redraws a changed row; a drawn menu, nothing. */
            if (view < HUD_TITLE_VIEW_COURT)
                return;
            if (HUD_TITLE_VIEW_SETTINGS(view))
                hud_settings_refresh();
            else if (view == HUD_TITLE_VIEW_CAREER + CAREER_VIEW_SLOTS)
                hud_career_slots_refresh();
            else if (view == HUD_TITLE_VIEW_CAREER + CAREER_VIEW_EDIT)
                hud_career_edit_refresh();
            else if (view == HUD_TITLE_VIEW_CAREER + HUD_CAREER_NAMING)
                hud_career_naming_refresh();
            return;
        }
        /* Parts back to back while the frame stays under the limit
         * (M4-UI-01): each fits after it, as the first one does. */
        do {
            if (hud_title_draw_part(view, s_title_part)) {
                s_title_part = 0xff;
                return;
            }
            ++s_title_part;
        } while (profile_scanline() <= HUD_TITLE_SCANLINE_LIMIT);
        return;
    }
    /* Otherwise draw the whole view a part per frame. Menu to menu still
     * needs no clear (every item overwrites its cells); a page does. */
    if (s_title_view != HUD_TITLE_CLEARED &&
            !(s_title_view < HUD_TITLE_VIEW_COURT &&
              view < HUD_TITLE_VIEW_COURT))
        hud_clear_rows(TITLE_MENU_ROW, TITLE_AREA_ROWS);
    s_title_view = view;
    s_title_part = 0;
}
