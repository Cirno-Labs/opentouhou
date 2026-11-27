#include "tests/test.h"
#include "tests/test_dir.h"

#include "core/base/arena.h"
#include "pc98/mdrv_data.h"

typedef struct {
    uint8_t bytes[2048];
    size_t size;
} DATA_BUILDER;

static void M_Word(
    DATA_BUILDER *const data, const size_t pos, const uint16_t value)
{
    data->bytes[pos] = value;
    data->bytes[pos + 1] = value >> 8;
}

static DATA_BUILDER M_Music(void)
{
    DATA_BUILDER data = { .size = 108 };
    data.bytes[0] = 2;
    data.bytes[1] = 3;
    data.bytes[2] = 6;
    data.bytes[3] = 0x80;
    M_Word(&data, 4, 1);
    const uint8_t channels[] = { 0x80, 0x40, 0x20, 0x10, 0, 0 };
    const uint16_t starts[] = { 56, 58, 56, 60, 0, 63 };
    for (int32_t i = 0; i < 6; i++) {
        M_Word(&data, 6 + i * 4, starts[i]);
        data.bytes[8 + i * 4] = channels[i];
        data.bytes[9 + i * 4] = i;
    }
    M_Word(&data, 30, 64);
    M_Word(&data, 32, 96);
    data.bytes[34] = 0xe0;
    data.bytes[35] = 0xa5;
    memcpy(data.bytes + 36, "PCM     SPM\0\0\0\0\0", 16);
    memcpy(
        data.bytes + 52,
        "\x93\x8c"
        "A$",
        4);
    for (size_t i = 56; i < data.size; i++) {
        data.bytes[i] = (uint8_t)(i ^ 0x81);
    }
    return data;
}

static DATA_BUILDER M_Effects(void)
{
    DATA_BUILDER data = { .size = 50 };
    data.bytes[0] = 1;
    M_Word(&data, 2, 2);
    M_Word(&data, 4, 38);
    M_Word(&data, 6, 0xabcd);
    data.bytes[16] = 2;
    data.bytes[17] = 0x40;
    M_Word(&data, 18, 24);
    data.bytes[20] = 1;
    data.bytes[21] = 0x42;
    M_Word(&data, 22, 32);
    M_Word(&data, 24, 28);
    M_Word(&data, 26, 30);
    M_Word(&data, 32, 34);
    for (size_t i = 38; i < data.size; i++) {
        data.bytes[i] = (uint8_t)(i * 3);
    }
    return data;
}

static RESULT M_LoadMusic(
    const DATA_BUILDER *const data, ARENA *const arena, MDRV_MUSIC *const out)
{
    READER reader;
    Reader_OpenBuffer(&reader, data->bytes, data->size, nullptr);
    return MdrvData_LoadMusic(&reader, Arena_Allocator(arena), out);
}

static RESULT M_LoadEffects(
    const DATA_BUILDER *const data, ARENA *const arena, MDRV_EFFECTS *const out)
{
    READER reader;
    Reader_OpenBuffer(&reader, data->bytes, data->size, nullptr);
    return MdrvData_LoadEffects(&reader, Arena_Allocator(arena), out);
}

static void M_BadMusic(const DATA_BUILDER *const data, ARENA *const arena)
{
    MDRV_MUSIC music;
    memset(&music, 0x55, sizeof(music));
    CHECK(!Result_Ignore(M_LoadMusic(data, arena, &music)));
    CHECK(music.bytes == nullptr);
    CHECK(music.tracks == nullptr);
    CHECK(music.title == nullptr);
    CHECK_EQ_INT(music.track_count, 0);
    CHECK_EQ_INT(music.size, 0);
    CHECK_EQ_INT(arena->used, 0);
}

