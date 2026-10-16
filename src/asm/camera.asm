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
;
; Parallax (ADR-051): it then writes the back half of g_cam_hdma, an HDMA
; table for BG1's horizontal scroll (mode 2, $210D) that render_camera_commit
; hands to channel 6 in VBlank:
;   * the backdrop (scene rows 0-73, ASM_CAM_FAR_LINES lines less the
;     camera's y) at half the camera: g_cam_far_x;
;   * the court (ASM_CAM_COURT_LINES lines) at the camera;
;   * the near fence and bushes (from scene row 186) in 4 steps from the
;     camera to 1.5 times it, the last to the bottom: it leans toward you.
; The table is rebuilt only when the camera moved (g_cam_built_fx/fy);
; g_cam_new tells the commit to swap halves.

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

    ; --- the parallax table, into the back half, if the camera moved ---
    lda.l g_cam_fx
    cmp.l g_cam_built_fx
    bne +
    lda.l g_cam_fy
    cmp.l g_cam_built_fy
    bne +
    plp
    rtl
+   lda.l g_cam_fx
    sta.l g_cam_built_fx
    lda.l g_cam_fy
    sta.l g_cam_built_fy
    lda.l g_cam_back
    and.w #$00FF
    beq +
    lda.w #ASM_CAM_TABLE_BYTES
+   tax
    ; Backdrop: its lines, then half the camera, rounded.
    sep #$20
    lda.b #ASM_CAM_FAR_LINES
    sec
    sbc.l oam_cam_y
    sta.l g_cam_hdma,x
    rep #$20
    lda.l g_cam_fx
    clc
    adc.w #16
    cmp.w #$8000
    ror a
    cmp.w #$8000
    ror a
    cmp.w #$8000
    ror a
    cmp.w #$8000
    ror a
    cmp.w #$8000
    ror a
    sta.l g_cam_far_x
    sta.l g_cam_hdma+1,x
    ; Court: at the camera.
    sep #$20
    lda.b #ASM_CAM_COURT_LINES
    sta.l g_cam_hdma+3,x
    rep #$20
    lda.l oam_cam_x
    sta.l g_cam_hdma+4,x
    ; Near fence: 4 steps of an eighth of the camera each (10 lines x 3,
    ; then the last to the bottom).
    lda.l g_cam_fx
    cmp.w #$8000
    ror a
    cmp.w #$8000
    ror a
    cmp.w #$8000
    ror a
    sta.l g_cam_step
    lda.l g_cam_fx
    ldy.w #0
_rc_ramp:
    clc
    adc.l g_cam_step
    sta.l g_cam_acc
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
    sta.l g_cam_hdma+7,x
    sep #$20
    lda.b #10
    cpy.w #3
    bne +
    lda.b #1
+   sta.l g_cam_hdma+6,x
    rep #$20
    inx
    inx
    inx
    lda.l g_cam_acc
    iny
    cpy.w #4
    bne _rc_ramp
    sep #$20
    lda.b #0
    sta.l g_cam_hdma+6,x
    lda.b #1
    sta.l g_cam_hdma_on
    sta.l g_cam_new
    plp
    rtl

; void render_camera_commit(void)
;
; In VBlank, after render_camera: a table just written (g_cam_new) goes to
; HDMA channel 6 for the coming frame (HDMA reloads its address from $4362
; at the frame's start), and the halves swap; otherwise the shown one
; stays. HDMA stays off until the
; camera has written a table (g_cam_hdma_on; render_camera_reset clears
; it and channel 6's enable). With CAMERA OFF and the court at rest it
; stays off.
render_camera_commit:
    php
    sep #$20
    lda.l g_cam_hdma_on
    beq _rcc_done
    lda.l g_cam_new
    bne _rcc_swap
    ; CAMERA OFF with the court at rest, and its all-zero table already
    ; shown (BG1's scroll is 0): HDMA off, which takes a share of every
    ; line it runs.
    lda.l g_cam_k
    bne _rcc_enable
    rep #$20
    lda.l g_cam_fx
    ora.l g_cam_fy
    sep #$20
    bne _rcc_enable
    lda.b #0
    sta.l $420C
    bra _rcc_done
_rcc_swap:
    lda.b #$02                  ; mode 2: two writes to one register
    sta.l $4360
    lda.b #$0D                  ; BG1HOFS
    sta.l $4361
    lda.b #:g_cam_hdma
    sta.l $4364
    rep #$20
    lda.l g_cam_back
    and.w #$00FF
    beq +
    lda.w #ASM_CAM_TABLE_BYTES
+   clc
    adc.w #g_cam_hdma
    sta.l $4362
    sep #$20
    lda.l g_cam_back
    eor.b #1
    sta.l g_cam_back
    lda.b #0
    sta.l g_cam_new
_rcc_enable:
    lda.b #$40                  ; HDMA channel 6 on
    sta.l $420C
_rcc_done:
    plp
    rtl

.ENDS
