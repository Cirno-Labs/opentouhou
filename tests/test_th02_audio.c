#include "th02/audio.h"

#include "pc98/pmd.h"

#include "core/audio.h"
#include "core/audio/backend.h"
#include "core/base/arena.h"
#include "test.h"

#include <SDL3/SDL.h>
#include <math.h>

typedef struct {
    ARENA arena;
    PMD *pmd;
} FIXTURE;

static void M_Init(FIXTURE *const fixture)
{
    memset(fixture, 0, sizeof(*fixture));
    Arena_Init(&fixture->arena, "OPN audio", 4 * 1024 * 1024);
    const ALLOCATOR *const allocator = Arena_Allocator(&fixture->arena);
    uint8_t song[67] = {};
    for (size_t part = 0; part < 11; part++) {
        song[1 + part * 2] = part == 6 ? 32 : 26;
    }
    song[23] = 50;
    song[25] = 62;
    song[27] = 0x80;
    const uint8_t stream[] = { 0xfd, 15, 0x40, 255, 0x80 };
    memcpy(song + 33, stream, sizeof(stream));
    song[59] = 64;
    song[61] = 0x48;
    song[62] = 0xfe;
    song[64] = 0xff;
    READER reader;
    Reader_OpenBuffer(&reader, song, sizeof(song), nullptr);
    PMD_SONG *music;
    CHECK(Result_Ignore(PmdData_LoadSong(&reader, allocator, &music)));
    Reader_Close(&reader);
    uint8_t effect[320] = {};
    for (size_t i = 0; i < PMD_EFFECT_COUNT; i++) {
        effect[i * 2 + 1] = 1;
    }
    const uint8_t code[] = { 0xff, 0, 0x43, 255, 0x80 };
    const uint16_t instrument = 256 + sizeof(code);
    effect[254] = (uint8_t)instrument;
    effect[255] = (uint8_t)(instrument >> 8);
    memcpy(effect + 256, code, sizeof(code));
    memset(effect + instrument + 1, 1, 4);
    memset(effect + instrument + 9, 31, 4);
    memset(effect + instrument + 21, 15, 4);
    effect[instrument + 25] = 7;
    effect[instrument + 27] = 0xff;
    Reader_OpenBuffer(&reader, effect, instrument + 28, nullptr);
    PMD_EFFECTS *effects;
    CHECK(Result_Ignore(PmdData_LoadEffects(&reader, allocator, &effects)));
    Reader_Close(&reader);
    CHECK(Result_Ignore(
        Pmd_Init(effects, PMD_BOARD_86, 200, allocator, &fixture->pmd)));
    Pmd_Load(fixture->pmd, music);
    Pmd_Start(fixture->pmd);
    Pmd_Effect(fixture->pmd, 0);
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

static void M_Close(FIXTURE *const fixture)
{
    Arena_Free(&fixture->arena);
}

TEST(headless_render_and_failed_preparation_preserve_timeline)
{
    FIXTURE fixture, reference, fresh;
    M_Init(&fixture);
    M_Init(&reference);
    M_Init(&fresh);
    GAME_AUDIO *output = nullptr;
    CHECK(!Result_Ignore(
        GameAudio_Prepare(Arena_Allocator(&fixture.arena), &output)));
    CHECK(output == nullptr);
    GameAudio_Free(output);
    CHECK(Result_Ignore(GameAudio_Render(nullptr, fixture.pmd, 1600)));
    OPNA_FRAME skipped[1600], actual[800], expected[800], beginning[800];
    Pmd_Render(reference.pmd, skipped, ARRAY_SIZE(skipped));
    Pmd_Render(fixture.pmd, actual, ARRAY_SIZE(actual));
    Pmd_Render(reference.pmd, expected, ARRAY_SIZE(expected));
    Pmd_Render(fresh.pmd, beginning, ARRAY_SIZE(beginning));
    CHECK(memcmp(actual, expected, sizeof(actual)) == 0);
    CHECK(memcmp(actual, beginning, sizeof(actual)) != 0);
    M_Close(&reference);
    M_Close(&fresh);
    M_Close(&fixture);
}

TEST(shared_pcm_reaches_mixer_with_gain_and_continuous_filter_history)
{
    CHECK(Result_Ignore(Audio_Init()));
    FIXTURE actual, reference;
    M_Init(&actual);
    M_Init(&reference);
    GAME_AUDIO *output;
    CHECK(Result_Ignore(
        GameAudio_Prepare(Arena_Allocator(&actual.arena), &output)));
    CHECK(Result_Ignore(Audio_SetMasterVolume(0.5f)));
    CHECK(Result_Ignore(Audio_SetGroupVolume(AUDIO_GROUP_MUSIC, 0.25f)));
    float previous_input[2] = {}, previous_output[2] = {};
    bool fm = false, ssg = false;
    const size_t counts[] = { 0, 1, 17, 1600, 139, 900, 800 };
    for (size_t block = 0; block < ARRAY_SIZE(counts); block++) {
        const size_t count = counts[block];
        OPNA_FRAME raw[1600];
        float mixed[3200];
        if (block == ARRAY_SIZE(counts) - 1) {
            Pmd_Stop(actual.pmd);
            Pmd_Stop(reference.pmd);
            fm = false;
        }
        Pmd_Render(reference.pmd, raw, count);
        CHECK(Result_Ignore(GameAudio_Render(output, actual.pmd, count)));
        M_Mix(mixed, (uint32_t)count);
        for (size_t i = 0; i < count; i++) {
            const int32_t tone = raw[i].ssg[0] + raw[i].ssg[1] + raw[i].ssg[2];
            ssg |= tone != 0;
            for (size_t channel = 0; channel < 2; channel++) {
                fm |= raw[i].fm_rhythm[channel] != 0;
                const float input =
                    (float)(raw[i].fm_rhythm[channel] + tone) / 65536.0f;
                const float filtered = input - previous_input[channel]
                    + 0.99738543f * previous_output[channel];
                previous_input[channel] = input;
                previous_output[channel] = filtered;
                CHECK(isfinite(mixed[i * 2 + channel]));
                CHECK(
                    fabsf(mixed[i * 2 + channel] - filtered * 0.125f)
                    < 0.000001f);
            }
        }
    }
    CHECK(fm && ssg);
    GameAudio_Free(output);
    Audio_Shutdown();
    M_Close(&actual);
    M_Close(&reference);
}

TEST(synthetic_music_and_effect_pcm_is_stable_through_fades_and_restarts)
{
    FIXTURE fixture;
    M_Init(&fixture);
    uint64_t hash = 14695981039346656037ULL;
    bool fm = false, ssg = false;
    for (size_t tick = 0; tick < 120; tick++) {
        if (tick == 30) {
            Pmd_Fade(fixture.pmd, 5);
        } else if (tick == 60) {
            Pmd_Start(fixture.pmd);
            Pmd_Effect(fixture.pmd, 0);
        } else if (tick == 90) {
            Pmd_Stop(fixture.pmd);
        }
        OPNA_FRAME frames[800];
        Pmd_Render(fixture.pmd, frames, ARRAY_SIZE(frames));
        for (size_t i = 0; i < ARRAY_SIZE(frames); i++) {
            for (size_t channel = 0; channel < 2; channel++) {
                const int32_t sample = frames[i].fm_rhythm[channel];
                fm |= sample != 0;
                hash = (hash ^ (uint32_t)sample) * 1099511628211ULL;
            }
            for (size_t channel = 0; channel < 3; channel++) {
                const int32_t sample = frames[i].ssg[channel];
                ssg |= sample != 0;
                hash = (hash ^ (uint32_t)sample) * 1099511628211ULL;
            }
        }
    }
    CHECK(fm && ssg);
    CHECK(hash == UINT64_C(16500819778359123827));
    M_Close(&fixture);
}
