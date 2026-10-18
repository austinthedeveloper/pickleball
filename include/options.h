#ifndef OPTIONS_H
#define OPTIONS_H

#include "game_types.h"
#include "menu.h"
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
    /* The ball's step pattern repeats every 60 frames: KID's 2 in 3 and
     * EASY's 3 in 4 divide it, and 5% ball speed steps (ADR-049). */
    OPTIONS_BALL_CYCLE = 60,
    /* The ball's speed, percent (ADR-049). The BALL SPEED row is gone
     * (ADR-065: FAST BALL is the one speed setting); `make stats SPEED=`
     * still tries others. */
    OPTIONS_BALL_SPEED = 100,
    /* CAMERA (ADR-050, ADR-065): OFF or ON, and its AMOUNT, 50% to 100% in
     * 5% steps, in g_save.camera. 2-12 is ON at AMOUNT 0-10, as every save
     * since ADR-050 has it; 0 (older saves) and 1 are OFF at the default
     * amount; OPTIONS_CAMERA_OFF | amount is OFF keeping an amount. Bit 7
     * stays clear: the options page marks its cursor with it. */
    OPTIONS_CAMERA_AMOUNTS = 11,
    OPTIONS_CAMERA_AMOUNT_DEFAULT = 7,      /* 85% */
    OPTIONS_CAMERA_OFF = 0x40
};

/* 50, 55, ... 100 (percent of the camera's full lean). */
extern const u8 g_options_cameras[OPTIONS_CAMERA_AMOUNTS];
/* The same as a multiplier in 1/128ths (percent * 128 / 100, rounded), for
 * the hardware multiplier in render.c. */
extern const u8 g_options_camera_k[OPTIONS_CAMERA_AMOUNTS];

/* 3, 5, 7, 9, 11, 15, 21. */
extern const u8 g_options_points[OPTIONS_POINT_CHOICES];

/* A saved value, or the default when it is not a valid choice. */
u8 options_points(u8 saved);
u8 options_difficulty(u8 saved);
const char *options_difficulty_name(u8 difficulty);

/* A CPU opposing the humans plays this much better or worse: the late
 * period (mistakes), reaction, and speed of its style change. */
void options_apply_difficulty(AiStyle *style, u8 difficulty);
/* Ball steps in one OPTIONS_BALL_CYCLE: the difficulty's share (KID 2 in
 * 3, EASY 3 in 4, else all) times the ball speed. Over 60 the ball takes
 * two steps on some frames. Arcs and bounces are unchanged. */
u8 options_ball_per_cycle(u8 difficulty, u8 percent);
/* CAMERA (ADR-050, ADR-065): a saved value's ON (nonzero) and AMOUNT
 * (0..OPTIONS_CAMERA_AMOUNTS-1), the value saving them, and its multiplier
 * (0 when OFF). Junk is OFF at the default amount. */
u8 options_camera_on(u8 saved);
u8 options_camera_amount(u8 saved);
u8 options_camera_saved(u8 on, u8 amount);
u8 options_camera_k(u8 saved);

/* SCORING (ADR-065, the WIN BY 2 modifier's bit): its name. */
const char *options_scoring_name(u8 modifiers);

/* Options page editor (like the court page, ADR-022). */
enum { OPTIONS_ROW_POINTS = 0, OPTIONS_ROW_SCORING = 1,
       OPTIONS_ROW_DIFFICULTY = 2, OPTIONS_ROW_SOUND = 3,
       OPTIONS_ROW_CAMERA = 4, OPTIONS_ROW_AMOUNT = 5, OPTIONS_ROWS = 6 };
enum { OPTIONS_EDIT_STAY = MENU_EDIT_STAY, OPTIONS_EDIT_DONE = MENU_EDIT_DONE,
       OPTIONS_EDIT_CANCEL = MENU_EDIT_CANCEL };

typedef struct {
    u8 row;
    u8 original_points;
    u8 original_difficulty;
    u8 original_sound;
    u8 original_modifiers;
    u8 original_camera;
} OptionsEdit;

extern OptionsEdit g_options_edit;

/* Opens the editor on the saved values (made valid in place); sound is an
 * AUDIO_* setting (ADR-029), modifiers the MODIFIER_* bits, of which the
 * SCORING row edits MODIFIER_WIN_BY_2. */
void options_edit_begin(u8 *points, u8 *difficulty, u8 *sound,
    u8 *modifiers, u8 *camera);
/* Up/Down rows, Left/Right values (wrapping), B or Start keeps, Y cancels. */
u8 options_edit_input(u8 *points, u8 *difficulty, u8 *sound, u8 *modifiers,
    u8 *camera, u16 pressed);
u8 options_edit_changed(u8 points, u8 difficulty, u8 sound, u8 modifiers,
    u8 camera);

#endif
