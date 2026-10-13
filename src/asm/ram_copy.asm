.include "hdr.asm"

; void ram_copy(void)
;
; Copies ram_copy_len bytes (1-65535) between two work-RAM addresses in bank
; $7E (ram_copy_src -> ram_copy_dst, non-overlapping) with MVN, about 7
; cycles per byte. The scratch-struct pattern copies Player/Ball/AiState in
; and out every frame, so this replaces the library memcpy on hot paths.
; Inputs are C globals; the data bank (changed by MVN) is restored.

.SECTION ".ram_copy_text" SUPERFREE

ram_copy:
    php
    phb
    rep #$30
    lda.l ram_copy_src
    tax
    lda.l ram_copy_dst
    tay
    lda.l ram_copy_len
    dec a
    mvn $7E,$7E
    plb
    plp
    rtl

.ENDS
