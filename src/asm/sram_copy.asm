.include "hdr.asm"

; void sram_copy(void)
;
; Copies sram_copy_len bytes (1-65535) from work RAM at sram_copy_src (bank
; $7E) to battery-backed SRAM at offset sram_copy_dst (bank $70, LoROM)
; with MVN, about 7 cycles a byte. The library's consoleCopySramWithOffset
; took several times that, and the save grew to 212 bytes with the career
; players (ADR-047): a match-end write overran the frame. Inputs are C
; globals; the data bank (changed by MVN) is restored.

.SECTION ".sram_copy_text" SUPERFREE

sram_copy:
    php
    phb
    rep #$30
    lda.l sram_copy_src
    tax
    lda.l sram_copy_dst
    tay
    lda.l sram_copy_len
    dec a
    mvn $7E,$70
    plb
    plp
    rtl

.ENDS
