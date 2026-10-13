#include <snes.h>
#include <string.h>
#include "ai.h"
#include "asset_frames.h"
#include "ball.h"
#include "court.h"
#include "game.h"
#include "hud.h"
#include "hud_tiles.h"
#include "input.h"
#include "player.h"
#include "profile.h"
#include "render.h"
#include "serve.h"

#define CGRAM_OBJ 128
#define OBJ_PRIORITY 2
#define BANNER_ROW 24
#define CENTER_BIG_ROW 8
#define CENTER_PANEL_ROW 11

extern char court_tiles, court_tiles_end;
extern char court_map, court_map_end;
extern char court_palette, court_palette_end;
extern char hud_tiles, hud_tiles_end;
extern char hud_palette, hud_palette_end;
extern char obj_tiles, obj_tiles_end;
extern char player_palettes, player_palettes_end;
extern char objects_palette, objects_palette_end;

static const u16 s_idle_frames[2] = { FRAME_IDLE0, FRAME_IDLE1 };
static const u16 s_run_frames[4] = {
    FRAME_RUN0, FRAME_RUN1, FRAME_RUN2, FRAME_RUN3
};
static const u8 s_team_hud_palette[TEAM_PALETTE_COUNT] = {
    HUD_PAL_BLUE, HUD_PAL_RED, HUD_PAL_GREEN, HUD_PAL_GOLD
};

static u8 s_team_color[2] = { TEAM_BLUE, TEAM_RED };
static u8 s_palette_dirty;

/* Last HUD inputs, so the BG3 map is only rebuilt when something changes. */
static u8 s_hud_score[2];
static u8 s_hud_scene;
static u8 s_hud_valid;
#if defined(BUILD_DEBUG) || defined(BUILD_HARDWARE_TEST)
static u8 s_hud_debug_build_shown;
#endif

/*
 * 128 + ((x - 128) * k) >> 8 with k = base + y/2, rewritten as
 * d + (d * (k - 256)) >> 8 so the product fits in 16 bits for on-screen y.
 */
s16 render_project_x(s16 x, s16 y)
{
    s16 d = x - 128;
    s16 k = COURT_PERSPECTIVE_BASE + (y >> 1) - 256;
    return 128 + d + ((d * k) >> 8);
}

void render_set_team_color(u8 team, u8 color)
{
    s_team_color[team] = color;
    s_palette_dirty = 1;
    s_hud_valid = 0;
}

u8 render_team_color(u8 team)
{
    return s_team_color[team];
}

static void render_upload_team_palettes(void)
{
    dmaCopyCGram((u8 *)&player_palettes + (u16)s_team_color[0] * 32,
        CGRAM_OBJ + RENDER_OBJPAL_TEAM0 * 16, 32);
    dmaCopyCGram((u8 *)&player_palettes + (u16)s_team_color[1] * 32,
        CGRAM_OBJ + RENDER_OBJPAL_TEAM1 * 16, 32);
    s_palette_dirty = 0;
}

/* Inputs for oam_write (src/asm/oam_write.asm). */
u8 oam_in_id;
s16 oam_in_x;
s16 oam_in_y;
u16 oam_in_tile;
u8 oam_in_attr;
u8 oam_in_large;
u8 oam_tmp_bits;
u8 oam_tmp_mask;
extern void oam_write(void);

#define OBJ_ATTR(palette, hflip) \
    ((OBJ_PRIORITY << 4) | ((palette) << 1) | ((hflip) << 6))

static void obj_put(u8 id, s16 x, s16 y, u16 tile, u8 attr, u8 large)
{
    oam_in_id = id;
    oam_in_x = x;
    oam_in_y = y;
    oam_in_tile = tile;
    oam_in_attr = attr;
    oam_in_large = large;
    oam_write();
}

static void obj_hide(u8 id)
{
    oam_in_id = id;
    oam_in_y = 224;
    oam_write();
}

/* Static scratch copy: 816-tcc reaches static fields with one long access,
 * whereas every pointer dereference costs several stack loads. */
static Player s_rp;

static u16 player_frame(void)
{
    if (s_rp.pose == PLAYER_POSE_CHEER)
        return FRAME_CHEER;
    if (s_rp.pose == PLAYER_POSE_TOSS)
        return FRAME_TOSS;
    switch (s_rp.state) {
    case PLAYER_STATE_WINDUP:
        return FRAME_WINDUP;
    case PLAYER_STATE_ACTIVE:
        return FRAME_SWING;
    case PLAYER_STATE_RECOVERY:
        return FRAME_FOLLOW;
    case PLAYER_STATE_MOVE:
        return s_run_frames[s_rp.animation_frame & 3];
    default:
        return s_idle_frames[s_rp.animation_frame & 1];
    }
}

