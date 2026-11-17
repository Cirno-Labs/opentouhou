#include "th01/score_data.h"

#include "core/hash/sha256.h"
#include "test.h"
#include "test_dir.h"

#include <limits.h>
#include <string.h>

static VFS *M_UserFiles(const char *const directory)
{
    VFS *const vfs = Vfs_Create(&g_HeapAllocator);
    CHECK(
        Result_Log(Vfs_MountDir(vfs, "", directory, 0, true), LOG_LEVEL_ERROR));
    return vfs;
}

static uint8_t *M_Encode(const SCORE_TABLE *const table, size_t *const size)
{
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator, nullptr);
    CHECK(Result_Log(ScoreData_Write(&writer, table), LOG_LEVEL_ERROR));
    uint8_t *const bytes = Writer_TakeBuffer(&writer, size);
    CHECK(Result_Ignore(Writer_Close(&writer)));
    return bytes;
}

TEST(defaults_and_original_round_trip)
{
    SCORE_TABLE table;
    ScoreData_Defaults(&table);
    size_t size;
    uint8_t *const bytes = M_Encode(&table, &size);
    CHECK_EQ_INT(size, SCORE_FILE_SIZE);
    char hash[SHA256_HEX_SIZE];
    Sha256_ComputeHex(bytes, size, hash);
    CHECK_EQ_STR(
        hash,
        "236cca73c49435551cc3978a2f7ce6b349b3cd9c55d5b5aa811ed2ffdf9b5bd1");
    CHECK(memcmp(bytes, "HISCORE", 7) == 0);
    for (int32_t i = 0; i < SCORE_PLACES; i++) {
        CHECK_EQ_INT(table.entries[i].score, 1000 - i * 100);
        CHECK_EQ_INT(table.entries[i].stage, 10 - i);
        CHECK_EQ_STR(table.entries[i].name, "東方★靈異伝　　");
        CHECK_EQ_STR(table.entries[i].route, "無");
    }
    READER reader;
    Reader_OpenBuffer(&reader, bytes, size, nullptr);
    SCORE_TABLE decoded;
    CHECK(Result_Ignore(ScoreData_Read(&reader, &decoded)));
    CHECK(memcmp(&table, &decoded, sizeof(table)) == 0);
    Reader_Close(&reader);
    Allocator_Free(&g_HeapAllocator, bytes, size);
}

TEST(all_truncations_and_invalid_names_leave_output_intact)
{
    SCORE_TABLE table;
    ScoreData_Defaults(&table);
    size_t size;
    uint8_t *const bytes = M_Encode(&table, &size);
    for (size_t length = 0; length < size; length++) {
        SCORE_TABLE out = table;
        READER reader;
        Reader_OpenBuffer(&reader, bytes, length, nullptr);
        CHECK(!Result_Ignore(ScoreData_Read(&reader, &out)));
        CHECK(memcmp(&table, &out, sizeof(table)) == 0);
    }
    bytes[7] = 0x9c;
    SCORE_TABLE out = table;
    READER reader;
    Reader_OpenBuffer(&reader, bytes, size, nullptr);
    CHECK(!Result_Ignore(ScoreData_Read(&reader, &out)));
    CHECK(memcmp(&table, &out, sizeof(table)) == 0);
    Allocator_Free(&g_HeapAllocator, bytes, size);
}

TEST(signature_checks_only_first_four_bytes_and_ignores_trailing_data)
{
    SCORE_TABLE table;
    ScoreData_Defaults(&table);
    size_t size;
    uint8_t *const bytes = M_Encode(&table, &size);
    bytes[4] = 0;
    bytes[5] = 0xff;
    bytes[6] = 1;
    READER reader;
    Reader_OpenBuffer(&reader, bytes, size, nullptr);
    SCORE_TABLE out;
    CHECK(Result_Ignore(ScoreData_Read(&reader, &out)));
    for (int32_t i = 0; i < 4; i++) {
        bytes[i] ^= 1;
        Reader_OpenBuffer(&reader, bytes, size, nullptr);
        CHECK(!Result_Ignore(ScoreData_Read(&reader, &out)));
        bytes[i] ^= 1;
    }
    Allocator_Free(&g_HeapAllocator, bytes, size);
}

