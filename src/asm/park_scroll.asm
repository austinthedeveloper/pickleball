.include "hdr.asm"

; void park_oam_scroll(void)
;
; The park scrolled (M4-OPEN-05): every object it shows moves with the view.
; Goes over the park's OAM cache (render_park.c: g_park_ox, g_park_oy,
; g_park_ot, g_park_oa, g_park_ocount entries; x in the park, not on the
; screen), skipping hidden ones (y $7FFF), from view g_park_cam_prev to
; g_park_cam:
;
; - on screen (x 0..255) before and after: only the x byte changes, one
;   store (most objects, most frames)
; - off screen (outside oam_write's -32..255) before and after: nothing
; - otherwise (crossing an edge): the whole entry through oam_write, which
;   parks it off screen or brings it back and sets the X high bit
;
; oam_write for every object cost about 100 scanlines a scroll frame.
; g_park_si is the loop index (a global: oam_write keeps no registers).
; Long addressing only, as oam_write.

.SECTION ".park_scroll_text" SUPERFREE

park_oam_scroll:
    php
    rep #$30
    lda.w #0
    sta.l g_park_si
_park_loop:
    lda.l g_park_si
    cmp.l g_park_ocount
    bcc +
    jmp _park_done
+   asl a
    tax
    lda.l g_park_oy,x
    cmp.w #$7FFF
    bne +
    jmp _park_next
    ; Before: x + 32 on the old view (0..287 is oam_write's window).
+   lda.l g_park_ox,x
    sec
    sbc.l g_park_cam_prev
    clc
    adc.w #32
    sta.l g_park_before
    ; After: the screen x, and x + 32.
    lda.l g_park_ox,x
    sec
    sbc.l g_park_cam
    sta.l oam_in_x
    clc
    adc.w #32
    cmp.w #288
    bcc _park_after_in
    ; Off screen now: nothing to do if it was off before too.
    lda.l g_park_before
    cmp.w #288
    bcs _park_next
    bra _park_slow
_park_after_in:
    ; On screen before and after (0..255, i.e. 32..287 here): x only.
    cmp.w #32
    bcc _park_slow
    lda.l g_park_before
    cmp.w #32
    bcc _park_slow
    cmp.w #288
    bcs _park_slow
    txa
    asl a
    tax
    sep #$20
    lda.l oam_in_x
    sta.l oamMemory,x
    rep #$20
    bra _park_next
_park_slow:
    lda.l g_park_oy,x
    sta.l oam_in_y
    lda.l g_park_si
    tax
    sep #$20
    sta.l oam_in_id
    lda.l g_park_ot,x
    sta.l oam_in_tile
    lda.b #0
    sta.l oam_in_tile+1
    sta.l oam_in_large
    lda.l g_park_oa,x
    sta.l oam_in_attr
    rep #$20
    jsl oam_write
    rep #$30
_park_next:
    lda.l g_park_si
    inc a
    sta.l g_park_si
    jmp _park_loop
_park_done:
    lda.l g_park_cam
    sta.l g_park_cam_prev
    plp
    rtl

.ENDS
