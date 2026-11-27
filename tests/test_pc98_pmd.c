#include "pc98/pmd.h"

#include "core/base/arena.h"
#include "core/base/log.h"
#include "test.h"
#include "test_allocator.h"

#include <setjmp.h>

typedef struct {
    ARENA arena;
    PMD *pmd;
    PMD_SONG *song;
} FIXTURE;

typedef struct {
    const uint8_t *fm;
    const uint8_t *ssg;
    size_t fm_size;
    size_t ssg_size;
    uint8_t fm_part;
    uint8_t ssg_part;
    uint16_t drums;
} SCORE;

static jmp_buf m_FatalJump;
static const char *m_ExpectedFatal;

static void M_Fatal(
    const char *const message, const char *const file, const int line)
{
    CHECK(file != nullptr && line > 0);
    CHECK(strstr(message, m_ExpectedFatal) != nullptr);
    longjmp(m_FatalJump, 1);
}

static uint64_t M_PcmHash(const OPNA_FRAME *const frames, const size_t count)
{
    uint64_t hash = UINT64_C(14695981039346656037);
    for (size_t i = 0; i < count; i++) {
        for (size_t channel = 0; channel < 2; channel++) {
            hash = (hash ^ (uint32_t)frames[i].fm_rhythm[channel])
                * UINT64_C(1099511628211);
        }
        for (size_t channel = 0; channel < 3; channel++) {
            hash = (hash ^ (uint32_t)frames[i].ssg[channel])
                * UINT64_C(1099511628211);
        }
    }
    return hash;
}

static void M_Word(uint8_t *const bytes, const uint16_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
}

static void M_InitEffectStreamBoard(
    FIXTURE *const fixture, const uint8_t *const effect, const size_t size,
    const uint8_t initial_timer_b, const PMD_BOARD board)
{
    memset(fixture, 0, sizeof(*fixture));
    Arena_Init(&fixture->arena, "PMD test", 4 * 1024 * 1024);
    const ALLOCATOR *const allocator = Arena_Allocator(&fixture->arena);
    uint8_t song[129] = {};
    for (size_t i = 0; i < 11; i++) {
        M_Word(song + 1 + i * 2, i == 6 ? 32 : i == 10 ? 64 : 26);
    }
    M_Word(song + 23, 96);
    M_Word(song + 25, 124);
    song[27] = 0x80;
    const uint8_t melody[] = { 0xfd, 15, 0x40, 255, 0x80 };
    memcpy(song + 33, melody, sizeof(melody));
    song[65] = 0;
    song[66] = 0x80;
    M_Word(song + 97, 98);
    const uint8_t rhythm[] = { 0x80, 1, 8, 0xff };
    memcpy(song + 99, rhythm, sizeof(rhythm));
    M_Word(song + 121, 126);
    M_Word(song + 123, 0xfe48);
    M_Word(song + 125, 0xff00);
    READER reader;
    Reader_OpenBuffer(&reader, song, sizeof(song), nullptr);
    CHECK(Result_Ignore(PmdData_LoadSong(&reader, allocator, &fixture->song)));
    Reader_Close(&reader);
    uint8_t effects[512] = {};
    CHECK(size <= sizeof(effects) - 284);
    for (size_t i = 0; i < 127; i++) {
        M_Word(effects + i * 2, 256);
    }
    memcpy(effects + 256, effect, size);
    const uint16_t instrument = (uint16_t)(256 + size);
    M_Word(effects + 254, instrument);
    memset(effects + instrument + 1, 1, 4);
    memset(effects + instrument + 9, 31, 4);
    memset(effects + instrument + 21, 15, 4);
    effects[instrument + 25] = 7;
    M_Word(effects + instrument + 26, 0xff00);
    Reader_OpenBuffer(&reader, effects, instrument + 28, nullptr);
    PMD_EFFECTS *bank;
    CHECK(Result_Ignore(PmdData_LoadEffects(&reader, allocator, &bank)));
    Reader_Close(&reader);
    CHECK(Result_Ignore(
        Pmd_Init(bank, board, initial_timer_b, allocator, &fixture->pmd)));
    Pmd_Load(fixture->pmd, fixture->song);
    Pmd_Start(fixture->pmd);
}

static void M_InitEffectStream(
    FIXTURE *fixture, const uint8_t *effect, size_t size, uint8_t timer)
{
    M_InitEffectStreamBoard(fixture, effect, size, timer, PMD_BOARD_26);
}

static void M_InitEffectProgram(
    FIXTURE *const fixture, const uint8_t duration, const uint8_t program)
{
    const uint8_t effect[] = { 0xff, program, 0x43, duration, 0x80 };
    M_InitEffectStream(fixture, effect, sizeof(effect), 200);
}

static void M_InitEffect(FIXTURE *const fixture, const uint8_t duration)
{
    M_InitEffectProgram(fixture, duration, 0);
}

static void M_Init(FIXTURE *const fixture)
{
    M_InitEffect(fixture, 255);
}

static void M_ScoreTracks(
    FIXTURE *const fixture, const SCORE *const score, const bool duplicate_fm)
{
    uint8_t song[443] = {};
    const uint16_t fm_offset = duplicate_fm ? 96 : 32;
    for (size_t i = 0; i < 11; i++) {
        const uint16_t offset = i == 0                     ? 26
            : i == score->fm_part && score->fm_size != 0   ? fm_offset
            : i == score->ssg_part && score->ssg_size != 0 ? 192
            : i == 10 && score->drums                      ? 320
                                                           : 176;
        M_Word(song + 1 + i * 2, offset);
    }
    song[27] = 0x80;
    song[177] = 0x80;
    CHECK(score->fm_size <= 150 && score->ssg_size < 128);
    if (score->fm_size != 0) {
        memcpy(
            song + (score->fm_part == 0 ? 27 : fm_offset + 1), score->fm,
            score->fm_size);
        if (duplicate_fm) {
            CHECK(score->fm_part == 2 && score->fm_size <= 68);
            memcpy(song + 27, score->fm, score->fm_size);
        }
    }
    if (score->ssg_size != 0) {
        memcpy(song + 193, score->ssg, score->ssg_size);
    }
    M_Word(song + 23, score->drums ? 352 : 400);
    M_Word(song + 25, 412);
    song[321] = 0;
    song[322] = 0x80;
    M_Word(song + 353, 354);
    const uint8_t rhythm[] = {
        (uint8_t)(0x80 | (score->drums >> 8)),
        (uint8_t)score->drums,
        4,
        0xff,
    };
    memcpy(song + 355, rhythm, sizeof(rhythm));
    M_Word(song + 409, 440);
    M_Word(song + 411, 0xfe48);
    song[413] = 7;
    memset(song + 414, 1, 4);
    memset(song + 422, 31, 4);
    memset(song + 434, 15, 4);
    song[438] = 7;
    M_Word(song + 439, 0xff00);
    READER reader;
    Reader_OpenBuffer(&reader, song, sizeof(song), nullptr);
    CHECK(Result_Ignore(PmdData_LoadSong(
        &reader, Arena_Allocator(&fixture->arena), &fixture->song)));
    Reader_Close(&reader);
    Pmd_Load(fixture->pmd, fixture->song);
    Pmd_Start(fixture->pmd);
}

static void M_Score(FIXTURE *const fixture, const SCORE *const score)
{
    M_ScoreTracks(fixture, score, false);
}

TEST(public_driver_keeps_music_effects_and_drums_identical_across_blocks)
{
    FIXTURE whole;
    FIXTURE split;
    M_Init(&whole);
    M_Init(&split);
    Pmd_Effect(whole.pmd, 0);
    Pmd_Effect(split.pmd, 0);
    OPNA_FRAME a[8192];
    OPNA_FRAME b[8192];
    Pmd_Render(whole.pmd, a, ARRAY_SIZE(a));
    const size_t blocks[] = { 1, 17, 6203, 0, 1971 };
    size_t position = 0;
    for (size_t i = 0; i < ARRAY_SIZE(blocks); i++) {
        Pmd_Render(split.pmd, b + position, blocks[i]);
        position += blocks[i];
    }
    CHECK_EQ_INT(position, ARRAY_SIZE(a));
    CHECK(memcmp(a, b, sizeof(a)) == 0);
    bool fm = false;
    bool melody = false;
    bool drums = false;
    for (size_t i = 0; i < ARRAY_SIZE(a); i++) {
        fm |= a[i].fm_rhythm[0] != 0 || a[i].fm_rhythm[1] != 0;
        melody |= a[i].ssg[0] != 0;
        drums |= a[i].ssg[2] != 0;
    }
    CHECK(fm && melody && drums);
    Pmd_Stop(whole.pmd);
    Pmd_Stop(split.pmd);
    Arena_Free(&whole.arena);
    Arena_Free(&split.arena);
}

