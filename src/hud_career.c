#include <string.h>
#include "career.h"
#include "career_page.h"
#include "hud.h"
#include "hud_internal.h"
#include "hud_tiles.h"
#include "museum.h"
#include "paddle_stats.h"
#include "save.h"
#include "text.h"

/* ---- The CAREER page (ADR-047, M4-CAREER-02) ---- */

#define CAREER_SLOTS_ROWS 8         /* header, gap, 3 slots, gap, NO PLAYER,
                                       RECORDS */
#define CAREER_EDIT_PAGE_ROWS 9     /* header, gap, 7 rows */
#define CAREER_NAME_COL 8
#define CAREER_UNDRAWN 0xff

/* Slots view rows as drawn: cursor << 7 | active << 6 | used << 5. */
static u8 s_career_slot_drawn[CAREER_SLOT_ROWS];
/* Edit rows as drawn: value | cursor << 7; the name row keeps its own. */
static u8 s_career_edit_drawn[CAREER_EDIT_ROWS];
static u8 s_career_name_drawn[CAREER_NAME + 2];   /* letters, cursor, mode */

static u8 hud_career_slot_state(u8 row)
{
    u8 state = (u8)((g_career_page.row == row) << 7);

    if (row < CAREER_SLOTS) {
        state |= (u8)((g_save.career_active == row) << 6);
        state |= (u8)(g_save.career[row].used << 5);
    } else if (row == CAREER_ROW_NOBODY) {
        state |= (u8)((career_active() == 0) << 6);
    }
    return state;
}

/* "> 1 ACE          12W", "  2 NEW PLAYER", then NO PLAYER (yellow when
 * nobody is active) after a gap. */
static void hud_career_slot_row(u8 row)
{
    static char name[CAREER_NAME + 1];
    const CareerPlayer *player;
    u8 line = TITLE_MENU_ROW + 2 + row + (row >= CAREER_ROW_NOBODY);
    u8 cursor = g_career_page.row == row;
    u8 i;

    hud_fill_box(RECORDS_COL + 1, line, RECORDS_WIDTH - 2, 1,
        hud_entry(' ', HUD_PAL_WHITE));
    hud_tile(RECORDS_COL + 1, line, cursor ?
        hud_entry(HUD_TILE_CURSOR, HUD_PAL_YELLOW) :
        hud_entry(' ', HUD_PAL_WHITE));
    if (row == CAREER_ROW_NOBODY) {
        hud_text(CAREER_NAME_COL, line, career_active() == 0 ?
            HUD_PAL_YELLOW : HUD_PAL_WHITE, "NO PLAYER");
    } else {
        player = &g_save.career[row];
        name[0] = (char)('1' + row);
        name[1] = 0;
        hud_text(RECORDS_COL + 2, line, HUD_PAL_WHITE, name);
        if (player->used) {
            for (i = 0; i < CAREER_NAME; ++i)
                name[i] = (char)player->name[i];
            name[CAREER_NAME] = 0;
            hud_text(CAREER_NAME_COL, line, g_save.career_active == row ?
                HUD_PAL_YELLOW : HUD_PAL_WHITE, name);
            hud_count(RECORDS_COUNT_COL - 1, line,
                player->wins[0] + player->wins[1]);
            hud_text(RECORDS_COUNT_COL + 3, line, HUD_PAL_YELLOW, "W");
        } else {
            hud_text(CAREER_NAME_COL, line, HUD_PAL_WHITE, "NEW PLAYER");
        }
    }
    s_career_slot_drawn[row] = hud_career_slot_state(row);
}

void hud_career_slots_refresh(void)
{
    u8 row;

    for (row = 0; row < CAREER_SLOT_ROWS; ++row)
        if (s_career_slot_drawn[row] != hud_career_slot_state(row)) {
            hud_career_slot_row(row);
            return;
        }
}

