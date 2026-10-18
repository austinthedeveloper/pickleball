#include "audio_events.h"
#include "input.h"
#include "menu.h"
#include "modifiers.h"
#include "options.h"
#include "player.h"

const u8 g_options_points[OPTIONS_POINT_CHOICES] = { 3, 5, 7, 9, 11, 15, 21 };
OptionsEdit g_options_edit;

static u8 points_index(u8 points)
{
    u8 i;

    for (i = 0; i < OPTIONS_POINT_CHOICES; ++i)
        if (g_options_points[i] == points)
            return i;
    return 0xff;
}

u8 options_points(u8 saved)
{
    return points_index(saved) == 0xff ? OPTIONS_POINTS_DEFAULT : saved;
}

u8 options_difficulty(u8 saved)
{
    return saved < DIFFICULTY_COUNT ? saved : DIFFICULTY_MEDIUM;
}

const char *options_difficulty_name(u8 difficulty)
{
    static const char *const names[DIFFICULTY_COUNT] = {
        "KID", "EASY", "MEDIUM", "HARD"
    };
    return names[options_difficulty(difficulty)];
}

/* Fewer mistakes are a longer late period; 0 (never late) stays 0. */
static u8 late_at_least_2(u8 late)
{
    return late < 2 ? 2 : late;
}

void options_apply_difficulty(AiStyle *style, u8 difficulty)
{
    u8 late = style->late_period;

    switch (difficulty) {
    case DIFFICULTY_KID:
        if (late)
            style->late_period = late_at_least_2(late / 3);
        style->reaction += 10;
        style->speed = style->speed > 16 ? style->speed - 4 : 12;
        break;
    case DIFFICULTY_EASY:
        if (late)
            style->late_period = late_at_least_2(late / 2);
        style->reaction += 5;
        style->speed = style->speed > 14 ? style->speed - 2 : 12;
        break;
    case DIFFICULTY_HARD:
        if (late)
            style->late_period = late + late / 2 + 2 > 30 ? 30 :
                late + late / 2 + 2;
        style->reaction = style->reaction > 3 ? style->reaction - 3 : 0;
        if (style->speed < PLAYER_MAX_SPEED_FP)
            ++style->speed;
        break;
    default:
        break;
    }
}

const u8 g_options_cameras[OPTIONS_CAMERA_AMOUNTS] = {
    50, 55, 60, 65, 70, 75, 80, 85, 90, 95, 100
};
const u8 g_options_camera_k[OPTIONS_CAMERA_AMOUNTS] = {
    64, 70, 77, 83, 90, 96, 102, 109, 115, 122, 128
};

u8 options_camera_on(u8 saved)
{
    return saved >= 2 && saved < 2 + OPTIONS_CAMERA_AMOUNTS;
}

u8 options_camera_amount(u8 saved)
{
    if (options_camera_on(saved))
        return saved - 2;
    if ((saved & OPTIONS_CAMERA_OFF) &&
            (saved & ~OPTIONS_CAMERA_OFF) < OPTIONS_CAMERA_AMOUNTS)
        return saved & ~OPTIONS_CAMERA_OFF;
    return OPTIONS_CAMERA_AMOUNT_DEFAULT;
}

u8 options_camera_saved(u8 on, u8 amount)
{
    return on ? amount + 2 : OPTIONS_CAMERA_OFF | amount;
}

u8 options_camera_k(u8 saved)
{
    if (options_camera_on(saved) == 0)
        return 0;
    return g_options_camera_k[saved - 2];
}

const char *options_scoring_name(u8 modifiers)
{
    return (modifiers & MODIFIER_WIN_BY_2) ? "WIN BY 2" : "FIRST TO";
}

u8 options_ball_per_cycle(u8 difficulty, u8 percent)
{
    u16 base = difficulty == DIFFICULTY_KID ? OPTIONS_BALL_CYCLE * 2 / 3 :
        difficulty == DIFFICULTY_EASY ? OPTIONS_BALL_CYCLE * 3 / 4 :
        OPTIONS_BALL_CYCLE;

    /* Once per match: a multiply and a divide are fine here. */
    return (u8)((base * percent + 50) / 100);
}

void options_edit_begin(u8 *points, u8 *difficulty, u8 *sound,
    u8 *modifiers, u8 *camera)
{
    *points = options_points(*points);
    *difficulty = options_difficulty(*difficulty);
    if (*sound >= AUDIO_SETTINGS)
        *sound = AUDIO_ALL;
    *camera = options_camera_saved(options_camera_on(*camera),
        options_camera_amount(*camera));
    g_options_edit.row = OPTIONS_ROW_POINTS;
    g_options_edit.original_points = *points;
    g_options_edit.original_difficulty = *difficulty;
    g_options_edit.original_sound = *sound;
    g_options_edit.original_modifiers = *modifiers;
    g_options_edit.original_camera = *camera;
}

u8 options_edit_changed(u8 points, u8 difficulty, u8 sound, u8 modifiers,
    u8 camera)
{
    return points != g_options_edit.original_points ||
        difficulty != g_options_edit.original_difficulty ||
        sound != g_options_edit.original_sound ||
        modifiers != g_options_edit.original_modifiers ||
        camera != g_options_edit.original_camera;
}

u8 options_edit_input(u8 *points, u8 *difficulty, u8 *sound, u8 *modifiers,
    u8 *camera, u16 pressed)
{
    u8 index;
    u8 row = g_options_edit.row;
    u8 keys = menu_edit_keys(&g_options_edit.row, OPTIONS_ROWS, pressed);

    if (keys == MENU_EDIT_CANCEL) {
        *points = g_options_edit.original_points;
        *difficulty = g_options_edit.original_difficulty;
        *sound = g_options_edit.original_sound;
        *modifiers = g_options_edit.original_modifiers;
        *camera = g_options_edit.original_camera;
        return OPTIONS_EDIT_CANCEL;
    }
    if (keys != MENU_EDIT_STEP)
        return keys;
    if (row == OPTIONS_ROW_CAMERA) {
        *camera = options_camera_saved(options_camera_on(*camera) == 0,
            options_camera_amount(*camera));
    } else if (row == OPTIONS_ROW_AMOUNT) {
        index = menu_step(options_camera_amount(*camera),
            OPTIONS_CAMERA_AMOUNTS, pressed);
        *camera = options_camera_saved(options_camera_on(*camera), index);
    } else if (row == OPTIONS_ROW_SCORING) {
        *modifiers ^= MODIFIER_WIN_BY_2;
    } else if (row == OPTIONS_ROW_SOUND) {
        *sound = menu_step(*sound, AUDIO_SETTINGS, pressed);
    } else if (row == OPTIONS_ROW_POINTS) {
        index = menu_step(points_index(*points), OPTIONS_POINT_CHOICES,
            pressed);
        *points = g_options_points[index];
    } else {
        *difficulty = menu_step(*difficulty, DIFFICULTY_COUNT, pressed);
    }
    return OPTIONS_EDIT_STAY;
}
