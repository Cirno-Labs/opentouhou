#include "th01/audio.h"

#include "core/audio.h"
#include "core/audio/backend.h"
#include "core/base/arena.h"
#include "test.h"

#include <SDL3/SDL.h>
#include <math.h>

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
    MDRV *mdrv;
} FIXTURE;

static void *M_Alloc(void *const context, const size_t size)
{
    FIXTURE *const fixture = context;
    fixture->allocations++;
    return Arena_Alloc(&fixture->arena, size);
}

static void M_Init(
    FIXTURE *const fixture, const uint8_t *const code, const size_t size,
    const uint8_t channel)
{
    memset(fixture, 0, sizeof(*fixture));
    Arena_Init(&fixture->arena, "MDRV producer", 4 * 1024 * 1024);
    fixture->allocator = (ALLOCATOR) { .alloc = M_Alloc, .context = fixture };
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

static MDRV *M_Play(FIXTURE *const fixture)
{
    MDRV *mdrv = nullptr;
    CHECK(Result_Ignore(Mdrv_Init(
        fixture->songs, 2, &fixture->effect_bank, 8, 8, &fixture->allocator,
        &mdrv)));
    Mdrv_Play(mdrv, 0);
    Mdrv_Effect(mdrv, 2);
    return mdrv;
}

static void M_Start(FIXTURE *const fixture)
{
    const uint8_t code[] = { 0xeb, 0, 0xec, 110, 0xea, 0, 0x40, 255, 0xff };
    M_Init(fixture, code, sizeof(code), 0x80);
    fixture->mdrv = M_Play(fixture);
}

static void M_Mix(float *const samples, const uint32_t frames)
{
    const Uint64 deadline = SDL_GetTicks() + 5000;
    while (!AudioBackend_Mix(samples, frames)) {
        if (SDL_GetTicks() >= deadline) {
            TEST_FAIL("audio mixer lock timeout");
            return;
        }
        SDL_Delay(1);
    }
}

static void M_Close(FIXTURE *const fixture, GAME_AUDIO *const output)
{
    GameAudio_Free(output);
    Audio_Shutdown();
    Arena_Free(&fixture->arena);
}

TEST(headless_and_failed_preparation_preserve_producer)
{
    FIXTURE fixture;
    M_Start(&fixture);
    GAME_AUDIO *output = nullptr;
    const size_t before = fixture.allocations;
    CHECK(!Result_Ignore(GameAudio_Prepare(&fixture.allocator, &output)));
    CHECK(output == nullptr);
    GameAudio_Free(output);
    GameAudio_Render(nullptr, fixture.mdrv, 800);
    CHECK_EQ_INT(fixture.allocations, before);
    Arena_Free(&fixture.arena);
}

TEST(real_producer_chunks_and_bus_controls_share_one_timeline)
{
    CHECK(Result_Ignore(Audio_Init()));
    FIXTURE fixture;
    M_Start(&fixture);
    GAME_AUDIO *output;
    CHECK(Result_Ignore(GameAudio_Prepare(&fixture.allocator, &output)));
    MDRV *const twin = M_Play(&fixture);
    const size_t before = fixture.allocations;
    float whole[3200];
    float split[3200];
    GameAudio_Render(output, fixture.mdrv, 1600);
    M_Mix(whole, 1600);
    GameAudio_Free(output);
    CHECK(Result_Ignore(GameAudio_Prepare(&fixture.allocator, &output)));
    size_t at = 0;
    while (at < 1600) {
        const size_t count = 1600 - at < 137 ? 1600 - at : 137;
        GameAudio_Render(output, twin, count);
        M_Mix(split + at * 2, (uint32_t)count);
        at += count;
    }
    CHECK(memcmp(whole, split, sizeof(whole)) == 0);
    bool nonzero = false;
    for (size_t i = 0; i < 3200; i++) {
        CHECK(isfinite(whole[i]));
        nonzero |= whole[i] != 0;
    }
    CHECK(nonzero);
    CHECK_EQ_INT(fixture.allocations, before + 1);
    M_Close(&fixture, output);
}

TEST(mixer_routes_components_with_gain_mute_and_pause)
{
    CHECK(Result_Ignore(Audio_Init()));
    FIXTURE fixture;
    M_Start(&fixture);
    GAME_AUDIO *output;
    CHECK(Result_Ignore(GameAudio_Prepare(&fixture.allocator, &output)));
    MDRV_FRAME raw[800];
    Mdrv_Render(fixture.mdrv, raw, 800);
    GameAudio_Render(output, M_Play(&fixture), 800);
    CHECK(Result_Ignore(Audio_SetMasterVolume(0.5f)));
    CHECK(Result_Ignore(Audio_SetGroupVolume(AUDIO_GROUP_MUSIC, 0.5f)));
    CHECK(Result_Ignore(Audio_SetGroupVolume(AUDIO_GROUP_EFFECTS, 0.25f)));
    float previous_input[2][2] = {};
    float previous_output[2][2] = {};
    bool music = false;
    bool effects = false;
    for (size_t i = 0; i < 800; i++) {
        const bool pause = i >= 200 && i < 400;
        const bool mute = i >= 400 && i < 600;
        Audio_SetGroupPaused(AUDIO_GROUP_MUSIC, pause);
        Audio_SetGroupMuted(AUDIO_GROUP_EFFECTS, mute);
        float mixed[2];
        M_Mix(mixed, 1);
        for (size_t channel = 0; channel < 2; channel++) {
            const int32_t components[] = { raw[i].music[channel],
                                           raw[i].effects[channel] };
            float expected = 0;
            for (size_t bus = 0; bus < 2; bus++) {
                const float input = (float)components[bus] / 65536.0f;
                const float filtered = input - previous_input[bus][channel]
                    + 0.99738543f * previous_output[bus][channel];
                previous_input[bus][channel] = input;
                previous_output[bus][channel] = filtered;
                if ((bus == 0 && !pause) || (bus == 1 && !mute)) {
                    expected += filtered * (bus == 0 ? 0.25f : 0.125f);
                }
            }
            CHECK(fabsf(mixed[channel] - expected) < 0.000001f);
            music |= components[0] != 0;
            effects |= components[1] != 0;
        }
    }
    CHECK(music);
    CHECK(effects);
    M_Close(&fixture, output);
}
