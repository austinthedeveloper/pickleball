.include "hdr.asm"

; void oam_write(void)
;
; Writes one OAM shadow entry (oamMemory, uploaded by the PVSnesLib NMI) from
; the C globals oam_in_id, oam_in_x, oam_in_y, oam_in_tile, oam_in_attr and
; oam_in_large. Inputs travel through globals so no stack ABI is involved.
; Entries outside x -32..255 / y -32..223 are parked at y = 224, which keeps
; even 32x32 objects fully below the 224-line display. Long addressing only,
; so the data bank and direct page are never assumed.

.SECTION ".oam_write_text" SUPERFREE

oam_write:
    php
    rep #$30
    lda.l oam_in_id
    and.w #$007F
    asl a
    asl a
    tax
    lda.l oam_in_y
    clc
    adc.w #32
    cmp.w #256
    bcs _oam_hide
    lda.l oam_in_x
    clc
    adc.w #32
    cmp.w #288
    bcs _oam_hide

    sep #$20
    lda.l oam_in_x
    sta.l oamMemory,x
    lda.l oam_in_y
    sta.l oamMemory+1,x
    lda.l oam_in_tile
    sta.l oamMemory+2,x
    lda.l oam_in_tile+1
    and.b #$01
    ora.l oam_in_attr
    sta.l oamMemory+3,x
    ; High-table pair: bit 0 = X bit 8 (x < 0), bit 1 = large size.
    lda.l oam_in_large
    beq +
    lda.b #2
+   sta.l oam_tmp_bits
    lda.l oam_in_x+1
    bpl _oam_high
    lda.l oam_tmp_bits
    ora.b #1
    sta.l oam_tmp_bits
    bra _oam_high

_oam_hide:
    sep #$20
    lda.b #$E0
    sta.l oamMemory+1,x
    lda.b #0
    sta.l oam_tmp_bits

_oam_high:
    rep #$20
    lda.l oam_in_id
    and.w #$007F
    lsr a
    lsr a
    tax
    sep #$20
    lda.b #$FC
    sta.l oam_tmp_mask
    lda.l oam_in_id
    and.b #$03
    beq _oam_apply
    tay
-   lda.l oam_tmp_bits
    asl a
    asl a
    sta.l oam_tmp_bits
    lda.l oam_tmp_mask
    sec
    rol a
    sec
    rol a
    sta.l oam_tmp_mask
    dey
    bne -
_oam_apply:
    lda.l oamMemory+512,x
    and.l oam_tmp_mask
    ora.l oam_tmp_bits
    sta.l oamMemory+512,x
    plp
    rtl

.ENDS