TEST(measure_advances_on_timer_b_and_survives_stop_until_restart)
{
    FIXTURE fixture;
    M_Init(&fixture);
    CHECK_EQ_INT(Pmd_Measure(fixture.pmd), 0);
    Pmd_Render(fixture.pmd, nullptr, 74436);
    CHECK_EQ_INT(Pmd_Measure(fixture.pmd), 0);
    Pmd_Render(fixture.pmd, nullptr, 1);
    CHECK_EQ_INT(Pmd_Measure(fixture.pmd), 1);
    Pmd_Render(fixture.pmd, nullptr, 0);
    CHECK_EQ_INT(Pmd_Measure(fixture.pmd), 1);
    Pmd_Stop(fixture.pmd);
    Pmd_Render(fixture.pmd, nullptr, 1600);
    CHECK_EQ_INT(Pmd_Measure(fixture.pmd), 1);
    Pmd_Load(fixture.pmd, fixture.song);
    CHECK_EQ_INT(Pmd_Measure(fixture.pmd), 1);
    Pmd_Start(fixture.pmd);
    CHECK_EQ_INT(Pmd_Measure(fixture.pmd), 0);
    Pmd_Stop(fixture.pmd);
    Arena_Free(&fixture.arena);
}

TEST(missing_effects_fail_before_allocating_a_driver)
{
    TEST_ALLOCATOR allocator;
    TestAllocator_Init(&allocator);
    allocator.allocations_left = 0;
    PMD *pmd;
    CHECK(!Result_Ignore(
        Pmd_Init(nullptr, PMD_BOARD_26, 200, &allocator.allocator, &pmd)));
    CHECK(pmd == nullptr);
    CHECK_EQ_INT(allocator.live_blocks, 0);
}

TEST(public_pcm_covers_commands_modulation_and_voice_arbitration)
{
    const uint8_t fm_notes[] = {
        0xff, 7,    0xfd, 100,  0xfe, 2,    0xc4, 32,   0xb3,
        1,    0xf5, 1,    0xf6, 0x40, 4,    0xfb, 0x41, 4,
        0xc1, 0xda, 0x42, 0x45, 6,    0xc1, 0x0f, 2,    0x80,
    };
    const uint8_t fm_lfos[] = {
        0xff, 7,    0xfd, 100,  0xf2, 0, 1,    2,    4,    0xcb, 3,
        0xf1, 3,    0xbf, 0,    1,    1, 4,    0xbc, 2,    0xbe, 3,
        0xca, 1,    0xbb, 1,    0xd6, 3, 1,    0xbd, 4,    255,  0xb7,
        4,    0xb7, 0x83, 0xf6, 0x40, 4, 0x0f, 2,    0x0c, 4,    0x80,
    };
    const uint8_t ssg_normal[] = {
        0xfd, 12,   0xf0, 2, 0,    3, 2,    0xee, 5,    0xed,
        8,    0xf6, 0x40, 8, 0x0f, 2, 0x44, 5,    0x80,
    };
    const uint8_t ssg_extended[] = {
        0xfd, 15, 0xcd, 4, 3,    2,    0x76, 8,    0xcc, 1,    0xc9, 1,
        0xf2, 0,  1,    1, 3,    0xcb, 2,    0xf1, 3,    0xca, 1,    0xf6,
        0x40, 4,  0x43, 4, 0xfb, 0x0c, 4,    0x0f, 2,    0x80,
    };
    const uint8_t ssg_glide[] = {
        0xfd, 15,   0xf0, 0, 0,    2,    1,    0xfa, 4,    0, 0xf5,
        255,  0xf6, 0x40, 3, 0xda, 0x41, 0x45, 5,    0x0f, 2, 0x80,
    };
    const uint8_t fm_timers[] = {
        0xff, 7,    0xfd, 108, 0xf6, 0xfc, 150,  0xef, 0x2f, 0, 0x40,
        4,    0xef, 0x2d, 0,   0x43, 2,    0xfc, 200,  0x40, 2, 0x80,
    };
    const uint8_t fm_loops[] = {
        0xff, 7, 0xfd, 108, 0xf9, 39, 0, 0x40, 1, 0xf7,
        39,   0, 0xf8, 3,   99,   31, 0, 0x41, 2, 0x80,
    };
    const uint8_t fm_shared[] = {
        0xff, 7, 0xfd, 90, 0xf6, 0x40, 2, 0x43, 2, 0x80,
    };
    const SCORE scores[] = {
        { fm_notes, nullptr, sizeof(fm_notes), 0, 0, 6, false },
        { fm_lfos, nullptr, sizeof(fm_lfos), 0, 0, 6, false },
        { nullptr, ssg_normal, 0, sizeof(ssg_normal), 0, 6, false },
        { nullptr, ssg_extended, 0, sizeof(ssg_extended), 0, 6, false },
        { nullptr, ssg_glide, 0, sizeof(ssg_glide), 0, 6, false },
        { fm_timers, nullptr, sizeof(fm_timers), 0, 0, 6, false },
        { fm_loops, nullptr, sizeof(fm_loops), 0, 0, 6, false },
        { fm_shared, ssg_normal, sizeof(fm_shared), sizeof(ssg_normal), 2, 8,
          true },
    };
    const uint64_t expected[] = {
        UINT64_C(7983417024268878111),  UINT64_C(9815664088493484195),
        UINT64_C(8857060099492598060),  UINT64_C(9284542421745451902),
        UINT64_C(17589447759663902085), UINT64_C(5468235605573665075),
        UINT64_C(950788058768863929),   UINT64_C(4395632791451341851),
    };
    static_assert(ARRAY_SIZE(scores) == ARRAY_SIZE(expected));
    for (size_t score = 0; score < ARRAY_SIZE(scores); score++) {
        FIXTURE fixture;
        M_Init(&fixture);
        M_Score(&fixture, &scores[score]);
        uint64_t hash = UINT64_C(14695981039346656037);
        for (size_t tick = 0; tick < 60; tick++) {
            if (tick == 0 || tick == 5) {
                Pmd_Effect(fixture.pmd, 0);
            } else if (tick == 15) {
                Pmd_Fade(fixture.pmd, 5);
            } else if (tick == 30) {
                Pmd_Start(fixture.pmd);
            } else if (tick == 45) {
                Pmd_Stop(fixture.pmd);
            } else if (tick == 50) {
                Pmd_Fade(fixture.pmd, -3);
            } else if (tick == 55) {
                Pmd_Load(fixture.pmd, fixture.song);
                Pmd_Start(fixture.pmd);
            }
            OPNA_FRAME frames[800];
            Pmd_Render(fixture.pmd, frames, ARRAY_SIZE(frames));
            for (size_t i = 0; i < ARRAY_SIZE(frames); i++) {
                for (size_t channel = 0; channel < 2; channel++) {
                    hash = (hash ^ (uint32_t)frames[i].fm_rhythm[channel])
                        * UINT64_C(1099511628211);
                }
                for (size_t channel = 0; channel < 3; channel++) {
                    hash = (hash ^ (uint32_t)frames[i].ssg[channel])
                        * UINT64_C(1099511628211);
                }
            }
        }
        if (hash != expected[score]) {
            TEST_FAIL(
                "PCM score %zu: expected %llu, got %llu", score,
                (unsigned long long)expected[score], (unsigned long long)hash);
        }
        Pmd_Stop(fixture.pmd);
        Arena_Free(&fixture.arena);
    }
}

