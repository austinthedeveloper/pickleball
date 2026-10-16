.include "hdr.asm"

; void oam_write(void)
;
; Writes one OAM shadow entry (oamMemory, uploaded by the PVSnesLib NMI) from
; the C globals oam_in_id, oam_in_x, oam_in_y, oam_in_tile, oam_in_attr and
; oam_in_large, less the camera's oam_cam_x/oam_cam_y (ADR-050; 0 off the
; court). Inputs travel through globals so no stack ABI is involved.
; Entries outside x -32..255 / y -32..223 are parked as small objects at
; y = 240: below every evaluated line, so they neither show nor count toward
; the PPU's 32-object range-over limit. Long addressing only,
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
    ; The camera (ADR-050): the object moves against the court's scroll.
    lda.l oam_in_y
    sec
    sbc.l oam_cam_y
    sta.l oam_tmp_y
    clc
    adc.w #32
    cmp.w #256
    bcs _oam_hide
    lda.l oam_in_x
    sec
    sbc.l oam_cam_x
    sta.l oam_tmp_x
    clc
    adc.w #32
    cmp.w #288
    bcs _oam_hide

    sep #$20
    lda.l oam_tmp_x
    sta.l oamMemory,x
    lda.l oam_tmp_y
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
    lda.l oam_tmp_x+1
    bpl _oam_high
    lda.l oam_tmp_bits
    ora.b #1
    sta.l oam_tmp_bits
    bra _oam_high

_oam_hide:
    sep #$20
    lda.b #$F0
    sta.l oamMemory+1,x
    lda.b #0
    sta.l oam_tmp_bits

_oam_high:
    ; The pair's place in its high-table byte from tables (R20): mask by
    ; id & 3, bits by (id & 3) * 4 + bits. No shift loop; 8-bit indexes
    ; (plp restores the caller's widths).
    sep #$30
    lda.l oam_in_id
    and.b #$03
    tay
    asl a
    asl a
    ora.l oam_tmp_bits
    tax
    lda.l _oam_pair_bits,x
    sta.l oam_tmp_bits
    tyx
    lda.l _oam_pair_mask,x
    sta.l oam_tmp_mask
    lda.l oam_in_id
    and.b #$7F
    lsr a
    lsr a
    tax
    lda.l oamMemory+512,x
    and.l oam_tmp_mask
    ora.l oam_tmp_bits
    sta.l oamMemory+512,x
    plp
    rtl

; The high-table byte's mask for entry id & 3, and its two bits (X bit 8,
; large) shifted into place, indexed by (id & 3) * 4 + bits.
_oam_pair_mask:
    .db $FC, $F3, $CF, $3F
_oam_pair_bits:
    .db $00, $01, $02, $03
    .db $00, $04, $08, $0C
    .db $00, $10, $20, $30
    .db $00, $40, $80, $C0

.ENDS
