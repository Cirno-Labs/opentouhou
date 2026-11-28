#include "th01/boss/boss.h"

#include "th01/ptn.h"
#include "th01/session.h"

#include "core/vfs/open.h"

static BOSS_STATE *m_Boss;
BOSS_STATE *const *const g_Boss = &m_Boss;
static bool m_TestDamage;
const bool *const g_TestDamage = &m_TestDamage;

void Boss_Init(void)
{
    m_Boss = nullptr;
    m_TestDamage = false;
}

int16_t Boss_Rank(
    const int16_t easy, const int16_t normal, const int16_t hard,
    const int16_t lunatic)
{
    switch (g_Session->settings.difficulty) {
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

RESULT BossAssets_LoadBos(
    VFS *const vfs, const char *const name, const ALLOCATOR *const allocator,
    BOS *const out)
{
    READER reader;
    MUST(Vfs_Open(vfs, name, VFS_IN_MEMORY, &reader));
    const RESULT result = Bos_Load(&reader, allocator, out);
    Reader_Close(&reader);
    return result;
}

RESULT BossAssets_LoadBackground(
    VFS *const vfs, const char *const name, const ALLOCATOR *const allocator,
    PI_IMAGE *const out)
{
    READER reader;
    MUST(Vfs_Open(vfs, name, VFS_IN_MEMORY, &reader));
    const RESULT result = Pi_Load(&reader, allocator, out);
    Reader_Close(&reader);
    return result;
}

RESULT BossAssets_LoadMissiles(
    VFS *const vfs, const ALLOCATOR *const allocator, PTN *const out)
{
    READER reader;
    MUST(Vfs_Open(vfs, "BOSS3_M.PTN", VFS_IN_MEMORY, &reader));
    const RESULT result = Ptn_Load(&reader, allocator, out);
    Reader_Close(&reader);
    return result;
}

void Boss_Select(BOSS_STATE *const state)
{
    m_Boss = state;
}

void Boss_Deselect(void)
{
    m_Boss = nullptr;
}

void Boss_ClearTestDamage(void)
{
    m_TestDamage = false;
}