TEST(public_measure_preserves_tempo_reload_lag_and_restart_phase)
{
    FIXTURE fixture;
    M_Init(&fixture);
    const uint8_t stream[] = { 0xdf, 1, 0xfc, 150, 0x40, 1, 0x41, 1, 0x80 };
    const SCORE score = {
        .fm = stream,
        .fm_size = sizeof(stream),
        .fm_part = 0,
        .ssg_part = 6,
    };
    M_Score(&fixture, &score);
    Pmd_Render(fixture.pmd, nullptr, 775);
    CHECK_EQ_INT(Pmd_Measure(fixture.pmd), 0);
    Pmd_Render(fixture.pmd, nullptr, 1);
    CHECK_EQ_INT(Pmd_Measure(fixture.pmd), 1);
    Pmd_Render(fixture.pmd, nullptr, 774);
    CHECK_EQ_INT(Pmd_Measure(fixture.pmd), 1);
    Pmd_Render(fixture.pmd, nullptr, 1);
    CHECK_EQ_INT(Pmd_Measure(fixture.pmd), 2);
    Pmd_Render(fixture.pmd, nullptr, 1467);
    CHECK_EQ_INT(Pmd_Measure(fixture.pmd), 2);
    Pmd_Render(fixture.pmd, nullptr, 1);
    CHECK_EQ_INT(Pmd_Measure(fixture.pmd), 3);
    Pmd_Stop(fixture.pmd);
    Pmd_Render(fixture.pmd, nullptr, 17);
    CHECK_EQ_INT(Pmd_Measure(fixture.pmd), 3);
    Pmd_Start(fixture.pmd);
    Pmd_Render(fixture.pmd, nullptr, 1450);
    CHECK_EQ_INT(Pmd_Measure(fixture.pmd), 0);
    Pmd_Render(fixture.pmd, nullptr, 1);
    CHECK_EQ_INT(Pmd_Measure(fixture.pmd), 1);
    Pmd_Render(fixture.pmd, nullptr, 774);
    CHECK_EQ_INT(Pmd_Measure(fixture.pmd), 1);
    Pmd_Render(fixture.pmd, nullptr, 1);
    CHECK_EQ_INT(Pmd_Measure(fixture.pmd), 2);
    Pmd_Stop(fixture.pmd);
    Arena_Free(&fixture.arena);
}

TEST(public_fade_stops_at_the_next_music_tick_after_overflow)
{
    FIXTURE fixture;
    M_Init(&fixture);
    const uint8_t stream[] = { 0xdf, 1, 0xf6, 0x40, 1, 0x80 };
    const SCORE score = {
        .fm = stream,
        .fm_size = sizeof(stream),
        .fm_part = 0,
        .ssg_part = 6,
    };
    M_Score(&fixture, &score);
    Pmd_Fade(fixture.pmd, 127);
    Pmd_Render(fixture.pmd, nullptr, 21710);
    CHECK_EQ_INT(Pmd_Measure(fixture.pmd), 27);
    Pmd_Render(fixture.pmd, nullptr, 1);
    CHECK_EQ_INT(Pmd_Measure(fixture.pmd), 27);
    Pmd_Render(fixture.pmd, nullptr, 4000);
    CHECK_EQ_INT(Pmd_Measure(fixture.pmd), 27);
    Pmd_Start(fixture.pmd);
    CHECK_EQ_INT(Pmd_Measure(fixture.pmd), 0);
    Pmd_Stop(fixture.pmd);
    Arena_Free(&fixture.arena);
}

TEST(public_effect_releases_the_shared_fm_voice_back_to_music)
{
    FIXTURE music;
    FIXTURE silent;
    M_InitEffect(&music, 4);
    M_InitEffect(&silent, 4);
    const uint8_t stream[] = {
        0xff, 7, 0xfd, 90, 0xf6, 0x40, 2, 0x43, 2, 0x80,
    };
    const SCORE with_music = {
        .fm = stream,
        .fm_size = sizeof(stream),
        .fm_part = 2,
        .ssg_part = 6,
    };
    const SCORE without_music = { .fm_part = 2, .ssg_part = 6 };
    M_Score(&music, &with_music);
    M_Score(&silent, &without_music);
    Pmd_Effect(music.pmd, 0);
    Pmd_Effect(silent.pmd, 0);
    OPNA_FRAME a[8000];
    OPNA_FRAME b[8000];
    Pmd_Render(music.pmd, a, 3000);
    Pmd_Render(silent.pmd, b, 3000);
    CHECK(memcmp(a, b, 3000 * sizeof(*a)) == 0);
    Pmd_Render(music.pmd, a, ARRAY_SIZE(a));
    Pmd_Render(silent.pmd, b, ARRAY_SIZE(b));
    CHECK(memcmp(a, b, sizeof(a)) != 0);
    bool audible = false;
    for (size_t i = 6000; i < ARRAY_SIZE(a); i++) {
        audible |= a[i].fm_rhythm[0] != 0 || a[i].fm_rhythm[1] != 0;
    }
    CHECK(audible);
    Pmd_Stop(music.pmd);
    Pmd_Stop(silent.pmd);
    Arena_Free(&music.arena);
    Arena_Free(&silent.arena);
}

TEST(public_percussion_plays_only_the_lowest_selected_effect)
{
    const uint16_t masks[][2] = { { 0x21, 1 },
                                  { 0x2001, 1 },
                                  { 0x2080, 0x80 } };
    for (size_t i = 0; i < ARRAY_SIZE(masks); i++) {
        FIXTURE combined;
        FIXTURE first;
        M_Init(&combined);
        M_Init(&first);
        const SCORE a_score = { .fm_part = 0,
                                .ssg_part = 6,
                                .drums = masks[i][0] };
        const SCORE b_score = { .fm_part = 0,
                                .ssg_part = 6,
                                .drums = masks[i][1] };
        M_Score(&combined, &a_score);
        M_Score(&first, &b_score);
        OPNA_FRAME a[8000];
        OPNA_FRAME b[8000];
        Pmd_Render(combined.pmd, a, ARRAY_SIZE(a));
        Pmd_Render(first.pmd, b, ARRAY_SIZE(b));
        CHECK(memcmp(a, b, sizeof(a)) == 0);
        bool audible = false;
        for (size_t frame = 0; frame < ARRAY_SIZE(a); frame++) {
            audible |= a[frame].ssg[2] != 0;
        }
        CHECK(audible);
        Pmd_Stop(combined.pmd);
        Pmd_Stop(first.pmd);
        Arena_Free(&combined.arena);
        Arena_Free(&first.arena);
    }
}

TEST(public_timer_b_keeps_deadlines_when_starting_between_samples)
{
    FIXTURE fixture;
    M_Init(&fixture);
    const uint8_t stream[] = { 0xdf, 1, 0xfc, 250, 0xf6, 0x40, 1, 0x80 };
    const SCORE score = { .fm = stream, .fm_size = sizeof(stream) };
    M_Score(&fixture, &score);
    Pmd_Stop(fixture.pmd);
    Pmd_Render(fixture.pmd, nullptr, 775);
    CHECK_EQ_INT(Pmd_Measure(fixture.pmd), 0);
    Pmd_Start(fixture.pmd);
    const size_t blocks[] = { 0, 1, 774, 1, 82, 1 };
    const uint16_t measures[] = { 0, 1, 1, 2, 2, 3 };
    for (size_t i = 0; i < ARRAY_SIZE(blocks); i++) {
        Pmd_Render(fixture.pmd, nullptr, blocks[i]);
        CHECK_EQ_INT(Pmd_Measure(fixture.pmd), measures[i]);
    }
    Arena_Free(&fixture.arena);
}

TEST(public_restart_resets_tempo_after_the_loaded_timer_interval)
{
    FIXTURE fixture;
    M_Init(&fixture);
    const uint8_t stream[] = { 0xdf, 1, 0xfc, 250, 0xf6, 0x40, 1, 0x80 };
    const SCORE score = { .fm = stream, .fm_size = sizeof(stream) };
    M_Score(&fixture, &score);
    Pmd_Render(fixture.pmd, nullptr, 1600);
    CHECK_EQ_INT(Pmd_Measure(fixture.pmd), 2);
    Pmd_Stop(fixture.pmd);
    Pmd_Start(fixture.pmd);
    const size_t blocks[] = { 33, 1, 775, 1 };
    const uint16_t measures[] = { 0, 1, 1, 2 };
    for (size_t i = 0; i < ARRAY_SIZE(blocks); i++) {
        Pmd_Render(fixture.pmd, nullptr, blocks[i]);
        CHECK_EQ_INT(Pmd_Measure(fixture.pmd), measures[i]);
    }
    Arena_Free(&fixture.arena);
}

