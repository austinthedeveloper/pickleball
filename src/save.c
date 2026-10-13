#include <string.h>
#include "opponents.h"
#include "options.h"
#include "practice.h"
#include "save.h"

SaveData g_save;
/* Copy that holds g_save; the next write goes to the other one. */
static u8 s_current;

static const u8 s_magic[4] = { 'P', 'B', '9', '4' };

/* Sizes the build checks: a layout change must not slip through. */
typedef char save_v4_is_32_bytes[sizeof(SaveV4) == 32 ? 1 : -1];
typedef char save_v7_is_40_bytes[sizeof(SaveData) == 40 ? 1 : -1];

/* The largest known layout: each copy is read whole, whatever it holds. */
#define SAVE_RAW_BYTES 40

#ifdef HOST_TEST
u8 g_host_sram[SAVE_SRAM_BYTES];

static void sram_read(u8 *dest, u16 offset)
{
    memcpy(dest, g_host_sram + offset, SAVE_RAW_BYTES);
}

static void sram_write(const SaveData *source, u16 offset)
{
    memcpy(g_host_sram + offset, source, sizeof(SaveData));
}
#else
static void sram_read(u8 *dest, u16 offset)
{
    consoleLoadSramWithOffset(dest, SAVE_RAW_BYTES, offset);
}

static void sram_write(const SaveData *source, u16 offset)
{
    consoleCopySramWithOffset((u8 *)source, sizeof(SaveData), offset);
}
#endif

/* Fletcher-style: two running byte sums, so swapped bytes also change it.
 * The same for every layout. */
u16 save_checksum_bytes(const u8 *byte, u16 count)
{
    u8 a = 0;
    u8 b = 0;

    while (count--) {
        a += *byte++;
        b += a;
    }
    return ((u16)b << 8) | a;
}

u16 save_checksum(const SaveData *data)
{
    /* The checksum is the last field. */
    return save_checksum_bytes((const u8 *)data,
        sizeof(SaveData) - sizeof(u16));
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

/* Block size of a known layout, or 0 for an unknown version. */
static u16 save_layout_size(u8 version)
{
    if (version == SAVE_VERSION)
        return sizeof(SaveData);
    if (version == SAVE_V4)
        return sizeof(SaveV4);
    return 0;
}

/* A copy in any known layout with a correct checksum. */
static u8 save_raw_valid(const u8 *raw)
{
    u16 size = save_layout_size(raw[4]);

    if (memcmp(raw, s_magic, 4) != 0 || size == 0)
        return 0;
    return (u16)(raw[size - 2] | (raw[size - 1] << 8)) ==
        save_checksum_bytes(raw, size - 2);
}

/* v0.4.0 to now: everything carries over; ladder clears, the practice best
 * and settings, the kitchen colour (DEFAULT), and SOUND (ALL) start fresh. */
static void save_migrate_v4(const u8 *raw)
{
    static SaveV4 old;

    memcpy(&old, raw, sizeof(old));
    save_defaults();
    g_save.sequence = old.sequence;
    g_save.records.wins[0] = old.wins[0];
    g_save.records.wins[1] = old.wins[1];
    g_save.records.losses[0] = old.losses[0];
    g_save.records.losses[1] = old.losses[1];
    g_save.records.streak = old.streak;
    g_save.records.best_streak = old.best_streak;
    g_save.records.shutouts = old.shutouts;
    g_save.records.longest_rally = old.longest_rally;
    g_save.settings.court[COURT_PART_SURFACE] = old.court[0];
    g_save.settings.court[COURT_PART_SURROUND] = old.court[1];
    g_save.settings.court[COURT_PART_LINES] = old.court[2];
    g_save.settings.ladder = old.ladder;
    g_save.settings.venue = old.venue;
    g_save.settings.points = old.points;
    g_save.settings.difficulty = old.difficulty;
}

void save_initialize(void)
{
    static u8 raw[2][SAVE_RAW_BYTES];
    u8 valid0;
    u8 valid1;
    u8 ahead;
    u8 pick;

    sram_read(raw[0], 0);
    sram_read(raw[1], SAVE_COPY_STRIDE);
    valid0 = save_raw_valid(raw[0]);
    valid1 = save_raw_valid(raw[1]);
    if (!valid0 && !valid1) {
        save_clear();
        return;
    }
    /* The newer valid copy (byte 5 is the sequence in every layout). */
    ahead = (u8)(raw[1][5] - raw[0][5]);
    pick = (valid1 && (!valid0 || (ahead != 0 && ahead < 0x80))) ? 1 : 0;
    s_current = pick;
    if (raw[pick][4] == SAVE_VERSION) {
        memcpy(&g_save, raw[pick], sizeof(g_save));
        return;
    }
    /* An older layout (ADR-032): migrate, then store it as current in both
     * copies so the old one is never read again. */
    save_migrate_v4(raw[pick]);
    save_write();
    save_write();
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
