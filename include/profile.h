#ifndef PROFILE_H
#define PROFILE_H

#include "game_types.h"

/* Scanline profiler; marks compile away unless BUILD_PROFILE is defined. */
enum { PROFILE_STAGES = 16 };

#ifdef BUILD_PROFILE
void profile_mark(u8 stage);
#define PROFILE_MARK(stage) profile_mark(stage)
#else
#define PROFILE_MARK(stage)
#endif

#endif
