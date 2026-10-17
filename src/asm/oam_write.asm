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

; void oam_write_pair(void)
;
; A 16x8 pair of small objects, entries oam_in_id and oam_in_id + 1, the
; second 8 px right of the first with tile oam_in_tile ^ 1, same attribute
; (a hat, ADR-059/060). Same placement as two oam_write calls (the camera,
; parking at y = 240) for about half the cost: one camera and bounds test,
; and the second entry copied from the first. When the first is off screen,
; both are parked: the second, 8 px to its right, can't show either.

oam_write_pair:
    php
    rep #$30
    lda.l oam_in_id
    and.w #$007F
    asl a
    asl a
    tax
    lda.l oam_in_y
    sec
    sbc.l oam_cam_y
    sta.l oam_tmp_y
    clc
    adc.w #32
    cmp.w #256
    bcc +
    brl _oam_pair_hide
+   lda.l oam_in_x
    sec
    sbc.l oam_cam_x
    sta.l oam_tmp_x
    clc
    adc.w #32
    cmp.w #288
    bcc +
    brl _oam_pair_hide
+   sep #$20
    lda.l oam_tmp_x
    sta.l oamMemory,x
    clc
    adc.b #8
    sta.l oamMemory+4,x
    lda.l oam_tmp_y
    sta.l oamMemory+1,x
    sta.l oamMemory+5,x
    lda.l oam_in_tile
    sta.l oamMemory+2,x
    eor.b #1
    sta.l oamMemory+6,x
    lda.l oam_in_tile+1
    and.b #$01
    ora.l oam_in_attr
    sta.l oamMemory+3,x
    sta.l oamMemory+7,x
    ; X bit 8 of each (both small): the first's is its sign. The second
    ; is at x + 8, -24..263: bit 8 set below 0, and from 256 it is parked,
    ; as oam_write parks it (an object at x 256 counts toward the PPU's
    ; per-line range). Its bits wait in oam_tmp_y, done with.
    rep #$20
    lda.l oam_tmp_x
    clc
    adc.w #8
    bmi _oam_pair_left
    cmp.w #256
    bcc _oam_pair_on
    sep #$20
    lda.b #$F0
    sta.l oamMemory+5,x
    rep #$20
_oam_pair_on:
    lda.w #0
    bra _oam_pair_second
_oam_pair_left:
    lda.w #1
_oam_pair_second:
    sta.l oam_tmp_y
    sep #$20
    lda.l oam_tmp_x+1
    rol a                       ; carry: the first's sign
    lda.b #0
    rol a
    sta.l oam_tmp_bits
    bra _oam_pair_high

_oam_pair_hide:
    sep #$20
    lda.b #$F0
    sta.l oamMemory+1,x
    sta.l oamMemory+5,x
    lda.b #0
    sta.l oam_tmp_bits
    sta.l oam_tmp_y

_oam_pair_high:
    sep #$30
    lda.l oam_in_id
    jsr _oam_pair_put
    lda.l oam_tmp_y
    sta.l oam_tmp_bits
    lda.l oam_in_id
    inc a
    jsr _oam_pair_put
    plp
    rtl

; Entry A's pair in the high table, its bits (X bit 8, large) in
; oam_tmp_bits, through the R20 tables as in oam_write. 8-bit A, X, Y;
; uses oam_tmp_mask for the id.
_oam_pair_put:
    and.b #$7F
    sta.l oam_tmp_mask
    and.b #$03
    tax
    lda.l _oam_pair_mask,x
    pha
    txa
    asl a
    asl a
    ora.l oam_tmp_bits
    tax
    lda.l _oam_pair_bits,x
    sta.l oam_tmp_bits
    lda.l oam_tmp_mask
    lsr a
    lsr a
    tax
    pla
    and.l oamMemory+512,x
    ora.l oam_tmp_bits
    sta.l oamMemory+512,x
    rts

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
