#include "tests/test.h"

#include "core/base/arena.h"
#include "pc98/mdrv.h"

#include <string.h>

#define M_FRAMES 48000

typedef struct {
    ARENA arena;
    ALLOCATOR allocator;
    size_t allocations;
    uint8_t bytes[2][806];
    uint8_t effect_bytes[118];
    MDRV_TRACK tracks[2][10];
    MDRV_MUSIC songs[2];
    MDRV_EFFECT effects[2];
    MDRV_EFFECTS effect_bank;
    MDRV_FRAME *frames;
} FIXTURE;

static void *M_Alloc(void *const context, const size_t size)
{
    FIXTURE *const fixture = context;
    fixture->allocations++;
    return Arena_Alloc(&fixture->arena, size);
}

// Builds two songs that play code on channel next to a held FM note, with
// one FM program and one SSG envelope, and two SSG effects of one and three
// channels.
static void M_Init(
    FIXTURE *const fixture, const uint8_t *const code, const size_t size,
    const uint8_t channel)
{
    memset(fixture, 0, sizeof(*fixture));
    Arena_Init(&fixture->arena, "MDRV", 16 * 1024 * 1024);
    fixture->allocator = (ALLOCATOR) { .alloc = M_Alloc, .context = fixture };
    fixture->frames =
        Arena_Alloc(&fixture->arena, M_FRAMES * sizeof(*fixture->frames));
    for (size_t song = 0; song < 2; song++) {
        uint8_t *const bytes = fixture->bytes[song];
        memcpy(bytes + 32, code, size);
        const uint8_t hold[] = { 0xea, 0, 0x40, 255, 0xff };
        memcpy(bytes + 160, hold, sizeof(hold));
        fixture->tracks[song][0] =
            (MDRV_TRACK) { .offset = 32, .channel = channel };
        fixture->tracks[song][1] = (MDRV_TRACK) {
            .offset = 160,
            .channel = channel == 0x80 ? 0x81 : 0x80,
        };
        bytes[771] = 0x78;
        bytes[772] = song == 0 ? 7 : 4;
        for (size_t i = 0; i < 4; i++) {
            bytes[774 + i] = i + 1;
            bytes[778 + i] = 3 + i * 2;
            bytes[782 + i] = 31;
            bytes[794 + i] = 15;
        }
        const uint8_t envelope[] = { 128, 64, 16, 96, 3, 7 };
        memcpy(bytes + 800, envelope, sizeof(envelope));
        fixture->songs[song] = (MDRV_MUSIC) {
            .bytes = bytes,
            .size = 806,
            .tracks = fixture->tracks[song],
            .track_count = 2,
            .commands_offset = 32,
            .fm_offset = 768,
            .ssg_offset = 800,
            .fm_count = 1,
            .ssg_count = 1,
        };
    }
    const uint8_t effect[] = { 0xeb, 0x80, 0xec, 15, 0x48, 8, 0xff };
    memcpy(fixture->effect_bytes + 32, effect, sizeof(effect));
    memcpy(fixture->effect_bytes + 64, effect, sizeof(effect));
    memcpy(fixture->effect_bytes + 80, effect, sizeof(effect));
    const uint8_t envelope[] = { 128, 64, 16, 96, 3, 7 };
    memcpy(fixture->effect_bytes + 112, envelope, sizeof(envelope));
    fixture->effects[0] = (MDRV_EFFECT) {
        .channel_count = 1,
        .base_channel = 0x40,
        .stream_offsets = { 32 },
    };
    fixture->effects[1] = (MDRV_EFFECT) {
        .channel_count = 3,
        .base_channel = 0x40,
        .stream_offsets = { 32, 64, 80 },
    };
    fixture->effect_bank = (MDRV_EFFECTS) {
        .bytes = fixture->effect_bytes,
        .size = sizeof(fixture->effect_bytes),
        .effects = fixture->effects,
        .effect_count = 2,
        .ssg_offset = 112,
        .ssg_count = 1,
    };
}

