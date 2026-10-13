#include <assert.h>
#include <stdio.h>
#include "match.h"

int main(void)
{
    u16 pal[16];

    match_config_defaults();
    assert(g_match_config.palette_id[0] == PAL_BLUE);
    assert(g_match_config.palette_id[2] == PAL_RED);
    assert(g_match_config.palette_id[1] == MATCH_NO_PALETTE);
    assert(match_config_palettes_unique());
    assert(match_team_palette(0) == PAL_BLUE);
    assert(match_team_palette(1) == PAL_RED);

    /* Contract v2 composition (ADR-013). */
    match_compose_palette(0, pal);
    assert(pal[0] == 0);
    assert(pal[1] == g_pal_shared[0][0]);
    assert(pal[2] == g_pal_skin[0][0] && pal[4] == g_pal_skin[0][2]);
    assert(pal[5] == g_pal_team[PAL_BLUE][0]);
    assert(pal[10] == g_pal_team[PAL_BLUE][5]);
    assert(pal[11] == g_pal_paddle[PAL_BLUE][0]);
    assert(pal[13] == g_pal_paddle[PAL_BLUE][2]);
    assert(pal[14] == g_pal_shared[0][1]);
    assert(pal[15] == g_pal_skin[0][3]);

    /* Colour is per match, not per slot: gold vs green is valid (ADR-010). */
    g_match_config.palette_id[0] = PAL_GOLD;
    g_match_config.palette_id[2] = PAL_GREEN;
    assert(match_config_palettes_unique());
    assert(match_team_palette(1) == PAL_GREEN);
    match_compose_palette(2, pal);
    assert(pal[6] == g_pal_team[PAL_GREEN][1]);

    g_match_config.palette_id[2] = PAL_GOLD;
    assert(!match_config_palettes_unique());
    puts("match host checks passed");
    return 0;
}
