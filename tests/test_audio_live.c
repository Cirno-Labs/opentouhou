#include "core/audio.h"

#include "core/audio/backend.h"

#include "test.h"

#include <SDL3/SDL.h>
#include <math.h>
#include <stdatomic.h>

#define WAIT_MILLISECONDS 5000

typedef struct {
    atomic_bool stop;
    atomic_uint mixes;
    atomic_uint bad_samples;
    atomic_uint queued_frames;
} MIX_CHECK;

static void M_CheckResult(RESULT result)
{
    CHECK(Result_Log(result, LOG_LEVEL_ERROR));
}

static void M_CheckNear(const float actual, const float expected)
{
    if (!isfinite(actual) || fabsf(actual - expected) > 0.00001f) {
        TEST_FAIL("expected %.8f, got %.8f", expected, actual);
    }
}

static void M_Mix(float *const out, const uint32_t frames)
{
    const Uint64 deadline = SDL_GetTicks() + WAIT_MILLISECONDS;
    while (!AudioBackend_Mix(out, frames)) {
        if (SDL_GetTicks() >= deadline) {
            TEST_FAIL("audio mixer did not acquire its state lock");
            return;
        }
        SDL_Delay(1);
    }
}

static AUDIO_LIVE *M_Create(const uint32_t capacity)
{
    const AUDIO_GROUP groups[] = { AUDIO_GROUP_MUSIC, AUDIO_GROUP_EFFECTS };
    AUDIO_LIVE *input = nullptr;
    M_CheckResult(AudioLive_Create(groups, 2, capacity, &input));
    return input;
}

static void M_Submit(
    AUDIO_LIVE *const input, const float *const music,
    const float *const effects, const uint32_t frames)
{
    const float *const buses[] = { music, effects };
    M_CheckResult(AudioLive_Submit(input, buses, frames));
}

static void M_CheckFrames(
    const float *const out, const uint32_t first, const uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) {
        M_CheckNear(out[i * 2], (float)(first + i));
        M_CheckNear(out[i * 2 + 1], (float)(first + i + 10));
    }
}

static int M_CheckMixThread(void *const context)
{
    MIX_CHECK *const check = context;
    while (!atomic_load(&check->stop)) {
        float out[34];
        if (AudioBackend_Mix(out, 17)) {
            for (uint32_t i = 0; i < 17; i++) {
                if (out[i * 2] != 0.0f
                    || (out[i * 2 + 1] != 0.0f && out[i * 2 + 1] != 1.0f)) {
                    atomic_fetch_add(&check->bad_samples, 1);
                }
                if (out[i * 2 + 1] == 1.0f) {
                    atomic_fetch_add(&check->queued_frames, 1);
                }
            }
            atomic_fetch_add(&check->mixes, 1);
        }
        SDL_Delay(1);
    }
    return 0;
}

static void M_WaitMixes(MIX_CHECK *const check, const unsigned target)
{
    const Uint64 deadline = SDL_GetTicks() + WAIT_MILLISECONDS;
    while (atomic_load(&check->mixes) < target && SDL_GetTicks() < deadline) {
        SDL_Delay(1);
    }
    CHECK(atomic_load(&check->mixes) >= target);
}

