#include "core/archive/zip.h"

#include "core/archive.h"
#include "test.h"
#include "test_allocator.h"
#include "test_dir.h"
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

static uint8_t *M_Write(
    const ZIP_ENTRY *const entries, const size_t count, size_t *const size)
{
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator, nullptr);
    CHECK(Result_Log(Zip_Write(&writer, entries, count), LOG_LEVEL_ERROR));
    uint8_t *const data = Writer_TakeBuffer(&writer, size);
    Result_Ignore(Writer_Close(&writer));
    return data;
}

TEST(written_entries_read_back_and_equal_entries_give_equal_files)
{
    char text[4096] = "";
    for (int i = 0; i < 64; i++) {
        strcat(text, M_TEXT);
    }
    const ZIP_ENTRY entries[] = {
        { "dir/紅魔郷.txt", text, strlen(text) },
        { "empty", "", 0 },
    };
    size_t size;
    uint8_t *const data = M_Write(entries, 2, &size);
    size_t again_size;
    uint8_t *const again = M_Write(entries, 2, &again_size);
    CHECK(size < strlen(text));
    CHECK(size == again_size && memcmp(data, again, size) == 0);
    M_CASE test = {};
    READER reader;
    Reader_OpenBuffer(&reader, data, size, nullptr);
    CHECK(Result_Log(
        Archive_Open(&g_ZipFormat, &reader, &g_HeapAllocator, &test.archive),
        LOG_LEVEL_ERROR));
    M_CheckRead(&test, "dir/紅魔郷.txt", text, strlen(text));
    M_CheckRead(&test, "empty", "", 0);
    Archive_Close(test.archive);
    Allocator_Free(&g_HeapAllocator, data, size);
    Allocator_Free(&g_HeapAllocator, again, again_size);
}

TEST(a_name_without_the_utf8_flag_reads_as_cp932_or_raw)
{
    const ZIP_BUILDER_ENTRY entries[] = {
        { "\x93\x8c\x95\xfb/th06.dat", "a", 1, 0, false },
        { "\x81.txt", "b", 1, 0, false },
    };
    M_CASE test;
    M_Open(&test, entries, 2);
    CHECK_EQ_STR(
        ArchiveEntry_GetName(Archive_GetEntry(test.archive, 0)),
        "東方/th06.dat");
    CHECK_EQ_STR(
        ArchiveEntry_GetName(Archive_GetEntry(test.archive, 1)), "\x81.txt");
    M_CheckRead(&test, "東方/th06.dat", "a", 1);
    M_Close(&test);
}

static READER M_Stream(M_CASE *const test, const char *const name)
{
    VFS_SOURCE *const source = Archive_ToSource(test->archive, "");
    READER reader;
    CHECK(Result_Log(
        source->funcs->open(source, name, VFS_STREAM, &reader),
        LOG_LEVEL_ERROR));
    return reader;
}

