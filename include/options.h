#ifndef OPTIONS_H
#define OPTIONS_H

#include "game_types.h"
#include "opponents.h"

/*
 * Options (docs/decisions/ADR-025-options.md): points to win and
 * difficulty, kept in the save. Pure: game.c, ai.c, and main.c apply them.
 */

typedef enum {
    DIFFICULTY_KID = 0,         /* slowest ball, swings for you, easiest CPU */
    DIFFICULTY_EASY = 1,        /* slow ball, easier CPU */
    DIFFICULTY_MEDIUM = 2,      /* the shipped game */
    DIFFICULTY_HARD = 3,        /* normal ball, harder CPU */
    DIFFICULTY_COUNT = 4
} Difficulty;

enum {
    OPTIONS_POINTS_DEFAULT = 5,
    OPTIONS_POINT_CHOICES = 7,
    /* The ball's step pattern repeats every 12 frames (3 and 4 divide it). */
    OPTIONS_BALL_CYCLE = 12
};

/* 3, 5, 7, 9, 11, 15, 21. */
extern const u8 g_options_points[OPTIONS_POINT_CHOICES];

/* A saved value, or the default when it is not a valid choice. */
u8 options_points(u8 saved);
u8 options_difficulty(u8 saved);
const char *options_difficulty_name(u8 difficulty);

/* A CPU opposing the humans plays this much better or worse: the late
 * period (mistakes), reaction, and speed of its style change. */
void options_apply_difficulty(AiStyle *style, u8 difficulty);
/* Nonzero when the ball steps on this tick (0..OPTIONS_BALL_CYCLE-1, or a
 * little past it for lookahead): KID 2 of 3, EASY 3 of 4, else every frame.
 * Arcs and bounces are unchanged; the ball is slower. */
u8 options_ball_moves(u8 difficulty, u8 tick);

/* Options page editor (like the court page, ADR-022). */
enum { OPTIONS_ROW_POINTS = 0, OPTIONS_ROW_DIFFICULTY = 1,
       OPTIONS_ROW_SOUND = 2, OPTIONS_ROWS = 3 };
enum { OPTIONS_EDIT_STAY = 0, OPTIONS_EDIT_DONE = 1, OPTIONS_EDIT_CANCEL = 2 };

typedef struct {
    u8 row;
    u8 original_points;
    u8 original_difficulty;
    u8 original_sound;
} OptionsEdit;

extern OptionsEdit g_options_edit;

/* Opens the editor on the saved values (made valid in place); sound is an
 * AUDIO_* setting (ADR-029). */
void options_edit_begin(u8 *points, u8 *difficulty, u8 *sound);
/* Up/Down rows, Left/Right values (wrapping), B or Start keeps, Y cancels. */
u8 options_edit_input(u8 *points, u8 *difficulty, u8 *sound, u16 pressed);
u8 options_edit_changed(u8 points, u8 difficulty, u8 sound);

#endif
