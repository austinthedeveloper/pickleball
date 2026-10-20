#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "career.h"
#include "game.h"
#include "input.h"
#include "match.h"
#include "opponents.h"
#include "rules.h"
#include "save.h"
#include "story.h"
#include "story_data.h"
#include "ai.h"

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
    assert(story_update(PAD_B, 0));
    /* ...but B ends it after the minimum. */
    for (f = 1; f < STORY_INTRO_MIN_FRAMES; ++f)
        assert(story_update(0, 0));
    assert(!story_update(PAD_B, 0));
    assert(g_story.card == STORY_CARD_NONE);

    /* Untouched, it goes by itself. */
    story_begin_intro();
    for (f = 1; f < STORY_INTRO_FRAMES; ++f)
        assert(story_update(0, 0));
    assert(!story_update(0, 0));

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
    save_advance_ladder(MATCH_SINGLES);
    assert(g_save.records.ladder_clears == 0);
    save_advance_ladder(MATCH_SINGLES);                      /* beat the last one */
    assert(g_save.settings.ladder == 0);
    assert(g_save.records.ladder_clears == 1);
    /* A doubles clear counts too (ADR-055). */
    g_save.ladder_doubles = OPPONENT_COUNT - 1;
    save_advance_ladder(MATCH_DOUBLES);
    assert(g_save.ladder_doubles == 0 && g_save.records.ladder_clears == 2);
    save_write();
    save_initialize();
    assert(g_save.records.ladder_clears == 2);
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

/* ADR-040: the initials card opens only after a beaten record, outside
 * the attract demo; letters wrap and repeat; it saves on the third B. */
static void test_initials_card(void)
{
    u16 f;

    memset(g_host_sram, 0, sizeof(g_host_sram));
    save_initialize();
    story_initialize();
    match_config_defaults();
    match_config_opponent(1);
    g_game_state.match_winner = 1;              /* a loss: game over */
    g_attract = 0;
    story_begin_initials();
    assert(g_story.card == STORY_CARD_NONE);    /* nothing beaten */

    save_record_rally(7);
    g_attract = 1;
    story_begin_initials();
    assert(g_story.card == STORY_CARD_NONE);
    g_attract = 0;
    story_begin_initials();
    assert(g_story.card == STORY_CARD_INITIALS);
    assert(memcmp(g_story.letters, "AAA", 3) == 0 && g_story.cursor == 0);

    /* Down wraps to Z; B is ignored before the minimum. */
    assert(story_update(PAD_DOWN, PAD_DOWN));
    assert(g_story.letters[0] == 'Z');
    assert(story_update(PAD_B, 0));
    assert(g_story.cursor == 0);
    for (f = 2; f < STORY_INITIALS_MIN_FRAMES; ++f)
        assert(story_update(0, 0));
    /* Up held: one step, then repeats after the delay. */
    assert(story_update(PAD_UP, PAD_UP));       /* Z -> A */
    for (f = 1; f < STORY_REPEAT_DELAY; ++f)
        assert(story_update(0, PAD_UP));
    assert(g_story.letters[0] == 'A');
    assert(story_update(0, PAD_UP));            /* the first repeat */
    assert(g_story.letters[0] == 'B');
    for (f = 0; f < STORY_REPEAT_FRAMES; ++f)
        assert(story_update(0, PAD_UP));
    assert(g_story.letters[0] == 'C');

    /* B moves on, Y goes back, the third B saves. */
    assert(story_update(PAD_B, 0) && g_story.cursor == 1);
    assert(story_update(PAD_Y, 0) && g_story.cursor == 0);
    assert(story_update(PAD_B, 0));
    assert(story_update(PAD_UP, PAD_UP));       /* A -> B */
    assert(story_update(PAD_B, 0) && g_story.cursor == 2);
    assert(!story_update(PAD_B, 0));
    assert(g_story.card == STORY_CARD_NONE);
    assert(memcmp(g_save.initials.longest_rally, "CBA", 3) == 0);
    assert(g_save_new_records == 0);
    story_begin_initials();
    assert(g_story.card == STORY_CARD_NONE);

    /* The next card starts from them; untouched, it keeps them. */
    save_record_rally(8);
    story_begin_initials();
    assert(memcmp(g_story.letters, "CBA", 3) == 0);
    for (f = 1; f < STORY_INITIALS_FRAMES; ++f)
        assert(story_update(0, 0));
    assert(!story_update(0, 0));
    assert(memcmp(g_save.initials.longest_rally, "CBA", 3) == 0);

    /* Start saves at once. */
    save_record_rally(9);
    story_begin_initials();
    for (f = 1; f < STORY_INITIALS_MIN_FRAMES; ++f)
        assert(story_update(0, 0));
    assert(!story_update(PAD_START, 0));
}