static void M_BadEffects(const DATA_BUILDER *const data, ARENA *const arena)
{
    MDRV_EFFECTS effects;
    memset(&effects, 0x55, sizeof(effects));
    CHECK(!Result_Ignore(M_LoadEffects(data, arena, &effects)));
    CHECK(effects.bytes == nullptr);
    CHECK(effects.effects == nullptr);
    CHECK_EQ_INT(effects.effect_count, 0);
    CHECK_EQ_INT(effects.size, 0);
    CHECK_EQ_INT(arena->used, 0);
}

TEST(loads_music_metadata_tracks_and_banks_without_decoding_commands)
{
    const DATA_BUILDER data = M_Music();
    ARENA arena;
    Arena_Init(&arena, "music", 65536);
    MDRV_MUSIC music;
    CHECK(Result_Log(M_LoadMusic(&data, &arena, &music), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(music.track_count, 6);
    CHECK_EQ_INT(music.flags, 0x80);
    CHECK_EQ_INT(music.hardware, 1);
    CHECK_EQ_INT(music.metadata_flags, 0xe0);
    CHECK_EQ_INT(music.reserved, 0xa5);
    CHECK_EQ_STR(music.pcm_name, "PCM");
    CHECK_EQ_STR(music.spm_name, "SPM");
    CHECK_EQ_STR(music.title, "東A");
    CHECK_EQ_INT(music.commands_offset, 56);
    CHECK_EQ_INT(music.fm_offset, 64);
    CHECK_EQ_INT(music.ssg_offset, 96);
    CHECK_EQ_INT(music.fm_count, 1);
    CHECK_EQ_INT(music.ssg_count, 2);
    CHECK_EQ_INT(music.tracks[4].offset, 0);
    CHECK_EQ_INT(music.tracks[5].offset, 63);
    CHECK_EQ_INT(music.tracks[5].channel, 0);
    CHECK_EQ_INT(music.tracks[5].reserved, 5);
    CHECK_EQ_INT(music.tracks[0].offset, music.tracks[2].offset);
    CHECK_EQ_INT(memcmp(data.bytes, music.bytes, data.size), 0);
    CHECK_EQ_INT(arena.used, 6 * sizeof(MDRV_TRACK) + data.size + 13);
    Arena_Free(&arena);
}

TEST(loads_absent_empty_and_independent_optional_text_fields)
{
    ARENA arena;
    Arena_Init(&arena, "metadata", 65536);
    for (uint8_t flags = 0; flags < 8; flags++) {
        DATA_BUILDER data = M_Music();
        data.bytes[34] = flags << 5;
        size_t pos = 36;
        if ((flags & 4) != 0) {
            memcpy(data.bytes + pos, "PCM     ", 8);
            pos += 8;
        }
        if ((flags & 1) != 0) {
            memcpy(data.bytes + pos, "SPM     ", 8);
            pos += 8;
        }
        if ((flags & 2) != 0) {
            data.bytes[pos++] = '$';
        }
        MDRV_MUSIC music;
        CHECK(Result_Log(M_LoadMusic(&data, &arena, &music), LOG_LEVEL_ERROR));
        CHECK_EQ_INT(music.commands_offset, pos);
        CHECK((music.pcm_name != nullptr) == ((flags & 4) != 0));
        CHECK((music.spm_name != nullptr) == ((flags & 1) != 0));
        CHECK((music.title != nullptr) == ((flags & 2) != 0));
        if (music.title != nullptr) {
            CHECK_EQ_STR(music.title, "");
        }
        Arena_Reset(&arena);
    }
    Arena_Free(&arena);
}

TEST(rejects_truncated_music_except_whole_optional_envelopes)
{
    DATA_BUILDER data = M_Music();
    const size_t size = data.size;
    ARENA arena;
    Arena_Init(&arena, "music truncation", 65536);
    for (data.size = 0; data.size < size; data.size++) {
        if (data.size == 96 || data.size == 102) {
            MDRV_MUSIC music;
            CHECK(Result_Ignore(M_LoadMusic(&data, &arena, &music)));
            CHECK_EQ_INT(music.ssg_count, (data.size - 96) / 6);
            Arena_Reset(&arena);
        } else {
            M_BadMusic(&data, &arena);
        }
    }
    Arena_Free(&arena);
}

TEST(rejects_bad_music_versions_channels_metadata_and_bank_ranges)
{
    const DATA_BUILDER valid = M_Music();
    const size_t positions[] = { 0, 1, 2, 8, 8, 8, 8, 34, 55, 53, 52, 37 };
    const uint8_t values[] = { 1,    2, 255, 0x86, 0x43, 0xc0,
                               0x11, 1, 0,   0x20, 0,    0 };
    ARENA arena;
    Arena_Init(&arena, "invalid music", 65536);
    for (size_t i = 0; i < sizeof(values); i++) {
        DATA_BUILDER data = valid;
        data.bytes[positions[i]] = values[i];
        M_BadMusic(&data, &arena);
    }
    const size_t fields[] = { 6, 6, 6, 6, 26, 30, 30, 32, 32, 32 };
    const uint16_t offsets[] = { 0, 55, 64, 65535, 65535, 35, 65, 63, 95, 109 };
    for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); i++) {
        DATA_BUILDER data = valid;
        M_Word(&data, fields[i], offsets[i]);
        M_BadMusic(&data, &arena);
    }
    Arena_Free(&arena);
}

