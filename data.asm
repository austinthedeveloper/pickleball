.include "hdr.asm"

; The ground every venue shares: BG1 tiles and palette 2 (ADR-024).
.section ".rodata_court" superfree
court_tiles:
.incbin "assets/generated/court.pic"
court_tiles_end:
court_palette:
.incbin "assets/generated/court.pal"
court_palette_end:
.ends

; Each venue's backdrop tiles, map, and palettes 3-7.
.include "assets/generated/venues.inc"

.section ".rodata_hud" superfree
hud_tiles:
.incbin "assets/generated/hud.pic"
hud_tiles_end:
hud_palette:
.incbin "assets/generated/hud.pal"
hud_palette_end:
.ends

.section ".rodata_obj" superfree
obj_tiles:
.incbin "assets/generated/obj.pic"
obj_tiles_end:
objects_palette:
.incbin "assets/generated/objects.pal"
objects_palette_end:
.ends

; Player Select (ADR-020): loaded in forced blank when the screen opens.
.section ".rodata_select" superfree
select_tiles:
.incbin "assets/generated/select.pic"
select_tiles_end:
.ends

.section ".rodata_select_map" superfree
select_map:
.incbin "assets/generated/select.map"
select_map_end:
select_palette:
.incbin "assets/generated/select.pal"
select_palette_end:
portrait_tiles:
.incbin "assets/generated/portrait.pic"
portrait_tiles_end:
portrait_m_map:
.incbin "assets/generated/portrait_m.map"
portrait_m_map_end:
portrait_f_map:
.incbin "assets/generated/portrait_f.map"
portrait_f_map_end:
.ends
