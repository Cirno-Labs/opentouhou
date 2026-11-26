#include "pc98/pmd/data.h"
#include "pc98/pmd.h"

#include "core/base/arena.h"
#include "th02/music/assets.h"

#include "test.h"
#include "test_allocator.h"
#include "test_dir.h"

#define M_SIZE 161

static void M_Word(uint8_t *const bytes, const uint16_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
}

static PMD *M_Driver(const ALLOCATOR *const allocator)
{
    uint8_t effects[259] = {};
    for (size_t i = 0; i < 127; i++) {
        M_Word(effects + i * 2, 256);
    }
    M_Word(effects + 254, 257);
    effects[256] = 0x80;
    M_Word(effects + 257, 0xff00);
    READER reader;
    Reader_OpenBuffer(&reader, effects, sizeof(effects), nullptr);
    PMD_EFFECTS *bank;
    CHECK(Result_Ignore(PmdData_LoadEffects(&reader, allocator, &bank)));
    Reader_Close(&reader);
    PMD *pmd;
    CHECK(Result_Ignore(Pmd_Init(bank, PMD_BOARD_86, 200, allocator, &pmd)));
    return pmd;
}

static void M_Song(uint8_t bytes[M_SIZE])
{
    memset(bytes, 0, M_SIZE);
    for (size_t i = 0; i < PMD_TRACK_COUNT; i++) {
        M_Word(bytes + 1 + i * 2, i == 1 ? 31 : i == 2 ? 27 : 26);
    }
    M_Word(bytes + 23, 32);
    M_Word(bytes + 25, 50);
    memset(bytes + 27, 0x80, 6);
    M_Word(bytes + 33, 36);
    M_Word(bytes + 35, 37);
    bytes[37] = bytes[38] = 0xff;
    bytes[39] = 120;
    bytes[43] = 60;
    M_Word(bytes + 47, 144);
    M_Word(bytes + 49, 0xfe48);
    for (size_t i = 0; i < 3; i++) {
        bytes[51 + i * 26] = i == 1 ? 255 : 7;
        for (size_t j = 0; j < PMD_INSTRUMENT_SIZE; j++) {
            bytes[52 + i * 26 + j] = (uint8_t)(i * 31 + j + 1);
        }
    }
    bytes[130] = 0xff;
    bytes[132] = 0x82;
    bytes[133] = 0xa0;
    memcpy(bytes + 135, "test", 5);
    memcpy(bytes + 140, "memo", 5);
    const uint16_t offsets[] = { 130, 130, 130, 131, 134, 134, 139, 0 };
    for (size_t i = 0; i < ARRAY_SIZE(offsets); i++) {
        M_Word(bytes + 145 + i * 2, offsets[i]);
    }
}

static void M_Reject(const uint8_t *const bytes, const size_t size)
{
    TEST_ALLOCATOR allocator;
    TestAllocator_Init(&allocator);
    allocator.allocations_left = 0;
    PMD_SONG previous;
    PMD_SONG *music = &previous;
    READER reader;
    Reader_OpenBuffer(&reader, bytes, size, nullptr);
    CHECK(!Result_Ignore(
        PmdData_LoadSong(&reader, &allocator.allocator, &music)));
    CHECK(music == nullptr);
    CHECK_EQ_INT(allocator.live_blocks, 0);
    Reader_Close(&reader);
}

TEST(owns_sliced_record_shared_starts_and_voices)
{
    uint8_t bytes[M_SIZE + 13];
    memset(bytes, 0x55, sizeof(bytes));
    M_Song(bytes + 13);
    TEST_ALLOCATOR allocator;
    TestAllocator_Init(&allocator);
    READER reader;
    Reader_OpenBuffer(&reader, bytes, sizeof(bytes), nullptr);
    Reader_Seek(&reader, 13);
    PMD_SONG *music;
    CHECK(Result_Log(
        PmdData_LoadSong(&reader, &allocator.allocator, &music),
        LOG_LEVEL_ERROR));
    CHECK_EQ_INT(Reader_Tell(&reader), sizeof(bytes));
    Reader_Close(&reader);
    CHECK_EQ_INT(allocator.live_blocks, 1);
    CHECK_EQ_INT(allocator.live_bytes, sizeof(*music) + music->size);
    CHECK_EQ_INT(music->size, M_SIZE);
    CHECK(memcmp(music->bytes, bytes + 13, M_SIZE) == 0);
    memset(bytes, 0x77, sizeof(bytes));
    CHECK_EQ_INT(music->command_end, 38);
    CHECK_EQ_INT(music->rhythm_count, 2);
    CHECK_EQ_INT(music->instrument_count, 3);
    ARENA playback;
    Arena_Init(&playback, "loaded song playback", 4 * 1024 * 1024);
    PMD *const pmd = M_Driver(Arena_Allocator(&playback));
    Pmd_Load(pmd, music);
    Pmd_Start(pmd);
    OPNA_FRAME frames[800];
    Pmd_Render(pmd, frames, ARRAY_SIZE(frames));
    for (size_t i = 0; i < ARRAY_SIZE(frames); i++) {
        CHECK_EQ_INT(frames[i].fm_rhythm[0], 0);
        CHECK_EQ_INT(frames[i].fm_rhythm[1], 0);
        for (size_t channel = 0; channel < 3; channel++) {
            CHECK_EQ_INT(frames[i].ssg[channel], 0);
        }
    }
    Pmd_Stop(pmd);
    Arena_Free(&playback);
    CHECK(PmdData_SongInstrument(music, 7) == music->bytes + 52);
    CHECK(PmdData_SongInstrument(music, 255) == music->bytes + 78);
    CHECK(PmdData_SongInstrument(music, 0) == nullptr);
    Allocator_Free(&allocator.allocator, music, sizeof(*music) + music->size);
    CHECK_EQ_INT(allocator.live_blocks, 0);
    CHECK_EQ_INT(allocator.size_errors, 0);
}

