#include "th01/boss.h"

#include "core/vfs/open.h"
#include "th01/session.h"

int16_t Boss_Rank(
    const int16_t easy, const int16_t normal, const int16_t hard,
    const int16_t lunatic)
{
    switch (g_Session.settings.difficulty) {
    case SETTINGS_DIFFICULTY_EASY:
        return easy;
    case SETTINGS_DIFFICULTY_HARD:
        return hard;
    case SETTINGS_DIFFICULTY_LUNATIC:
        return lunatic;
    default:
        return normal;
    }
}

RESULT Boss_LoadBos(
    VFS *const vfs, const char *const name, const ALLOCATOR *const allocator,
    BOS *const out)
{
    READER reader;
    MUST(Vfs_Open(vfs, name, VFS_IN_MEMORY, &reader));
    const RESULT result = Bos_Load(&reader, allocator, out);
    Reader_Close(&reader);
    return result;
}

RESULT Boss_LoadBackground(
    VFS *const vfs, const char *const name, const ALLOCATOR *const allocator,
    PI_IMAGE *const out)
{
    READER reader;
    MUST(Vfs_Open(vfs, name, VFS_IN_MEMORY, &reader));
    const RESULT result = Pi_Load(&reader, allocator, out);
    Reader_Close(&reader);
    return result;
}

PLAYER *Boss_Player(const BOSS_HIT_CONTEXT *const hit)
{
    return &g_Player;
}
