#include "career.h"
#include "game.h"
#include "input.h"
#include "match.h"
#include "opponents.h"
#include "save.h"
#include "story.h"
#define STORY_DATA_DEFINE
#include "story_data.h"

StoryState g_story;
/* The initials entered last this session: the next card starts there. */
static u8 s_last[3];
/* The quip said last (story_quip): not said again next. */
static const char *s_last_quip;
/* Each side's shuffled deck of quips (ADR-063): the next line, the stride
 * (odd, so it visits all sixteen), and how many are left in this pass. */
static u8 s_quip_next[2];
static u8 s_quip_stride[2];
static u8 s_quip_left[2];

void story_initialize(void)
{
    g_story.card = STORY_CARD_NONE;
    g_story.timer = 0;
    g_story.scene = 0;
    g_story.page = 0;
    g_story.end = 0;
    g_story.cleared = 0;
    s_last_quip = 0;
    s_quip_left[0] = 0;
    s_quip_left[1] = 0;
    s_last[0] = 'A';
    s_last[1] = 'A';
    s_last[2] = 'A';
}

void story_begin_initials(void)
{
    const CareerPlayer *player;

    /* Two tests, not `g_attract || !g_save_new_records`: 816-tcc
     * miscompiles `|| !` (CLAUDE.md), and the attract demo's matches
     * opened this card. */
    if (g_attract)
        return;
    if (g_save_new_records == 0)
        return;
    /* A career player's records carry their name (ADR-065): its first
     * three letters, without asking. */
    player = career_active();
    if (player && player->name[0] != ' ') {
        save_set_initials(player->name);
        return;
    }
    /* Only at a game over (amended 2026-10-16): a loss to the CPU ends the
     * run, the last ladder opponent beaten ends the ladder, and a 2P match
     * has no run. A ladder or open play win keeps its records waiting. */
    if (match_vs_cpu() && g_game_state.match_winner == 0) {
        if (g_match_config.open_play)
            return;
        if (g_match_config.opponent != OPPONENT_COUNT - 1)
            return;
    }
    g_story.card = STORY_CARD_INITIALS;
    g_story.timer = 0;
    g_story.cursor = 0;
    g_story.held_frames = 0;
    g_story.letters[0] = s_last[0];
    g_story.letters[1] = s_last[1];
    g_story.letters[2] = s_last[2];
}

/* A letter step, A to Z around. */
static void story_letter(s16 step)
{
    u8 *letter = &g_story.letters[g_story.cursor];

    if (step > 0)
        *letter = *letter == 'Z' ? 'A' : *letter + 1;
    else
        *letter = *letter == 'A' ? 'Z' : *letter - 1;
}

static void story_save_initials(void)
{
    s_last[0] = g_story.letters[0];
    s_last[1] = g_story.letters[1];
    s_last[2] = g_story.letters[2];
    save_set_initials(g_story.letters);
    g_story.card = STORY_CARD_NONE;
}

static u8 story_update_initials(u16 pressed, u16 held)
{
    u16 vertical = held & (PAD_UP | PAD_DOWN);

    ++g_story.timer;
    if (g_story.timer >= STORY_INITIALS_FRAMES) {
        story_save_initials();
        return 0;
    }
    /* Up/Down: a press steps; held, it repeats. */
    if (pressed & (PAD_UP | PAD_DOWN)) {
        story_letter((pressed & PAD_UP) ? 1 : -1);
        g_story.held_frames = 0;
    } else if (vertical) {
        if (++g_story.held_frames >= STORY_REPEAT_DELAY) {
            g_story.held_frames -= STORY_REPEAT_FRAMES;
            story_letter((vertical & PAD_UP) ? 1 : -1);
        }
    } else {
        g_story.held_frames = 0;
    }
    if (g_story.timer < STORY_INITIALS_MIN_FRAMES)
        return 1;
    if (pressed & PAD_START) {
        story_save_initials();
        return 0;
    }
    if (pressed & (PAD_B | PAD_A | PAD_RIGHT)) {
        if (g_story.cursor == 2) {
            story_save_initials();
            return 0;
        }
        ++g_story.cursor;
    } else if ((pressed & (PAD_Y | PAD_LEFT)) && g_story.cursor) {
        --g_story.cursor;
    }
    return 1;
}

