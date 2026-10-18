#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "game.h"
#include "input.h"
#include "match.h"
#include "open_play.h"
#include "opponents.h"
#include "options.h"

/* ADR-034 open play: the park's stacks, games, walking, and your turn. */

static void place_you(s16 x, s16 y)
{
    g_open_play.x_fp = TO_FP(x);
    g_open_play.y_fp = TO_FP(y);
}

/* Every team is in exactly one place: on a court or in one stack. You are
 * in at most one stack, and only where your_court says. */
static void check_population(void)
{
    u8 seen[OPPONENT_COUNT];
    u8 you = 0;
    u8 c;
    u8 i;
    u8 e;

    memset(seen, 0, sizeof(seen));
    for (c = 0; c < OPEN_COURTS; ++c) {
        const OpenCourt *court = &g_open_play.court[c];
        for (i = 0; i < 2 + court->stack_count; ++i) {
            e = i < 2 ? court->playing[i] : court->stack[i - 2];
            if (e == OPEN_NONE)
                continue;
            if (e == OPEN_YOU) {
                ++you;
                assert(i < 2 || g_open_play.your_court == c);
                continue;
            }
            assert(e < OPPONENT_COUNT);
            ++seen[e];
        }
        assert(court->stack_count <= OPEN_STACK_MAX);
    }
    for (e = 0; e < OPPONENT_COUNT; ++e)
        assert(seen[e] == 1);
    assert(you <= 1);
}

static void test_begin(void)
{
    u8 c;
    u8 side;

    open_play_begin(1234, 0);
    for (c = 0; c < OPEN_COURTS; ++c) {
        assert(g_open_play.court[c].state == OPEN_COURT_PLAYING);
        assert(g_open_play.court[c].score[0] < OPEN_GAME_POINTS);
    }
    /* Every team starts at its level's court. */
    for (c = 0; c < OPEN_COURTS; ++c)
        for (side = 0; side < 2; ++side)
            assert(open_team_court(g_open_play.court[c].playing[side]) == c);
    assert(g_open_play.your_court == OPEN_NONE);
    assert(g_open_play.called == OPEN_NONE);
    assert(open_walkable(g_open_play.x_fp, g_open_play.y_fp));
    check_population();
}

static void test_walking(void)
{
    s16 left = open_court_left(1);
    u16 n;

    open_play_begin(1, 0);
    /* Inside a court is fenced off; the walks between courts are open. */
    assert(!open_walkable(TO_FP(left + 20), TO_FP(OPEN_COURT_TOP + 40)));
    assert(open_walkable(TO_FP(left - 12), TO_FP(OPEN_COURT_TOP + 40)));
    assert(!open_walkable(TO_FP(2), TO_FP(150)));
    /* Walking up into a court stops at its fence. */
    place_you(left + 20, OPEN_RACK_Y);
    for (n = 0; n < 200; ++n)
        open_play_update(0, -1, 0);
    assert(FROM_FP(g_open_play.y_fp) >=
        OPEN_COURT_TOP + OPEN_COURT_H + OPEN_FENCE_MARGIN - 1);
    /* Diagonally, you slide along it. */
    open_play_update(1, -1, 0);
    assert(FROM_FP(g_open_play.x_fp) > left + 20);
    assert(g_open_play.walking && !g_open_play.facing_left);
    open_play_update(-1, 0, 0);
    assert(g_open_play.facing_left);
}

static void test_stacking(void)
{
    u8 c = 2;

    open_play_begin(7, 0);
    /* Away from every rack, B does nothing. */
    place_you(128, 200);
    assert(open_rack_near() == OPEN_NONE);
    assert(open_play_update(0, 0, 1) == OPEN_EVENT_NONE);
    place_you(open_rack_x(c), OPEN_RACK_Y + 4);
    assert(open_rack_near() == c);
    assert(open_play_update(0, 0, 1) == OPEN_EVENT_STACKED);
    assert(g_open_play.your_court == c);
    assert(g_open_play.court[c].stack[g_open_play.court[c].stack_count - 1] ==
        OPEN_YOU);
    assert(open_your_wait() == (g_open_play.court[c].stack_count - 1) / 2);
    check_population();
    /* B again takes it back. */
    assert(open_play_update(0, 0, 1) == OPEN_EVENT_UNSTACKED);
    assert(g_open_play.your_court == OPEN_NONE);
    assert(open_your_wait() == OPEN_NONE);
    /* Stacking at another court moves your paddle there. */
    open_play_update(0, 0, 1);
    place_you(open_rack_x(0), OPEN_RACK_Y);
    assert(open_play_update(0, 0, 1) == OPEN_EVENT_STACKED);
    assert(g_open_play.your_court == 0);
    check_population();
    /* A full stack has no room. */
    g_open_play.court[1].stack_count = OPEN_STACK_MAX;
    place_you(open_rack_x(1), OPEN_RACK_Y);
    assert(open_play_update(0, 0, 1) == OPEN_EVENT_FULL);
    assert(g_open_play.your_court == 0);
}

