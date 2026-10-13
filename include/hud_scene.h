#ifndef HUD_SCENE_H
#define HUD_SCENE_H

#include "game_types.h"

/* Score panels and staged scene overlays on BG3 (src/hud_scene.c). */
void hud_scene_initialize(void);
/* Forces a full redraw on the next update (after a full-screen overlay). */
void hud_scene_invalidate(void);
/* Call once per frame before VBlank; touches only WRAM. */
void hud_scene_update(void);

#endif
