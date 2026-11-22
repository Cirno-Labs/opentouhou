#include "shell/data.h"

#include "core/archive/zip.h"
#include "test.h"
#include "zip_builder.h"

static void M_CheckRoot(
    const ZIP_BUILDER_ENTRY *const entries, const int count,
    const char *const expected)
{
    ZIP_BUILDER_ARCHIVE data = ZipBuilder_Build(entries, count);
    READER reader;
    Reader_OpenBuffer(&reader, data.data, data.size, nullptr);
    ARCHIVE *archive;
    CHECK(Result_Log(
        Archive_Open(&g_ZipFormat, &reader, &g_HeapAllocator, &archive),
        LOG_LEVEL_ERROR));
    char *const root = ShellData_FindRoot(archive);
    CHECK_EQ_STR(root, expected);
    Mem_Free(root);
    Archive_Close(archive);
    Mem_Free(data.data);
}

TEST(the_root_is_the_one_top_folder_of_every_file)
{
    const ZIP_BUILDER_ENTRY entries[] = {
        { "東方紅魔郷/", "", 0, 0, false },
        { "東方紅魔郷/th06.exe", "a", 1, 0, false },
        { "東方紅魔郷/data/th06e.dat", "b", 1, 0, false },
    };
    M_CheckRoot(entries, 3, "東方紅魔郷/");
}

TEST(the_root_is_empty_with_a_file_at_the_top)
{
    const ZIP_BUILDER_ENTRY entries[] = {
        { "game/th06.exe", "a", 1, 0, false },
        { "th06e.dat", "b", 1, 0, false },
    };
    M_CheckRoot(entries, 2, "");
}

TEST(the_root_is_empty_with_two_top_folders)
{
    const ZIP_BUILDER_ENTRY entries[] = {
        { "game/th06.exe", "a", 1, 0, false },
        { "gamex/th06e.dat", "b", 1, 0, false },
        { "other/", "", 0, 0, false },
    };
    M_CheckRoot(entries, 3, "");
}
