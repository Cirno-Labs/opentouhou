#include "th01/bonus.h"

#include "core/base/assert.h"

#include <string.h>

void Bonus_RowShift32(
    VIDEO *const video, const uint16_t y, const uint16_t transferred)
{
    ASSERT(y < VIDEO_HEIGHT && transferred <= 76 && transferred % 4 == 0);
    uint8_t *const row = video->pages[0] + (size_t)y * VIDEO_WIDTH;
    const uint8_t *const source = video->pages[1] + (size_t)y * VIDEO_WIDTH;
    if ((y & 1) != 0) {
        memmove(row + 32, row, VIDEO_WIDTH - 32);
        memcpy(row, source + VIDEO_WIDTH - 32 - transferred * 8, 32);
    } else {
        memmove(row, row + 32, VIDEO_WIDTH - 32);
        memcpy(row + VIDEO_WIDTH - 32, source + transferred * 8, 32);
    }
    Video_SelectDrawingPage(video, 0);
}

void Bonus_BeginRowTransition(BONUS_ROW_TRANSITION *const t)
{
    ASSERT(!t->pending);
    *t = (BONUS_ROW_TRANSITION) { .previous_done = 1, .pending = true };
}

bool Bonus_ResumeRowTransition(
    BONUS_ROW_TRANSITION *const t, VIDEO *const video,
    PALETTE_STATE *const palette, const PALETTE *const target,
    FRAME_VSYNC *const vsync)
{
    while (t->pending) {
        if (!t->waiting) {
            for (uint16_t y = 0; y < VIDEO_HEIGHT; y++) {
                if (!t->active[y]) {
                    continue;
                }
                Bonus_RowShift32(video, y, t->transferred[y]);
                t->transferred[y] += 4;
                if (t->transferred[y] >= 80) {
                    t->active[y] = false;
                    t->done++;
                }
            }
            t->waiting = true;
        }
        if (!Frame_DelayReady(vsync, 2)) {
            return false;
        }
        t->waiting = false;
        if (t->done >= VIDEO_HEIGHT) {
            t->pending = false;
            return true;
        }
        if (t->activated < VIDEO_HEIGHT) {
            for (uint16_t y = t->activated; y < t->activated + 8; y++) {
                t->active[y] = true;
            }
        }
        t->activated += 8;
        if (t->done != t->previous_done && (t->activated & 15) == 0) {
            Palette_StepTo(palette, video, target, false);
        }
        t->previous_done = t->done;
    }
    return true;
}
