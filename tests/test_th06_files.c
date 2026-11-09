#include "th06/files.h"

#include "pbg3_builder.h"
#include "test.h"
#include "test_dir.h"

#include <SDL3/SDL.h>
#include <string.h>

// Fills entry and buffer with a single-entry literal stream holding
// content.
static void M_MakeEntry(
    PBG3_BUILDER_ENTRY *const entry, uint8_t *const buffer,
    const char *const name, const char *const content)
{
    PBG3_BUILDER_WRITER bw;
    Pbg3Builder_Init(&bw);
    for (const unsigned char *c = (const unsigned char *)content; *c != '\0';
         c++) {
        Pbg3Builder_WriteLiteral(&bw, *c);
    }
    Pbg3Builder_FlushByte(&bw);
    memcpy(buffer, bw.bytes, bw.length);
    *entry = (PBG3_BUILDER_ENTRY) {
        .name = name,
        .compressed = buffer,
        .compressed_size = bw.length,
        .checksum = Pbg3Builder_Checksum(buffer, bw.length),
        .decoded_size = (uint32_t)strlen(content),
    };
}

static void M_WriteArchive(
    const char *const directory, const char *const name,
    const PBG3_BUILDER_ENTRY *const entries, const int32_t count)
{
    uint8_t archive[PBG3_BUILDER_MAX_ARCHIVE_SIZE];
    const size_t size = Pbg3Builder_BuildArchive(entries, count, archive);
    char *const path = TestDir_Join(directory, name);
    if (!SDL_SaveFile(path, archive, size)) {
        TEST_FAIL("cannot create %s: %s", path, SDL_GetError());
    }
    Mem_Free(path);
}

// Writes a standard archive, with ver0102.dat and nothing else, at
// directory/name.
static void M_WriteVersionOnlyArchive(
    const char *const directory, const char *const name)
{
    uint8_t buffer[16];
    PBG3_BUILDER_ENTRY entry;
    M_MakeEntry(&entry, buffer, "ver0102.dat", "1.02h");
    M_WriteArchive(directory, name, &entry, 1);
}

static const char *const M_ARCHIVES[] = {
    "紅魔郷IN.DAT", "紅魔郷MD.DAT", "紅魔郷ST.DAT",
    "紅魔郷TL.DAT", "紅魔郷CM.DAT", "紅魔郷ED.DAT",
};

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
    for (size_t i = 0; i < ARRAY_SIZE(M_ARCHIVES); i++) {
        if (i == 4) {
            continue; // written below, with an extra entry
        }
        M_WriteVersionOnlyArchive(directory, M_ARCHIVES[i]);
    }
    uint8_t buffers[2][16];
    PBG3_BUILDER_ENTRY entries[2];
    M_MakeEntry(&entries[0], buffers[0], "ver0102.dat", "1.02h");
    M_MakeEntry(&entries[1], buffers[1], "card.txt", "from-archive");
    M_WriteArchive(directory, M_ARCHIVES[4], entries, 2);

    char *const loose_path = TestDir_Join(directory, "extra.txt");
    if (!SDL_SaveFile(loose_path, "from-loose", strlen("from-loose"))) {
        TEST_FAIL("cannot create %s: %s", loose_path, SDL_GetError());
    }

    CHECK(Result_Log(Files_Init(directory), LOG_LEVEL_ERROR));

    READER reader;
    CHECK(Result_Log(Files_Load("card.txt", &reader), LOG_LEVEL_ERROR));
    CHECK(M_ReadsAs(&reader, "from-archive"));
    Reader_Close(&reader);

    CHECK(Result_Log(Files_Load("extra.txt", &reader), LOG_LEVEL_ERROR));
    CHECK(M_ReadsAs(&reader, "from-loose"));
    Reader_Close(&reader);

    Files_Shutdown();
    Mem_Free(loose_path);
    Mem_Free(directory);
}

TEST(an_archive_file_wins_over_a_loose_file_of_the_same_name)
{
    char *const directory = TestDir_Prepare("archive_wins");
    for (size_t i = 1; i < ARRAY_SIZE(M_ARCHIVES); i++) {
        M_WriteVersionOnlyArchive(directory, M_ARCHIVES[i]);
    }
    uint8_t buffers[2][16];
    PBG3_BUILDER_ENTRY entries[2];
    M_MakeEntry(&entries[0], buffers[0], "ver0102.dat", "1.02h");
    M_MakeEntry(&entries[1], buffers[1], "same.txt", "archive-version");
    M_WriteArchive(directory, M_ARCHIVES[0], entries, 2);

    char *const loose_path = TestDir_Join(directory, "same.txt");
    if (!SDL_SaveFile(loose_path, "loose-version", strlen("loose-version"))) {
        TEST_FAIL("cannot create %s: %s", loose_path, SDL_GetError());
    }

    CHECK(Result_Log(Files_Init(directory), LOG_LEVEL_ERROR));

    READER reader;
    CHECK(Result_Log(Files_Load("same.txt", &reader), LOG_LEVEL_ERROR));
    CHECK(M_ReadsAs(&reader, "archive-version"));
    Reader_Close(&reader);

    Files_Shutdown();
    Mem_Free(loose_path);
    Mem_Free(directory);
}

TEST(a_missing_archive_fails_naming_it)
{
    char *const directory = TestDir_Prepare("missing_archive");
    for (size_t i = 0; i < ARRAY_SIZE(M_ARCHIVES); i++) {
        if (i == 1) {
            continue; // leave 紅魔郷MD.DAT missing
        }
        M_WriteVersionOnlyArchive(directory, M_ARCHIVES[i]);
    }

    const RESULT result = Files_Init(directory);
    CHECK(!IS_OK(result));
    CHECK(strstr(result.message, M_ARCHIVES[1]) != nullptr);
    Result_Ignore(result);
    Files_Shutdown();
    Mem_Free(directory);
}

TEST(an_archive_without_ver0102_fails_naming_it)
{
    char *const directory = TestDir_Prepare("no_version");
    for (size_t i = 0; i < ARRAY_SIZE(M_ARCHIVES); i++) {
        if (i == 3) {
            continue;
        }
        M_WriteVersionOnlyArchive(directory, M_ARCHIVES[i]);
    }
    uint8_t buffer[16];
    PBG3_BUILDER_ENTRY entry;
    M_MakeEntry(&entry, buffer, "other.txt", "no version here");
    M_WriteArchive(directory, M_ARCHIVES[3], &entry, 1);

    const RESULT result = Files_Init(directory);
    CHECK(!IS_OK(result));
    CHECK(strstr(result.message, M_ARCHIVES[3]) != nullptr);
    Result_Ignore(result);
    Files_Shutdown();
    Mem_Free(directory);
}

TEST(shutdown_without_init_is_safe)
{
    Files_Shutdown();
}
