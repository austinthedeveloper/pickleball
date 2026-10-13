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

/* Constant indices throughout: 816-tcc turns variable array indexing into a
 * multiply call, and this runs every frame. */
#define CAPTURE_PAD(n) \
    do { \
        u16 current = padsCurrent(n); \
        u16 previous = g_pad_snapshots[n].held; \
        g_pad_snapshots[n].held = current; \
        g_pad_snapshots[n].pressed = current & (u16)~previous; \
        g_pad_snapshots[n].released = previous & (u16)~current; \
        g_player_intents[n].move_x = 0; \
        g_player_intents[n].move_y = 0; \
        g_player_intents[n].aim_x = 0; \
        g_player_intents[n].aim_y = 0; \
        g_player_intents[n].hit_pressed = 0; \
        g_player_intents[n].pause_pressed = 0; \
    } while (0)

void input_capture_frame(void)
{
    u16 held;

    CAPTURE_PAD(0);
    CAPTURE_PAD(1);
    CAPTURE_PAD(2);
    CAPTURE_PAD(3);

    held = g_pad_snapshots[0].held;
    if (held & KEY_LEFT)
        g_player_intents[0].move_x = -1;
    else if (held & KEY_RIGHT)
        g_player_intents[0].move_x = 1;

    if (held & KEY_UP)
        g_player_intents[0].move_y = -1;
    else if (held & KEY_DOWN)
        g_player_intents[0].move_y = 1;

    g_player_intents[0].aim_x = g_player_intents[0].move_x;
    g_player_intents[0].aim_y = g_player_intents[0].move_y;
    g_player_intents[0].hit_pressed =
        (g_pad_snapshots[0].pressed & KEY_B) != 0;
    g_player_intents[0].pause_pressed =
        (g_pad_snapshots[0].pressed & KEY_START) != 0;
}