/* Stack, wait for your game, play it, come back: the park goes on. */
static void test_your_turn(void)
{
    unsigned long frames = 0;
    u8 event = OPEN_EVENT_NONE;
    u8 c = 1;
    u8 called_court;
    u8 opponent;

    open_play_begin(99, 0);
    place_you(open_rack_x(c), OPEN_RACK_Y);
    assert(open_play_update(0, 0, 1) == OPEN_EVENT_STACKED);
    place_you(128, 200);
    while (event != OPEN_EVENT_CALLED && frames < 60UL * 60 * 5) {
        event = open_play_update(0, 0, 0);
        check_population();
        ++frames;
    }
    assert(event == OPEN_EVENT_CALLED);
    printf("open play: called after %lu s on court %u\n",
        (unsigned long)(frames / 60), g_open_play.called + 1);
    called_court = g_open_play.called;
    assert(called_court == c);
    opponent = g_open_play.opponent;
    assert(opponent < OPPONENT_COUNT);
    assert(open_team_court(opponent) == c);     /* a team of that level */
    /* Your partner: the team waiting next there (not your opponents). */
    if (g_open_play.court[c].stack_count) {
        assert(g_open_play.partner == g_open_play.court[c].stack[0]);
        assert(g_open_play.partner != opponent);
        assert(open_team_court(g_open_play.partner) == c);
    } else {
        assert(g_open_play.partner == OPEN_NONE);
    }
    printf("open play: you and %s's captain vs %s\n",
        g_open_play.partner < OPPONENT_COUNT ?
            g_opponent_team[g_open_play.partner] : "the regular",
        g_opponent_team[opponent]);
    assert(g_open_play.court[c].state == OPEN_COURT_YOURS);
    assert(g_open_play.your_court == OPEN_NONE);
    /* The call shows for a while, input ignored, then the match. */
    place_you(128, 200);
    for (frames = 1; frames < OPEN_CALL_FRAMES; ++frames)
        assert(open_play_update(1, 0, 1) == OPEN_EVENT_NONE);
    assert(FROM_FP(g_open_play.x_fp) == 128);
    assert(open_play_update(0, 0, 0) == OPEN_EVENT_PLAY);
    /* Back from a lost match: you leave, the court changes over (a win
     * keeps you on: test_you_stay). */
    open_play_return(0, 7, 11);
    assert(g_open_play.called == OPEN_NONE);
    assert(g_open_play.last_won == 0 && g_open_play.last_score[1] == 11);
    assert(g_open_play.court[c].state == OPEN_COURT_CHANGEOVER);
    assert(open_walkable(g_open_play.x_fp, g_open_play.y_fp));
    for (frames = 0; frames <= OPEN_CHANGEOVER_FRAMES; ++frames)
        open_play_update(0, 0, 0);
    assert(g_open_play.court[c].state == OPEN_COURT_PLAYING);
    check_population();
}

/* A long soak: games end and start on every court, teams move between
 * courts, and nobody is lost or duplicated. */
static void test_soak(void)
{
    u16 games[OPEN_COURTS] = { 0, 0, 0 };
    u8 away = 0;
    u8 side;
    u8 e;
    unsigned long f;
    u8 c;
    u8 before[OPEN_COURTS];

    open_play_begin(4321, 0);

    for (f = 0; f < 60UL * 60 * 20; ++f) {
        for (c = 0; c < OPEN_COURTS; ++c)
            before[c] = g_open_play.court[c].state;
        open_play_update(0, 0, 0);
        for (c = 0; c < OPEN_COURTS; ++c) {
            if (before[c] == OPEN_COURT_CHANGEOVER &&
                    g_open_play.court[c].state == OPEN_COURT_PLAYING)
                ++games[c];
            /* Teams only play at their own level. */
            for (side = 0; side < 2; ++side) {
                e = g_open_play.court[c].playing[side];
                if (e < OPPONENT_COUNT && open_team_court(e) != c)
                    away = 1;
            }
        }
        if ((f & 255) == 0)
            check_population();
    }
    printf("open play: 20 minutes, games by court %u %u %u\n", games[0],
        games[1], games[2]);
    for (c = 0; c < OPEN_COURTS; ++c)
        assert(games[c] >= 15);
    assert(!away);
}

