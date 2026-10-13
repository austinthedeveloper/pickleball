.include "hdr.asm"

.section ".rodata_court" superfree
court_tiles:
.incbin "assets/generated/court.pic"
court_tiles_end:
court_map:
.incbin "assets/generated/court.map"
court_map_end:
court_palette:
.incbin "assets/generated/court.pal"
court_palette_end:
.ends

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
