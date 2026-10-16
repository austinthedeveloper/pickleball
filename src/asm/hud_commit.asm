.include "hdr.asm"
.include "assets/generated/asm_shared.inc"

; void hud_commit(void)
;
; In VBlank: uploads up to ASM_HUD_MAX_BANDS dirty 4-row bands of the BG3
; HUD map (g_hud_map, bit n of hud_dirty for band n) to VRAM, first band
; first, and clears their bits; the rest stay dirty for the next frame. It
; writes DMA channel 0 directly (R18): through dmaCopyVram a band cost
; about four scanlines of VBlank, most of it call overhead. The NMI has
; already sent OAM (channel 7) when this runs. Long addressing only.

.SECTION ".hud_commit_text" SUPERFREE

hud_commit:
    php
    sep #$20
    lda.l hud_dirty
    beq _hc_done
    rep #$10
    lda.b #$80                  ; VMAIN: word writes, step after the high byte
    sta.l $2115
    lda.b #$01                  ; DMA mode 1: $2118/$2119
    sta.l $4300
    lda.b #$18
    sta.l $4301
    lda.b #:g_hud_map
    sta.l $4304
    ldy.w #0                    ; bands sent
    ldx.w #0                    ; band offset in bytes
    lda.b #1
    sta.l hud_commit_bit
_hc_band:
    lda.l hud_dirty
    and.l hud_commit_bit
    beq _hc_next
    rep #$20
    txa
    lsr a
    clc
    adc.w #ASM_HUD_MAP_ADDR
    sta.l $2116
    txa
    clc
    adc.w #g_hud_map
    sta.l $4302
    lda.w #ASM_HUD_BAND_BYTES
    sta.l $4305
    sep #$20
    lda.b #$01
    sta.l $420B
    lda.l hud_commit_bit
    eor.b #$FF
    and.l hud_dirty
    sta.l hud_dirty
    iny
    cpy.w #ASM_HUD_MAX_BANDS
    beq _hc_done
_hc_next:
    rep #$20
    txa
    clc
    adc.w #ASM_HUD_BAND_BYTES
    tax
    sep #$20
    lda.l hud_commit_bit
    asl a
    sta.l hud_commit_bit
    cmp.b #1 << ASM_HUD_BANDS
    bne _hc_band
_hc_done:
    plp
    rtl

.ENDS
