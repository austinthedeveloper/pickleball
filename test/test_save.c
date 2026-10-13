#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "game.h"
#include "match.h"
#include "rules.h"
#include "save.h"

/* ADR-021 battery save: validation, copy selection, records, game hooks. */

static SaveData copy_at(u16 offset)
{
    SaveData data;

    memcpy(&data, g_host_sram + offset, sizeof(data));
    return data;
}

static void test_blank_sram_formats(void)
{
    SaveData a;
    SaveData b;

    memset(g_host_sram, 0xff, sizeof(g_host_sram));
    save_initialize();
    assert(save_valid(&g_save));
    assert(g_save.records.wins[MATCH_SINGLES] == 0);
    a = copy_at(0);
    b = copy_at(SAVE_COPY_STRIDE);
    assert(save_valid(&a) && save_valid(&b));
    assert(a.sequence != b.sequence);
}

static void test_round_trip_and_newer_copy(void)
{
    memset(g_host_sram, 0, sizeof(g_host_sram));
    save_initialize();
    g_save.records.wins[MATCH_DOUBLES] = 7;
    save_write();
    g_save.records.wins[MATCH_DOUBLES] = 8;
    save_write();
    memset(&g_save, 0, sizeof(g_save));
    save_initialize();
    assert(g_save.records.wins[MATCH_DOUBLES] == 8);

    /* Writes alternate: the next one lands on the older copy. */
    g_save.records.wins[MATCH_DOUBLES] = 9;
    save_write();
    save_initialize();
    assert(g_save.records.wins[MATCH_DOUBLES] == 9);
}

static void test_corrupt_copy_falls_back(void)
{
    SaveData a;
    u16 newer;

    memset(g_host_sram, 0, sizeof(g_host_sram));
    save_initialize();
    g_save.records.shutouts = 3;
    save_write();
    g_save.records.shutouts = 4;
    save_write();
    a = copy_at(0);
    newer = a.records.shutouts == 4 ? 0 : SAVE_COPY_STRIDE;

    /* A power cut mid-write: the newer copy is torn, the older one stays. */
    g_host_sram[newer + 8] ^= 0x5a;
    save_initialize();
    assert(g_save.records.shutouts == 3);

    /* Both torn: back to defaults. */
    g_host_sram[(newer ^ SAVE_COPY_STRIDE) + 8] ^= 0x5a;
    save_initialize();
    assert(g_save.records.shutouts == 0);
}

static void test_version_mismatch_resets(void)
{
    memset(g_host_sram, 0, sizeof(g_host_sram));
    save_initialize();
    g_save.records.best_streak = 5;
    g_save.version = SAVE_VERSION + 1;
    save_write();
    save_write();
    save_initialize();
    assert(g_save.version == SAVE_VERSION);
    assert(g_save.records.best_streak == 0);
}

static void test_sequence_wraps(void)
{
    u16 i;

    memset(g_host_sram, 0, sizeof(g_host_sram));
    save_initialize();
    for (i = 0; i < 300; ++i) {
        g_save.records.longest_rally = i;
        save_write();
    }
    save_initialize();
    assert(g_save.records.longest_rally == 299);
}