/* The levels: ladder order, names, and the difficulty of your game. */
static void test_levels(void)
{
    assert(open_team_court(0) == 0 && open_team_court(2) == 0);
    assert(open_team_court(3) == 1 && open_team_court(5) == 1);
    assert(open_team_court(6) == 2 && open_team_court(7) == 2);
    assert(strcmp(open_court_level_name(0), "ROOKIE") == 0);
    assert(strcmp(open_court_level_name(2), "CHAMP") == 0);
    assert(open_court_difficulty(0, DIFFICULTY_HARD) == DIFFICULTY_EASY);
    assert(open_court_difficulty(1, DIFFICULTY_EASY) == DIFFICULTY_MEDIUM);
    assert(open_court_difficulty(2, DIFFICULTY_MEDIUM) == DIFFICULTY_HARD);
    assert(open_court_difficulty(2, DIFFICULTY_KID) == DIFFICULTY_KID);
}

/* 2P as your partner (M4-OPEN-04): slot 1 is human on the port 2 pad;
 * in versus, port 2 still plays slot 2. */
static void test_two_players(void)
{
    u16 raw[MAX_PADS];

    open_play_begin(5, 1);
    assert(g_open_play.two_players == 1);
    input_configure(0);
    match_config_doubles();
    g_match_config.human[1] = 1;
    g_attract = 0;
    game_new_match();
    assert(g_input_owners[1] == INPUT_OWNER_HUMAN);
    assert(g_input_pad_for_player[1] == 1);
    assert(!g_players[1].cpu_controlled);
    memset(raw, 0, sizeof(raw));
    raw[1] = PAD_RIGHT;
    input_map_frame(raw);
    assert(g_player_intents[1].move_x == 1);
    assert(g_player_intents[2].move_x == 0);
    /* Versus: port 2 is slot 2's again, and slot 1 has no pad. */
    match_config_doubles();
    g_match_config.human[2] = 1;
    game_new_match();
    assert(g_input_pad_for_player[1] == INPUT_NO_PAD);
    input_map_frame(raw);
    assert(g_player_intents[2].move_x == 1);
    match_config_defaults();
    game_new_match();
}

/* M4-OPEN-05: the clubhouse. Its walls are solid; the door in the bottom
 * wall lets you in. Inside, B at the board turns its page and B at the
 * ball machine asks for practice; the bench seats the first two teams
 * waiting. */
static void test_clubhouse(void)
{
    u16 n;

    open_play_begin(7, 0);
    /* Walls, inside, the door. */
    assert(!open_walkable(TO_FP(OPEN_HOUSE_LEFT + 2), TO_FP(100)));
    assert(!open_walkable(TO_FP(OPEN_HOUSE_RIGHT - 2), TO_FP(100)));
    assert(!open_walkable(TO_FP(360), TO_FP(OPEN_HOUSE_TOP + 2)));
    assert(!open_walkable(TO_FP(340), TO_FP(OPEN_HOUSE_BOTTOM - 2)));
    assert(open_walkable(TO_FP(360), TO_FP(100)));
    assert(open_walkable(TO_FP(OPEN_HOUSE_DOOR_X),
        TO_FP(OPEN_HOUSE_BOTTOM - 2)));
    assert(open_walkable(TO_FP(OPEN_HOUSE_LEFT - 8), TO_FP(100)));
    /* Walk in: along the path to the door, then up through it. */
    place_you(OPEN_HOUSE_DOOR_X, OPEN_START_Y);
    assert(!open_in_house());
    for (n = 0; n < 60; ++n)
        open_play_update(0, -1, 0);
    assert(open_in_house());
    /* Walking into a wall from inside stops at it. */
    for (n = 0; n < 200; ++n)
        open_play_update(1, 0, 0);
    assert(FROM_FP(g_open_play.x_fp) <
        OPEN_HOUSE_RIGHT - OPEN_HOUSE_WALL);
    /* The board: each B the next page, round again; leaving clears it. */
    place_you(OPEN_BOARD_X, OPEN_BOARD_Y);
    assert(open_play_update(0, 0, 1) == OPEN_EVENT_BOARD);
    assert(g_open_play.board_page == 0);
    for (n = 1; n < OPEN_BOARD_PAGES; ++n)
        open_play_update(0, 0, 1);
    assert(g_open_play.board_page == OPEN_BOARD_PAGES - 1);
    open_play_update(0, 0, 1);
    assert(g_open_play.board_page == 0);
    place_you(400, 120);
    open_play_update(0, 0, 0);
    assert(g_open_play.board_page == OPEN_NONE);
    /* The ball machine. */
    place_you(OPEN_MACHINE_X, OPEN_MACHINE_Y);
    assert(open_machine_near());
    assert(open_play_update(0, 0, 1) == OPEN_EVENT_PRACTICE);
    /* The bench: the two teams waiting at the start (one each at the
     * first two levels' courts). */
    assert(open_waiting_team(0) < OPPONENT_COUNT);
    assert(open_waiting_team(1) < OPPONENT_COUNT);
    assert(open_waiting_team(0) != open_waiting_team(1));
    assert(open_waiting_team(2) == OPEN_NONE);
    place_you(OPEN_BENCH_X + 24, OPEN_BENCH_Y + 8);
    assert(open_bench_near());
    check_population();
}

