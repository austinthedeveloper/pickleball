#include <snes.h>
#include "input.h"

/* The only module that touches pad hardware (docs/04). */

#ifdef BUILD_DEBUG
/* Debug ROM only: with g_input_inject_on set, pad positions 1-4 read
 * g_input_inject instead of the library, so headless runs press
 * controllers 2-4 with --poke (the runner presses port 1 only, a poke of
 * the library's pad_keys is overwritten by its VBlank read, and without a
 * Multitap positions 2-4 hold power-on RAM; KNOWN_ISSUES #16). */
u8 g_input_inject_on;
u16 g_input_inject[MAX_PADS];
#define PAD(n) (g_input_inject_on ? g_input_inject[n] : padsCurrent(n))
#else
#define PAD(n) padsCurrent(n)
#endif

void input_initialize(void)
{
    /* Boot-time Multitap detection through the library path; hot-plugging
     * is unsupported. */
    detectMPlay5();
    input_configure(snes_mplay5 != 0);
#ifdef BUILD_DEBUG
    {
        u8 i;
        g_input_inject_on = 0;
        for (i = 0; i < MAX_PADS; ++i)
            g_input_inject[i] = 0;
    }
#endif
}

void input_capture_frame(void)
{
    static u16 raw[MAX_PADS];

    /* Constant indices: 816-tcc multiplies for variable array indexing. */
    raw[0] = padsCurrent(0);
    raw[1] = PAD(1);
    raw[2] = PAD(2);
    raw[3] = PAD(3);
    raw[4] = PAD(4);
    input_map_frame(raw);
}
