// SPDX-License-Identifier: GPL-3.0-only

#include "core/app/log_file.h"

#include "core/base/log.h"
#include "core/base/memory.h"

#include "test.h"
#include "test_dir.h"

#include <SDL3/SDL.h>
#include <stdio.h>

// Returns the text of the log at path, or nullptr when it cannot be read.
// stdio reads it as text, so a line ends with "\n" on every platform, and
// on Windows it shares the file with the log that is still open.
static char *M_LoadLog(const char *const path)
{
    FILE *const file = fopen(path, "r");
    if (file == nullptr) {
        return nullptr;
    }
    char *const text = Mem_Alloc(4096);
    text[fread(text, 1, 4095, file)] = '\0';
    fclose(file);
    return text;
}

TEST(file_is_named_after_the_executable)
{
    char *const dir = TestDir_Prepare("named");
    CHECK(Result_Log(AppLogFile_Open(dir, "th06"), LOG_LEVEL_ERROR));
    char *const path = TestDir_Join(dir, "oth06.log");
    CHECK_EQ_STR(AppLogFile_GetPath(), path);
    AppLogFile_Close();
    CHECK(AppLogFile_GetPath() == nullptr);
    Mem_Free(path);
    Mem_Free(dir);
}

TEST(file_holds_the_lines_logged_while_open)
{
    char *const dir = TestDir_Prepare("lines");
    LOG_INFO("before");
    CHECK(Result_Log(AppLogFile_Open(dir, "th06"), LOG_LEVEL_ERROR));
    Log_Message(LOG_LEVEL_WARNING, "src/core/foo.c", 9, "while %s", "open");
    char *const path = TestDir_Join(dir, "oth06.log");
    char *text = M_LoadLog(path);
    CHECK_EQ_STR(text, "WARNING foo.c:9: while open\n");
    Mem_Free(text);
    AppLogFile_Close();
    LOG_INFO("after");
    text = M_LoadLog(path);
    CHECK_EQ_STR(text, "WARNING foo.c:9: while open\n");
    Mem_Free(text);
    Mem_Free(path);
    Mem_Free(dir);
}

TEST(file_of_an_earlier_start_is_replaced)
{
    char *const dir = TestDir_Prepare("replaced");
    CHECK(Result_Log(AppLogFile_Open(dir, "th06"), LOG_LEVEL_ERROR));
    LOG_INFO("first start");
    AppLogFile_Close();
    CHECK(Result_Log(AppLogFile_Open(dir, "th06"), LOG_LEVEL_ERROR));
    Log_Message(LOG_LEVEL_INFO, "src/core/foo.c", 2, "second start");
    AppLogFile_Close();
    char *const path = TestDir_Join(dir, "oth06.log");
    char *const text = M_LoadLog(path);
    CHECK_EQ_STR(text, "INFO foo.c:2: second start\n");
    Mem_Free(text);
    Mem_Free(path);
    Mem_Free(dir);
}

TEST(missing_folder_reports_failure)
{
    char *const dir = TestDir_Prepare("missing");
    char *const missing = TestDir_Join(dir, "none");
    const RESULT result = AppLogFile_Open(missing, "th06");
    CHECK(!IS_OK(result));
    CHECK(strstr(result.message, "oth06.log") != nullptr);
    Result_Ignore(result);
    CHECK(AppLogFile_GetPath() == nullptr);
    Mem_Free(missing);
    Mem_Free(dir);
}

TEST(file_in_a_folder_longer_than_max_path_holds_the_lines)
{
    char *dir = TestDir_Prepare("long_path");
    for (int i = 0; i < 10; i++) {
        char *const deeper = TestDir_Join(dir, "a_directory_with_a_long_name");
        Mem_Free(dir);
        dir = deeper;
    }
    CHECK(Result_Log(File_MakeDir(dir), LOG_LEVEL_ERROR));
    CHECK(Result_Log(AppLogFile_Open(dir, "th06"), LOG_LEVEL_ERROR));
    Log_Message(LOG_LEVEL_INFO, "src/core/foo.c", 3, "deep");
    AppLogFile_Close();
    char *const path = TestDir_Join(dir, "oth06.log");
    char *const text = TestDir_Read(path);
    CHECK(text != nullptr && Str_StartsWith(text, "INFO foo.c:3: deep"));
    Mem_Free(text);
    Mem_Free(path);
    Mem_Free(dir);
}