static MDRV *M_Start(FIXTURE *const fixture, const uint8_t fm_balance)
{
    MDRV *mdrv = nullptr;
    CHECK(Result_Log(
        Mdrv_Init(
            fixture->songs, 2, &fixture->effect_bank, fm_balance, 8,
            &fixture->allocator, &mdrv),
        LOG_LEVEL_ERROR));
    Mdrv_Play(mdrv, 0);
    return mdrv;
}

static int32_t M_Peak(
    const MDRV_FRAME *const frames, const size_t count, const bool effects)
{
    int32_t peak = 0;
    for (size_t i = 0; i < count; i++) {
        for (size_t stereo = 0; stereo < 2; stereo++) {
            int32_t value =
                effects ? frames[i].effects[stereo] : frames[i].music[stereo];
            value = value < 0 ? -value : value;
            peak = value > peak ? value : peak;
        }
    }
    return peak;
}

static bool M_Constant(const MDRV_FRAME *const frames, const size_t count)
{
    for (size_t i = 1; i < count; i++) {
        if (memcmp(&frames[i], &frames[0], sizeof(frames[0])) != 0) {
            return false;
        }
    }
    return true;
}

TEST(init_validates_before_allocation)
{
    const uint8_t code[] = { 0x40, 8, 0xff };
    FIXTURE fixture;
    M_Init(&fixture, code, sizeof(code), 0x80);
    const size_t allocations = fixture.allocations;
    MDRV *mdrv = nullptr;
    CHECK(!Result_Ignore(Mdrv_Init(
        fixture.songs, 2, &fixture.effect_bank, 128, 0, &fixture.allocator,
        &mdrv)));
    CHECK(!Result_Ignore(Mdrv_Init(
        fixture.songs, 2, &fixture.effect_bank, 0, 64, &fixture.allocator,
        &mdrv)));
    fixture.tracks[1][0].channel = 0x20;
    CHECK(!Result_Ignore(Mdrv_Init(
        fixture.songs, 2, &fixture.effect_bank, 8, 8, &fixture.allocator,
        &mdrv)));
    fixture.songs[1].track_count = 7;
    for (size_t i = 0; i < 7; i++) {
        fixture.tracks[1][i] = (MDRV_TRACK) { .channel = 0x80, .offset = 32 };
    }
    CHECK(!Result_Ignore(Mdrv_Init(
        fixture.songs, 2, &fixture.effect_bank, 8, 8, &fixture.allocator,
        &mdrv)));
    CHECK(mdrv == nullptr);
    CHECK_EQ_INT(fixture.allocations, allocations);
    Arena_Free(&fixture.arena);
}

TEST(init_reads_instrument_banks)
{
    const uint8_t code[] = { 0x40, 8, 0xff };
    FIXTURE fixture;
    M_Init(&fixture, code, sizeof(code), 0x80);
    fixture.songs[1].size = 799;
    MDRV *mdrv = nullptr;
    CHECK(!Result_Ignore(Mdrv_Init(
        fixture.songs, 2, &fixture.effect_bank, 8, 8, &fixture.allocator,
        &mdrv)));
    CHECK(mdrv == nullptr);
    Arena_Free(&fixture.arena);
}

TEST(render_is_deterministic_across_chunks_and_discards)
{
    const uint8_t code[] = {
        0xeb, 0,    0xec, 110,  0xed, 220,  6,    12,
        2,    0x40, 8,    0x42, 8,    0xf3, 0xfa, 0xff,
    };
    FIXTURE fixture;
    M_Init(&fixture, code, sizeof(code), 0x80);
    MDRV *const whole = M_Start(&fixture, 8);
    MDRV *const split = M_Start(&fixture, 8);
    MDRV *const skip = M_Start(&fixture, 8);
    MDRV_FRAME *const second =
        Arena_Alloc(&fixture.arena, 12000 * sizeof(*second));
    const size_t allocations = fixture.allocations;
    Mdrv_Effect(whole, 2);
    Mdrv_Effect(split, 2);
    Mdrv_Effect(skip, 2);
    Mdrv_Render(whole, fixture.frames, 12000);
    size_t position = 0;
    while (position < 12000) {
        size_t count = position % 137 + 1;
        if (count > 12000 - position) {
            count = 12000 - position;
        }
        Mdrv_Render(split, second + position, count);
        position += count;
    }
    CHECK(memcmp(fixture.frames, second, 12000 * sizeof(*second)) == 0);
    Mdrv_Render(skip, nullptr, 5000);
    Mdrv_Render(skip, second, 7000);
    CHECK(memcmp(fixture.frames + 5000, second, 7000 * sizeof(*second)) == 0);
    CHECK(M_Peak(fixture.frames, 12000, false) != 0);
    CHECK(M_Peak(fixture.frames, 12000, true) != 0);
    Mdrv_Play(whole, 1);
    Mdrv_Stop(whole);
    Mdrv_Effect(whole, 1);
    Mdrv_Fade(whole, 2);
    Mdrv_Render(whole, fixture.frames, 1000);
    CHECK_EQ_INT(fixture.allocations, allocations);
    Arena_Free(&fixture.arena);
}

