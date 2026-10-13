#ifndef MATCH_H
#define MATCH_H

#include "game_types.h"
#include "input.h"
#include "palette_parts.h"

#define MATCH_NO_PALETTE 0xff

/* Per-match presentation choices (ADR-010/013): colour never follows slot. */
typedef struct {
    u8 palette_id[MAX_PLAYERS];   /* PAL_BLUE..PAL_GOLD or MATCH_NO_PALETTE */
    u8 skin_id[MAX_PLAYERS];
    u8 paddle_id[MAX_PLAYERS];
} MatchConfig;

extern MatchConfig g_match_config;

/* POC defaults: P1 (slot 0) blue, CPU (slot 2) red, slots 1/3 unused. */
void match_config_defaults(void);
/* Nonzero when every assigned palette is unique; asserted in debug builds. */
u8 match_config_palettes_unique(void);
/* Builds a slot's 16-colour OBJ palette from shared/team/skin/paddle parts. */
void match_compose_palette(u8 slot, u16 out[16]);
/* Same composition from explicit part IDs (stress scene, select screens). */
void match_compose_parts(u8 palette_id, u8 skin_id, u8 paddle_id,
    u16 out[16]);
/* HUD label colour for a team: the palette of its first slot (0 or 2). */
u8 match_team_palette(u8 team);

#endif
