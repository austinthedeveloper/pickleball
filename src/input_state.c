#include "input.h"

PadSnapshot g_pad_snapshots[MAX_PADS];
PlayerIntent g_player_intents[MAX_PLAYERS];
u8 g_input_owners[MAX_PLAYERS];
u8 g_input_pad_for_player[MAX_PLAYERS];
u8 g_multitap_present;

void input_update_snapshot(PadSnapshot *snapshot, u16 current)
{
    u16 previous = snapshot->held;
    snapshot->held = current;
    snapshot->pressed = current & (u16)~previous;
    snapshot->released = previous & (u16)~current;
}

void input_configure(u8 multitap_present)
{
    u8 i;

    g_multitap_present = multitap_present;
    for (i = 0; i < MAX_PADS; ++i) {
        g_pad_snapshots[i].held = 0;
        g_pad_snapshots[i].pressed = 0;
        g_pad_snapshots[i].released = 0;
    }
    g_input_owners[0] = INPUT_OWNER_HUMAN;
    g_input_owners[1] = INPUT_OWNER_NONE;
    g_input_owners[2] = INPUT_OWNER_CPU;
    g_input_owners[3] = INPUT_OWNER_NONE;
    g_input_pad_for_player[0] = 0;
    g_input_pad_for_player[2] = 1;
    g_input_pad_for_player[1] = multitap_present ? 2 : INPUT_NO_PAD;
    g_input_pad_for_player[3] = multitap_present ? 3 : INPUT_NO_PAD;
}

static void build_intent(PlayerIntent *intent, const PadSnapshot *pad,
    u8 may_pause)
{
    u16 held = pad->held;

    intent->move_x = 0;
    intent->move_y = 0;
    if (held & PAD_LEFT)
        intent->move_x = -1;
    else if (held & PAD_RIGHT)
        intent->move_x = 1;
    if (held & PAD_UP)
        intent->move_y = -1;
    else if (held & PAD_DOWN)
        intent->move_y = 1;
    intent->aim_x = intent->move_x;
    intent->aim_y = intent->move_y;
    intent->hit_pressed = (pad->pressed & PAD_B) != 0;
    /* Pause is arbitrated centrally; only P1 pauses in the POC. */
    intent->pause_pressed = may_pause && (pad->pressed & PAD_START) != 0;
}

void input_map_frame(const u16 raw[MAX_PADS])
{
    u8 i;
    u8 pad;
    PlayerIntent *intent;

    for (i = 0; i < MAX_PADS; ++i)
        input_update_snapshot(&g_pad_snapshots[i], raw[i]);
    intent = g_player_intents;
    for (i = 0; i < MAX_PLAYERS; ++i, ++intent) {
        pad = g_input_pad_for_player[i];
        if (g_input_owners[i] == INPUT_OWNER_HUMAN && pad != INPUT_NO_PAD) {
            build_intent(intent, &g_pad_snapshots[pad], i == 0);
        } else {
            intent->move_x = 0;
            intent->move_y = 0;
            intent->aim_x = 0;
            intent->aim_y = 0;
            intent->hit_pressed = 0;
            intent->pause_pressed = 0;
        }
    }
}
