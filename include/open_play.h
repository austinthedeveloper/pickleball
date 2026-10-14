#ifndef OPEN_PLAY_H
#define OPEN_PLAY_H

#include "game_types.h"
#include "player.h"

/*
 * Open play (docs/decisions/ADR-034-open-play.md): a park with three
 * courts. Each court has a game going and a paddle stack: paddles in line
 * for the next game. Entries are ladder teams (ADR-023, a pair each) or
 * you. When a game ends all four come off and the top two entries go on;
 * the players coming off join the back of a stack. You walk the park, put
 * your paddle in a court's stack, and play when your turn comes. Pure:
 * render_park.c draws it and main.c runs the match.
 */

enum {
    OPEN_COURTS = 3,
    OPEN_STACK_MAX = 4,         /* entries waiting at one court */
    OPEN_YOU = 0x80,            /* an entry: you (and a regular partner) */
    OPEN_NONE = 0xff,
    OPEN_GAME_POINTS = 7,       /* the park's own games: first to 7 */
    /* A point every 1 to 3 s, so a game lasts about half a minute. */
    OPEN_POINT_MIN_FRAMES = 60,
    OPEN_POINT_SPAN_FRAMES = 128,  /* a power of two (a mask, no divide) */
    OPEN_CHANGEOVER_FRAMES = 120,   /* players walk off and on */
    OPEN_CALL_FRAMES = 150,         /* "YOUR GAME!" before the match */

    /* Park layout in screen pixels (256x224). Courts run top to bottom;
     * the path along the bottom has a paddle rack at each court's gate. */
    OPEN_COURT_W = 56,
    OPEN_COURT_H = 112,
    OPEN_COURT_TOP = 48,
    OPEN_COURT_LEFT0 = 20,
    OPEN_COURT_PITCH = 80,          /* court n's left edge: 20 + 80 n */
    OPEN_RACK_Y = OPEN_COURT_TOP + OPEN_COURT_H + 10,
    OPEN_RACK_DX = OPEN_COURT_W / 2,    /* the rack at the court's gate */
    OPEN_RACK_REACH = 14,           /* B works within this of a rack */
    OPEN_PEEK_REACH = 28,           /* the HUD names a court this close */

    /* You: a ground point (feet), in 12.4. */
    OPEN_START_X = 128,
    OPEN_START_Y = 196,
    OPEN_WALK_FP = 20,              /* 1.25 px per frame */
    OPEN_WALK_TOP = 30,
    OPEN_WALK_BOTTOM = 210,
    OPEN_WALK_LEFT = 8,
    OPEN_WALK_RIGHT = 248,
    OPEN_FENCE_MARGIN = 4           /* feet stay this far off a fence */
};

typedef enum {
    OPEN_COURT_PLAYING = 0,
    OPEN_COURT_CHANGEOVER = 1,      /* between games */
    OPEN_COURT_YOURS = 2            /* your game: the match is on */
} OpenCourtState;

typedef struct {
    u8 state;
    u8 playing[2];              /* the two entries on court (top, bottom) */
    u8 score[2];
    u8 stack[OPEN_STACK_MAX];   /* next first */
    u8 stack_count;
    u16 timer;                  /* frames to the next point or game */
    u8 rally;                   /* animation: frames into the rally */
} OpenCourt;

/* open_play_update results. */
enum {
    OPEN_EVENT_NONE = 0,
    OPEN_EVENT_STACKED = 1,     /* your paddle went into a stack */
    OPEN_EVENT_UNSTACKED = 2,   /* you took it back */
    OPEN_EVENT_FULL = 3,        /* that stack has no room */
    OPEN_EVENT_CALLED = 4,      /* your game is up: the call banner */
    OPEN_EVENT_PLAY = 5         /* the banner is done: start the match */
};

typedef struct {
    OpenCourt court[OPEN_COURTS];
    fx x_fp;                    /* you */
    fx y_fp;
    u8 facing_left;
    u8 walking;
    u8 step;                    /* walk animation frame counter */
    u8 your_court;              /* the court your paddle waits at, or none */
    u8 called;                  /* the court calling you, or OPEN_NONE */
    u8 call_timer;
    u8 opponent;                /* your opponents' team, when called */
    u16 seed;
    u8 lineup;                  /* changes when anyone comes on or off */
    u8 stacks;                  /* changes when any stack changes */
    u8 last_won;                /* your last game: 1 won, 0 lost, none */
    u8 last_score[2];           /* yours, theirs */
} OpenPlay;

extern OpenPlay g_open_play;

/* A fresh park: six teams playing, two waiting, you at the gate. */
void open_play_begin(u16 seed);
/* One park frame: walking (the D-pad), B at a rack, the courts' games.
 * Returns OPEN_EVENT_*. While called, input is ignored. */
u8 open_play_update(s16 move_x, s16 move_y, u8 b_pressed);
/* Back from your match: you won or lost by this score; your court's game
 * ends and the next one starts. */
void open_play_return(u8 won, u8 points_for, u8 points_against);

/* Court geometry and lookups (render_park.c and the HUD). */
s16 open_court_left(u8 court);
s16 open_rack_x(u8 court);
/* The rack within reach of you, or OPEN_NONE. */
u8 open_rack_near(void);
/* The court you are next to (for the HUD's peek), or OPEN_NONE. */
u8 open_court_near(void);
/* Your place in your stack: 0 = next game, 1 = the one after; OPEN_NONE
 * when not stacked. Two entries per game. */
u8 open_your_wait(void);
/* Nonzero when a ground point is walkable (not inside a fenced court). */
u8 open_walkable(fx x_fp, fx y_fp);

#endif
