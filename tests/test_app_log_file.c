// SPDX-License-Identifier: GPL-3.0-only

#include "core/app/log_file.h"

#include "core/base/log.h"
#include "core/base/memory.h"

#include "test.h"
#include "test_dir.h"

#include <SDL3/SDL.h>

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
    char *text = SDL_LoadFile(path, nullptr);
    CHECK_EQ_STR(text, "WARNING foo.c:9: while open\n");
    SDL_free(text);
    AppLogFile_Close();
    LOG_INFO("after");
    text = SDL_LoadFile(path, nullptr);
    CHECK_EQ_STR(text, "WARNING foo.c:9: while open\n");
    SDL_free(text);
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
    char *const text = SDL_LoadFile(path, nullptr);
    CHECK_EQ_STR(text, "INFO foo.c:2: second start\n");
    SDL_free(text);
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
