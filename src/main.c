#include <snes.h>
#include "ai.h"
#include "ball.h"
#include "game.h"
#include "input.h"
#include "player.h"
#include "render.h"
#include "profile.h"
#include "serve.h"

#define REG_SLHV_LATCH (*(vuint8 *)0x2137)
#define REG_OPVCT_READ (*(vuint8 *)0x213D)

/* Main-loop iterations that missed their VBlank (frame-overrun latch). */
u16 g_overrun_frames;
/* Scanline reached when the frame's CPU work finished; worst case kept. */
u16 g_work_scanline;
u16 g_work_scanline_max;
/* Per-stage worst and wrapping-sum scanline costs (see profile.h). */
u16 g_stage_scanline_max[PROFILE_STAGES];
u16 g_stage_scanline_sum[PROFILE_STAGES];

static u16 read_scanline(void)
{
    u8 low;
    u8 high;
    low = REG_STAT78;           /* reset the OPVCT read flip-flop */
    low = REG_SLHV_LATCH;             /* latch H/V counters */
    low = REG_OPVCT_READ;
    high = REG_OPVCT_READ;
    return ((u16)(high & 1) << 8) | low;
}

#ifdef BUILD_PROFILE
static u16 s_stage_start;

/* Records the worst duration (in scanlines) of each main-loop stage. */
void profile_mark(u8 stage)
{
    u16 line = read_scanline();
    u16 used = line >= s_stage_start ? line - s_stage_start :
        line + 262 - s_stage_start;
    if (used > g_stage_scanline_max[stage])
        g_stage_scanline_max[stage] = used;
    g_stage_scanline_sum[stage] += used;
    s_stage_start = line;
}
#endif

int main(void)
{
    u16 last_vblank;
    u16 now;

    input_initialize();
    players_initialize();
    ball_initialize();
    serve_initialize();
    ai_initialize();
    game_new_match();
    render_initialize();
    g_overrun_frames = 0;
    g_work_scanline_max = 0;
    for (now = 0; now < PROFILE_STAGES; ++now) {
        g_stage_scanline_max[now] = 0;
        g_stage_scanline_sum[now] = 0;
    }
    last_vblank = snes_vblank_count;

    while (1) {
#ifdef BUILD_PROFILE
        s_stage_start = read_scanline();
#endif
        input_capture_frame();
        ai_update(2, &g_player_intents[2]);
        PROFILE_MARK(0);
        game_update(g_player_intents);
        PROFILE_MARK(1);
        render_prepare();
        PROFILE_MARK(7);
        g_work_scanline = read_scanline();
        if (g_work_scanline < 225 && g_work_scanline > g_work_scanline_max)
            g_work_scanline_max = g_work_scanline;
        WaitForVBlank();
        render_commit();
        now = snes_vblank_count;
        if ((u16)(now - last_vblank) > 1)
            ++g_overrun_frames;
        last_vblank = now;
    }

    return 0;
}