TEST(public_long_playback_keeps_pcm_and_measures_across_arbitrary_blocks)
{
    FIXTURE whole;
    FIXTURE split;
    M_Init(&whole);
    M_Init(&split);
    const uint8_t stream[] = {
        0xdf, 1, 0xff, 7, 0xfd, 100, 0xfc, 220, 0xf6, 0x40, 1, 0x41, 3, 0x80,
    };
    const SCORE score = { .fm = stream, .fm_size = sizeof(stream) };
    M_Score(&whole, &score);
    M_Score(&split, &score);
    const size_t frames = 96000;
    OPNA_FRAME *const a =
        Allocator_Alloc(Arena_Allocator(&whole.arena), frames * sizeof(*a));
    OPNA_FRAME *const b =
        Allocator_Alloc(Arena_Allocator(&split.arena), frames * sizeof(*b));
    Pmd_Render(whole.pmd, a, frames);
    const size_t blocks[] = { 1, 0, 17, 800, 3, 2047 };
    size_t position = 0;
    for (size_t i = 0; position < frames; i++) {
        const size_t requested = blocks[i % ARRAY_SIZE(blocks)];
        const size_t remaining = frames - position;
        const size_t count = requested < remaining ? requested : remaining;
        Pmd_Render(split.pmd, b + position, count);
        position += count;
    }
    CHECK(memcmp(a, b, frames * sizeof(*a)) == 0);
    CHECK_EQ_INT(Pmd_Measure(whole.pmd), Pmd_Measure(split.pmd));
    bool audible = false;
    for (size_t i = 0; i < frames; i++) {
        audible |= a[i].fm_rhythm[0] != 0 || a[i].fm_rhythm[1] != 0;
    }
    CHECK(audible);
    CHECK(Pmd_Measure(whole.pmd) > 96);
    Arena_Free(&whole.arena);
    Arena_Free(&split.arena);
}

TEST(public_stop_and_reload_keep_phase_without_advancing_stopped_music)
{
    FIXTURE fixture;
    M_Init(&fixture);
    const uint8_t stream[] = { 0xdf, 1, 0xf6, 0x40, 1, 0x80 };
    const SCORE score = { .fm = stream, .fm_size = sizeof(stream) };
    M_Score(&fixture, &score);
    Pmd_Render(fixture.pmd, nullptr, 800);
    CHECK_EQ_INT(Pmd_Measure(fixture.pmd), 1);
    Pmd_Stop(fixture.pmd);
    Pmd_Render(fixture.pmd, nullptr, 800);
    CHECK_EQ_INT(Pmd_Measure(fixture.pmd), 1);
    Pmd_Load(fixture.pmd, fixture.song);
    Pmd_Start(fixture.pmd);
    Pmd_Render(fixture.pmd, nullptr, 726);
    CHECK_EQ_INT(Pmd_Measure(fixture.pmd), 0);
    Pmd_Render(fixture.pmd, nullptr, 1);
    CHECK_EQ_INT(Pmd_Measure(fixture.pmd), 1);
    Arena_Free(&fixture.arena);
}

TEST(public_pcm_preserves_coincident_timer_order_and_prescaler_reload)
{
    const uint8_t normal[] = {
        0xdf, 1, 0xff, 7, 0xfd, 100,  0xf2, 0,    1,  2,    4,
        0xf1, 1, 0xca, 1, 0xf6, 0x40, 32,   0x41, 32, 0x80,
    };
    const uint8_t prescaled[] = {
        0xef, 0x2f, 0,    0xdf, 1,    0xff, 7,    0xfd, 100, 0xf2, 0,  1,
        2,    4,    0xf1, 1,    0xca, 1,    0xf6, 0x40, 32,  0x41, 32, 0x80,
    };
    const SCORE scores[] = {
        { .fm = normal, .fm_size = sizeof(normal) },
        { .fm = prescaled, .fm_size = sizeof(prescaled) },
    };
    const size_t boundaries[] = { 6203, 2067 };
    const uint16_t measures[] = { 7, 3 };
    const uint64_t expected[] = {
        UINT64_C(8033118241784468245),
        UINT64_C(13582626584610750677),
    };
    for (size_t score = 0; score < ARRAY_SIZE(scores); score++) {
        FIXTURE fixture;
        M_Init(&fixture);
        M_Score(&fixture, &scores[score]);
        OPNA_FRAME frames[8192];
        const size_t boundary = boundaries[score];
        Pmd_Render(fixture.pmd, frames, boundary);
        CHECK_EQ_INT(Pmd_Measure(fixture.pmd), measures[score]);
        Pmd_Render(fixture.pmd, frames + boundary, 1);
        CHECK_EQ_INT(Pmd_Measure(fixture.pmd), measures[score] + 1);
        Pmd_Render(
            fixture.pmd, frames + boundary + 1,
            ARRAY_SIZE(frames) - boundary - 1);
        uint64_t hash = UINT64_C(14695981039346656037);
        bool audible = false;
        for (size_t i = 0; i < ARRAY_SIZE(frames); i++) {
            for (size_t channel = 0; channel < 2; channel++) {
                const int32_t sample = frames[i].fm_rhythm[channel];
                audible |= sample != 0;
                hash = (hash ^ (uint32_t)sample) * UINT64_C(1099511628211);
            }
            for (size_t channel = 0; channel < 3; channel++) {
                hash = (hash ^ (uint32_t)frames[i].ssg[channel])
                    * UINT64_C(1099511628211);
            }
        }
        CHECK(audible);
        CHECK(hash == expected[score]);
        Arena_Free(&fixture.arena);
    }
}

TEST(public_driver_reports_fatal_stream_voice_and_effect_errors)
{
    const uint8_t empty_repeat[] = { 0xf6, 0x80 };
    const uint8_t missing_voice[] = { 0xff, 99, 0x40, 1, 0x80 };
    const uint8_t voice_before_loop[] = { 0xff, 99, 0xf9, 255, 255 };
    const uint8_t valid[] = { 0xff, 7, 0xf6, 0x40, 1, 0x80 };
    const struct {
        const uint8_t *stream;
        size_t size;
        const char *error;
        uint8_t effect_program;
        uint8_t effect_id;
        bool effect;
    } cases[] = {
        { empty_repeat, sizeof(empty_repeat), "PMD dispatch limit exceeded", 0,
          0, false },
        { missing_voice, sizeof(missing_voice), "Missing PMD instrument 99", 0,
          0, false },
        { voice_before_loop, sizeof(voice_before_loop),
          "Missing PMD instrument 99", 0, 0, false },
        { valid, sizeof(valid), "Missing EFC instrument 99", 99, 0, true },
        { valid, sizeof(valid), "Invalid PMD effect request", 0, 127, true },
    };
    for (size_t i = 0; i < ARRAY_SIZE(cases); i++) {
        FIXTURE fixture;
        M_InitEffectProgram(&fixture, 4, cases[i].effect_program);
        const SCORE score = {
            .fm = cases[i].stream,
            .fm_size = cases[i].size,
            .fm_part = cases[i].effect ? 2 : 0,
        };
        M_Score(&fixture, &score);
        m_ExpectedFatal = cases[i].error;
        Log_SetFatalHandler(M_Fatal);
        if (setjmp(m_FatalJump) == 0) {
            if (cases[i].effect) {
                Pmd_Effect(fixture.pmd, cases[i].effect_id);
            }
            Pmd_Render(fixture.pmd, nullptr, 1000);
            TEST_FAIL("expected fatal: %s", cases[i].error);
        }
        Log_SetFatalHandler(nullptr);
        Arena_Free(&fixture.arena);
    }
}

TEST(public_fm_modulation_keeps_pcm_and_independent_component_buses)
{
    FIXTURE whole;
    FIXTURE split;
    M_Init(&whole);
    M_Init(&split);
    const uint8_t stream[] = {
        0xff, 7, 0xfd, 108, 0xf2, 0, 1,    1, 255,  0xf1, 1,  0xca, 1,  0xbf,
        0,    1, 1,    255, 0xbe, 2, 0xbb, 1, 0xf6, 0x40, 12, 0x44, 12, 0x80,
    };
    const SCORE score = { .fm = stream, .fm_size = sizeof(stream) };
    M_Score(&whole, &score);
    M_Score(&split, &score);
    OPNA_FRAME a[8192];
    OPNA_FRAME b[8192];
    Pmd_Render(whole.pmd, a, ARRAY_SIZE(a));
    const size_t blocks[] = { 1, 775, 800, 4627, 1, 1988 };
    size_t position = 0;
    for (size_t i = 0; i < ARRAY_SIZE(blocks); i++) {
        Pmd_Render(split.pmd, b + position, blocks[i]);
        position += blocks[i];
    }
    CHECK_EQ_INT(position, ARRAY_SIZE(a));
    CHECK(memcmp(a, b, sizeof(a)) == 0);
    bool audible = false;
    for (size_t i = 0; i < ARRAY_SIZE(a); i++) {
        audible |= a[i].fm_rhythm[0] != 0 || a[i].fm_rhythm[1] != 0;
        for (size_t channel = 0; channel < 3; channel++) {
            CHECK_EQ_INT(a[i].ssg[channel], 0);
        }
    }
    CHECK(audible);
    CHECK(M_PcmHash(a, ARRAY_SIZE(a)) == UINT64_C(16317647398378189805));
    Arena_Free(&whole.arena);
    Arena_Free(&split.arena);
}