TEST(streamed_entries_seek_slice_and_interleave)
{
    const size_t size = 9 * 1024 * 1024 + 71;
    uint8_t *const pattern = Mem_Alloc(size);
    for (size_t i = 0; i < size; i++) {
        pattern[i] = (uint8_t)(i * 7 + (i >> 13));
    }
    for (int deflate = 0; deflate < 2; deflate++) {
        const ZIP_BUILDER_ENTRY entry = { "music", pattern, size, 0,
                                          deflate != 0 };
        M_CASE test;
        M_Open(&test, &entry, 1);
        READER first = M_Stream(&test, "music");
        READER second;
        VFS_SOURCE *const source = (VFS_SOURCE *)test.archive;
        CHECK(Result_Log(
            source->funcs->open(source, "music", VFS_STREAM, &second),
            LOG_LEVEL_ERROR));
        const size_t offsets[] = { 0,          5 * 1024 * 1024,
                                   23,         4 * 1024 * 1024 - 10,
                                   size - 128, 0 };
        for (size_t i = 0; i < sizeof(offsets) / sizeof(*offsets); i++) {
            uint8_t bytes[128];
            Reader_Seek(&first, offsets[i]);
            Reader_ReadBytes(&first, bytes, sizeof(bytes));
            CHECK(Result_Log(Reader_Result(&first), LOG_LEVEL_ERROR));
            CHECK(memcmp(bytes, pattern + offsets[i], sizeof(bytes)) == 0);
            CHECK_EQ_INT(Reader_ReadU8(&second), pattern[i]);
        }
        Reader_Seek(&first, 4 * 1024 * 1024 - 8);
        READER slice = Reader_Slice(&first, 128);
        uint8_t bytes[128];
        Reader_ReadBytes(&slice, bytes, sizeof(bytes));
        CHECK(Result_Log(Reader_Result(&slice), LOG_LEVEL_ERROR));
        CHECK(memcmp(bytes, pattern + 4 * 1024 * 1024 - 8, sizeof(bytes)) == 0);
        Reader_Seek(&first, size);
        CHECK(Result_Log(Reader_Result(&first), LOG_LEVEL_ERROR));
        Reader_ReadU8(&first);
        CHECK(!Result_Ignore(Reader_Result(&first)));
        Reader_Close(&second);
        Reader_Close(&first);
        M_Close(&test);
    }
    Mem_Free(pattern);
}

TEST(streaming_checks_crc_and_local_header)
{
    for (int deflate = 0; deflate < 2; deflate++) {
        const ZIP_BUILDER_ENTRY entry = { "file", M_TEXT, strlen(M_TEXT), 0,
                                          deflate != 0 };
        for (int damage = 0; damage < 3; damage++) {
            M_CASE test;
            M_Open(&test, &entry, 1);
            if (damage == 0) {
                test.data.data[30 + strlen(entry.name)] ^= 0xFF;
            } else if (damage == 1) {
                test.data.data[0] ^= 1;
            } else {
                test.data.data[28] = 0xFF;
                test.data.data[29] = 0xFF;
            }
            VFS_SOURCE *const source = Archive_ToSource(test.archive, "");
            READER reader;
            CHECK(!Result_Ignore(
                source->funcs->open(source, "file", VFS_STREAM, &reader)));
            M_Close(&test);
        }
    }
}

TEST(empty_entries_stream)
{
    for (int deflate = 0; deflate < 2; deflate++) {
        const ZIP_BUILDER_ENTRY entry = { "empty", "", 0, 0, deflate != 0 };
        M_CASE test;
        M_Open(&test, &entry, 1);
        READER reader = M_Stream(&test, "empty");
        CHECK_EQ_INT(Reader_Size(&reader), 0);
        Reader_Seek(&reader, 0);
        CHECK(Result_Log(Reader_Result(&reader), LOG_LEVEL_ERROR));
        Reader_Close(&reader);
        M_Close(&test);
    }
}

