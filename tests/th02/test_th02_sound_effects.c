#include "tests/test.h"
#include "tests/th02/th02_sound.h"
#include "th02/audio_clock.h"
#include "th02/sound_effects.h"

#include <string.h>

TEST(original_priority_matrix_reaches_the_driver_at_the_update_boundary)
{
    TEST_SOUND sound;
    TestSound_Init(&sound);
    const uint8_t priorities[33] = {
        0, 0, 32, 9, 2, 16, 5, 6, 48, 8, 9, 5, 16, 10, 17, 12, 32, 32, 16,
    };
    for (int16_t first = 0; first < 33; first++) {
        for (int16_t second = 0; second < 33; second++) {
            SoundEffects_Reset();
            SoundEffects_Play(first);
            SoundEffects_Play(second);
            TestSound_Check(&sound, -1);
            SoundEffects_Update();
            TestSound_Check(
                &sound,
                priorities[first] <= priorities[second] ? second : first);
        }
    }
    TestSound_Free(&sound);
}

TEST(original_hold_frames_reject_low_priority_until_the_expiry_update)
{
    TEST_SOUND sound;
    TestSound_Init(&sound);
    const uint8_t holds[19] = {
        0, 0, 36, 16, 4, 16, 8, 10, 48, 80, 24, 17, 11, 4, 80, 16, 48, 74, 32,
    };
    for (int16_t id = 2; id <= 18; id++) {
        SoundEffects_Reset();
        SoundEffects_Play(id);
        SoundEffects_Update();
        TestSound_Check(&sound, id);
        for (uint8_t frame = 1; frame <= holds[id]; frame++) {
            SoundEffects_Play(0);
            SoundEffects_Update();
            TestSound_Check(&sound, -1);
        }
        SoundEffects_Play(0);
        SoundEffects_Update();
        TestSound_Check(&sound, 0);
    }
    TestSound_Free(&sound);
}

TEST(equal_priority_restarts_and_reset_keeps_the_driver_voice)
{
    TEST_SOUND sound;
    TestSound_Init(&sound);
    SoundEffects_Play(SOUND_EFFECT_BOSS_HEAVY_HIT);
    TestSound_Check(&sound, -1);
    SoundEffects_Update();
    TestSound_Check(&sound, 2);
    SoundEffects_Play(SOUND_EFFECT_SHOT);
    SoundEffects_Update();
    TestSound_Check(&sound, -1);
    SoundEffects_Play(SOUND_EFFECT_BOMB_2);
    SoundEffects_Update();
    TestSound_Check(&sound, 16);
    SoundEffects_Reset();
    SoundEffects_Update();
    TestSound_Check(&sound, -1);
    TestSound_Free(&sound);
}

TEST(force_starts_immediately_and_overrides_higher_priority)
{
    TEST_SOUND sound;
    TestSound_Init(&sound);
    SoundEffects_Play(SOUND_EFFECT_EXTEND);
    SoundEffects_Force(SOUND_EFFECT_SHOT);
    TestSound_Check(&sound, 1);
    SoundEffects_Update();
    TestSound_Check(&sound, -1);
    TestSound_Free(&sound);
}

TEST(unavailable_fm_ignores_requests_before_validating_the_id)
{
    TEST_SOUND sound;
    TestSound_Init(&sound);
    SoundEffects_Init(nullptr);
    SoundEffects_Play(INT16_MIN);
    SoundEffects_Update();
    SoundEffects_Force(INT16_MAX);
    TestSound_Check(&sound, -1);
    SoundEffects_Init(sound.actual);
    SoundEffects_Play(SOUND_EFFECT_BOSS_HEAVY_HIT);
    SoundEffects_Update();
    TestSound_Check(&sound, 2);
    TestSound_Free(&sound);
}