TEST(public_partial_slot_stub_stays_silent_beside_ssg_music)
{
    FIXTURE fixture;
    M_Init(&fixture);
    const uint8_t fm[] = { 0xff, 7, 0xcf, 0x10, 0x40, 4, 0x80 };
    const uint8_t ssg[] = { 0x40, 4, 0x80 };
    const SCORE score = {
        .fm = fm,
        .fm_size = sizeof(fm),
        .ssg = ssg,
        .ssg_size = sizeof(ssg),
        .ssg_part = 6,
    };
    M_Score(&fixture, &score);
    OPNA_FRAME frames[1600];
    Pmd_Render(fixture.pmd, frames, ARRAY_SIZE(frames));
    bool audible = false;
    for (size_t i = 0; i < ARRAY_SIZE(frames); i++) {
        CHECK_EQ_INT(frames[i].fm_rhythm[0], 0);
        CHECK_EQ_INT(frames[i].fm_rhythm[1], 0);
        audible |= frames[i].ssg[0] != 0;
        CHECK_EQ_INT(frames[i].ssg[1], 0);
        CHECK_EQ_INT(frames[i].ssg[2], 0);
    }
    CHECK(audible);
    Arena_Free(&fixture.arena);
}

TEST(public_masked_extension_keeps_parsing_without_a_bound_fm_voice)
{
    FIXTURE fixture;
    M_Init(&fixture);
    const uint8_t stream[] = { 0x40, 1, 0xdf, 2, 0xf6, 0x40, 1, 0x80 };
    const SCORE score = {
        .fm = stream,
        .fm_size = sizeof(stream),
        .fm_part = 3,
    };
    M_Score(&fixture, &score);
    OPNA_FRAME frames[1551];
    Pmd_Render(fixture.pmd, frames, 1550);
    CHECK_EQ_INT(Pmd_Measure(fixture.pmd), 0);
    Pmd_Render(fixture.pmd, frames + 1550, 1);
    CHECK_EQ_INT(Pmd_Measure(fixture.pmd), 1);
    for (size_t i = 0; i < ARRAY_SIZE(frames); i++) {
        CHECK_EQ_INT(frames[i].fm_rhythm[0], 0);
        CHECK_EQ_INT(frames[i].fm_rhythm[1], 0);
        for (size_t channel = 0; channel < 3; channel++) {
            CHECK_EQ_INT(frames[i].ssg[channel], 0);
        }
    }
    Arena_Free(&fixture.arena);
}

TEST(public_fm_and_ssg_random_lfos_share_the_same_pcm_timeline)
{
    FIXTURE whole;
    FIXTURE split;
    M_Init(&whole);
    M_Init(&split);
    const uint8_t fm[] = {
        0xff, 7, 0xf2, 0, 1,    2,    4, 0xf1, 1,
        0xcb, 3, 0xca, 1, 0xf6, 0x40, 8, 0x80,
    };
    const uint8_t ssg[] = {
        0xf2, 0, 1, 2, 4, 0xf1, 1, 0xcb, 3, 0xca, 1, 0xf6, 0x43, 9, 0x80,
    };
    const SCORE score = {
        .fm = fm,
        .fm_size = sizeof(fm),
        .ssg = ssg,
        .ssg_size = sizeof(ssg),
        .ssg_part = 6,
    };
    M_Score(&whole, &score);
    M_Score(&split, &score);
    OPNA_FRAME a[8192];
    OPNA_FRAME b[8192];
    Pmd_Render(whole.pmd, a, ARRAY_SIZE(a));
    const size_t blocks[] = { 17, 0, 775, 503, 2048, 1 };
    size_t position = 0;
    for (size_t i = 0; position < ARRAY_SIZE(b); i++) {
        const size_t remaining = ARRAY_SIZE(b) - position;
        const size_t requested = blocks[i % ARRAY_SIZE(blocks)];
        const size_t count = requested < remaining ? requested : remaining;
        Pmd_Render(split.pmd, b + position, count);
        position += count;
    }
    CHECK(memcmp(a, b, sizeof(a)) == 0);
    bool fm_audible = false;
    bool ssg_audible = false;
    for (size_t i = 0; i < ARRAY_SIZE(a); i++) {
        fm_audible |= a[i].fm_rhythm[0] != 0 || a[i].fm_rhythm[1] != 0;
        ssg_audible |= a[i].ssg[0] != 0;
    }
    CHECK(fm_audible && ssg_audible);
    CHECK(M_PcmHash(a, ARRAY_SIZE(a)) == UINT64_C(8199170059553910081));
    Arena_Free(&whole.arena);
    Arena_Free(&split.arena);
}

TEST(public_effects_keep_stopped_timer_phase_across_start_and_counter_wrap)
{
    const size_t skipped[] = { 775, 1000, 226855 };
    const size_t before[] = { 111, 772, 0 };
    const uint64_t expected[] = {
        UINT64_C(9649603406668771777),
        UINT64_C(14444050139024332959),
        UINT64_C(2785745332975181829),
    };
    for (size_t phase = 0; phase < ARRAY_SIZE(skipped); phase++) {
        FIXTURE fixture;
        M_InitEffect(&fixture, 1);
        Pmd_Stop(fixture.pmd);
        Pmd_Render(fixture.pmd, nullptr, skipped[phase]);
        if (phase == 1) {
            Pmd_Start(fixture.pmd);
            Pmd_Stop(fixture.pmd);
        }
        Pmd_Effect(fixture.pmd, 0);
        OPNA_FRAME frames[3000];
        Pmd_Render(fixture.pmd, frames, before[phase]);
        for (size_t i = 0; i < before[phase]; i++) {
            CHECK_EQ_INT(frames[i].fm_rhythm[0], 0);
            CHECK_EQ_INT(frames[i].fm_rhythm[1], 0);
        }
        Pmd_Render(fixture.pmd, frames + before[phase], 1);
        Pmd_Render(
            fixture.pmd, frames + before[phase] + 1,
            ARRAY_SIZE(frames) - before[phase] - 1);
        CHECK_EQ_INT(Pmd_Measure(fixture.pmd), 0);
        bool audible = false;
        for (size_t i = 0; i < ARRAY_SIZE(frames); i++) {
            audible |=
                frames[i].fm_rhythm[0] != 0 || frames[i].fm_rhythm[1] != 0;
            for (size_t channel = 0; channel < 3; channel++) {
                CHECK_EQ_INT(frames[i].ssg[channel], 0);
            }
        }
        CHECK(audible);
        CHECK(M_PcmHash(frames, ARRAY_SIZE(frames)) == expected[phase]);
        Arena_Free(&fixture.arena);
    }
}