static ZIP_BUILDER_ARCHIVE M_LargeZip(const size_t size)
{
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator, nullptr);
    z_stream stream = {};
    CHECK(
        deflateInit2(
            &stream, Z_BEST_SPEED, Z_DEFLATED, -MAX_WBITS, 8,
            Z_DEFAULT_STRATEGY)
        == Z_OK);
    uint8_t input[65536] = {};
    uint8_t output[65536];
    uint32_t crc = 0;
    size_t remaining = size;
    int status = Z_OK;
    while (status != Z_STREAM_END) {
        const size_t count =
            remaining < sizeof(input) ? remaining : sizeof(input);
        crc = Crc32_Update(crc, input, count);
        stream.next_in = input;
        stream.avail_in = (uInt)count;
        remaining -= count;
        do {
            stream.next_out = output;
            stream.avail_out = sizeof(output);
            status = deflate(&stream, remaining == 0 ? Z_FINISH : Z_NO_FLUSH);
            CHECK(status == Z_OK || status == Z_STREAM_END);
            Writer_WriteBytes(
                &writer, output, sizeof(output) - stream.avail_out);
        } while (stream.avail_in != 0
                 || (remaining == 0 && status != Z_STREAM_END));
    }
    deflateEnd(&stream);
    size_t packed_size;
    uint8_t *const packed = Writer_TakeBuffer(&writer, &packed_size);
    CHECK(Result_Ignore(Writer_Close(&writer)));
    const ZIP_BUILDER_ENTRY entry = { "music", "", 0, 0, true };
    ZIP_BUILDER_ARCHIVE small = ZipBuilder_Build(&entry, 1);
    const size_t header = 30 + strlen(entry.name);
    const size_t directory_size = 46 + strlen(entry.name);
    const size_t directory = header + packed_size;
    ZIP_BUILDER_ARCHIVE result = {
        .size = directory + directory_size + 22,
    };
    result.data = Mem_Alloc(result.size);
    memcpy(result.data, small.data, header);
    memcpy(result.data + header, packed, packed_size);
    memcpy(
        result.data + directory, small.data + small.size - directory_size - 22,
        directory_size + 22);
    uint8_t *cursor = result.data + 14;
    M_ZipPut32(&cursor, crc);
    M_ZipPut32(&cursor, (uint32_t)packed_size);
    M_ZipPut32(&cursor, (uint32_t)size);
    cursor = result.data + directory + 16;
    M_ZipPut32(&cursor, crc);
    M_ZipPut32(&cursor, (uint32_t)packed_size);
    M_ZipPut32(&cursor, (uint32_t)size);
    cursor = result.data + result.size - 6;
    M_ZipPut32(&cursor, (uint32_t)directory);
    Mem_Free(small.data);
    Allocator_Free(&g_HeapAllocator, packed, packed_size);
    return result;
}

TEST(retail_sized_music_streams_without_a_whole_entry_allocation)
{
    const size_t size = 444516656;
    ZIP_BUILDER_ARCHIVE built = M_LargeZip(size);
    TEST_ALLOCATOR allocator;
    TestAllocator_Init(&allocator);
    READER input;
    Reader_OpenBuffer(&input, built.data, built.size, "game.zip");
    ARCHIVE *archive;
    CHECK(Result_Log(
        Archive_Open(&g_ZipFormat, &input, &allocator.allocator, &archive),
        LOG_LEVEL_ERROR));
    VFS_SOURCE *const source = Archive_ToSource(archive, "");
    READER reader;
    CHECK(Result_Log(
        source->funcs->open(source, "music", VFS_STREAM, &reader),
        LOG_LEVEL_ERROR));
    CHECK_EQ_STR(Reader_GetName(&reader), "game.zip/music");
    CHECK_EQ_INT(Reader_Size(&reader), size);
    CHECK(allocator.live_bytes < 1024 * 1024);
    const size_t offsets[] = { size - 1, 4 * 1024 * 1024, 0, size / 2 };
    for (size_t i = 0; i < sizeof(offsets) / sizeof(*offsets); i++) {
        Reader_Seek(&reader, offsets[i]);
        CHECK_EQ_INT(Reader_ReadU8(&reader), 0);
        CHECK(Result_Log(Reader_Result(&reader), LOG_LEVEL_ERROR));
    }
    Reader_Close(&reader);
    source->funcs->close(source);
    CHECK_EQ_INT(allocator.live_blocks, 0);
    CHECK_EQ_INT(allocator.size_errors, 0);
    Mem_Free(built.data);
}

TEST(streaming_rejects_wrong_sizes_and_releases_checkpoints)
{
    for (int damage = 0; damage < 3; damage++) {
        const ZIP_BUILDER_ENTRY entry = { "music", M_TEXT, strlen(M_TEXT), 0,
                                          true };
        ZIP_BUILDER_ARCHIVE built = ZipBuilder_Build(&entry, 1);
        const size_t directory = built.size - 22 - 46 - strlen(entry.name);
        uint8_t *cursor = built.data + directory + (damage == 2 ? 20 : 24);
        const uint32_t size = damage == 0 ? (uint32_t)entry.size - 1
            : damage == 1 ? (uint32_t)entry.size + 1
                          : (uint32_t)(directory - 30 - strlen(entry.name) - 1);
        M_ZipPut32(&cursor, size);
        TEST_ALLOCATOR allocator;
        TestAllocator_Init(&allocator);
        READER input;
        Reader_OpenBuffer(&input, built.data, built.size, nullptr);
        ARCHIVE *archive;
        CHECK(Result_Log(
            Archive_Open(&g_ZipFormat, &input, &allocator.allocator, &archive),
            LOG_LEVEL_ERROR));
        VFS_SOURCE *const source = Archive_ToSource(archive, "");
        READER reader;
        CHECK(!Result_Ignore(
            source->funcs->open(source, "music", VFS_STREAM, &reader)));
        Reader_Close(&reader);
        source->funcs->close(source);
        CHECK_EQ_INT(allocator.live_blocks, 0);
        CHECK_EQ_INT(allocator.size_errors, 0);
        Mem_Free(built.data);
    }
}

