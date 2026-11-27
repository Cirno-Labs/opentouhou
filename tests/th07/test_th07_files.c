#include "tests/patch_builder.h"
#include "tests/test.h"
#include "tests/test_dir.h"
#include "tests/th07/th07_data.h"
#include "th07/files.h"

#include "core/base/arena.h"
#include "core/json.h"
#include "core/patch.h"
#include "core/vfs.h"
#include "shell/game.h"

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

TEST(the_data_files_are_th07_dat_and_thbgm_dat)
{
    CHECK_EQ_INT(FILES_DATA_COUNT, 2);
    CHECK_EQ_STR(g_FilesData[0], "th07.dat");
    CHECK_EQ_STR(g_FilesData[1], "thbgm.dat");
}

TEST(loads_an_archive_file_by_its_name_and_a_loose_file)
{
    char *const directory = TestDir_Prepare("loads");
    const TH07_DATA_FILE files[] = {
        m_Version,
        { "text.anm", "from-archive" },
    };
    Th07Data_WriteArchive(directory, files, 2);
    Th07Data_WriteMusic(directory, nullptr, 0);
    Th07Data_WriteFile(directory, "loose.dat", "from-loose", 10);

    CHECK(Result_Log(Th07Data_Mount(directory), LOG_LEVEL_ERROR));

    READER reader;
    CHECK(Result_Log(
        Vfs_Open(Shell_GetFiles(), "data/text.anm", VFS_IN_MEMORY, &reader),
        LOG_LEVEL_ERROR));
    CHECK(M_ReadsAs(&reader, "from-archive"));
    Reader_Close(&reader);

    CHECK(Result_Log(
        Vfs_Open(Shell_GetFiles(), "loose.dat", VFS_STREAM, &reader),
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
    Th07Data_WriteMusic(directory, nullptr, 0);
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

TEST(a_missing_thbgm_dat_fails_naming_it)
{
    char *const directory = TestDir_Prepare("no_music");
    Th07Data_WriteArchive(directory, &m_Version, 1);
    const RESULT result = Th07Data_Mount(directory);
    CHECK(!IS_OK(result));
    CHECK(strstr(result.message, "thbgm.dat") != nullptr);
    Result_Ignore(result);
    Th07Data_Unmount();
    Mem_Free(directory);
}

TEST(a_thbgm_dat_of_another_version_fails)
{
    char *const directory = TestDir_Prepare("other_music");
    Th07Data_WriteArchive(directory, &m_Version, 1);
    const uint8_t headers[][16] = {
        { 'Z', 'W', 'A', 'V', 2, 0, 0, 0, 0, 7, 0, 0 },
        { 'Z', 'W', 'A', 'V', 1, 0, 0, 0, 0, 8, 0, 0 },
        { 'Z', 'W', 'A', 'X', 1, 0, 0, 0, 0, 7, 0, 0 },
    };
    for (size_t i = 0; i < ARRAY_SIZE(headers); i++) {
        Th07Data_WriteFile(directory, "thbgm.dat", headers[i], 16);
        const RESULT result = Th07Data_Mount(directory);
        CHECK(!IS_OK(result));
        CHECK(strstr(result.message, "thbgm.dat") != nullptr);
        Result_Ignore(result);
        Th07Data_Unmount();
    }
    Th07Data_WriteFile(directory, "thbgm.dat", "ZWAV", 4);
    CHECK(!Result_Ignore(Th07Data_Mount(directory)));
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
    Th07Data_WriteMusic(directory, nullptr, 0);
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

TEST(a_patch_file_replaces_a_nested_path_by_its_archive_name)
{
    // thcrap's lang_en ships data/title/th07logo.jpg flat, as "th07logo.jpg",
    // since that is the name th07.dat itself stores it by.
    char *const directory = TestDir_Prepare("archive_name_fallback");
    const TH07_DATA_FILE files[] = {
        m_Version,
        { "th07logo.jpg", "archive" },
    };
    Th07Data_WriteArchive(directory, files, 2);
    Th07Data_WriteMusic(directory, nullptr, 0);
    char *const patches = TestDir_Join(directory, "patches");
    PatchBuilder_AddPatch(patches, "a/lang", "{}");
    PatchBuilder_Write(patches, "a/lang/th07/th07logo.jpg", "patched");
    PATCH_STACK *stack = nullptr;
    CHECK(Result_Log(
        Patch_OpenStack(patches, "th07", &g_PatchGame, nullptr, &stack),
        LOG_LEVEL_ERROR));
    Mem_Free(patches);
    CHECK(Result_Log(Th07Data_Mount(directory), LOG_LEVEL_ERROR));
    Files_SetPatches(stack);

    READER reader;
    CHECK(Result_Log(
        Vfs_Open(
            Shell_GetFiles(), "data/title/th07logo.jpg", VFS_IN_MEMORY,
            &reader),
        LOG_LEVEL_ERROR));
    CHECK(M_ReadsAs(&reader, "patched"));
    Reader_Close(&reader);
    CHECK(Files_IsPatched("data/title/th07logo.jpg"));

    Th07Data_Unmount();
    Patch_CloseStack(stack);
    Mem_Free(directory);
}

TEST(a_dialogue_jdiff_is_found_by_its_archive_name)
{
    char *const directory = TestDir_Prepare("jdiff_archive_name");
    Th07Data_WriteArchive(directory, &m_Version, 1);
    Th07Data_WriteMusic(directory, nullptr, 0);
    char *const patches = TestDir_Join(directory, "patches");
    PatchBuilder_AddPatch(patches, "a/lang", "{}");
    PatchBuilder_Write(patches, "a/lang/th07/msg1.dat.jdiff", "{'0': {}}");
    PATCH_STACK *stack = nullptr;
    CHECK(Result_Log(
        Patch_OpenStack(patches, "th07", &g_PatchGame, nullptr, &stack),
        LOG_LEVEL_ERROR));
    Mem_Free(patches);
    CHECK(Result_Log(Th07Data_Mount(directory), LOG_LEVEL_ERROR));
    Files_SetPatches(stack);

    ARENA arena;
    Arena_Init(&arena, "test", 1 << 16);
    const JSON_VALUE *const diff =
        Files_LoadDiff("data/msg1.dat", Arena_Allocator(&arena));
    CHECK(diff != nullptr && Json_Get(diff, "0") != nullptr);
    CHECK(Files_IsPatched("data/msg1.dat"));
    Arena_Free(&arena);

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
