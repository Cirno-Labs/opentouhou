#include "files.h"
#include "harness.h"

#include "th01/input.h"
#include "th01/score_data.h"
#include "th01/settings.h"

#include "core/base/string.h"
#include "core/base/utils.h"
#include "test.h"

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

TEST(staged_config_reads_as_the_default_settings)
{
    const STAGED_FILE *const file = M_File("reiiden.cfg");
    READER reader;
    Reader_OpenBuffer(&reader, file->data, file->size, file->path);
    TH01_SETTINGS settings = {};
    CHECK(
        Result_Log(Settings_ReadOriginal(&reader, &settings), LOG_LEVEL_ERROR));
    Reader_Close(&reader);
    CHECK_EQ_INT(settings.difficulty, SETTINGS_DIFFICULTY_NORMAL);
    CHECK_EQ_INT(settings.music, SETTINGS_MUSIC_FM);
    CHECK_EQ_INT(settings.bombs, 1);
    CHECK_EQ_INT(settings.lives_extra, 2);
}

TEST(staged_score_files_hold_the_default_tables)
{
    SCORE_TABLE defaults;
    ScoreData_Defaults(&defaults);
    for (int32_t rank = 0; rank < SCORE_RANKS; rank++) {
        const STAGED_FILE *const file = M_File(ScoreData_File(rank));
        CHECK_EQ_INT(file->size, SCORE_FILE_SIZE);
        READER reader;
        Reader_OpenBuffer(&reader, file->data, file->size, file->path);
        SCORE_TABLE table;
        CHECK(Result_Log(ScoreData_Read(&reader, &table), LOG_LEVEL_ERROR));
        Reader_Close(&reader);
        for (int32_t i = 0; i < SCORE_PLACES; i++) {
            const SCORE_ENTRY *const expected = &defaults.entries[i];
            const SCORE_ENTRY *const actual = &table.entries[i];
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
