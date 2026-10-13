.include "hdr.asm"

; void hud_fill(void)
;
; Stores hud_fill_value into hud_fill_count consecutive words of g_hud_map,
; starting at word index hud_fill_index. Inputs are C globals (no stack ABI);
; long addressing only. About 12 cycles per word, against roughly 100 for an
; 816-tcc loop or the library's byte-wise memset.

.SECTION ".hud_fill_text" SUPERFREE

hud_fill:
    php
    rep #$30
    lda.l hud_fill_count
    beq _hud_fill_done
    tay
    lda.l hud_fill_index
    asl a
    tax
    lda.l hud_fill_value
-   sta.l g_hud_map,x
    inx
    inx
    dey
    bne -
_hud_fill_done:
    plp
    rtl

.ENDS
