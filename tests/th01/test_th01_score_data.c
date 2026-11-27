#include "tests/test.h"
#include "tests/test_dir.h"
#include "th01/score_data.h"

#include "core/base/memory.h"
#include "core/hash/sha256.h"
#include "core/io/reader.h"

#include <limits.h>
#include <string.h>

static VFS *M_UserFiles(const char *const directory)
{
    VFS *const vfs = Vfs_Create(&g_HeapAllocator);
    CHECK(
        Result_Log(Vfs_MountDir(vfs, "", directory, 0, true), LOG_LEVEL_ERROR));
    return vfs;
}

static uint8_t *M_ReadFile(const char *const path, size_t *const size)
{
    READER reader;
    CHECK(Result_Ignore(Reader_OpenFile(&reader, path)));
    *size = Reader_Size(&reader);
    uint8_t *const bytes = Mem_Alloc(*size);
    Reader_ReadBytes(&reader, bytes, *size);
    CHECK(Result_Ignore(Reader_Result(&reader)));
    Reader_Close(&reader);
    return bytes;
}

static SCORE_TABLE M_Defaults(void)
{
    VFS *const user_files = Vfs_Create(&g_HeapAllocator);
    SCORE_SAVES saves;
    CHECK(Result_Ignore(ScoreData_Load(nullptr, user_files, &saves)));
    Vfs_Destroy(user_files);
    return saves.tables[0];
}

// Returns the bytes that ScoreData_WriteSaves writes for table.
static uint8_t *M_Encode(const SCORE_TABLE *const table, size_t *const size)
{
    char *const directory = TestDir_Prepare("encode");
    VFS *const user_files = M_UserFiles(directory);
    SCORE_SAVES saves;
    CHECK(Result_Ignore(ScoreData_Load(nullptr, user_files, &saves)));
    saves.tables[0] = *table;
    CHECK(Result_Log(ScoreData_WriteSaves(&saves), LOG_LEVEL_ERROR));
    char *const path = TestDir_Join(directory, "REYHIES.DAT");
    uint8_t *const bytes = M_ReadFile(path, size);
    Mem_Free(path);
    Vfs_Destroy(user_files);
    Mem_Free(directory);
    return bytes;
}

// Loads bytes as the original REYHINO.DAT into out.
static RESULT M_Decode(
    const uint8_t *const bytes, const size_t size, SCORE_TABLE *const out)
{
    char *const original_dir = TestDir_Prepare("decode-original");
    char *const user_dir = TestDir_Prepare("decode-user");
    char *const path = TestDir_Join(original_dir, "REYHINO.DAT");
    TestDir_Write(path, bytes, size);
    VFS *const original = Vfs_Create(&g_HeapAllocator);
    CHECK(Result_Ignore(Vfs_MountDir(original, "", original_dir, 0, false)));
    VFS *const user_files = M_UserFiles(user_dir);
    SCORE_SAVES saves;
    const RESULT result = ScoreData_Load(original, user_files, &saves);
    *out = saves.tables[1];
    Vfs_Destroy(user_files);
    Vfs_Destroy(original);
    Mem_Free(path);
    Mem_Free(user_dir);
    Mem_Free(original_dir);
    return result;
}

TEST(defaults_and_original_round_trip)
{
    const SCORE_TABLE table = M_Defaults();
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
    SCORE_TABLE decoded;
    CHECK(Result_Ignore(M_Decode(bytes, size, &decoded)));
    CHECK(memcmp(&table, &decoded, sizeof(table)) == 0);
    Mem_Free(bytes);
}

TEST(all_truncations_and_invalid_names_fall_back_to_defaults)
{
    const SCORE_TABLE table = M_Defaults();
    SCORE_TABLE changed = table;
    changed.entries[0].score = 12345;
    size_t size;
    uint8_t *const bytes = M_Encode(&changed, &size);
    for (size_t length = 0; length < size; length++) {
        SCORE_TABLE out;
        CHECK(!Result_Ignore(M_Decode(bytes, length, &out)));
        CHECK(memcmp(&table, &out, sizeof(table)) == 0);
    }
    bytes[7] = 0x9c;
    SCORE_TABLE out;
    CHECK(!Result_Ignore(M_Decode(bytes, size, &out)));
    CHECK(memcmp(&table, &out, sizeof(table)) == 0);
    Mem_Free(bytes);
}

TEST(signature_checks_only_first_four_bytes_and_ignores_trailing_data)
{
    const SCORE_TABLE table = M_Defaults();
    size_t size;
    uint8_t *const encoded = M_Encode(&table, &size);
    uint8_t *const bytes = Mem_Alloc(size + 3);
    memcpy(bytes, encoded, size);
    memset(bytes + size, 0xff, 3);
    bytes[4] = 0;
    bytes[5] = 0xff;
    bytes[6] = 1;
    SCORE_TABLE out;
    CHECK(Result_Ignore(M_Decode(bytes, size + 3, &out)));
    for (int32_t i = 0; i < 4; i++) {
        bytes[i] ^= 1;
        CHECK(!Result_Ignore(M_Decode(bytes, size, &out)));
        bytes[i] ^= 1;
    }
    Mem_Free(bytes);
    Mem_Free(encoded);
}

