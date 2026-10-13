#include <snes.h>
#include <string.h>
#include "ai.h"
#include "asset_frames.h"
#include "ball.h"
#include "court.h"
#include "fx.h"
#include "game.h"
#include "hud.h"
#include "hud_tiles.h"
#include "input.h"
#include "match.h"
#include "player.h"
#include "profile.h"
#include "projection.h"
#include "render.h"
#include "render_internal.h"
#include "hud_scene.h"
#include "serve.h"


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

/* OAM writer inputs (src/asm/oam_write.asm); declared in render_internal.h. */
u8 oam_in_id;
s16 oam_in_x;
s16 oam_in_y;
u16 oam_in_tile;
u8 oam_in_attr;
u8 oam_in_large;
u8 oam_tmp_bits;
u8 oam_tmp_mask;

void render_obj_hide(u8 id)
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
        render_obj_hide(body_id);
        render_obj_hide(shadow_id);
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

void render_reset_bodies(void)
{
    s_active_bodies = 0xff;
}

void render_request_palettes(void)
{
    s_palette_dirty = 1;
}

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
                render_obj_hide(RENDER_OAM_BODIES + j);
            if (s_depth[j] == -32767)
                render_obj_hide(RENDER_OAM_SHADOWS + j);
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
        render_obj_hide(RENDER_OAM_BALL);
        render_obj_hide(RENDER_OAM_BALL_SHADOW);
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

    if (g_fx.spark_timer) {
        sx = projection_x(g_fx.spark_x, g_fx.spark_y);
        obj_put(RENDER_OAM_SPARK, sx - 4,
            g_fx.spark_y - FROM_FP(g_ball.z_fp) - 4,
            (g_fx.spark_timer & 2) ? OBJTILE_SPARK :
            OBJTILE_SPARK_SMALL, OBJ_ATTR(RENDER_OBJPAL_OBJECTS, 0), 0);
    } else {
        render_obj_hide(RENDER_OAM_SPARK);
    }

#ifdef BUILD_DEBUG
    if (g_ai.has_target) {
        obj_put(RENDER_OAM_DEBUG_MARKER,
            projection_x(g_ai.predicted_land_x, g_ai.predicted_land_y) - 4,
            g_ai.predicted_land_y - 4, OBJTILE_SPARK_SMALL,
            OBJ_ATTR(RENDER_OBJPAL_OBJECTS, 0), 0);
    } else {
        render_obj_hide(RENDER_OAM_DEBUG_MARKER);
    }
#else
    render_obj_hide(RENDER_OAM_DEBUG_MARKER);
#endif
}

void render_prepare(void)
{
#ifdef RENDER_DIAGNOSTICS
    if (render_stress_active()) {
        diag_stress_render();
        return;
    }
#endif
    render_players();
    PROFILE_MARK(2);
    render_ball();
    render_effects();
    PROFILE_MARK(3);
    hud_scene_update();
}

void render_commit(void)
{
    /* Range/time-over flags describe the frame just drawn; they clear at
     * the end of VBlank. */
    if (REG_STAT77_FLAGS & 0xC0)
        ++g_obj_overflow_frames;
#ifdef RENDER_DIAGNOSTICS
    if (s_palette_dirty && diag_upload_palettes())
        s_palette_dirty = 0;
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
    s_active_bodies = 0xff;
    g_obj_overflow_frames = 0;
    hud_scene_initialize();
#ifdef RENDER_DIAGNOSTICS
    diag_initialize();
#endif

    setScreenOff();
    oamInit();
    for (i = 0; i < 128; ++i)
        render_obj_hide(i);

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
    hud_scene_invalidate();
    render_prepare();
    hud_commit_all();
    setScreenOn();
}
