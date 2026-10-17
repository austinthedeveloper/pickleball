#include <snes.h>
#include "asset_frames.h"
#include "career.h"
#include "hud.h"
#include "hud_tiles.h"
#include "museum.h"
#include "player.h"
#include "render.h"
#include "render_internal.h"

extern char objects_palette;

/*
 * The HAT MUSEUM scene (ADR-061). BG1 and BG2 are off: the backdrop
 * (CGRAM 0, a dark wall here) shows wherever BG3 is clear. Each case is a
 * block of the font's space tile without the priority bit, so it sits
 * behind every object, in BG3 palette 3 with its fill (the panels' navy,
 * colour 3) lit to a gallery grey here; the museum writes no text in that
 * palette. The player stands in each case wearing its hat, a black
 * silhouette while it is locked. Text is ordinary BG3 HUD text. The cursor
 * is the Player Select corners.
 *
 * OBJ palettes: 0 the player (their saved look), 1 the silhouette, 2 the
 * objects (the cursor's yellow).
 * OAM: cursor corners 0-3, hats 4-27 (two halves each, in front of the
 * bodies: a lower index draws on top), bodies 28-39, ponytails 40-51
 * (behind).
 */
#define MUS_OBJPAL_PLAYER 0
#define MUS_OBJPAL_DARK 1
#define MUS_OBJPAL_CURSOR 2
#define MUS_OAM_CURSOR 0
#define MUS_OAM_HATS 4
#define MUS_OAM_BODIES 28
#define MUS_OAM_PONYTAILS 40

/* Priority 3: in front of every BG3 tile without the priority bit. */
#define MUS_ATTR(palette, flags) ((3 << 4) | ((palette) << 1) | (flags))
#define MUS_HFLIP 0x40
#define MUS_VFLIP 0x80

/* Cases: MUSEUM_COLS x MUSEUM_ROWS blocks of CASE_COLS x CASE_ROWS cells,
 * the first at (CASE_COL0, CASE_ROW0). */
#define CASE_COLS 6
#define CASE_ROWS 5
#define CASE_COL0 1
#define CASE_ROW0 5
#define CASE_COL_STEP 8
#define CASE_ROW_STEP 6
/* The figure's top-left in its case, in pixels. */
#define CASE_FIGURE_X 8
#define CASE_FIGURE_Y 6

#define MUS_TITLE_ROW 1
#define MUS_COUNT_ROW 3
#define MUS_INFO_ROW 23
#define MUS_PROMPT_ROW 25

#define MUS_WALL RGB5(6, 3, 5)
#define MUS_CASE RGB5(17, 18, 21)
#define MUS_CASE_PAL HUD_PAL_TEAM_RIGHT
#define MUS_CASE_CGRAM (MUS_CASE_PAL * 4 + 3)
#define MUS_PANEL_FILL RGB5(1, 3, 7)        /* the HUD palettes' colour 3 */
#define MUS_IDLE_CEL_FRAMES 24

static u16 s_pal_player[16];
static u16 s_pal_dark[16];
static u8 s_palettes_dirty;
static u8 s_anim;
static u8 s_cel;
static u8 s_drawn_cursor;
static u16 s_wall;

static const u16 s_hat_tiles[HAT_COUNT] = HAT_TILES;

/* A case's top-left cell. */
static u8 case_col(u8 index)
{
    return CASE_COL0 + (index % MUSEUM_COLS) * CASE_COL_STEP;
}

static u8 case_row(u8 index)
{
    return CASE_ROW0 + (index / MUSEUM_COLS) * CASE_ROW_STEP;
}

/* The case's figure: its body, hat (the player's colours, or the dark
 * silhouette while locked), and ponytail. The picked case idles. */
