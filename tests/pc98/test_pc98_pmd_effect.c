#include "pc98/pmd/effect.h"

#include "pc98/pmd/data.h"

#include "core/base/arena.h"
#include "test.h"
#include "test_allocator.h"

typedef struct {
    TEST_ALLOCATOR allocator;
    PMD_EFFECTS *bank;
    PMD_EFFECT_PLAYER player;
    ARENA arena;
    PMD_CHIP chip;
} FIXTURE;

static void M_Word(uint8_t *const bytes, const uint16_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
}

static void M_Init(
    FIXTURE *const fixture, const uint8_t *const code, const size_t size)
{
    memset(fixture, 0, sizeof(*fixture));
    TestAllocator_Init(&fixture->allocator);
    uint8_t bytes[512] = {};
    CHECK(size < 228);
    for (size_t i = 0; i < 127; i++) {
        M_Word(bytes + i * 2, 256);
    }
    M_Word(bytes + 2, (uint16_t)(256 + size - 1));
    M_Word(bytes + 254, (uint16_t)(256 + size));
    memcpy(bytes + 256, code, size);
    bytes[256 + size + 25] = 7;
    bytes[256 + size + 27] = 0xff;
    READER reader;
    Reader_OpenBuffer(&reader, bytes, 256 + size + 28, nullptr);
    CHECK(Result_Ignore(PmdData_LoadEffects(
        &reader, &fixture->allocator.allocator, &fixture->bank)));
    Arena_Init(&fixture->arena, "EFC chip test", 4 * 1024 * 1024);
    PmdChip_Init(
        &fixture->chip, Arena_Allocator(&fixture->arena), nullptr, nullptr);
    CHECK(Result_Ignore(PmdEffect_Init(
        &fixture->player, fixture->bank, &fixture->allocator.allocator,
        &fixture->chip, 2)));
    fixture->allocator.allocations_left = 0;
}

static void M_Free(FIXTURE *const fixture)
{
    PmdEffect_Stop(&fixture->player);
    Allocator_Free(
        &fixture->allocator.allocator, fixture->player.work,
        fixture->player.work_size);
    Allocator_Free(
        &fixture->allocator.allocator, fixture->bank,
        sizeof(*fixture->bank) + fixture->bank->size);
    CHECK_EQ_INT(fixture->allocator.live_blocks, 0);
    CHECK_EQ_INT(fixture->allocator.size_errors, 0);
    Arena_Free(&fixture->arena);
}

TEST(autonomous_note_sets_voice_and_stops_after_duration)
{
    const uint8_t code[] = { 0xff, 0, 0xfd, 127, 0x40, 2, 0x80 };
    FIXTURE fixture;
    M_Init(&fixture, code, sizeof(code));
    CHECK(Result_Ignore(PmdEffect_Play(&fixture.player, 0)));
    CHECK_EQ_INT(fixture.player.step, 0);
    CHECK(Result_Ignore(PmdEffect_Step(&fixture.player, 0, false)));
    CHECK_EQ_INT(fixture.player.step, 1);
    CHECK_EQ_INT(fixture.player.channel.volume, 127);
    CHECK_EQ_INT(fixture.player.channel.pitch.note, 0x40);
    CHECK_EQ_INT(fixture.player.channel.fm.key_slots, 0xf0);
    CHECK(Result_Ignore(PmdEffect_Step(&fixture.player, 0, false)));
    CHECK_EQ_INT(fixture.player.step, 2);
    CHECK(Result_Ignore(PmdEffect_Step(&fixture.player, 0, false)));
    CHECK(!fixture.player.playing);
    CHECK_EQ_INT(fixture.player.id, 255);
    CHECK_EQ_INT(fixture.player.channel.fm.key_slots, 0);
    const PMD_PART before = fixture.player.channel;
    CHECK(Result_Ignore(PmdEffect_Step(&fixture.player, 255, true)));
    PmdEffect_Stop(&fixture.player);
    CHECK(memcmp(&before, &fixture.player.channel, sizeof(before)) == 0);
    M_Free(&fixture);
}

TEST(tied_note_retains_keys_and_advances_pitch)
{
    const uint8_t code[] = { 0xff, 0, 0x40, 1, 0xfb, 0x41, 1, 0x80 };
    FIXTURE fixture;
    M_Init(&fixture, code, sizeof(code));
    CHECK(Result_Ignore(PmdEffect_Play(&fixture.player, 0)));
    CHECK(Result_Ignore(PmdEffect_Step(&fixture.player, 0, false)));
    CHECK_EQ_INT(fixture.player.channel.note.key_flags, 2);
    CHECK(Result_Ignore(PmdEffect_Step(&fixture.player, 0, false)));
    CHECK_EQ_INT(fixture.player.channel.fm.key_slots, 0xf0);
    CHECK_EQ_INT(fixture.player.channel.pitch.note, 0x41);
    CHECK(!fixture.player.control.tied);
    M_Free(&fixture);
}

TEST(replacement_keeps_command_work_and_invalid_trigger_keeps_voice)
{
    const uint8_t code[] = {
        0xf9, 6, 1, 0x40, 1, 0xf8, 3, 0, 1, 1, 0x80,
    };
    FIXTURE fixture;
    M_Init(&fixture, code, sizeof(code));
    CHECK(Result_Ignore(PmdEffect_Play(&fixture.player, 0)));
    CHECK(Result_Ignore(PmdEffect_Step(&fixture.player, 0, false)));
    CHECK(Result_Ignore(PmdEffect_Step(&fixture.player, 0, false)));
    CHECK_EQ_INT(fixture.player.work[263], 1);
    CHECK_EQ_INT(fixture.bank->bytes[263], 0);
    const PMD_PART before = fixture.player.channel;
    CHECK(!Result_Ignore(PmdEffect_Play(&fixture.player, 127)));
    CHECK(fixture.player.playing);
    CHECK(memcmp(&before, &fixture.player.channel, sizeof(before)) == 0);
    uint8_t *const work = fixture.player.work;
    CHECK(Result_Ignore(PmdEffect_Play(&fixture.player, 0)));
    CHECK_EQ_INT(fixture.player.channel.fm.key_slots, 0);
    CHECK(fixture.player.work == work);
    CHECK_EQ_INT(work[263], 1);
    CHECK(Result_Ignore(PmdEffect_Step(&fixture.player, 0, false)));
    CHECK_EQ_INT(work[263], 0);
    M_Free(&fixture);
}

TEST(bad_voice_data_stops_with_silence_and_can_restart)
{
    const uint8_t codes[][5] = {
        { 0xff, 9, 0x40, 1, 0x80 },
        { 0x81, 0, 0, 0, 0x80 },
        { 0x4d, 1, 0, 0, 0x80 },
    };
    for (size_t i = 0; i < ARRAY_SIZE(codes); i++) {
        FIXTURE fixture;
        M_Init(&fixture, codes[i], sizeof(codes[i]));
        CHECK(Result_Ignore(PmdEffect_Play(&fixture.player, 0)));
        CHECK(!Result_Ignore(PmdEffect_Step(&fixture.player, 0, false)));
        CHECK(!fixture.player.playing);
        CHECK(fixture.player.failed);
        CHECK_EQ_INT(fixture.player.channel.fm.key_slots, 0);
        CHECK(Result_Ignore(PmdEffect_Play(&fixture.player, 1)));
        CHECK(!fixture.player.failed);
        CHECK(Result_Ignore(PmdEffect_Step(&fixture.player, 0, false)));
        CHECK(!fixture.player.playing);
        M_Free(&fixture);
    }
}