TEST(public_effect_release_waits_for_the_next_music_note)
{
    FIXTURE music;
    FIXTURE rest;
    M_InitEffect(&music, 1);
    M_InitEffect(&rest, 1);
    const uint8_t notes[] = { 0xff, 7, 0xf6, 0x40, 8, 0x80 };
    const uint8_t rests[] = { 0xff, 7, 0xf6, 0x0f, 8, 0x80 };
    const SCORE music_score = { .fm = notes,
                                .fm_size = sizeof(notes),
                                .fm_part = 2 };
    const SCORE rest_score = { .fm = rests,
                               .fm_size = sizeof(rests),
                               .fm_part = 2 };
    M_Score(&music, &music_score);
    M_Score(&rest, &rest_score);
    Pmd_Effect(music.pmd, 0);
    Pmd_Effect(rest.pmd, 0);
    OPNA_FRAME a[8192];
    OPNA_FRAME b[8192];
    Pmd_Render(music.pmd, a, 6978);
    Pmd_Render(rest.pmd, b, 6978);
    CHECK(memcmp(a, b, 6978 * sizeof(*a)) == 0);
    bool effect_audible = false;
    for (size_t i = 0; i < 6978; i++) {
        effect_audible |= a[i].fm_rhythm[0] != 0 || a[i].fm_rhythm[1] != 0;
        if (i >= 6000) {
            CHECK_EQ_INT(a[i].fm_rhythm[0], 0);
            CHECK_EQ_INT(a[i].fm_rhythm[1], 0);
        }
    }
    CHECK(effect_audible);
    Pmd_Render(music.pmd, a + 6978, ARRAY_SIZE(a) - 6978);
    Pmd_Render(rest.pmd, b + 6978, ARRAY_SIZE(b) - 6978);
    CHECK(memcmp(a, b, sizeof(a)) != 0);
    bool music_audible = false;
    for (size_t i = 6978; i < ARRAY_SIZE(a); i++) {
        music_audible |= a[i].fm_rhythm[0] != 0 || a[i].fm_rhythm[1] != 0;
        CHECK_EQ_INT(b[i].fm_rhythm[0], 0);
        CHECK_EQ_INT(b[i].fm_rhythm[1], 0);
    }
    CHECK(music_audible);
    CHECK(M_PcmHash(a, ARRAY_SIZE(a)) == UINT64_C(17988645142390029233));
    Arena_Free(&music.arena);
    Arena_Free(&rest.arena);
}

TEST(public_music_and_effect_pcm_keep_coincident_block_boundaries)
{
    FIXTURE whole;
    FIXTURE split;
    const uint8_t effect[] = { 0xff, 0, 0x40, 6, 0x44, 8, 0x80 };
    M_InitEffectStream(&whole, effect, sizeof(effect), 200);
    M_InitEffectStream(&split, effect, sizeof(effect), 200);
    const uint8_t notes[] = { 0xff, 7, 0xf6, 0x40, 1, 0x43, 1, 0x80 };
    const SCORE score = { .fm = notes, .fm_size = sizeof(notes), .fm_part = 2 };
    M_ScoreTracks(&whole, &score, true);
    M_ScoreTracks(&split, &score, true);
    Pmd_Effect(whole.pmd, 0);
    Pmd_Effect(split.pmd, 0);
    OPNA_FRAME a[8192];
    OPNA_FRAME b[8192];
    Pmd_Render(whole.pmd, a, ARRAY_SIZE(a));
    const size_t blocks[] = { 6203, 1, 0, 1988 };
    size_t position = 0;
    for (size_t i = 0; i < ARRAY_SIZE(blocks); i++) {
        Pmd_Render(split.pmd, b + position, blocks[i]);
        position += blocks[i];
    }
    CHECK_EQ_INT(position, ARRAY_SIZE(a));
    CHECK(memcmp(a, b, sizeof(a)) == 0);
    CHECK(M_PcmHash(a, ARRAY_SIZE(a)) == UINT64_C(9009492031995194899));
    Arena_Free(&whole.arena);
    Arena_Free(&split.arena);
}

TEST(public_default_restore_uses_retained_work_after_the_bank_terminator)
{
    uint64_t hashes[2];
    for (unsigned retained = 0; retained < 2; retained++) {
        FIXTURE fixture;
        M_InitEffect(&fixture, 1);
        const uint8_t notes[] = { 0x0f, 8, 0x40, 16, 0x80 };
        const SCORE score = { .fm = notes,
                              .fm_size = sizeof(notes),
                              .fm_part = 2 };
        M_Score(&fixture, &score);
        uint8_t resident[543] = {};
        for (size_t i = 0; i < 11; i++) {
            M_Word(resident + 1 + i * 2, 26);
        }
        resident[27] = 0x80;
        M_Word(resident + 23, 500);
        M_Word(resident + 25, 512);
        M_Word(resident + 509, 540);
        M_Word(resident + 511, 0xfe48);
        resident[513] = 7;
        M_Word(resident + 539, 0xff00);
        memset(resident + 443, retained != 0 ? 31 : 0, 23);
        READER reader;
        Reader_OpenBuffer(&reader, resident, sizeof(resident), nullptr);
        PMD_SONG *prior;
        CHECK(Result_Ignore(PmdData_LoadSong(
            &reader, Arena_Allocator(&fixture.arena), &prior)));
        Reader_Close(&reader);
        Pmd_Load(fixture.pmd, prior);
        Pmd_Load(fixture.pmd, fixture.song);
        Pmd_Start(fixture.pmd);
        Pmd_Effect(fixture.pmd, 0);
        OPNA_FRAME frames[16000];
        Pmd_Render(fixture.pmd, frames, ARRAY_SIZE(frames));
        bool music_audible = false;
        for (size_t i = 7000; i < ARRAY_SIZE(frames); i++) {
            music_audible |=
                frames[i].fm_rhythm[0] != 0 || frames[i].fm_rhythm[1] != 0;
        }
        CHECK(music_audible);
        hashes[retained] = M_PcmHash(frames, ARRAY_SIZE(frames));
        const uint64_t expected[] = {
            UINT64_C(5437171774315614297),
            UINT64_C(6016270248608977987),
        };
        CHECK(hashes[retained] == expected[retained]);
        Arena_Free(&fixture.arena);
    }
    CHECK(hashes[0] != hashes[1]);
}

TEST(public_inline_fm_effect_keeps_the_current_note_dispatch_branch)
{
    FIXTURE actual;
    FIXTURE reference;
    M_InitEffect(&actual, 4);
    M_InitEffect(&reference, 4);
    const uint8_t commands[] = {
        0xff, 7, 0xd3, 1, 0xd3, 0, 0x40, 2, 0xc1, 0x80
    };
    const uint8_t without_stop[] = { 0xff, 7, 0xd3, 1, 0x40, 2, 0xc1, 0x80 };
    const SCORE score = { .fm = commands,
                          .fm_size = sizeof(commands),
                          .fm_part = 2 };
    const SCORE reference_score = { .fm = without_stop,
                                    .fm_size = sizeof(without_stop),
                                    .fm_part = 2 };
    M_Score(&actual, &score);
    M_Score(&reference, &reference_score);
    OPNA_FRAME a[8192];
    OPNA_FRAME b[8192];
    Pmd_Render(actual.pmd, a, ARRAY_SIZE(a));
    Pmd_Render(reference.pmd, b, ARRAY_SIZE(b));
    CHECK(memcmp(a, b, sizeof(a)) == 0);
    bool note_audible = false;
    for (size_t i = 775; i < 886; i++) {
        note_audible |= a[i].fm_rhythm[0] != 0 || a[i].fm_rhythm[1] != 0;
    }
    CHECK(note_audible);
    CHECK(M_PcmHash(a, ARRAY_SIZE(a)) == UINT64_C(17777292458913469077));
    Arena_Free(&actual.arena);
    Arena_Free(&reference.arena);
}

TEST(public_fade_in_keeps_the_equality_refresh_until_the_next_borrow)
{
    FIXTURE fixture;
    M_Init(&fixture);
    const uint8_t fm[] = { 0xdf, 1, 0xff, 7, 0xfd, 108, 0xf6, 0x40, 255, 0x80 };
    const uint8_t ssg[] = { 0xfd, 15, 0xf6, 0x40, 255, 0x80 };
    const SCORE score = {
        .fm = fm,
        .fm_size = sizeof(fm),
        .ssg = ssg,
        .ssg_size = sizeof(ssg),
        .ssg_part = 6,
    };
    M_Score(&fixture, &score);
    const size_t count = 32000;
    OPNA_FRAME *const frames = Allocator_Alloc(
        Arena_Allocator(&fixture.arena), count * sizeof(*frames));
    Pmd_Fade(fixture.pmd, 1);
    Pmd_Render(fixture.pmd, frames, 7090);
    CHECK_EQ_INT(Pmd_Measure(fixture.pmd), 9);
    Pmd_Fade(fixture.pmd, -1);
    Pmd_Render(fixture.pmd, frames + 7090, 14179 - 7090);
    CHECK_EQ_INT(Pmd_Measure(fixture.pmd), 18);
    Pmd_Render(fixture.pmd, frames + 14179, 21268 - 14179);
    CHECK_EQ_INT(Pmd_Measure(fixture.pmd), 27);
    Pmd_Render(fixture.pmd, frames + 21268, count - 21268);
    CHECK_EQ_INT(Pmd_Measure(fixture.pmd), 41);
    bool fm_audible = false;
    bool ssg_audible = false;
    for (size_t i = 0; i < count; i++) {
        fm_audible |=
            frames[i].fm_rhythm[0] != 0 || frames[i].fm_rhythm[1] != 0;
        ssg_audible |= frames[i].ssg[0] != 0;
    }
    CHECK(fm_audible && ssg_audible);
    CHECK(M_PcmHash(frames, count) == UINT64_C(5071831786766543125));
    Arena_Free(&fixture.arena);
}

