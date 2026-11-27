#include "tests/test.h"
#include "tests/test_allocator.h"

#include "pc98/pmd/data.h"
#include "pc98/pmd/effect.h"

typedef struct {
    TEST_ALLOCATOR allocator;
    PMD_EFFECTS *bank;
    PMD_EFFECT_PLAYER player;
} FIXTURE;

static void M_Init(
    FIXTURE *const fixture, const uint8_t *const code, const size_t size)
{
    memset(fixture, 0, sizeof(*fixture));
    TestAllocator_Init(&fixture->allocator);
    uint8_t bytes[1024] = {};
    CHECK(size <= sizeof(bytes) - 284);
    for (size_t i = 0; i < 127; i++) {
        bytes[i * 2 + 1] = 1;
    }
    const uint16_t instrument = (uint16_t)(256 + size);
    bytes[254] = (uint8_t)instrument;
    bytes[255] = (uint8_t)(instrument >> 8);
    memcpy(bytes + 256, code, size);
    bytes[instrument] = 7;
    bytes[instrument + 25] = 7;
    bytes[instrument + 27] = 0xff;
    READER reader;
    Reader_OpenBuffer(&reader, bytes, size + 284, nullptr);
    CHECK(Result_Ignore(PmdData_LoadEffects(
        &reader, &fixture->allocator.allocator, &fixture->bank)));
    Reader_Close(&reader);
    CHECK(Result_Ignore(PmdEffect_Init(
        &fixture->player, fixture->bank, &fixture->allocator.allocator, nullptr,
        2)));
    CHECK(Result_Ignore(PmdEffect_Play(&fixture->player, 0)));
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
}

static void M_Step(FIXTURE *const fixture, const size_t count)
{
    for (size_t i = 0; i < count; i++) {
        CHECK(Result_Log(
            PmdEffect_Step(&fixture->player, 0, false), LOG_LEVEL_ERROR));
    }
}

TEST(commands_apply_to_the_part_and_zero_duration_counts_256_steps)
{
    const uint8_t code[] = {
        0xff, 7,    0xfd, 100,  0xf2, 1,    2, 3,    4, 0xf1,
        1,    0xf5, 12,   0xf3, 0xfb, 0x40, 0, 0x0f, 2, 0x80,
    };
    FIXTURE fixture;
    M_Init(&fixture, code, sizeof(code));
    M_Step(&fixture, 1);
    CHECK_EQ_INT(fixture.player.channel.program, 7);
    CHECK_EQ_INT(fixture.player.channel.volume, 96);
    CHECK_EQ_INT(fixture.player.channel.transpose, 12);
    CHECK_EQ_INT(fixture.player.channel.lfo_flags, 1);
    CHECK_EQ_INT(fixture.player.channel.pitch.note, 0x50);
    CHECK_EQ_INT(fixture.player.remaining, 0);
    M_Step(&fixture, 255);
    CHECK_EQ_INT(fixture.player.step, 256);
    CHECK_EQ_INT(fixture.player.remaining, 1);
    CHECK_EQ_INT(fixture.player.channel.pitch.note, 0x50);
    M_Step(&fixture, 1);
    CHECK_EQ_INT(fixture.player.channel.pitch.note, 255);
    CHECK_EQ_INT(fixture.player.remaining, 2);
    M_Step(&fixture, 2);
    CHECK(!fixture.player.playing);
    CHECK_EQ_INT(fixture.player.step, 259);
    M_Step(&fixture, 3);
    CHECK_EQ_INT(fixture.player.step, 259);
    M_Free(&fixture);
}

TEST(counted_loops_keep_private_mutation_and_retrigger_resets_at_loop_start)
{
    const uint8_t code[] = {
        0xf9, 7, 1, 0x40, 1, 0xf3, 0xf8, 3, 99, 1, 1, 0x80,
    };
    FIXTURE fixture;
    M_Init(&fixture, code, sizeof(code));
    M_Step(&fixture, 4);
    CHECK(!fixture.player.playing);
    CHECK_EQ_INT(fixture.player.work[264], 3);
    CHECK_EQ_INT(fixture.player.channel.volume, 96);
    CHECK_EQ_INT(fixture.bank->bytes[264], 99);
    CHECK(Result_Ignore(PmdEffect_Play(&fixture.player, 0)));
    CHECK_EQ_INT(fixture.player.work[264], 3);
    M_Step(&fixture, 1);
    CHECK_EQ_INT(fixture.player.work[264], 0);
    const PMD_EFFECT_PLAYER before = fixture.player;
    CHECK(!Result_Ignore(PmdEffect_Play(&fixture.player, 127)));
    CHECK(memcmp(&before, &fixture.player, sizeof(before)) == 0);
    M_Free(&fixture);
}

