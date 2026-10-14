#ifndef STORY_H
#define STORY_H

#include "game_types.h"

/*
 * Story cards (docs/decisions/ADR-026-story-cards.md). The intro card
 * shows before a ladder match from Player Select; the match waits under it.
 * The result lines are drawn from the match state (hud_scene.c). Pure.
 */

enum { STORY_CARD_NONE = 0, STORY_CARD_INTRO = 1 };
enum {
    STORY_INTRO_MIN_FRAMES = 30,    /* a held button cannot skip it */
    STORY_INTRO_FRAMES = 300        /* then it goes by itself (5 s) */
};

/* The captain's bust on the intro card (M4-STORY-03): a 6x6-tile BG2
 * frame, centred between the rung (row 14) and the prompt (row 24).
 * tools/assetgen/select.py reads these. */
#define STORY_BUST_COL 13
#define STORY_BUST_ROW 16
#define STORY_BUST_TILES 6

typedef struct {
    u8 card;
    u16 timer;
} StoryState;

extern StoryState g_story;

/* No card. Call at boot: RAM is not cleared at power-on, and a stray card
 * value would freeze the attract demo under an invisible card. */
void story_initialize(void);
/* Shows the intro card when the match has a ladder opponent. */
void story_begin_intro(void);
/* One frame of 1P's newly pressed buttons; B, A, or Start ends the card
 * after STORY_INTRO_MIN_FRAMES. Nonzero while a card stays up. The caller
 * keeps that frame's input from the match even when the card closes. */
u8 story_update(u16 pressed);

#endif
