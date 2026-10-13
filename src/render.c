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
#define TITLE_ROW 7

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
u16 g_obj_overflow_frames;

#define REG_STAT77_FLAGS (*(vuint8 *)0x213E)

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

/* M3-OBJ-02: OAM ranges from docs/03 cannot overlap; a violation fails the
 * build with a negative array size. */
typedef char oam_bodies_after_ball[
    (RENDER_OAM_BODIES > RENDER_OAM_BALL_SHADOW) ? 1 : -1];
typedef char oam_shadows_after_bodies[
    (RENDER_OAM_SHADOWS >= RENDER_OAM_BODIES + MAX_PLAYERS) ? 1 : -1];
typedef char oam_effects_after_shadows[
    (RENDER_OAM_SPARK >= RENDER_OAM_SHADOWS + MAX_PLAYERS) ? 1 : -1];
typedef char oam_effects_within_reserve[
    (RENDER_OAM_DEBUG_MARKER < RENDER_OAM_SPARK + 4) ? 1 : -1];
typedef char oam_accessories_after_effects[
    (RENDER_OAM_ACCESSORIES >= RENDER_OAM_SPARK + 4) ? 1 : -1];
typedef char oam_hidden_after_accessories[
    (RENDER_OAM_FIRST_HIDDEN >= RENDER_OAM_ACCESSORIES + MAX_PLAYERS) ? 1 : -1];

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

/* A macro, not a function: 816-tcc's six stack arguments cost more than the
 * OAM write itself. */
#define obj_put(id, x, y, tile, attr, large) \
    do { \
        oam_in_id = (id); \
        oam_in_x = (x); \
        oam_in_y = (y); \
        oam_in_tile = (tile); \
        oam_in_attr = (attr); \
        oam_in_large = (large); \
        oam_write(); \
    } while (0)

static void obj_hide(u8 id)
{
    oam_in_id = id;
    oam_in_x = 0;
    oam_in_y = 240;
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
static u8 s_active_bodies;

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

#define HUD_KEY_TITLE 0x40
#define HUD_KEY_TITLE_BLINK 0x41

static u8 hud_scene_key(void)
{
    u8 phase = g_game_state.phase;

    if (g_attract)
        return (g_game_state.frame_count & 32) ? HUD_KEY_TITLE_BLINK :
            HUD_KEY_TITLE;

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

    if (key == HUD_KEY_TITLE || key == HUD_KEY_TITLE_BLINK) {
        /* Only the prompt blinks; the logo is drawn once. */
        if (!s_hud_valid ||
            (s_hud_scene != HUD_KEY_TITLE && s_hud_scene != HUD_KEY_TITLE_BLINK)) {
            hud_clear_rows(CENTER_BIG_ROW - 2, 9);
            hud_center_big(TITLE_ROW, HUD_PAL_YELLOW, "PICKLEBALL");
            hud_center_big(TITLE_ROW + 2, HUD_PAL_YELLOW, "'94");
        }
        hud_clear_rows(BANNER_ROW - 1, 4);
        if (key == HUD_KEY_TITLE)
            hud_center_prompt(BANNER_ROW, HUD_PAL_WHITE, "PRESS START");
        return;
    }
    if (s_hud_scene == HUD_KEY_TITLE || s_hud_scene == HUD_KEY_TITLE_BLINK)
        hud_clear_rows(CENTER_BIG_ROW - 2, 9);
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
        hud_text(0, 26, HUD_PAL_WHITE,
            g_multitap_present ? "MULTITAP: YES" : "MULTITAP: NO");
        hud_text(0, 27, HUD_PAL_WHITE, BUILD_ID_STRING);
        s_hud_debug_build_shown = 1;
    } else if (key != 0x80 && s_hud_debug_build_shown) {
        hud_clear_rows(26, 2);
        s_hud_debug_build_shown = 0;
    }
#endif
}

#ifdef RENDER_DIAGNOSTICS
/*
 * OBJ stress scene (M3-OBJ-01/02, M3-ASSET-03). Every object below overlaps
 * screen row 148, the worst planned row from ADR-014:
 *   4 bodies x 4 slivers (OAM 2-5)          16
 *   4 player shadows (OAM 6-9)               4
 *   4 accessory placeholders (OAM 14-17)     4
 *   ball + ball shadow (OAM 0-1)             2
 *   4 impact effects (OAM 10-13)             4
 *                                    total  30 of 34
 * Bodies use OBJ palettes 0-3 loaded with all four composed team palettes
 * (one shared tile set), rotate palettes every 64 frames, flip every 32,
 * and step through every frame slot. The PPU's range/time-over flags are
 * latched in g_obj_overflow_frames.
 */
#define STRESS_ROW 150
static u8 s_stress;
static u8 s_stress_timer;
static u16 s_stress_palettes[4][16];
static const u16 s_stress_frames[11] = {
    FRAME_IDLE0, FRAME_IDLE1, FRAME_RUN0, FRAME_RUN1, FRAME_RUN2, FRAME_RUN3,
    FRAME_WINDUP, FRAME_SWING, FRAME_FOLLOW, FRAME_TOSS, FRAME_CHEER
};
static const s16 s_stress_x[4] = { 40, 96, 152, 208 };