/* ADR-070: the park rating's rules, and courts above it closed. */
static void test_rating(void)
{
    /* Levels: ROOKIE, CLUB, CHAMP. Shown as points: 10 to start, the
     * courts at 15 and 25, 0 to 40. */
    assert(open_rating_points(OPEN_RATING_START) == 10);
    assert(open_rating_points(open_court_floor(1)) == 15);
    assert(open_rating_points(open_court_floor(2)) == 25);
    assert(open_rating_points(OPEN_RATING_MIN) == 0);
    assert(open_rating_points(OPEN_RATING_MAX) == 40);
    assert(open_rating_court(20) == 0 && open_rating_court(34) == 0);
    assert(open_rating_court(35) == 1 && open_rating_court(44) == 1);
    assert(open_rating_court(45) == 2 && open_rating_court(60) == 2);
    assert(open_court_floor(1) == 35 && open_court_floor(2) == 45);
    /* At your level a win is +0.2, above it +0.3, below it nothing; a
     * loss is -0.1 anywhere. */
    assert(open_rating_after(30, 0, 1) == 32);
    assert(open_rating_after(30, 1, 1) == 33);
    assert(open_rating_after(30, 2, 1) == 33);
    assert(open_rating_after(40, 0, 1) == 40);
    assert(open_rating_after(40, 0, 0) == 39);
    assert(open_rating_after(40, 1, 0) == 39);
    /* The floor and the cap. */
    assert(open_rating_after(OPEN_RATING_MIN, 0, 0) == OPEN_RATING_MIN);
    assert(open_rating_after(59, 2, 1) == OPEN_RATING_MAX);
    assert(open_rating_after(OPEN_RATING_MAX, 2, 1) == OPEN_RATING_MAX);

    /* Unrated (a fresh park): every court open. */
    open_play_begin(7, 0);
    assert(g_open_play.rating == OPEN_RATING_NONE);
    assert(open_court_open(0) && open_court_open(1) && open_court_open(2));
    /* Rated 3.2 with only the first court reached: the others closed. */
    open_play_rate(32, 0);
    assert(open_court_open(0) && !open_court_open(1) && !open_court_open(2));
    place_you(open_rack_x(1), OPEN_RACK_Y);
    assert(open_play_update(0, 0, 1) == OPEN_EVENT_LOCKED);
    assert(g_open_play.your_court == OPEN_NONE);
    place_you(open_rack_x(0), OPEN_RACK_Y);
    assert(open_play_update(0, 0, 1) == OPEN_EVENT_STACKED);
    /* A court reached stays open below its level (best_court). */
    open_play_rate(42, 2);
    assert(open_court_open(2));
    place_you(open_rack_x(2), OPEN_RACK_Y);
    assert(open_play_update(0, 0, 1) == OPEN_EVENT_STACKED);
    assert(g_open_play.your_court == 2);
    check_population();
}

/* ADR-071: run court c's game to its end with side winning it (the other
 * side's points held at 0). */
static void finish_game(u8 c, u8 side)
{
    OpenCourt *court = &g_open_play.court[c];
    u16 n;

    for (n = 0; court->state == OPEN_COURT_PLAYING && n < 2000; ++n) {
        court->score[side] = OPEN_GAME_POINTS - 1;
        court->score[side ^ 1] = 0;
        open_play_update(0, 0, 0);
    }
    assert(court->state == OPEN_COURT_CHANGEOVER);
    assert(court->stay == side);
}

/* Through court c's changeover; OPEN_EVENT_CALLED if it called you. */
static u8 run_changeover(u8 c)
{
    OpenCourt *court = &g_open_play.court[c];
    u8 event = OPEN_EVENT_NONE;
    u16 n;

    for (n = 0; court->state == OPEN_COURT_CHANGEOVER &&
            n <= OPEN_CHANGEOVER_FRAMES; ++n)
        if (open_play_update(0, 0, 0) == OPEN_EVENT_CALLED)
            event = OPEN_EVENT_CALLED;
    return event;
}

/* Winners stay on, the losers leave; after OPEN_STAY_MAX wins in a row
 * the winners come off too. */