static void render_player(const Player *player, u8 body_id, u8 shadow_id)
{
    s16 y;
    s16 sx;
    u8 palette;

    memcpy(&s_rp, player, sizeof(Player));
    if (!s_rp.active) {
        obj_hide(body_id);
        obj_hide(shadow_id);
        obj_hide(shadow_id + 1);
        return;
    }
    y = FROM_FP(s_rp.y_fp);
    sx = render_project_x(FROM_FP(s_rp.x_fp), y);
    palette = s_rp.flash_timer ? RENDER_OBJPAL_FLASH : s_rp.palette;
    obj_put(body_id, sx - PLAYER_SPRITE_HALF_WIDTH, y - PLAYER_SPRITE_FOOT_Y,
        player_frame(), OBJ_ATTR(palette, s_rp.facing_left), 1);
    obj_put(shadow_id, sx - 8, y - 4, OBJTILE_PLAYER_SHADOW,
        OBJ_ATTR(RENDER_OBJPAL_SHADOW, 0), 0);
    obj_put(shadow_id + 1, sx, y - 4, OBJTILE_PLAYER_SHADOW,
        OBJ_ATTR(RENDER_OBJPAL_SHADOW, 1), 0);
}

static void render_players(void)
{
    const Player *a = &g_players[0];
    const Player *b = &g_players[2];

    /* The player nearer the camera (larger y) gets the higher-priority slot. */
    if (b->active && (!a->active || b->y_fp > a->y_fp)) {
        render_player(b, RENDER_OAM_BODY_FRONT, RENDER_OAM_SHADOW_FRONT);
        render_player(a, RENDER_OAM_BODY_BACK, RENDER_OAM_SHADOW_BACK);
    } else {
        render_player(a, RENDER_OAM_BODY_FRONT, RENDER_OAM_SHADOW_FRONT);
        render_player(b, RENDER_OAM_BODY_BACK, RENDER_OAM_SHADOW_BACK);
    }
}

static void render_ball(void)
{
    s16 x;
    s16 y;
    s16 z;
    s16 sx;

    if (!g_ball.active) {
        obj_hide(RENDER_OAM_BALL);
        obj_hide(RENDER_OAM_BALL_SHADOW);
        return;
    }
    x = FROM_FP(g_ball.x_fp);
    y = FROM_FP(g_ball.y_fp);
    z = FROM_FP(g_ball.z_fp);
    sx = render_project_x(x, y);
    obj_put(RENDER_OAM_BALL_SHADOW, sx - 4, y - 4, OBJTILE_BALL_SHADOW,
        OBJ_ATTR(RENDER_OBJPAL_SHADOW, 0), 0);
    obj_put(RENDER_OAM_BALL, sx - 4, y - z - 4, OBJTILE_BALL,
        OBJ_ATTR(RENDER_OBJPAL_OBJECTS, 0), 0);
}

static void render_effects(void)
{
    s16 sx;

    if (g_game_state.spark_timer) {
        sx = render_project_x(g_game_state.spark_x, g_game_state.spark_y);
        obj_put(RENDER_OAM_SPARK, sx - 4,
            g_game_state.spark_y - FROM_FP(g_ball.z_fp) - 4,
            (g_game_state.spark_timer & 2) ? OBJTILE_SPARK :
            OBJTILE_SPARK_SMALL, OBJ_ATTR(RENDER_OBJPAL_OBJECTS, 0), 0);
    } else {
        obj_hide(RENDER_OAM_SPARK);
    }

#ifdef BUILD_DEBUG
    if (g_ai.has_target) {
        obj_put(RENDER_OAM_DEBUG_MARKER,
            render_project_x(g_ai.predicted_land_x, g_ai.predicted_land_y) - 4,
            g_ai.predicted_land_y - 4, OBJTILE_SPARK_SMALL,
            OBJ_ATTR(RENDER_OBJPAL_OBJECTS, 0), 0);
    } else {
        obj_hide(RENDER_OAM_DEBUG_MARKER);
    }
#else
    obj_hide(RENDER_OAM_DEBUG_MARKER);
#endif
}

/* ---- HUD ------------------------------------------------------------- */

static const char *fault_word(u8 fault)
{
    switch (fault) {
    case GAME_FAULT_NET:
        return "NET!";
    case GAME_FAULT_KITCHEN:
        return "KITCHEN!";
    case GAME_FAULT_WRONG_BOX:
        return "FAULT!";
    case GAME_FAULT_OUT:
        return "OUT!";
    case GAME_FAULT_SECOND_BOUNCE:
        return "2 BOUNCES";
    case GAME_FAULT_EARLY_VOLLEY:
        return "VOLLEY!";
    default:
        return "POINT!";
    }
}

