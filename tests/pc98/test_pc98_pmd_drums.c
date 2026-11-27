#include "tests/test.h"

#include "core/base/arena.h"
#include "pc98/pmd/drums.h"

#include <string.h>

typedef struct {
    SSG_EFFECT_BANK bank;
    SSG_EFFECTS effects;
    ARENA arena;
    PMD_CHIP chip;
} FIXTURE;

static const uint8_t m_Table[] = {
    1, 6,    0,   2, 18, 0, 3, 254, 255, 31, 1, 15, 1,   2,    9,
    2, 0x11, 255, 0, 5,  0, 0, 0,   8,   0,  0, 0,  255, 0xf0, 255,
};

static void M_Init(FIXTURE *const fixture)
{
    memset(fixture, 0, sizeof(*fixture));
    fixture->bank = (SSG_EFFECT_BANK) {
        .bytes = m_Table,
        .size = sizeof(m_Table),
        .count = 2,
        .priorities = { 1, 2 },
        .offsets = { 6, 18 },
    };
    Arena_Init(&fixture->arena, "SSG effects test", 4 * 1024 * 1024);
    PmdChip_Init(
        &fixture->chip, Arena_Allocator(&fixture->arena), nullptr, nullptr);
    PmdChip_Write(&fixture->chip, 7, 0xbf);
    SsgEffects_Init(&fixture->effects, &fixture->bank, &fixture->chip);
}

TEST(first_frame_sweeps_and_stop_preserve_shared_mixer_channels)
{
    FIXTURE fixture;
    M_Init(&fixture);
    CHECK(Result_Ignore(SsgEffects_Play(&fixture.effects, 0)));
    const uint8_t regs[] = { 4, 5, 6, 7, 10, 11, 12, 13 };
    const uint8_t values[] = { 254, 255, 31, 0x9f, 15, 1, 2, 9 };
    for (size_t i = 0; i < ARRAY_SIZE(regs); i++) {
        CHECK_EQ_INT(fixture.chip.registers[regs[i]], values[i]);
    }
    CHECK_EQ_INT(fixture.chip.last_noise, 31);
    CHECK_EQ_INT(fixture.chip.registers[7], 0x9f);
    CHECK(Result_Ignore(SsgEffects_Step(&fixture.effects)));
    CHECK_EQ_INT(fixture.effects.tone, 0);
    CHECK_EQ_INT(fixture.effects.noise, 32);
    CHECK_EQ_INT(fixture.chip.registers[6], 32);
    CHECK(Result_Ignore(SsgEffects_Step(&fixture.effects)));
    CHECK_EQ_INT(fixture.effects.tone, 2);
    CHECK(Result_Ignore(SsgEffects_Step(&fixture.effects)));
    CHECK_EQ_INT(fixture.effects.priority, 0);
    CHECK_EQ_INT(fixture.effects.id, 255);
    CHECK_EQ_INT(fixture.chip.registers[10], 0);
    CHECK_EQ_INT(fixture.chip.registers[7], 0xbf);
    Arena_Free(&fixture.arena);
}

TEST(priority_rejection_updates_id_and_preserves_the_active_voice)
{
    FIXTURE fixture;
    M_Init(&fixture);
    CHECK(Result_Ignore(SsgEffects_Play(&fixture.effects, 1)));
    uint8_t before[16];
    memcpy(before, fixture.chip.registers, sizeof(before));
    CHECK(Result_Ignore(SsgEffects_Play(&fixture.effects, 0)));
    CHECK_EQ_INT(fixture.effects.priority, 2);
    CHECK_EQ_INT(fixture.effects.id, 0);
    CHECK_EQ_INT(fixture.effects.tone, 5);
    CHECK(memcmp(before, fixture.chip.registers, sizeof(before)) == 0);
    CHECK(!Result_Ignore(SsgEffects_Play(&fixture.effects, 2)));
    CHECK_EQ_INT(fixture.effects.id, 0);
    CHECK(Result_Ignore(SsgEffects_Play(&fixture.effects, 1)));
    CHECK_EQ_INT(fixture.effects.tone, 5);
    CHECK_EQ_INT(fixture.chip.registers[10], 8);
    SsgEffects_Stop(&fixture.effects);
    SsgEffects_Stop(&fixture.effects);
    CHECK_EQ_INT(fixture.chip.registers[7], 0xbf);
    Arena_Free(&fixture.arena);
}

TEST(zero_count_wraps_and_retains_register_and_sweep_state)
{
    FIXTURE fixture;
    M_Init(&fixture);
    CHECK(Result_Ignore(SsgEffects_Play(&fixture.effects, 1)));
    CHECK(Result_Ignore(SsgEffects_Step(&fixture.effects)));
    CHECK_EQ_INT(fixture.effects.count, 255);
    CHECK_EQ_INT(fixture.effects.tone, 4);
    CHECK_EQ_INT(fixture.effects.noise_count, 255);
    for (unsigned i = 0; i < 255; i++) {
        CHECK(Result_Ignore(SsgEffects_Step(&fixture.effects)));
    }
    CHECK_EQ_INT(fixture.effects.priority, 0);
    CHECK_EQ_INT(fixture.chip.registers[10], 0);
    Arena_Free(&fixture.arena);
}

TEST(table_loader_loads_the_built_in_driver_table)
{
    FIXTURE fixture;
    memset(&fixture, 0, sizeof(fixture));
    CHECK(IS_OK(SsgEffects_LoadTable(&fixture.bank)));
    CHECK_EQ_INT(fixture.bank.size, 941);
    CHECK_EQ_INT(fixture.bank.count, 40);
}
