#ifndef MATCH_H
#define MATCH_H

#include "game_types.h"
#include "input.h"
#include "opponents.h"
#include "paddle_stats.h"
#include "palette_parts.h"

#define MATCH_NO_PALETTE 0xff

typedef enum {
    MATCH_SINGLES = 0,
    MATCH_DOUBLES = 1     /* docs/decisions/ADR-018-doubles.md */
} MatchMode;

/* Per-match choices (ADR-010/013/018): colour never follows slot. */
typedef struct {
    u8 mode;
    u8 palette_id[MAX_PLAYERS];   /* PAL_BLUE..PAL_GOLD or MATCH_NO_PALETTE */
    u8 skin_id[MAX_PLAYERS];
    u8 paddle_id[MAX_PLAYERS];    /* PADDLE_* (ADR-020) */
    u8 variant[MAX_PLAYERS];      /* PLAYER_VARIANT_* */
    u8 human[MAX_PLAYERS];        /* 1P on slot 0, 2P on slot 2 */
    u8 opponent;                  /* ADR-023 personality on slots 2-3, or
                                     OPPONENT_NONE (the default CPU) */
} MatchConfig;

/* One human's Player Select result (ADR-020). */
typedef struct {
    u8 palette_id;
    u8 variant;
    u8 paddle_id;
} PlayerChoice;

extern MatchConfig g_match_config;

/* Singles defaults: P1 (slot 0) blue, CPU (slot 2) red, slots 1/3 unused. */
void match_config_defaults(void);
/* Doubles defaults from the mockup: blue+gold (0, 1) vs red+green (2, 3). */
void match_config_doubles(void);
/* Config from Player Select: 1P on slot 0, optional 2P on slot 2; CPUs
 * take the unused colours in order, the standard paddle, and the fixed
 * variants (slots 1 and 3 female). */
void match_config_select(u8 mode, const PlayerChoice *p1,
    const PlayerChoice *p2);
/* The right team's CPUs become an opponent's captain (slot 2) and partner
 * (slot 3): look, body variant, and paddle (ADR-023). Only when no human
 * plays on that team; OPPONENT_NONE keeps the default CPU. */
void match_config_opponent(u8 opponent);
/* The opponent member (0 captain, 1 partner) playing the slot, or 0xff. */
u8 match_slot_member(u8 slot);
/* Play style of a CPU slot: its opponent member's, or the default. */
const AiStyle *match_slot_style(u8 slot);
/* BG3 label colours 1-2 for a team. */
const u16 *match_team_hud_colors(u8 team);
/* Nonzero when the slot plays in the configured mode. */
u8 match_slot_active(u8 slot);
/* Nonzero when every assigned palette is unique; asserted in debug builds. */
u8 match_config_palettes_unique(void);
/* Builds a slot's 16-colour OBJ palette from shared/team/skin/paddle parts. */
void match_compose_palette(u8 slot, u16 out[16]);
/* Same composition from explicit part IDs (stress scene, select screens). */
void match_compose_parts(u8 palette_id, u8 skin_id, u8 paddle_id,
    u16 out[16]);
/* HUD label colour for a team: the palette of its first slot (0 or 2). */
u8 match_team_palette(u8 team);
/* Lane per ADR-018: slot 0 bottom, 1 top, 2 top, 3 bottom. */
u8 match_slot_lane_top(u8 slot);

#endif