static u8 team_hud_palette(u8 team)
{
    return s_team_hud_palette[s_team_color[team]];
}

static void hud_draw_scores(void)
{
    hud_clear_rows(1, 3);
    hud_panel(1, 1, 6);
    hud_text(2, 2, team_hud_palette(0), "P1");
    hud_number(g_game_state.team_score[0] >= 10 ? 6 : 7, 2, HUD_PAL_WHITE,
        g_game_state.team_score[0]);
    hud_panel(9, 1, 12);
    hud_text(10, 2, HUD_PAL_YELLOW, "ARCADE RULES");
    hud_panel(23, 1, 6);
    hud_text(24, 2, team_hud_palette(1), "CPU");
    hud_number(g_game_state.team_score[1] >= 10 ? 28 : 29, 2, HUD_PAL_WHITE,
        g_game_state.team_score[1]);
    s_hud_score[0] = g_game_state.team_score[0];
    s_hud_score[1] = g_game_state.team_score[1];
}

static void hud_center_big(u8 row, u8 palette, const char *text)
{
    hud_big_text((u8)(16 - hud_big_width(text) / 2), row, palette, text);
}

/* Centered three-row panel with a B button glyph replacing '@'. */
static void hud_center_prompt(u8 row, u8 palette, const char *text)
{
    u8 len = 0;
    u8 col;
    const char *p = text;

    while (*p++)
        ++len;
    col = (u8)(16 - (len + 2) / 2);
    hud_panel(col, row, len);
    hud_text(col + 1, row + 1, palette, text);
    p = text;
    while (*p) {
        if (*p == '@')
            hud_tile(col + 1 + (u8)(p - text), row + 1,
                HUD_TILE_BUTTON_B | (HUD_PAL_BUTTON << 10) | 0x2000);
        ++p;
    }
}

static u8 hud_scene_key(void)
{
    u8 phase = g_game_state.phase;

    if (g_game_state.paused)
        return 0x80;
    if (phase == GAME_PHASE_SERVE_TOSS || phase == GAME_PHASE_SERVE_FAULT)
        phase = GAME_PHASE_LIVE;
    if (phase == GAME_PHASE_SERVE_SETUP)
        return phase | (g_game_state.serving_team << 4);
    if (phase == GAME_PHASE_POINT_RESULT || phase == GAME_PHASE_MATCH_RESULT)
        return phase | (g_game_state.point_winner << 4) |
            ((g_game_state.last_fault & 7) << 5);
    return phase;
}

static void hud_draw_scene(u8 key)
{
    u8 winner = g_game_state.point_winner;

    hud_clear_rows(CENTER_BIG_ROW, 7);
    hud_clear_rows(BANNER_ROW - 1, 4);

    if (key == 0x80) {
        hud_center_prompt(CENTER_PANEL_ROW, HUD_PAL_WHITE, "PAUSED");
        return;
    }
    switch (g_game_state.phase) {
    case GAME_PHASE_SERVE_SETUP:
        if (g_game_state.serving_team == 0)
            hud_center_prompt(BANNER_ROW, HUD_PAL_WHITE, "PRESS @ TO SERVE");
        else
            hud_center_prompt(BANNER_ROW, HUD_PAL_WHITE, "CPU TO SERVE");
        break;
    case GAME_PHASE_POINT_RESULT:
        hud_center_big(CENTER_BIG_ROW, HUD_PAL_YELLOW,
            fault_word(g_game_state.last_fault));
        hud_center_prompt(CENTER_PANEL_ROW, team_hud_palette(winner),
            winner == 0 ? "P1 POINT" : "CPU POINT");
        break;
    case GAME_PHASE_MATCH_RESULT:
        hud_center_big(CENTER_BIG_ROW, HUD_PAL_YELLOW,
            g_game_state.match_winner == 0 ? "YOU WIN!" : "CPU WINS");
        hud_center_prompt(BANNER_ROW, HUD_PAL_WHITE, "PRESS @ TO REMATCH");
        break;
    default:
        break;
    }
}

#if defined(BUILD_DEBUG) || defined(BUILD_HARDWARE_TEST)
extern u16 g_overrun_frames;
static const char s_hex[] = "0123456789ABCDEF";
static char s_debug_line[20];

/* Hex only: 816-tcc division is far too slow for a per-frame overlay. */
static void debug_hex(u8 at, u16 value)
{
    s_debug_line[at] = s_hex[(value >> 8) & 15];
    s_debug_line[at + 1] = s_hex[(value >> 4) & 15];
    s_debug_line[at + 2] = s_hex[value & 15];
}

