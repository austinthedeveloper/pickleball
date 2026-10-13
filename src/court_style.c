#define COURT_COLORS_DEFINE
#define VENUES_DEFINE
#include "court_style.h"
#include "input.h"
#include "opponents.h"

CourtEdit g_court_edit;

static u8 preset_index(u8 part, u8 preset)
{
    if (preset >= g_court_part_count[part])
        preset = 0;
    return g_court_part_first[part] + preset;
}

void court_style_compose(u16 out[COURT_GROUND_COLORS],
    const u16 painted[COURT_GROUND_COLORS], const u8 choice[COURT_PARTS],
    u8 venue)
{
    u8 part;
    u8 preset;
    u8 i;

    if (venue >= VENUE_COUNT)
        venue = 0;
    for (i = 0; i < COURT_GROUND_COLORS; ++i)
        out[i] = painted[i];
    for (part = 0; part < COURT_PARTS; ++part) {
        const u16 *colors;
        const u8 *entries = g_court_part_entries[part];

        preset = choice[part];
        if (preset == 0 || preset >= g_court_part_count[part])
            preset = g_venue_ground[venue][part];
        colors = g_court_preset_colors[preset_index(part, preset)];
        for (i = 0; i < g_court_part_size[part]; ++i)
            out[entries[i]] = colors[i];
    }
}

const char *court_style_name(u8 part, u8 preset)
{
    return g_court_preset_names[preset_index(part, preset)];
}

u8 court_style_venue(u8 saved, u8 opponent)
{
    if (saved != COURT_VENUE_AUTO && saved <= VENUE_COUNT)
        return saved - 1;
    if (opponent < OPPONENT_COUNT)
        return g_opponent_venue[opponent];
    return 0;
}

const char *court_style_venue_name(u8 saved)
{
    if (saved == COURT_VENUE_AUTO || saved > VENUE_COUNT)
        return "AUTO";
    return g_venue_names[saved - 1];
}

void court_edit_begin(u8 choice[COURT_PARTS], u8 *venue)
{
    u8 part;

    g_court_edit.row = COURT_ROW_VENUE;
    for (part = 0; part < COURT_PARTS; ++part) {
        if (choice[part] >= g_court_part_count[part])
            choice[part] = 0;
        g_court_edit.original[part] = choice[part];
    }
    if (*venue > VENUE_COUNT)
        *venue = COURT_VENUE_AUTO;
    g_court_edit.original_venue = *venue;
    g_court_edit.recolor = 0;
}

u8 court_edit_changed(const u8 choice[COURT_PARTS], u8 venue)
{
    u8 part;

    if (venue != g_court_edit.original_venue)
        return 1;
    for (part = 0; part < COURT_PARTS; ++part)
        if (choice[part] != g_court_edit.original[part])
            return 1;
    return 0;
}

/* Left/Right on a value with count settings, wrapping. */
static u8 step_value(u8 value, u8 count, u16 pressed)
{
    if (pressed & PAD_LEFT)
        return value ? value - 1 : count - 1;
    return value + 1 >= count ? 0 : value + 1;
}

u8 court_edit_input(u8 choice[COURT_PARTS], u8 *venue, u16 pressed)
{
    u8 row = g_court_edit.row;
    u8 part;

    if (pressed & PAD_Y) {
        for (part = 0; part < COURT_PARTS; ++part)
            choice[part] = g_court_edit.original[part];
        *venue = g_court_edit.original_venue;
        g_court_edit.recolor = 1;
        return COURT_EDIT_CANCEL;
    }
    if (pressed & (PAD_B | PAD_START))
        return COURT_EDIT_DONE;
    if (pressed & PAD_UP)
        g_court_edit.row = row ? row - 1 : COURT_ROWS - 1;
    else if (pressed & PAD_DOWN)
        g_court_edit.row = row == COURT_ROWS - 1 ? 0 : row + 1;
    else if (pressed & (PAD_LEFT | PAD_RIGHT)) {
        if (row == COURT_ROW_VENUE) {
            *venue = step_value(*venue, VENUE_COUNT + 1, pressed);
        } else {
            part = row - 1;
            choice[part] = step_value(choice[part], g_court_part_count[part],
                pressed);
            g_court_edit.recolor = 1;
        }
    }
    return COURT_EDIT_STAY;
}