TEST(create_checks_initialization_groups_and_capacity)
{
    AUDIO_LIVE *input = nullptr;
    AUDIO_GROUP groups[] = { AUDIO_GROUP_MUSIC, AUDIO_GROUP_EFFECTS,
                             AUDIO_GROUP_INTERFACE };
    CHECK(!Result_Ignore(AudioLive_Create(groups, 2, 4, &input)));
    CHECK(input == nullptr);
    CHECK(!Result_Ignore(AudioLive_Submit(nullptr, nullptr, 0)));
    AudioLive_Free(nullptr);
    AudioLive_Clear(nullptr);
    M_CheckResult(Audio_Init());
    CHECK(!Result_Ignore(AudioLive_Create(groups, 2, 4, nullptr)));
    CHECK(!Result_Ignore(AudioLive_Create(nullptr, 2, 4, &input)));
    CHECK(!Result_Ignore(AudioLive_Create(groups, 0, 4, &input)));
    CHECK(!Result_Ignore(AudioLive_Create(groups, 4, 4, &input)));
    CHECK(!Result_Ignore(AudioLive_Create(groups, 2, 0, &input)));
    groups[1] = groups[0];
    CHECK(!Result_Ignore(AudioLive_Create(groups, 2, 4, &input)));
    groups[1] = (AUDIO_GROUP)-1;
    CHECK(!Result_Ignore(AudioLive_Create(groups, 2, 4, &input)));
    groups[1] = (AUDIO_GROUP)3;
    CHECK(!Result_Ignore(AudioLive_Create(groups, 2, 4, &input)));
    CHECK(input == nullptr);
    float out[2] = { 1.0f, 1.0f };
    M_Mix(out, 1);
    M_CheckNear(out[0], 0.0f);
    M_CheckNear(out[1], 0.0f);
    Audio_Shutdown();
}

TEST(submit_copies_samples_and_group_descriptors)
{
    M_CheckResult(Audio_Init());
    AUDIO_GROUP groups[] = { AUDIO_GROUP_MUSIC, AUDIO_GROUP_EFFECTS };
    AUDIO_LIVE *input = nullptr;
    M_CheckResult(AudioLive_Create(groups, 2, 2, &input));
    groups[0] = AUDIO_GROUP_INTERFACE;
    groups[1] = AUDIO_GROUP_INTERFACE;
    float music[] = { 0.5f, 0.25f, -0.5f, -0.25f };
    float effects[] = { 0.25f, -0.5f, -0.25f, 0.5f };
    M_Submit(input, music, effects, 2);
    memset(music, 0, sizeof(music));
    memset(effects, 0, sizeof(effects));
    M_CheckResult(Audio_SetMasterVolume(0.5f));
    M_CheckResult(Audio_SetGroupVolume(AUDIO_GROUP_MUSIC, 0.5f));
    M_CheckResult(Audio_SetGroupVolume(AUDIO_GROUP_EFFECTS, 0.25f));
    M_CheckResult(Audio_SetGroupVolume(AUDIO_GROUP_INTERFACE, 0.0f));
    float out[4];
    M_Mix(out, 2);
    M_CheckNear(out[0], 0.15625f);
    M_CheckNear(out[1], 0.0f);
    M_CheckNear(out[2], -0.15625f);
    M_CheckNear(out[3], 0.0f);
    AudioLive_Free(input);
    Audio_Shutdown();
}

TEST(three_buses_stay_synchronized_through_pause_and_mute)
{
    M_CheckResult(Audio_Init());
    const AUDIO_GROUP groups[] = { AUDIO_GROUP_MUSIC, AUDIO_GROUP_EFFECTS,
                                   AUDIO_GROUP_INTERFACE };
    AUDIO_LIVE *input = nullptr;
    M_CheckResult(AudioLive_Create(groups, 3, 4, &input));
    const float music[] = { 1, 1, 2, 2, 3, 3, 4, 4 };
    const float effects[] = { 10, 10, 20, 20, 30, 30, 40, 40 };
    const float ui[] = { 100, 100, 200, 200, 300, 300, 400, 400 };
    const float *const buses[] = { music, effects, ui };
    M_CheckResult(AudioLive_Submit(input, buses, 4));
    float out[2];
    Audio_SetGroupPaused(AUDIO_GROUP_MUSIC, true);
    M_Mix(out, 1);
    M_CheckNear(out[0], 110);
    M_CheckNear(out[1], 110);
    Audio_SetGroupPaused(AUDIO_GROUP_MUSIC, false);
    Audio_SetGroupMuted(AUDIO_GROUP_EFFECTS, true);
    M_Mix(out, 1);
    M_CheckNear(out[0], 202);
    M_CheckNear(out[1], 202);
    Audio_SetMuted(true);
    M_Mix(out, 1);
    M_CheckNear(out[0], 0);
    M_CheckNear(out[1], 0);
    Audio_SetMuted(false);
    Audio_SetGroupMuted(AUDIO_GROUP_EFFECTS, false);
    M_Mix(out, 1);
    M_CheckNear(out[0], 444);
    M_CheckNear(out[1], 444);
    AudioLive_Free(input);
    Audio_Shutdown();
}

