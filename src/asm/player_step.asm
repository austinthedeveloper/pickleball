.include "hdr.asm"
.include "assets/generated/asm_shared.inc"

; void player_step(void)
;
; One player's frame in assembly (the doubles frame budget, M4-UI-03/04):
; the same result as the C player_step in src/player.c, which the host tests
; run. Inputs are C globals: ps_player and ps_intent hold the addresses of
; the Player and its PlayerIntent in bank $7E. The data bank is set to $7E
; so the fields are read with absolute,X (player) and absolute,Y (intent);
; the working variables (ps_*, src/player.c) are long, as are the paddle
; and diagonal tables in ROM. The golden trace checks it against the C.
;
; Positions, velocities, limits, and clamps are all far below 16,384 in
; magnitude, so a signed compare is a subtraction and a sign test.

.SECTION ".player_step_text" SUPERFREE

; Velocity on one axis: accelerate toward the move, or slow by friction to
; a stop; then clamp to +-ps_limit.
.MACRO PS_VELOCITY ARGS MOVE, VEL
    lda.l MOVE
    beq _psv_friction\@
    bmi _psv_minus\@
    lda.w VEL,x
    clc
    adc.l ps_accel
    bra _psv_clamp\@
_psv_minus\@:
    lda.w VEL,x
    sec
    sbc.l ps_accel
    bra _psv_clamp\@
_psv_friction\@:
    lda.w VEL,x
    bmi _psv_negative\@
    cmp.w #ASM_FRICTION + 1
    bcc _psv_zero\@
    sbc.w #ASM_FRICTION         ; carry set by the compare
    bra _psv_clamp\@
_psv_negative\@:
    cmp.w #$10000 - ASM_FRICTION
    bcs _psv_zero\@             ; -6..-1 stop
    adc.w #ASM_FRICTION         ; carry clear by the compare
    bra _psv_clamp\@
_psv_zero\@:
    lda.w #0
_psv_clamp\@:
    sta.l ps_v
    sec
    sbc.l ps_limit
    bmi _psv_low\@
    beq _psv_low\@
    lda.l ps_limit              ; above the limit
    bra _psv_store\@
_psv_low\@:
    lda.l ps_v
    clc
    adc.l ps_limit
    bpl _psv_keep\@
    lda.w #0                    ; below -limit
    sec
    sbc.l ps_limit
    bra _psv_store\@
_psv_keep\@:
    lda.l ps_v
_psv_store\@:
    sta.w VEL,x
.ENDM

; Position on one axis: add the velocity, then clamp to LOW..HIGH; a clamp
; stops the player on that axis.
.MACRO PS_CLAMP ARGS POS, VEL, LOW, HIGH
    lda.w POS,x
    clc
    adc.w VEL,x
    sta.l ps_v
    sec
    sbc.w #LOW
    bpl _psc_high\@
    lda.w #LOW
    bra _psc_stop\@
_psc_high\@:
    lda.w #HIGH
    sec
    sbc.l ps_v
    bpl _psc_keep\@
    lda.w #HIGH
_psc_stop\@:
    sta.w POS,x
    lda.w #0
    sta.w VEL,x
    bra _psc_done\@
_psc_keep\@:
    lda.l ps_v
    sta.w POS,x
_psc_done\@:
.ENDM

; The animation clock: the next cel every CEL frames, COUNT cels in a loop.
.MACRO PS_TICK ARGS CEL, COUNT
    sep #$20
    lda.w ASM_PLAYER_ANIM_TIMER,x
    inc a
    cmp.b #CEL
    bcc _pst_keep\@
    lda.b #0
    sta.w ASM_PLAYER_ANIM_TIMER,x
    lda.w ASM_PLAYER_ANIM_FRAME,x
    inc a
    cmp.b #COUNT
    bcc _pst_frame\@
    lda.b #0
_pst_frame\@:
    sta.w ASM_PLAYER_ANIM_FRAME,x
    bra _pst_done\@
_pst_keep\@:
    sta.w ASM_PLAYER_ANIM_TIMER,x
_pst_done\@:
    rep #$20
.ENDM

player_step:
    php
    phb
    rep #$30
    pea $7E7E
    plb
    plb
    lda.l ps_player
    tax
    lda.l ps_intent
    tay

    ; The hit flash counts down.
    sep #$20
    lda.w ASM_PLAYER_FLASH,x
    beq _ps_flash_done
    dec a
    sta.w ASM_PLAYER_FLASH,x
_ps_flash_done:
    rep #$20

    ; The intent, or none while frozen. ps_hit: hit (low), shot (high).
    lda.w ASM_PLAYER_FROZEN,x
    and.w #$00FF
    beq _ps_input
    lda.w #0
    sta.l ps_mx
    sta.l ps_my
    sta.l ps_hit
    bra _ps_input_done
_ps_input:
    lda.w ASM_INTENT_MOVE_X,y
    sta.l ps_mx
    lda.w ASM_INTENT_MOVE_Y,y
    sta.l ps_my
    lda.w ASM_INTENT_HIT,y
    sta.l ps_hit
_ps_input_done:

    ; Was the player moving (for the animation reset)?
    lda.w ASM_PLAYER_STATE,x
    and.w #$00FF
    sta.l ps_state
    ldy.w #0
    cmp.w #ASM_STATE_MOVE
    bne _ps_was_set
    iny
