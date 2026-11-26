#include "th02/music/assets.h"

#include <string.h>

static const char *const m_Paths[MUSIC_SONG_COUNT] = {
    "stage0.m", "stage1.m", "stage2.m", "stage3.m", "stage4.m",
    "stage5.m", "boss1.m",  "boss2.m",  "boss3.m",  "boss4.m",
    "boss5.m",  "mima.m",   "op.m",     "end1.m",   "ending.m",
};

const char *MusicAssets_Path(const MUSIC_ID id)
{
    return (uint32_t)id < MUSIC_SONG_COUNT ? m_Paths[id] : nullptr;
}

RESULT MusicAssets_Load(
    VFS *const files, const ALLOCATOR *const allocator, MUSIC_ASSETS *const out)
{
    memset(out, 0, sizeof(*out));
    MUSIC_ASSETS assets = {};
    for (size_t i = 0; i < MUSIC_SONG_COUNT; i++) {
        READER reader;
        RESULT result = Vfs_Open(files, m_Paths[i], VFS_STREAM, &reader);
        if (IS_OK(result)) {
            result = PmdData_LoadSong(&reader, allocator, &assets.songs[i]);
            Reader_Close(&reader);
        }
        if (!IS_OK(result)) {
            return Result_Prefix(result, "%s", m_Paths[i]);
        }
    }
    *out = assets;
    return OK;
}
