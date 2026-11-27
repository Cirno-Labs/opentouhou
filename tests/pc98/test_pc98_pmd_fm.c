#include "tests/test.h"

#include "pc98/pmd/registers.h"

TEST(program_caches_instrument_and_clears_keys_for_muted_voices)
{
    PMD_FM voice;
    PmdFm_Init(&voice, 2);
    uint8_t parameters[25];
    for (uint8_t i = 0; i < 25; i++) {
        parameters[i] = i;
    }
    parameters[24] = 0x2c;
    PmdFm_KeyOn(&voice, nullptr);
    PmdFm_Program(&voice, parameters, nullptr);
    CHECK_EQ_INT(voice.carriers, 0xa0);
    CHECK_EQ_INT(voice.algorithm_feedback, 0x2c);
    CHECK(memcmp(voice.levels, parameters + 4, 4) == 0);
    CHECK_EQ_INT(voice.key_slots, 0);
}

TEST(volume_maps_carrier_slots_and_retains_cached_levels)
{
    PMD_FM voice;
    PmdFm_Init(&voice, 0);
    const uint8_t parameters[25] = {
        [4] = 1, [5] = 2, [6] = 3, [7] = 4, [24] = 7,
    };
    PmdFm_Program(&voice, parameters, nullptr);
    const PMD_FM before = voice;
    const uint8_t values[] = { 4, 2, 3, 1 };
    for (uint8_t slot = 0; slot < 4; slot++) {
        CHECK_EQ_INT(PmdFm_Level(&voice, 127, nullptr, slot), values[slot]);
    }
    CHECK(memcmp(&voice, &before, sizeof(voice)) == 0);
}

TEST(volume_zero_ignores_modulation_and_byte_modulation_saturates_in_order)
{
    PMD_FM voice;
    PmdFm_Init(&voice, 2);
    const uint8_t parameters[25] = { [24] = 0 };
    PmdFm_Program(&voice, parameters, nullptr);
    PMD_FM_MODULATION modulation[2] = {
        { .amount = 127, .slots = 0xff, .enabled = true },
        { .amount = -128, .slots = 0xff, .enabled = true },
    };
    for (uint8_t slot = 0; slot < 4; slot++) {
        CHECK_EQ_INT(
            PmdFm_Level(&voice, 0, modulation, slot), slot == 0 ? 127 : -1);
        CHECK_EQ_INT(PmdFm_Level(&voice, 127, modulation, slot), 1);
    }
    modulation[0].amount = 256;
    modulation[1].enabled = false;
    for (uint8_t slot = 0; slot < 4; slot++) {
        CHECK_EQ_INT(PmdFm_Level(&voice, 127, modulation, slot), 0);
    }
}

TEST(silence_retains_the_banked_instrument_and_clears_keys)
{
    PMD_FM voice;
    PmdFm_Init(&voice, 5);
    const uint8_t parameters[25] = { [4] = 20, [24] = 0x3f };
    PmdFm_Program(&voice, parameters, nullptr);
    PmdFm_KeyOn(&voice, nullptr);
    CHECK_EQ_INT(voice.key_slots, 0xf0);
    PmdFm_KeyOff(&voice, nullptr);
    PmdFm_KeyOff(&voice, nullptr);
    CHECK_EQ_INT(voice.key_slots, 0);
    PmdFm_Silence(&voice, nullptr);
    CHECK_EQ_INT(voice.algorithm_feedback, 0x3f);
    CHECK_EQ_INT(voice.levels[0], 20);
    CHECK_EQ_INT(voice.key_slots, 0);
}
