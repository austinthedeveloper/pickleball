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

/* A controller's pad position: 1P port 1, 2P port 2 (or the Multitap's
 * first pad, the same position), 3P and 4P the Multitap's next two. */
static u8 controller_pad(u8 controller)
{
    if (controller < 2)
        return controller;
    if (controller < MAX_HUMAN_PADS && g_multitap_present)
        return controller;
    return INPUT_NO_PAD;
}

void input_assign_match(const u8 active[MAX_PLAYERS],
    const u8 controllers[MAX_PLAYERS])
{
    u8 i;

    g_input_owners[0] = INPUT_OWNER_HUMAN;
    g_input_pad_for_player[0] = 0;
    for (i = 1; i < MAX_PLAYERS; ++i) {
        if (active[i] == 0) {
            g_input_owners[i] = INPUT_OWNER_NONE;
            g_input_pad_for_player[i] = INPUT_NO_PAD;
        } else if (controllers[i] == INPUT_CPU ||
                controller_pad(controllers[i]) == INPUT_NO_PAD) {
            g_input_owners[i] = INPUT_OWNER_CPU;
            g_input_pad_for_player[i] = INPUT_NO_PAD;
        } else {
            g_input_owners[i] = INPUT_OWNER_HUMAN;
            g_input_pad_for_player[i] = controller_pad(controllers[i]);
        }
    }
}

/* away: the aim_x that points away from this player's net (-1 on the
 * left team): a held L or R aims there, a short shot (ADR-056), and the
 * D-pad still moves and aims at a sideline. */
static void build_intent(PlayerIntent *intent, const PadSnapshot *pad,
    u8 may_pause, s16 away)
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
    intent->aim_x = (held & PAD_SHORT) ? away : intent->move_x;
    intent->aim_y = intent->move_y;
    intent->hit_pressed = (pad->pressed & PAD_SWING) != 0;
    intent->hit_held = (held & PAD_SWING) != 0;
    if (pad->pressed & PAD_A)
        intent->shot = SHOT_DRIVE;
    else if (pad->pressed & PAD_Y)
        intent->shot = SHOT_SOFT;
    else if (pad->pressed & PAD_X)
        intent->shot = SHOT_SLICE;
    else
        intent->shot = SHOT_STANDARD;
    /* Pause is arbitrated centrally; only P1 pauses in the POC. */
    intent->pause_pressed = may_pause && (pad->pressed & PAD_START) != 0;
}

/* Constant indices: 816-tcc multiplies for variable struct-array indexing,
 * and this runs every frame. */
#define SNAPSHOT(n) \
    do { \
        u16 now = raw[n]; \
        u16 before = g_pad_snapshots[n].held; \
        g_pad_snapshots[n].held = now; \
        g_pad_snapshots[n].pressed = now & (u16)~before; \
        g_pad_snapshots[n].released = before & (u16)~now; \
    } while (0)

#define MAP_PLAYER(i) \
    do { \
        if (g_input_owners[i] == INPUT_OWNER_HUMAN && \
                g_input_pad_for_player[i] != INPUT_NO_PAD) { \
            build_intent(&g_player_intents[i], \
                &g_pad_snapshots[g_input_pad_for_player[i]], (i) == 0, \
                (i) >= 2 ? 1 : -1); \
        } else { \
            g_player_intents[i].move_x = 0; \
            g_player_intents[i].move_y = 0; \
            g_player_intents[i].aim_x = 0; \
            g_player_intents[i].aim_y = 0; \
            g_player_intents[i].hit_pressed = 0; \
            g_player_intents[i].hit_held = 0; \
            g_player_intents[i].shot = SHOT_STANDARD; \
            g_player_intents[i].pause_pressed = 0; \
        } \
    } while (0)

void input_map_frame(const u16 raw[MAX_PADS])
{
    SNAPSHOT(0);
    SNAPSHOT(1);
    SNAPSHOT(2);
    SNAPSHOT(3);
    SNAPSHOT(4);
    MAP_PLAYER(0);
    MAP_PLAYER(1);
    MAP_PLAYER(2);
    MAP_PLAYER(3);
}
