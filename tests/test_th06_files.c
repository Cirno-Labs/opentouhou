#include "th06/files.h"

#include "test.h"
#include "test_dir.h"
#include "th06_data.h"

#include <string.h>

static bool M_ReadsAs(READER *const reader, const char *const expected)
{
    const size_t size = Reader_Size(reader);
    char *const buffer = Mem_Alloc(size + 1);
    Reader_ReadBytes(reader, buffer, size);
    buffer[size] = '\0';
    const bool matches = strcmp(buffer, expected) == 0;
    Mem_Free(buffer);
    return matches && Result_Ignore(Reader_Result(reader));
}

TEST(all_six_present_loads_an_archive_file_and_a_loose_file)
{
    char *const directory = TestDir_Prepare("all_six");
    Th06Data_WriteArchives(directory, 4);
    const TH06_DATA_FILE files[] = {
        { "ver0102.dat", "1.02h" },
        { "card.txt", "from-archive" },
    };
    Th06Data_WriteArchive(directory, g_Th06Archives[4], files, 2);
    Th06Data_WriteFile(directory, "extra.txt", "from-loose", 10);

    CHECK(Result_Log(Files_Init(directory), LOG_LEVEL_ERROR));

    READER reader;
    CHECK(Result_Log(Files_Load("card.txt", &reader), LOG_LEVEL_ERROR));
    CHECK(M_ReadsAs(&reader, "from-archive"));
    Reader_Close(&reader);

    CHECK(Result_Log(Files_Load("extra.txt", &reader), LOG_LEVEL_ERROR));
    CHECK(M_ReadsAs(&reader, "from-loose"));
    Reader_Close(&reader);

    Files_Shutdown();
    Mem_Free(directory);
}

TEST(an_archive_file_wins_over_a_loose_file_of_the_same_name)
{
    char *const directory = TestDir_Prepare("archive_wins");
    Th06Data_WriteArchives(directory, 0);
    const TH06_DATA_FILE files[] = {
        { "ver0102.dat", "1.02h" },
        { "same.txt", "archive-version" },
    };
    Th06Data_WriteArchive(directory, g_Th06Archives[0], files, 2);
    Th06Data_WriteFile(directory, "same.txt", "loose-version", 13);

    CHECK(Result_Log(Files_Init(directory), LOG_LEVEL_ERROR));

    READER reader;
    CHECK(Result_Log(Files_Load("same.txt", &reader), LOG_LEVEL_ERROR));
    CHECK(M_ReadsAs(&reader, "archive-version"));
    Reader_Close(&reader);

    Files_Shutdown();
    Mem_Free(directory);
}

TEST(a_missing_archive_fails_naming_it)
{
    char *const directory = TestDir_Prepare("missing_archive");
    Th06Data_WriteArchives(directory, 1);

    const RESULT result = Files_Init(directory);
    CHECK(!IS_OK(result));
    CHECK(strstr(result.message, g_Th06Archives[1]) != nullptr);
    Result_Ignore(result);
    Files_Shutdown();
    Mem_Free(directory);
}

TEST(an_archive_without_ver0102_fails_naming_it)
{
    char *const directory = TestDir_Prepare("no_version");
    Th06Data_WriteArchives(directory, 3);
    const TH06_DATA_FILE file = { "other.txt", "no version here" };
    Th06Data_WriteArchive(directory, g_Th06Archives[3], &file, 1);

    const RESULT result = Files_Init(directory);
    CHECK(!IS_OK(result));
    CHECK(strstr(result.message, g_Th06Archives[3]) != nullptr);
    Result_Ignore(result);
    Files_Shutdown();
    Mem_Free(directory);
}

TEST(shutdown_without_init_is_safe)
{
    Files_Shutdown();
}