TEST(measured_effect_completions_advance_driver_and_fade_before_playback)
{
    TEST_SOUND sound;
    TestSound_Init(&sound);
    MUSIC_WAIT wait;
    MUSIC_WAIT expected_wait;
    MusicWait_Init(&wait);
    MusicWait_Init(&expected_wait);
    AudioClock_Init(sound.actual, nullptr, &wait);
    const TH02_CLOCK_SAMPLE clocks[] = {
        { .key = TH02_CLOCK_FRAME, .microseconds = 1000000 },
        { .key = TH02_CLOCK_ORIGIN, .microseconds = 1000000 },
        { .key = TH02_CLOCK_EFFECT_KEY, .microseconds = 1010000 },
        { .key = TH02_CLOCK_EFFECT_KEY + 1, .microseconds = 1020000 },
    };
    AudioClock_Begin(clocks, ARRAY_SIZE(clocks));
    SoundEffects_Force(SOUND_EFFECT_BOSS_HEAVY_HIT);
    Pmd_Render(sound.expected, nullptr, 480);
    Pmd_Effect(sound.expected, 2);
    SoundEffects_Force(SOUND_EFFECT_BOMB_2);
    Pmd_Render(sound.expected, nullptr, 480);
    Pmd_Effect(sound.expected, 16);
    TestSound_Check(&sound, -1);
    MusicWait_Advance(&expected_wait, 960);
    CHECK(memcmp(&wait, &expected_wait, sizeof(wait)) == 0);
    CHECK(!AudioClock_Failed());
    AudioClock_Init(nullptr, nullptr, nullptr);
    TestSound_Free(&sound);
}

TEST(legacy_clocks_do_not_advance_effects_without_completion_samples)
{
    TEST_SOUND sound;
    TestSound_Init(&sound);
    MUSIC_WAIT wait;
    MusicWait_Init(&wait);
    AudioClock_Init(sound.actual, nullptr, &wait);
    const TH02_CLOCK_SAMPLE clocks[] = {
        { .key = TH02_CLOCK_FRAME, .microseconds = 1000000 },
        { .key = TH02_CLOCK_ORIGIN, .microseconds = 1000000 },
    };
    AudioClock_Begin(clocks, ARRAY_SIZE(clocks));
    SoundEffects_Force(SOUND_EFFECT_BOSS_HEAVY_HIT);
    TestSound_Check(&sound, 2);
    CHECK(!AudioClock_Failed());
    AudioClock_Init(nullptr, nullptr, nullptr);
    TestSound_Free(&sound);
}

TEST(measured_clock_requires_its_origin)
{
    TEST_SOUND sound;
    TestSound_Init(&sound);
    MUSIC_WAIT wait;
    MusicWait_Init(&wait);
    AudioClock_Init(sound.actual, nullptr, &wait);
    const TH02_CLOCK_SAMPLE clock = { .key = TH02_CLOCK_FRAME,
                                      .microseconds = 1000000 };
    AudioClock_Begin(&clock, 1);
    CHECK(AudioClock_Failed());
    CHECK(!Result_Ignore(AudioClock_TakeError()));
    CHECK(!AudioClock_Failed());
    AudioClock_Init(nullptr, nullptr, nullptr);
    TestSound_Free(&sound);
}

TEST(effect_entry_precedes_pending_interrupts_in_the_completion_sample)
{
    TEST_SOUND sound;
    TestSound_Init(&sound);
    MUSIC_WAIT wait;
    MUSIC_WAIT expected_wait;
    MusicWait_Init(&wait);
    MusicWait_Init(&expected_wait);
    AudioClock_Init(sound.actual, nullptr, &wait);
    const TH02_CLOCK_SAMPLE clocks[] = {
        { .key = TH02_CLOCK_FRAME, .microseconds = 1000000 },
        { .key = TH02_CLOCK_ORIGIN, .microseconds = 1000000 },
        { .key = TH02_CLOCK_EFFECT_ENTRY_KEY, .microseconds = 1010000 },
        { .key = TH02_CLOCK_EFFECT_KEY, .microseconds = 1020000 },
    };
    AudioClock_Begin(clocks, ARRAY_SIZE(clocks));
    SoundEffects_Force(SOUND_EFFECT_BOSS_HEAVY_HIT);
    Pmd_Render(sound.expected, nullptr, 480);
    TestSound_Check(&sound, 2);
    MusicWait_Advance(&expected_wait, 480);
    CHECK(memcmp(&wait, &expected_wait, sizeof(wait)) == 0);
    CHECK(!AudioClock_Failed());
    AudioClock_Init(nullptr, nullptr, nullptr);
    TestSound_Free(&sound);
}
