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
 * OBJ palette layouts (ADR-018). Only palettes 0-3 are exempt from colour
 * math. Singles: players 0-1, objects 2, flash 3, translucent shadow 4.
 * Doubles: players 0-3, objects 4, flash 5, opaque shadow 6, math off.
 * Player.palette holds the player's slot in the active layout.
 */
enum {
    RENDER_OBJPAL_OBJECTS = 2,        /* singles values */
    RENDER_OBJPAL_FLASH = 3,
    RENDER_OBJPAL_SHADOW = 4,
    RENDER_OBJPAL_DOUBLES_OBJECTS = 4,
    RENDER_OBJPAL_DOUBLES_FLASH = 5,
    RENDER_OBJPAL_DOUBLES_SHADOW = 6
};

/* OAM entry indices (docs/03_SPRITE_ASSET_PIPELINE.md OAM allocation). */
enum {
    RENDER_OAM_BALL = 0,
    RENDER_OAM_BALL_SHADOW = 1,
    RENDER_OAM_BODIES = 2,        /* 2-5: sorted nearest-first (ADR-012) */
    RENDER_OAM_SHADOWS = 6,       /* 6-9: by slot */
    RENDER_OAM_SPARK = 10,        /* 10-13: impact effects reserve */
    RENDER_OAM_DEBUG_MARKER = 11,
    RENDER_OAM_ACCESSORIES = 14,  /* 14-17: ponytails by slot (ADR-014) */
    RENDER_OAM_MACHINE = 18,      /* 18-21: practice ball machine (ADR-028) */
    RENDER_OAM_FIRST_HIDDEN = 22
};

void render_initialize(void);
/* Composes player palettes and HUD team colours from g_match_config; the
 * upload happens in the next render_commit (or immediately in forced blank
 * during render_initialize). */
void render_apply_match_config(void);
/* Recomposes the ground palette from the saved court colours (ADR-022);
 * uploaded in render_commit's VBlank. */
void render_apply_court_colors(void);
/* Mode whose palettes/colour math are currently set up. */
u8 render_match_mode_applied(void);
/* Scene switches with a fade (ADR-020): the court and match HUD from the
 * current g_match_config, or the Player Select screen. */
void render_open_match(void);
void render_select_open(void);
/* Player Select frame: buffers before VBlank, uploads inside it. */
void render_select_prepare(void);
void render_select_commit(void);
/* Brightness fade out (1) or in (0) over a few frames. */
void render_fade(u8 out);
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