static void test_winners_stay(void)
{
    OpenCourt *court = &g_open_play.court[0];
    u8 winner;
    u8 loser;
    u8 next;

    open_play_begin(321, 0);
    assert(court->stay == OPEN_NONE && court->streak == 0);
    winner = court->playing[1];
    loser = court->playing[0];
    next = court->stack[0];
    finish_game(0, 1);
    assert(court->streak == 1);
    run_changeover(0);
    assert(court->state == OPEN_COURT_PLAYING);
    /* The winners keep their side; the next team comes on at the other;
     * the losers wait at the back. */
    assert(court->playing[1] == winner);
    assert(court->playing[0] == next);
    assert(court->stack[court->stack_count - 1] == loser);
    assert(court->stay == 1 && court->streak == 1);
    check_population();
    /* A second win in a row: the cap, so all four come off. */
    finish_game(0, 1);
    assert(court->streak == OPEN_STAY_MAX);
    run_changeover(0);
    assert(court->state == OPEN_COURT_PLAYING);
    assert(court->playing[0] != winner && court->playing[1] != winner);
    assert(court->stay == OPEN_NONE && court->streak == 0);
    check_population();
    /* The other side winning ends a streak and starts its own. */
    finish_game(0, 0);
    run_changeover(0);
    winner = court->playing[0];
    finish_game(0, 1);
    assert(court->stay == 1 && court->streak == 1);
    run_changeover(0);
    assert(court->playing[0] != winner);
    check_population();
}

/* Stack at a court and play up to your call there, then its match. */
static void wait_for_call(u8 c)
{
    unsigned long frames = 0;
    u8 event = OPEN_EVENT_NONE;

    place_you(open_rack_x(c), OPEN_RACK_Y);
    assert(open_play_update(0, 0, 1) == OPEN_EVENT_STACKED);
    place_you(128, 200);
    while (event != OPEN_EVENT_CALLED && frames < 60UL * 60 * 5) {
        event = open_play_update(0, 0, 0);
        ++frames;
    }
    assert(event == OPEN_EVENT_CALLED && g_open_play.called == c);
    for (frames = 1; frames < OPEN_CALL_FRAMES; ++frames)
        open_play_update(0, 0, 0);
    assert(open_play_update(0, 0, 0) == OPEN_EVENT_PLAY);
}

/* You win: you stay on, called again with the same partner against a team
 * that isn't your partner's; a second win and you come off. */
static void test_you_stay(void)
{
    u8 c = 0;
    OpenCourt *court = &g_open_play.court[c];
    u8 partner;
    u8 first;
    u8 side;

    open_play_begin(99, 0);
    wait_for_call(c);
    partner = g_open_play.partner;
    first = g_open_play.opponent;
    assert(partner < OPPONENT_COUNT);
    assert(court->stack[0] == partner);
    assert(g_open_play.stay_court == OPEN_NONE);
    open_play_return(1, 11, 4);
    assert(g_open_play.last_won == 1 && g_open_play.last_streak == 1);
    assert(g_open_play.stay_court == c);
    assert(court->playing[0] == OPEN_YOU || court->playing[1] == OPEN_YOU);
    side = court->playing[0] == OPEN_YOU ? 0 : 1;
    assert(court->stay == side && court->streak == 1);
    check_population();
    /* The changeover calls you again: same side, same partner, and the
     * partner's team (first in line) is skipped. */
    assert(run_changeover(c) == OPEN_EVENT_CALLED);
    assert(g_open_play.called == c);
    assert(court->state == OPEN_COURT_YOURS);
    assert(court->playing[side] == OPEN_YOU);
    assert(g_open_play.stay_court == c);
    assert(g_open_play.partner == partner);
    assert(g_open_play.opponent < OPPONENT_COUNT);
    assert(g_open_play.opponent != partner);
    assert(g_open_play.opponent != first);  /* the losers went to the back */
    assert(court->stack[0] == partner);
    check_population();
    printf("open play: winners stay, you and %s's captain vs %s\n",
        g_opponent_team[partner], g_opponent_team[g_open_play.opponent]);
    /* A second win: the cap, you come off. */
    open_play_return(1, 11, 9);
    assert(g_open_play.last_streak == OPEN_STAY_MAX);
    assert(g_open_play.stay_court == OPEN_NONE);
    assert(court->playing[0] != OPEN_YOU && court->playing[1] != OPEN_YOU);
    assert(run_changeover(c) == OPEN_EVENT_NONE);
    assert(court->state == OPEN_COURT_PLAYING);
    assert(court->stay == OPEN_NONE && court->streak == 0);
    check_population();

    /* 2P as your partner: no partner team to skip, still called again. */
    open_play_begin(99, 1);
    wait_for_call(c);
    open_play_return(1, 11, 2);
    assert(run_changeover(c) == OPEN_EVENT_CALLED);
    assert(g_open_play.stay_court == c);
    check_population();
}