TEST(rejects_optional_metadata_that_overlaps_tracks_or_banks)
{
    ARENA arena;
    Arena_Init(&arena, "metadata overlap", 65536);
    DATA_BUILDER data = M_Music();
    memset(data.bytes + 52, 'A', 12);
    M_BadMusic(&data, &arena);
    data.bytes[60] = '$';
    M_BadMusic(&data, &arena);
    data = M_Music();
    M_Word(&data, 30, 40);
    M_Word(&data, 32, 72);
    data.size = 78;
    M_BadMusic(&data, &arena);
    Arena_Free(&arena);
}

TEST(loads_effect_channel_groups_and_envelope_bank)
{
    const DATA_BUILDER data = M_Effects();
    ARENA arena;
    Arena_Init(&arena, "effects", 65536);
    MDRV_EFFECTS effects;
    CHECK(Result_Log(M_LoadEffects(&data, &arena, &effects), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(effects.effect_count, 2);
    CHECK_EQ_INT(effects.ssg_offset, 38);
    CHECK_EQ_INT(effects.ssg_count, 2);
    CHECK_EQ_INT(effects.reserved[0], 0xabcd);
    CHECK_EQ_INT(effects.effects[0].channel_count, 2);
    CHECK_EQ_INT(effects.effects[0].base_channel, 0x40);
    CHECK_EQ_INT(effects.effects[0].block_offset, 24);
    CHECK_EQ_INT(effects.effects[0].stream_offsets[0], 28);
    CHECK_EQ_INT(effects.effects[0].stream_offsets[1], 30);
    CHECK_EQ_INT(effects.effects[0].stream_offsets[2], 0);
    CHECK_EQ_INT(effects.effects[1].stream_offsets[0], 34);
    CHECK_EQ_INT(memcmp(data.bytes, effects.bytes, data.size), 0);
    CHECK_EQ_INT(arena.used, 2 * sizeof(MDRV_EFFECT) + data.size);
    Arena_Free(&arena);
}

TEST(allows_shared_effect_groups_and_stream_starts)
{
    DATA_BUILDER data = M_Effects();
    data.bytes[20] = 2;
    data.bytes[21] = 0x40;
    M_Word(&data, 22, 24);
    M_Word(&data, 26, 28);
    ARENA arena;
    Arena_Init(&arena, "shared effects", 65536);
    MDRV_EFFECTS effects;
    CHECK(Result_Log(M_LoadEffects(&data, &arena, &effects), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(effects.effects[1].block_offset, 24);
    CHECK_EQ_INT(effects.effects[1].stream_offsets[1], 28);
    Arena_Free(&arena);
}

TEST(loads_three_effect_channels_and_unordered_groups)
{
    DATA_BUILDER data = M_Effects();
    data.bytes[16] = 3;
    M_Word(&data, 24, 30);
    M_Word(&data, 26, 30);
    M_Word(&data, 28, 31);
    uint8_t first[4];
    memcpy(first, data.bytes + 16, 4);
    memcpy(data.bytes + 16, data.bytes + 20, 4);
    memcpy(data.bytes + 20, first, 4);
    ARENA arena;
    Arena_Init(&arena, "three channels", 65536);
    MDRV_EFFECTS effects;
    CHECK(Result_Log(M_LoadEffects(&data, &arena, &effects), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(effects.effects[1].channel_count, 3);
    CHECK_EQ_INT(effects.effects[1].stream_offsets[2], 31);
    Arena_Free(&arena);
}

TEST(rejects_truncated_effects_except_whole_optional_envelopes)
{
    DATA_BUILDER data = M_Effects();
    const size_t size = data.size;
    ARENA arena;
    Arena_Init(&arena, "effects truncation", 65536);
    for (data.size = 0; data.size < size; data.size++) {
        if (data.size == 38 || data.size == 44) {
            MDRV_EFFECTS effects;
            CHECK(Result_Ignore(M_LoadEffects(&data, &arena, &effects)));
            CHECK_EQ_INT(effects.ssg_count, (data.size - 38) / 6);
            Arena_Reset(&arena);
        } else {
            M_BadEffects(&data, &arena);
        }
    }
    Arena_Free(&arena);
}

TEST(rejects_effect_versions_counts_and_channels)
{
    const DATA_BUILDER valid = M_Effects();
    const size_t positions[] = { 0, 1, 2, 3, 16, 16, 17, 17 };
    const uint8_t values[] = { 0, 1, 255, 255, 0, 4, 0x80, 0x42 };
    ARENA arena;
    Arena_Init(&arena, "bad effects", 65536);
    for (size_t i = 0; i < sizeof(values); i++) {
        DATA_BUILDER data = valid;
        data.bytes[positions[i]] = values[i];
        M_BadEffects(&data, &arena);
    }
    Arena_Free(&arena);
}

TEST(rejects_effect_pointer_overlaps_and_out_of_range_streams)
{
    const DATA_BUILDER valid = M_Effects();
    const size_t fields[] = { 4, 4, 4, 18, 18, 18, 22, 22, 24, 24, 24, 24 };
    const uint16_t offsets[] = { 23, 37, 51, 23, 37, 65535,
                                 25, 24, 27, 32, 38, 65535 };
    ARENA arena;
    Arena_Init(&arena, "effect overlap", 65536);
    for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); i++) {
        DATA_BUILDER data = valid;
        M_Word(&data, fields[i], offsets[i]);
        M_BadEffects(&data, &arena);
    }
    Arena_Free(&arena);
}

TEST(loads_empty_directories_and_empty_banks)
{
    ARENA arena;
    Arena_Init(&arena, "empty data", 65536);
    DATA_BUILDER data = { .size = 12 };
    data.bytes[0] = 2;
    data.bytes[1] = 3;
    M_Word(&data, 6, 12);
    M_Word(&data, 8, 12);
    MDRV_MUSIC music;
    CHECK(Result_Log(M_LoadMusic(&data, &arena, &music), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(music.track_count, 0);
    CHECK_EQ_INT(music.fm_count, 0);
    CHECK_EQ_INT(music.ssg_count, 0);
    data = (DATA_BUILDER) { .size = 16 };
    data.bytes[0] = 1;
    M_Word(&data, 4, 16);
    MDRV_EFFECTS effects;
    CHECK(Result_Log(M_LoadEffects(&data, &arena, &effects), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(effects.effect_count, 0);
    CHECK_EQ_INT(effects.ssg_count, 0);
    Arena_Free(&arena);
}

TEST(loads_the_maximum_music_track_count)
{
    DATA_BUILDER data = { .size = 1033 };
    data.bytes[0] = 2;
    data.bytes[1] = 3;
    data.bytes[2] = 255;
    for (int32_t i = 0; i < 255; i++) {
        M_Word(&data, 6 + i * 4, 1032);
        data.bytes[8 + i * 4] = 0x80;
    }
    M_Word(&data, 1026, 1033);
    M_Word(&data, 1028, 1033);
    ARENA arena;
    Arena_Init(&arena, "track limit", 65536);
    MDRV_MUSIC music;
    CHECK(Result_Log(M_LoadMusic(&data, &arena, &music), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(music.tracks[254].offset, 1032);
    Arena_Free(&arena);
}

TEST(loads_a_full_segment_with_a_long_cp932_title)
{
    const size_t size = 65536;
    uint8_t *const bytes = Mem_AllocZ(size);
    bytes[0] = 2;
    bytes[1] = 3;
    bytes[6] = bytes[8] = 0xfa;
    bytes[7] = bytes[9] = 0xff;
    bytes[10] = 0x40;
    memset(bytes + 12, 0xa1, 65517);
    bytes[65529] = '$';
    READER reader;
    Reader_OpenBuffer(&reader, bytes, size, nullptr);
    ARENA arena;
    Arena_Init(&arena, "full segment", 512 * 1024);
    MDRV_MUSIC music;
    CHECK(Result_Log(
        MdrvData_LoadMusic(&reader, Arena_Allocator(&arena), &music),
        LOG_LEVEL_ERROR));
    CHECK_EQ_INT(music.size, size);
    CHECK_EQ_INT(music.commands_offset, 65530);
    CHECK_EQ_INT(strlen(music.title), 65517 * 3);
    CHECK_EQ_INT(music.ssg_count, 1);
    Mem_Free(bytes);
    Arena_Free(&arena);
}

TEST(bounds_large_effect_counts_before_allocating_descriptors)
{
    const size_t size = 65535;
    const uint16_t count = 16377;
    uint8_t *const bytes = Mem_AllocZ(size);
    bytes[0] = 1;
    bytes[2] = (uint8_t)count;
    bytes[3] = count >> 8;
    bytes[4] = 0xf9;
    bytes[5] = 0xff;
    for (int32_t i = 0; i < count; i++) {
        const size_t pos = 16 + (size_t)i * 4;
        bytes[pos] = 1;
        bytes[pos + 1] = 0x40;
        bytes[pos + 2] = 0xf4;
        bytes[pos + 3] = 0xff;
    }
    bytes[65524] = 0xf6;
    bytes[65525] = 0xff;
    ARENA arena;
    Arena_Init(&arena, "many effects", 512 * 1024);
    READER reader;
    Reader_OpenBuffer(&reader, bytes, size, nullptr);
    MDRV_EFFECTS effects;
    CHECK(Result_Log(
        MdrvData_LoadEffects(&reader, Arena_Allocator(&arena), &effects),
        LOG_LEVEL_ERROR));
    CHECK_EQ_INT(effects.effect_count, count);
    CHECK_EQ_INT(effects.effects[count - 1].stream_offsets[0], 65526);
    Arena_Reset(&arena);
    bytes[2] = bytes[3] = 0xff;
    Reader_OpenBuffer(&reader, bytes, size, nullptr);
    CHECK(!Result_Ignore(
        MdrvData_LoadEffects(&reader, Arena_Allocator(&arena), &effects)));
    CHECK_EQ_INT(arena.used, 0);
    Mem_Free(bytes);
    Arena_Free(&arena);
}

TEST(offsets_are_relative_to_entry_position_and_slice_origin)
{
    const DATA_BUILDER music_data = M_Music();
    const DATA_BUILDER effect_data = M_Effects();
    DATA_BUILDER data = {};
    memcpy(data.bytes + 7, music_data.bytes, music_data.size);
    memcpy(
        data.bytes + 7 + music_data.size, effect_data.bytes, effect_data.size);
    data.size = 7 + music_data.size + effect_data.size;
    ARENA arena;
    Arena_Init(&arena, "origins", 65536);
    READER parent;
    Reader_OpenBuffer(&parent, data.bytes, data.size, nullptr);
    Reader_Skip(&parent, 3);
    READER slice = Reader_Slice(&parent, 4 + music_data.size);
    Reader_Skip(&slice, 4);
    MDRV_MUSIC music;
    CHECK(Result_Log(
        MdrvData_LoadMusic(&slice, Arena_Allocator(&arena), &music),
        LOG_LEVEL_ERROR));
    CHECK_EQ_INT(Reader_Remaining(&slice), 0);
    CHECK_EQ_INT(music.tracks[0].offset, 56);
    CHECK_EQ_STR(music.title, "東A");
    MDRV_EFFECTS effects;
    CHECK(Result_Log(
        MdrvData_LoadEffects(&parent, Arena_Allocator(&arena), &effects),
        LOG_LEVEL_ERROR));
    CHECK_EQ_INT(Reader_Tell(&parent), data.size);
    CHECK_EQ_INT(effects.effects[0].stream_offsets[0], 28);
    Arena_Free(&arena);
}

TEST(owns_data_after_owned_reader_and_source_storage_are_released)
{
    DATA_BUILDER data = M_Music();
    ARENA arena;
    Arena_Init(&arena, "asset lifetime", 65536);
    READER reader;
    Reader_OpenOwnedBuffer(
        &reader, Mem_Dup(data.bytes, data.size), data.size, &g_HeapAllocator,
        nullptr);
    MDRV_MUSIC music;
    CHECK(Result_Log(
        MdrvData_LoadMusic(&reader, Arena_Allocator(&arena), &music),
        LOG_LEVEL_ERROR));
    Reader_Close(&reader);
    memset(data.bytes, 0, sizeof(data.bytes));
    CHECK_EQ_STR(music.title, "東A");
    CHECK_EQ_INT(music.bytes[music.fm_offset], 64 ^ 0x81);
    CHECK_EQ_INT(music.tracks[0].offset, 56);
    data = M_Effects();
    Reader_OpenOwnedBuffer(
        &reader, Mem_Dup(data.bytes, data.size), data.size, &g_HeapAllocator,
        nullptr);
    MDRV_EFFECTS effects;
    CHECK(Result_Log(
        MdrvData_LoadEffects(&reader, Arena_Allocator(&arena), &effects),
        LOG_LEVEL_ERROR));
    Reader_Close(&reader);
    memset(data.bytes, 0, sizeof(data.bytes));
    CHECK_EQ_INT(effects.bytes[effects.ssg_offset], 38 * 3);
    CHECK_EQ_INT(effects.effects[1].stream_offsets[0], 34);
    Arena_Reset(&arena);
    CHECK_EQ_INT(arena.used, 0);
    Arena_Free(&arena);
}

TEST(file_slices_load_both_formats_and_keep_offsets_record_relative)
{
    const DATA_BUILDER music_data = M_Music();
    const DATA_BUILDER effect_data = M_Effects();
    DATA_BUILDER data = {};
    memcpy(data.bytes + 7, music_data.bytes, music_data.size);
    memcpy(
        data.bytes + 7 + music_data.size, effect_data.bytes, effect_data.size);
    data.size = 7 + music_data.size + effect_data.size;
    char *const directory = TestDir_Prepare("file_slices");
    char *const path = TestDir_Join(directory, "synthetic.dat");
    TestDir_Write(path, data.bytes, data.size);
    READER file;
    CHECK(Result_Log(Reader_OpenFile(&file, path), LOG_LEVEL_ERROR));
    Reader_Skip(&file, 7);
    READER slice = Reader_Slice(&file, music_data.size);
    ARENA arena;
    Arena_Init(&arena, "file data", 65536);
    MDRV_MUSIC music;
    MDRV_EFFECTS effects;
    CHECK(Result_Log(
        MdrvData_LoadMusic(&slice, Arena_Allocator(&arena), &music),
        LOG_LEVEL_ERROR));
    CHECK(Result_Log(
        MdrvData_LoadEffects(&file, Arena_Allocator(&arena), &effects),
        LOG_LEVEL_ERROR));
    Reader_Close(&file);
    CHECK_EQ_STR(music.title, "東A");
    CHECK_EQ_INT(effects.effects[0].stream_offsets[1], 30);
    Arena_Free(&arena);
    Mem_Free(path);
    Mem_Free(directory);
}

TEST(rejects_oversized_and_already_failed_readers_without_allocation)
{
    uint8_t *const bytes = Mem_AllocZ(65537);
    ARENA arena;
    Arena_Init(&arena, "reader failures", 65536);
    for (int32_t mode = 0; mode < 2; mode++) {
        READER reader;
        Reader_OpenBuffer(&reader, bytes, mode == 0 ? 65537 : 16, nullptr);
        if (mode == 1) {
            Reader_Seek(&reader, 17);
        }
        MDRV_MUSIC music;
        CHECK(!Result_Ignore(
            MdrvData_LoadMusic(&reader, Arena_Allocator(&arena), &music)));
        CHECK(music.bytes == nullptr);
        MDRV_EFFECTS effects;
        CHECK(!Result_Ignore(
            MdrvData_LoadEffects(&reader, Arena_Allocator(&arena), &effects)));
        CHECK(effects.bytes == nullptr);
        CHECK_EQ_INT(arena.used, 0);
    }
    Mem_Free(bytes);
    Arena_Free(&arena);
}

#ifndef _WIN32
TEST(late_file_errors_publish_nothing_and_do_not_use_the_asset_arena)
{
    char *const directory = TestDir_Prepare("late_errors");
    char *const path = TestDir_Join(directory, "synthetic.dat");
    ARENA arena;
    Arena_Init(&arena, "file errors", 65536);
    for (int32_t mode = 0; mode < 2; mode++) {
        const DATA_BUILDER data = mode == 0 ? M_Music() : M_Effects();
        const size_t file_size = READER_BUFFER_SIZE + data.size;
        uint8_t *const file_bytes = Mem_AllocZ(file_size);
        memcpy(file_bytes + READER_BUFFER_SIZE, data.bytes, data.size);
        TestDir_Write(path, file_bytes, file_size);
        READER reader;
        CHECK(Result_Log(Reader_OpenFile(&reader, path), LOG_LEVEL_ERROR));
        Reader_Skip(&reader, READER_BUFFER_SIZE);
        CHECK(SDL_SaveFile(path, file_bytes, file_size - 1));
        Mem_Free(file_bytes);
        if (mode == 0) {
            MDRV_MUSIC music;
            CHECK(!Result_Ignore(
                MdrvData_LoadMusic(&reader, Arena_Allocator(&arena), &music)));
            CHECK(music.bytes == nullptr);
        } else {
            MDRV_EFFECTS effects;
            CHECK(!Result_Ignore(MdrvData_LoadEffects(
                &reader, Arena_Allocator(&arena), &effects)));
            CHECK(effects.bytes == nullptr);
        }
        CHECK(Reader_Failed(&reader));
        CHECK_EQ_INT(arena.used, 0);
        Reader_Close(&reader);
    }
    Arena_Free(&arena);
    Mem_Free(path);
    Mem_Free(directory);
}
#endif