TEST(signed_insertion_ties_precede_existing_entries)
{
    SCORE_TABLE table;
    ScoreData_Defaults(&table);
    SCORE_ENTRY entry = table.entries[4];
    strcpy(entry.name, "                ");
    strcpy(entry.route, "地");
    entry.stage = 60;
    CHECK_EQ_INT(ScoreData_Insert(&table, &entry), 4);
    CHECK_EQ_STR(table.entries[4].name, "                ");
    CHECK_EQ_STR(table.entries[4].route, "地");
    CHECK_EQ_INT(table.entries[5].score, entry.score);
    entry.score = INT32_MIN;
    CHECK_EQ_INT(ScoreData_Insert(&table, &entry), SCORE_PLACES);
    entry.score = INT32_MAX;
    CHECK_EQ_INT(ScoreData_Insert(&table, &entry), 0);
    size_t size;
    uint8_t *const bytes = M_Encode(&table, &size);
    READER reader;
    Reader_OpenBuffer(&reader, bytes, size, nullptr);
    SCORE_TABLE out;
    CHECK(Result_Ignore(ScoreData_Read(&reader, &out)));
    CHECK_EQ_STR(out.entries[0].name, "                ");
    CHECK_EQ_INT(out.entries[0].score, INT32_MAX);
    Allocator_Free(&g_HeapAllocator, bytes, size);
}

TEST(user_directory_takes_priority_and_originals_stay_unchanged)
{
    char *const original_dir = TestDir_Prepare("original");
    char *const user_dir = TestDir_Prepare("user");
    VFS *const user_files = M_UserFiles(user_dir);
    SCORE_TABLE table;
    ScoreData_Defaults(&table);
    table.entries[0].score = 12345;
    size_t size;
    uint8_t *const bytes = M_Encode(&table, &size);
    char *const original = TestDir_Join(original_dir, ScoreData_File(1));
    TestDir_Write(original, bytes, size);
    VFS *const vfs = Vfs_Create(&g_HeapAllocator);
    CHECK(Result_Ignore(Vfs_MountDir(vfs, "", original_dir, 0, false)));
    SCORE_SAVES saves;
    CHECK(Result_Ignore(ScoreData_Load(vfs, user_files, &saves)));
    CHECK_EQ_INT(saves.tables[1].entries[0].score, 12345);
    saves.tables[1].entries[0].score = 54321;
    ScoreData_Queue(&saves, 1);
    CHECK(Result_Ignore(ScoreData_WriteSaves(&saves)));
    CHECK(!saves.pending[1]);
    CHECK(Result_Ignore(ScoreData_Load(vfs, user_files, &saves)));
    CHECK_EQ_INT(saves.tables[1].entries[0].score, 54321);
    READER reader;
    CHECK(Result_Ignore(Reader_OpenFile(&reader, original)));
    SCORE_TABLE restored;
    CHECK(Result_Ignore(ScoreData_Read(&reader, &restored)));
    CHECK_EQ_INT(restored.entries[0].score, 12345);
    Reader_Close(&reader);
    char *const native = TestDir_Join(user_dir, ScoreData_File(1));
    TestDir_Write(native, "broken", 6);
    CHECK(!Result_Ignore(ScoreData_Load(vfs, user_files, &saves)));
    CHECK(!saves.writable[1]);
    ScoreData_Queue(&saves, 1);
    CHECK(Result_Ignore(ScoreData_WriteSaves(&saves)));
    CHECK(Result_Ignore(Reader_OpenFile(&reader, native)));
    CHECK_EQ_INT(Reader_Size(&reader), 6);
    Reader_Close(&reader);
    Vfs_Destroy(vfs);
    Allocator_Free(&g_HeapAllocator, bytes, size);
    Mem_Free(native);
    Mem_Free(original);
    Vfs_Destroy(user_files);
    Mem_Free(user_dir);
    Mem_Free(original_dir);
}

TEST(failed_write_consumes_request_without_changing_simulation)
{
    char *const directory = TestDir_Prepare("failure");
    VFS *const user_files = Vfs_Create(&g_HeapAllocator);
    SCORE_SAVES saves;
    CHECK(Result_Ignore(ScoreData_Load(nullptr, user_files, &saves)));
    saves.writable[0] = true;
    const SCORE_TABLE before = saves.tables[0];
    ScoreData_Queue(&saves, 0);
    CHECK(!Result_Ignore(ScoreData_WriteSaves(&saves)));
    CHECK(!saves.pending[0]);
    CHECK(memcmp(&before, &saves.tables[0], sizeof(before)) == 0);
    CHECK(Result_Ignore(ScoreData_WriteSaves(&saves)));
    Vfs_Destroy(user_files);
    Mem_Free(directory);
}