/* You lose: you come off and the winners stay. B at a rack while waiting
 * to be called again gives up your place. */
static void test_you_lose(void)
{
    u8 c = 0;
    OpenCourt *court = &g_open_play.court[c];
    u8 winner;

    open_play_begin(99, 0);
    wait_for_call(c);
    winner = g_open_play.opponent;
    open_play_return(0, 6, 11);
    assert(g_open_play.last_won == 0 && g_open_play.last_streak == 0);
    assert(g_open_play.stay_court == OPEN_NONE);
    assert(court->playing[0] != OPEN_YOU && court->playing[1] != OPEN_YOU);
    assert(court->playing[court->stay] == winner && court->streak == 1);
    run_changeover(c);
    assert(court->state == OPEN_COURT_PLAYING);
    assert(court->playing[court->stay] == winner);
    check_population();

    /* Won, then stacked elsewhere before the call: you give up the stay. */
    open_play_begin(99, 0);
    wait_for_call(c);
    open_play_return(1, 11, 4);
    assert(g_open_play.stay_court == c);
    place_you(open_rack_x(1), OPEN_RACK_Y);
    assert(open_play_update(0, 0, 1) == OPEN_EVENT_STACKED);
    assert(g_open_play.stay_court == OPEN_NONE);
    assert(court->playing[0] != OPEN_YOU && court->playing[1] != OPEN_YOU);
    assert(court->stay == OPEN_NONE);
    assert(run_changeover(c) == OPEN_EVENT_NONE);
    assert(court->state == OPEN_COURT_PLAYING);
    check_population();
}

/* Nobody waiting: the winners come off and the court waits. Eight teams
 * always leave someone waiting, so the stacks are emptied by hand (the
 * population check does not apply). */
static void test_stay_nobody_waiting(void)
{
    u8 c = 2;
    OpenCourt *court = &g_open_play.court[c];
    u8 i;

    /* A team that beat you, nobody anywhere to play it. */
    open_play_begin(5, 0);
    for (i = 0; i < OPEN_COURTS; ++i)
        g_open_play.court[i].stack_count = 0;
    court->playing[1] = OPEN_YOU;
    court->state = OPEN_COURT_YOURS;
    g_open_play.called = c;
    open_play_return(0, 2, 11);
    assert(court->stay == 0 && court->streak == 1);
    run_changeover(c);
    assert(court->state == OPEN_COURT_CHANGEOVER);
    assert(court->playing[0] == OPEN_NONE && court->playing[1] == OPEN_NONE);
    assert(court->stay == OPEN_NONE && court->streak == 0);
    assert(court->stack_count == 1 && court->stack[0] == 6);

    /* You won, and only your partner's team waits: you come off. */
    open_play_begin(5, 0);
    for (i = 0; i < OPEN_COURTS; ++i)
        g_open_play.court[i].stack_count = 0;
    court->playing[0] = OPEN_YOU;
    court->playing[1] = OPEN_NONE;      /* no losers to come back on */
    court->stack[0] = 7;
    court->stack_count = 1;
    court->state = OPEN_COURT_YOURS;
    g_open_play.called = c;
    g_open_play.partner = 7;
    open_play_return(1, 11, 3);
    assert(g_open_play.stay_court == c);
    assert(run_changeover(c) == OPEN_EVENT_NONE);
    assert(g_open_play.called == OPEN_NONE);
    assert(g_open_play.stay_court == OPEN_NONE);
    assert(court->playing[0] == OPEN_NONE && court->playing[1] == OPEN_NONE);
    assert(court->stack_count == 1 && court->stack[0] == 7);
}

/* A long soak with you in it: stack, play, win or lose at random, stay
 * on or come off; nobody is lost or duplicated. */
