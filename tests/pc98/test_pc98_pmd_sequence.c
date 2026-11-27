#include "pc98/pmd/driver.h"

#include "core/base/arena.h"
#include "test.h"

#define M_BYTES 512

typedef struct {
    ARENA arena;
    PMD *pmd;
    PMD_SONG *song;
    uint8_t bytes[M_BYTES + 16];
} FIXTURE;

static void M_Word(uint8_t *const bytes, const uint16_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
}

static void M_Init(FIXTURE *const fixture)
{
    memset(fixture, 0, sizeof(*fixture));
    memset(fixture->bytes + 27, 0x80, M_BYTES - 27);
    for (size_t part = 0; part < 11; part++) {
        M_Word(fixture->bytes + 1 + part * 2, part == 0 ? 26 : 480);
    }
    M_Word(fixture->bytes + 23, M_BYTES - 1);
    M_Word(fixture->bytes + 25, M_BYTES + 11);
    M_Word(fixture->bytes + M_BYTES + 8, M_BYTES + 13);
    M_Word(fixture->bytes + M_BYTES + 10, 0xfe48);
    M_Word(fixture->bytes + M_BYTES + 12, 0xff00);
    Arena_Init(&fixture->arena, "PMD parser test", 4 * 1024 * 1024);
    const ALLOCATOR *const allocator = Arena_Allocator(&fixture->arena);
    uint8_t effects[259] = {};
    for (size_t i = 0; i < 127; i++) {
        M_Word(effects + i * 2, 256);
    }
    M_Word(effects + 254, 257);
    effects[256] = 0x80;
    M_Word(effects + 257, 0xff00);
    READER reader;
    Reader_OpenBuffer(&reader, effects, sizeof(effects), nullptr);
    PMD_EFFECTS *bank;
    CHECK(Result_Ignore(PmdData_LoadEffects(&reader, allocator, &bank)));
    Reader_Close(&reader);
    CHECK(Result_Ignore(
        Pmd_Init(bank, PMD_BOARD_26, 200, allocator, &fixture->pmd)));
}

static void M_Stream(
    FIXTURE *const fixture, const uint8_t part, const uint16_t offset,
    const uint8_t *const bytes, const size_t size)
{
    CHECK((size_t)offset + size < M_BYTES);
    memcpy(fixture->bytes + offset + 1, bytes, size);
    M_Word(fixture->bytes + 1 + part * 2, offset);
}

static void M_Start(FIXTURE *const fixture)
{
    READER reader;
    Reader_OpenBuffer(&reader, fixture->bytes, sizeof(fixture->bytes), nullptr);
    CHECK(Result_Ignore(PmdData_LoadSong(
        &reader, Arena_Allocator(&fixture->arena), &fixture->song)));
    Reader_Close(&reader);
    CHECK(Result_Ignore(MusicSeq_Load(fixture->pmd, fixture->song)));
    CHECK(Result_Ignore(MusicSeq_Start(fixture->pmd)));
}

static void M_Step(FIXTURE *const fixture)
{
    CHECK(Result_Log(MusicSeq_Step(fixture->pmd), LOG_LEVEL_ERROR));
}

static void M_Close(FIXTURE *const fixture)
{
    MusicSeq_Stop(fixture->pmd);
    Arena_Free(&fixture->arena);
}

