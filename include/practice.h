#ifndef PRACTICE_H
#define PRACTICE_H

#include "game_types.h"
#include "input.h"

/*
 * Practice mode (docs/decisions/ADR-028-practice.md): a ball machine on the
 * far baseline feeds the chosen shot to the chosen target; returns that
 * land in count toward a streak. No points, no serves, no rules faults.
 */

typedef enum {
    PRACTICE_SHOT_DRIVE = 0, PRACTICE_SHOT_LOB = 1, PRACTICE_SHOT_DINK = 2,
    PRACTICE_SHOT_MIX = 3, PRACTICE_SHOTS = 4
} PracticeShot;

typedef enum {
    PRACTICE_TARGET_KITCHEN = 0, PRACTICE_TARGET_DEEP = 1,
    PRACTICE_TARGET_FOREHAND = 2, PRACTICE_TARGET_BACKHAND = 3,
    PRACTICE_TARGET_AT_YOU = 4, PRACTICE_TARGET_RANDOM = 5,
    PRACTICE_TARGETS = 6
} PracticeTarget;

typedef enum {
    PRACTICE_RATE_SLOW = 0, PRACTICE_RATE_NORMAL = 1, PRACTICE_RATE_FAST = 2,
    PRACTICE_RATES = 3
} PracticeRate;

enum {
    /* The machine: on the far baseline, centred; feeds leave its nozzle
     * (the sprite's left edge, 8 px up; render.c draws it there). */
    PRACTICE_MACHINE_X = 232,
    PRACTICE_MACHINE_Y = 130,
    PRACTICE_MACHINE_Z = 8,
    PRACTICE_FIRST_FEED_FRAMES = 60,
    PRACTICE_FEED_TIMEOUT_FRAMES = 300, /* a stuck feed ends anyway */
    PRACTICE_STREAK_MAX = 99            /* two digits on the score panel */
};

/* Feed state. */
enum { PRACTICE_WAIT = 0, PRACTICE_FEED = 1, PRACTICE_RETURN = 2 };

typedef struct {
    u8 shot;            /* settings for this session */
    u8 target;
    u8 rate;
    u8 state;
    u16 timer;          /* frames until the next feed, or in this feed */
    u8 streak;
    u8 best;            /* this session, with the saved best folded in */
    u8 feeds;           /* drives the lanes and the MIX/RANDOM pattern */
    u8 rng;
    u8 last_shot;       /* ShotType of the last feed */
} PracticeState;

extern PracticeState g_practice;

/* Settings (saved): a valid value, or the default. */
u8 practice_shot(u8 saved);
u8 practice_target(u8 saved);
u8 practice_rate(u8 saved);
const char *practice_shot_name(u8 shot);
const char *practice_target_name(u8 target);
const char *practice_rate_name(u8 rate);
/* Frames between one feed ending and the next. */
u16 practice_rate_frames(u8 rate);

/* Starts a session (called by game_new_match in practice): no serve; the
 * first feed comes after PRACTICE_FIRST_FEED_FRAMES. */
void practice_begin(void);
/* One frame of the session, after the players moved (game_update). */
void practice_update(const PlayerIntent intents[MAX_PLAYERS]);
/* Leaving practice: a new best streak is saved. */
void practice_finish(void);

/* Practice page editor (like OPTIONS, ADR-025). */
enum { PRACTICE_ROW_SHOT = 0, PRACTICE_ROW_TARGET = 1, PRACTICE_ROW_RATE = 2,
       PRACTICE_ROWS = 3 };
enum { PRACTICE_EDIT_STAY = 0, PRACTICE_EDIT_DONE = 1,
       PRACTICE_EDIT_CANCEL = 2 };

typedef struct {
    u8 row;
    u8 original[PRACTICE_ROWS];
} PracticeEdit;

extern PracticeEdit g_practice_edit;

/* values: shot, target, rate (the saved settings, made valid in place). */
void practice_edit_begin(u8 values[PRACTICE_ROWS]);
/* Up/Down rows, Left/Right values (wrapping), B or Start plays, Y cancels. */
u8 practice_edit_input(u8 values[PRACTICE_ROWS], u16 pressed);
u8 practice_edit_changed(const u8 values[PRACTICE_ROWS]);

#endif