static void test_stay_soak(void)
{
    unsigned long f;
    u16 rng = 0x1234;
    u16 games = 0;
    u16 stays = 0;
    u16 caps = 0;
    u16 team_caps = 0;
    u8 c;
    u8 event;
    u8 rack = 0;

    open_play_begin(2024, 0);
    for (f = 0; f < 60UL * 60 * 30; ++f) {
        if (g_open_play.called == OPEN_NONE &&
                g_open_play.your_court == OPEN_NONE &&
                g_open_play.stay_court == OPEN_NONE) {
            rack = rack >= OPEN_COURTS - 1 ? 0 : rack + 1;
            place_you(open_rack_x(rack), OPEN_RACK_Y);
            open_play_update(0, 0, 1);
            place_you(128, 200);
        }
        for (c = 0; c < OPEN_COURTS; ++c)
            if (g_open_play.court[c].streak >= OPEN_STAY_MAX &&
                    g_open_play.court[c].state == OPEN_COURT_CHANGEOVER &&
                    g_open_play.court[c].timer == 1)
                ++team_caps;
        event = open_play_update(0, 0, 0);
        if (event == OPEN_EVENT_CALLED &&
                g_open_play.stay_court != OPEN_NONE) {
            ++stays;
            assert(g_open_play.opponent != g_open_play.partner);
        }
        if (event == OPEN_EVENT_PLAY) {
            rng ^= rng << 7;
            rng ^= rng >> 9;
            rng ^= rng << 8;
            open_play_return(rng & 1, 11, 5);
            if (g_open_play.last_streak >= OPEN_STAY_MAX)
                ++caps;
            ++games;
        }
        check_population();
    }
    printf("open play: 30 minutes with you, %u games, %u stays, %u capped, "
        "%u team caps\n", games, stays, caps, team_caps);
    assert(games >= 20 && stays >= 3 && caps >= 1 && team_caps >= 10);
}

/* Pick your partner (ADR-072): Y at your rack cycles next in line, then
 * each team you may pick in ladder order, then next in line. */
static void test_pick_cycle(void)
{
    open_play_begin(77, 0);
    assert(g_open_play.pick == OPEN_NONE && g_open_play.pickable == 0);
    place_you(open_rack_x(1), OPEN_RACK_Y);
    assert(open_play_update(0, 0, 1) == OPEN_EVENT_STACKED);
    /* No teams to pick: Y does nothing. */
    assert(!open_pick_ready() && !open_play_pick_next());
    assert(g_open_play.pick == OPEN_NONE);
    open_play_partners(0x29);           /* teams 0, 3, 5 */
    assert(open_pick_ready());
    assert(open_play_pick_next() && g_open_play.pick == 0);
    assert(open_play_pick_next() && g_open_play.pick == 3);
    assert(open_play_pick_next() && g_open_play.pick == 5);
    assert(open_play_pick_next() && g_open_play.pick == OPEN_NONE);
    assert(open_play_pick_next() && g_open_play.pick == 0);
    /* Only at your own rack: another rack, or away from racks. */
    place_you(open_rack_x(0), OPEN_RACK_Y);
    assert(!open_pick_ready() && !open_play_pick_next());
    place_you(128, 200);
    assert(!open_play_pick_next());
    assert(g_open_play.pick == 0);
    /* Not holding your paddle there (taken back): nothing. */
    place_you(open_rack_x(1), OPEN_RACK_Y);
    assert(open_play_update(0, 0, 1) == OPEN_EVENT_UNSTACKED);
    assert(!open_play_pick_next());
    assert(open_play_update(0, 0, 1) == OPEN_EVENT_STACKED);
    assert(open_play_pick_next() && g_open_play.pick == 3);
    /* A pick no longer available goes back to next in line. */
    open_play_partners(0x01);
    assert(g_open_play.pick == OPEN_NONE);
    open_play_partners(0x80);
    assert(open_play_pick_next() && g_open_play.pick == 7);
    assert(open_play_pick_next() && g_open_play.pick == OPEN_NONE);
    /* 2P along: 2P is your partner, Y does nothing. */
    open_play_begin(77, 1);
    open_play_partners(0xff);
    place_you(open_rack_x(1), OPEN_RACK_Y);
    assert(open_play_update(0, 0, 1) == OPEN_EVENT_STACKED);
    assert(!open_pick_ready() && !open_play_pick_next());
    assert(g_open_play.pick == OPEN_NONE);
    check_population();
}

/* Stack at court c with a pick, and play up to your call. */
static void pick_and_wait(u16 seed, u8 c, u8 pick)
{
    unsigned long frames = 0;

    open_play_begin(seed, 0);
    open_play_partners(0xff);
    place_you(open_rack_x(c), OPEN_RACK_Y);
    assert(open_play_update(0, 0, 1) == OPEN_EVENT_STACKED);
    while (g_open_play.pick != pick)
        assert(open_play_pick_next());
    place_you(128, 200);
    while (g_open_play.called == OPEN_NONE && ++frames < 60UL * 60 * 5)
        open_play_update(0, 0, 0);
    assert(g_open_play.called == c);
}

/* Called with a pick: the picked captain partners you, and their team is
 * never your opponent; winning, you stay on with them. */
