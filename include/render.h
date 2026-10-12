#ifndef RENDER_H
#define RENDER_H

#include "game_types.h"

void render_initialize(void);
void render_prepare_players(void);
void render_commit_hud(void);
void render_set_paused(u8 paused);

#endif