TEST(rejects_truncated_and_malformed_profiles_without_retained_allocations)
{
    uint8_t bytes[M_SIZE];
    M_Song(bytes);
    for (size_t size = 0; size < M_SIZE; size++) {
        M_Reject(bytes, size);
    }
    static const struct {
        uint16_t position;
        uint16_t value;
    } invalid[] = {
        { 0, 1 },        { 1, 24 },       { 3, 25 },     { 5, 38 },
        { 7, 65535 },    { 23, 25 },      { 23, 65535 }, { 25, 30 },
        { 25, 65535 },   { 33, 35 },      { 33, 65535 }, { 35, 35 },
        { 35, 38 },      { 47, 50 },      { 47, 65535 }, { 49, 0xfe47 },
        { 129, 0xffff }, { 145, 129 },    { 145, 144 },  { 145, 65535 },
        { 151, 65535 },  { 140, 0x0082 }, { 159, 130 },
    };
    for (size_t i = 0; i < ARRAY_SIZE(invalid); i++) {
        M_Song(bytes);
        M_Word(bytes + invalid[i].position, invalid[i].value);
        M_Reject(bytes, sizeof(bytes));
    }
    uint8_t *const huge = Mem_Alloc(65537);
    M_Reject(huge, 65537);
    Mem_Free(huge);
}

TEST(accepts_absent_rhythm_and_bounds_memo_counts)
{
    uint8_t bytes[145 + (PMD_MEMO_LIMIT + 2) * 2];
    M_Song(bytes);
    M_Word(bytes + 23, 38);
    for (size_t i = 0; i < PMD_MEMO_LIMIT; i++) {
        M_Word(bytes + 145 + i * 2, 130);
    }
    M_Word(bytes + 145 + PMD_MEMO_LIMIT * 2, 0);
    READER reader;
    Reader_OpenBuffer(&reader, bytes, sizeof(bytes) - 2, nullptr);
    PMD_SONG *music;
    CHECK(Result_Log(
        PmdData_LoadSong(&reader, &g_HeapAllocator, &music), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(music->rhythm_count, 0);
    Allocator_Free(&g_HeapAllocator, music, sizeof(*music) + music->size);
    Reader_Close(&reader);
    M_Word(bytes + 145 + PMD_MEMO_LIMIT * 2, 130);
    M_Word(bytes + 145 + (PMD_MEMO_LIMIT + 1) * 2, 0);
    M_Reject(bytes, sizeof(bytes));
}

TEST(whole_music_cache_survives_namespace_closure_and_failed_load)
{
    char *const directory = TestDir_Prepare("music-assets");
    uint8_t bytes[M_SIZE];
    M_Song(bytes);
    char *path = TestDir_Join(directory, MusicAssets_Path(MUSIC_STAGE0));
    TestDir_Write(path, bytes, sizeof(bytes));
    Mem_Free(path);
    VFS *files = Vfs_Create(&g_HeapAllocator);
    CHECK(Result_Ignore(Vfs_MountDir(files, "", directory, 0, true)));
    ARENA arena;
    Arena_Init(&arena, "song assets", 4 * 1024 * 1024);
    MUSIC_ASSETS assets;
    CHECK(!Result_Ignore(
        MusicAssets_Load(files, Arena_Allocator(&arena), &assets)));
    const MUSIC_ASSETS empty = {};
    CHECK(memcmp(&assets, &empty, sizeof(assets)) == 0);
    Arena_Reset(&arena);
    for (int i = 1; i < MUSIC_SONG_COUNT; i++) {
        path = TestDir_Join(directory, MusicAssets_Path((MUSIC_ID)i));
        TestDir_Write(path, bytes, sizeof(bytes));
        Mem_Free(path);
    }
    CHECK(Result_Log(
        MusicAssets_Load(files, Arena_Allocator(&arena), &assets),
        LOG_LEVEL_ERROR));
    Vfs_Destroy(files);
    PMD *const pmd = M_Driver(Arena_Allocator(&arena));
    for (size_t i = 0; i < MUSIC_SONG_COUNT; i++) {
        CHECK_EQ_INT(assets.songs[i]->instrument_count, 3);
        CHECK_EQ_INT(PmdData_SongInstrument(assets.songs[i], 7)[0], 1);
        Pmd_Load(pmd, assets.songs[i]);
        Pmd_Start(pmd);
        Pmd_Render(pmd, nullptr, 800);
        CHECK_EQ_INT(Pmd_Measure(pmd), 0);
        Pmd_Stop(pmd);
    }
    Arena_Free(&arena);
    CHECK(MusicAssets_Path((MUSIC_ID)-1) == nullptr);
    CHECK(MusicAssets_Path(MUSIC_SONG_COUNT) == nullptr);
    Mem_Free(directory);
}
