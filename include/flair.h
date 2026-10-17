#ifndef FLAIR_H
#define FLAIR_H

#include "game_types.h"
#include "input.h"

/*
 * ADR-064: the sprite wish list in play. Presentation only, like fx (R9):
 * driven by events and the game state, never read by the simulation, so the
 * golden trace is unchanged. src/render.c draws what this decides.
 */
enum {
    FLAIR_NONE = 0xff,          /* no reaction frame: the usual one */
    /* A dive (a loser running when the ball bounced twice past): the dive,
     * then up on a knee, then the slump. */
    FLAIR_DIVE_FRAMES = 12,
    FLAIR_SCRAMBLE_END = 24,
    FLAIR_DUST_FRAMES = 12,     /* the puff, then its fade at half */
    FLAIR_CORD_FRAMES = 16,     /* the net shivers, alternating every 4 */
    FLAIR_SQUASH_FRAMES = 3,    /* a drive's contact */
    FLAIR_STAR_RALLY = 8,       /* shots in a rally its winners star for */
    FLAIR_NOTE_RUN = 3,         /* soft shots in a row: a dink rally */
    FLAIR_NOTE_FRAMES = 40,
    FLAIR_ZZZ_WAIT = 300,       /* a human server this slow: the Z's */
    FLAIR_RESULT_EMOTE_FRAMES = 255,    /* until the next serve */
    /* Celebrations are looked up from this far into the first phase on:
     * a match's first frames are its heaviest. No point ends this soon. */
    FLAIR_LOOKUP_DELAY = 8
};

/* Reactions at a point's end, by slot. */
enum {
    FLAIR_REACT_NONE = 0,
    FLAIR_REACT_CELEBRATE = 1,  /* the slot's celebration frame */
    FLAIR_REACT_SLUMP = 2,
    FLAIR_REACT_DIVE = 3        /* dive, scramble, then slump */
};

/* Emotes over a player's head (0: none). */
enum {
    FLAIR_EMOTE_NONE = 0,
    FLAIR_EMOTE_EXCLAIM = 1,    /* into the net */
    FLAIR_EMOTE_QUESTION = 2,   /* out */
    FLAIR_EMOTE_SWEAT = 3,      /* couldn't reach it */
    FLAIR_EMOTE_NOTE = 4,       /* a dink rally */
    FLAIR_EMOTE_STAR = 5,       /* won a long rally */
    FLAIR_EMOTE_ZZZ = 6,        /* waiting on a slow server */
    FLAIR_EMOTE_ANGER = 7,      /* a rules fault: kitchen, box, volley */
    FLAIR_EMOTES = 8
};

typedef struct {
    u8 active;                  /* anything up: timers, emotes, reactions,
                                   a streak; 0 lets ticks and draws skip */
    u8 react_serial;            /* moves when a body frame may change */
    u8 reacting;                /* react[] holds a point's reactions */
    u8 wake;                    /* flair_tick wanted though not active: a
                                   phase change, serve setup, lookups */
    u8 fresh;                   /* an event this frame: render waits a
                                   frame, off the contact's or bounce's
                                   frame, the rally's heaviest */
    u8 react[MAX_PLAYERS];
    u8 celebrate[MAX_PLAYERS];  /* frame index, from the match: a slot a
                                   tick from the match's start */
    u8 emote[MAX_PLAYERS];
    u8 emote_timer[MAX_PLAYERS];
    u8 result_frames;           /* frames into a point or match result */
    u8 rally_shots;             /* contacts since the serve, serve included */
    u8 soft_run;                /* soft shots in a row */
    u8 shot;                    /* the ball's shot type; 0xff after a bounce */
    u8 squash_timer;
    u8 dust_timer;
    u8 cord_timer;
    u8 reserved;                /* keeps the s16s even on host and SNES */
    s16 dust_x;
    s16 dust_y;
    s16 cord_y;
} Flair;

extern Flair g_flair;

/* At match start; the slots' celebrations (from the match config) follow
 * on the first frames' ticks, a slot a frame. */
void flair_initialize(void);
void flair_on_event(u8 kind, s16 x, s16 y, u8 detail);
/* Once per unpaused frame: FLAIR_TICK() calls it only when there is work,
 * most frames none (the new match's frame has no time to spare). */
void flair_tick(void);
#define FLAIR_TICK() \
    do { \
        if (g_flair.active | g_flair.wake) \
            flair_tick(); \
    } while (0)
/* The side that lost the point says a quip (story_quip): its players emote
 * how they lost it. */
void flair_quip(u8 losing_team);
/* A slot's body frame index for its point reaction, or FLAIR_NONE. */
u8 flair_body_frame(u8 slot);

#endif
