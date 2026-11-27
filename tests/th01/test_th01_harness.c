#include "replay/th01/files.h"
#include "replay/th01/harness.h"
#include "tests/test.h"
#include "tests/test_dir.h"
#include "th01/input.h"
#include "th01/score_data.h"
#include "th01/settings.h"

#include "core/base/string.h"
#include "core/base/utils.h"
#include "core/vfs.h"

#include <string.h>

static const STAGED_FILE *M_File(const char *const path)
{
    for (size_t i = 0; i < ARRAY_SIZE(g_Th01Files); i++) {
        if (Str_EqualIgnoreCase(g_Th01Files[i].path, path)) {
            return &g_Th01Files[i];
        }
    }
    CHECK(false);
    return nullptr;
}

// Returns a VFS over a directory that holds the staged file at each path.
static VFS *M_Stage(const char *const *const paths, const size_t count)
{
    char *const directory = TestDir_Prepare("staged");
    for (size_t i = 0; i < count; i++) {
        const STAGED_FILE *const file = M_File(paths[i]);
        char *const path = TestDir_Join(directory, file->path);
        TestDir_Write(path, file->data, file->size);
        Mem_Free(path);
    }
    VFS *const vfs = Vfs_Create(&g_HeapAllocator);
    CHECK(Result_Log(
        Vfs_MountDir(vfs, "", directory, 0, false), LOG_LEVEL_ERROR));
    Mem_Free(directory);
    return vfs;
}

TEST(staged_config_reads_as_the_default_settings)
{
    VFS *const vfs = M_Stage((const char *const[]) { "reiiden.cfg" }, 1);
    VFS *const user_files = Vfs_Create(&g_HeapAllocator);
    TH01_SETTINGS settings = {};
    CHECK(Result_Log(
        Settings_ImportOriginal(vfs, user_files, &settings), LOG_LEVEL_ERROR));
    Vfs_Destroy(user_files);
    Vfs_Destroy(vfs);
    CHECK_EQ_INT(settings.difficulty, SETTINGS_DIFFICULTY_NORMAL);
    CHECK_EQ_INT(settings.music, SETTINGS_MUSIC_FM);
    CHECK_EQ_INT(settings.bombs, 1);
    CHECK_EQ_INT(settings.lives_extra, 2);
}

TEST(staged_score_files_hold_the_default_tables)
{
    static const char *const files[SCORE_RANKS] = {
        "REYHIES.DAT",
        "REYHINO.DAT",
        "REYHIHA.DAT",
        "REYHILU.DAT",
    };
    for (int32_t rank = 0; rank < SCORE_RANKS; rank++) {
        CHECK_EQ_INT(M_File(files[rank])->size, SCORE_FILE_SIZE);
    }
    VFS *const user_files = Vfs_Create(&g_HeapAllocator);
    SCORE_SAVES defaults;
    CHECK(Result_Ignore(ScoreData_Load(nullptr, user_files, &defaults)));
    VFS *const vfs = M_Stage(files, SCORE_RANKS);
    SCORE_SAVES staged;
    CHECK(
        Result_Log(ScoreData_Load(vfs, user_files, &staged), LOG_LEVEL_ERROR));
    Vfs_Destroy(vfs);
    Vfs_Destroy(user_files);
    for (int32_t rank = 0; rank < SCORE_RANKS; rank++) {
        for (int32_t i = 0; i < SCORE_PLACES; i++) {
            const SCORE_ENTRY *const expected =
                &defaults.tables[rank].entries[i];
            const SCORE_ENTRY *const actual = &staged.tables[rank].entries[i];
            CHECK(strcmp(actual->name, expected->name) == 0);
            CHECK(strcmp(actual->route, expected->route) == 0);
            CHECK_EQ_INT(actual->score, expected->score);
            CHECK_EQ_INT(actual->stage, expected->stage);
        }
    }
}

TEST(bindings_name_the_input_schema_buttons)
{
    for (size_t i = 0; i < g_Th01Harness.binding_count; i++) {
        const BINDING *const binding = &g_Th01Harness.bindings[i];
        bool found = false;
        for (size_t j = 0; j < g_InputSchema.count; j++) {
            found |= g_InputSchema.actions[j].type == INPUT_ACTION_BUTTON
                && strcmp(g_InputSchema.actions[j].name, binding->name) == 0;
        }
        CHECK(found);
    }
    CHECK_EQ_INT(g_Th01Harness.binding_count, g_InputSchema.count);
}
