#include "core/base/arena.h"
#include "pc98/mdrv.h"

int main(void)
{
    ARENA arena;
    Arena_Init(&arena, "MDRV", 4 * 1024 * 1024);
    static uint8_t bytes[] = { 0xd0, 8, 0xff, 0, 0, 0 };
    const MDRV_TRACK track = { .channel = 0x80 };
    const MDRV_MUSIC song = {
        .bytes = bytes,
        .size = sizeof(bytes),
        .tracks = &track,
        .track_count = 1,
        .fm_offset = 3,
        .ssg_offset = 3,
    };
    const MDRV_EFFECTS effects = { .bytes = bytes, .size = sizeof(bytes) };
    MDRV *mdrv;
    if (!Result_Log(
            Mdrv_Init(&song, 1, &effects, 0, 0, Arena_Allocator(&arena), &mdrv),
            LOG_LEVEL_ERROR)) {
        return 1;
    }
    Mdrv_Play(mdrv, 0);
    Mdrv_Render(mdrv, nullptr, 48000);
    return 0;
}