TEST(last_iteration_escape_and_whole_effect_repeat)
{
    const uint8_t escape[] = {
        0xf9, 11, 1, 0x40, 1, 0xf7, 11, 1, 0x41, 1, 0xf8, 3, 0, 1, 1, 0x80,
    };
    FIXTURE fixture;
    M_Init(&fixture, escape, sizeof(escape));
    const uint8_t notes[] = { 0x40, 0x41, 0x40, 0x41, 0x40 };
    for (size_t i = 0; i < ARRAY_SIZE(notes); i++) {
        M_Step(&fixture, 1);
        CHECK_EQ_INT(fixture.player.channel.pitch.note, notes[i]);
    }
    M_Step(&fixture, 1);
    CHECK(!fixture.player.playing);
    M_Free(&fixture);
    const uint8_t repeat[] = { 0xf6, 0x40, 1, 0x80 };
    M_Init(&fixture, repeat, sizeof(repeat));
    M_Step(&fixture, 3);
    CHECK(fixture.player.playing);
    CHECK_EQ_INT(fixture.player.channel.pitch.note, 0x40);
    CHECK_EQ_INT(fixture.player.repeat_position, 257);
    CHECK_EQ_INT(fixture.player.step, 3);
    M_Free(&fixture);
}

TEST(portamento_and_note_consume_inline_slur)
{
    const uint8_t code[] = {
        0xda, 0x40, 0x50, 3, 0xc1, 0x41, 2, 0xc1, 0x80,
    };
    FIXTURE fixture;
    M_Init(&fixture, code, sizeof(code));
    M_Step(&fixture, 1);
    CHECK_EQ_INT(fixture.player.channel.pitch.note, 0x40);
    CHECK_EQ_INT(fixture.player.channel.note.gate, 0);
    CHECK_EQ_INT(fixture.player.position, 261);
    M_Step(&fixture, 3);
    CHECK_EQ_INT(fixture.player.channel.pitch.note, 0x41);
    CHECK_EQ_INT(fixture.player.channel.note.gate, 0);
    CHECK_EQ_INT(fixture.player.position, 264);
    M_Step(&fixture, 2);
    CHECK(!fixture.player.playing);
    CHECK_EQ_INT(fixture.player.step, 6);
    M_Free(&fixture);
}

TEST(stops_on_malformed_commands_and_can_restart)
{
    const uint8_t malformed[][5] = {
        { 0x81, 0x80 },
        { 0xda, 0x40, 0x50, 0, 0x80 },
        { 0xf9, 0xff, 0xff, 0x80 },
        { 0xf6, 0x80 },
    };
    for (size_t i = 0; i < ARRAY_SIZE(malformed); i++) {
        FIXTURE fixture;
        M_Init(&fixture, malformed[i], sizeof(malformed[i]));
        CHECK(!Result_Ignore(PmdEffect_Step(&fixture.player, 0, false)));
        CHECK(fixture.player.failed);
        CHECK(!fixture.player.playing);
        CHECK(Result_Ignore(PmdEffect_Play(&fixture.player, 0)));
        CHECK(!fixture.player.failed);
        CHECK(fixture.player.playing);
        M_Free(&fixture);
    }
    PMD_EFFECTS bank = {};
    PMD_EFFECT_PLAYER player;
    CHECK(!Result_Ignore(
        PmdEffect_Init(&player, &bank, &g_HeapAllocator, nullptr, 2)));
    CHECK(player.work == nullptr);
}

TEST(rejects_truncated_command_and_note_at_record_end)
{
    const uint8_t code[] = { 0x80 };
    const uint8_t opcodes[] = { 0xf2, 0x40 };
    for (size_t i = 0; i < ARRAY_SIZE(opcodes); i++) {
        FIXTURE fixture;
        M_Init(&fixture, code, sizeof(code));
        fixture.player.position = (uint16_t)(fixture.player.work_size - 1);
        fixture.player.work[fixture.player.position] = opcodes[i];
        CHECK(!Result_Ignore(PmdEffect_Step(&fixture.player, 0, false)));
        CHECK(fixture.player.failed);
        CHECK_EQ_INT(fixture.player.step, 1);
        M_Free(&fixture);
    }
}
