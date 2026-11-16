#include "th01/frame.h"

#include "core/base/assert.h"

#define M_VSYNC_RATE 105263
#define M_HOST_RATE 111936

bool Frame_Advance(FRAME_CLOCK *const clock)
{
    ASSERT(clock->phase < M_HOST_RATE);
    clock->phase += M_VSYNC_RATE;
    if (clock->phase < M_HOST_RATE) {
        return false;
    }
    clock->phase -= M_HOST_RATE;
    return true;
}
