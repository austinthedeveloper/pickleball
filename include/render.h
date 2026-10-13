#ifndef RENDER_H
#define RENDER_H

#include "game_types.h"

/* VRAM word addresses; see docs/09_M1_VRAM_AND_BUILD.md. */
#define RENDER_BG1_MAP_ADDR 0x0000
#define RENDER_BG3_MAP_ADDR 0x0400
#define RENDER_BG1_TILES_ADDR 0x1000
#define RENDER_BG3_TILES_ADDR 0x5000
#define RENDER_OBJ_TILES_ADDR 0x6000

/*
 * OBJ palette slots. Only palettes 0-3 are exempt from colour math, so the
 * two POC players, objects, and the hit flash fill them; palette 4 is the
 * colour-math shadow. Doubles needs a new plan here (see docs/09).
 */
enum {
    RENDER_OBJPAL_PLAYER_A = 0,   /* slot 0 */
    RENDER_OBJPAL_PLAYER_B = 1,   /* slot 2 */
    RENDER_OBJPAL_OBJECTS = 2,
    RENDER_OBJPAL_FLASH = 3,
    RENDER_OBJPAL_SHADOW = 4
};

/* OAM entry indices (docs/03_SPRITE_ASSET_PIPELINE.md OAM allocation). */
enum {
    RENDER_OAM_BALL = 0,
    RENDER_OAM_BALL_SHADOW = 1,
    RENDER_OAM_BODIES = 2,        /* 2-5: sorted nearest-first (ADR-012) */
    RENDER_OAM_SHADOWS = 6,       /* 6-9: by slot */
    RENDER_OAM_SPARK = 10,        /* 10-13: impact effects reserve */
    RENDER_OAM_DEBUG_MARKER = 11,
    RENDER_OAM_ACCESSORIES = 14,  /* 14-17: ADR-014, unused in the POC */
    RENDER_OAM_FIRST_HIDDEN = 18
};

void render_initialize(void);
/* Composes player palettes and HUD team colours from g_match_config; the
 * upload happens in the next render_commit (or immediately in forced blank
 * during render_initialize). */
void render_apply_match_config(void);
/* Builds OAM and HUD buffers; call before WaitForVBlank. */
void render_prepare(void);
/* VBlank-only uploads (HUD bands, pending palettes). */
void render_commit(void);

/* Frames on which the PPU reported OBJ range/time over ($213E bits 6/7). */
extern u16 g_obj_overflow_frames;

#if defined(BUILD_DEBUG) || defined(BUILD_HARDWARE_TEST)
#define RENDER_DIAGNOSTICS 1
/* OBJ stress scene (M3-OBJ-01): toggled with Select in diagnostic builds. */
void render_stress_toggle(void);
u8 render_stress_active(void);
#endif

#endif
