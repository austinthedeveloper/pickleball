#include "ai.h"
#include "ball.h"
#include "collision.h"
#include "game.h"
#include "player.h"
#include "profile.h"
#include "rules.h"
#include "serve.h"

GameState g_game_state;
u8 g_attract;

void game_set_phase(u8 phase)
{
    g_game_state.phase = phase;
    g_game_state.phase_timer = 0;
}

void game_initialize(void)
{
    g_game_state.paused = 0;
    g_game_state.last_fault = GAME_FAULT_NONE;
    g_game_state.point_winner = GAME_NO_TEAM;
    g_game_state.match_winner = GAME_NO_TEAM;
    g_game_state.team_score[0] = 0;
    g_game_state.team_score[1] = 0;
    g_game_state.required_bounce = GAME_REQUIRED_BOUNCE_NONE;
    g_game_state.serving_team = 0;
    g_game_state.hit_stop = 0;
    g_game_state.shot_serial = 0;
    g_game_state.spark_timer = 0;
    g_game_state.spark_x = 0;
    g_game_state.spark_y = 0;
    g_game_state.frame_count = 0;
    game_set_phase(GAME_PHASE_SERVE_SETUP);
}

static void game_begin_serve(u8 serving_team)
{
    u8 i;
    for (i = 0; i < MAX_PLAYERS; ++i)
        g_players[i].pose = PLAYER_POSE_NONE;
    serve_setup(serving_team);
    game_set_phase(GAME_PHASE_SERVE_SETUP);
}

/* Fresh match without a console reset (RST-01): every simulation module is
 * re-initialized, so nothing from the previous match carries over. */
void game_new_match(void)
{
    players_initialize();
    ball_initialize();
    serve_initialize();
    ai_initialize();
    game_initialize();
    g_players[2].active = 1;
    g_ai_slots[2].late_period = AI_LATE_SHOT_PERIOD;
    game_begin_serve(0);
}

void game_start_attract(void)
{
    game_new_match();
    g_attract = 1;
    g_ai_slots[0].late_period = GAME_ATTRACT_LATE_PERIOD;
}

void game_start_match(void)
{
    game_new_match();
    g_attract = 0;
}

static void game_on_contact(u8 player_index)
{
    Player *player = &g_players[player_index];

    ++g_game_state.shot_serial;
    player->flash_timer = GAME_HIT_FLASH_FRAMES;
    g_game_state.spark_timer = GAME_SPARK_FRAMES;
    g_game_state.spark_x = FROM_FP(g_ball.x_fp);
    g_game_state.spark_y = FROM_FP(g_ball.y_fp);
    g_game_state.hit_stop = GAME_HIT_STOP_FRAMES;
}

static void game_update_live(const PlayerIntent intents[MAX_PLAYERS])
{
    u8 hit_player;
    u8 ball_events;

    hit_player = collision_resolve_player_hits(&g_ball, g_players, intents,
        MAX_PLAYERS);
    PROFILE_MARK(5);
    if (hit_player) {
        game_on_contact(hit_player - 1);
        rules_process_player_hit(hit_player - 1);
        if (g_game_state.phase != GAME_PHASE_LIVE)
            return;
    }
    ball_events = ball_update(0);
    serve_process_ball_events(ball_events);
    rules_process_ball_events(ball_events);
}

static void game_update_point_result(const PlayerIntent intents[MAX_PLAYERS])
{
    u8 winner = g_game_state.point_winner;
    u8 i;

    for (i = 0; i < MAX_PLAYERS; ++i) {
        if (!g_players[i].active)
            continue;
        g_players[i].frozen = 1;
        g_players[i].pose = g_players[i].team == winner ?
            PLAYER_POSE_CHEER : PLAYER_POSE_NONE;
    }

    if (g_game_state.phase_timer < GAME_POINT_RESULT_MIN_FRAMES)
        return;
    if (g_game_state.phase_timer < GAME_POINT_RESULT_FRAMES &&
        !intents[0].hit_pressed)
        return;

    if (g_game_state.team_score[winner] >= GAME_POINTS_TO_WIN) {
        g_game_state.match_winner = winner;
        game_set_phase(GAME_PHASE_MATCH_RESULT);
        return;
    }
    game_begin_serve(winner);
}

static void game_update_match_result(const PlayerIntent intents[MAX_PLAYERS])
{
    u8 i;

    for (i = 0; i < MAX_PLAYERS; ++i) {
        if (!g_players[i].active)
            continue;
        g_players[i].frozen = 1;
        g_players[i].pose = g_players[i].team == g_game_state.match_winner ?
            PLAYER_POSE_CHEER : PLAYER_POSE_NONE;
    }
    if (g_attract) {
        if (g_game_state.phase_timer >= GAME_ATTRACT_RESULT_FRAMES)
            game_start_attract();
        return;
    }
    if (g_game_state.phase_timer >= GAME_POINT_RESULT_MIN_FRAMES &&
        intents[0].hit_pressed)
        game_new_match();
}

void game_update(const PlayerIntent intents[MAX_PLAYERS])
{
    ++g_game_state.frame_count;
    if (intents[0].pause_pressed && !g_attract)
        g_game_state.paused = !g_game_state.paused;
    if (g_game_state.paused)
        return;

    if (g_game_state.spark_timer)
        --g_game_state.spark_timer;
    if (g_game_state.hit_stop) {
        --g_game_state.hit_stop;
        return;
    }
    if (g_game_state.phase_timer < 0xffff)
        ++g_game_state.phase_timer;

    players_update(intents, 0);
    PROFILE_MARK(4);
    switch (g_game_state.phase) {
    case GAME_PHASE_SERVE_SETUP:
    case GAME_PHASE_SERVE_TOSS:
        serve_update(intents, 0);
        break;
    case GAME_PHASE_LIVE:
        game_update_live(intents);
        break;
    case GAME_PHASE_POINT_RESULT:
        game_update_point_result(intents);
        break;
    case GAME_PHASE_MATCH_RESULT:
        game_update_match_result(intents);
        break;
    default:
        break;
    }
}