_ps_was_set:
    tya
    sta.l ps_was

    ; The swing runs out its frames; otherwise a hit starts one, or the
    ; player moves or idles. State and its frame count are one word.
    lda.l ps_state
    cmp.w #ASM_STATE_WINDUP
    bcc _ps_not_swinging
    sep #$20
    dec.w ASM_PLAYER_STATE_FRAMES,x
    rep #$20
    bne _ps_state_done
    lda.l ps_state
    cmp.w #ASM_STATE_WINDUP
    bne _ps_after_active
    lda.w #ASM_STATE_ACTIVE | (ASM_ACTIVE_FRAMES << 8)
    sta.w ASM_PLAYER_STATE,x
    bra _ps_state_done
_ps_after_active:
    cmp.w #ASM_STATE_ACTIVE
    bne _ps_after_recovery
    lda.w ASM_PLAYER_PADDLE,x
    and.w #$00FF
    phx
    tax
    lda.l g_paddle_recovery,x
    plx
    xba
    and.w #$FF00
    ora.w #ASM_STATE_RECOVERY
    sta.w ASM_PLAYER_STATE,x
    bra _ps_state_done
_ps_after_recovery:
    sep #$20
    lda.b #0
    sta.w ASM_PLAYER_STATE,x
    rep #$20
    bra _ps_state_done
_ps_not_swinging:
    lda.l ps_hit
    and.w #$00FF
    beq _ps_walk
    lda.w #ASM_STATE_WINDUP | (ASM_WINDUP_FRAMES << 8)
    sta.w ASM_PLAYER_STATE,x
    sep #$20
    lda.b #0
    sta.w ASM_PLAYER_CONTACT,x
    lda.l ps_hit + 1
    sta.w ASM_PLAYER_SHOT_TYPE,x
    rep #$20
    bra _ps_state_done
_ps_walk:
    lda.l ps_mx
    ora.l ps_my
    beq _ps_walk_set
    lda.w #ASM_STATE_MOVE
_ps_walk_set:
    sep #$20
    sta.w ASM_PLAYER_STATE,x
    rep #$20
_ps_state_done:

    ; Speed limit: the paddle's while swinging, else the player's; on a
    ; diagonal, a slower push and the 181/256 limit.
    lda.w ASM_PLAYER_STATE,x
    and.w #$00FF
    cmp.w #ASM_STATE_WINDUP
    bcc _ps_run_limit
    lda.w ASM_PLAYER_PADDLE,x
    and.w #$00FF
    phx
    tax
    lda.l g_paddle_swing_limit,x
    plx
    and.w #$00FF
    bra _ps_limit_set
_ps_run_limit:
    lda.w ASM_PLAYER_MAX_SPEED,x
_ps_limit_set:
    sta.l ps_limit
    lda.w #ASM_ACCEL
    sta.l ps_accel
    lda.l ps_mx
    beq _ps_accel_done
    lda.l ps_my
    beq _ps_accel_done
    lda.w #ASM_DIAGONAL_ACCEL
    sta.l ps_accel
    lda.l ps_limit
    and.w #31
    phx
    tax
    lda.l g_player_diagonal_limit,x
    plx
    and.w #$00FF
    sta.l ps_limit
_ps_accel_done:

    PS_VELOCITY ps_mx, ASM_PLAYER_VX
    PS_VELOCITY ps_my, ASM_PLAYER_VY

    ; Court clamps by side, then the y clamp.
    lda.w ASM_PLAYER_TEAM,x
    and.w #$00FF
    bne _ps_right
    PS_CLAMP ASM_PLAYER_X, ASM_PLAYER_VX, ASM_CLAMP_MIN_X_FP, ASM_CLAMP_MAX_X_FP
    bra _ps_clamp_y
_ps_right:
    PS_CLAMP ASM_PLAYER_X, ASM_PLAYER_VX, ASM_RIGHT_CLAMP_MIN_X_FP, ASM_RIGHT_CLAMP_MAX_X_FP
_ps_clamp_y:
    PS_CLAMP ASM_PLAYER_Y, ASM_PLAYER_VY, ASM_CLAMP_MIN_Y_FP, ASM_CLAMP_MAX_Y_FP

    ; Players always square up to the net.
    sep #$20
    lda.w ASM_PLAYER_TEAM,x
    sta.w ASM_PLAYER_FACING_LEFT,x
    rep #$20

    ; Animation: starting or stopping a run restarts it (frame and timer
    ; are one word).
    lda.w ASM_PLAYER_STATE,x
    and.w #$00FF
    cmp.w #ASM_STATE_MOVE
    beq _ps_moving
    lda.l ps_was
    beq _ps_idle_tick
    stz.w ASM_PLAYER_ANIM_FRAME,x
_ps_idle_tick:
    PS_TICK ASM_IDLE_CEL_FRAMES, 2
    bra _ps_done
_ps_moving:
    lda.l ps_was
    bne _ps_run_tick
    stz.w ASM_PLAYER_ANIM_FRAME,x
_ps_run_tick:
    PS_TICK ASM_RUN_CEL_FRAMES, 4
_ps_done:
    plb
    plp
    rtl

.ENDS
