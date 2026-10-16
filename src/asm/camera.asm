.include "hdr.asm"
.include "assets/generated/asm_shared.inc"

; void render_camera(void)
;
; One frame of the camera (ADR-050), the C rule in src/render.c in
; assembly (816-tcc's version took about 8 scanlines a frame):
;   * target: with the ball in play and g_cam_k (the CAMERA setting in
;     1/128ths) nonzero, |d| times k / 128 with d the ball's distance from
;     the net in px (to 128: 8 px at 100%), and from the centre line in
;     1/8 px (to 32 px: 4 px), in 1/16 px, signed as d; else 0;
;   * g_cam_fx/fy ease an eighth of the way to it a frame (at least 1/16
;     px, at most half a pixel);
;   * oam_cam_x/y are those rounded to whole pixels; g_cam_dirty is set
;     when they change (render_commit scrolls BG1 in VBlank).
; Long addressing only; the hardware multiplier ($4202/$4203/$4216).

.SECTION ".camera_text" SUPERFREE

; Eases AT toward TARGET (both 1/16 px words).
.MACRO RC_EASE ARGS AT, TARGET
    lda.l TARGET
    sec
    sbc.l AT
    beq _rce_done\@
    bmi _rce_neg\@
    cmp.w #64
    bcc _rce_small\@
    lda.w #8
    bra _rce_add\@
_rce_small\@:
    clc
    adc.w #7
    lsr a
    lsr a
    lsr a
    bra _rce_add\@
_rce_neg\@:
    cmp.w #$FFC1
    bcs _rce_nsmall\@
    lda.w #$FFF8
    bra _rce_add\@
_rce_nsmall\@:
    eor.w #$FFFF
    inc a
    clc
    adc.w #7
    lsr a
    lsr a
    lsr a
    eor.w #$FFFF
    inc a
_rce_add\@:
    clc
    adc.l AT
    sta.l AT
_rce_done\@:
.ENDM

; (AT + 8) >> 4, arithmetic, into OUT; sets g_cam_dirty when it changes.
.MACRO RC_ROUND ARGS AT, OUT
    lda.l AT
    clc
    adc.w #8
    cmp.w #$8000
    ror a
    cmp.w #$8000
    ror a
    cmp.w #$8000
    ror a
    cmp.w #$8000
    ror a
    cmp.l OUT
    beq _rcr_same\@
    sta.l OUT
    sep #$20
    lda.b #1
    sta.l g_cam_dirty
    rep #$20
_rcr_same\@:
.ENDM

; A: a signed distance; Y: right shifts to bring |A| to 0..128. Returns
; A = sign * ((|A| >> Y) * g_cam_k >> 7).
_rc_lean:
    sta.l g_cam_sign
    ora.w #0                    ; the sign flag (the caller's ldy set it)
    bpl +
    eor.w #$FFFF
    inc a
+
-   lsr a
    dey
    bne -
    sep #$20
    sta.l $4202
    lda.l g_cam_k
    sta.l $4203
    rep #$20
    nop
    nop
    nop
    lda.l $4216
    asl a
    xba
    and.w #$00FF
    sta.l g_cam_mag
    lda.l g_cam_sign
    bpl +
    lda.l g_cam_mag
    eor.w #$FFFF
    inc a
    rts
+   lda.l g_cam_mag
    rts

render_camera:
    php
    rep #$30
    lda.w #0
    sta.l g_cam_tx
    sta.l g_cam_ty
    lda.l g_cam_k
    and.w #$00FF
    beq _rc_ease
    lda.l g_ball + ASM_BALL_ACTIVE
    and.w #$00FF
    beq _rc_ease

    ; Across: x - net, clamped to +-128 px, then to whole px.
    lda.l g_ball + ASM_BALL_X
    sec
    sbc.w #ASM_CAM_CENTER_X_FP
    bmi _rc_xneg
    cmp.w #ASM_CAM_REACH_X_FP
    bcc _rc_xok
    lda.w #ASM_CAM_REACH_X_FP
    bra _rc_xok
_rc_xneg:
    cmp.w #$10000 - ASM_CAM_REACH_X_FP
    bcs _rc_xok
    lda.w #$10000 - ASM_CAM_REACH_X_FP
_rc_xok:
    ldy.w #4
    jsr _rc_lean
    sta.l g_cam_tx

    ; Along: y - centre line, clamped to +-32 px, in 1/8 px.
    lda.l g_ball + ASM_BALL_Y
    sec
    sbc.w #ASM_CAM_CENTER_Y_FP
    bmi _rc_yneg
    cmp.w #ASM_CAM_REACH_Y_FP
    bcc _rc_yok
    lda.w #ASM_CAM_REACH_Y_FP
    bra _rc_yok
_rc_yneg:
    cmp.w #$10000 - ASM_CAM_REACH_Y_FP
    bcs _rc_yok
    lda.w #$10000 - ASM_CAM_REACH_Y_FP
_rc_yok:
    ldy.w #3
    jsr _rc_lean
    sta.l g_cam_ty

_rc_ease:
    RC_EASE g_cam_fx, g_cam_tx
    RC_EASE g_cam_fy, g_cam_ty
    RC_ROUND g_cam_fx, oam_cam_x
    RC_ROUND g_cam_fy, oam_cam_y
    plp
    rtl

.ENDS
