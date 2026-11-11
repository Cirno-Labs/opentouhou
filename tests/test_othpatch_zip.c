#include "othpatch/zip.h"

#include "test.h"
#include "zip_builder.h"

#define M_TEXT "Every package holds the game and othpatch. "

typedef struct {
    ARENA arena;
    ZIP_BUILDER_ARCHIVE archive;
    ZIP_ENTRY_LIST list;
} M_CASE;

static M_CASE M_Open(const ZIP_BUILDER_ENTRY *const entries, const int count)
{
    M_CASE test = {};
    Arena_Init(&test.arena, "zip test", 1 << 20);
    test.archive = ZipBuilder_Build(entries, count);
    CHECK(Result_Log(
        Zip_List(test.archive.data, test.archive.size, &test.arena, &test.list),
        LOG_LEVEL_ERROR));
    return test;
}

static void M_Close(M_CASE *const test)
{
    Mem_Free(test->archive.data);
    Arena_Free(&test->arena);
}

static void M_CheckRead(
    const M_CASE *const test, const char *const name,
    const char *const expected, const size_t expected_size)
{
    const ZIP_ENTRY *const entry = Zip_Find(&test->list, name);
    CHECK(entry != nullptr);
    if (entry == nullptr) {
        return;
    }
    uint8_t *data;
    size_t size;
    CHECK(Result_Log(
        Zip_Read(test->archive.data, test->archive.size, entry, &data, &size),
        LOG_LEVEL_ERROR));
    CHECK_EQ_INT(size, expected_size);
    CHECK(data != nullptr && memcmp(data, expected, expected_size) == 0);
    Mem_Free(data);
}

TEST(list_gives_the_names_and_modes_in_order)
{
    const ZIP_BUILDER_ENTRY entries[] = {
        { "oth06.exe", "a", 1, 0, false },
        { "othpatch", "b", 1, 0100755, true },
        { "oth06.app/", "", 0, 040755, false },
    };
    M_CASE test = M_Open(entries, 3);
    CHECK_EQ_INT(test.list.count, 3);
    CHECK_EQ_STR(test.list.items[0].name, "oth06.exe");
    CHECK_EQ_INT(test.list.items[0].mode, 0);
    CHECK_EQ_STR(test.list.items[1].name, "othpatch");
    CHECK_EQ_INT(test.list.items[1].mode, 0100755);
    CHECK_EQ_STR(test.list.items[2].name, "oth06.app/");
    CHECK(Zip_Find(&test.list, "README.txt") == nullptr);
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
    M_CASE test = M_Open(entries, 3);
    CHECK(test.list.items[1].packed_size < test.list.items[1].size);
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
    M_CASE test = M_Open(entries, 1);
    test.archive.data[30 + strlen("file")] ^= 1;
    uint8_t *data;
    size_t size;
    const RESULT result = Zip_Read(
        test.archive.data, test.archive.size, &test.list.items[0], &data,
        &size);
    CHECK(!IS_OK(result));
    CHECK(data == nullptr);
    CHECK(strstr(result.message, "CRC-32") != nullptr);
    Result_Ignore(result);
    M_Close(&test);
}

TEST(data_without_a_directory_fails)
{
    ARENA arena;
    Arena_Init(&arena, "zip test", 1 << 16);
    ZIP_ENTRY_LIST list;
    const char text[] = "not an archive, but long enough to be one";
    const RESULT result =
        Zip_List((const uint8_t *)text, sizeof(text), &arena, &list);
    CHECK(!IS_OK(result));
    Result_Ignore(result);
    CHECK(!Result_Ignore(Zip_List((const uint8_t *)"", 0, &arena, &list)));
    Arena_Free(&arena);
}

TEST(directory_past_the_end_fails)
{
    const ZIP_BUILDER_ENTRY entries[] = {
        { "file", M_TEXT, strlen(M_TEXT), 0, false },
    };
    ZIP_BUILDER_ARCHIVE archive = ZipBuilder_Build(entries, 1);
    archive.data[archive.size - 6] = 0xFF;
    ARENA arena;
    Arena_Init(&arena, "zip test", 1 << 16);
    ZIP_ENTRY_LIST list;
    CHECK(!Result_Ignore(Zip_List(archive.data, archive.size, &arena, &list)));
    Arena_Free(&arena);
    Mem_Free(archive.data);
}
