#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/base/utils.h"
#include "core/io/backend.h"

#include "test.h"

#include <string.h>

static void M_CheckSystemPath(
    const char *const path, const char *const expected)
{
    char *const system_path = IoBackend_SystemPath(path);
    CHECK_EQ_STR(system_path, expected);
    Mem_Free(system_path);
}

#ifdef _WIN32
TEST(an_absolute_path_takes_the_extended_form)
{
    const char *const cases[][2] = {
        { "C:/th06/./replay/../紅魔郷.dat", "\\\\?\\C:\\th06\\紅魔郷.dat" },
        { "C:\\th06\\", "\\\\?\\C:\\th06" },
        { "C:/", "\\\\?\\C:\\" },
        { "//server/share/th06", "\\\\?\\UNC\\server\\share\\th06" },
        { "\\\\?\\C:\\th06\\.", "\\\\?\\C:\\th06\\." },
        { "\\\\.\\NUL", "\\\\.\\NUL" },
        { "", "" },
    };
    for (size_t i = 0; i < ARRAY_SIZE(cases); i++) {
        M_CheckSystemPath(cases[i][0], cases[i][1]);
    }
}

TEST(a_relative_path_is_made_absolute)
{
    char *const system_path = IoBackend_SystemPath("東方/./紅魔郷.dat");
    CHECK(Str_StartsWith(system_path, "\\\\?\\"));
    CHECK(Str_EndsWith(system_path, "\\東方\\紅魔郷.dat"));
    Mem_Free(system_path);
}

TEST(a_path_without_a_drive_takes_the_current_drive)
{
    char *const system_path = IoBackend_SystemPath("/th06");
    CHECK(Str_StartsWith(system_path, "\\\\?\\"));
    CHECK(Str_EndsWith(system_path, ":\\th06"));
    CHECK_EQ_INT(strlen(system_path), strlen("\\\\?\\C:\\th06"));
    Mem_Free(system_path);
}
#else
TEST(a_path_stays_as_it_is)
{
    M_CheckSystemPath(
        "th06/./replay/../紅魔郷.dat", "th06/./replay/../紅魔郷.dat");
    M_CheckSystemPath("", "");
}
#endif
