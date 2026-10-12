#include "input.h"

void input_update_snapshot(PadSnapshot *snapshot, u16 current)
{
    u16 previous = snapshot->held;
    snapshot->held = current;
    snapshot->pressed = current & (u16)~previous;
    snapshot->released = previous & (u16)~current;
}

