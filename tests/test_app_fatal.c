#include "core/app/fatal.h"

#include "test.h"

TEST(uses_box_by_default)
{
    CHECK(AppFatal_UsesBox(nullptr, nullptr));
    CHECK(AppFatal_UsesBox("windows", nullptr));
    CHECK(AppFatal_UsesBox("x11", ""));
}

TEST(uses_no_box_without_video)
{
    CHECK(!AppFatal_UsesBox("offscreen", nullptr));
    CHECK(!AppFatal_UsesBox("dummy", nullptr));
    CHECK(!AppFatal_UsesBox("Offscreen", nullptr));
}

TEST(uses_no_box_on_ci)
{
    CHECK(!AppFatal_UsesBox(nullptr, "true"));
    CHECK(!AppFatal_UsesBox("windows", "1"));
}

TEST(missing_data_names_file_folder_and_fix)
{
    char text[1024];
    AppFatal_FormatMissingData(
        text, sizeof(text), "紅魔郷CM.DAT", "/games/th06");
    CHECK_EQ_STR(
        text,
        "Cannot find the game data file 紅魔郷CM.DAT.\n"
        "\n"
        "Searched folder: /games/th06\n"
        "\n"
        "Put the program into the folder of the original game, or start "
        "it with --data <folder>.");
}

TEST(internal_error_names_message_and_position)
{
    char text[1024];
    AppFatal_FormatInternalError(
        text, sizeof(text), "th06", "Assertion failed: x", "anm.c", 12);
    CHECK_EQ_STR(
        text,
        "th06 stopped because of an internal error:\n"
        "\n"
        "Assertion failed: x (anm.c:12)");
}

TEST(text_is_truncated_to_size)
{
    char text[8];
    AppFatal_FormatMissingData(text, sizeof(text), "a", "b");
    CHECK_EQ_STR(text, "Cannot ");
}
