#include "tests/test.h"
#include "th02/music/wait.h"

#include "core/base/arena.h"
#include "core/io/reader.h"
#include "pc98/pmd.h"

#include <string.h>

typedef struct {
    ARENA arena;
    PMD *pmd;
    MUSIC_WAIT wait;
} FIXTURE;

static void M_Init(FIXTURE *const fixture)
{
    memset(fixture, 0, sizeof(*fixture));
    Arena_Init(&fixture->arena, "Scene music wait", 4 * 1024 * 1024);
    const ALLOCATOR *const allocator = Arena_Allocator(&fixture->arena);
    uint8_t song[67] = {};
    for (size_t i = 0; i < 11; i++) {
        song[1 + i * 2] = i == 0 ? 26 : 40;
    }
    song[41] = 0x80;
    song[23] = 50;
    song[25] = 62;
    const uint8_t stream[] = { 0xdf, 1, 0xf6, 0x0f, 1, 0x80 };
    memcpy(song + 27, stream, sizeof(stream));
    song[59] = 64;
    song[61] = 0x48;
    song[62] = 0xfe;
    song[64] = 0xff;
    READER reader;
    Reader_OpenBuffer(&reader, song, sizeof(song), nullptr);
    PMD_SONG *music;
    CHECK(Result_Ignore(PmdData_LoadSong(&reader, allocator, &music)));
    Reader_Close(&reader);
    uint8_t effects[259] = {};
    for (size_t i = 0; i < 127; i++) {
        effects[i * 2 + 1] = 1;
    }
    effects[254] = 1;
    effects[255] = 1;
    effects[256] = 0x80;
    effects[258] = 0xff;
    Reader_OpenBuffer(&reader, effects, sizeof(effects), nullptr);
    PMD_EFFECTS *bank;
    CHECK(Result_Ignore(PmdData_LoadEffects(&reader, allocator, &bank)));
    Reader_Close(&reader);
    CHECK(Result_Ignore(
        Pmd_Init(bank, PMD_BOARD_86, 200, allocator, &fixture->pmd)));
    Pmd_Load(fixture->pmd, music);
    MusicWait_Init(&fixture->wait);
}

static void M_Advance(FIXTURE *const fixture, const uint32_t frames)
{
    MusicWait_Advance(&fixture->wait, frames);
    Pmd_Render(fixture->pmd, nullptr, frames);
}

TEST(scene_wait_matches_driver_phase_across_start_counter_wrap_and_fades)
{
    const uint32_t phases[] = { 0, 13, 883 };
    const uint8_t counts[] = { 0, 1, 6, 7, 8, 9, 247, 248, 254, 255 };
    const struct {
        uint32_t frames;
        uint16_t ready_measure;
        uint16_t stop_measure;
    } boundaries[][10] = {
        { { 120517, 155, 164 },
          { 48739, 64, 64 },
          { 115201, 155, 164 },
          { 43422, 64, 64 },
          { 113428, 155, 164 },
          { 48739, 73, 73 },
          { 114314, 429, 438 },
          { 42536, 338, 338 },
          { 115201, 438, 448 },
          { 43422, 347, 347 } },
        { { 120504, 155, 164 },
          { 48726, 64, 64 },
          { 115188, 155, 164 },
          { 43409, 64, 64 },
          { 120505, 164, 173 },
          { 48726, 73, 73 },
          { 114301, 429, 438 },
          { 49612, 347, 347 },
          { 115188, 438, 448 },
          { 43409, 347, 347 } },
        { { 119634, 154, 163 },
          { 47856, 63, 63 },
          { 114318, 154, 163 },
          { 42539, 63, 63 },
          { 119635, 163, 172 },
          { 47856, 72, 72 },
          { 113431, 428, 437 },
          { 48742, 346, 346 },
          { 114318, 437, 447 },
          { 42539, 346, 346 } },
    };
    for (size_t phase = 0; phase < ARRAY_SIZE(phases); phase++) {
        for (size_t count = 0; count < ARRAY_SIZE(counts); count++) {
            FIXTURE fixture;
            M_Init(&fixture);
            M_Advance(&fixture, phases[phase]);
            MusicWait_Start(&fixture.wait);
            Pmd_Start(fixture.pmd);
            M_Advance(
                &fixture, (uint32_t)((uint64_t)counts[count] * 737280 / 832));
            const uint8_t speed = count % 2 == 0 ? 15 : 40;
            MusicWait_FadeOut(&fixture.wait, speed);
            Pmd_Fade(fixture.pmd, (int8_t)speed);
            const uint32_t blocks[] = { 1, 0, 137, 800, 2047 };
            uint32_t remaining = boundaries[phase][count].frames - 1;
            unsigned index = 0;
            while (remaining != 0) {
                const uint32_t block = blocks[index++ % ARRAY_SIZE(blocks)];
                const uint32_t frames = block < remaining ? block : remaining;
                M_Advance(&fixture, frames);
                remaining -= frames;
                CHECK(!MusicWait_Ready(&fixture.wait));
            }
            M_Advance(&fixture, 1);
            CHECK(MusicWait_Ready(&fixture.wait));
            CHECK_EQ_INT(
                Pmd_Measure(fixture.pmd),
                boundaries[phase][count].ready_measure);
            M_Advance(&fixture, 16000);
            CHECK_EQ_INT(
                Pmd_Measure(fixture.pmd),
                boundaries[phase][count].stop_measure);
            M_Advance(&fixture, 10000);
            CHECK_EQ_INT(
                Pmd_Measure(fixture.pmd),
                boundaries[phase][count].stop_measure);
            Arena_Free(&fixture.arena);
        }
    }
}

TEST(scene_wait_releases_at_equality_before_driver_stop_and_preserves_phase)
{
    FIXTURE fixture;
    M_Init(&fixture);
    MusicWait_Start(&fixture.wait);
    Pmd_Start(fixture.pmd);
    MusicWait_FadeOut(&fixture.wait, 15);
    Pmd_Fade(fixture.pmd, 15);
    const uint32_t equality = (uint32_t)((737280ull * 136 + 831) / 832);
    M_Advance(&fixture, equality - 1);
    CHECK(!MusicWait_Ready(&fixture.wait));
    M_Advance(&fixture, 1);
    CHECK(MusicWait_Ready(&fixture.wait));
    CHECK_EQ_INT(fixture.wait.speed, 15);
    CHECK_EQ_INT(Pmd_Measure(fixture.pmd), 155);
    M_Advance(&fixture, 776);
    CHECK_EQ_INT(Pmd_Measure(fixture.pmd), 156);
    const uint32_t phase = fixture.wait.phase;
    MusicWait_Start(&fixture.wait);
    Pmd_Start(fixture.pmd);
    CHECK_EQ_INT(fixture.wait.phase, phase);
    CHECK(!MusicWait_Ready(&fixture.wait));
    M_Advance(&fixture, 17);
    MusicWait_Stop(&fixture.wait);
    Pmd_Stop(fixture.pmd);
    CHECK(MusicWait_Ready(&fixture.wait));
    M_Advance(&fixture, 800);
    CHECK_EQ_INT(Pmd_Measure(fixture.pmd), 0);
    Arena_Free(&fixture.arena);
}