/* ADR-040: a record beaten in a match left early waits for the next
 * game over, which asks for it. */
static void test_initials_wait_for_game_over(void)
{
    memset(g_host_sram, 0, sizeof(g_host_sram));
    save_initialize();
    story_initialize();
    match_config_defaults();
    match_config_opponent(1);
    g_attract = 0;
    game_new_match();
    save_record_rally(5);
    game_new_match();                           /* RESTART */
    assert(g_save.records.longest_rally == 5);
    assert(g_save.initials.longest_rally[0] == 0);
    assert(g_save_new_records == SAVE_NEW_RALLY);

    /* A ladder win, not the last opponent's: no card. */
    g_game_state.match_winner = 0;
    story_begin_initials();
    assert(g_story.card == STORY_CARD_NONE);
    /* Nor an open play win. */
    g_match_config.open_play = 1;
    story_begin_initials();
    assert(g_story.card == STORY_CARD_NONE);
    g_match_config.open_play = 0;
    /* The ladder cleared asks. */
    match_config_opponent(OPPONENT_COUNT - 1);
    story_begin_initials();
    assert(g_story.card == STORY_CARD_INITIALS);
    g_story.card = STORY_CARD_NONE;
    /* So does a loss, open play's too. */
    match_config_opponent(1);
    g_game_state.match_winner = 1;
    g_match_config.open_play = 1;
    story_begin_initials();
    assert(g_story.card == STORY_CARD_INITIALS);
    g_story.card = STORY_CARD_NONE;
    g_match_config.open_play = 0;
    /* And a 2P match, won or lost. */
    match_config_defaults();
    g_match_config.human[2] = 1;
    g_game_state.match_winner = 0;
    story_begin_initials();
    assert(g_story.card == STORY_CARD_INITIALS);
    g_story.card = STORY_CARD_NONE;
    g_save_new_records = 0;

    /* ADR-065: a career player is named without a card, game over or not. */
    career_create(0, "ACE");
    g_match_config.human[2] = 0;
    match_config_opponent(1);
    g_game_state.match_winner = 0;
    save_record_rally(6);
    story_begin_initials();
    assert(g_story.card == STORY_CARD_NONE && g_save_new_records == 0);
    assert(memcmp(g_save.initials.longest_rally, "ACE", 3) == 0);
    career_select(CAREER_NONE);
}

/* ADR-047: the unlock card shows what a career player just earned, never
 * in the attract demo, and clears it when it closes. */
