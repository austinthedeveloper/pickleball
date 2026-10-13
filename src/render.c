#include <snes.h>
#include <string.h>
#include "ai.h"
#include "asset_frames.h"
#include "ball.h"
#include "court.h"
#include "court_style.h"
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
#include "save.h"
#include "serve.h"


extern char court_tiles, court_tiles_end;
extern char court_map, court_map_end;
extern char court_palette, court_palette_end;
extern char hud_tiles, hud_tiles_end;
extern char hud_palette, hud_palette_end;
extern char obj_tiles, obj_tiles_end;
extern char objects_palette, objects_palette_end;

u16 g_obj_overflow_frames;

/* Inputs for ram_copy (src/asm/ram_copy.asm). */
u16 ram_copy_src;
u16 ram_copy_dst;
u16 ram_copy_len;

#define REG_STAT77_FLAGS (*(vuint8 *)0x213E)

static u16 s_player_palettes[MAX_PLAYERS][16];
static u16 s_hud_team_colors[2][2];
static u8 s_palette_dirty;
/* BG1 palette 2 with the saved court colours (ADR-022). */
static u16 s_ground_palette[COURT_GROUND_COLORS];
static u8 s_ground_dirty;
static u8 s_doubles;
static u8 s_applied_mode;

static void render_load_court(void);

u8 render_match_mode_applied(void)
{
    return s_applied_mode;
}
u8 g_objpal_objects;
u8 g_objpal_flash;
u8 g_objpal_shadow;

/* Composes player palettes and picks the OBJ layout for the match mode
 * (ADR-018); uploads happen in render_commit's VBlank. */
void render_apply_match_config(void)
{
    u8 team;
    u8 slot;

    s_doubles = g_match_config.mode == MATCH_DOUBLES;
    s_applied_mode = g_match_config.mode;
    g_objpal_objects = s_doubles ? RENDER_OBJPAL_DOUBLES_OBJECTS :
        RENDER_OBJPAL_OBJECTS;
    g_objpal_flash = s_doubles ? RENDER_OBJPAL_DOUBLES_FLASH :
        RENDER_OBJPAL_FLASH;
    g_objpal_shadow = s_doubles ? RENDER_OBJPAL_DOUBLES_SHADOW :
        RENDER_OBJPAL_SHADOW;
    for (slot = 0; slot < MAX_PLAYERS; ++slot)
        if (match_slot_active(slot))
            match_compose_palette(slot, s_player_palettes[slot]);
    /* Ponytails are written only for female players; park the rest. */
    for (slot = 0; slot < MAX_PLAYERS; ++slot)
        if (!match_slot_active(slot) ||
                g_match_config.variant[slot] != PLAYER_VARIANT_FEMALE)
            render_obj_hide(RENDER_OAM_ACCESSORIES + slot);
    for (team = 0; team < 2; ++team) {
        s_hud_team_colors[team][0] = match_team_hud_colors(team)[0];
        s_hud_team_colors[team][1] = match_team_hud_colors(team)[1];
    }
    s_palette_dirty = 1;
}

/* Every OBJ palette of the active layout, the HUD team colours, and the
 * shadow colour math (on for singles, off for doubles). */