TEST(ring_wrap_preserves_order_and_drops_oldest_frames_together)
{
    M_CheckResult(Audio_Init());
    AUDIO_LIVE *const input = M_Create(4);
    const float music[] = { 1, 0, 2, 0, 3, 0, 4,  0, 5,  0, 6,  0,
                            7, 0, 8, 0, 9, 0, 10, 0, 11, 0, 12, 0 };
    const float effects[] = { 0, 11, 0, 12, 0, 13, 0, 14, 0, 15, 0, 16,
                              0, 17, 0, 18, 0, 19, 0, 20, 0, 21, 0, 22 };
    M_Submit(input, music, effects, 3);
    float out[8];
    M_Mix(out, 2);
    M_CheckFrames(out, 1, 2);
    M_Submit(input, music + 6, effects + 6, 3);
    M_Mix(out, 4);
    M_CheckFrames(out, 3, 4);
    M_Submit(input, music + 12, effects + 12, 3);
    M_Submit(input, music + 18, effects + 18, 3);
    M_Mix(out, 4);
    M_CheckFrames(out, 9, 4);
    AudioLive_Free(input);
    Audio_Shutdown();
}

TEST(oversized_submit_replaces_backlog_with_newest_frames)
{
    M_CheckResult(Audio_Init());
    AUDIO_LIVE *const input = M_Create(3);
    const float music[] = { 1, 0, 2, 0, 3, 0, 4, 0, 5, 0, 6, 0 };
    const float effects[] = { 0, 11, 0, 12, 0, 13, 0, 14, 0, 15, 0, 16 };
    M_Submit(input, music, effects, 2);
    M_Submit(input, music, effects, 6);
    float out[6];
    M_Mix(out, 3);
    M_CheckFrames(out, 4, 3);
    AudioLive_Free(input);
    Audio_Shutdown();
}

TEST(underrun_is_silent_and_new_frames_start_together)
{
    M_CheckResult(Audio_Init());
    AUDIO_LIVE *const input = M_Create(2);
    const float music[] = { 1, 0, 2, 0 };
    const float effects[] = { 0, 11, 0, 12 };
    M_Submit(input, music, effects, 1);
    float out[6];
    M_Mix(out, 3);
    M_CheckFrames(out, 1, 1);
    for (int i = 2; i < 6; i++) {
        M_CheckNear(out[i], 0);
    }
    M_Submit(input, music + 2, effects + 2, 1);
    M_Mix(out, 2);
    M_CheckFrames(out, 2, 1);
    M_CheckNear(out[2], 0);
    M_CheckNear(out[3], 0);
    AudioLive_Free(input);
    Audio_Shutdown();
}

