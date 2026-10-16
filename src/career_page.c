#include <string.h>
#include "career_page.h"
#include "input.h"
#include "paddle_stats.h"
#include "palette_parts.h"
#include "player.h"

CareerPage g_career_page;

static const char *const s_body_names[2] = { "MALE", "FEMALE" };
static const char *const s_shirt_names[PAL_COUNT] = {
    "BLUE", "RED", "GREEN", "GOLD"
};
/* paddles.json order (ADR-020). */
static const char *const s_paddle_names[PADDLE_COUNT] = {
    "WOOD", "BLUE", "RED", "GOLD"
};

const char *career_body_name(u8 variant)
{
    return s_body_names[variant & 1];
}

const char *career_shirt_name(u8 shirt)
{
    return s_shirt_names[shirt % PAL_COUNT];
}

const char *career_paddle_name(u8 paddle)
{
    return s_paddle_names[paddle % PADDLE_COUNT];
}

static CareerPlayer *page_player(void)
{
    return &g_save.career[g_career_page.slot];
}

u8 career_look_value(const CareerPlayer *player, u8 row)
{
    switch (row) {
    case CAREER_EDIT_BODY:
        return player->variant;
    case CAREER_EDIT_SKIN:
        return player->skin;
    case CAREER_EDIT_HAIR:
        return player->hair;
    case CAREER_EDIT_SHIRT:
        return player->shirt;
    case CAREER_EDIT_HAT:
        return player->hat;
    case CAREER_EDIT_PADDLE:
        return player->paddle;
    default:
        return 0;
    }
}

u8 career_next_unlock(const CareerPlayer *player)
{
    u8 i;

    for (i = 0; i < CAREER_UNLOCKS; ++i)
        if (((player->unlocked >> i) & 1) == 0)
            return i;
    return CAREER_UNLOCKS;
}

u8 career_unlock_count(const CareerPlayer *player)
{
    u8 count = 0;
    u8 i;

    for (i = 0; i < CAREER_UNLOCKS; ++i)
        count += (player->unlocked >> i) & 1;
    return count;
}

void career_page_begin(void)
{
    g_career_page.view = CAREER_VIEW_SLOTS;
    g_career_page.row = g_save.career_active < CAREER_SLOTS ?
        g_save.career_active : 0;
    g_career_page.slot = 0;
    g_career_page.naming = 0;
    g_career_page.cursor = 0;
    g_career_page.creating = 0;
    g_career_page.held_frames = 0;
    g_career_page.look_changed = 0;
}

/* Up/Down through count rows, wrapping. */
static u8 page_row(u8 row, u8 count, u16 pressed)
{
    if (pressed & PAD_UP)
        return row ? row - 1 : count - 1;
    if (pressed & PAD_DOWN)
        return row + 1 >= count ? 0 : row + 1;
    return row;
}

/* Left/Right through count values, wrapping. */
static u8 page_step(u8 value, u8 count, u16 pressed)
{
    if (pressed & PAD_LEFT)
        return value ? value - 1 : count - 1;
    return value + 1 >= count ? 0 : value + 1;
}

static u8 page_erase_combo(u16 pressed, u16 held)
{
    return (held & (PAD_L | PAD_R)) == (PAD_L | PAD_R) && (pressed & PAD_X);
}

static void page_open_edit(u8 creating)
{
    g_career_page.view = CAREER_VIEW_EDIT;
    g_career_page.row = CAREER_EDIT_NAME;
    g_career_page.creating = creating;
    g_career_page.naming = creating;
    g_career_page.cursor = 0;
    g_career_page.held_frames = 0;
    g_career_page.original = *page_player();
    g_career_page.look_changed = 1;
}

static u8 page_slots(u16 pressed, u16 held)
{
    u8 row = g_career_page.row;

    (void)held;
    if (pressed & PAD_Y)
        return CAREER_PAGE_CLOSE;
    if (pressed & (PAD_B | PAD_START)) {
        if (row == CAREER_ROW_RECORDS) {
            g_career_page.view = CAREER_VIEW_RECORDS;
            return CAREER_PAGE_STAY;
        }
        g_career_page.slot = row;
        if (g_save.career[row].used) {
            career_select(row);
            g_career_page.view = CAREER_VIEW_CARD;
            return CAREER_PAGE_SAVE;
        }
        /* A new player: named first ("A", then typed). */
        career_create(row, "A");
        page_open_edit(1);
        return CAREER_PAGE_STAY;
    }
    g_career_page.row = page_row(row, CAREER_SLOT_ROWS, pressed);
    return CAREER_PAGE_STAY;
}

static u8 page_card(u16 pressed, u16 held)
{
    if (page_erase_combo(pressed, held)) {
        career_delete(g_career_page.slot);
        g_career_page.view = CAREER_VIEW_SLOTS;
        g_career_page.row = g_career_page.slot;
        return CAREER_PAGE_SAVE;
    }
    if (pressed & PAD_Y) {
        g_career_page.view = CAREER_VIEW_SLOTS;
        g_career_page.row = g_career_page.slot;
        return CAREER_PAGE_STAY;
    }
    if (pressed & (PAD_B | PAD_START))
        page_open_edit(0);
    return CAREER_PAGE_STAY;
}

