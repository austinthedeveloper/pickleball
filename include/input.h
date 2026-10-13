#ifndef INPUT_H
#define INPUT_H

#include "game_types.h"

#define MAX_HUMAN_PADS 4
#define MAX_PLAYERS 4

typedef enum {
    INPUT_OWNER_NONE = 0,
    INPUT_OWNER_HUMAN = 1,
    INPUT_OWNER_CPU = 2
} InputOwner;

typedef struct {
    u16 held;
    u16 pressed;
    u16 released;
} PadSnapshot;

typedef struct {
    s16 move_x;
    s16 move_y;
    /* Shot direction read at contact; humans aim with the D-pad (= move). */
    s16 aim_x;
    s16 aim_y;
    u8 hit_pressed;
    u8 pause_pressed;
} PlayerIntent;

extern PadSnapshot g_pad_snapshots[MAX_HUMAN_PADS];
extern PlayerIntent g_player_intents[MAX_PLAYERS];
extern u8 g_input_owners[MAX_PLAYERS];

void input_initialize(void);
void input_update_snapshot(PadSnapshot *snapshot, u16 current);
void input_capture_frame(void);

#endif

