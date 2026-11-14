#include "core/archive/zip.h"

#include "core/archive.h"
#include "test.h"
#include "zip_builder.h"

#define M_TEXT "Every package holds the game and othpatch. "

typedef struct {
    ZIP_BUILDER_ARCHIVE data;
    ARCHIVE *archive;
} M_CASE;

static void M_Open(
    M_CASE *const test, const ZIP_BUILDER_ENTRY *const entries, const int count)
{
    *test = (M_CASE) {};
    test->data = ZipBuilder_Build(entries, count);
    READER reader;
    Reader_OpenBuffer(&reader, test->data.data, test->data.size, nullptr);
    CHECK(Result_Log(
        Archive_Open(&g_ZipFormat, &reader, &g_HeapAllocator, &test->archive),
        LOG_LEVEL_ERROR));
}

static void M_Close(M_CASE *const test)
{
    if (test->archive != nullptr) {
        Archive_Close(test->archive);
    }
    Mem_Free(test->data.data);
}

static RESULT M_OpenBytes(const uint8_t *const data, const size_t size)
{
    READER reader;
    Reader_OpenBuffer(&reader, data, size, nullptr);
    ARCHIVE *archive;
    MUST(Archive_Open(&g_ZipFormat, &reader, &g_HeapAllocator, &archive));
    Archive_Close(archive);
    return OK;
}

static void M_CheckRead(
    const M_CASE *const test, const char *const name,
    const char *const expected, const size_t expected_size)
{
    const ARCHIVE_ENTRY *const entry = Archive_Find(test->archive, name);
    CHECK(entry != nullptr);
    if (entry == nullptr) {
        return;
    }
    CHECK_EQ_INT(ArchiveEntry_GetSize(entry), expected_size);
    uint8_t *data;
    CHECK(
        Result_Log(Archive_Read(test->archive, entry, &data), LOG_LEVEL_ERROR));
    CHECK(data != nullptr && memcmp(data, expected, expected_size) == 0);
    Allocator_Free(&g_HeapAllocator, data, expected_size);
}

TEST(list_gives_the_names_and_modes_in_order)
{
    const ZIP_BUILDER_ENTRY entries[] = {
        { "oth06.exe", "a", 1, 0, false },
        { "othpatch", "b", 1, 0100755, true },
        { "oth06.app/", "", 0, 040755, false },
    };
    M_CASE test;
    M_Open(&test, entries, 3);
    CHECK_EQ_INT(Archive_GetCount(test.archive), 3);
    const ARCHIVE_ENTRY *const first = Archive_GetEntry(test.archive, 0);
    const ARCHIVE_ENTRY *const second = Archive_GetEntry(test.archive, 1);
    const ARCHIVE_ENTRY *const third = Archive_GetEntry(test.archive, 2);
    CHECK_EQ_STR(ArchiveEntry_GetName(first), "oth06.exe");
    CHECK_EQ_INT(ArchiveEntry_GetMode(first), 0);
    CHECK_EQ_STR(ArchiveEntry_GetName(second), "othpatch");
    CHECK_EQ_INT(ArchiveEntry_GetMode(second), 0100755);
    CHECK_EQ_STR(ArchiveEntry_GetName(third), "oth06.app/");
    CHECK(Archive_Find(test.archive, "README.txt") == nullptr);
    M_Close(&test);
}

TEST(lookup_matches_the_whole_name_exactly)
{
    const ZIP_BUILDER_ENTRY entries[] = {
        { "oth06.app/Contents/Info.plist", "a", 1, 0100644, false },
    };
    M_CASE test;
    M_Open(&test, entries, 1);
    CHECK(
        Archive_Find(test.archive, "oth06.app/Contents/Info.plist")
        == Archive_GetEntry(test.archive, 0));
    CHECK(Archive_Find(test.archive, "Info.plist") == nullptr);
    CHECK(
        Archive_Find(test.archive, "oth06.app/contents/info.plist") == nullptr);
    M_Close(&test);
}