static void test_records(void)
{
    u16 i;

    memset(g_host_sram, 0, sizeof(g_host_sram));
    save_initialize();
    save_record_match(MATCH_SINGLES, 1, 1, 0);
    save_record_match(MATCH_SINGLES, 1, 1, 3);
    assert(g_save.records.wins[MATCH_SINGLES] == 2);
    assert(g_save.records.shutouts == 1);
    assert(g_save.records.streak == 2 && g_save.records.best_streak == 2);
    save_record_match(MATCH_DOUBLES, 1, 0, 2);
    assert(g_save.records.losses[MATCH_DOUBLES] == 1);
    assert(g_save.records.streak == 0 && g_save.records.best_streak == 2);

    /* 2P versus matches leave the CPU records alone. */
    save_record_match(MATCH_SINGLES, 0, 1, 0);
    assert(g_save.records.wins[MATCH_SINGLES] == 2);
    assert(g_save.records.shutouts == 1);

    save_record_rally(12);
    save_record_rally(9);
    assert(g_save.records.longest_rally == 12);

    /* Written with the match, and loaded back. */
    save_record_match(MATCH_SINGLES, 1, 1, 1);
    memset(&g_save, 0, sizeof(g_save));
    save_initialize();
    assert(g_save.records.longest_rally == 12);
    assert(g_save.records.wins[MATCH_SINGLES] == 3);

    /* Counters saturate at four digits. */
    g_save.records.wins[MATCH_SINGLES] = SAVE_COUNTER_MAX;
    for (i = 0; i < 3; ++i)
        save_record_match(MATCH_SINGLES, 1, 1, 1);
    assert(g_save.records.wins[MATCH_SINGLES] == SAVE_COUNTER_MAX);

    /* Erasing records keeps the settings (ADR-022). */
    g_save.settings.court[0] = 3;
    save_clear_records();
    assert(g_save.records.wins[MATCH_SINGLES] == 0);
    assert(g_save.settings.court[0] == 3);
    save_initialize();
    assert(g_save.records.longest_rally == 0);
    assert(g_save.settings.court[0] == 3);

    save_clear();
    assert(g_save.records.wins[MATCH_SINGLES] == 0);
    assert(g_save.settings.court[0] == 0);
    save_initialize();
    assert(g_save.records.longest_rally == 0);
}

/* Drives the result phases until the next serve or the match result. */
static void finish_point(PlayerIntent intents[MAX_PLAYERS])
{
    u16 n;

    for (n = 0; n < 400 && g_game_state.phase == GAME_PHASE_POINT_RESULT;
            ++n)
        game_update(intents);
}

/* A human match vs the CPU records once; the attract demo never does. */
static void test_game_hooks(void)
{
    PlayerIntent intents[MAX_PLAYERS];
    u8 point;

    memset(intents, 0, sizeof(intents));
    memset(g_host_sram, 0, sizeof(g_host_sram));
    save_initialize();

    match_config_defaults();
    g_attract = 0;
    game_new_match();
    for (point = 0; point < GAME_POINTS_TO_WIN; ++point) {
        g_game_state.phase = GAME_PHASE_LIVE;
        g_game_state.shot_serial += 6;       /* a serve and five returns */
        g_ball.rally_state = BALL_RALLY_LIVE;
        rules_resolve_point(1, GAME_FAULT_OUT);     /* the CPU loses it */
        finish_point(intents);
    }
    assert(g_game_state.phase == GAME_PHASE_MATCH_RESULT);
    assert(g_save.records.wins[MATCH_SINGLES] == 1);
    assert(g_save.records.shutouts == 1);
    assert(g_save.records.longest_rally == 6);

    /* The match result does not record twice. */
    for (point = 0; point < 50; ++point)
        game_update(intents);
    assert(g_save.records.wins[MATCH_SINGLES] == 1);

    g_attract = 1;
    game_new_match();
    for (point = 0; point < GAME_POINTS_TO_WIN; ++point) {
        g_game_state.phase = GAME_PHASE_LIVE;
        g_game_state.shot_serial += 40;
        g_ball.rally_state = BALL_RALLY_LIVE;
        rules_resolve_point(0, GAME_FAULT_OUT);
        finish_point(intents);
    }
    assert(g_save.records.losses[MATCH_SINGLES] == 0);
    assert(g_save.records.longest_rally == 6);
    g_attract = 0;
}

int main(void)
{
    test_blank_sram_formats();
    test_round_trip_and_newer_copy();
    test_corrupt_copy_falls_back();
    test_version_mismatch_resets();
    test_sequence_wraps();
    test_records();
    test_game_hooks();
    printf("save host checks passed\n");
    return 0;
}
