#include "game.h"
#include "input.h"
#include "match.h"
#include "story.h"

StoryState g_story;

void story_begin_intro(void)
{
    g_story.timer = 0;
    g_story.card = (!g_attract && g_match_config.opponent != OPPONENT_NONE) ?
        STORY_CARD_INTRO : STORY_CARD_NONE;
}

u8 story_update(u16 pressed)
{
    if (g_story.card == STORY_CARD_NONE)
        return 0;
    ++g_story.timer;
    if (g_story.timer >= STORY_INTRO_FRAMES ||
            (g_story.timer >= STORY_INTRO_MIN_FRAMES &&
             (pressed & (PAD_B | PAD_A | PAD_START))))
        g_story.card = STORY_CARD_NONE;
    return g_story.card != STORY_CARD_NONE;
}
