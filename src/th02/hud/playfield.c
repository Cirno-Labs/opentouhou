#include "th02/hud/playfield.h"

#include "th02/gaiji.h"
#include "th02/tram.h"

#include <string.h>

void Hud_WipePlayfield(void)
{
    const TRAM_CHARACTER space = { .code = 0x20 };
    for (uint16_t y = 1; y < 24; y++) {
        for (uint16_t x = 4; x < 52; x++) {
            Tram_PutCharacter(x, y, &space, TRAM_WHITE);
        }
    }
}

static void M_Draw(const HUD_WIPE *const wipe)
{
    for (uint16_t x = 0; x < 24; x++) {
        int cel = wipe->frame - (x / 2);
        if (cel < 0) {
            cel = 0;
        } else if (cel > 8) {
            cel = 8;
        }
        for (uint16_t y = 1; y < 24; y++) {
            Tram_PutGaiji(4 + x * 2, y, wipe->first + cel, TRAM_BLACK);
        }
    }
}

void Hud_BeginWipe(HUD_WIPE *const wipe, const bool leave)
{
    memset(wipe, 0, sizeof(*wipe));
    wipe->first = leave ? GAIJI_WIPE_CLOSE : GAIJI_WIPE_OPEN;
    M_Draw(wipe);
    Wait_Delay(&wipe->wait, 2);
}

bool Hud_UpdateWipe(HUD_WIPE *const wipe)
{
    if (wipe->complete) {
        return true;
    }
    if (!Wait_Ready(&wipe->wait, nullptr, 0)) {
        return false;
    }
    wipe->frame++;
    if (wipe->frame == 21) {
        wipe->complete = true;
        return true;
    }
    M_Draw(wipe);
    Wait_Delay(&wipe->wait, 2);
    return false;
}
