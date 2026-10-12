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
hud_map:
.incbin "assets/generated/hud.map"
hud_map_end:
hud_palette:
.incbin "assets/generated/hud.pal"
hud_palette_end:
.ends

.section ".rodata_player" superfree
player_tiles:
.incbin "assets/generated/player.pic"
player_tiles_end:
player_palette:
.incbin "assets/generated/player.pal"
player_palette_end:
.ends