const char *story_quip(u8 loser, u16 roll)
{
    const char *const *lines;
    u8 pick;

    if (roll % 3 != 0)
        return 0;
    if (loser == 0)
        lines = g_side_quips[QUIPS_PLAYER];
    else if (g_match_config.opponent < OPPONENT_COUNT)
        lines = g_opponent_quips[g_match_config.opponent];
    else if (match_vs_cpu() == 0)
        lines = g_side_quips[QUIPS_PLAYER];
    else
        lines = g_side_quips[QUIPS_CPU];
    /* ADR-063: a shuffled deck per side. Each pass starts somewhere and
     * steps by an odd stride, both from the frame count, so it says all
     * sixteen lines before any again, in a new order each pass. No RNG:
     * the frame count stands in, as for the CPU's duds (ADR-058). */
    loser = loser ? 1 : 0;
    if (s_quip_left[loser] == 0) {
        s_quip_next[loser] = (u8)((roll >> 2) & (OPPONENT_QUIPS - 1));
        s_quip_stride[loser] = (u8)(((roll >> 6) & (OPPONENT_QUIPS - 1)) | 1);
        s_quip_left[loser] = OPPONENT_QUIPS;
        /* Not on the line the last pass ended on: that one comes last. */
        if (lines[s_quip_next[loser]] == s_last_quip)
            s_quip_next[loser] = (u8)((s_quip_next[loser] +
                s_quip_stride[loser]) & (OPPONENT_QUIPS - 1));
    }
    pick = s_quip_next[loser];
    s_quip_next[loser] = (u8)((pick + s_quip_stride[loser]) &
        (OPPONENT_QUIPS - 1));
    --s_quip_left[loser];
    s_last_quip = lines[pick];
    return s_last_quip;
}

void story_begin_unlock(void)
{
    if (g_attract)
        return;
    if (g_career_new_unlocks == 0)
        return;
    g_story.card = STORY_CARD_UNLOCK;
    g_story.timer = 0;
}

static void story_begin_scene(u8 scene)
{
    g_story.card = STORY_CARD_SCENE;
    g_story.timer = 0;
    g_story.scene = scene;
    g_story.page = g_story_first[scene];
    g_story.end = g_story_first[scene + 1];
}

void story_begin_intro(void)
{
    g_story.timer = 0;
    g_story.cleared = 0;
    g_story.card = (!g_attract && g_match_config.opponent != OPPONENT_NONE) ?
        STORY_CARD_INTRO : STORY_CARD_NONE;
    if (g_story.card == STORY_CARD_INTRO && g_match_config.story)
        story_begin_scene(g_match_config.opponent);
}

void story_begin_ending(void)
{
    g_story.cleared = 0;
    story_begin_scene(STORY_ENDING);
}

const char *story_speaker(u8 page)
{
    u8 who = g_story_who[page];

    if (who == STORY_WHO_PARTNER)
        return g_story_partner_name;
    if (who == STORY_WHO_VILLAIN)
        return g_story_villain;
    if (who == STORY_WHO_CAPTAIN)
        return g_opponent_captain[g_match_config.opponent];
    if (who == STORY_WHO_MATE)
        return g_story_mate[g_match_config.opponent];
    return 0;
}

/* A scene's frame: a page turns on B or A, Start skips to its end. Past
 * the last page a chapter opens its intro card; the ending, the title. */
static u8 story_update_scene(u16 pressed)
{
    if (++g_story.timer < STORY_PAGE_MIN_FRAMES)
        return 1;
    if (pressed & PAD_START)
        g_story.page = g_story.end;
    else if (pressed & (PAD_B | PAD_A))
        ++g_story.page;
    else
        return 1;
    g_story.timer = 0;
    if (g_story.page < g_story.end)
        return 1;
    if (g_story.scene == STORY_ENDING) {
        g_story.card = STORY_CARD_NONE;
        game_start_attract();
        return 0;
    }
    g_story.card = STORY_CARD_INTRO;
    return 1;
}

u8 story_update(u16 pressed, u16 held)
{
    if (g_story.card == STORY_CARD_NONE)
        return 0;
    if (g_story.card == STORY_CARD_INITIALS)
        return story_update_initials(pressed, held);
    if (g_story.card == STORY_CARD_SCENE)
        return story_update_scene(pressed);
    ++g_story.timer;
    if (g_story.card == STORY_CARD_UNLOCK) {
        if (g_story.timer >= STORY_UNLOCK_FRAMES ||
                (g_story.timer >= STORY_UNLOCK_MIN_FRAMES &&
                 (pressed & (PAD_B | PAD_A | PAD_START)))) {
            g_story.card = STORY_CARD_NONE;
            g_career_new_unlocks = 0;
        }
        return g_story.card != STORY_CARD_NONE;
    }
    if (g_story.timer >= STORY_INTRO_FRAMES ||
            (g_story.timer >= STORY_INTRO_MIN_FRAMES &&
             (pressed & (PAD_B | PAD_A | PAD_START))))
        g_story.card = STORY_CARD_NONE;
    return g_story.card != STORY_CARD_NONE;
}