TEST(zero_duration_keeps_every_part_active_for_256_calls)
{
    FIXTURE fixture;
    M_Init(&fixture);
    const uint8_t stream[] = { 0x40, 0, 0x80 };
    const uint8_t parts[] = { 6, 7, 8, 0, 1, 2, 3, 4, 5 };
    for (size_t i = 0; i < ARRAY_SIZE(parts); i++) {
        M_Stream(
            &fixture, parts[i], parts[i] == 0 ? 26 : 32, stream,
            sizeof(stream));
    }
    M_Start(&fixture);
    CHECK(Result_Ignore(MusicSeq_Load(fixture.pmd, fixture.song)));
    CHECK(fixture.pmd->sequence.playing);
    M_Step(&fixture);
    MUSIC_SEQUENCE *const sequence = &fixture.pmd->sequence;
    for (size_t i = 0; i < ARRAY_SIZE(parts); i++) {
        CHECK_EQ_INT(sequence->parts[parts[i]].key_ons, 1);
        CHECK_EQ_INT(sequence->parts[parts[i]].remaining, 0);
    }
    for (size_t i = 0; i < 255; i++) {
        M_Step(&fixture);
    }
    CHECK_EQ_INT(sequence->parts[0].key_ons, 1);
    CHECK_EQ_INT(sequence->parts[0].remaining, 1);
    M_Step(&fixture);
    CHECK_EQ_INT(sequence->loops, 255);
    CHECK(sequence->playing);
    CHECK_EQ_INT(sequence->parts[0].position, 28);
    MusicSeq_Stop(fixture.pmd);
    M_Step(&fixture);
    MusicSeq_Stop(fixture.pmd);
    CHECK_EQ_INT(sequence->step, 257);
    M_Close(&fixture);
}

TEST(counted_loop_escape_and_writable_copy)
{
    FIXTURE fixture;
    M_Init(&fixture);
    const uint8_t stream[] = {
        0xf9, 35, 0, 0x40, 1, 0xf7, 35, 0, 0xf8, 3, 99, 27, 0, 0x41, 2, 0x80,
    };
    M_Stream(&fixture, 0, 26, stream, sizeof(stream));
    M_Start(&fixture);
    MUSIC_SEQUENCE *const sequence = &fixture.pmd->sequence;
    for (size_t i = 0; i < 3; i++) {
        M_Step(&fixture);
        CHECK_EQ_INT(fixture.pmd->fm.channels[0].pitch.note, 0x40);
    }
    M_Step(&fixture);
    CHECK_EQ_INT(fixture.pmd->fm.channels[0].pitch.note, 0x41);
    CHECK_EQ_INT(sequence->work[37], 2);
    CHECK_EQ_INT(fixture.bytes[37], 99);
    M_Step(&fixture);
    M_Step(&fixture);
    CHECK_EQ_INT(fixture.pmd->fm.channels[0].pitch.note, 255);
    MusicSeq_Stop(fixture.pmd);
    CHECK(Result_Ignore(MusicSeq_Start(fixture.pmd)));
    CHECK_EQ_INT(sequence->parts[0].key_ons, 4);
    CHECK_EQ_INT(sequence->work[37], 2);
    M_Step(&fixture);
    CHECK_EQ_INT(sequence->work[37], 0);
    M_Close(&fixture);
}

TEST(whole_repeat_global_loop_count_and_status)
{
    FIXTURE fixture;
    M_Init(&fixture);
    const uint8_t stream[] = { 0xdc, 254, 0xdb, 3, 0xf6, 0x40, 1, 0x80 };
    M_Stream(&fixture, 0, 26, stream, sizeof(stream));
    M_Start(&fixture);
    M_Step(&fixture);
    MUSIC_SEQUENCE *const sequence = &fixture.pmd->sequence;
    CHECK_EQ_INT(sequence->status, 1);
    CHECK_EQ_INT(sequence->parts[0].repeat_position, 31);
    M_Step(&fixture);
    CHECK_EQ_INT(sequence->loops, 1);
    CHECK_EQ_INT(sequence->parts[0].loop_check, 0);
    sequence->loops = 254;
    M_Step(&fixture);
    CHECK_EQ_INT(sequence->loops, 1);
    M_Close(&fixture);
}

