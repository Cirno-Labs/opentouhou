#include "shell/fatal.h"

#include "test.h"

TEST(uses_box_by_default)
{
    CHECK(Fatal_UsesBox(nullptr, nullptr));
    CHECK(Fatal_UsesBox("windows", nullptr));
    CHECK(Fatal_UsesBox("x11", ""));
}

TEST(uses_no_box_without_video)
{
    CHECK(!Fatal_UsesBox("offscreen", nullptr));
    CHECK(!Fatal_UsesBox("dummy", nullptr));
    CHECK(!Fatal_UsesBox("Offscreen", nullptr));
}

TEST(uses_no_box_on_ci)
{
    CHECK(!Fatal_UsesBox(nullptr, "true"));
    CHECK(!Fatal_UsesBox("windows", "1"));
}

TEST(missing_data_names_file_folder_and_fix)
{
    char text[1024];
    Fatal_FormatMissingData(
        text, sizeof(text), "紅魔郷CM.DAT", "/games/th06", nullptr);
    CHECK_EQ_STR(
        text,
        "Cannot find the game data file 紅魔郷CM.DAT.\n"
        "\n"
        "Searched folder: /games/th06\n"
        "\n"
        "Put the program into the folder of the original game, or start "
        "it with --data <folder>.");
}

TEST(missing_data_names_the_log_file)
{
    char text[1024];
    Fatal_FormatMissingData(
        text, sizeof(text), "紅魔郷CM.DAT", "/games/th06", "/u/oth06.log");
    CHECK_EQ_STR(
        text,
        "Cannot find the game data file 紅魔郷CM.DAT.\n"
        "\n"
        "Searched folder: /games/th06\n"
        "\n"
        "Put the program into the folder of the original game, or start "
        "it with --data <folder>.\n"
        "\n"
        "Log file: /u/oth06.log");
}

TEST(startup_error_names_message_and_log_file)
{
    char text[1024];
    Fatal_FormatStartupError(
        text, sizeof(text), "Cannot create the window", nullptr);
    CHECK_EQ_STR(text, "Cannot create the window");
    Fatal_FormatStartupError(
        text, sizeof(text), "Cannot create the window", "/u/oth06.log");
    CHECK_EQ_STR(
        text,
        "Cannot create the window\n"
        "\n"
        "Log file: /u/oth06.log");
}

TEST(internal_error_names_message_and_position)
{
    char text[1024];
    Fatal_FormatInternalError(
        text, sizeof(text), "th06", "Assertion failed: x", "anm.c", 12,
        nullptr);
    CHECK_EQ_STR(
        text,
        "th06 stopped because of an internal error:\n"
        "\n"
        "Assertion failed: x (anm.c:12)");
}

TEST(internal_error_names_the_log_file)
{
    char text[1024];
    Fatal_FormatInternalError(
        text, sizeof(text), "th06", "Assertion failed: x", "anm.c", 12,
        "C:\\Users\\a\\oth06.log");
    CHECK_EQ_STR(
        text,
        "th06 stopped because of an internal error:\n"
        "\n"
        "Assertion failed: x (anm.c:12)\n"
        "\n"
        "Log file: C:\\Users\\a\\oth06.log");
}

TEST(text_is_truncated_to_size)
{
    char text[8];
    Fatal_FormatMissingData(text, sizeof(text), "a", "b", nullptr);
    CHECK_EQ_STR(text, "Cannot ");
    Fatal_FormatStartupError(text, sizeof(text), "Failed", "/log");
    CHECK_EQ_STR(text, "Failed\n");
    Fatal_FormatStartupError(text, sizeof(text), "Failed!", "/log");
    CHECK_EQ_STR(text, "Failed!");
}