/* " SINGLES  W nnnn  L nnnn " for the card's player. */
static void hud_career_mode_line(u8 row, const char *label, u16 wins,
    u16 losses)
{
    hud_text(RECORDS_COL + 1, row, HUD_PAL_WHITE, label);
    hud_text(RECORDS_COUNT_COL - 9, row, HUD_PAL_YELLOW, "W");
    hud_count(RECORDS_COUNT_COL - 7, row, wins);
    hud_text(RECORDS_COUNT_COL - 2, row, HUD_PAL_YELLOW, "L");
    hud_count(RECORDS_COUNT_COL, row, losses);
}

/* "n/m" with one or two digit numbers, written from text; its length. */
static u8 hud_fraction(char *text, u8 count, u8 total)
{
    char *end = text_count(text, count);

    *end++ = '/';
    return (u8)(text_count(end, total) - text);
}

/* "HATS 1/12      HAIR 5/10": what the player may wear, counted as the
 * HAT MUSEUM counts it (ADR-065). */
static void hud_career_unlocks_line(u8 row, const CareerPlayer *player)
{
    static char text[6];
    u8 length;

    hud_text(RECORDS_COL + 1, row, HUD_PAL_WHITE, "HATS");
    hud_fraction(text, career_hats_held(player), MUSEUM_HATS);
    hud_text(RECORDS_COL + 6, row, HUD_PAL_YELLOW, text);
    hud_text(RECORDS_COUNT_COL - 6, row, HUD_PAL_WHITE, "HAIR");
    length = hud_fraction(text, career_hair_held(player), CAREER_HAIRS);
    hud_text(RECORDS_COUNT_COL + 4 - length, row, HUD_PAL_YELLOW, text);
}

/* "NEXT: HEADBAND" and how to earn it, or ALL UNLOCKED! */
static void hud_career_next_lines(u8 row, const CareerPlayer *player)
{
    u8 next = career_next_unlock(player);

    if (next >= CAREER_UNLOCKS) {
        hud_text(RECORDS_COL + 1, row, HUD_PAL_YELLOW, "ALL UNLOCKED!");
        return;
    }
    hud_text(RECORDS_COL + 1, row, HUD_PAL_WHITE, "NEXT:");
    hud_text(RECORDS_COL + 7, row, HUD_PAL_YELLOW,
        g_career_unlock_names[next]);
    hud_text(RECORDS_COL + 1, row + 1, HUD_PAL_WHITE,
        g_career_unlock_how[next]);
}

static void hud_career_line(u8 row, const char *label, u16 value)
{
    hud_text(RECORDS_COL + 1, row, HUD_PAL_WHITE, label);
    hud_count(RECORDS_COUNT_COL, row, value);
}

/* The card: a part per frame, two rows at most. */
static u8 hud_career_card_part(u8 part)
{
    static char name[CAREER_NAME + 1];
    static char rating[17];
    const CareerPlayer *player = &g_save.career[g_career_page.slot];
    u8 i;

    switch (part) {
    case 0:
        hud_fill_box(RECORDS_COL, TITLE_MENU_ROW, RECORDS_WIDTH, RECORDS_ROWS,
            hud_entry(' ', HUD_PAL_WHITE));
        return 0;
    case 1:
        for (i = 0; i < CAREER_NAME; ++i)
            name[i] = (char)player->name[i];
        name[CAREER_NAME] = 0;
        hud_text(HUD_HEADER_COL(RECORDS_COL, RECORDS_WIDTH, CAREER_NAME),
            TITLE_MENU_ROW, HUD_PAL_YELLOW, name);
        /* The park rating (ADR-070), under the name. */
        text_tenths(text_append(rating, "PARK RATING "),
            career_park_rating(player));
        hud_text(RECORDS_COL + 1, TITLE_MENU_ROW + 1, HUD_PAL_WHITE, rating);
        return 0;
    case 2:
        hud_career_mode_line(TITLE_MENU_ROW + 2, "SINGLES", player->wins[0],
            player->losses[0]);
        hud_career_mode_line(TITLE_MENU_ROW + 3, "DOUBLES", player->wins[1],
            player->losses[1]);
        return 0;
    case 3:
        hud_career_line(TITLE_MENU_ROW + 4, "ACES", player->aces);
        hud_career_line(TITLE_MENU_ROW + 5, "SHUTOUTS", player->shutouts);
        return 0;
    case 4:
        hud_career_line(TITLE_MENU_ROW + 6, "BEST STREAK",
            player->best_streak);
        hud_career_line(TITLE_MENU_ROW + 7, "LONGEST RALLY",
            player->longest_rally);
        return 0;
    case 5:
        hud_career_unlocks_line(TITLE_MENU_ROW + 8, player);
        return 0;
    default:
        hud_career_next_lines(TITLE_MENU_ROW + 9, player);
        return 1;
    }
}

