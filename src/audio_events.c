#include "audio_events.h"

u16 g_audio_event_counts[6];

void audio_events_initialize(void)
{
    u8 i;
    for (i = 0; i < 6; ++i)
        g_audio_event_counts[i] = 0;
}

void audio_on_event(u8 kind, u8 detail)
{
    (void)detail;
    if (kind < 6)
        ++g_audio_event_counts[kind];
}
