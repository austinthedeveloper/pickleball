#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "court_style.h"
#include "input.h"
#include "opponents.h"

/* ADR-022 court colours: composition, names, and the editor. */

static u16 painted[COURT_GROUND_COLORS];

/* Palette 2 of the generated court palette (run from the repo root). */
static void load_painted(void)
{
    FILE *f = fopen("assets/generated/court.pal", "rb");
    u8 bytes[COURT_GROUND_COLORS * 2];
    u8 i;

    assert(f);
    assert(fread(bytes, 1, sizeof(bytes), f) == sizeof(bytes));
    fclose(f);
    for (i = 0; i < COURT_GROUND_COLORS; ++i)
        painted[i] = (u16)(bytes[i * 2] | (bytes[i * 2 + 1] << 8));
}

static void test_default_is_painted(void)
{
    u16 out[COURT_GROUND_COLORS];
    u8 choice[COURT_PARTS] = { 0, 0, 0 };

    /* DEFAULT presets hold the painted colours of court_scene.png, so the
     * default choice changes nothing. */
    load_painted();
    court_style_compose(out, painted, choice, 0);
    assert(memcmp(out, painted, sizeof(out)) == 0);
    assert(strcmp(court_style_name(COURT_PART_SURFACE, 0), "DEFAULT") == 0);
}

static void test_presets_replace_only_their_part(void)
{
    u16 out[COURT_GROUND_COLORS];
    u8 choice[COURT_PARTS] = { 1, 0, 0 };
    const u8 *entries = g_court_part_entries[COURT_PART_SURFACE];
    u8 i;
    u8 touched[COURT_GROUND_COLORS] = { 0 };

    court_style_compose(out, painted, choice, 0);
    for (i = 0; i < g_court_part_size[COURT_PART_SURFACE]; ++i) {
        touched[entries[i]] = 1;
        assert(out[entries[i]] ==
            g_court_preset_colors[g_court_part_first[COURT_PART_SURFACE] + 1][i]);
        assert(out[entries[i]] != painted[entries[i]]);
    }
    for (i = 0; i < COURT_GROUND_COLORS; ++i)
        if (!touched[i])
            assert(out[i] == painted[i]);

    /* Entry 0 (transparent) and the net/fence entries are never parts. */
    assert(!touched[0]);

    /* Out of range (a corrupted or newer save) counts as DEFAULT. */
    choice[COURT_PART_SURFACE] = 200;
    court_style_compose(out, painted, choice, 0);
    assert(memcmp(out, painted, sizeof(out)) == 0);
    assert(strcmp(court_style_name(COURT_PART_SURFACE, 200), "DEFAULT") == 0);
}

static void test_names_fit(void)
{
    u8 part;
    u8 preset;

    for (part = 0; part < COURT_PARTS; ++part) {
        assert(g_court_part_count[part] >= 2);
        for (preset = 0; preset < g_court_part_count[part]; ++preset)
            assert(strlen(court_style_name(part, preset)) <= 8);
    }
}

/* ADR-024: DEFAULT means the venue's ground; the venue follows AUTO. */
static void test_venues(void)
{
    u16 out[COURT_GROUND_COLORS];
    u16 expect[COURT_GROUND_COLORS];
    u8 choice[COURT_PARTS] = { 0, 0, 0 };
    u8 venue;
    u8 o;

    for (venue = 0; venue < VENUE_COUNT; ++venue) {
        court_style_compose(out, painted, choice, venue);
        court_style_compose(expect, painted, g_venue_ground[venue], 0);
        assert(memcmp(out, expect, sizeof(out)) == 0);
    }
    /* An explicit choice wins over the venue's ground. */
    choice[COURT_PART_SURROUND] = 2;
    court_style_compose(out, painted, choice, VENUE_COUNT - 1);
    assert(out[g_court_part_entries[COURT_PART_SURROUND][0]] ==
        g_court_preset_colors[g_court_part_first[COURT_PART_SURROUND] + 2][0]);

    /* AUTO: the opponent's home, else the first venue; a saved venue wins. */
    assert(court_style_venue(COURT_VENUE_AUTO, OPPONENT_NONE) == 0);
    for (o = 0; o < OPPONENT_COUNT; ++o) {
        assert(court_style_venue(COURT_VENUE_AUTO, o) == g_opponent_venue[o]);
        assert(g_opponent_venue[o] < VENUE_COUNT);
    }
    assert(court_style_venue(VENUE_COUNT, 0) == VENUE_COUNT - 1);
    assert(court_style_venue(VENUE_COUNT + 5, OPPONENT_NONE) == 0);
    assert(strcmp(court_style_venue_name(COURT_VENUE_AUTO), "AUTO") == 0);
    assert(strcmp(court_style_venue_name(1), g_venue_names[0]) == 0);
}