TEST(songs_select_their_own_programs)
{
    const uint8_t code[] = { 0xeb, 0, 0xec, 110, 0x40, 255, 0xff };
    FIXTURE fixture;
    M_Init(&fixture, code, sizeof(code), 0x80);
    MDRV *const first = M_Start(&fixture, 8);
    MDRV *const second = M_Start(&fixture, 8);
    Mdrv_Play(second, 1);
    MDRV_FRAME *const other =
        Arena_Alloc(&fixture.arena, 4800 * sizeof(*other));
    Mdrv_Render(first, fixture.frames, 4800);
    Mdrv_Render(second, other, 4800);
    CHECK(memcmp(fixture.frames, other, 4800 * sizeof(*other)) != 0);
    Arena_Free(&fixture.arena);
}

TEST(fm_balance_attenuates_music)
{
    const uint8_t code[] = { 0xeb, 0, 0xec, 110, 0x40, 255, 0xff };
    FIXTURE fixture;
    M_Init(&fixture, code, sizeof(code), 0x80);
    MDRV *const loud = M_Start(&fixture, 0);
    MDRV *const quiet = M_Start(&fixture, 40);
    Mdrv_Render(loud, fixture.frames, 4800);
    const int32_t peak = M_Peak(fixture.frames, 4800, false);
    Mdrv_Render(quiet, fixture.frames, 4800);
    CHECK(M_Peak(fixture.frames, 4800, false) * 4 < peak);
    Arena_Free(&fixture.arena);
}

TEST(effects_hold_ssg_channels_until_they_end)
{
    const uint8_t code[] = { 0xeb, 0, 0xec, 110, 0x40, 255, 0xff };
    FIXTURE fixture;
    M_Init(&fixture, code, sizeof(code), 0x80);
    MDRV *const mdrv = M_Start(&fixture, 8);
    MDRV *const twin = M_Start(&fixture, 8);
    Mdrv_Render(mdrv, fixture.frames, 2000);
    CHECK_EQ_INT(M_Peak(fixture.frames, 2000, true), 0);
    Mdrv_Render(twin, nullptr, 2000);
    Mdrv_Effect(mdrv, 1);
    Mdrv_Effect(mdrv, 0);
    Mdrv_Effect(twin, 1);
    Mdrv_Render(mdrv, fixture.frames, 16000);
    CHECK(M_Peak(fixture.frames, 4000, true) != 0);
    CHECK(M_Peak(fixture.frames, 16000, false) != 0);
    CHECK_EQ_INT(M_Peak(fixture.frames + 12000, 4000, true), 0);
    MDRV_FRAME *const other =
        Arena_Alloc(&fixture.arena, 16000 * sizeof(*other));
    Mdrv_Render(twin, other, 16000);
    CHECK(memcmp(fixture.frames, other, 16000 * sizeof(*other)) == 0);
    Arena_Free(&fixture.arena);
}

