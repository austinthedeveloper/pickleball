#ifndef HUD_H
#define HUD_H

#include "game_types.h"

/* BG3 text palettes (see HUD_PALETTES in tools/generate_assets.py). */
enum {
    HUD_PAL_WHITE = 0,
    HUD_PAL_YELLOW = 1,
    HUD_PAL_BLUE = 2,
    HUD_PAL_RED = 3,
    HUD_PAL_BUTTON = 4,
    HUD_PAL_DIM = 5,
    HUD_PAL_GREEN = 6,
    HUD_PAL_GOLD = 7
};

enum {
    HUD_ROWS = 28,
    HUD_BAND_ROWS = 4,
    HUD_BANDS = 7
};

void hud_initialize(void);
void hud_clear_rows(u8 first_row, u8 count);
void hud_text(u8 col, u8 row, u8 palette, const char *text);
/* Single digit or two digits when value >= 10. */
void hud_number(u8 col, u8 row, u8 palette, u8 value);
void hud_tile(u8 col, u8 row, u16 entry);
/* Three-row panel: border rows at row and row+2, interior on row+1. */
void hud_panel(u8 col, u8 row, u8 inner_cols);
void hud_big_text(u8 col, u8 row, u8 palette, const char *text);
u8 hud_big_width(const char *text);
/* DMA dirty 4-row bands; call only inside VBlank. */
void hud_commit(void);

#endif