TEST(signed_insertion_ties_precede_existing_entries)
{
    SCORE_TABLE table = M_Defaults();
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
    SCORE_TABLE out;
    CHECK(Result_Ignore(M_Decode(bytes, size, &out)));
    CHECK_EQ_STR(out.entries[0].name, "                ");
    CHECK_EQ_INT(out.entries[0].score, INT32_MAX);
    Mem_Free(bytes);
}

TEST(user_directory_takes_priority_and_originals_stay_unchanged)
{
    char *const original_dir = TestDir_Prepare("original");
    char *const user_dir = TestDir_Prepare("user");
    VFS *const user_files = M_UserFiles(user_dir);
    SCORE_TABLE table = M_Defaults();
    table.entries[0].score = 12345;
    size_t size;
    uint8_t *const bytes = M_Encode(&table, &size);
    char *const original = TestDir_Join(original_dir, "REYHINO.DAT");
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
    size_t original_size;
    uint8_t *const restored = M_ReadFile(original, &original_size);
    CHECK_EQ_INT(original_size, size);
    CHECK(memcmp(restored, bytes, size) == 0);
    Mem_Free(restored);
    char *const native = TestDir_Join(user_dir, "REYHINO.DAT");
    TestDir_Write(native, "broken", 6);
    CHECK(!Result_Ignore(ScoreData_Load(vfs, user_files, &saves)));
    CHECK(!saves.writable[1]);
    ScoreData_Queue(&saves, 1);
    CHECK(Result_Ignore(ScoreData_WriteSaves(&saves)));
    READER reader;
    CHECK(Result_Ignore(Reader_OpenFile(&reader, native)));
    CHECK_EQ_INT(Reader_Size(&reader), 6);
    Reader_Close(&reader);
    Vfs_Destroy(vfs);
    Mem_Free(bytes);
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

TEST(missing_native_tables_queue_default_creation)
{
    char *const directory = TestDir_Prepare("create");
    VFS *const user_files = M_UserFiles(directory);
    SCORE_SAVES saves;
    CHECK(Result_Ignore(ScoreData_Load(nullptr, user_files, &saves)));
    for (int32_t rank = 0; rank < SCORE_RANKS; rank++) {
        CHECK(saves.pending[rank]);
    }
    CHECK(Result_Ignore(ScoreData_WriteSaves(&saves)));
    static const char *const files[SCORE_RANKS] = {
        "REYHIES.DAT",
        "REYHINO.DAT",
        "REYHIHA.DAT",
        "REYHILU.DAT",
    };
    for (int32_t rank = 0; rank < SCORE_RANKS; rank++) {
        char *const path = TestDir_Join(directory, files[rank]);
        size_t size;
        Mem_Free(M_ReadFile(path, &size));
        CHECK_EQ_INT(size, SCORE_FILE_SIZE);
        Mem_Free(path);
    }
    CHECK(Result_Ignore(ScoreData_Load(nullptr, user_files, &saves)));
    for (int32_t rank = 0; rank < SCORE_RANKS; rank++) {
        CHECK(!saves.pending[rank]);
        CHECK_EQ_INT(saves.tables[rank].entries[0].score, 1000);
    }
    Vfs_Destroy(user_files);
    Mem_Free(directory);
}

TEST(invalid_queued_name_does_not_replace_a_valid_file)
{
    char *const directory = TestDir_Prepare("invalid-save");
    VFS *const user_files = M_UserFiles(directory);
    SCORE_SAVES saves;
    CHECK(Result_Ignore(ScoreData_Load(nullptr, user_files, &saves)));
    CHECK(Result_Ignore(ScoreData_WriteSaves(&saves)));
    strcpy(saves.tables[0].entries[0].name, "short");
    ScoreData_Queue(&saves, 0);
    CHECK(!Result_Ignore(ScoreData_WriteSaves(&saves)));
    CHECK(Result_Ignore(ScoreData_Load(nullptr, user_files, &saves)));
    CHECK_EQ_STR(saves.tables[0].entries[0].name, "東方★靈異伝　　");
    Vfs_Destroy(user_files);
    Mem_Free(directory);
}

TEST(existing_lowercase_native_file_keeps_its_spelling)
{
    char *const directory = TestDir_Prepare("lowercase");
    VFS *const user_files = M_UserFiles(directory);
    char *const path = TestDir_Join(directory, "reyhies.dat");
    const SCORE_TABLE table = M_Defaults();
    size_t size;
    uint8_t *const bytes = M_Encode(&table, &size);
    TestDir_Write(path, bytes, size);
    Mem_Free(bytes);
    SCORE_SAVES saves;
    CHECK(Result_Ignore(ScoreData_Load(nullptr, user_files, &saves)));
    saves.tables[0].entries[0].score = 45678;
    ScoreData_Queue(&saves, 0);
    CHECK(Result_Ignore(ScoreData_WriteSaves(&saves)));
    CHECK(Result_Ignore(ScoreData_Load(nullptr, user_files, &saves)));
    CHECK_EQ_INT(saves.tables[0].entries[0].score, 45678);
    Mem_Free(path);
    Vfs_Destroy(user_files);
    Mem_Free(directory);
}