/* A look row's display name. */
static const char *hud_career_look_name(const CareerPlayer *player, u8 row)
{
    u8 value = career_look_value(player, row);

    switch (row) {
    case CAREER_EDIT_BODY:
        return career_body_name(value);
    case CAREER_EDIT_SKIN:
        return g_career_skin_names[value % CAREER_SKINS];
    case CAREER_EDIT_HAIR:
        return g_career_hair_names[value % CAREER_HAIRS];
    case CAREER_EDIT_SHIRT:
        return career_shirt_name(value);
    case CAREER_EDIT_HAT:
        return g_career_hat_names[value % CAREER_HATS];
    default:
        return career_paddle_name(value);
    }
}

/* The editor (and the keyboard) sit right of the title demo's P1, who
 * stands still at the left baseline wearing the look. */
#define CAREER_EDIT_COL 8
#define CAREER_EDIT_WIDTH 20
#define CAREER_EDIT_LABEL_COL (CAREER_EDIT_COL + 2)
#define CAREER_EDIT_VALUE_COL (CAREER_EDIT_COL + 9)     /* "<NAME    >" */

/* An editor row: cursor, label, and "<VALUE   >". */
static void hud_career_setting(u8 row, const char *label, const char *name,
    u8 cursor)
{
    char text[11];
    u8 i = 1;

    hud_tile(CAREER_EDIT_LABEL_COL - 1, row, cursor ?
        hud_entry(HUD_TILE_CURSOR, HUD_PAL_YELLOW) :
        hud_entry(' ', HUD_PAL_WHITE));
    hud_text(CAREER_EDIT_LABEL_COL, row,
        cursor ? HUD_PAL_YELLOW : HUD_PAL_WHITE, label);
    text[0] = '<';
    while (*name && i < 9)
        text[i++] = *name++;
    while (i < 9)
        text[i++] = ' ';
    text[9] = '>';
    text[10] = 0;
    hud_text(CAREER_EDIT_VALUE_COL, row, HUD_PAL_WHITE, text);
}

static void hud_career_edit_row(u8 row)
{
    static const char *const labels[CAREER_EDIT_ROWS] = {
        "NAME", "BODY", "SKIN", "HAIR", "SHIRT", "HAT", "PADDLE"
    };
    static char name[CAREER_NAME + 1];
    static char rating[17];
    const CareerPlayer *player = &g_save.career[g_career_page.slot];
    u8 cursor = g_career_page.row == row;
    u8 i;

    if (row == CAREER_EDIT_NAME) {
        for (i = 0; i < CAREER_NAME; ++i)
            name[i] = (char)player->name[i];
        name[CAREER_NAME] = 0;
        hud_career_setting(TITLE_MENU_ROW + 2, labels[0], name, cursor);
        s_career_edit_drawn[row] = (u8)(cursor << 7);
        return;
    }
    hud_career_setting(TITLE_MENU_ROW + 2 + row, labels[row],
        hud_career_look_name(player, row), cursor);
    s_career_edit_drawn[row] = (u8)(career_look_value(player, row) |
        (cursor << 7));
}

/* The look's paddle stats with the hat's gear (stage 4): "PWR 2  CTL 4
 * SPD 3" at the box's foot. */
#define CAREER_STATS_ROW (TITLE_MENU_ROW + 10)
static u8 s_career_stats_drawn;

static u8 hud_career_profile(void)
{
    const CareerPlayer *player = &g_save.career[g_career_page.slot];

    return PADDLE_PROFILE(player->paddle % PADDLE_COUNT,
        player->hat % PADDLE_GEARS);
}

