#include "audio_events.h"
#include "input.h"
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

u8 options_ball_moves(u8 difficulty, u8 tick)
{
    if (difficulty == DIFFICULTY_KID)
        return tick % 3 != 2;
    if (difficulty == DIFFICULTY_EASY)
        return (tick & 3) != 3;
    return 1;
}

void options_edit_begin(u8 *points, u8 *difficulty, u8 *sound)
{
    *points = options_points(*points);
    *difficulty = options_difficulty(*difficulty);
    if (*sound >= AUDIO_SETTINGS)
        *sound = AUDIO_ALL;
    g_options_edit.row = OPTIONS_ROW_POINTS;
    g_options_edit.original_points = *points;
    g_options_edit.original_difficulty = *difficulty;
    g_options_edit.original_sound = *sound;
}

u8 options_edit_changed(u8 points, u8 difficulty, u8 sound)
{
    return points != g_options_edit.original_points ||
        difficulty != g_options_edit.original_difficulty ||
        sound != g_options_edit.original_sound;
}

u8 options_edit_input(u8 *points, u8 *difficulty, u8 *sound, u16 pressed)
{
    u8 index;
    u8 row = g_options_edit.row;
    u8 left = (pressed & PAD_LEFT) != 0;

    if (pressed & PAD_Y) {
        *points = g_options_edit.original_points;
        *difficulty = g_options_edit.original_difficulty;
        *sound = g_options_edit.original_sound;
        return OPTIONS_EDIT_CANCEL;
    }
    if (pressed & (PAD_B | PAD_START))
        return OPTIONS_EDIT_DONE;
    if (pressed & PAD_UP)
        g_options_edit.row = row ? row - 1 : OPTIONS_ROWS - 1;
    else if (pressed & PAD_DOWN)
        g_options_edit.row = row == OPTIONS_ROWS - 1 ? 0 : row + 1;
    else if (pressed & (PAD_LEFT | PAD_RIGHT)) {
        if (row == OPTIONS_ROW_SOUND) {
            if (left)
                *sound = *sound ? *sound - 1 : AUDIO_SETTINGS - 1;
            else
                *sound = *sound + 1 >= AUDIO_SETTINGS ? 0 : *sound + 1;
        } else if (row == OPTIONS_ROW_POINTS) {
            index = points_index(*points);
            if (left)
                index = index ? index - 1 : OPTIONS_POINT_CHOICES - 1;
            else
                index = index + 1 >= OPTIONS_POINT_CHOICES ? 0 : index + 1;
            *points = g_options_points[index];
        } else if (left) {
            *difficulty = *difficulty ? *difficulty - 1 : DIFFICULTY_COUNT - 1;
        } else {
            *difficulty = *difficulty + 1 >= DIFFICULTY_COUNT ? 0 :
                *difficulty + 1;
        }
    }
    return OPTIONS_EDIT_STAY;
}