u8 render_stress_active(void)
{
    return s_stress;
}

void render_stress_toggle(void)
{
    u8 i;

    s_stress = !s_stress;
    s_stress_timer = 0;
    for (i = 0; i < RENDER_OAM_FIRST_HIDDEN; ++i)
        obj_hide(i);
    for (i = 0; i < 4; ++i)
        match_compose_parts(i, 0, i, s_stress_palettes[i]);
    s_palette_dirty = 1;
    s_active_bodies = 0xff;
    s_hud_valid = 0;
    hud_clear_rows(4, 24);
    g_obj_overflow_frames = 0;
}

static void render_stress(void)
{
    u8 i;
    u8 pal;
    u8 frame;
    s16 x;

    ++s_stress_timer;
    for (i = 0; i < 4; ++i) {
        x = s_stress_x[i];
        pal = (u8)((i + (s_stress_timer >> 6)) & 3);
        frame = (u8)(((s_stress_timer >> 3) + i * 3) % 11);
        obj_put(RENDER_OAM_BODIES + i, x - PLAYER_SPRITE_HALF_WIDTH,
            STRESS_ROW - PLAYER_SPRITE_FOOT_Y, s_stress_frames[frame],
            OBJ_ATTR(pal, ((s_stress_timer >> 5) + i) & 1), 1);
        obj_put(RENDER_OAM_SHADOWS + i, x - 4, STRESS_ROW - 4,
            OBJTILE_PLAYER_SHADOW, OBJ_ATTR(RENDER_OBJPAL_SHADOW, 0), 0);
        obj_put(RENDER_OAM_ACCESSORIES + i, x + 10, STRESS_ROW - 6,
            OBJTILE_SPARK_SMALL, OBJ_ATTR(pal, 0), 0);
        obj_put(RENDER_OAM_SPARK + i, x + 18, STRESS_ROW - 6,
            (s_stress_timer & 4) ? OBJTILE_SPARK : OBJTILE_SPARK_SMALL,
            OBJ_ATTR(RENDER_OBJPAL_OBJECTS, 0), 0);
    }
    obj_put(RENDER_OAM_BALL, 120, STRESS_ROW - 6, OBJTILE_BALL,
        OBJ_ATTR(RENDER_OBJPAL_OBJECTS, 0), 0);
    obj_put(RENDER_OAM_BALL_SHADOW, 124, STRESS_ROW - 4, OBJTILE_BALL_SHADOW,
        OBJ_ATTR(RENDER_OBJPAL_SHADOW, 0), 0);

    if (!s_hud_valid) {
        hud_clear_rows(4, 24);
        hud_center_prompt(5, HUD_PAL_YELLOW, "OBJ STRESS 30/34");
        hud_text(2, 22, HUD_PAL_WHITE, "4 PALETTES, 1 TILE SET");
        hud_text(2, 24, HUD_PAL_WHITE, "SELECT: BACK TO GAME");
        s_hud_valid = 1;
    }
    hud_text(2, 9, HUD_PAL_WHITE,
        g_obj_overflow_frames ? "OBJ OVERFLOW: YES" : "OBJ OVERFLOW: NO ");
    hud_text(2, 10, HUD_PAL_WHITE,
        g_multitap_present ? "MULTITAP: YES" : "MULTITAP: NO ");
}
#endif

void render_prepare(void)
{
#ifdef RENDER_DIAGNOSTICS
    if (s_stress) {
        render_stress();
        return;
    }
#endif
    render_players();
    PROFILE_MARK(2);
    render_ball();
    render_effects();
    PROFILE_MARK(3);
    render_hud();
}

void render_commit(void)
{
    /* Range/time-over flags describe the frame just drawn; they clear at
     * the end of VBlank. */
    if (REG_STAT77_FLAGS & 0xC0)
        ++g_obj_overflow_frames;
#ifdef RENDER_DIAGNOSTICS
    if (s_palette_dirty && s_stress) {
        dmaCopyCGram((u8 *)s_stress_palettes, CGRAM_OBJ, 4 * 32);
        s_palette_dirty = 0;
    } else if (s_palette_dirty && !s_stress) {
        /* Leaving the stress scene restores objects and flash too. */
        dmaCopyCGram((u8 *)&objects_palette,
            CGRAM_OBJ + RENDER_OBJPAL_OBJECTS * 16, 32);
        dmaCopyCGram((u8 *)g_pal_flash,
            CGRAM_OBJ + RENDER_OBJPAL_FLASH * 16, 32);
    }
#endif
    if (s_palette_dirty)
        render_upload_palettes();
    hud_commit();
}

void render_initialize(void)
{
    u8 i;

    /* RAM is not cleared at power-on (real hardware or emulators that
     * randomize it), so every piece of renderer state is set explicitly. */
    s_palette_dirty = 0;
    s_hud_valid = 0;
    s_hud_scene = 0;
    s_active_bodies = 0xff;
    g_obj_overflow_frames = 0;
#ifdef RENDER_DIAGNOSTICS
    s_hud_debug_build_shown = 0;
    s_stress = 0;
    s_stress_timer = 0;
#endif

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
    hud_commit_all();
    setScreenOn();
}