static void test_unlock_card(void)
{
    u16 f;

    story_initialize();
    g_attract = 1;
    g_career_new_unlocks = 1;
    story_begin_unlock();
    assert(g_story.card == STORY_CARD_NONE);
    g_attract = 0;
    g_career_new_unlocks = 0;
    story_begin_unlock();
    assert(g_story.card == STORY_CARD_NONE);
    g_career_new_unlocks = 3;
    story_begin_unlock();
    assert(g_story.card == STORY_CARD_UNLOCK);
    /* B can't skip it at once; after the minimum it closes. */
    assert(story_update(PAD_B, PAD_B));
    for (f = 1; f < STORY_UNLOCK_MIN_FRAMES; ++f)
        story_update(0, 0);
    assert(!story_update(PAD_B, PAD_B));
    assert(g_story.card == STORY_CARD_NONE && g_career_new_unlocks == 0);
    /* Untouched, it goes by itself. */
    g_career_new_unlocks = 1;
    story_begin_unlock();
    for (f = 0; f < STORY_UNLOCK_FRAMES; ++f)
        story_update(0, 0);
    assert(g_story.card == STORY_CARD_NONE && g_career_new_unlocks == 0);
}

/* Quips: about one point in three, from the side that lost it, never the
 * same line twice in a row; a side says all sixteen of a pass before any
 * again (ADR-063), in a different order on the next pass. */
static void test_quips(void)
{
    const char *last = 0;
    const char *q;
    const char *first_pass[OPPONENT_QUIPS];
    u16 roll;
    u16 said = 0;
    u16 seen;
    u8 i;
    u8 found;
    u8 reordered = 0;

    story_initialize();
    match_config_defaults();
    seen = 0;
    for (roll = 0; roll < 300; ++roll) {
        q = story_quip(1, roll * 7);
        if ((roll * 7) % 3 != 0) {
            assert(q == 0);
            continue;
        }
        assert(q != last);
        found = 0;
        for (i = 0; i < OPPONENT_QUIPS; ++i)
            if (q == g_side_quips[QUIPS_CPU][i]) {
                found = 1;
                /* Each line once in a pass of sixteen. */
                assert((seen & (1u << i)) == 0);
                seen |= (u16)(1u << i);
            }
        assert(found);                          /* the default CPU's */
        if (said < OPPONENT_QUIPS)
            first_pass[said] = q;
        else if (said < 2 * OPPONENT_QUIPS)
            reordered |= q != first_pass[said - OPPONENT_QUIPS];
        ++said;
        if ((said & (OPPONENT_QUIPS - 1)) == 0) {
            assert(seen == 0xffff);
            seen = 0;
        }
        last = q;
    }
    assert(said == 100);
    assert(reordered);
    /* A ladder team speaks its own; the human side the player's. */
    match_config_opponent(3);
    q = story_quip(1, 0);
    found = 0;
    for (i = 0; i < OPPONENT_QUIPS; ++i)
        found |= q == g_opponent_quips[3][i];
    assert(found);
    q = story_quip(0, 3);
    found = 0;
    for (i = 0; i < OPPONENT_QUIPS; ++i)
        found |= q == g_side_quips[QUIPS_PLAYER][i];
    assert(found);
}

/* A match won 5-0 from 4-0: the point, then the frames to the result. */
static void win_match(void)
{
    static PlayerIntent none[MAX_PLAYERS];
    u16 n;

    g_game_state.team_score[0] = 4;
    g_game_state.phase = GAME_PHASE_LIVE;
    g_ball.rally_state = BALL_RALLY_LIVE;
    rules_resolve_point(1, GAME_FAULT_OUT);
    for (n = 0; n < GAME_POINT_RESULT_FRAMES + 2 &&
            g_game_state.phase != GAME_PHASE_MATCH_RESULT; ++n)
        game_update(none);
    assert(g_game_state.phase == GAME_PHASE_MATCH_RESULT);
    assert(g_game_state.match_winner == 0);
}

/* Turns every page of the scene up with B; returns how many there were. */
static u8 read_scene(void)
{
    u8 pages = 0;
    u16 f;

    while (g_story.card == STORY_CARD_SCENE) {
        /* B is ignored for the page's first frames. */
        assert(story_update(PAD_B, 0));
        for (f = 1; f < STORY_PAGE_MIN_FRAMES; ++f)
            story_update(0, 0);
        story_update(PAD_B, 0);
        ++pages;
        assert(pages < 40);
    }
    return pages;
}