static void test_pick_called(void)
{
    u8 c;
    u8 pick;
    u8 side;
    u16 seed;
    u16 games = 0;

    for (seed = 1; seed <= 6; ++seed)
        for (c = 0; c < OPEN_COURTS; ++c)
            for (pick = 0; pick < OPPONENT_COUNT; ++pick) {
                pick_and_wait(seed, c, pick);
                assert(g_open_play.partner == pick);
                assert(g_open_play.opponent < OPPONENT_COUNT);
                assert(g_open_play.opponent != pick);
                check_population();
                ++games;
            }
    printf("open play: %u calls with a picked partner\n", games);
    /* The picked team first in line at that court: passed over. */
    open_play_begin(99, 0);
    pick = g_open_play.court[0].stack[0];
    pick_and_wait(99, 0, pick);
    assert(g_open_play.partner == pick && g_open_play.opponent != pick);
    /* Winners stay keeps the picked partner. */
    open_play_return(1, 11, 3);
    assert(g_open_play.stay_court == 0);
    side = g_open_play.court[0].playing[0] == OPEN_YOU ? 0 : 1;
    assert(run_changeover(0) == OPEN_EVENT_CALLED);
    assert(g_open_play.stay_court == 0);
    assert(g_open_play.court[0].playing[side] == OPEN_YOU);
    assert(g_open_play.partner == pick && g_open_play.opponent != pick);
    check_population();
}

/* You come on against winners who are your picked partner's team: a
 * fresh game instead, so they are not your opponents. */
static void test_pick_vs_winners(void)
{
    OpenCourt *court = &g_open_play.court[0];
    u8 winners;

    open_play_begin(321, 0);
    open_play_partners(0xff);
    winners = court->playing[1];
    /* You first in line at court 0, picking the team about to win. */
    place_you(open_rack_x(0), OPEN_RACK_Y);
    assert(open_play_update(0, 0, 1) == OPEN_EVENT_STACKED);
    while (g_open_play.pick != winners)
        assert(open_play_pick_next());
    while (court->stack[0] != OPEN_YOU) {
        /* Move you to the front: the others go behind. */
        u8 e = court->stack[0];
        u8 i;
        for (i = 1; i < court->stack_count; ++i)
            court->stack[i - 1] = court->stack[i];
        court->stack[court->stack_count - 1] = e;
    }
    finish_game(0, 1);
    assert(court->stay == 1);
    assert(run_changeover(0) == OPEN_EVENT_CALLED);
    assert(g_open_play.partner == winners);
    assert(g_open_play.opponent != winners);
    assert(court->streak == 0);
    check_population();
}

/* A soak with you picking partners as you go: nobody lost or doubled,
 * and a picked partner's team is never your opponent. */
static void test_pick_soak(void)
{
    unsigned long f;
    u16 rng = 0x4321;
    u16 games = 0;
    u16 picked = 0;
    u8 event;
    u8 rack = 0;
    u8 n;

    open_play_begin(2025, 0);
    open_play_partners(0xb5);
    for (f = 0; f < 60UL * 60 * 30; ++f) {
        if (g_open_play.called == OPEN_NONE &&
                g_open_play.your_court == OPEN_NONE &&
                g_open_play.stay_court == OPEN_NONE) {
            rack = rack >= OPEN_COURTS - 1 ? 0 : rack + 1;
            place_you(open_rack_x(rack), OPEN_RACK_Y);
            open_play_update(0, 0, 1);
            rng ^= rng << 7;
            rng ^= rng >> 9;
            rng ^= rng << 8;
            for (n = (u8)(rng & 7); n; --n)
                open_play_pick_next();
            place_you(128, 200);
        }
        event = open_play_update(0, 0, 0);
        if (event == OPEN_EVENT_CALLED) {
            assert(g_open_play.opponent != g_open_play.partner);
            if (g_open_play.pick != OPEN_NONE) {
                assert(g_open_play.partner == g_open_play.pick);
                ++picked;
            }
        }
        if (event == OPEN_EVENT_PLAY) {
            rng ^= rng << 7;
            rng ^= rng >> 9;
            rng ^= rng << 8;
            open_play_return(rng & 1, 11, 5);
            ++games;
        }
        check_population();
    }
    printf("open play: 30 minutes picking partners, %u games, %u picked\n",
        games, picked);
    assert(games >= 20 && picked >= 10);
}

int main(void)
{
    test_levels();
    test_begin();
    test_walking();
    test_stacking();
    test_your_turn();
    test_soak();
    test_two_players();
    test_clubhouse();
    test_rating();
    test_winners_stay();
    test_you_stay();
    test_you_lose();
    test_stay_nobody_waiting();
    test_stay_soak();
    test_pick_cycle();
    test_pick_called();
    test_pick_vs_winners();
    test_pick_soak();
    printf("open play host checks passed\n");
    return 0;
}
