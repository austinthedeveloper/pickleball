#ifndef AUDIO_EVENTS_H
#define AUDIO_EVENTS_H

#include "game_types.h"

/*
 * Audio seam (docs/02 src/audio_events). The POC has no audio (ADR-009):
 * events are only counted so tests can see that the hooks fire. A sound
 * driver replaces audio_on_event later without touching the simulation.
 */
extern u16 g_audio_event_counts[6];

void audio_events_initialize(void);
void audio_on_event(u8 kind, u8 detail);

#endif
