// SPDX-License-Identifier: GPL-3.0-only

#include "th06/files.h"

#include "core/base/arena.h"
#include "core/json.h"
#include "core/patch.h"

#include "patch_builder.h"
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

TEST(stream_opens_an_archive_file_and_a_loose_file)
{
    char *const directory = TestDir_Prepare("stream");
    Th06Data_WriteArchives(directory, 2);
    const TH06_DATA_FILE files[] = {
        { "ver0102.dat", "1.02h" },
        { "th06_01.pos", "from-archive" },
    };
    Th06Data_WriteArchive(directory, g_Th06Archives[2], files, 2);
    Th06Data_WriteFile(directory, "th06_01.wav", "from-loose", 10);

    CHECK(Result_Log(Files_Init(directory), LOG_LEVEL_ERROR));

    READER reader;
    CHECK(
        Result_Log(Files_Stream("bgm/th06_01.pos", &reader), LOG_LEVEL_ERROR));
    CHECK(M_ReadsAs(&reader, "from-archive"));
    Reader_Close(&reader);

    CHECK(Result_Log(Files_Stream("th06_01.wav", &reader), LOG_LEVEL_ERROR));
    CHECK(M_ReadsAs(&reader, "from-loose"));
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

// Writes the archives of directory with name holding "archive", and a
// patch folder in directory/patches with a/lang holding patch_files.
static PATCH_STACK *M_PreparePatched(
    const char *const directory, const char *const name,
    const TH06_DATA_FILE *const patch_files, const int32_t count)
{
    Th06Data_WriteArchives(directory, 2);
    const TH06_DATA_FILE files[] = {
        { "ver0102.dat", "1.02h" },
        { name, "archive" },
    };
    Th06Data_WriteArchive(directory, g_Th06Archives[2], files, 2);
    char *const patches = TestDir_Join(directory, "patches");
    PatchBuilder_AddPatch(patches, "a/lang", "{}");
    for (int32_t i = 0; i < count; i++) {
        char *const path = Str_Format("a/lang/th06/%s", patch_files[i].name);
        PatchBuilder_Write(patches, path, patch_files[i].content);
        Mem_Free(path);
    }
    PATCH_STACK *stack = nullptr;
    CHECK(Result_Log(
        Patch_OpenStack(patches, "th06", &g_PatchGame, nullptr, 0, &stack),
        LOG_LEVEL_ERROR));
    Mem_Free(patches);
    return stack;
}

TEST(a_patch_file_replaces_the_archived_file)
{
    char *const directory = TestDir_Prepare("patched");
    const TH06_DATA_FILE patch_files[] = {
        { "end00.end", "patched" },
        { "eff01.anm", "patched" },
        { "front.anm", "patched" },
        { "front.png", "patched" },
    };
    PATCH_STACK *const stack =
        M_PreparePatched(directory, "end00.end", patch_files, 4);
    CHECK(Result_Log(Files_Init(directory), LOG_LEVEL_ERROR));
    Files_SetPatches(stack);

    READER reader;
    CHECK(Result_Log(Files_Load("data/end00.end", &reader), LOG_LEVEL_ERROR));
    CHECK(M_ReadsAs(&reader, "patched"));
    Reader_Close(&reader);
    CHECK(Files_IsPatched("data/end00.end"));
    CHECK(Files_IsPatched("data/front.png"));
    CHECK(Files_IsPatched("data/front.anm"));
    CHECK(!Files_IsPatched("data/eff01.anm"));

    Files_Shutdown();
    Patch_CloseStack(stack);
    Mem_Free(directory);
}

TEST(a_gameplay_anm_of_a_patch_is_not_used)
{
    char *const directory = TestDir_Prepare("ignored");
    const TH06_DATA_FILE patch_files[] = { { "eff01.anm", "patched" } };
    PATCH_STACK *const stack =
        M_PreparePatched(directory, "eff01.anm", patch_files, 1);
    CHECK(Result_Log(Files_Init(directory), LOG_LEVEL_ERROR));
    Files_SetPatches(stack);

    READER reader;
    CHECK(Result_Log(Files_Load("data/eff01.anm", &reader), LOG_LEVEL_ERROR));
    CHECK(M_ReadsAs(&reader, "archive"));
    Reader_Close(&reader);

    Files_Shutdown();
    Patch_CloseStack(stack);
    Mem_Free(directory);
}

TEST(a_diff_comes_from_the_patches)
{
    char *const directory = TestDir_Prepare("diff");
    const TH06_DATA_FILE patch_files[] = {
        { "msg1.dat.jdiff", "{'0': {}}" },
    };
    PATCH_STACK *const stack =
        M_PreparePatched(directory, "msg1.dat", patch_files, 1);
    CHECK(Result_Log(Files_Init(directory), LOG_LEVEL_ERROR));
    Files_SetPatches(stack);

    ARENA arena;
    Arena_Init(&arena, "test", 1 << 16);
    const JSON_VALUE *const diff =
        Files_LoadDiff("data/msg1.dat", Arena_Allocator(&arena));
    CHECK(diff != nullptr && Json_Get(diff, "0") != nullptr);
    CHECK(Files_LoadDiff("data/msg2.dat", Arena_Allocator(&arena)) == nullptr);
    CHECK(Files_IsPatched("data/msg1.dat"));
    Arena_Free(&arena);

    Files_Shutdown();
    Patch_CloseStack(stack);
    Mem_Free(directory);
}

TEST(without_patches_nothing_is_patched)
{
    char *const directory = TestDir_Prepare("unpatched");
    Th06Data_WriteArchives(directory, -1);
    CHECK(Result_Log(Files_Init(directory), LOG_LEVEL_ERROR));
    Files_SetPatches(nullptr);
    CHECK(!Files_IsPatched("data/msg1.dat"));
    CHECK(Files_LoadDiff("data/msg1.dat", &g_HeapAllocator) == nullptr);
    Files_Shutdown();
    Mem_Free(directory);
}

TEST(gameplay_files_are_classified)
{
    CHECK_EQ_INT(g_PatchGame.classify("ecldata1.ecl"), PATCH_FILE_GAMEPLAY);
    CHECK_EQ_INT(g_PatchGame.classify("stage1.std"), PATCH_FILE_GAMEPLAY);
    CHECK_EQ_INT(g_PatchGame.classify("ply00a.sht"), PATCH_FILE_GAMEPLAY);
    CHECK_EQ_INT(g_PatchGame.classify("msg1.dat"), PATCH_FILE_GAMEPLAY);
    CHECK_EQ_INT(g_PatchGame.classify("front.anm"), PATCH_FILE_DATA);
    CHECK_EQ_INT(g_PatchGame.classify("text.anm"), PATCH_FILE_DATA);
    CHECK_EQ_INT(g_PatchGame.classify("replay00.anm"), PATCH_FILE_DATA);
    CHECK_EQ_INT(g_PatchGame.classify("title01s.anm"), PATCH_FILE_DATA);
    CHECK_EQ_INT(g_PatchGame.classify("select05.anm"), PATCH_FILE_DATA);
    CHECK_EQ_INT(g_PatchGame.classify("slpl00b.anm"), PATCH_FILE_DATA);
    CHECK_EQ_INT(g_PatchGame.classify("result03.anm"), PATCH_FILE_DATA);
    CHECK_EQ_INT(g_PatchGame.classify("music02.anm"), PATCH_FILE_DATA);
    CHECK_EQ_INT(g_PatchGame.classify("staff01.anm"), PATCH_FILE_DATA);
    CHECK_EQ_INT(g_PatchGame.classify("eff01.anm"), PATCH_FILE_IGNORED);
    CHECK_EQ_INT(g_PatchGame.classify("player00.anm"), PATCH_FILE_IGNORED);
    CHECK_EQ_INT(g_PatchGame.classify("replay01.anm"), PATCH_FILE_IGNORED);
    CHECK_EQ_INT(g_PatchGame.classify("front.png"), PATCH_FILE_DATA);
    CHECK_EQ_INT(g_PatchGame.classify("th06logo.jpg"), PATCH_FILE_DATA);
    CHECK_EQ_INT(g_PatchGame.classify("end00.end"), PATCH_FILE_DATA);
    CHECK_EQ_INT(g_PatchGame.classify("musiccmt.txt"), PATCH_FILE_DATA);
    CHECK_EQ_INT(g_PatchGame.classify("ver0102.dat"), PATCH_FILE_DATA);
}
