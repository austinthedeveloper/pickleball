#ifndef COLLISION_H
#define COLLISION_H

#include "ball.h"
#include "court.h"
#include "player.h"

enum {
    HIT_REACH_X = 22,
    HIT_REACH_Y = 16,
    HIT_REACH_Z = 24,
    HIT_TARGET_LANE_TOP = COURT_SIDELINE_TOP + 14,
    HIT_TARGET_LANE_CENTER = COURT_CENTERLINE_Y,
    HIT_TARGET_LANE_BOTTOM = COURT_SIDELINE_BOTTOM - 14,
    HIT_TARGET_SHORT_RIGHT = COURT_KITCHEN_RIGHT_X + 12,
    HIT_TARGET_NEUTRAL_RIGHT = COURT_KITCHEN_RIGHT_X + 40,
    HIT_TARGET_DEEP_RIGHT = COURT_BASELINE_RIGHT - 12,
    HIT_TARGET_SHORT_LEFT = COURT_KITCHEN_LEFT_X - 12,
    HIT_TARGET_NEUTRAL_LEFT = COURT_KITCHEN_LEFT_X - 40,
    HIT_TARGET_DEEP_LEFT = COURT_BASELINE_LEFT + 12,
    HIT_FLIGHT_FRAMES_DEEP = 32,
    HIT_FLIGHT_FRAMES_NEUTRAL = 34,
    HIT_FLIGHT_FRAMES_SHORT = 38
};

u8 collision_ball_in_player_reach(const Player *player, const Ball *ball);
/* Returns the contacting player index plus one, or zero for no contact. */
u8 collision_resolve_player_hits(Ball *ball, Player players[MAX_PLAYERS],
    const PlayerIntent intents[MAX_PLAYERS], u8 player_count);

#endif
