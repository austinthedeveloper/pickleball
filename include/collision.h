#ifndef COLLISION_H
#define COLLISION_H

#include "ball.h"
#include "court.h"
#include "player.h"

enum {
    HIT_REACH_X = 22,
    HIT_REACH_Y = 16,
    HIT_REACH_Z = 24,
    /* Contacts inside the sweet box go where aimed; stretched contacts at
     * the edge of reach spray wide and long by HIT_STRETCH_* pixels. */
    HIT_SWEET_X = 14,
    HIT_SWEET_Y = 10,
    HIT_STRETCH_WIDE = 20,
    HIT_STRETCH_LONG = 16,
    HIT_TARGET_LANE_TOP = COURT_SIDELINE_TOP + 14,
    HIT_TARGET_LANE_CENTER = COURT_CENTERLINE_Y,
    HIT_TARGET_LANE_BOTTOM = COURT_SIDELINE_BOTTOM - 14,
    HIT_TARGET_SHORT_RIGHT = COURT_KITCHEN_RIGHT_X + 12,
    HIT_TARGET_NEUTRAL_RIGHT = COURT_KITCHEN_RIGHT_X + 40,
    HIT_TARGET_DEEP_RIGHT = COURT_BASELINE_RIGHT - 12,
    HIT_TARGET_SHORT_LEFT = COURT_KITCHEN_LEFT_X - 12,
    HIT_TARGET_NEUTRAL_LEFT = COURT_KITCHEN_LEFT_X - 40,
    HIT_TARGET_DEEP_LEFT = COURT_BASELINE_LEFT + 12,
    /* Horizontal pace in 12.4 units: 3.0 / 2.75 / 2.5 px per frame. Flight
     * time is distance / pace, clamped so lobs stay below the HUD (apex
     * ~50 px) and inside the AI's 60-step prediction horizon. */
    HIT_PACE_DEEP_FP = 48,
    HIT_PACE_NEUTRAL_FP = 44,
    HIT_PACE_SHORT_FP = 40,
    HIT_FLIGHT_MIN_FRAMES = 24,
    HIT_FLIGHT_MAX_FRAMES = 46
};

u8 collision_ball_in_player_reach(const Player *player, const Ball *ball);
u8 collision_ball_in_sweet_spot(const Player *player, const Ball *ball);
/* Returns the contacting player index plus one, or zero for no contact. */
u8 collision_resolve_player_hits(Ball *ball, Player players[MAX_PLAYERS],
    const PlayerIntent intents[MAX_PLAYERS], u8 player_count);

#endif