/* ADR-077: story mode. Doubles with the story partner against the
 * chapter's team; each chapter opens on its scene, then the intro card; a
 * win moves the story on (not the ladders), quitting keeps it, and beating
 * the Champs saves the park: the ending, then the title. */
static void test_story_mode(void)
{
    u16 palette[16];
    u8 i;
    u16 f;

    memset(g_host_sram, 0, sizeof(g_host_sram));
    save_initialize();
    g_attract = 0;
    assert(save_story_chapter() == 0);
    match_config_doubles();
    match_config_story(save_story_chapter());
    assert(g_match_config.story && g_match_config.opponent == 0);
    assert(g_match_config.partner == MATCH_PARTNER_STORY);
    assert(match_slot_hat(1) == g_story_partner_hat);
    assert(match_slot_style(1) == &g_ai_partner_style);
    match_compose_palette(1, palette);
    for (i = 0; i < 16; ++i)
        assert(palette[i] == g_story_partner_palette[i]);
    assert(g_match_config.paddle_id[1] == g_story_partner_paddle);

    /* Every chapter opens on CHAPTER n, the ending closes on THE END, and
     * every page but the narrator's has a speaker. */
    for (i = 0; i <= STORY_CHAPTERS; ++i)
        assert(g_story_first[i] < g_story_first[i + 1]);
    for (i = 0; i < STORY_CHAPTERS; ++i)
        assert(g_story_who[g_story_first[i]] == STORY_WHO_CHAPTER);
    assert(g_story_who[STORY_PAGES - 1] == STORY_WHO_END);
    for (i = 0; i < STORY_PAGES; ++i)
        if (g_story_who[i] >= STORY_WHO_PARTNER &&
                g_story_who[i] <= STORY_WHO_VILLAIN)
            assert(story_speaker(i) && story_speaker(i)[0]);
    assert(strcmp(story_speaker(g_story_first[1] - 1), "DAD") == 0);

    game_start_match();
    story_begin_intro();
    assert(g_story.card == STORY_CARD_SCENE && g_story.scene == 0);
    assert(read_scene() == g_story_first[1] - g_story_first[0]);
    assert(g_story.card == STORY_CARD_INTRO);
    /* Start skips the rest of a scene. */
    story_begin_intro();
    for (f = 0; f < STORY_PAGE_MIN_FRAMES; ++f)
        story_update(0, 0);
    story_update(PAD_START, 0);
    assert(g_story.card == STORY_CARD_INTRO);
    g_story.card = STORY_CARD_NONE;

    win_match();
    assert(save_story_chapter() == 1 && !g_story.cleared);
    assert(g_save.settings.ladder == 0 && g_save.ladder_doubles == 0);
    /* Quitting a chapter keeps the story where it is. */
    game_quit_match();
    assert(save_story_chapter() == 1);

    /* The last chapter: the park is saved, the story starts over, and no
     * ladder clear is recorded. */
    g_save.story_chapter = STORY_CHAPTERS - 1;
    match_config_doubles();
    match_config_story(save_story_chapter());
    game_start_match();
    win_match();
    assert(g_story.cleared && save_story_chapter() == 0);
    assert(g_save.records.ladder_clears == 0);
    story_begin_ending();
    assert(g_story.card == STORY_CARD_SCENE && g_story.scene == STORY_ENDING);
    assert(read_scene() == STORY_PAGES - g_story_first[STORY_ENDING]);
    assert(g_story.card == STORY_CARD_NONE && g_attract);
    g_attract = 0;
    match_config_defaults();
}

int main(void)
{
    test_intro_card();
    test_ladder_clear_record();
    test_lines();
    test_initials_card();
    test_initials_wait_for_game_over();
    test_unlock_card();
    test_quips();
    test_story_mode();
    printf("story host checks passed\n");
    return 0;
}
