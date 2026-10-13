#include "court.h"
#include "player.h"
#include "rules.h"

static u8 opposing_team(u8 team)
{
    if (team == 0)
        return 1;
    return 0;
}

static u8 last_hitter_team(void)
{
    if (g_ball.last_hitter >= MAX_PLAYERS)
        return 0;
    return g_players[g_ball.last_hitter].team;
}

static void rules_resolve_point(u8 losing_team, u8 fault)
{
    u8 winner;

    if (g_game_state.phase == GAME_PHASE_POINT_RESULT)
        return;

    winner = opposing_team(losing_team);
    g_game_state.point_winner = winner;
    g_game_state.last_fault = fault;
    ++g_game_state.team_score[winner];
    g_game_state.phase = GAME_PHASE_POINT_RESULT;
    g_game_state.phase_timer = 0;
    g_ball.rally_state = BALL_RALLY_INACTIVE;
    g_ball.vx_fp = 0;
    g_ball.vy_fp = 0;
    g_ball.vz_fp = 0;
}

void rules_process_ball_events(u8 events)
{
    u8 losing_team;

    if (g_game_state.phase == GAME_PHASE_SERVE_FAULT) {
        rules_resolve_point(last_hitter_team(), g_game_state.last_fault);
        return;
    }

    if (g_game_state.phase != GAME_PHASE_LIVE)
        return;

    if (events & BALL_EVENT_NET_FAULT) {
        rules_resolve_point(last_hitter_team(), GAME_FAULT_NET);
        return;
    }

    if (events & BALL_EVENT_OUT) {
        rules_resolve_point(last_hitter_team(), GAME_FAULT_OUT);
        return;
    }

    if (!(events & BALL_EVENT_BOUNCE))
        return;
    if (g_ball.bounce_count < 2)
        return;

    if (g_ball.x_fp < TO_FP(COURT_NET_X))
        losing_team = 0;
    else
        losing_team = 1;
    rules_resolve_point(losing_team, GAME_FAULT_SECOND_BOUNCE);
}
