#include <snes.h>
#include "input.h"

PadSnapshot g_pad_snapshots[MAX_HUMAN_PADS];
PlayerIntent g_player_intents[MAX_PLAYERS];
u8 g_input_owners[MAX_PLAYERS];

static void clear_intent(PlayerIntent *intent)
{
    intent->move_x = 0;
    intent->move_y = 0;
    intent->hit_pressed = 0;
    intent->pause_pressed = 0;
}

void input_initialize(void)
{
    u8 i;
    for (i = 0; i < MAX_HUMAN_PADS; ++i) {
        g_pad_snapshots[i].held = 0;
        g_pad_snapshots[i].pressed = 0;
        g_pad_snapshots[i].released = 0;
        clear_intent(&g_player_intents[i]);
        g_input_owners[i] = INPUT_OWNER_NONE;
    }
    g_input_owners[0] = INPUT_OWNER_HUMAN;
    g_input_owners[2] = INPUT_OWNER_CPU;
}

void input_capture_frame(void)
{
    u8 i;
    u16 current;

    for (i = 0; i < MAX_HUMAN_PADS; ++i) {
        current = padsCurrent(i);
        input_update_snapshot(&g_pad_snapshots[i], current);
        clear_intent(&g_player_intents[i]);
    }

    if (g_pad_snapshots[0].held & KEY_LEFT)
        g_player_intents[0].move_x = -1;
    else if (g_pad_snapshots[0].held & KEY_RIGHT)
        g_player_intents[0].move_x = 1;

    if (g_pad_snapshots[0].held & KEY_UP)
        g_player_intents[0].move_y = -1;
    else if (g_pad_snapshots[0].held & KEY_DOWN)
        g_player_intents[0].move_y = 1;

    g_player_intents[0].hit_pressed =
        (g_pad_snapshots[0].pressed & KEY_B) != 0;
    g_player_intents[0].pause_pressed =
        (g_pad_snapshots[0].pressed & KEY_START) != 0;
}

