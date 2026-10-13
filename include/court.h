#ifndef COURT_H
#define COURT_H

#include "game_types.h"

/*
 * World coordinates are whole ground pixels: y is the screen row of the
 * ground point and x is the column at the near (bottom) sideline. Rendering
 * narrows x toward the far sideline with k(y) = COURT_PERSPECTIVE_BASE + y/2
 * (see render_project_x); physics, rules, and AI never see the projection.
 * tools/art/prep_court_scene.py paints the court from these same values.
 */
enum {
    COURT_BASELINE_LEFT = 16,
    COURT_BASELINE_RIGHT = 240,
    COURT_SIDELINE_TOP = 90,
    COURT_SIDELINE_BOTTOM = 170,
    COURT_NET_X = 128,
    COURT_KITCHEN_LEFT_X = 92,
    COURT_KITCHEN_RIGHT_X = 164,
    COURT_CENTERLINE_Y = 130,
    COURT_NET_HEIGHT = 12,
    COURT_PERSPECTIVE_BASE = 171,

    /* Player x/y is the ground point between the feet. */
    PLAYER_SPRITE_HALF_WIDTH = 16,
    PLAYER_SPRITE_FOOT_Y = 30,
    PLAYER_RUNOFF_BACK = 12,
    PLAYER_RUNOFF_SIDE = 10,
    PLAYER_NET_GAP = 8,

    PLAYER_SPAWN_X = 40,
    PLAYER_SPAWN_Y = 150,
    PLAYER_CLAMP_MIN_X = COURT_BASELINE_LEFT - PLAYER_RUNOFF_BACK,
    PLAYER_CLAMP_MAX_X = COURT_NET_X - PLAYER_NET_GAP,
    PLAYER_CLAMP_MIN_Y = COURT_SIDELINE_TOP - PLAYER_RUNOFF_SIDE + 4,
    PLAYER_CLAMP_MAX_Y = COURT_SIDELINE_BOTTOM + PLAYER_RUNOFF_SIDE,
    PLAYER_RIGHT_CLAMP_MIN_X = COURT_NET_X + PLAYER_NET_GAP,
    PLAYER_RIGHT_CLAMP_MAX_X = COURT_BASELINE_RIGHT + PLAYER_RUNOFF_BACK
};

/* Clamp a ground position to the legal half for the given team (0 = left). */
fx court_clamp_x_fp(fx x_fp, u8 team);
fx court_clamp_y_fp(fx y_fp);
u8 court_contains_ground_fp(fx x_fp, fx y_fp);

#endif
