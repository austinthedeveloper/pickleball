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
#include "match.h"
#include "player.h"
#include "profile.h"
#include "projection.h"
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
extern char objects_palette, objects_palette_end;

static const u16 s_idle_frames[2] = { FRAME_IDLE0, FRAME_IDLE1 };
static const u16 s_run_frames[4] = {
    FRAME_RUN0, FRAME_RUN1, FRAME_RUN2, FRAME_RUN3
};
static u16 s_player_palettes[2][16];
static u16 s_hud_team_colors[2][2];
static u8 s_palette_dirty;

/* Last HUD inputs, so the BG3 map is only rebuilt when something changes. */
static u8 s_hud_score[2];
static u8 s_hud_scene;
static u8 s_hud_valid;
#if defined(BUILD_DEBUG) || defined(BUILD_HARDWARE_TEST)
static u8 s_hud_debug_build_shown;
#endif

void render_apply_match_config(void)
{
    u8 team;

    match_compose_palette(0, s_player_palettes[0]);
    match_compose_palette(2, s_player_palettes[1]);
    for (team = 0; team < 2; ++team) {
        s_hud_team_colors[team][0] = g_pal_hud[match_team_palette(team)][0];
        s_hud_team_colors[team][1] = g_pal_hud[match_team_palette(team)][1];
    }
    s_palette_dirty = 1;
}

static void render_upload_palettes(void)
{
    u8 team;

    dmaCopyCGram((u8 *)s_player_palettes[0],
        CGRAM_OBJ + RENDER_OBJPAL_PLAYER_A * 16, 32);
    dmaCopyCGram((u8 *)s_player_palettes[1],
        CGRAM_OBJ + RENDER_OBJPAL_PLAYER_B * 16, 32);
    /* BG3 palettes 2/3 colours 1-2 are the team label colours. */
    for (team = 0; team < 2; ++team)
        dmaCopyCGram((u8 *)s_hud_team_colors[team],
            (HUD_PAL_TEAM_LEFT + team) * 4 + 1, 4);
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
        return;
    }
    y = FROM_FP(s_rp.y_fp);
    sx = projection_x(FROM_FP(s_rp.x_fp), y);
    palette = s_rp.flash_timer ? RENDER_OBJPAL_FLASH : s_rp.palette;
    obj_put(body_id, sx - PLAYER_SPRITE_HALF_WIDTH, y - PLAYER_SPRITE_FOOT_Y,
        player_frame(), OBJ_ATTR(palette, s_rp.facing_left), 1);
    obj_put(shadow_id, sx - 4, y - 4, OBJTILE_PLAYER_SHADOW,
        OBJ_ATTR(RENDER_OBJPAL_SHADOW, 0), 0);
}

/*
 * ADR-012: bodies take OAM 2-5 nearest-first (larger y wins; ties by slot) so
 * the nearer player draws on top; shadows keep fixed per-slot IDs 6-9.
 */
static s16 s_depth[MAX_PLAYERS];
static u8 s_active_bodies = 0xff;

static void render_players(void)
{
    u8 order[MAX_PLAYERS];
    u8 i;
    u8 j;
    u8 key;

    /* Constant indices: 816-tcc multiplies for every variable g_players[i]. */
    s_depth[0] = g_players[0].active ? g_players[0].y_fp : -32767;
    s_depth[1] = g_players[1].active ? g_players[1].y_fp : -32767;
    s_depth[2] = g_players[2].active ? g_players[2].y_fp : -32767;
    s_depth[3] = g_players[3].active ? g_players[3].y_fp : -32767;
    for (i = 0; i < MAX_PLAYERS; ++i) {
        key = i;
        j = i;
        while (j > 0 && s_depth[key] > s_depth[order[j - 1]]) {
            order[j] = order[j - 1];
            --j;
        }
        order[j] = key;
    }
    /* Inactive slots sort last; stop there and only re-hide their objects
     * when the active count changes. */
    for (i = 0; i < MAX_PLAYERS && s_depth[order[i]] != -32767; ++i)
        render_player(&g_players[order[i]], RENDER_OAM_BODIES + i,
            RENDER_OAM_SHADOWS + order[i]);
    if (i != s_active_bodies) {
        s_active_bodies = i;
        for (j = 0; j < MAX_PLAYERS; ++j) {
            if (j >= i)
                obj_hide(RENDER_OAM_BODIES + j);
            if (s_depth[j] == -32767)
                obj_hide(RENDER_OAM_SHADOWS + j);
        }
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
    sx = projection_x(x, y);
    obj_put(RENDER_OAM_BALL_SHADOW, sx - 4, y - 4, OBJTILE_BALL_SHADOW,
        OBJ_ATTR(RENDER_OBJPAL_SHADOW, 0), 0);
    obj_put(RENDER_OAM_BALL, sx - 4, y - z - 4, OBJTILE_BALL,
        OBJ_ATTR(RENDER_OBJPAL_OBJECTS, 0), 0);
}

static void render_effects(void)
{
    s16 sx;

    if (g_game_state.spark_timer) {
        sx = projection_x(g_game_state.spark_x, g_game_state.spark_y);
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
            projection_x(g_ai.predicted_land_x, g_ai.predicted_land_y) - 4,
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
    return team ? HUD_PAL_TEAM_RIGHT : HUD_PAL_TEAM_LEFT;
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
                HUD_TILE_BUTTON_B | (HUD_PAL_YELLOW << 10) | 0x2000);
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
    hud_text(0, 0, HUD_PAL_WHITE, s_debug_line);
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
        hud_text(0, 27, HUD_PAL_WHITE, BUILD_ID_STRING);
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
        render_upload_palettes();
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
    dmaCopyCGram((u8 *)g_pal_flash, CGRAM_OBJ + RENDER_OBJPAL_FLASH * 16, 32);
    /* Match palettes are composed and uploaded inside this forced blank. */
    render_apply_match_config();
    render_upload_palettes();

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