TEST(public_fade_suppresses_later_percussion_until_restart)
{
    FIXTURE fixture;
    M_Init(&fixture);
    uint8_t song[133] = {};
    for (size_t i = 0; i < 11; i++) {
        M_Word(song + 1 + i * 2, i == 10 ? 32 : 26);
    }
    song[27] = 0x80;
    song[33] = 0;
    song[34] = 0x80;
    M_Word(song + 23, 96);
    M_Word(song + 25, 128);
    M_Word(song + 97, 98);
    const uint8_t rhythm[] = { 0x80, 0, 16, 0x80, 1, 8, 0xff };
    memcpy(song + 99, rhythm, sizeof(rhythm));
    M_Word(song + 125, 130);
    M_Word(song + 127, 0xfe48);
    M_Word(song + 129, 0xff00);
    READER reader;
    Reader_OpenBuffer(&reader, song, sizeof(song), nullptr);
    CHECK(Result_Ignore(PmdData_LoadSong(
        &reader, Arena_Allocator(&fixture.arena), &fixture.song)));
    Reader_Close(&reader);
    Pmd_Load(fixture.pmd, fixture.song);
    for (unsigned restart = 0; restart < 2; restart++) {
        Pmd_Start(fixture.pmd);
        if (restart == 0) {
            Pmd_Fade(fixture.pmd, 1);
        }
        bool audible = false;
        for (size_t block = 0; block < 25; block++) {
            OPNA_FRAME frames[800];
            Pmd_Render(fixture.pmd, frames, ARRAY_SIZE(frames));
            for (size_t i = 0; i < ARRAY_SIZE(frames); i++) {
                CHECK_EQ_INT(frames[i].fm_rhythm[0], 0);
                CHECK_EQ_INT(frames[i].fm_rhythm[1], 0);
                CHECK_EQ_INT(frames[i].ssg[0], 0);
                CHECK_EQ_INT(frames[i].ssg[1], 0);
                audible |= frames[i].ssg[2] != 0;
            }
        }
        CHECK(audible == (restart != 0));
    }
    Arena_Free(&fixture.arena);
}

TEST(public_ssg_melody_takes_over_low_priority_drums_before_slur_and_glide)
{
    const uint8_t melodies[][8] = {
        { 0x0f, 1, 0x40, 2, 0xc1, 0x80 },
        { 0x0f, 1, 0xda, 0x40, 0x43, 2, 0xc1, 0x80 },
    };
    const uint8_t drum[] = { 0xd4, 1, 0x80 };
    for (size_t melody = 0; melody < ARRAY_SIZE(melodies); melody++) {
        FIXTURE fixture;
        M_Init(&fixture);
        const SCORE score = {
            .fm = drum,
            .fm_size = sizeof(drum),
            .ssg = melodies[melody],
            .ssg_size = sizeof(melodies[melody]),
            .ssg_part = 8,
        };
        M_Score(&fixture, &score);
        OPNA_FRAME frames[8192];
        Pmd_Render(fixture.pmd, frames, ARRAY_SIZE(frames));
        bool audible = false;
        for (size_t i = 0; i < ARRAY_SIZE(frames); i++) {
            CHECK_EQ_INT(frames[i].fm_rhythm[0], 0);
            CHECK_EQ_INT(frames[i].fm_rhythm[1], 0);
            CHECK_EQ_INT(frames[i].ssg[0], 0);
            CHECK_EQ_INT(frames[i].ssg[1], 0);
            audible |= frames[i].ssg[2] != 0;
        }
        CHECK(audible);
        const uint64_t expected[] = {
            UINT64_C(618429879841721068),
            UINT64_C(16750468697836679062),
        };
        CHECK(M_PcmHash(frames, ARRAY_SIZE(frames)) == expected[melody]);
        Arena_Free(&fixture.arena);
    }
}

TEST(public_regular_ssg_effect_prevents_melody_takeover)
{
    FIXTURE actual;
    FIXTURE reference;
    M_Init(&actual);
    M_Init(&reference);
    const uint8_t effect[] = { 0xd4, 11, 0x80 };
    const uint8_t melody[] = { 0x0f, 1, 0x40, 2, 0xc1, 0x80 };
    const SCORE score = {
        .fm = effect,
        .fm_size = sizeof(effect),
        .ssg = melody,
        .ssg_size = sizeof(melody),
        .ssg_part = 8,
    };
    const SCORE without_melody = { .fm = effect, .fm_size = sizeof(effect) };
    M_Score(&actual, &score);
    M_Score(&reference, &without_melody);
    OPNA_FRAME a[8192];
    OPNA_FRAME b[8192];
    Pmd_Render(actual.pmd, a, ARRAY_SIZE(a));
    Pmd_Render(reference.pmd, b, ARRAY_SIZE(b));
    CHECK(memcmp(a, b, sizeof(a)) == 0);
    bool audible = false;
    for (size_t i = 0; i < ARRAY_SIZE(a); i++) {
        audible |= a[i].ssg[2] != 0;
    }
    CHECK(audible);
    CHECK(M_PcmHash(a, ARRAY_SIZE(a)) == UINT64_C(5464511127235052908));
    Arena_Free(&actual.arena);
    Arena_Free(&reference.arena);
}

TEST(public_inline_ssg_effect_keeps_the_current_note_dispatch_branch)
{
    FIXTURE actual;
    FIXTURE reference;
    M_Init(&actual);
    M_Init(&reference);
    const uint8_t commands[] = { 0xd4, 11, 0xd4, 0, 0x40, 2, 0xc1, 0x80 };
    const uint8_t without_stop[] = { 0xd4, 11, 0x40, 2, 0xc1, 0x80 };
    const SCORE score = { .ssg = commands,
                          .ssg_size = sizeof(commands),
                          .ssg_part = 8 };
    const SCORE reference_score = { .ssg = without_stop,
                                    .ssg_size = sizeof(without_stop),
                                    .ssg_part = 8 };
    M_Score(&actual, &score);
    M_Score(&reference, &reference_score);
    OPNA_FRAME a[8192];
    OPNA_FRAME b[8192];
    Pmd_Render(actual.pmd, a, ARRAY_SIZE(a));
    Pmd_Render(reference.pmd, b, ARRAY_SIZE(b));
    CHECK(memcmp(a, b, sizeof(a)) == 0);
    bool audible = false;
    for (size_t i = 0; i < ARRAY_SIZE(a); i++) {
        audible |= a[i].ssg[2] != 0;
    }
    CHECK(audible);
    CHECK(M_PcmHash(a, ARRAY_SIZE(a)) == UINT64_C(16514350452843364578));
    Arena_Free(&actual.arena);
    Arena_Free(&reference.arena);
}

TEST(public_effect_ties_glides_gates_and_modulation_keep_pcm)
{
    const uint8_t tied[] = {
        0xff, 0,    0xfd, 127,  0x40, 1, 0xfb, 0x41,
        2,    0xc1, 0xda, 0x42, 0x45, 4, 0x80,
    };
    const uint8_t modulated[] = {
        0xff, 0,    0xfd, 100,  0xfe, 2, 0xf2, 0, 1,    2,
        4,    0xf1, 3,    0xf6, 0x40, 4, 0x0f, 2, 0x80,
    };
    const struct {
        const uint8_t *bytes;
        size_t size;
    } streams[] = { { tied, sizeof(tied) }, { modulated, sizeof(modulated) } };
    for (size_t i = 0; i < ARRAY_SIZE(streams); i++) {
        FIXTURE fixture;
        M_InitEffectStream(&fixture, streams[i].bytes, streams[i].size, 200);
        Pmd_Stop(fixture.pmd);
        Pmd_Effect(fixture.pmd, 0);
        OPNA_FRAME frames[16000];
        Pmd_Render(fixture.pmd, frames, ARRAY_SIZE(frames));
        bool audible = false;
        for (size_t frame = 0; frame < ARRAY_SIZE(frames); frame++) {
            audible |= frames[frame].fm_rhythm[0] != 0
                || frames[frame].fm_rhythm[1] != 0;
        }
        CHECK(audible);
        const uint64_t expected[] = { UINT64_C(541676128557234787),
                                      UINT64_C(13604679219586899901) };
        CHECK(M_PcmHash(frames, ARRAY_SIZE(frames)) == expected[i]);
        Arena_Free(&fixture.arena);
    }
}

