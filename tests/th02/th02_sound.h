#pragma once

#include "tests/test.h"
#include "th02/sound_effects.h"

#include "core/base/arena.h"
#include "pc98/pmd.h"

typedef struct {
    ARENA arena;
    PMD *actual;
    PMD *expected;
} TEST_SOUND;

static inline void TestSound_Init(TEST_SOUND *const sound)
{
    memset(sound, 0, sizeof(*sound));
    Arena_Init(&sound->arena, "TH02 sound test", 4 * 1024 * 1024);
    const ALLOCATOR *const allocator = Arena_Allocator(&sound->arena);
    uint8_t bytes[256 + 127 * 7 + 28] = {};
    for (uint16_t id = 0; id < 127; id++) {
        const uint16_t offset = (uint16_t)(256 + id * 7);
        bytes[id * 2] = (uint8_t)offset;
        bytes[id * 2 + 1] = (uint8_t)(offset >> 8);
        const uint8_t code[] = {
            0xff, 0,    0xfd, 127, (uint8_t)(((2 + id / 12) << 4) | (id % 12)),
            255,  0x80,
        };
        memcpy(bytes + offset, code, sizeof(code));
    }
    const uint16_t instrument = 256 + 127 * 7;
    bytes[254] = (uint8_t)instrument;
    bytes[255] = (uint8_t)(instrument >> 8);
    memset(bytes + instrument + 1, 1, 4);
    memset(bytes + instrument + 9, 31, 4);
    memset(bytes + instrument + 21, 15, 4);
    bytes[instrument + 25] = 7;
    bytes[instrument + 27] = 0xff;
    READER reader;
    Reader_OpenBuffer(&reader, bytes, sizeof(bytes), nullptr);
    PMD_EFFECTS *bank;
    CHECK(Result_Ignore(PmdData_LoadEffects(&reader, allocator, &bank)));
    Reader_Close(&reader);
    CHECK(Result_Ignore(
        Pmd_Init(bank, PMD_BOARD_86, 200, allocator, &sound->actual)));
    CHECK(Result_Ignore(
        Pmd_Init(bank, PMD_BOARD_86, 200, allocator, &sound->expected)));
    SoundEffects_Init(sound->actual);
}

static inline void TestSound_Check(TEST_SOUND *const sound, const int id)
{
    if (id >= 0) {
        Pmd_Effect(sound->expected, (uint8_t)id);
    }
    OPNA_FRAME actual[800];
    OPNA_FRAME expected[800];
    Pmd_Render(sound->actual, actual, ARRAY_SIZE(actual));
    Pmd_Render(sound->expected, expected, ARRAY_SIZE(expected));
    CHECK(memcmp(actual, expected, sizeof(actual)) == 0);
}

static inline void TestSound_Free(TEST_SOUND *const sound)
{
    SoundEffects_Init(nullptr);
    Arena_Free(&sound->arena);
}

static inline uint64_t TestSound_Hash(TEST_SOUND *const sound, uint64_t hash)
{
    OPNA_FRAME frames[800];
    Pmd_Render(sound->actual, frames, ARRAY_SIZE(frames));
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
    return hash;
}