TEST(play_cancels_effects)
{
    const uint8_t code[] = { 0xeb, 0, 0xec, 110, 0x40, 255, 0xff };
    FIXTURE fixture;
    M_Init(&fixture, code, sizeof(code), 0x80);
    MDRV *const mdrv = M_Start(&fixture, 8);
    Mdrv_Effect(mdrv, 2);
    Mdrv_Render(mdrv, fixture.frames, 2000);
    CHECK(M_Peak(fixture.frames, 2000, true) != 0);
    Mdrv_Play(mdrv, 0);
    Mdrv_Render(mdrv, fixture.frames, 4000);
    CHECK_EQ_INT(M_Peak(fixture.frames, 4000, true), 0);
    CHECK(M_Peak(fixture.frames, 4000, false) != 0);
    Arena_Free(&fixture.arena);
}

TEST(stop_silences_music_and_effects)
{
    const uint8_t code[] = { 0xeb, 0, 0xec, 110, 0x40, 255, 0xff };
    FIXTURE fixture;
    M_Init(&fixture, code, sizeof(code), 0x80);
    MDRV *const mdrv = M_Start(&fixture, 8);
    Mdrv_Effect(mdrv, 2);
    Mdrv_Render(mdrv, fixture.frames, 2000);
    Mdrv_Stop(mdrv);
    Mdrv_Render(mdrv, fixture.frames, 9600);
    CHECK_EQ_INT(M_Peak(fixture.frames, 9600, true), 0);
    CHECK(M_Constant(fixture.frames + 4800, 4800));
    Arena_Free(&fixture.arena);
}

TEST(fade_out_stops_playback_and_counts_zero_as_256)
{
    const uint8_t code[] = { 0xeb, 0,   0xec, 110,  0xea, 0,
                             0x40, 255, 0xf3, 0xfc, 0xff };
    FIXTURE fixture;
    M_Init(&fixture, code, sizeof(code), 0x80);
    MDRV *const fast = M_Start(&fixture, 8);
    MDRV *const slow = M_Start(&fixture, 8);
    Mdrv_Fade(fast, 1);
    Mdrv_Fade(slow, 0);
    Mdrv_Render(fast, fixture.frames, 4800);
    CHECK(M_Peak(fixture.frames, 4800, false) != 0);
    Mdrv_Render(fast, nullptr, 2 * M_FRAMES);
    Mdrv_Render(fast, fixture.frames, 4800);
    CHECK(M_Constant(fixture.frames, 4800));
    Mdrv_Render(slow, nullptr, 2 * M_FRAMES + 4800);
    Mdrv_Render(slow, fixture.frames, 4800);
    CHECK(!M_Constant(fixture.frames, 4800));
    Arena_Free(&fixture.arena);
}

TEST(fade_in_starts_attenuated)
{
    const uint8_t code[] = { 0xeb, 0, 0xec, 110, 0x40, 255, 0xff };
    FIXTURE fixture;
    M_Init(&fixture, code, sizeof(code), 0x80);
    MDRV *const plain = M_Start(&fixture, 8);
    MDRV *const faded = M_Start(&fixture, 8);
    Mdrv_Fade(faded, 0x88);
    Mdrv_Render(plain, fixture.frames, 9600);
    const int32_t peak = M_Peak(fixture.frames + 4800, 4800, false);
    Mdrv_Render(faded, fixture.frames, 9600);
    CHECK(M_Peak(fixture.frames + 4800, 4800, false) * 4 < peak);
    Arena_Free(&fixture.arena);
}

TEST(header_repeat_restarts_the_song)
{
    const uint8_t code[] = { 0xeb, 0, 0xec, 110, 0x40, 8, 0xff };
    FIXTURE fixture;
    M_Init(&fixture, code, sizeof(code), 0x80);
    fixture.songs[0].track_count = 1;
    fixture.songs[1].track_count = 1;
    fixture.songs[1].flags = 0x80;
    MDRV *const once = M_Start(&fixture, 8);
    MDRV *const repeat = M_Start(&fixture, 8);
    Mdrv_Play(repeat, 1);
    Mdrv_Render(once, fixture.frames, M_FRAMES);
    CHECK(M_Peak(fixture.frames, 4800, false) != 0);
    CHECK(M_Constant(fixture.frames + 43200, 4800));
    Mdrv_Render(repeat, fixture.frames, M_FRAMES);
    CHECK(!M_Constant(fixture.frames + 43200, 4800));
    Arena_Free(&fixture.arena);
}
