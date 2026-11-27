#include "tests/test.h"
#include "tests/test_dir.h"

#include "core/base/memory.h"
#include "core/base/string.h"
#include "shell/game/entry.h"
#include "shell/log_file.h"

#include <string.h>

static const char *const m_DataFiles[] = { "first.dat", "second.dat" };

static const GAME_DESC m_Desc = {
    .id = "test",
    .title = "Test",
    .version = "0",
    .data_files = m_DataFiles,
    .data_file_count = 2,
};

TEST(missing_data_folder_names_the_first_data_file)
{
    char *const dir = TestDir_Prepare("missing_data_folder");
    char *const data_dir = TestDir_Join(dir, "missing");
    char *const user_dir = TestDir_Join(dir, "user");
    CHECK(Result_Log(File_MakeDir(user_dir), LOG_LEVEL_ERROR));
    char *argv[] = {
        "test", "--data", data_dir, "--user-dir", user_dir, nullptr,
    };
    CHECK_EQ_INT(Shell_Init(&m_Desc, 5, argv), SHELL_FAILURE);
    // Windows keeps the log file locked until Shell_Quit closes it.
    char *const path = Str_Dup(LogFile_GetPath());
    Shell_Quit();
    char *const log = TestDir_Read(path);
    CHECK(log != nullptr && strstr(log, "first.dat: ") != nullptr);
    Mem_Free(log);
    Mem_Free(path);
    Mem_Free(user_dir);
    Mem_Free(data_dir);
    Mem_Free(dir);
}