static void draw_figure(u8 index)
{
    s16 x = case_col(index) * 8 + CASE_FIGURE_X;
    s16 y = case_row(index) * 8 + CASE_FIGURE_Y;
    u8 cel = index == g_museum.cursor ? s_cel : 0;
    u16 tile = cel ? FRAME_IDLE1 : FRAME_IDLE0;
    s16 acc_y = cel ? FRAME_IDLE1_ACC_Y : FRAME_IDLE0_ACC_Y;
    u16 hat = s_hat_tiles[museum_hat(index)];
    u8 palette = museum_unlocked(index) ? MUS_OBJPAL_PLAYER : MUS_OBJPAL_DARK;
    u8 oam = MUS_OAM_HATS + index * 2;

    obj_put(MUS_OAM_BODIES + index, x, y, tile,
        MUS_ATTR(MUS_OBJPAL_PLAYER, 0), 1);
    if (hat) {
        obj_put(oam, x + FRAME_IDLE0_ACC_X + HAT_DX, y + acc_y - HAT_RAISE,
            hat, MUS_ATTR(palette, 0), 0);
        obj_put(oam + 1, x + FRAME_IDLE0_ACC_X + HAT_DX + 8,
            y + acc_y - HAT_RAISE, hat ^ 1, MUS_ATTR(palette, 0), 0);
    } else {
        render_obj_hide(oam);
        render_obj_hide(oam + 1);
    }
    if (museum_player()->variant == PLAYER_VARIANT_FEMALE)
        obj_put(MUS_OAM_PONYTAILS + index, x + FRAME_IDLE0_ACC_X, y + acc_y,
            OBJTILE_PONYTAIL, MUS_ATTR(MUS_OBJPAL_PLAYER, 0), 0);
    else
        render_obj_hide(MUS_OAM_PONYTAILS + index);
}

/* The corners around the picked case. */
static void draw_cursor(void)
{
    s16 x0 = case_col(g_museum.cursor) * 8;
    s16 y0 = case_row(g_museum.cursor) * 8;
    s16 x1 = x0 + CASE_COLS * 8 - 8;
    s16 y1 = y0 + CASE_ROWS * 8 - 8;
    u8 attr = MUS_ATTR(MUS_OBJPAL_CURSOR, 0);

    obj_put(MUS_OAM_CURSOR, x0 - 2, y0 - 2, OBJTILE_SELECT_CORNER, attr, 0);
    obj_put(MUS_OAM_CURSOR + 1, x1 + 2, y0 - 2, OBJTILE_SELECT_CORNER,
        attr | MUS_HFLIP, 0);
    obj_put(MUS_OAM_CURSOR + 2, x0 - 2, y1 + 2, OBJTILE_SELECT_CORNER,
        attr | MUS_VFLIP, 0);
    obj_put(MUS_OAM_CURSOR + 3, x1 + 2, y1 + 2, OBJTILE_SELECT_CORNER,
        attr | MUS_HFLIP | MUS_VFLIP, 0);
}

/* Text centred on the 32-column map. */
static void center_text(u8 row, u8 palette, const char *text)
{
    u8 length = 0;

    while (text[length])
        ++length;
    hud_text((u8)(16 - length / 2), row, palette, text);
}

/* "ACE   5 OF 12" under the title. */
static void draw_count(void)
{
    static char text[CAREER_NAME + 12];
    const CareerPlayer *player = museum_player();
    u8 count = museum_unlocked_count();
    u8 i = 0;
    u8 k;

    for (k = 0; k < CAREER_NAME && player->name[k] != ' '; ++k)
        text[i++] = (char)player->name[k];
    text[i++] = ' ';
    text[i++] = ' ';
    if (count >= 10)
        text[i++] = (char)('0' + count / 10);
    text[i++] = (char)('0' + count % 10);
    text[i++] = ' ';
    text[i++] = 'O';
    text[i++] = 'F';
    text[i++] = ' ';
    if (MUSEUM_HATS >= 10)
        text[i++] = (char)('0' + MUSEUM_HATS / 10);
    text[i++] = (char)('0' + MUSEUM_HATS % 10);
    text[i] = 0;
    hud_fill_box(0, MUS_COUNT_ROW, 32, 1, HUD_TILE_CLEAR);
    center_text(MUS_COUNT_ROW, HUD_PAL_WHITE, text);
}

/* The picked hat under the cases: its name (yellow), and WEARING when it
 * is worn; locked, ??? and how to earn it. */
