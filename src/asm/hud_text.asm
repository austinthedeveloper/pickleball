.include "hdr.asm"

; void hud_text_write(void)
;
; The text loop of hud_text (src/hud.c) in assembly (M4-UI-01): each
; character of the string at hud_text_src becomes a BG3 map entry in
; g_hud_map from word index hud_text_index, at most hud_text_cols of them.
; Lower case is upper-cased and anything outside the font (32-95) is '?'.
; hud_text_attr is the entry's palette and priority bits. About 40 cycles
; a character, against some 500 for the 816-tcc loop.
;
; The string may be in any bank: its 32-bit C pointer is copied to the
; compiler's scratch register tcc__r9 (direct page; 816-tcc spills its
; registers around calls, and the NMI has its own) and read with [dp],y.

.SECTION ".hud_text_text" SUPERFREE

hud_text_write:
    php
    rep #$30
    lda.l hud_text_src
    sta.b tcc__r9
    lda.l hud_text_src + 2
    sta.b tcc__r9h
    lda.l hud_text_index
    asl a
    tax
    ldy.w #0
_hud_text_next:
    tya
    cmp.l hud_text_cols
    bcs _hud_text_done
    lda.b [tcc__r9],y
    and.w #$00FF
    beq _hud_text_done
    cmp.w #'a'
    bcc _hud_text_range
    cmp.w #'z' + 1
    bcs _hud_text_range
    sbc.w #'a' - 'A' - 1        ; carry clear: subtracts 'a' - 'A'
_hud_text_range:
    cmp.w #32
    bcc _hud_text_unknown
    cmp.w #96
    bcc _hud_text_store
_hud_text_unknown:
    lda.w #'?'
_hud_text_store:
    ora.l hud_text_attr
    sta.l g_hud_map,x
    inx
    inx
    iny
    bra _hud_text_next
_hud_text_done:
    plp
    rtl

.ENDS