static void render_upload_palettes(void)
{
    u8 team;

    if (s_doubles) {
        dmaCopyCGram((u8 *)s_player_palettes, CGRAM_OBJ, 4 * 32);
        dmaCopyCGram((u8 *)&objects_palette + 64,
            CGRAM_OBJ + g_objpal_shadow * 16, 32);
        setColorEffect(0x02, 0x00);
    } else {
        dmaCopyCGram((u8 *)s_player_palettes[0], CGRAM_OBJ, 32);
        dmaCopyCGram((u8 *)s_player_palettes[2], CGRAM_OBJ + 16, 32);
        dmaCopyCGram((u8 *)&objects_palette + 32,
            CGRAM_OBJ + g_objpal_shadow * 16, 32);
        /* Shadows: OBJ palette 4 pixels are black and averaged with BG1 on
         * the subscreen, i.e. half-brightness court under every shadow. */
        setColorEffect(0x02, 0x50);
    }
    dmaCopyCGram((u8 *)&objects_palette, CGRAM_OBJ + g_objpal_objects * 16,
        32);
    dmaCopyCGram((u8 *)g_pal_flash, CGRAM_OBJ + g_objpal_flash * 16, 32);
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




/*
 * ADR-012: bodies take OAM 2-5 nearest-first (larger y wins; ties by slot) so
 * the nearer player draws on top; shadows keep fixed per-slot IDs 6-9.
 */
/* Working variables for src/asm/render_bodies.asm (all words). */
s16 rb_depth[MAX_PLAYERS];
u16 rb_order[MAX_PLAYERS];
u16 rb_i;
u16 rb_key;
s16 rb_kd;
u16 rb_tmp;
u16 rb_slot;
s16 rb_y;
s16 rb_d;
u16 rb_sign;
s16 rb_prod;
s16 rb_sx;
u16 rb_attr;
u16 rb_count;
u16 rb_frame;
extern void render_bodies(void);

static u8 s_active_bodies;

void render_reset_bodies(void)
{
    s_active_bodies = 0xff;
}

void render_apply_court_colors(void)
{
    court_style_compose(s_ground_palette, (const u16 *)&court_palette,
        g_save.settings.court);
    s_ground_dirty = 1;
}

void render_request_palettes(void)
{
    s_palette_dirty = 1;
}

/*
 * ADR-012: bodies take OAM 2-5 nearest-first and shadows keep per-slot IDs
 * 6-9. The per-player work is in src/asm/render_bodies.asm (four players in
 * doubles did not fit the frame in 816-tcc C); here only the rare re-hide
 * when the number of active players changes.
 */
static void render_players(void)
{
    u8 j;

    render_bodies();
    if (rb_count != s_active_bodies) {
        s_active_bodies = (u8)rb_count;
        for (j = 0; j < MAX_PLAYERS; ++j) {
            if (j >= rb_count)
                render_obj_hide(RENDER_OAM_BODIES + j);
            if (rb_depth[j] == -32767)
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
        OBJ_ATTR(g_objpal_shadow, 0), 0);
    obj_put(RENDER_OAM_BALL, sx - 4, y - z - 4, OBJTILE_BALL,
        OBJ_ATTR(g_objpal_objects, 0), 0);
}

static void render_effects(void)
{
    s16 sx;

    if (g_fx.spark_timer) {
        sx = projection_x(g_fx.spark_x, g_fx.spark_y);
        obj_put(RENDER_OAM_SPARK, sx - 4,
            g_fx.spark_y - FROM_FP(g_ball.z_fp) - 4,
            (g_fx.spark_style == FX_SPARK_BIG ||
             (g_fx.spark_style == FX_SPARK_ALTERNATE &&
              (g_fx.spark_timer & 2))) ? OBJTILE_SPARK : OBJTILE_SPARK_SMALL, OBJ_ATTR(g_objpal_objects, 0), 0);
    } else {
        render_obj_hide(RENDER_OAM_SPARK);
    }

#ifdef BUILD_DEBUG
    if (g_ai.has_target) {
        obj_put(RENDER_OAM_DEBUG_MARKER,
            projection_x(g_ai.predicted_land_x, g_ai.predicted_land_y) - 4,
            g_ai.predicted_land_y - 4, OBJTILE_SPARK_SMALL,
            OBJ_ATTR(g_objpal_objects, 0), 0);
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
    if (s_ground_dirty) {
        dmaCopyCGram((u8 *)s_ground_palette, 32, 32);
        s_ground_dirty = 0;
    }
    hud_commit();
}

void render_initialize(void)
{
    u8 i;

    /* RAM is not cleared at power-on (real hardware or emulators that
     * randomize it), so every piece of renderer state is set explicitly. */
    s_palette_dirty = 0;
    s_ground_dirty = 0;
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

    dmaCopyVram((u8 *)&hud_tiles, RENDER_BG3_TILES_ADDR,
        (u16)(&hud_tiles_end - &hud_tiles));
    dmaCopyVram((u8 *)&obj_tiles, RENDER_OBJ_TILES_ADDR,
        (u16)(&obj_tiles_end - &obj_tiles));
    dmaCopyCGram((u8 *)&hud_palette, 0,
        (u16)(&hud_palette_end - &hud_palette));

    bgSetGfxPtr(0, RENDER_BG1_TILES_ADDR);
    bgSetMapPtr(0, RENDER_BG1_MAP_ADDR, SC_32x32);
    bgSetGfxPtr(2, RENDER_BG3_TILES_ADDR);
    bgSetMapPtr(2, RENDER_BG3_MAP_ADDR, SC_32x32);
    oamInitGfxAttr(RENDER_OBJ_TILES_ADDR, OBJ_SIZE8_L32);

    setMode(BG_MODE1, BG3_MODE1_PRIORITY_HIGH);
    bgSetEnable(0);
    bgSetEnable(2);
    /* Shadows: OBJ palette 4 pixels are black and averaged with BG1 on the
     * subscreen, i.e. half-brightness court under every shadow. */
    bgSetEnableSub(0);

    render_load_court();
    setScreenOn();
}

/* Court scene into VRAM/CGRAM and the match HUD; forced blank only. Also
 * how the match returns from the Player Select screen. */
static void render_load_court(void)
{
    dmaCopyVram((u8 *)&court_tiles, RENDER_BG1_TILES_ADDR,
        (u16)(&court_tiles_end - &court_tiles));
    dmaCopyVram((u8 *)&court_map, RENDER_BG1_MAP_ADDR,
        (u16)(&court_map_end - &court_map));
    dmaCopyCGram((u8 *)&court_palette, 32,
        (u16)(&court_palette_end - &court_palette));
    render_apply_court_colors();
    dmaCopyCGram((u8 *)s_ground_palette, 32, 32);
    s_ground_dirty = 0;
    bgSetDisable(1);
    /* Match palettes are composed and uploaded inside this forced blank. */
    render_apply_match_config();
    render_upload_palettes();
    render_reset_bodies();

    hud_initialize();
    hud_scene_invalidate();
    render_prepare();
    hud_commit_all();
}

void render_open_match(void)
{
    u8 i;

    render_fade(1);
    setScreenOff();
    for (i = 0; i < 128; ++i)
        render_obj_hide(i);
    render_load_court();
    render_fade(0);
}
