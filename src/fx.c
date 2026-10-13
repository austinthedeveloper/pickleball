#include "court.h"
#include "events.h"
#include "fx.h"

Fx g_fx;

void fx_initialize(void)
{
    g_fx.spark_timer = 0;
    g_fx.spark_x = 0;
    g_fx.spark_y = 0;
}

void fx_on_event(u8 kind, s16 x, s16 y, u8 detail)
{
    if (kind != EVENT_HIT)
        return;
    /* detail is the hitter's slot; slots 2-3 are the right team. */
    g_fx.spark_timer = FX_SPARK_FRAMES;
    g_fx.spark_x = x + court_toward_net(detail >= 2) * FX_SPARK_LEAD_X;
    g_fx.spark_y = y;
}

void fx_tick(void)
{
    if (g_fx.spark_timer)
        --g_fx.spark_timer;
}
