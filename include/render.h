#ifndef RENDER_H
#define RENDER_H

#include "game_types.h"

/* VRAM word addresses; see docs/09_M1_VRAM_AND_BUILD.md. */
#define RENDER_BG1_MAP_ADDR 0x0000
#define RENDER_BG3_MAP_ADDR 0x0400
#define RENDER_BG1_TILES_ADDR 0x1000
#define RENDER_BG3_TILES_ADDR 0x5000
#define RENDER_OBJ_TILES_ADDR 0x6000

/* OBJ palette slots. Palettes 4..7 take part in shadow color math. */
enum {
    RENDER_OBJPAL_TEAM0 = 0,
    RENDER_OBJPAL_TEAM1 = 1,
    RENDER_OBJPAL_OBJECTS = 2,
    RENDER_OBJPAL_FLASH = 3,
    RENDER_OBJPAL_SHADOW = 4
};

/* OAM entry indices (multiply by 4 for PVSnesLib byte offsets). */
enum {
    RENDER_OAM_SPARK = 0,
    RENDER_OAM_BALL = 1,
    RENDER_OAM_BODY_FRONT = 2,
    RENDER_OAM_BODY_BACK = 3,
    RENDER_OAM_DEBUG_MARKER = 4,
    RENDER_OAM_BALL_SHADOW = 5,
    RENDER_OAM_SHADOW_FRONT = 6,
    RENDER_OAM_SHADOW_BACK = 8,
    RENDER_OAM_USED = 10
};

void render_initialize(void);
s16 render_project_x(s16 x, s16 y);
/* Team colour (TEAM_BLUE..TEAM_GOLD) shown for the left (0) or right team. */
void render_set_team_color(u8 team, u8 color);
u8 render_team_color(u8 team);
/* Builds OAM and HUD buffers; call before WaitForVBlank. */
void render_prepare(void);
/* VBlank-only uploads (HUD bands, pending palettes). */
void render_commit(void);

#endif
