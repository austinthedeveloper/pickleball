#ifndef COURT_STYLE_H
#define COURT_STYLE_H

#include "game_types.h"
#include "court_colors.h"

/*
 * Court colours (docs/decisions/ADR-022-court-colors.md). The choice is one
 * preset per part (surface, surround, lines), kept in the save; preset 0 is
 * DEFAULT, the painted colours. Pure: the renderer uploads the result.
 */

enum { COURT_GROUND_COLORS = 16 };      /* BG1 palette 2 */

/* The ground palette for a choice: painted with each part's entries
 * replaced. Out-of-range choices count as DEFAULT. */
void court_style_compose(u16 out[COURT_GROUND_COLORS],
    const u16 painted[COURT_GROUND_COLORS], const u8 choice[COURT_PARTS]);
/* Display name of a part's preset (at most 8 characters). */
const char *court_style_name(u8 part, u8 preset);

/* court_edit_input results. */
enum {
    COURT_EDIT_STAY = 0,
    COURT_EDIT_DONE = 1,        /* keep the choice (save it if it changed) */
    COURT_EDIT_CANCEL = 2       /* the choice is back to what it was */
};

typedef struct {
    u8 row;                     /* COURT_PART_* with the cursor */
    u8 original[COURT_PARTS];   /* the choice when the page opened */
    u8 recolor;                 /* nonzero after a change; the caller
                                   re-uploads the palette and clears it */
} CourtEdit;

extern CourtEdit g_court_edit;

/* Opens the editor on choice (edited in place). */
void court_edit_begin(u8 choice[COURT_PARTS]);
/* One frame of newly pressed buttons. Up/Down rows, Left/Right presets
 * (both wrap), B or Start keeps, Y cancels. */
u8 court_edit_input(u8 choice[COURT_PARTS], u16 pressed);
/* Nonzero when choice differs from the one the page opened with. */
u8 court_edit_changed(const u8 choice[COURT_PARTS]);

#endif
