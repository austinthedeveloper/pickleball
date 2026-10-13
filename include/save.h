#ifndef SAVE_H
#define SAVE_H

#include "game_types.h"
#include "court_colors.h"

/*
 * Battery-backed save RAM (docs/decisions/ADR-021-battery-save.md).
 * The cart has 2 KiB of SRAM holding two copies of SaveData; each write goes
 * to the older copy, so a power cut mid-write loses at most that write.
 */

enum {
    SAVE_SRAM_BYTES = 2048,
    SAVE_COPY_STRIDE = 1024,    /* copy 0 at offset 0, copy 1 at 1024 */
    SAVE_VERSION = 4,           /* 2: settings (ADR-022); 3: venue; 4: options */
    SAVE_COUNTER_MAX = 9999     /* counters saturate; four digits on screen */
};

/* Records against the CPU (2P matches only count toward the longest rally). */
typedef struct {
    u16 wins[2];                /* by MatchMode: singles, doubles */
    u16 losses[2];
    u16 streak;                 /* current win streak, either mode */
    u16 best_streak;
    u16 shutouts;               /* wins without conceding a point */
    u16 longest_rally;          /* shots in one point, serve included */
} SaveRecords;

/* Choices that persist (ADR-022). */
typedef struct {
    u8 court[COURT_PARTS];      /* court colour preset per part; 0 DEFAULT */
    u8 ladder;                  /* next opponent (ADR-023) */
    u8 venue;                   /* COURT_VENUE_AUTO or venue + 1 (ADR-024) */
    u8 points;                  /* points to win (ADR-025) */
    u8 difficulty;              /* Difficulty (ADR-025) */
    u8 reserved;
} SaveSettings;

typedef struct {
    u8 magic[4];                /* "PB94" */
    u8 version;                 /* SAVE_VERSION; any other value resets */
    u8 sequence;                /* the newer copy has the higher (wrapping) */
    SaveRecords records;
    SaveSettings settings;
    u16 checksum;               /* over every byte before it */
} SaveData;

extern SaveData g_save;

/* Loads the newer valid copy, or formats SRAM with defaults when neither
 * copy is valid (blank cart, corruption, older layout). */
void save_initialize(void);
/* Writes g_save to the older copy. Call only at safe points. */
void save_write(void);
/* Back to defaults and written to both copies. */
void save_clear(void);
/* Zeroes the records only (settings stay) and writes. */
void save_clear_records(void);

/* A finished human match. vs_cpu: the opponents were all CPU (1P is always
 * on team 0). Updates the records and writes. */
void save_record_match(u8 mode, u8 vs_cpu, u8 won, u8 points_against);
/* A win over a ladder opponent: the next one is up (after the last, the
 * first again). Written with the match. */
void save_advance_ladder(void);
/* A finished point with this many shots; written with the match. */
void save_record_rally(u8 shots);

/* Validation, exposed for the host tests. */
u16 save_checksum(const SaveData *data);
u8 save_valid(const SaveData *data);

#ifdef HOST_TEST
/* Stand-in for the cartridge SRAM. */
extern u8 g_host_sram[SAVE_SRAM_BYTES];
#endif

#endif
