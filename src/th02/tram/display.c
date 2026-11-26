#include "th02/tram/display.h"

#include <string.h>

void TramDisplay_Init(TRAM_DISPLAY *const display, const uint8_t blink_frame)
{
    memset(display, 0, sizeof(*display));
    display->blink_frame = blink_frame & 127;
    display->visible = true;
}

void TramDisplay_VSync(TRAM_DISPLAY *const display)
{
    display->blink_frame = (display->blink_frame + 1) & 127;
}

void TramDisplay_Clock(TRAM_DISPLAY *const display, const uint64_t microseconds)
{
    // The 21.0526 MHz dot clock scans 848 by 440 dots per refresh. The
    // display starts its first refresh at time zero.
    const uint64_t frames = microseconds / 1865600000ULL * 105263ULL
        + microseconds % 1865600000ULL * 105263ULL / 1865600000ULL;
    display->blink_frame = (frames + 1) & 127;
}

bool TramDisplay_BlinkVisible(const TRAM_DISPLAY *const display)
{
    return display->blink_frame >= 32;
}
