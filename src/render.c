#include <snes.h>
#include "court.h"
#include "input.h"
#include "player.h"
#include "render.h"

#define BG1_MAP_ADDR 0x0000
#define BG3_MAP_ADDR 0x0400
#define BG1_TILES_ADDR 0x1000
#define BG3_TILES_ADDR 0x3000
#define OBJ_TILES_ADDR 0x4000
#define PLAYER_OAM_ID 0
#define SHADOW_OAM_ID 4
#define HUD_TILE_PRIORITY 0x2000

extern char court_tiles, court_tiles_end;
extern char court_map, court_map_end;
extern char court_palette, court_palette_end;
extern char hud_tiles, hud_tiles_end;
extern char hud_map, hud_map_end;
extern char hud_palette, hud_palette_end;
extern char player_tiles, player_tiles_end;
extern char player_palette, player_palette_end;

static u16 hud_buffer[32 * 4];

static void hud_clear(void)
{
    u16 i;
    for (i = 0; i < 32 * 4; ++i)
        hud_buffer[i] = HUD_TILE_PRIORITY;
}

static void hud_put_text(u8 x, u8 y, const char *text)
{
    u8 character;
    while (*text && x < 32) {
        character = (u8)*text++;
        if (character >= 'a' && character <= 'z')
            character -= ('a' - 'A');
        if (character < 32 || character > 127)
            character = '?';
        hud_buffer[(u16)y * 32 + x] = (character - 32) | HUD_TILE_PRIORITY;
        ++x;
    }
}

static void hud_put_u16_3(u8 x, u8 y, u16 value)
{
    hud_buffer[(u16)y * 32 + x] =
        (('0' - 32) + ((value / 100) % 10)) | HUD_TILE_PRIORITY;
    hud_buffer[(u16)y * 32 + x + 1] =
        (('0' - 32) + ((value / 10) % 10)) | HUD_TILE_PRIORITY;
    hud_buffer[(u16)y * 32 + x + 2] =
        (('0' - 32) + (value % 10)) | HUD_TILE_PRIORITY;
}

static void hud_put_hex4(u8 x, u8 y, u16 value)
{
    static const char digits[] = "0123456789ABCDEF";
    u8 shift;
    u8 character;
    for (shift = 12; shift < 16; shift -= 4) {
        character = digits[(value >> shift) & 15];
        hud_buffer[(u16)y * 32 + x] = (character - 32) | HUD_TILE_PRIORITY;
        ++x;
        if (shift == 0)
            break;
    }
}

void render_initialize(void)
{
    setScreenOff();
    oamInit();
    oamClear(0, 0);

    bgSetMapPtr(0, BG1_MAP_ADDR, SC_32x32);
    bgSetMapPtr(2, BG3_MAP_ADDR, SC_32x32);

    bgInitTileSet(0, &court_tiles, &court_palette, 1,
        (&court_tiles_end - &court_tiles),
        (&court_palette_end - &court_palette), BG_16COLORS, BG1_TILES_ADDR);

    bgInitTileSet(2, &hud_tiles, &hud_palette, 0,
        (&hud_tiles_end - &hud_tiles),
        (&hud_palette_end - &hud_palette), BG_4COLORS, BG3_TILES_ADDR);

    dmaCopyVram(&court_map, BG1_MAP_ADDR, (&court_map_end - &court_map));
    dmaCopyVram(&hud_map, BG3_MAP_ADDR, (&hud_map_end - &hud_map));

    oamInitGfxSet(&player_tiles, (&player_tiles_end - &player_tiles),
        &player_palette, (&player_palette_end - &player_palette),
        0, OBJ_TILES_ADDR, OBJ_SIZE8_L32);

    setMode(BG_MODE1, BG3_MODE1_PRIORITY_HIGH);
    bgSetEnable(0);
#if defined(BUILD_DEBUG) || defined(BUILD_HARDWARE_TEST)
    bgSetEnable(2);
#else
    bgSetDisable(2);
#endif
    bgSetDisable(1);
    hud_clear();
#if !defined(BUILD_DEBUG) && !defined(BUILD_HARDWARE_TEST)
    hud_put_text(13, 1, "PAUSED");
    dmaCopyVram((u8 *)hud_buffer, BG3_MAP_ADDR, sizeof(hud_buffer));
#endif
    render_prepare_players();
    render_commit_hud(0);
    setScreenOn();
}

void render_prepare_players(void)
{
    Player *player = &g_players[0];
    const PlayerAnimation *animation;
    u16 tile;
    s16 x;
    s16 y;

    if (!player->active) {
        oamSetVisible(PLAYER_OAM_ID, OBJ_HIDE);
        oamSetVisible(SHADOW_OAM_ID, OBJ_HIDE);
        return;
    }

    x = FROM_FP(player->x_fp);
    y = FROM_FP(player->y_fp);
    animation = &g_player_animations[player->animation];
    tile = animation->tile_base[player->animation_frame];

    oamSet(PLAYER_OAM_ID, x - PLAYER_BODY_HALF_WIDTH,
        y - PLAYER_BODY_TOP_OFFSET, 3,
        player->facing_left, 0, tile, 0);
    oamSetEx(PLAYER_OAM_ID, OBJ_LARGE, OBJ_SHOW);
    oamSet(SHADOW_OAM_ID, x - PLAYER_SHADOW_HALF_WIDTH,
        y + PLAYER_SHADOW_Y_OFFSET, 2, 0, 0, 8, 0);
    oamSetEx(SHADOW_OAM_ID, OBJ_SMALL, OBJ_SHOW);
}

void render_commit_hud(u8 paused)
{
#if defined(BUILD_DEBUG) || defined(BUILD_HARDWARE_TEST)
    hud_clear();
    hud_put_text(1, 0, "BUILD " BUILD_ID_STRING);
    hud_put_text(1, 1, "P1 ");
    hud_put_u16_3(4, 1, (u16)FROM_FP(g_players[0].x_fp));
    hud_put_text(7, 1, ",");
    hud_put_u16_3(8, 1, (u16)FROM_FP(g_players[0].y_fp));
#ifdef BUILD_DEBUG
    hud_put_text(1, 2, "H");
    hud_put_hex4(2, 2, g_pad_snapshots[0].held);
    hud_put_text(8, 2, "P");
    hud_put_hex4(9, 2, g_pad_snapshots[0].pressed);
    hud_put_text(15, 2, "R");
    hud_put_hex4(16, 2, g_pad_snapshots[0].released);
#endif
    if (paused)
        hud_put_text(13, 1, "PAUSED");
    dmaCopyVram((u8 *)hud_buffer, BG3_MAP_ADDR, sizeof(hud_buffer));
#else
    if (paused)
        bgSetEnable(2);
    else
        bgSetDisable(2);
#endif
}

