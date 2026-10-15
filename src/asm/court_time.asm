.include "hdr.asm"

; void court_time_apply(void)
;
; TIME (ADR-045): ct_count BGR15 colours from ct_src to ct_dst (work-RAM
; addresses in bank $7E; the same address transforms in place), each
; channel through g_court_time_r/g/b. A colour takes the lit tables when
; its bit of ct_mask (bit 0 first) is set, else the dark ones; ct_dark and
; ct_lit are the tables' byte offsets for the time. Inputs and working
; variables (ct_*) are C globals in src/court_style.c (no stack ABI), long
; addressing only. About 110 cycles a colour, against several hundred for
; the 816-tcc loop that host builds run.

.SECTION ".court_time_text" SUPERFREE

court_time_apply:
    php
    rep #$30
    lda.l ct_count
    bne +
    jmp _ct_done
+   sta.l ct_left
_ct_loop:
    ; The tables for this colour: lit or dark, by the next mask bit.
    lda.l ct_mask
    lsr a
    sta.l ct_mask
    lda.l ct_dark
    bcc +
    lda.l ct_lit
+   sta.l ct_off
    lda.l ct_src
    tax
    lda.l $7E0000,x
    sta.l ct_color
    ; Red: bits 0-4.
    and #$001F
    asl a
    clc
    adc.l ct_off
    tax
    lda.l g_court_time_r,x
    sta.l ct_out
    ; Green: bits 5-9.
    lda.l ct_color
    lsr a
    lsr a
    lsr a
    lsr a
    lsr a
    and #$001F
    asl a
    clc
    adc.l ct_off
    tax
    lda.l g_court_time_g,x
    ora.l ct_out
    sta.l ct_out
    ; Blue: bits 10-14.
    lda.l ct_color
    xba
    lsr a
    lsr a
    and #$001F
    asl a
    clc
    adc.l ct_off
    tax
    lda.l g_court_time_b,x
    ora.l ct_out
    sta.l ct_out
    ; Store, and on to the next colour.
    lda.l ct_dst
    tax
    lda.l ct_out
    sta.l $7E0000,x
    inx
    inx
    txa
    sta.l ct_dst
    lda.l ct_src
    inc a
    inc a
    sta.l ct_src
    lda.l ct_left
    dec a
    sta.l ct_left
    beq _ct_done
    jmp _ct_loop
_ct_done:
    plp
    rtl

.ENDS
