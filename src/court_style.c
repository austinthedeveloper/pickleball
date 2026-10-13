#define COURT_COLORS_DEFINE
#include "court_style.h"
#include "input.h"

CourtEdit g_court_edit;

static u8 preset_index(u8 part, u8 preset)
{
    if (preset >= g_court_part_count[part])
        preset = 0;
    return g_court_part_first[part] + preset;
}

void court_style_compose(u16 out[COURT_GROUND_COLORS],
    const u16 painted[COURT_GROUND_COLORS], const u8 choice[COURT_PARTS])
{
    u8 part;
    u8 i;

    for (i = 0; i < COURT_GROUND_COLORS; ++i)
        out[i] = painted[i];
    for (part = 0; part < COURT_PARTS; ++part) {
        const u16 *colors = g_court_preset_colors[preset_index(part,
            choice[part])];
        const u8 *entries = g_court_part_entries[part];

        for (i = 0; i < g_court_part_size[part]; ++i)
            out[entries[i]] = colors[i];
    }
}

const char *court_style_name(u8 part, u8 preset)
{
    return g_court_preset_names[preset_index(part, preset)];
}

void court_edit_begin(u8 choice[COURT_PARTS])
{
    u8 part;

    g_court_edit.row = COURT_PART_SURFACE;
    for (part = 0; part < COURT_PARTS; ++part) {
        if (choice[part] >= g_court_part_count[part])
            choice[part] = 0;
        g_court_edit.original[part] = choice[part];
    }
    g_court_edit.recolor = 0;
}

u8 court_edit_changed(const u8 choice[COURT_PARTS])
{
    u8 part;

    for (part = 0; part < COURT_PARTS; ++part)
        if (choice[part] != g_court_edit.original[part])
            return 1;
    return 0;
}

u8 court_edit_input(u8 choice[COURT_PARTS], u16 pressed)
{
    u8 row = g_court_edit.row;
    u8 count = g_court_part_count[row];
    u8 part;

    if (pressed & PAD_Y) {
        for (part = 0; part < COURT_PARTS; ++part)
            choice[part] = g_court_edit.original[part];
        g_court_edit.recolor = 1;
        return COURT_EDIT_CANCEL;
    }
    if (pressed & (PAD_B | PAD_START))
        return COURT_EDIT_DONE;
    if (pressed & PAD_UP)
        g_court_edit.row = row ? row - 1 : COURT_PARTS - 1;
    else if (pressed & PAD_DOWN)
        g_court_edit.row = row == COURT_PARTS - 1 ? 0 : row + 1;
    else if (pressed & PAD_LEFT) {
        choice[row] = choice[row] ? choice[row] - 1 : count - 1;
        g_court_edit.recolor = 1;
    } else if (pressed & PAD_RIGHT) {
        choice[row] = choice[row] + 1 >= count ? 0 : choice[row] + 1;
        g_court_edit.recolor = 1;
    }
    return COURT_EDIT_STAY;
}