static void hud_draw_debug(void)
{
    static const char req[3] = { '-', 'S', 'R' };

    debug_hex(0, (u16)FROM_FP(g_players[0].x_fp));
    s_debug_line[3] = ',';
    debug_hex(4, (u16)FROM_FP(g_players[0].y_fp));
    s_debug_line[7] = ' ';
    s_debug_line[8] = req[g_game_state.required_bounce % 3];
    s_debug_line[9] = '0' + g_game_state.phase;
    s_debug_line[10] = ' ';
    s_debug_line[11] = 'O';
    debug_hex(12, g_overrun_frames);
    s_debug_line[15] = 0;
    hud_text(0, 0, HUD_PAL_DIM, s_debug_line);
}
#endif

static void render_hud(void)
{
    u8 key;

    if (!s_hud_valid ||
        s_hud_score[0] != g_game_state.team_score[0] ||
        s_hud_score[1] != g_game_state.team_score[1])
        hud_draw_scores();

    key = hud_scene_key();
    if (!s_hud_valid || key != s_hud_scene) {
        hud_draw_scene(key);
        s_hud_scene = key;
    }
    s_hud_valid = 1;

#if defined(BUILD_DEBUG) || defined(BUILD_HARDWARE_TEST)
    hud_draw_debug();
    if (key == 0x80 && !s_hud_debug_build_shown) {
        hud_text(0, 27, HUD_PAL_DIM, BUILD_ID_STRING);
        s_hud_debug_build_shown = 1;
    } else if (key != 0x80 && s_hud_debug_build_shown) {
        hud_clear_rows(27, 1);
        s_hud_debug_build_shown = 0;
    }
#endif
}

void render_prepare(void)
{
    render_players();
    PROFILE_MARK(2);
    render_ball();
    render_effects();
    PROFILE_MARK(3);
    render_hud();
}

void render_commit(void)
{
    if (s_palette_dirty)
        render_upload_team_palettes();
    hud_commit();
}

void render_initialize(void)
{
    u8 i;

    setScreenOff();
    oamInit();
    for (i = 0; i < 128; ++i)
        obj_hide(i);

    dmaCopyVram((u8 *)&court_tiles, RENDER_BG1_TILES_ADDR,
        (u16)(&court_tiles_end - &court_tiles));
    dmaCopyVram((u8 *)&court_map, RENDER_BG1_MAP_ADDR,
        (u16)(&court_map_end - &court_map));
    dmaCopyVram((u8 *)&hud_tiles, RENDER_BG3_TILES_ADDR,
        (u16)(&hud_tiles_end - &hud_tiles));
    dmaCopyVram((u8 *)&obj_tiles, RENDER_OBJ_TILES_ADDR,
        (u16)(&obj_tiles_end - &obj_tiles));

    dmaCopyCGram((u8 *)&hud_palette, 0,
        (u16)(&hud_palette_end - &hud_palette));
    dmaCopyCGram((u8 *)&court_palette, 32,
        (u16)(&court_palette_end - &court_palette));
    dmaCopyCGram((u8 *)&objects_palette,
        CGRAM_OBJ + RENDER_OBJPAL_OBJECTS * 16, 32);
    dmaCopyCGram((u8 *)&objects_palette + 32,
        CGRAM_OBJ + RENDER_OBJPAL_SHADOW * 16, 32);
    dmaCopyCGram((u8 *)&player_palettes + TEAM_FLASH_PALETTE * 32,
        CGRAM_OBJ + RENDER_OBJPAL_FLASH * 16, 32);
    render_upload_team_palettes();

    bgSetGfxPtr(0, RENDER_BG1_TILES_ADDR);
    bgSetMapPtr(0, RENDER_BG1_MAP_ADDR, SC_32x32);
    bgSetGfxPtr(2, RENDER_BG3_TILES_ADDR);
    bgSetMapPtr(2, RENDER_BG3_MAP_ADDR, SC_32x32);
    oamInitGfxAttr(RENDER_OBJ_TILES_ADDR, OBJ_SIZE8_L32);

    setMode(BG_MODE1, BG3_MODE1_PRIORITY_HIGH);
    bgSetEnable(0);
    bgSetDisable(1);
    bgSetEnable(2);
    /* Shadows: OBJ palette 4 pixels are black and averaged with BG1 on the
     * subscreen, i.e. half-brightness court under every shadow. */
    bgSetEnableSub(0);
    setColorEffect(0x02, 0x50);

    hud_initialize();
    s_hud_valid = 0;
    render_prepare();
    hud_commit();
    setScreenOn();
}
