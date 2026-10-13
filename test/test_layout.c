#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include "asm_shared.h"
#include "court.h"
#include "court_projection.h"
#include "player.h"
#include "render.h"

/* Every number the assembly uses must match the C definitions. */
int main(void)
{
    assert(sizeof(Player) == ASM_PLAYER_SIZE);
    assert(offsetof(Player, active) == ASM_PLAYER_ACTIVE);
    assert(offsetof(Player, x_fp) == ASM_PLAYER_X);
    assert(offsetof(Player, y_fp) == ASM_PLAYER_Y);
    assert(offsetof(Player, facing_left) == ASM_PLAYER_FACING_LEFT);
    assert(offsetof(Player, animation_frame) == ASM_PLAYER_ANIM_FRAME);
    assert(offsetof(Player, state) == ASM_PLAYER_STATE);
    assert(offsetof(Player, pose) == ASM_PLAYER_POSE);
    assert(offsetof(Player, palette) == ASM_PLAYER_PALETTE);
    assert(offsetof(Player, flash_timer) == ASM_PLAYER_FLASH);
    assert(PLAYER_STATE_MOVE == ASM_STATE_MOVE);
    assert(PLAYER_STATE_WINDUP == ASM_STATE_WINDUP);
    assert(PLAYER_STATE_ACTIVE == ASM_STATE_ACTIVE);
    assert(PLAYER_STATE_RECOVERY == ASM_STATE_RECOVERY);
    assert(PLAYER_POSE_TOSS == ASM_POSE_TOSS);
    assert(PLAYER_POSE_CHEER == ASM_POSE_CHEER);
    assert(PLAYER_SPRITE_HALF_WIDTH == ASM_SPRITE_HALF_WIDTH);
    assert(PLAYER_SPRITE_FOOT_Y == ASM_SPRITE_FOOT_Y);
    assert(RENDER_OAM_BODIES == ASM_OAM_BODIES);
    assert(RENDER_OAM_SHADOWS == ASM_OAM_SHADOWS);
    assert(COURT_PROJECTION_ROWS == ASM_PROJECTION_ROWS);
    puts("layout host checks passed");
    return 0;
}