static u8 page_records(u16 pressed, u16 held)
{
    if (page_erase_combo(pressed, held)) {
        save_clear_records();               /* writes */
        g_career_page.look_changed = 1;     /* the table redraws */
        return CAREER_PAGE_STAY;
    }
    if (pressed & (PAD_B | PAD_Y | PAD_START)) {
        g_career_page.view = CAREER_VIEW_SLOTS;
        g_career_page.row = CAREER_ROW_RECORDS;
    }
    return CAREER_PAGE_STAY;
}

/* The next letter: A-Z, then a space, around. */
static u8 page_letter(u8 letter, s16 step)
{
    if (letter == ' ')
        return step > 0 ? 'A' : 'Z';
    if (step > 0)
        return letter == 'Z' ? ' ' : letter + 1;
    return letter == 'A' ? ' ' : letter - 1;
}

/* A name of spaces only is not a name: the first letter becomes A. */
static void page_name_done(CareerPlayer *player)
{
    u8 i;

    g_career_page.naming = 0;
    for (i = 0; i < CAREER_NAME; ++i)
        if (player->name[i] != ' ')
            return;
    player->name[0] = 'A';
}

static u8 page_naming(CareerPlayer *player, u16 pressed, u16 held)
{
    u8 *letter = &player->name[g_career_page.cursor];
    u16 vertical = held & (PAD_UP | PAD_DOWN);

    if (pressed & (PAD_UP | PAD_DOWN)) {
        *letter = page_letter(*letter, (pressed & PAD_UP) ? 1 : -1);
        g_career_page.held_frames = 0;
        g_career_page.look_changed = 1;
    } else if (vertical) {
        if (++g_career_page.held_frames >= CAREER_REPEAT_DELAY) {
            g_career_page.held_frames -= CAREER_REPEAT_FRAMES;
            *letter = page_letter(*letter, (vertical & PAD_UP) ? 1 : -1);
            g_career_page.look_changed = 1;
        }
    } else {
        g_career_page.held_frames = 0;
    }
    if (pressed & PAD_START) {
        page_name_done(player);
    } else if (pressed & (PAD_B | PAD_A | PAD_RIGHT)) {
        if (g_career_page.cursor + 1 >= CAREER_NAME)
            page_name_done(player);
        else
            ++g_career_page.cursor;
    } else if ((pressed & (PAD_Y | PAD_LEFT)) && g_career_page.cursor) {
        --g_career_page.cursor;
    }
    return CAREER_PAGE_STAY;
}

/* Left/Right on a look row; hair and hats skip what is locked. */
static void page_change(CareerPlayer *player, u8 row, u16 pressed)
{
    u8 tries;

    switch (row) {
    case CAREER_EDIT_BODY:
        player->variant = page_step(player->variant, 2, pressed);
        break;
    case CAREER_EDIT_SKIN:
        player->skin = page_step(player->skin, CAREER_SKINS, pressed);
        break;
    case CAREER_EDIT_HAIR:
        tries = CAREER_HAIRS;
        do {
            player->hair = page_step(player->hair, CAREER_HAIRS, pressed);
        } while (career_hair_unlocked(player, player->hair) == 0 && --tries);
        break;
    case CAREER_EDIT_SHIRT:
        player->shirt = page_step(player->shirt, PAL_COUNT, pressed);
        break;
    case CAREER_EDIT_HAT:
        tries = CAREER_HATS;
        do {
            player->hat = page_step(player->hat, CAREER_HATS, pressed);
        } while (career_hat_unlocked(player, player->hat) == 0 && --tries);
        break;
    case CAREER_EDIT_PADDLE:
        player->paddle = page_step(player->paddle, PADDLE_COUNT, pressed);
        break;
    default:
        return;
    }
    g_career_page.look_changed = 1;
}

static u8 page_edit(u16 pressed, u16 held)
{
    CareerPlayer *player = page_player();
    u8 row = g_career_page.row;

    if (g_career_page.naming)
        return page_naming(player, pressed, held);
    if (pressed & PAD_Y) {
        g_career_page.look_changed = 1;
        if (g_career_page.creating) {
            career_delete(g_career_page.slot);
            g_career_page.view = CAREER_VIEW_SLOTS;
            g_career_page.row = g_career_page.slot;
            g_career_page.creating = 0;
            return CAREER_PAGE_STAY;
        }
        *player = g_career_page.original;
        g_career_page.view = CAREER_VIEW_CARD;
        return CAREER_PAGE_STAY;
    }
    if (pressed & (PAD_B | PAD_START)) {
        if (row == CAREER_EDIT_NAME && (pressed & PAD_B)) {
            g_career_page.naming = 1;
            g_career_page.cursor = 0;
            g_career_page.held_frames = 0;
            return CAREER_PAGE_STAY;
        }
        g_career_page.creating = 0;
        g_career_page.view = CAREER_VIEW_CARD;
        return CAREER_PAGE_SAVE;
    }
    if (pressed & (PAD_LEFT | PAD_RIGHT)) {
        page_change(player, row, pressed);
        return CAREER_PAGE_STAY;
    }
    g_career_page.row = page_row(row, CAREER_EDIT_ROWS, pressed);
    return CAREER_PAGE_STAY;
}

u8 career_page_input(u16 pressed, u16 held)
{
    switch (g_career_page.view) {
    case CAREER_VIEW_CARD:
        return page_card(pressed, held);
    case CAREER_VIEW_EDIT:
        return page_edit(pressed, held);
    case CAREER_VIEW_RECORDS:
        return page_records(pressed, held);
    default:
        return page_slots(pressed, held);
    }
}
