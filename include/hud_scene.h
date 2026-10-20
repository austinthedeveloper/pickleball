#ifndef HUD_SCENE_H
#define HUD_SCENE_H

#include "game_types.h"

/* Score panels and staged scene overlays on BG3 (src/hud_scene.c). */
void hud_scene_initialize(void);
/* Forces a full redraw on the next update (after a full-screen overlay). */
void hud_scene_invalidate(void);
/* Redraws the title menu or records page (after the records change). */
void hud_scene_refresh_title(void);
/* Call once per frame before VBlank; touches only WRAM. */
void hud_scene_update(void);

/* Title menu entries (g_title_mode), ADR-065: the ways to play (STORY
 * first, ADR-077, and the TUTORIAL, ADR-075), then the players and
 * records, then the settings. The cursor starts on SINGLES. */
enum { TITLE_STORY = 0, TITLE_SINGLES = 1, TITLE_DOUBLES = 2,
       TITLE_OPEN_PLAY = 3, TITLE_PRACTICE = 4, TITLE_TUTORIAL = 5,
       TITLE_CAREER = 6, TITLE_RECORDS = 7, TITLE_COURT = 8,
       TITLE_OPTIONS = 9, TITLE_MODIFIERS = 10, TITLE_ENTRIES = 11 };
/* Title pages (g_title_page): the menu, or a page over it. CAREER holds
 * the RECORDS table too (a view of its own). */
enum { TITLE_PAGE_MENU = 0, TITLE_PAGE_COURT = 1, TITLE_PAGE_CAREER = 2,
       TITLE_PAGE_OPTIONS = 3, TITLE_PAGE_PRACTICE = 4,
       TITLE_PAGE_MODIFIERS = 5 };

/* Controllers joined on the title (ADR-066): bit n for controller n + 1.
 * 1P is always in; 2P is on port 2, 3P and 4P on a Multitap. Only 1P and
 * 2P play until M4-MP-01. */
enum { TITLE_JOIN_1P = 1, TITLE_JOIN_2P = 2, TITLE_JOIN_3P = 4,
       TITLE_JOIN_4P = 8 };

/* Title menu choice, the open page, and who joined (set by main.c). */
extern u8 g_title_mode;
extern u8 g_title_page;
extern u8 g_title_joined;

/* The close call's frame is drawn (ADR-041): its close-up may show. */
u8 hud_replay_framed(void);

#endif