static void hud_career_stats_line(void)
{
    static char text[] = "PWR 0  CTL 0  SPD 0";
    u8 profile = hud_career_profile();

    text[4] = (char)('0' + g_paddle_stats[profile][0]);
    text[11] = (char)('0' + g_paddle_stats[profile][1]);
    text[18] = (char)('0' + g_paddle_stats[profile][2]);
    hud_text(CAREER_EDIT_COL + 1, CAREER_STATS_ROW, HUD_PAL_YELLOW, text);
    s_career_stats_drawn = profile;
}

void hud_career_edit_refresh(void)
{
    const CareerPlayer *player = &g_save.career[g_career_page.slot];
    u8 row;

    if (s_career_stats_drawn != hud_career_profile()) {
        hud_career_stats_line();
        return;
    }
    for (row = 0; row < CAREER_EDIT_ROWS; ++row)
        if (s_career_edit_drawn[row] != (u8)((row == CAREER_EDIT_NAME ? 0 :
                career_look_value(player, row)) |
                ((g_career_page.row == row) << 7))) {
            hud_career_edit_row(row);
            return;
        }
}

/* The keyboard (typing the name): the name with a box per empty place,
 * the letters, then DEL and END; the key picked is yellow. */
#define CAREER_NAME_LINE_COL 14
#define CAREER_KEYS_COL (CAREER_EDIT_COL + 1)
#define CAREER_KEYS_ROW (TITLE_MENU_ROW + 4)
/* The key last drawn picked, and a keyboard row still to redraw (the old
 * key's, when the pick moved to another row), or 0xff. */
static u8 s_career_key_drawn;
static u8 s_career_row_pending;

static void hud_career_name_line(void)
{
    static char letter[2];
    const CareerPlayer *player = &g_save.career[g_career_page.slot];
    u8 cursor = g_career_page.cursor;
    u8 row = TITLE_MENU_ROW + 2;
    u8 i;

    /* Typed letters, then an empty box per place left, the next one
     * yellow (the font has no visible underscore). */
    letter[1] = 0;
    for (i = 0; i < CAREER_NAME; ++i) {
        if (i < cursor) {
            letter[0] = (char)player->name[i];
            hud_text(CAREER_NAME_LINE_COL + i, row, HUD_PAL_WHITE, letter);
        } else {
            hud_tile(CAREER_NAME_LINE_COL + i, row,
                hud_entry(HUD_TILE_BAR_OFF,
                    i == cursor ? HUD_PAL_YELLOW : HUD_PAL_WHITE));
        }
    }
    memcpy(s_career_name_drawn, player->name, CAREER_NAME);
    s_career_name_drawn[CAREER_NAME] = cursor;
}

/* A keyboard row: letters two cells apart (a space shows as _), or DEL
 * and END. */
static void hud_career_key_row(u8 row)
{
    static char key[4];
    u8 picked = career_page_key();
    u8 first = row * CAREER_KEY_COLS;
    u8 i;

    if (row == CAREER_KEY_ROWS - 1) {
        hud_text(CAREER_KEYS_COL + 2, CAREER_KEYS_ROW + row,
            picked == CAREER_KEY_DEL ? HUD_PAL_YELLOW : HUD_PAL_WHITE, "DEL");
        hud_text(CAREER_KEYS_COL + 12, CAREER_KEYS_ROW + row,
            picked == CAREER_KEY_END ? HUD_PAL_YELLOW : HUD_PAL_WHITE, "END");
        return;
    }
    key[1] = 0;
    for (i = 0; i < CAREER_KEY_COLS && first + i < CAREER_KEY_CHARS; ++i) {
        /* The space key is an empty box, like an empty place. */
        if (g_career_key_chars[first + i] == ' ') {
            hud_tile(CAREER_KEYS_COL + 2 * i, CAREER_KEYS_ROW + row,
                hud_entry(HUD_TILE_BAR_OFF, first + i == picked ?
                    HUD_PAL_YELLOW : HUD_PAL_WHITE));
            continue;
        }
        key[0] = g_career_key_chars[first + i];
        hud_text(CAREER_KEYS_COL + 2 * i, CAREER_KEYS_ROW + row,
            first + i == picked ? HUD_PAL_YELLOW : HUD_PAL_WHITE, key);
    }
}

