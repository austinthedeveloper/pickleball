#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "game.h"
#include "input.h"
#include "match.h"
#include "rules.h"
#include "save.h"
#include "story.h"

/* ADR-026 story cards: the intro card and the ladder clear record. */

static void test_intro_card(void)
{
    u16 f;

    g_attract = 0;
    match_config_defaults();
    story_begin_intro();
    assert(g_story.card == STORY_CARD_NONE);    /* the default CPU: none */

    match_config_opponent(2);
    story_begin_intro();
    assert(g_story.card == STORY_CARD_INTRO);
    /* A held button cannot skip it at once... */
    assert(story_update(PAD_B));
    /* ...but B ends it after the minimum. */
    for (f = 1; f < STORY_INTRO_MIN_FRAMES; ++f)
        assert(story_update(0));
    assert(!story_update(PAD_B));
    assert(g_story.card == STORY_CARD_NONE);

    /* Untouched, it goes by itself. */
    story_begin_intro();
    for (f = 1; f < STORY_INTRO_FRAMES; ++f)
        assert(story_update(0));
    assert(!story_update(0));

    /* Never in the attract demo. */
    g_attract = 1;
    story_begin_intro();
    assert(g_story.card == STORY_CARD_NONE);
    g_attract = 0;
    match_config_defaults();
}

static void test_ladder_clear_record(void)
{
    memset(g_host_sram, 0, sizeof(g_host_sram));
    save_initialize();
    g_save.settings.ladder = OPPONENT_COUNT - 2;
    save_advance_ladder();
    assert(g_save.records.ladder_clears == 0);
    save_advance_ladder();                      /* beat the last one */
    assert(g_save.settings.ladder == 0);
    assert(g_save.records.ladder_clears == 1);
    save_write();
    save_initialize();
    assert(g_save.records.ladder_clears == 1);
}

/* Every opponent has all three lines, in the HUD's limits. */
static void test_lines(void)
{
    u8 o;
    u8 k;

    for (o = 0; o < OPPONENT_COUNT; ++o)
        for (k = 0; k < 3; ++k) {
            assert(strlen(g_opponent_lines[o][k]) >= 1);
            assert(strlen(g_opponent_lines[o][k]) <= 28);
            assert(!strchr(g_opponent_lines[o][k], '@'));
        }
}

int main(void)
{
    test_intro_card();
    test_ladder_clear_record();
    test_lines();
    printf("story host checks passed\n");
    return 0;
}
