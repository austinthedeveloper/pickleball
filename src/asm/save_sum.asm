.include "hdr.asm"

; void save_sum(void)
;
; The save block's Fletcher-style checksum (src/save.c): over save_sum_len
; bytes of work RAM at save_sum_src (bank $7E), a += byte, b += a, both mod
; 256; save_sum_result = b << 8 | a. Inputs, save_sum_b (the second sum),
; and the result are C globals; long addressing only. About 25 cycles a
; byte, against several times that for the 816-tcc loop, which host builds
; run: the save grew to 212 bytes with the career players (ADR-047), and
; every write sums it.

.SECTION ".save_sum_text" SUPERFREE

save_sum:
    php
    rep #$30
    lda.l save_sum_src
    tax
    lda.l save_sum_len
    tay
    sep #$20
    lda #0
    sta.l save_sum_b
    cpy #0
    beq _ss_done
_ss_loop:
    clc
    adc.l $7E0000,x         ; A: a += byte
    pha
    clc
    adc.l save_sum_b        ; b += a
    sta.l save_sum_b
    pla
    inx
    dey
    bne _ss_loop
_ss_done:
    sta.l save_sum_result
    lda.l save_sum_b
    sta.l save_sum_result + 1
    plp
    rtl

.ENDS