TEST(stored_and_deflated_entries_unpack)
{
    char text[4096] = "";
    for (int i = 0; i < 64; i++) {
        strcat(text, M_TEXT);
    }
    const ZIP_BUILDER_ENTRY entries[] = {
        { "stored", text, strlen(text), 0100644, false },
        { "deflated", text, strlen(text), 0100644, true },
        { "empty", "", 0, 0100644, true },
    };
    M_CASE test;
    M_Open(&test, entries, 3);
    CHECK(test.data.size < 2 * strlen(text));
    M_CheckRead(&test, "stored", text, strlen(text));
    M_CheckRead(&test, "deflated", text, strlen(text));
    M_CheckRead(&test, "empty", "", 0);
    M_Close(&test);
}

TEST(damaged_data_fails_its_crc)
{
    const ZIP_BUILDER_ENTRY entries[] = {
        { "file", M_TEXT, strlen(M_TEXT), 0, false },
    };
    M_CASE test;
    M_Open(&test, entries, 1);
    test.data.data[30 + strlen("file")] ^= 1;
    uint8_t *data;
    const RESULT result =
        Archive_Read(test.archive, Archive_GetEntry(test.archive, 0), &data);
    CHECK(!IS_OK(result));
    CHECK(data == nullptr);
    CHECK_EQ_STR(result.message, "file: its CRC-32 does not match");
    Result_Ignore(result);
    M_Close(&test);
}

TEST(damaged_deflate_data_fails)
{
    char text[1024] = "";
    for (int i = 0; i < 16; i++) {
        strcat(text, M_TEXT);
    }
    const ZIP_BUILDER_ENTRY entries[] = {
        { "file", text, strlen(text), 0, true },
    };
    M_CASE test;
    M_Open(&test, entries, 1);
    test.data.data[30 + strlen("file")] = 0xFF;
    uint8_t *data;
    const RESULT result =
        Archive_Read(test.archive, Archive_GetEntry(test.archive, 0), &data);
    CHECK(!IS_OK(result));
    CHECK(data == nullptr);
    Result_Ignore(result);
    M_Close(&test);
}

TEST(data_without_a_directory_fails)
{
    const char text[] = "not an archive, but long enough to be one";
    const RESULT result = M_OpenBytes((const uint8_t *)text, sizeof(text));
    CHECK_EQ_STR(result.message, "the archive has no end of central directory");
    Result_Ignore(result);
    CHECK(!Result_Ignore(M_OpenBytes((const uint8_t *)"", 0)));
}

TEST(an_end_record_before_a_comment_is_found)
{
    const ZIP_BUILDER_ENTRY entries[] = {
        { "file", M_TEXT, strlen(M_TEXT), 0, false },
    };
    ZIP_BUILDER_ARCHIVE archive = ZipBuilder_Build(entries, 1);
    const size_t comment = 300;
    uint8_t *const data = Mem_AllocZ(archive.size + comment);
    memcpy(data, archive.data, archive.size);
    data[archive.size - 2] = (uint8_t)comment;
    data[archive.size - 1] = (uint8_t)(comment >> 8);
    CHECK(
        Result_Log(M_OpenBytes(data, archive.size + comment), LOG_LEVEL_ERROR));
    Mem_Free(data);
    Mem_Free(archive.data);
}

TEST(directory_past_the_end_fails)
{
    const ZIP_BUILDER_ENTRY entries[] = {
        { "file", M_TEXT, strlen(M_TEXT), 0, false },
    };
    ZIP_BUILDER_ARCHIVE archive = ZipBuilder_Build(entries, 1);
    archive.data[archive.size - 6] = 0xFF;
    CHECK(!Result_Ignore(M_OpenBytes(archive.data, archive.size)));
    Mem_Free(archive.data);
}

TEST(an_end_record_far_from_the_start_is_found)
{
    const size_t size = 100000;
    char *const text = Mem_AllocZ(size);
    memset(text, 'x', size);
    const ZIP_BUILDER_ENTRY entries[] = {
        { "large", text, size, 0, false },
    };
    M_CASE test;
    M_Open(&test, entries, 1);
    M_CheckRead(&test, "large", text, size);
    M_Close(&test);
    Mem_Free(text);
}
