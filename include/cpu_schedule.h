#ifndef CPU_SCHEDULE_H
#define CPU_SCHEDULE_H

#include "ai.h"
#include "game.h"
#include "input.h"
#include "serve.h"

/*
 * When a CPU thinks (ADR-018, ADR-069). Every frame while it plays the
 * ball, is alone on its side, or serves; otherwise (a doubles CPU covering
 * its lane) on one frame in four, staggered by slot, repeating its
 * movement in between (ai_hold_intent). One definition for main.c, the
 * host harnesses, the conformance traces and the lockstep.
 *
 * The stagger counts the match's frames (g_game_state.frame_count, 0 at
 * each new match), so the same inputs play the same match however long
 * the menus took (KNOWN_ISSUES #37; it counted the main loop since
 * power-on before). Macros so every index is a constant: 816-tcc
 * multiplies for each variable g_players[i] / g_ai_slots[i] access.
 */
#define CPU_FRAME() ((u8)g_game_state.frame_count)
/* The slot is serving: the serve setup and toss only. server_index keeps
 * the last server through the rally, and a lane cover thinking every frame
 * there put a third think on most rally frames (M4-UI-03/04). */
#define CPU_SERVING(slot) \
    (g_serve_state.server_index == (slot) && \
     g_game_state.phase <= GAME_PHASE_SERVE_TOSS)
#define CPU_THINKS(slot) \
    (!g_players[(slot) ^ 1].active || g_ai_slots[slot].has_target || \
     CPU_SERVING(slot) || (CPU_FRAME() & 3) == (slot))

#endif