TEST(invalid_bus_or_sample_leaves_every_queued_bus_unchanged)
{
    M_CheckResult(Audio_Init());
    AUDIO_LIVE *const input = M_Create(2);
    const float music[] = { 1, 0, 2, 0, 3, 0 };
    const float effects[] = { 0, 11, 0, 12, 0, 13 };
    M_Submit(input, music, effects, 2);
    CHECK(!Result_Ignore(AudioLive_Submit(nullptr, nullptr, 1)));
    CHECK(!Result_Ignore(AudioLive_Submit(input, nullptr, 1)));
    const float *buses[] = { music, nullptr };
    CHECK(!Result_Ignore(AudioLive_Submit(input, buses, 1)));
    float invalid[] = { NAN, 0, 0, 12, 0, 13 };
    buses[1] = invalid;
    CHECK(!Result_Ignore(AudioLive_Submit(input, buses, 3)));
    invalid[0] = INFINITY;
    CHECK(!Result_Ignore(AudioLive_Submit(input, buses, 1)));
    invalid[0] = -INFINITY;
    CHECK(!Result_Ignore(AudioLive_Submit(input, buses, 1)));
    M_CheckResult(AudioLive_Submit(input, nullptr, 0));
    float out[4];
    M_Mix(out, 2);
    M_CheckFrames(out, 1, 2);
    AudioLive_Free(input);
    Audio_Shutdown();
}

TEST(clear_free_and_shutdown_release_independent_inputs)
{
    M_CheckResult(Audio_Init());
    AUDIO_LIVE *const first = M_Create(2);
    AUDIO_LIVE *const middle = M_Create(2);
    AUDIO_LIVE *const last = M_Create(2);
    const float music[] = { 1, 0 };
    const float effects[] = { 0, 11 };
    M_Submit(first, music, effects, 1);
    M_Submit(middle, music, effects, 1);
    M_Submit(last, music, effects, 1);
    AudioLive_Clear(first);
    AudioLive_Free(middle);
    float out[2];
    M_Mix(out, 1);
    M_CheckFrames(out, 1, 1);
    M_Submit(first, music, effects, 1);
    AudioLive_Free(last);
    M_Mix(out, 1);
    M_CheckFrames(out, 1, 1);
    M_Submit(first, music, effects, 1);
    Audio_Shutdown();
    M_CheckResult(Audio_Init());
    M_Mix(out, 1);
    M_CheckNear(out[0], 0);
    M_CheckNear(out[1], 0);
    const AUDIO_GROUP group = AUDIO_GROUP_INTERFACE;
    AUDIO_LIVE *single = nullptr;
    M_CheckResult(AudioLive_Create(&group, 1, 1, &single));
    const float *const bus[] = { music };
    M_CheckResult(AudioLive_Submit(single, bus, 1));
    M_Mix(out, 1);
    M_CheckNear(out[0], 1);
    M_CheckNear(out[1], 0);
    AudioLive_Free(single);
    Audio_Shutdown();
    AudioLive_Free(nullptr);
}

TEST(submit_clear_and_free_share_the_actual_mixer_lock)
{
    M_CheckResult(Audio_Init());
    AUDIO_LIVE *const input = M_Create(64);
    MIX_CHECK check = {};
    SDL_Thread *const thread =
        SDL_CreateThread(M_CheckMixThread, "PCM mixer test", &check);
    CHECK(thread != nullptr);
    if (thread == nullptr) {
        AudioLive_Free(input);
        Audio_Shutdown();
        return;
    }
    M_WaitMixes(&check, 1);
    float music[16];
    float effects[16];
    for (uint32_t step = 0; step < 1000; step++) {
        for (uint32_t i = 0; i < 8; i++) {
            music[i * 2] = (float)(step % 113 + 1);
            music[i * 2 + 1] = 1.0f;
            effects[i * 2] = -music[i * 2];
            effects[i * 2 + 1] = 0.0f;
        }
        M_Submit(input, music, effects, 8);
        if (step % 50 == 0) {
            M_WaitMixes(&check, atomic_load(&check.mixes) + 1);
        }
        if (step % 17 == 0) {
            AudioLive_Clear(input);
        }
    }
    AudioLive_Free(input);
    const unsigned target = atomic_load(&check.mixes) + 4;
    M_WaitMixes(&check, target);
    atomic_store(&check.stop, true);
    SDL_WaitThread(thread, nullptr);
    CHECK_EQ_INT(atomic_load(&check.bad_samples), 0);
    CHECK(atomic_load(&check.queued_frames) != 0);
    Audio_Shutdown();
}
