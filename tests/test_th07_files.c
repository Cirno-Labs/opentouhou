#include "th07/files.h"

#include "shell/game.h"
#include "core/patch.h"
#include "core/vfs.h"

#include "patch_builder.h"
#include "test.h"
#include "test_dir.h"
#include "th07_data.h"

#include <string.h>

static const TH07_DATA_FILE m_Version = { "th07_0100b.ver", "0100b" };

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

TEST(the_data_files_are_th07_dat)
{
    CHECK_EQ_INT(FILES_ARCHIVE_COUNT, 1);
    CHECK_EQ_STR(g_FilesArchives[0], "th07.dat");
}

TEST(loads_an_archive_file_by_its_name_and_a_loose_file)
{
    char *const directory = TestDir_Prepare("loads");
    const TH07_DATA_FILE files[] = {
        m_Version,
        { "text.anm", "from-archive" },
    };
    Th07Data_WriteArchive(directory, files, 2);
    Th07Data_WriteFile(directory, "thbgm.dat", "from-loose", 10);

    CHECK(Result_Log(Th07Data_Mount(directory), LOG_LEVEL_ERROR));

    READER reader;
    CHECK(Result_Log(
        Vfs_Open(Shell_GetFiles(), "data/text.anm", VFS_IN_MEMORY, &reader),
        LOG_LEVEL_ERROR));
    CHECK(M_ReadsAs(&reader, "from-archive"));
    Reader_Close(&reader);

    CHECK(Result_Log(
        Vfs_Open(Shell_GetFiles(), "thbgm.dat", VFS_STREAM, &reader),
        LOG_LEVEL_ERROR));
    CHECK(M_ReadsAs(&reader, "from-loose"));
    Reader_Close(&reader);

    Th07Data_Unmount();
    Mem_Free(directory);
}

TEST(an_archive_file_wins_over_a_loose_file_of_the_same_name)
{
    char *const directory = TestDir_Prepare("archive_wins");
    const TH07_DATA_FILE files[] = {
        m_Version,
        { "same.txt", "archive-version" },
    };
    Th07Data_WriteArchive(directory, files, 2);
    Th07Data_WriteFile(directory, "same.txt", "loose-version", 13);

    CHECK(Result_Log(Th07Data_Mount(directory), LOG_LEVEL_ERROR));
    READER reader;
    CHECK(Result_Log(
        Vfs_Open(Shell_GetFiles(), "same.txt", VFS_IN_MEMORY, &reader),
        LOG_LEVEL_ERROR));
    CHECK(M_ReadsAs(&reader, "archive-version"));
    Reader_Close(&reader);

    Th07Data_Unmount();
    Mem_Free(directory);
}

TEST(a_missing_archive_fails_naming_it)
{
    char *const directory = TestDir_Prepare("missing_archive");
    const RESULT result = Th07Data_Mount(directory);
    CHECK(!IS_OK(result));
    CHECK(strstr(result.message, "th07.dat") != nullptr);
    Result_Ignore(result);
    Th07Data_Unmount();
    Mem_Free(directory);
}

TEST(an_archive_without_the_version_file_fails_naming_it)
{
    char *const directory = TestDir_Prepare("no_version");
    const TH07_DATA_FILE file = { "th07_0100a.ver", "0100a" };
    Th07Data_WriteArchive(directory, &file, 1);

    const RESULT result = Th07Data_Mount(directory);
    CHECK(!IS_OK(result));
    CHECK(strstr(result.message, "th07_0100b.ver") != nullptr);
    Result_Ignore(result);
    Th07Data_Unmount();
    Mem_Free(directory);
}

TEST(a_patch_file_replaces_the_archived_file_by_its_path)
{
    char *const directory = TestDir_Prepare("patched");
    const TH07_DATA_FILE files[] = {
        m_Version,
        { "end00.end", "archive" },
        { "eff01.anm", "archive" },
    };
    Th07Data_WriteArchive(directory, files, 3);
    char *const patches = TestDir_Join(directory, "patches");
    PatchBuilder_AddPatch(patches, "a/lang", "{}");
    PatchBuilder_Write(patches, "a/lang/th07/data/end00.end", "patched");
    PatchBuilder_Write(patches, "a/lang/th07/data/eff01.anm", "patched");
    PatchBuilder_Write(patches, "a/lang/th07/data/front.anm", "patched");
    PATCH_STACK *stack = nullptr;
    CHECK(Result_Log(
        Patch_OpenStack(patches, "th07", &g_PatchGame, nullptr, &stack),
        LOG_LEVEL_ERROR));
    Mem_Free(patches);
    CHECK(!Patch_ChangesGameplay(stack));
    CHECK(Result_Log(Th07Data_Mount(directory), LOG_LEVEL_ERROR));
    Files_SetPatches(stack);

    READER reader;
    CHECK(Result_Log(
        Vfs_Open(Shell_GetFiles(), "data/end00.end", VFS_IN_MEMORY, &reader),
        LOG_LEVEL_ERROR));
    CHECK(M_ReadsAs(&reader, "patched"));
    Reader_Close(&reader);
    CHECK(Result_Log(
        Vfs_Open(Shell_GetFiles(), "data/eff01.anm", VFS_IN_MEMORY, &reader),
        LOG_LEVEL_ERROR));
    CHECK(M_ReadsAs(&reader, "archive"));
    Reader_Close(&reader);
    CHECK(Files_IsPatched("data/end00.end"));
    CHECK(Files_IsPatched("data/front.anm"));
    CHECK(!Files_IsPatched("data/eff01.anm"));
    CHECK(!Files_IsPatched("end00.end"));

    Th07Data_Unmount();
    Patch_CloseStack(stack);
    Mem_Free(directory);
}

TEST(files_are_classified_by_their_name)
{
    static const char *const gameplay[] = {
        "data/ecldata1.ecl", "data/stage7.std", "data/ply00as.sht",
        "data/msg8.dat",     "ecldata1.ecl",
    };
    for (size_t i = 0; i < ARRAY_SIZE(gameplay); i++) {
        CHECK_EQ_INT(g_PatchGame.classify(gameplay[i]), PATCH_FILE_GAMEPLAY);
    }
    static const char *const data[] = {
        "data/front.anm",      "data/text.anm",     "data/title01.anm",
        "data/result00.anm",   "data/music00.anm",  "data/staff01.anm",
        "data/end00.end",      "data/musiccmt.txt", "data/title/th07logo.jpg",
        "data/th07_0100b.ver",
    };
    for (size_t i = 0; i < ARRAY_SIZE(data); i++) {
        CHECK_EQ_INT(g_PatchGame.classify(data[i]), PATCH_FILE_DATA);
    }
    static const char *const ignored[] = {
        "data/eff01.anm",
        "data/etama.anm",
        "data/stg1enm.anm",
        "data/face_rm00.anm",
    };
    for (size_t i = 0; i < ARRAY_SIZE(ignored); i++) {
        CHECK_EQ_INT(g_PatchGame.classify(ignored[i]), PATCH_FILE_IGNORED);
    }
}

TEST(the_patch_game_is_v1_00b_with_paths)
{
    CHECK_EQ_STR(g_PatchGame.build, "v1.00b");
    CHECK_EQ_INT(g_PatchGame.layout, PATCH_LAYOUT_PATHS);
}