TEST(rhythm_patterns_have_commands_shots_rests_and_lengths)
{
    FIXTURE fixture;
    M_Init(&fixture);
    const uint8_t stream[] = { 0, 0x80 };
    const uint8_t rhythm[] = { 0xfc, 150, 0xdf, 2, 0x80, 1, 3, 0, 2, 0xff };
    M_Stream(&fixture, 10, 32, stream, sizeof(stream));
    memcpy(fixture.bytes + 101, rhythm, sizeof(rhythm));
    M_Word(fixture.bytes + 23, 98);
    M_Word(fixture.bytes + 99, 100);
    M_Start(&fixture);
    MUSIC_SEQUENCE *const sequence = &fixture.pmd->sequence;
    M_Step(&fixture);
    CHECK_EQ_INT(sequence->parts[10].remaining, 3);
    CHECK_EQ_INT(sequence->parts[10].position, 33);
    CHECK_EQ_INT(sequence->timer_b, 150);
    CHECK_EQ_INT(sequence->rhythm_position, 107);
    M_Step(&fixture);
    CHECK_EQ_INT(Pmd_Measure(fixture.pmd), 1);
    M_Step(&fixture);
    M_Step(&fixture);
    CHECK_EQ_INT(sequence->rhythm_position, 109);
    CHECK_EQ_INT(sequence->parts[10].remaining, 2);
    M_Step(&fixture);
    M_Step(&fixture);
    CHECK_EQ_INT(sequence->parts[10].loop_check, 3);
    CHECK_EQ_INT(sequence->parts[10].key_ons, 2);
    M_Close(&fixture);
}

TEST(fm3_subparts_start_in_the_same_call_and_masks_control_slurs)
{
    FIXTURE fixture;
    M_Init(&fixture);
    const uint8_t stream[] = { 0xc6, 100, 0, 0, 0, 0, 0, 0x40, 2, 0x80 };
    const uint8_t extra[] = { 0xcf, 0x10, 0x41, 1, 0xc1, 0x80 };
    M_Stream(&fixture, 2, 32, stream, sizeof(stream));
    memcpy(fixture.bytes + 101, extra, sizeof(extra));
    M_Start(&fixture);
    M_Step(&fixture);
    MUSIC_SEQUENCE *const sequence = &fixture.pmd->sequence;
    CHECK_EQ_INT(sequence->parts[2].key_ons, 1);
    CHECK_EQ_INT(sequence->parts[3].key_ons, 1);
    CHECK_EQ_INT(sequence->parts[3].position, 105);
    Pmd_Effect(fixture.pmd, 0);
    MusicSeq_Stop(fixture.pmd);
    CHECK(Result_Ignore(MusicSeq_Start(fixture.pmd)));
    M_Step(&fixture);
    CHECK_EQ_INT(sequence->parts[3].position, 104);
    CHECK_EQ_INT(sequence->parts[3].mask, 2);
    M_Step(&fixture);
    CHECK_EQ_INT(sequence->parts[3].position, 105);
    M_Close(&fixture);
}

TEST(portamento_masked_parsing_and_unknown_opcode_rewrite)
{
    FIXTURE fixture;
    M_Init(&fixture);
    const uint8_t stream[] = { 0xda, 0x40, 0x41, 2, 0xc1, 0x81 };
    M_Stream(&fixture, 0, 26, stream, sizeof(stream));
    M_Start(&fixture);
    MUSIC_SEQUENCE *const sequence = &fixture.pmd->sequence;
    M_Step(&fixture);
    CHECK_EQ_INT(sequence->parts[0].position, 31);
    CHECK_EQ_INT(fixture.pmd->fm.channels[0].note.gate, 0);
    M_Step(&fixture);
    M_Step(&fixture);
    CHECK_EQ_INT(sequence->work[32], 0x80);
    MusicSeq_Stop(fixture.pmd);
    CHECK(Result_Ignore(MusicSeq_Load(fixture.pmd, fixture.song)));
    MusicSeq_SetMask(sequence, 0, 1);
    CHECK(Result_Ignore(MusicSeq_Start(fixture.pmd)));
    M_Step(&fixture);
    CHECK_EQ_INT(sequence->parts[0].position, 30);
    CHECK_EQ_INT(sequence->parts[0].mask, 1);
    CHECK_EQ_INT(fixture.pmd->fm.channels[0].pitch.note, 255);
    M_Close(&fixture);
}

