#include <string.h>
#include "opponents.h"
#include "options.h"
#include "practice.h"
#include "save.h"

SaveData g_save;
/* Copy that holds g_save; the next write goes to the other one. */
static u8 s_current;

static const u8 s_magic[4] = { 'P', 'B', '9', '4' };

#ifdef HOST_TEST
u8 g_host_sram[SAVE_SRAM_BYTES];

static void sram_read(SaveData *dest, u16 offset)
{
    memcpy(dest, g_host_sram + offset, sizeof(SaveData));
}

static void sram_write(const SaveData *source, u16 offset)
{
    memcpy(g_host_sram + offset, source, sizeof(SaveData));
}
#else
static void sram_read(SaveData *dest, u16 offset)
{
    consoleLoadSramWithOffset((u8 *)dest, sizeof(SaveData), offset);
}

static void sram_write(const SaveData *source, u16 offset)
{
    consoleCopySramWithOffset((u8 *)source, sizeof(SaveData), offset);
}
#endif

/* Fletcher-style: two running byte sums, so swapped bytes also change it. */
u16 save_checksum(const SaveData *data)
{
    const u8 *byte = (const u8 *)data;
    /* The checksum is the last field. */
    u16 count = sizeof(SaveData) - sizeof(u16);
    u8 a = 0;
    u8 b = 0;

    while (count--) {
        a += *byte++;
        b += a;
    }
    return ((u16)b << 8) | a;
}

u8 save_valid(const SaveData *data)
{
    return memcmp(data->magic, s_magic, 4) == 0 &&
        data->version == SAVE_VERSION &&
        data->checksum == save_checksum(data);
}

static void save_defaults(void)
{
    memset(&g_save, 0, sizeof(g_save));
    memcpy(g_save.magic, s_magic, 4);
    g_save.version = SAVE_VERSION;
    g_save.settings.points = OPTIONS_POINTS_DEFAULT;
    g_save.settings.difficulty = DIFFICULTY_MEDIUM;
    /* Practice: MIX to RANDOM targets at a NORMAL rate (ADR-028). */
    g_save.settings.practice[PRACTICE_ROW_SHOT] = PRACTICE_SHOT_MIX;
    g_save.settings.practice[PRACTICE_ROW_TARGET] = PRACTICE_TARGET_RANDOM;
    g_save.settings.practice[PRACTICE_ROW_RATE] = PRACTICE_RATE_NORMAL;
}

void save_write(void)
{
    s_current ^= 1;
    ++g_save.sequence;
    g_save.checksum = save_checksum(&g_save);
    sram_write(&g_save, s_current ? SAVE_COPY_STRIDE : 0);
}

void save_clear(void)
{
    save_defaults();
    save_write();
    save_write();
}

static void save_bump(u16 *counter)
{
    if (*counter < SAVE_COUNTER_MAX)
        ++*counter;
}

void save_clear_records(void)
{
    memset(&g_save.records, 0, sizeof(g_save.records));
    save_write();
}

void save_initialize(void)
{
    static SaveData other;
    u8 valid0;
    u8 valid1;
    u8 ahead;

    sram_read(&g_save, 0);
    sram_read(&other, SAVE_COPY_STRIDE);
    valid0 = save_valid(&g_save);
    valid1 = save_valid(&other);
    ahead = (u8)(other.sequence - g_save.sequence);
    s_current = 0;
    if (valid1 && (!valid0 || (ahead != 0 && ahead < 0x80))) {
        memcpy(&g_save, &other, sizeof(g_save));
        s_current = 1;
    } else if (!valid0) {
        save_clear();
    }
}


void save_advance_ladder(void)
{
    u8 next = g_save.settings.ladder + 1;

    if (next >= OPPONENT_COUNT) {
        next = 0;
        save_bump(&g_save.records.ladder_clears);
    }
    g_save.settings.ladder = next;
}

void save_record_rally(u8 shots)
{
    if (shots > g_save.records.longest_rally)
        g_save.records.longest_rally = shots;
}

void save_record_match(u8 mode, u8 vs_cpu, u8 won, u8 points_against)
{
    SaveRecords *records = &g_save.records;

    if (vs_cpu) {
        if (won) {
            save_bump(&records->wins[mode]);
            save_bump(&records->streak);
            if (records->streak > records->best_streak)
                records->best_streak = records->streak;
            if (points_against == 0)
                save_bump(&records->shutouts);
        } else {
            save_bump(&records->losses[mode]);
            records->streak = 0;
        }
    }
    save_write();
}
