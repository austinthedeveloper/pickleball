.include "hdr.asm"
.include "assets/generated/asm_shared.inc"

; void render_bodies(void)
;
; Player bodies and shadows in assembly (doubles budget, ADR-018). Same
; result as the former C render_players():
;   * bodies take OAM ASM_OAM_BODIES.. nearest-first (larger y first, ties
;     by slot); shadows take ASM_OAM_SHADOWS + slot (ADR-012);
;   * screen x = 128 + d + ((d * (s(y) - 256) + 128) >> 8), d = x - 128,
;     using the hardware 8x8 multiplier (ADR-011, src/projection.c);
;   * frame from pose/state/animation, palette from flash or the player;
;   * female players add a ponytail at ASM_OAM_ACCESSORIES + slot (ADR-014),
;     offset per frame and mirrored with the body.
; Outputs rb_count = number of active bodies drawn. Working variables are
; C globals in src/render.c (rb_*); every access is long, so the data bank
; and direct page are never assumed. Calls oam_write for each object.

.SECTION ".render_bodies_text" SUPERFREE

_rb_slot_offset:
    .dw 0, ASM_PLAYER_SIZE, ASM_PLAYER_SIZE * 2, ASM_PLAYER_SIZE * 3
_rb_idle:
    .dw FRAME_IDLE0_SLOT, FRAME_IDLE1_SLOT
_rb_run:
    .dw FRAME_RUN0_SLOT, FRAME_RUN1_SLOT, FRAME_RUN2_SLOT, FRAME_RUN3_SLOT
; Indexed by frame slot (assets/manifest/player_frames.json order).
_rb_tile:
    .dw FRAME_IDLE0, FRAME_IDLE1, FRAME_RUN0, FRAME_RUN1, FRAME_RUN2
    .dw FRAME_RUN3, FRAME_WINDUP, FRAME_SWING, FRAME_FOLLOW, FRAME_TOSS
    .dw FRAME_CHEER
_rb_acc_x:
    .dw FRAME_IDLE0_ACC_X, FRAME_IDLE1_ACC_X, FRAME_RUN0_ACC_X
    .dw FRAME_RUN1_ACC_X, FRAME_RUN2_ACC_X, FRAME_RUN3_ACC_X
    .dw FRAME_WINDUP_ACC_X, FRAME_SWING_ACC_X, FRAME_FOLLOW_ACC_X
    .dw FRAME_TOSS_ACC_X, FRAME_CHEER_ACC_X
_rb_acc_y:
    .dw FRAME_IDLE0_ACC_Y, FRAME_IDLE1_ACC_Y, FRAME_RUN0_ACC_Y
    .dw FRAME_RUN1_ACC_Y, FRAME_RUN2_ACC_Y, FRAME_RUN3_ACC_Y
    .dw FRAME_WINDUP_ACC_Y, FRAME_SWING_ACC_Y, FRAME_FOLLOW_ACC_Y
    .dw FRAME_TOSS_ACC_Y, FRAME_CHEER_ACC_Y

; Depth key for one slot: y_fp when active, $8001 (sorts last) otherwise.
.MACRO RB_DEPTH ARGS SLOT
    lda.l g_players + SLOT * ASM_PLAYER_SIZE + ASM_PLAYER_ACTIVE
    and.w #$00FF
    beq _rbd_off\@
    lda.l g_players + SLOT * ASM_PLAYER_SIZE + ASM_PLAYER_Y
    bra _rbd_set\@
_rbd_off\@:
    lda.w #$8001
_rbd_set\@:
    sta.l rb_depth + SLOT * 2
    lda.w #SLOT
    sta.l rb_order + SLOT * 2
.ENDM

; A = A >> 4, arithmetic (12.4 fixed point to whole pixels).
.MACRO RB_SAR4
    cmp.w #$8000
    ror a
    cmp.w #$8000
    ror a
    cmp.w #$8000
    ror a
    cmp.w #$8000
    ror a
.ENDM

render_bodies:
    php
    rep #$30

    RB_DEPTH 0
    RB_DEPTH 1
    RB_DEPTH 2
    RB_DEPTH 3

    ; Insertion sort of rb_order (word slots) by rb_depth, larger first.
    lda.w #2
    sta.l rb_i