TEST(bounds_zero_portamento_and_dispatch_cycles_fail_closed)
{
    static const uint8_t invalid[][8] = {
        { 0xda, 0x40, 0x41, 0 },
        { 0xf9, 0xff, 0xff },
        { 0xf8, 0, 0, 0xff, 0xff },
        { 0xf6, 0x80 },
    };
    for (size_t i = 0; i < ARRAY_SIZE(invalid); i++) {
        FIXTURE fixture;
        M_Init(&fixture);
        M_Stream(&fixture, 0, 26, invalid[i], sizeof(invalid[i]));
        M_Start(&fixture);
        CHECK(!Result_Ignore(MusicSeq_Step(fixture.pmd)));
        CHECK(fixture.pmd->sequence.failed);
        CHECK(!fixture.pmd->sequence.playing);
        M_Close(&fixture);
    }
    FIXTURE fixture;
    M_Init(&fixture);
    const uint8_t truncated[] = { 0xb4 };
    M_Stream(&fixture, 6, M_BYTES - 2, truncated, sizeof(truncated));
    M_Start(&fixture);
    CHECK(!Result_Ignore(MusicSeq_Step(fixture.pmd)));
    M_Close(&fixture);
}

TEST(tempo_forms_and_wrapping_measure_counters)
{
    FIXTURE fixture;
    M_Init(&fixture);
    const uint8_t stream[] = {
        0xfc, 250,  0x40, 1,    0xfc, 254,  128,  0x40, 1,
        0xfc, 255,  18,   0x40, 1,    0xfc, 253,  255,  0x40,
        1,    0xfc, 252,  127,  0xdf, 0,    0x40, 0,    0x80,
    };
    M_Stream(&fixture, 0, 26, stream, sizeof(stream));
    M_Start(&fixture);
    const uint8_t timers[] = { 250, 122, 12, 12, 226 };
    const uint8_t tempos[] = { 255, 32, 18, 18, 145 };
    MUSIC_SEQUENCE *const sequence = &fixture.pmd->sequence;
    for (size_t i = 0; i < ARRAY_SIZE(timers); i++) {
        M_Step(&fixture);
        CHECK_EQ_INT(sequence->timer_b, timers[i]);
        CHECK_EQ_INT(sequence->tempo, tempos[i]);
    }
    sequence->measure = UINT16_MAX;
    sequence->measure_tick = 255;
    M_Step(&fixture);
    CHECK_EQ_INT(Pmd_Measure(fixture.pmd), 0);
    CHECK_EQ_INT(sequence->measure_tick, 0);
    M_Close(&fixture);
}

TEST(part_boundaries_keep_masks_before_duration_and_slur_parsing)
{
    FIXTURE fixture;
    M_Init(&fixture);
    const uint8_t stream[] = { 0x40, 4, 0xc1, 0x80 };
    M_Stream(&fixture, 0, 26, stream, sizeof(stream));
    M_Start(&fixture);
    MUSIC_SEQUENCE *const sequence = &fixture.pmd->sequence;
    CHECK_EQ_INT(sequence->parts[0].remaining, 1);
    MusicSeq_SetMask(sequence, 0, 1);
    M_Step(&fixture);
    CHECK_EQ_INT(sequence->parts[0].remaining, 4);
    CHECK_EQ_INT(sequence->parts[0].position, 28);
    CHECK_EQ_INT(sequence->parts[0].mask, 1);
    CHECK_EQ_INT(fixture.pmd->fm.channels[0].pitch.note, 255);
    M_Close(&fixture);
}