TEST(cold_boot_retains_the_zero_timer_b_interval_before_idle_tempo)
{
    FIXTURE fixture;
    const uint8_t effect[] = { 0xff, 0, 0x43, 255, 0x80 };
    M_InitEffectStream(&fixture, effect, sizeof(effect), 0);
    const uint8_t stream[] = { 0xdf, 1, 0xfc, 250, 0xf6, 0x40, 1, 0x80 };
    const SCORE score = { .fm = stream, .fm_size = sizeof(stream) };
    M_Score(&fixture, &score);
    Pmd_Render(fixture.pmd, nullptr, 3544);
    CHECK_EQ_INT(Pmd_Measure(fixture.pmd), 0);
    Pmd_Render(fixture.pmd, nullptr, 1);
    CHECK_EQ_INT(Pmd_Measure(fixture.pmd), 1);
    Pmd_Render(fixture.pmd, nullptr, 774);
    CHECK_EQ_INT(Pmd_Measure(fixture.pmd), 1);
    Pmd_Render(fixture.pmd, nullptr, 1);
    CHECK_EQ_INT(Pmd_Measure(fixture.pmd), 2);
    Arena_Free(&fixture.arena);
}

TEST(board86_renders_each_of_the_six_physical_fm_voices)
{
    const uint8_t effect[] = { 0xff, 0, 0x43, 255, 0x80 };
    const uint8_t music[] = { 0xff, 7, 0xfd, 108, 0x40, 255, 0x80 };
    for (uint8_t voice = 0; voice < 6; voice++) {
        FIXTURE fixture;
        M_InitEffectStreamBoard(
            &fixture, effect, sizeof(effect), 200, PMD_BOARD_86);
        const SCORE score = { .fm = music,
                              .fm_size = sizeof(music),
                              .fm_part = voice };
        M_Score(&fixture, &score);
        OPNA_FRAME frames[8192];
        Pmd_Render(fixture.pmd, frames, ARRAY_SIZE(frames));
        bool audible = false;
        for (size_t i = 0; i < ARRAY_SIZE(frames); i++) {
            audible |=
                frames[i].fm_rhythm[0] != 0 || frames[i].fm_rhythm[1] != 0;
        }
        CHECK(audible);
        Arena_Free(&fixture.arena);
    }
}

TEST(board86_effect_borrows_fm6_without_interrupting_fm3)
{
    const uint8_t effect[] = { 0xec, 1, 0xff, 0, 0x43, 255, 0x80 };
    const uint8_t music[] = { 0xff, 7, 0xec, 2, 0xfd, 108, 0x40, 255, 0x80 };
    FIXTURE control;
    FIXTURE playing;
    M_InitEffectStreamBoard(
        &control, effect, sizeof(effect), 200, PMD_BOARD_86);
    M_InitEffectStreamBoard(
        &playing, effect, sizeof(effect), 200, PMD_BOARD_86);
    const SCORE score = { .fm = music, .fm_size = sizeof(music), .fm_part = 2 };
    M_Score(&control, &score);
    M_Score(&playing, &score);
    Pmd_Render(control.pmd, nullptr, 4096);
    Pmd_Render(playing.pmd, nullptr, 4096);
    Pmd_Effect(playing.pmd, 0);
    OPNA_FRAME a[8192];
    OPNA_FRAME b[8192];
    Pmd_Render(control.pmd, a, ARRAY_SIZE(a));
    Pmd_Render(playing.pmd, b, ARRAY_SIZE(b));
    bool effect_audible = false;
    for (size_t i = 0; i < ARRAY_SIZE(a); i++) {
        CHECK_EQ_INT(a[i].fm_rhythm[0], b[i].fm_rhythm[0]);
        CHECK_EQ_INT(a[i].fm_rhythm[1], 0);
        effect_audible |= b[i].fm_rhythm[1] != 0;
    }
    CHECK(effect_audible);
    Arena_Free(&control.arena);
    Arena_Free(&playing.arena);
}

TEST(board86_direct_rhythm_commands_set_level_and_pan)
{
    const uint8_t effect[] = { 0x80 };
    const uint8_t music[] = { 0xe8, 63, 0xea, 0x3f, 0xe9, 0x22,
                              0xeb, 1,  0x0f, 255,  0x80 };
    FIXTURE fixture;
    M_InitEffectStreamBoard(
        &fixture, effect, sizeof(effect), 200, PMD_BOARD_86);
    const SCORE score = { .fm = music, .fm_size = sizeof(music) };
    M_Score(&fixture, &score);
    OPNA_FRAME frames[8192];
    Pmd_Render(fixture.pmd, frames, ARRAY_SIZE(frames));
    bool audible = false;
    for (size_t i = 0; i < ARRAY_SIZE(frames); i++) {
        audible |= frames[i].fm_rhythm[0] != 0;
        CHECK_EQ_INT(frames[i].fm_rhythm[1], 0);
        CHECK_EQ_INT(frames[i].ssg[0] + frames[i].ssg[1] + frames[i].ssg[2], 0);
    }
    CHECK(audible);
    Arena_Free(&fixture.arena);
}

TEST(public_loading_song_data_keeps_current_playback_running)
{
    FIXTURE control;
    FIXTURE loading;
    M_Init(&control);
    M_Init(&loading);
    const uint8_t music[] = { 0xfd, 15, 0x40, 255, 0x80 };
    const SCORE score = { .ssg = music,
                          .ssg_size = sizeof(music),
                          .ssg_part = 6 };
    M_Score(&control, &score);
    M_Score(&loading, &score);
    Pmd_Render(control.pmd, nullptr, 2560);
    Pmd_Render(loading.pmd, nullptr, 2560);
    Pmd_Load(loading.pmd, loading.song);
    OPNA_FRAME a[4096];
    OPNA_FRAME b[4096];
    Pmd_Render(control.pmd, a, ARRAY_SIZE(a));
    Pmd_Render(loading.pmd, b, ARRAY_SIZE(b));
    CHECK(memcmp(a, b, sizeof(a)) == 0);
    CHECK_EQ_INT(Pmd_Measure(control.pmd), Pmd_Measure(loading.pmd));
    bool audible = false;
    for (size_t i = 0; i < ARRAY_SIZE(a); i++) {
        audible |= a[i].ssg[0] != 0;
    }
    CHECK(audible);
    Arena_Free(&control.arena);
    Arena_Free(&loading.arena);
}

TEST(board86_song_start_keeps_the_borrowed_effect_pan_and_pcm)
{
    FIXTURE control;
    FIXTURE starting;
    const uint8_t effect[] = { 0xec, 2, 0xff, 0, 0x40, 255, 0x80 };
    M_InitEffectStreamBoard(
        &control, effect, sizeof(effect), 200, PMD_BOARD_86);
    M_InitEffectStreamBoard(
        &starting, effect, sizeof(effect), 200, PMD_BOARD_86);
    const SCORE quiet = {};
    M_Score(&control, &quiet);
    M_Score(&starting, &quiet);
    Pmd_Stop(control.pmd);
    Pmd_Stop(starting.pmd);
    Pmd_Effect(control.pmd, 0);
    Pmd_Effect(starting.pmd, 0);
    Pmd_Render(control.pmd, nullptr, 4096);
    Pmd_Render(starting.pmd, nullptr, 4096);
    Pmd_Start(starting.pmd);
    OPNA_FRAME a[4096];
    OPNA_FRAME b[4096];
    Pmd_Render(control.pmd, a, ARRAY_SIZE(a));
    Pmd_Render(starting.pmd, b, ARRAY_SIZE(b));
    CHECK(memcmp(a, b, sizeof(a)) == 0);
    bool audible = false;
    for (size_t i = 0; i < ARRAY_SIZE(a); i++) {
        audible |= a[i].fm_rhythm[0] != 0;
        CHECK_EQ_INT(a[i].fm_rhythm[1], 0);
    }
    CHECK(audible);
    Arena_Free(&control.arena);
    Arena_Free(&starting.arena);
}
