#include "court.h"

s32 court_clamp_x_fp(s32 x_fp)
{
    if (x_fp < ((s32)PLAYER_CLAMP_MIN_X << 8))
        return ((s32)PLAYER_CLAMP_MIN_X << 8);
    if (x_fp > ((s32)PLAYER_CLAMP_MAX_X << 8))
        return ((s32)PLAYER_CLAMP_MAX_X << 8);
    return x_fp;
}

s32 court_clamp_y_fp(s32 y_fp)
{
    if (y_fp < ((s32)PLAYER_CLAMP_MIN_Y << 8))
        return ((s32)PLAYER_CLAMP_MIN_Y << 8);
    if (y_fp > ((s32)PLAYER_CLAMP_MAX_Y << 8))
        return ((s32)PLAYER_CLAMP_MAX_Y << 8);
    return y_fp;
}

u8 court_contains_ground_fp(s32 x_fp, s32 y_fp)
{
    if (x_fp < ((s32)COURT_BASELINE_LEFT << 8))
        return 0;
    if (x_fp > ((s32)COURT_BASELINE_RIGHT << 8))
        return 0;
    if (y_fp < ((s32)COURT_SIDELINE_TOP << 8))
        return 0;
    if (y_fp > ((s32)COURT_SIDELINE_BOTTOM << 8))
        return 0;
    return 1;
}

