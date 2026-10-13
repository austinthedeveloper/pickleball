#define PALETTE_PARTS_DEFINE
#include "match.h"

#ifdef HOST_TEST
/* Host tests start from the singles defaults without calling them; on the
 * SNES main() calls match_config_defaults() (RAM is not initialized). */
MatchConfig g_match_config = {
    MATCH_SINGLES,
    { PAL_BLUE, MATCH_NO_PALETTE, PAL_RED, MATCH_NO_PALETTE },
    { 0, 0, 0, 0 },
    { PAL_BLUE, 0, PAL_RED, 0 }
};
#else
MatchConfig g_match_config;
#endif

void match_config_defaults(void)
{
    u8 i;

    g_match_config.mode = MATCH_SINGLES;
    for (i = 0; i < MAX_PLAYERS; ++i) {
        g_match_config.palette_id[i] = MATCH_NO_PALETTE;
        g_match_config.skin_id[i] = 0;
        g_match_config.paddle_id[i] = 0;
    }
    g_match_config.palette_id[0] = PAL_BLUE;
    g_match_config.paddle_id[0] = PAL_BLUE;
    g_match_config.palette_id[2] = PAL_RED;
    g_match_config.paddle_id[2] = PAL_RED;
}

void match_config_doubles(void)
{
    u8 i;

    match_config_defaults();
    g_match_config.mode = MATCH_DOUBLES;
    g_match_config.palette_id[1] = PAL_GOLD;
    g_match_config.palette_id[3] = PAL_GREEN;
    for (i = 0; i < MAX_PLAYERS; ++i)
        g_match_config.paddle_id[i] = g_match_config.palette_id[i];
}

u8 match_slot_active(u8 slot)
{
    return g_match_config.palette_id[slot] != MATCH_NO_PALETTE;
}

u8 match_slot_lane_top(u8 slot)
{
    return slot == 1 || slot == 2;
}

u8 match_config_palettes_unique(void)
{
    u8 i;
    u8 j;

    for (i = 0; i < MAX_PLAYERS; ++i) {
        if (g_match_config.palette_id[i] == MATCH_NO_PALETTE)
            continue;
        for (j = i + 1; j < MAX_PLAYERS; ++j)
            if (g_match_config.palette_id[j] == g_match_config.palette_id[i])
                return 0;
    }
    return 1;
}

/* Contract v2 layout: 1 outline, 2-4 skin, 5-10 team, 11-13 paddle,
 * 14 highlight, 15 hair. */
void match_compose_palette(u8 slot, u16 out[16])
{
    match_compose_parts(g_match_config.palette_id[slot],
        g_match_config.skin_id[slot], g_match_config.paddle_id[slot], out);
}

void match_compose_parts(u8 palette_id, u8 skin_id, u8 paddle_id,
    u16 out[16])
{
    const u16 *team = g_pal_team[palette_id & 3];
    const u16 *skin = g_pal_skin[skin_id % SKIN_COUNT];
    const u16 *paddle = g_pal_paddle[paddle_id & 3];
    u8 i;

    out[0] = 0;
    out[1] = g_pal_shared[0][0];
    for (i = 0; i < 3; ++i) {
        out[2 + i] = skin[i];
        out[11 + i] = paddle[i];
    }
    for (i = 0; i < 6; ++i)
        out[5 + i] = team[i];
    out[14] = g_pal_shared[0][1];
    out[15] = skin[3];
}

u8 match_team_palette(u8 team)
{
    return g_match_config.palette_id[team ? 2 : 0] & 3;
}