static void test_editor(void)
{
    u8 choice[COURT_PARTS] = { 0, 9, 1 };   /* surround out of range */
    u8 venue = 200;                         /* out of range */
    u8 surfaces = g_court_part_count[COURT_PART_SURFACE];

    court_edit_begin(choice, &venue);
    assert(choice[COURT_PART_SURROUND] == 0);   /* clamped on open */
    assert(venue == COURT_VENUE_AUTO);
    assert(g_court_edit.row == COURT_ROW_VENUE);
    assert(!court_edit_changed(choice, venue));

    /* The venue row: AUTO, then each venue, wrapping; no recolour. */
    court_edit_input(choice, &venue, PAD_LEFT);
    assert(venue == VENUE_COUNT);
    assert(!g_court_edit.recolor);
    court_edit_input(choice, &venue, PAD_RIGHT);
    assert(venue == COURT_VENUE_AUTO);
    court_edit_input(choice, &venue, PAD_RIGHT);
    assert(venue == 1 && court_edit_changed(choice, venue));

    /* Down to SURFACE: Left wraps to the last preset and recolours. */
    court_edit_input(choice, &venue, PAD_DOWN);
    assert(g_court_edit.row == COURT_PART_SURFACE + 1);
    assert(court_edit_input(choice, &venue, PAD_LEFT) == COURT_EDIT_STAY);
    assert(choice[COURT_PART_SURFACE] == surfaces - 1);
    assert(g_court_edit.recolor);
    g_court_edit.recolor = 0;
    court_edit_input(choice, &venue, PAD_RIGHT);
    court_edit_input(choice, &venue, PAD_RIGHT);
    assert(choice[COURT_PART_SURFACE] == 1);

    /* Rows wrap; Left/Right change only the cursor's row. */
    court_edit_input(choice, &venue, PAD_UP);
    court_edit_input(choice, &venue, PAD_UP);
    assert(g_court_edit.row == COURT_ROWS - 1);
    court_edit_input(choice, &venue, PAD_RIGHT);
    assert(choice[COURT_PART_LINES] == 2);
    court_edit_input(choice, &venue, PAD_DOWN);
    assert(g_court_edit.row == COURT_ROW_VENUE);

    /* Y puts everything back. */
    assert(court_edit_input(choice, &venue, PAD_Y) == COURT_EDIT_CANCEL);
    assert(choice[COURT_PART_SURFACE] == 0 && choice[COURT_PART_LINES] == 1);
    assert(venue == COURT_VENUE_AUTO);
    assert(!court_edit_changed(choice, venue));

    /* B keeps. */
    court_edit_begin(choice, &venue);
    court_edit_input(choice, &venue, PAD_RIGHT);
    assert(court_edit_input(choice, &venue, PAD_B) == COURT_EDIT_DONE);
    assert(venue == 1 && court_edit_changed(choice, venue));
}

int main(void)
{
    test_default_is_painted();
    test_presets_replace_only_their_part();
    test_names_fit();
    test_venues();
    test_editor();
    printf("court style host checks passed\n");
    return 0;
}
