#ifndef RENDER_H
#define RENDER_H

#include "game_types.h"

void render_initialize(void);
void render_prepare_players(void);
void render_prepare_ball(void);
void render_prepare_hud(u8 paused);
void render_commit_hud(u8 paused);

#endif

