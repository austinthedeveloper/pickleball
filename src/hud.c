#include <snes.h>
#include "hud.h"
#include "hud_tiles.h"
#include "render.h"

#define HUD_PRIORITY 0x2000
#define HUD_HFLIP 0x4000
#define HUD_VFLIP 0x8000
#define HUD_SPACE 32

static u16 hud_map[32 * HUD_ROWS];
static u8 hud_dirty;

static u16 hud_entry(u16 tile, u8 palette)
{
    return tile | ((u16)palette << 10) | HUD_PRIORITY;
}

static void hud_mark(u8 row)
{
    hud_dirty |= (u8)(1 << (row >> 2));
}

void hud_initialize(void)
{
    hud_clear_rows(0, HUD_ROWS);
}

void hud_clear_rows(u8 first_row, u8 count)
{
    u16 i;
    u16 end;
    u8 row;

    i = (u16)first_row * 32;
    end = i + (u16)count * 32;
    for (; i < end; ++i)
        hud_map[i] = HUD_TILE_CLEAR;
    for (row = first_row; row < first_row + count; ++row)
        hud_mark(row);
}

void hud_tile(u8 col, u8 row, u16 entry)
{
    hud_map[(u16)row * 32 + col] = entry;
    hud_mark(row);
}

void hud_text(u8 col, u8 row, u8 palette, const char *text)
{
    u16 index = (u16)row * 32 + col;
    u8 c;

    while ((c = (u8)*text++) != 0 && col < 32) {
        if (c >= 'a' && c <= 'z')
            c -= 'a' - 'A';
        if (c < 32 || c > 95)
            c = '?';
        hud_map[index++] = hud_entry(c, palette);
        ++col;
    }
    hud_mark(row);
}

void hud_number(u8 col, u8 row, u8 palette, u8 value)
{
    if (value >= 10) {
        hud_map[(u16)row * 32 + col] = hud_entry('0' + value / 10, palette);
        ++col;
    }
    hud_map[(u16)row * 32 + col] = hud_entry('0' + value % 10, palette);
    hud_mark(row);
}

void hud_panel(u8 col, u8 row, u8 inner_cols)
{
    u8 x;
    u8 right = col + inner_cols + 1;
    u16 top = (u16)row * 32;
    u16 mid = top + 32;
    u16 bottom = mid + 32;

    hud_map[top + col] = hud_entry(HUD_TILE_PANEL_TL, 0);
    hud_map[top + right] = hud_entry(HUD_TILE_PANEL_TL, 0) | HUD_HFLIP;
    hud_map[bottom + col] = hud_entry(HUD_TILE_PANEL_TL, 0) | HUD_VFLIP;
    hud_map[bottom + right] =
        hud_entry(HUD_TILE_PANEL_TL, 0) | HUD_HFLIP | HUD_VFLIP;
    hud_map[mid + col] = hud_entry(HUD_TILE_PANEL_L, 0);
    hud_map[mid + right] = hud_entry(HUD_TILE_PANEL_L, 0) | HUD_HFLIP;
    for (x = col + 1; x < right; ++x) {
        hud_map[top + x] = hud_entry(HUD_TILE_PANEL_T, 0);
        hud_map[mid + x] = hud_entry(HUD_SPACE, 0);
        hud_map[bottom + x] = hud_entry(HUD_TILE_PANEL_T, 0) | HUD_VFLIP;
    }
    hud_mark(row);
    hud_mark(row + 1);
    hud_mark(row + 2);
}

static s16 hud_big_index(u8 c)
{
    const char *chars = HUD_BIG_CHARS;
    s16 i = 0;

    while (chars[i]) {
        if ((u8)chars[i] == c)
            return i;
        ++i;
    }
    return -1;
}

u8 hud_big_width(const char *text)
{
    u8 n = 0;
    while (*text++)
        n += 2;
    return n;
}

void hud_big_text(u8 col, u8 row, u8 palette, const char *text)
{
    u16 top = (u16)row * 32;
    u16 tile;
    s16 index;
    u8 c;

    while ((c = (u8)*text++) != 0 && col < 31) {
        index = hud_big_index(c);
        if (index < 0) {
            hud_map[top + col] = HUD_TILE_CLEAR;
            hud_map[top + col + 1] = HUD_TILE_CLEAR;
            hud_map[top + 32 + col] = HUD_TILE_CLEAR;
            hud_map[top + 32 + col + 1] = HUD_TILE_CLEAR;
        } else {
            tile = HUD_TILE_BIG_BASE + (u16)index * 4;
            hud_map[top + col] = hud_entry(tile, palette);
            hud_map[top + col + 1] = hud_entry(tile + 1, palette);
            hud_map[top + 32 + col] = hud_entry(tile + 2, palette);
            hud_map[top + 32 + col + 1] = hud_entry(tile + 3, palette);
        }
        col += 2;
    }
    hud_mark(row);
    hud_mark(row + 1);
}

void hud_commit(void)
{
    u8 band;

    if (!hud_dirty)
        return;
    for (band = 0; band < HUD_BANDS; ++band) {
        if (hud_dirty & (1 << band))
            dmaCopyVram((u8 *)&hud_map[(u16)band * 32 * HUD_BAND_ROWS],
                RENDER_BG3_MAP_ADDR + (u16)band * 32 * HUD_BAND_ROWS,
                32 * HUD_BAND_ROWS * 2);
    }
    hud_dirty = 0;
}
