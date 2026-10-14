#include <snes.h>
#include <string.h>
#define VENUE_DATA_DEFINE
#include "ai.h"
#include "asset_frames.h"
#include "ball.h"
#include "court.h"
#include "audio_events.h"
#include "court_style.h"
#include "fx.h"
#include "game.h"
#include "hud.h"
#include "hud_tiles.h"
#include "input.h"
#include "match.h"
#include "modifiers.h"
#include "player.h"
#include "profile.h"
#include "projection.h"
#include "render.h"
#include "render_internal.h"
#include "hud_scene.h"
#include "save.h"
#include "practice.h"
#include "serve.h"


/* The ground every venue shares (ADR-024); venues.h has the rest. */
extern char court_tiles, court_tiles_end;
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
/* Palette 2 as painted, copied from ROM when the court loads: composing
 * from WRAM is far cheaper for 816-tcc than reading ROM per colour, and a
 * live recolour (COURT page) must fit a busy title frame. */
static u16 s_painted_ground[COURT_GROUND_COLORS];
/* A recolour waits for a frame with time left (the title demo alone
 * reaches ~200 scanlines on its heavy frames), but at most a few frames. */
static u8 s_ground_pending;
static u8 s_ground_waits;
#define RENDER_RECOLOR_SCANLINE_LIMIT 110
#define RENDER_RECOLOR_MAX_WAITS 3
static u8 s_ground_dirty;
/* Venue on screen (ADR-024). */
static u8 s_venue;
/* The LAVA kitchen is on the ground palette (ADR-033). */
static u8 s_lava_shown;
u8 g_render_busy;
/* A LAVA match, or its preview on the title's MODIFIERS page. A macro: it
 * runs on a new match's frame, which is near the budget. */
#define RENDER_LAVA_WANTED() \
    (g_attract ? (g_title_page == TITLE_PAGE_MODIFIERS && \
                  (g_save.settings.modifiers & MODIFIER_LAVA) != 0) : \
     (g_match_config.modifiers & MODIFIER_LAVA) != 0)
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
    /* Back to the title from a LAVA match, or into one: the kitchen. */
    if (s_lava_shown != RENDER_LAVA_WANTED())
        s_ground_pending = 1;
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
typedef char oam_machine_after_accessories[
    (RENDER_OAM_MACHINE >= RENDER_OAM_ACCESSORIES + MAX_PLAYERS) ? 1 : -1];
typedef char oam_hidden_after_machine[
    (RENDER_OAM_FIRST_HIDDEN >= RENDER_OAM_MACHINE + 4) ? 1 : -1];

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

static void render_compose_ground(void)
{
    court_style_compose(s_ground_palette, s_painted_ground,
        g_save.settings.court, s_venue);
    s_lava_shown = RENDER_LAVA_WANTED();
    if (s_lava_shown)
        court_style_lava(s_ground_palette);
    s_ground_dirty = 1;
    s_ground_pending = 0;
    s_ground_waits = 0;
}

void render_apply_court_colors(void)
{
    s_ground_pending = 1;
}

/* In render_prepare: a pending recolour on a light frame (or a late one). */
static void render_update_ground(void)
{
    if (!s_ground_pending)
        return;
    if (profile_scanline() > RENDER_RECOLOR_SCANLINE_LIMIT &&
            s_ground_waits < RENDER_RECOLOR_MAX_WAITS) {
        ++s_ground_waits;
        return;
    }
    render_compose_ground();
    /* The HUD skips its own big redraw this frame: the two together ran
     * past the budget on the COURT page (a row and the recolour). */
    g_render_busy = 1;
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
    if (g_match_config.modifiers & MODIFIER_BIG_BALL) {
        /* BIG BALL (ADR-033): a 14 px ball centred in a 32x32 object. */
        obj_put(RENDER_OAM_BALL_SHADOW, sx - 4, y - 4,
            OBJTILE_BIG_BALL_SHADOW, OBJ_ATTR(g_objpal_shadow, 0), 0);
        obj_put(RENDER_OAM_BALL, sx - 16, y - z - 16, OBJTILE_BIG_BALL,
            OBJ_ATTR(g_objpal_objects, 0), 1);
        return;
    }
    obj_put(RENDER_OAM_BALL_SHADOW, sx - 4, y - 4, OBJTILE_BALL_SHADOW,
        OBJ_ATTR(g_objpal_shadow, 0), 0);
    obj_put(RENDER_OAM_BALL, sx - 4, y - z - 4, OBJTILE_BALL,
        OBJ_ATTR(g_objpal_objects, 0), 0);
}

/* The practice ball machine (ADR-028): four 8x8 objects, its nozzle (the
 * left edge, 8 px up) where the feeds leave. Hidden outside practice. */
static void render_machine(void)
{
    s16 x;
    s16 y = PRACTICE_MACHINE_Y - 16;
    u8 attr = OBJ_ATTR(g_objpal_objects, 0);
    u8 i;

    if (!g_match_config.practice || g_attract) {
        for (i = 0; i < 4; ++i)
            render_obj_hide(RENDER_OAM_MACHINE + i);
        return;
    }
    x = projection_x(PRACTICE_MACHINE_X, PRACTICE_MACHINE_Y) - 1;
    obj_put(RENDER_OAM_MACHINE, x, y, OBJTILE_MACHINE_TL, attr, 0);
    obj_put(RENDER_OAM_MACHINE + 1, x + 8, y, OBJTILE_MACHINE_TR, attr, 0);
    obj_put(RENDER_OAM_MACHINE + 2, x, y + 8, OBJTILE_MACHINE_BL, attr, 0);
    obj_put(RENDER_OAM_MACHINE + 3, x + 8, y + 8, OBJTILE_MACHINE_BR, attr, 0);
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
    render_machine();
    render_update_ground();
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
    s_ground_pending = 0;
    s_ground_waits = 0;
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
 * how the match returns from the Player Select screen. The venue is the
 * saved choice, or for AUTO the opponent's home court (ADR-024): shared
 * ground tiles first, then the venue's tiles, map, and palettes 3-7. */
static void render_load_court(void)
{
    u16 ground = (u16)(&court_tiles_end - &court_tiles);
    const VenueData *venue;

    /* Open play (ADR-034) is at the park: AUTO gives the first venue. */
    s_venue = court_style_venue(g_save.settings.venue,
        (g_attract || g_match_config.open_play) ? OPPONENT_NONE :
            g_match_config.opponent);
    /* The venue's ambience (ADR-031); loads only when the venue changed. */
    audio_ambience(s_venue);
    venue = &g_venue_data[s_venue];
    dmaCopyVram((u8 *)&court_tiles, RENDER_BG1_TILES_ADDR, ground);
    dmaCopyVram((u8 *)venue->tiles, RENDER_BG1_TILES_ADDR + ground / 2,
        venue->tiles_size);
    dmaCopyVram((u8 *)venue->map, RENDER_BG1_MAP_ADDR, venue->map_size);
    /* The park (M4-OPEN-05) scrolls a 64x32 map of its own. */
    bgSetMapPtr(0, RENDER_BG1_MAP_ADDR, SC_32x32);
    bgSetScroll(0, 0, 0);
    dmaCopyCGram((u8 *)venue->palettes, 48, 160);
    memcpy(s_painted_ground, &court_palette, sizeof(s_painted_ground));
    render_compose_ground();
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