TEST(whole_entry_limit_is_checked_before_allocation)
{
    const ZIP_BUILDER_ENTRY entry = { "music", "", 0, 0, true };
    ZIP_BUILDER_ARCHIVE built = ZipBuilder_Build(&entry, 1);
    const size_t directory = built.size - 22 - 46 - strlen(entry.name);
    uint8_t *cursor = built.data + directory + 24;
    M_ZipPut32(&cursor, (uint32_t)ZIP_MAX_ENTRY_SIZE + 1);
    TEST_ALLOCATOR allocator;
    TestAllocator_Init(&allocator);
    READER input;
    Reader_OpenBuffer(&input, built.data, built.size, "game.zip");
    ARCHIVE *archive;
    CHECK(Result_Log(
        Archive_Open(&g_ZipFormat, &input, &allocator.allocator, &archive),
        LOG_LEVEL_ERROR));
    uint8_t *data;
    const RESULT result =
        Archive_Read(archive, Archive_GetEntry(archive, 0), &data);
    CHECK(!IS_OK(result));
    CHECK_EQ_STR(
        result.message, "game.zip: music: entry is too large to unpack");
    CHECK(data == nullptr);
    CHECK(allocator.live_bytes < 1024 * 1024);
    Result_Ignore(result);
    Archive_Close(archive);
    CHECK_EQ_INT(allocator.live_blocks, 0);
    Mem_Free(built.data);
}

TEST(file_backed_streams_keep_independent_positions)
{
    const ZIP_BUILDER_ENTRY entries[] = {
        { "stored", M_TEXT, strlen(M_TEXT), 0, false },
        { "deflated", M_TEXT, strlen(M_TEXT), 0, true },
    };
    ZIP_BUILDER_ARCHIVE built = ZipBuilder_Build(entries, 2);
    char *const directory = TestDir_Prepare("file_backed_streams");
    char *const path = TestDir_Join(directory, "game.zip");
    CHECK(SDL_SaveFile(path, built.data, built.size));
    READER input;
    CHECK(Result_Log(Reader_OpenFile(&input, path), LOG_LEVEL_ERROR));
    ARCHIVE *archive;
    CHECK(Result_Log(
        Archive_Open(&g_ZipFormat, &input, &g_HeapAllocator, &archive),
        LOG_LEVEL_ERROR));
    VFS_SOURCE *const source = Archive_ToSource(archive, "");
    READER readers[2];
    for (int i = 0; i < 2; i++) {
        CHECK(Result_Log(
            source->funcs->open(
                source, entries[i].name, VFS_STREAM, &readers[i]),
            LOG_LEVEL_ERROR));
    }
    for (size_t i = 0; i < strlen(M_TEXT); i++) {
        CHECK_EQ_INT(Reader_ReadU8(&readers[0]), (uint8_t)M_TEXT[i]);
        CHECK_EQ_INT(Reader_ReadU8(&readers[1]), (uint8_t)M_TEXT[i]);
    }
    for (int i = 0; i < 2; i++) {
        CHECK(Result_Log(Reader_Result(&readers[i]), LOG_LEVEL_ERROR));
        Reader_Close(&readers[i]);
    }
    source->funcs->close(source);
    Mem_Free(path);
    Mem_Free(directory);
    Mem_Free(built.data);
}
