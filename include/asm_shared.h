#ifndef ASM_SHARED_H
#define ASM_SHARED_H

/*
 * Numbers shared with hand-written assembly (src/asm/). The asset build
 * turns every "#define NAME value" here into assets/generated/asm_shared.inc
 * for WLA-DX, and test/test_layout.c checks each against the C definitions
 * (offsetof, enums), so C and asm cannot drift apart.
 */

/* Player struct layout (include/player.h). */
#define ASM_PLAYER_SIZE 28
#define ASM_PLAYER_ACTIVE 0
#define ASM_PLAYER_X 4
#define ASM_PLAYER_Y 6
#define ASM_PLAYER_FACING_LEFT 14
#define ASM_PLAYER_ANIM_FRAME 15
#define ASM_PLAYER_STATE 17
#define ASM_PLAYER_POSE 21
#define ASM_PLAYER_PALETTE 22
#define ASM_PLAYER_FLASH 23
#define ASM_PLAYER_VARIANT 25

/* Player states and poses (include/player.h). */
#define ASM_STATE_MOVE 1
#define ASM_STATE_WINDUP 2
#define ASM_STATE_ACTIVE 3
#define ASM_STATE_RECOVERY 4
#define ASM_POSE_TOSS 1
#define ASM_POSE_CHEER 2

/* Sprite placement (include/court.h) and OAM slots (include/render.h). */
#define ASM_SPRITE_HALF_WIDTH 16
#define ASM_SPRITE_FOOT_Y 30
#define ASM_OAM_BODIES 2
#define ASM_OAM_SHADOWS 6
#define ASM_OBJ_PRIORITY_ATTR 0x20

/* Projection table rows (assets/generated/court_projection.h). */
#define ASM_PROJECTION_ROWS 224

#endif
