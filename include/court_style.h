#ifndef COURT_STYLE_H
#define COURT_STYLE_H

#include "game_types.h"
#include "court_colors.h"
#include "venues.h"

/*
 * Court colours (docs/decisions/ADR-022-court-colors.md) and venues
 * (ADR-024). The colour choice is one preset per part (surface, surround,
 * lines), kept in the save; preset 0 is DEFAULT, the venue's own ground
 * colours. The venue choice is 0 AUTO (the opponent's home court, else the
 * first venue) or venue + 1. Pure: the renderer loads and uploads.
 */

enum { COURT_GROUND_COLORS = 16 };      /* BG1 palette 2 */

enum { COURT_VENUE_AUTO = 0 };

/* The ground palette for a choice at a venue: painted, with each part's
 * entries replaced. DEFAULT and out-of-range choices take the venue's
 * preset for that part. */
void court_style_compose(u16 out[COURT_GROUND_COLORS],
    const u16 painted[COURT_GROUND_COLORS], const u8 choice[COURT_PARTS],
    u8 venue);
/* Display name of a part's preset (at most 8 characters). */
const char *court_style_name(u8 part, u8 preset);
/* The venue to load: the saved choice, or for AUTO the opponent's home
 * court (OPPONENT_NONE: the first venue). */
u8 court_style_venue(u8 saved, u8 opponent);
/* "AUTO" or the venue's name. */
const char *court_style_venue_name(u8 saved);

/* court_edit_input results. */
enum {
    COURT_EDIT_STAY = 0,
    COURT_EDIT_DONE = 1,        /* keep the choice (save it if it changed) */
    COURT_EDIT_CANCEL = 2       /* the choice is back to what it was */
};

/* Editor rows: the venue, then one per part (row - 1 = COURT_PART_*). */
enum { COURT_ROW_VENUE = 0, COURT_ROWS = COURT_PARTS + 1 };

typedef struct {
    u8 row;                     /* COURT_ROW_VENUE or part + 1 */
    u8 original[COURT_PARTS];   /* the choice when the page opened */
    u8 original_venue;
    u8 recolor;                 /* nonzero after a colour change; the caller
                                   re-uploads the palette and clears it.
                                   A venue change applies on DONE. */
} CourtEdit;

extern CourtEdit g_court_edit;

/* Opens the editor on choice and venue (edited in place). */
void court_edit_begin(u8 choice[COURT_PARTS], u8 *venue);
/* One frame of newly pressed buttons. Up/Down rows, Left/Right presets or
 * venues (both wrap), B or Start keeps, Y cancels. */
u8 court_edit_input(u8 choice[COURT_PARTS], u8 *venue, u16 pressed);
/* Nonzero when choice or venue differs from when the page opened. */
u8 court_edit_changed(const u8 choice[COURT_PARTS], u8 venue);

#endif
