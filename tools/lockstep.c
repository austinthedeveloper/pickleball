/*
 * The ROM against the host C, frame by frame (ADR-069).
 *
 * Reads a `mesen_probe.py lockstep-record` file: the ROM's match config,
 * intents and simulation state on every frame of a timeline. From the
 * first frame of the timeline's match it replays the match here: humans
 * play the ROM's recorded intents, the CPUs think on their schedule
 * (CPU_THINKS, on the ROM's loop count), and after each update every simulation field is compared
 * with the ROM's. The first difference is printed and fails the run.
 *
 * The ROM runs src/asm/player_step.asm and src/asm/ai_probe.asm and the C
 * as 816-tcc compiled it; the host runs src/player.c's player_step, the C
 * probe, and the C as cc compiled it. Agreement means the asm matches its
 * C twin, the compiler didn't change the meaning, and the conformance
 * traces (tools/conformance.c) describe the ROM.
 *
 *   make lockstep
 */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ai.h"
#include "cpu_schedule.h"
#include "game.h"
#include "match.h"
#include "serve.h"

typedef struct {
    const char *name;
    unsigned offset;
    unsigned size;
} Field;

#define F(type, field) \
    { #field, offsetof(type, field), sizeof(((type *)0)->field) }
#define END { 0, 0, 0 }

static const Field s_game_fields[] = {
    F(GameState, paused), F(GameState, phase), F(GameState, phase_timer),
    F(GameState, last_fault), F(GameState, point_winner),
    F(GameState, team_score), F(GameState, required_bounce),
    F(GameState, serving_team), F(GameState, match_winner),
    F(GameState, hit_stop), F(GameState, shot_serial),
    F(GameState, frame_count), END
};
static const Field s_player_fields[] = {
    F(Player, active), F(Player, team), F(Player, logical_input_slot),
    F(Player, cpu_controlled), F(Player, x_fp), F(Player, y_fp),
    F(Player, vx_fp), F(Player, vy_fp), F(Player, max_speed_fp),
    F(Player, facing_left), F(Player, animation_frame),
    F(Player, animation_timer), F(Player, state),
    F(Player, state_frames_remaining), F(Player, contact_consumed),
    F(Player, frozen), F(Player, pose), F(Player, palette),
    F(Player, flash_timer), F(Player, shot_type), F(Player, variant),
    F(Player, paddle), END
};
static const Field s_ball_fields[] = {
    F(Ball, active), F(Ball, x_fp), F(Ball, y_fp), F(Ball, z_fp),
    F(Ball, vx_fp), F(Ball, vy_fp), F(Ball, vz_fp), F(Ball, last_hitter),
    F(Ball, bounce_count), F(Ball, rally_state), END
};
static const Field s_serve_fields[] = {
    F(ServeState, server_index), F(ServeState, receiver_index),
    F(ServeState, landing_pending), F(ServeState, requested),
    F(ServeState, aim_x), F(ServeState, aim_y), F(ServeState, server_y_fp),
    END
};
static const Field s_charge_fields[] = {
    F(ServeCharge, charging), F(ServeCharge, level), F(ServeCharge, rising),
    F(ServeCharge, armed), F(ServeCharge, power), END
};
static const Field s_ai_fields[] = {
    F(AiState, has_target), F(AiState, target_x), F(AiState, target_y),
    F(AiState, predicted_land_x), F(AiState, predicted_land_y),
    F(AiState, predicted_frames), F(AiState, plan_age),
    F(AiState, shot_counter), F(AiState, plan_key),
    F(AiState, reaction_frames), F(AiState, incoming_frames),
    F(AiState, late_period), F(AiState, returns), F(AiState, late_this_ball),
    F(AiState, seen_shot), F(AiState, planned_shot), F(AiState, planned_depth),
    END
};
static const Field s_intent_fields[] = {
    F(PlayerIntent, move_x), F(PlayerIntent, move_y), F(PlayerIntent, aim_x),
    F(PlayerIntent, aim_y), F(PlayerIntent, hit_pressed),
    F(PlayerIntent, shot), F(PlayerIntent, pause_pressed),
    F(PlayerIntent, hit_held), END
};

enum { PROBES = 11, MAX_BYTES = 128 };
static const char *const s_probe_names[PROBES] = {
    "cfg", "attract", "owners", "intents", "charge",
    "game", "players", "ball", "serve", "ai", "loops"
};

/* main.c's loop count, which CPU_THINKS reads: here, the ROM's. */
u16 g_loop_count;

typedef struct {
    unsigned frame;
    unsigned char bytes[PROBES][MAX_BYTES];
} Row;

static Row *s_rows;
static unsigned s_count;

static int hex(char c)
{
    return c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10;
}

static void load(const char *path)
{
    FILE *in = fopen(path, "r");
    static char line[4096];
    unsigned capacity = 0;

    if (!in) {
        perror(path);
        exit(2);
    }
    while (fgets(line, sizeof(line), in)) {
        Row *row;
        char *word;

        if (strncmp(line, "frame ", 6))
            continue;
        if (s_count == capacity) {
            capacity = capacity ? capacity * 2 : 1024;
            s_rows = realloc(s_rows, capacity * sizeof(Row));
        }
        row = &s_rows[s_count++];
        memset(row, 0, sizeof(*row));
        row->frame = (unsigned)strtoul(line + 6, &word, 10);
        for (word = strtok(word, " \n"); word; word = strtok(0, " \n")) {
            char *eq = strchr(word, '=');
            unsigned p;
            unsigned i;

            if (!eq)
                continue;
            *eq = 0;
            for (p = 0; p < PROBES; ++p)
                if (!strcmp(word, s_probe_names[p]))
                    break;
            if (p == PROBES)
                continue;
            for (i = 0; eq[1 + 2 * i] && eq[2 + 2 * i] && i < MAX_BYTES; ++i)
                row->bytes[p][i] = (unsigned char)(hex(eq[1 + 2 * i]) << 4 |
                    hex(eq[2 + 2 * i]));
        }
    }
    fclose(in);
}

enum { P_CFG, P_ATTRACT, P_OWNERS, P_INTENTS, P_CHARGE, P_GAME, P_PLAYERS,
       P_BALL, P_SERVE, P_AI, P_LOOPS };

static unsigned u16_at(const unsigned char *b, unsigned offset)
{
    return b[offset] | b[offset + 1] << 8;
}

static long value(const unsigned char *b, const Field *f)
{
    if (f->size == 1)
        return b[f->offset];
    return (short)u16_at(b, f->offset);
}

/* Compares `count` structs field by field (padding left out). */
static int differs(const char *what, const Field *fields, const void *host,
    const unsigned char *rom, unsigned size, unsigned count,
    const Row *row, unsigned match_frame)
{
    unsigned n;
    const Field *f;

    for (n = 0; n < count; ++n)
        for (f = fields; f->name; ++f) {
            const unsigned char *h = (const unsigned char *)host + n * size;
            const unsigned char *r = rom + n * size;
            unsigned i;

            for (i = 0; i < f->size; ++i)
                if (h[f->offset + i] != r[f->offset + i])
                    break;
            if (i == f->size)
                continue;
            printf("lockstep: differs at emulator frame %u (match frame "
                "%u): %s", row->frame, match_frame, what);
            if (count > 1)
                printf("[%u]", n);
            printf(".%s: host %ld, ROM %ld\n", f->name, value(h, f),
                value(r, f));
            return 1;
        }
    return 0;
}

static int compare(const Row *row, const PlayerIntent *intents,
    unsigned match_frame)
{
    return differs("intent", s_intent_fields, intents,
                row->bytes[P_INTENTS], sizeof(PlayerIntent), MAX_PLAYERS,
                row, match_frame) ||
        differs("g_game_state", s_game_fields, &g_game_state,
            row->bytes[P_GAME], sizeof(GameState), 1, row, match_frame) ||
        differs("g_players", s_player_fields, g_players,
            row->bytes[P_PLAYERS], sizeof(Player), MAX_PLAYERS, row,
            match_frame) ||
        differs("g_ball", s_ball_fields, &g_ball, row->bytes[P_BALL],
            sizeof(Ball), 1, row, match_frame) ||
        differs("g_serve_state", s_serve_fields, &g_serve_state,
            row->bytes[P_SERVE], sizeof(ServeState), 1, row, match_frame) ||
        differs("g_serve_charge", s_charge_fields, &g_serve_charge,
            row->bytes[P_CHARGE], sizeof(ServeCharge), 1, row,
            match_frame) ||
        differs("g_ai_slots", s_ai_fields, g_ai_slots, row->bytes[P_AI],
            sizeof(AiState), MAX_PLAYERS, row, match_frame);
}

int main(int argc, char **argv)
{
    PlayerIntent intents[MAX_PLAYERS];
    const PlayerIntent *rom_intents;
    unsigned start;
    unsigned previous;
    unsigned steps = 0;
    unsigned i;
    u8 slot;

    if (argc != 2) {
        fprintf(stderr, "usage: lockstep RECORD\n");
        return 2;
    }
    load(argv[1]);
    /* The first update of a match that isn't the attract demo. */
    for (start = 0; start < s_count; ++start)
        if (s_rows[start].frame >= 30 && !s_rows[start].bytes[P_ATTRACT][0]
                && u16_at(s_rows[start].bytes[P_GAME],
                    offsetof(GameState, frame_count)) == 1)
            break;
    if (start == s_count) {
        printf("lockstep: %s: no match found\n", argv[1]);
        return 1;
    }
    memcpy(&g_match_config, s_rows[start].bytes[P_CFG], sizeof(MatchConfig));
    g_attract = 0;
    game_new_match();
    previous = 0;
    for (i = start; i < s_count; ++i) {
        const Row *row = &s_rows[i];
        unsigned frame_count = u16_at(row->bytes[P_GAME],
            offsetof(GameState, frame_count));

        if (row->bytes[P_ATTRACT][0])
            break;                      /* back to the title */
        if (frame_count == previous)
            continue;                   /* no update on this frame */
        if (memcmp(g_input_owners, row->bytes[P_OWNERS], MAX_PLAYERS)) {
            printf("lockstep: input owners differ at emulator frame %u\n",
                row->frame);
            return 1;
        }
        rom_intents = (const PlayerIntent *)row->bytes[P_INTENTS];
        memcpy(intents, rom_intents, sizeof(intents));
        /* The ROM's loop count this frame (profile_frame_begin counts it
         * before the CPUs think). */
        g_loop_count = (u16)u16_at(row->bytes[P_LOOPS], 0);
        for (slot = 0; slot < MAX_PLAYERS; ++slot) {
            if (!g_players[slot].active ||
                    g_input_owners[slot] != INPUT_OWNER_CPU)
                continue;
            if (CPU_THINKS(slot))
                ai_update(slot, &intents[slot]);
            else
                ai_hold_intent(slot, &intents[slot]);
        }
        game_update(intents);
        if (compare(row, intents, g_game_state.frame_count))
            return 1;
        previous = frame_count;
        ++steps;
    }
    if (steps < 300) {
        printf("lockstep: %s: only %u match frames\n", argv[1], steps);
        return 1;
    }
    printf("lockstep: %s: %u match frames identical (emulator frames "
        "%u-%u)\n", argv[1], steps, s_rows[start].frame,
        s_rows[i - 1].frame);
    return 0;
}