_rb_outer:
    lda.l rb_i
    tax
    lda.l rb_order,x
    sta.l rb_key
    asl a
    phx
    tax
    lda.l rb_depth,x
    sta.l rb_kd
    plx
_rb_inner:
    cpx.w #0
    beq _rb_place
    lda.l rb_order - 2,x
    sta.l rb_tmp
    asl a
    phx
    tax
    lda.l rb_depth,x
    plx
    ; Move prev down only while depth[key] > depth[prev] (signed).
    sec
    sbc.l rb_kd
    bvc +
    eor.w #$8000
+   bpl _rb_place
    lda.l rb_tmp
    sta.l rb_order,x
    dex
    dex
    bra _rb_inner
_rb_place:
    lda.l rb_key
    sta.l rb_order,x
    lda.l rb_i
    clc
    adc.w #2
    sta.l rb_i
    cmp.w #8
    bne _rb_outer

    ; One body + shadow per active slot, nearest first.
    lda.w #0
    sta.l rb_i
_rb_body:
    lda.l rb_i
    asl a
    tax
    lda.l rb_order,x
    sta.l rb_slot
    asl a
    tax
    lda.l rb_depth,x
    cmp.w #$8001
    bne +
    brl _rb_done
+   lda.l _rb_slot_offset,x
    tax

    lda.l g_players + ASM_PLAYER_Y,x
    RB_SAR4
    sta.l rb_y
    lda.l g_players + ASM_PLAYER_X,x
    RB_SAR4
    sec
    sbc.w #128
    sta.l rb_d

    ; --- projection: |d| * |delta| on the hardware multiplier ---
    phx
    lda.w #0
    sta.l rb_sign
    lda.l rb_y
    bpl +
    lda.w #0
+   cmp.w #ASM_PROJECTION_ROWS
    bcc +
    lda.w #ASM_PROJECTION_ROWS - 1
+   tax
    sep #$20
    lda.l g_court_projection_delta,x
    bpl +
    eor.b #$FF
    inc a
    sta.l rb_tmp
    lda.b #1
    sta.l rb_sign
    lda.l rb_tmp
+   sta.l $004202
    rep #$20
    lda.l rb_d
    bpl +
    eor.w #$FFFF
    inc a
    sta.l rb_tmp
    lda.l rb_sign
    eor.w #1
    sta.l rb_sign
    lda.l rb_tmp
+   sep #$20
    sta.l $004203
    rep #$20
    nop
    nop
    nop
    nop
    lda.l $004216
    sta.l rb_prod
    lda.l rb_sign
    and.w #$00FF
    beq +
    lda.l rb_prod
    eor.w #$FFFF
    inc a
    sta.l rb_prod
+   lda.l rb_prod
    clc
    adc.w #128
    xba                     ; arithmetic >> 8: sign-extended high byte
    and.w #$00FF
    cmp.w #$0080
    bcc +
    ora.w #$FF00
+   clc
    adc.l rb_d
    clc
    adc.w #128
    sta.l rb_sx
    plx

    ; --- frame tile ---
    lda.l g_players + ASM_PLAYER_POSE,x
    and.w #$00FF
    cmp.w #ASM_POSE_CHEER
    bne +
    lda.w #FRAME_CHEER_SLOT
    bra _rb_have_tile
+   cmp.w #ASM_POSE_TOSS
    bne +
    lda.w #FRAME_TOSS_SLOT
    bra _rb_have_tile
+   lda.l g_players + ASM_PLAYER_STATE,x
    and.w #$00FF
    cmp.w #ASM_STATE_WINDUP
    bne +
    lda.w #FRAME_WINDUP_SLOT
    bra _rb_have_tile
+   cmp.w #ASM_STATE_ACTIVE
    bne +
    lda.w #FRAME_SWING_SLOT
    bra _rb_have_tile
+   cmp.w #ASM_STATE_RECOVERY
    bne +
    lda.w #FRAME_FOLLOW_SLOT
    bra _rb_have_tile