static void draw_info(void)
{
    u8 index = g_museum.cursor;
    u8 hat = museum_hat(index);
    u8 unlock = g_career_hat_unlock[hat];
    const char *name = "???";
    const char *detail = "";
    u8 name_len;
    u8 detail_len = 0;
    u8 col;

    if (museum_unlocked(index)) {
        name = g_career_hat_names[hat];
        if (museum_player()->hat == hat)
            detail = "WEARING";
    } else if (unlock < CAREER_UNLOCKS) {
        detail = g_career_unlock_how[unlock];
    }
    for (name_len = 0; name[name_len]; ++name_len)
        ;
    while (detail[detail_len])
        ++detail_len;
    if (detail_len)
        detail_len += 2;
    col = (u8)(16 - (name_len + detail_len) / 2);
    hud_fill_box(0, MUS_INFO_ROW, 32, 1, HUD_TILE_CLEAR);
    hud_text(col, MUS_INFO_ROW, HUD_PAL_YELLOW, name);
    if (detail_len)
        hud_text(col + name_len + 2, MUS_INFO_ROW, HUD_PAL_WHITE, detail);
}

static void museum_redraw(void)
{
    u8 i;

    career_compose_palette(museum_player(), s_pal_player);
    s_palettes_dirty = 1;
    for (i = 0; i < MUSEUM_HATS; ++i)
        draw_figure(i);
    draw_cursor();
    draw_count();
    draw_info();
    s_drawn_cursor = g_museum.cursor;
}

void render_museum_prepare(void)
{
    u8 previous;

    if (g_museum.changed) {
        g_museum.changed = 0;
        previous = s_drawn_cursor;
        if (previous != g_museum.cursor && previous < MUSEUM_HATS) {
            s_drawn_cursor = g_museum.cursor;
            draw_figure(previous);          /* stops idling */
            draw_figure(g_museum.cursor);
            draw_cursor();
            draw_info();
        } else {
            museum_redraw();
        }
    } else if (++s_anim >= MUS_IDLE_CEL_FRAMES) {
        s_anim = 0;
        s_cel ^= 1;
        draw_figure(g_museum.cursor);
    }
}

/* VBlank: palettes and HUD bands. */
void render_museum_commit(void)
{
    if (s_palettes_dirty) {
        dmaCopyCGram((u8 *)s_pal_player,
            CGRAM_OBJ + MUS_OBJPAL_PLAYER * 16, 32);
        s_palettes_dirty = 0;
    }
    hud_commit();
}

void render_museum_open(void)
{
    u8 i;

    render_fade(1);
    setScreenOff();
    render_camera_reset();
    for (i = 0; i < 128; ++i)
        render_obj_hide(i);
    bgSetDisable(0);
    bgSetDisable(1);
    bgSetScroll(2, 0, 0);
    setColorEffect(0x00, 0x00);

    s_wall = MUS_WALL;
    dmaCopyCGram((u8 *)&s_wall, 0, 2);
    s_wall = MUS_CASE;
    dmaCopyCGram((u8 *)&s_wall, MUS_CASE_CGRAM, 2);
    /* The silhouette: black, its outline grey so the shape reads over
     * dark hair. */
    s_pal_dark[0] = 0;
    s_pal_dark[1] = RGB5(9, 9, 12);
    for (i = 2; i < 16; ++i)
        s_pal_dark[i] = RGB5(2, 2, 3);
    dmaCopyCGram((u8 *)s_pal_dark, CGRAM_OBJ + MUS_OBJPAL_DARK * 16, 32);
    dmaCopyCGram((u8 *)&objects_palette, CGRAM_OBJ + MUS_OBJPAL_CURSOR * 16,
        32);

    hud_initialize();
    hud_center_big(MUS_TITLE_ROW, HUD_PAL_YELLOW, "HAT MUSEUM");
    for (i = 0; i < MUSEUM_HATS; ++i)
        hud_fill_box(case_col(i), case_row(i), CASE_COLS, CASE_ROWS,
            (u16)' ' | (MUS_CASE_PAL << 10));
    hud_center_prompt(MUS_PROMPT_ROW, HUD_PAL_WHITE, "@ WEAR  Y BACK");

    s_anim = 0;
    s_cel = 0;
    s_drawn_cursor = 0xff;
    g_museum.changed = 0;
    museum_redraw();
    render_museum_commit();
    hud_commit_all();
    render_fade(0);
}

/* Back to the court scene: the backdrop black, the panel fill navy, and
 * BG1 on again (the caller then opens the match scene, which loads the
 * rest). */
void render_museum_close(void)
{
    render_fade(1);
    setScreenOff();
    s_wall = 0;
    dmaCopyCGram((u8 *)&s_wall, 0, 2);
    s_wall = MUS_PANEL_FILL;
    dmaCopyCGram((u8 *)&s_wall, MUS_CASE_CGRAM, 2);
    bgSetEnable(0);
}
