.include "hdr.asm"
.include "assets/generated/asm_shared.inc"

; void ai_probe_fast(void)
;
; One step of the swing lookahead's ball (g_probe, src/ai.c) in assembly,
; for the common cases: off the ground and not landing this step, rising
; from it, or lying on it. That is four adds, but in 816-tcc's C each step of a doubles
; CPU's lookahead (14-16 steps a frame with FAST BALL) cost 2-3 scanlines
; (KNOWN_ISSUES #22). A step that lands (the bounce) sets ai_probe_slow, and
; the C caller does it in ai_probe_step_c instead, so the bounce rules live
; in one place; otherwise ai_probe_slow is 0. The host tests run the C step
; for every step; the golden trace (the probe runs every swing check)
; compares the two.

.SECTION ".ai_probe_text" SUPERFREE

ai_probe_fast:
    php
    rep #$30
    lda.l g_probe + ASM_PROBE_Z
    beq _apf_ground
    bmi _apf_slow               ; below the ground: never, but the C knows
    clc
    adc.l g_probe + ASM_PROBE_VZ
    beq _apf_slow               ; lands this step: the bounce
    bmi _apf_slow
    sta.l g_probe + ASM_PROBE_Z
    bra _apf_move
_apf_ground:
    lda.l g_probe + ASM_PROBE_VZ
    beq _apf_rest
    bmi _apf_rest
    sta.l g_probe + ASM_PROBE_Z ; rising off the ground: z = 0 + vz
_apf_move:
    lda.l g_probe + ASM_PROBE_X
    clc
    adc.l g_probe + ASM_PROBE_VX
    sta.l g_probe + ASM_PROBE_X
    lda.l g_probe + ASM_PROBE_Y
    clc
    adc.l g_probe + ASM_PROBE_VY
    sta.l g_probe + ASM_PROBE_Y
    lda.l g_probe + ASM_PROBE_VZ
    sec
    sbc.w #ASM_BALL_FALL
    sta.l g_probe + ASM_PROBE_VZ
    sep #$20
    lda.b #0
    sta.l ai_probe_slow
    plp
    rtl
_apf_rest:
    ; On the ground and not rising: it lies there (as the C step).
    lda.w #0
    sta.l g_probe + ASM_PROBE_VX
    sta.l g_probe + ASM_PROBE_VY
    sta.l g_probe + ASM_PROBE_VZ
    sep #$20
    sta.l ai_probe_slow
    plp
    rtl
_apf_slow:
    sep #$20
    lda.b #1
    sta.l ai_probe_slow
    plp
    rtl

.ENDS