+   cmp.w #ASM_STATE_MOVE
    bne _rb_idle_frame
    lda.l g_players + ASM_PLAYER_ANIM_FRAME,x
    and.w #3
    asl a
    phx
    tax
    lda.l _rb_run,x
    plx
    bra _rb_have_tile
_rb_idle_frame:
    lda.l g_players + ASM_PLAYER_ANIM_FRAME,x
    and.w #1
    asl a
    phx
    tax
    lda.l _rb_idle,x
    plx
_rb_have_tile:
    sta.l rb_frame
    asl a
    phx
    tax
    lda.l _rb_tile,x
    sta.l oam_in_tile
    plx

    ; --- attributes: flash or player palette, priority, facing ---
    lda.l g_players + ASM_PLAYER_FLASH,x
    and.w #$00FF
    beq +
    lda.l g_objpal_flash
    bra ++
+   lda.l g_players + ASM_PLAYER_PALETTE,x
++  and.w #$00FF
    asl a
    ora.w #ASM_OBJ_PRIORITY_ATTR
    sta.l rb_attr
    lda.l g_players + ASM_PLAYER_FACING_LEFT,x
    and.w #$00FF
    beq +
    lda.l rb_attr
    ora.w #$0040
    sta.l rb_attr
+
    ; --- body ---
    sep #$20
    lda.l rb_i
    clc
    adc.b #ASM_OAM_BODIES
    sta.l oam_in_id
    lda.l rb_attr
    sta.l oam_in_attr
    lda.b #1
    sta.l oam_in_large
    rep #$20
    lda.l rb_sx
    sec
    sbc.w #ASM_SPRITE_HALF_WIDTH
    sta.l oam_in_x
    lda.l rb_y
    sec
    sbc.w #ASM_SPRITE_FOOT_Y
    sta.l oam_in_y
    jsl oam_write

    ; --- shadow ---
    sep #$20
    lda.l rb_slot
    clc
    adc.b #ASM_OAM_SHADOWS
    sta.l oam_in_id
    lda.l g_objpal_shadow
    asl a
    ora.b #ASM_OBJ_PRIORITY_ATTR
    sta.l oam_in_attr
    lda.b #0
    sta.l oam_in_large
    rep #$20
    lda.l rb_sx
    sec
    sbc.w #4
    sta.l oam_in_x
    lda.l rb_y
    sec
    sbc.w #4
    sta.l oam_in_y
    lda.w #OBJTILE_PLAYER_SHADOW
    sta.l oam_in_tile
    jsl oam_write

    ; --- ponytail (female variant), same palette and flip as the body ---
    lda.l rb_slot
    asl a
    tax
    lda.l _rb_slot_offset,x
    tax
    lda.l g_players + ASM_PLAYER_VARIANT,x
    and.w #$00FF
    beq _rb_next
    lda.l rb_frame
    asl a
    tax
    lda.l _rb_acc_x,x
    sta.l rb_tmp
    lda.l rb_attr
    and.w #$0040
    beq +
    lda.w #24
    sec
    sbc.l rb_tmp
    sta.l rb_tmp
+   lda.l rb_sx
    sec
    sbc.w #ASM_SPRITE_HALF_WIDTH
    clc
    adc.l rb_tmp
    sta.l oam_in_x
    lda.l rb_y
    sec
    sbc.w #ASM_SPRITE_FOOT_Y
    clc
    adc.l _rb_acc_y,x
    sta.l oam_in_y
    lda.w #OBJTILE_PONYTAIL
    sta.l oam_in_tile
    sep #$20
    lda.l rb_slot
    clc
    adc.b #ASM_OAM_ACCESSORIES
    sta.l oam_in_id
    lda.l rb_attr
    sta.l oam_in_attr
    lda.b #0
    sta.l oam_in_large
    rep #$20
    jsl oam_write
_rb_next:

    lda.l rb_i
    inc a
    sta.l rb_i
    cmp.w #4
    beq _rb_done
    brl _rb_body
_rb_done:
    lda.l rb_i
    sta.l rb_count
    plp
    rtl

.ENDS
