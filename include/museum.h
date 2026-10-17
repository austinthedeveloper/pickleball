#ifndef MUSEUM_H
#define MUSEUM_H

#include "game_types.h"
#include "career.h"

/*
 * The HAT MUSEUM (ADR-061): a career player's hats in display cases, the
 * player wearing each one. Locked hats show as a dark silhouette named
 * "???", with how to earn them under the cursor. Opened with X from the
 * player's CARD on the CAREER page. Pure input logic; render_museum.c
 * draws it and main.c switches the scene and writes the save.
 *
 *   D-pad  moves through the cases (rows of MUSEUM_COLS, wrapping)
 *   B      wears an unlocked hat
 *   Y      back to the CARD (Start too); the save is written then if a
 *          hat was put on (a safe point: leaving a menu)
 *
 * The cases hold every hat but the bare head (CAREER_HAT_NONE), in hat
 * id order: the cap first.
 */
enum {
    MUSEUM_HATS = CAREER_HATS - 1,
    MUSEUM_COLS = 4,
    MUSEUM_ROWS = (MUSEUM_HATS + MUSEUM_COLS - 1) / MUSEUM_COLS
};

/* museum_input results. */
enum {
    MUSEUM_STAY = 0,
    MUSEUM_CLOSE = 1,           /* back to the CAREER page's CARD */
    MUSEUM_CLOSE_SAVE = 2       /* the same; a hat was put on: write */
};

typedef struct {
    u8 slot;                    /* the career player shown */
    u8 cursor;                  /* the case picked, 0..MUSEUM_HATS-1 */
    u8 changed;                 /* the cursor or the worn hat moved: the
                                   renderer redraws and clears this */
    u8 worn;                    /* a hat was put on since opening */
} Museum;

extern Museum g_museum;

/* Opens on a slot's player, the cursor on the hat they wear (the cap when
 * bareheaded). */
void museum_begin(u8 slot);
/* One frame of input: newly pressed buttons. */
u8 museum_input(u16 pressed);

/* The player shown. */
CareerPlayer *museum_player(void);
/* The hat id in a case. */
u8 museum_hat(u8 index);
/* Nonzero when the player holds the case's hat. */
u8 museum_unlocked(u8 index);
/* How many cases are unlocked. */
u8 museum_unlocked_count(void);

#endif