/* The keyboard row a key is on. */
static u8 hud_career_key_row_of(u8 key)
{
    if (key >= CAREER_KEY_CHARS)
        return CAREER_KEY_ROWS - 1;
    return key / CAREER_KEY_COLS;
}

void hud_career_naming_refresh(void)
{
    const CareerPlayer *player = &g_save.career[g_career_page.slot];
    u8 picked = career_page_key();
    u8 old = s_career_key_drawn;

    if (memcmp(s_career_name_drawn, player->name, CAREER_NAME) != 0 ||
            s_career_name_drawn[CAREER_NAME] != g_career_page.cursor) {
        hud_career_name_line();
        return;
    }
    /* The old key's row, a frame after the new one's. */
    if (s_career_row_pending != 0xff) {
        hud_career_key_row(s_career_row_pending);
        s_career_row_pending = 0xff;
        return;
    }
    if (old == picked)
        return;
    hud_career_key_row(hud_career_key_row_of(picked));
    if (hud_career_key_row_of(old) != hud_career_key_row_of(picked))
        s_career_row_pending = hud_career_key_row_of(old);
    s_career_key_drawn = picked;
}

static u8 hud_career_naming_part(u8 part)
{
    if (part == 0) {
        hud_fill_box(CAREER_EDIT_COL, TITLE_MENU_ROW, CAREER_EDIT_WIDTH,
            CAREER_EDIT_PAGE_ROWS, hud_entry(' ', HUD_PAL_WHITE));
        s_career_row_pending = 0xff;
        return 0;
    }
    if (part == 1) {
        HUD_PAGE_HEADER(CAREER_EDIT_COL, CAREER_EDIT_WIDTH, "NAME");
        hud_career_name_line();
        return 0;
    }
    hud_career_key_row(part - 2);
    if (part - 2 == CAREER_KEY_ROWS - 1) {
        s_career_key_drawn = career_page_key();
        return 1;
    }
    return 0;
}

/* One part of a CAREER view. */
u8 hud_career_draw_part(u8 view, u8 part)
{
    u8 i;

    if (view == CAREER_VIEW_CARD)
        return hud_career_card_part(part);
    if (view == HUD_CAREER_NAMING)
        return hud_career_naming_part(part);
    if (view == CAREER_VIEW_EDIT) {
        if (part == 0) {
            /* Two more rows than the keyboard's: the stats line. */
            hud_fill_box(CAREER_EDIT_COL, TITLE_MENU_ROW, CAREER_EDIT_WIDTH,
                CAREER_EDIT_PAGE_ROWS + 2, hud_entry(' ', HUD_PAL_WHITE));
            for (i = 0; i < CAREER_EDIT_ROWS; ++i)
                s_career_edit_drawn[i] = CAREER_UNDRAWN;
            return 0;
        }
        if (part == 1) {
            HUD_PAGE_HEADER(CAREER_EDIT_COL, CAREER_EDIT_WIDTH, "PLAYER");
            return 0;
        }
        if (part - 2 < CAREER_EDIT_ROWS) {
            hud_career_edit_row(part - 2);
            return 0;
        }
        hud_career_stats_line();
        return 1;
    }
    /* SLOTS */
    if (part == 0) {
        hud_fill_box(RECORDS_COL, TITLE_MENU_ROW, RECORDS_WIDTH,
            CAREER_SLOTS_ROWS, hud_entry(' ', HUD_PAL_WHITE));
        for (i = 0; i < CAREER_SLOT_ROWS; ++i)
            s_career_slot_drawn[i] = CAREER_UNDRAWN;
        return 0;
    }
    if (part == 1) {
        HUD_PAGE_HEADER(RECORDS_COL, RECORDS_WIDTH, "CAREER");
        return 0;
    }
    hud_career_slot_row(part - 2);
    return part - 2 == CAREER_SLOT_ROWS - 1;
}
