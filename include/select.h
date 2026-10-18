#ifndef SELECT_H
#define SELECT_H

#include "game_types.h"
#include "match.h"

/* Player Select logic (docs/decisions/ADR-020-player-select.md). Pure: the
 * screen itself is src/render_select.c. */

enum {
    SELECT_ROW_VARIANT = 0,     /* MALE / FEMALE */
    SELECT_ROW_COLOUR = 1,      /* BLUE RED GREEN GOLD */
    SELECT_ROW_PADDLE = 2,      /* WOOD BLUE RED GOLD */
    SELECT_ROWS = 3
};

/* select_input results. */
enum {
    SELECT_STAY = 0,
    SELECT_DONE = 1,            /* every human confirmed: start the match */
    SELECT_BACK = 2             /* 1P backed out: return to the title */
};

typedef struct {
    u8 mode;                    /* MATCH_SINGLES or MATCH_DOUBLES */
    u8 humans;                  /* 1 to 4 (M4-MP-01): who joined */
    u8 current;                 /* the human choosing: 0 is 1P */
    u8 row;                     /* SELECT_ROW_* with the cursor */
    u8 changed;                 /* nonzero after any change; the renderer
                                   clears it */
    u8 reserved;
    u8 controller[MAX_PLAYERS]; /* each human's controller: 0 for 1P .. 3
                                   for 4P, in that order */
    PlayerChoice choice[MAX_PLAYERS];
} SelectState;

extern SelectState g_select;

/* Opens the screen for 1P: blue, male, standard paddle. joined is a mask
 * of controllers (bit n: controller n; 1P always): each joined human
 * chooses in turn, 1P, 2P, 3P, 4P; singles takes 1P and 2P only (M4-MP-01).
 * Each later human starts on the first colour nobody before has taken.
 * With a career player active, 1P is that player and chooses only the
 * paddle (ADR-065). */
void select_begin(u8 mode, u8 joined);
/* One frame of the current human's newly pressed buttons (PAD_* bits).
 * Up/Down rows, Left/Right values. B steps to the next row and confirms
 * on the paddle row; Start confirms at once; Y steps back. */
u8 select_input(u16 pressed);
/* Nonzero when a human before the current one has the colour. */
u8 select_colour_taken(u8 palette_id);
/* The current human's choice. */
PlayerChoice *select_current(void);
/* Writes the confirmed choices to g_match_config (match_config_select). */
void select_apply(void);

#endif
