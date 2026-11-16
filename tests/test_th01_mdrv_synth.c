#include "th01/mdrv_synth.h"

#include "core/base/arena.h"
#include "test.h"

typedef struct {
    uint64_t time, pulse;
    uint16_t reg;
    uint8_t value;
} WRITE;

typedef struct {
    ARENA arena;
    ALLOCATOR allocator;
    size_t allocations;
    uint8_t bytes[2][806], effect_bytes[118];
    MDRV_TRACK tracks[2][10];
    MDRV_MUSIC songs[2];
    MDRV_EFFECT effects[2];
    MDRV_EFFECTS effect_bank;
    MDRV_SYNTH *synth;
    WRITE writes[8192];
    size_t write_count;
} FIXTURE;

static void *M_Alloc(void *const context, const size_t size)
{
    FIXTURE *const fixture = context;
    fixture->allocations++;
    return Arena_Alloc(&fixture->arena, size);
}

static void M_Write(
    void *const context, const uint64_t time, const uint16_t reg,
    const uint8_t value)
{
    FIXTURE *const fixture = context;
    if (fixture->write_count < 8192) {
        fixture->writes[fixture->write_count++] = (WRITE) {
            .time = time,
            .pulse = fixture->synth == nullptr
                ? 0
                : MdrvSynth_Status(fixture->synth).pulses,
            .reg = reg,
            .value = value,
        };
    }
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

static void M_Start(FIXTURE *const fixture)
{
    CHECK(Result_Log(
        MdrvSynth_Init(
            fixture->songs, 2, &fixture->effect_bank, &fixture->allocator,
            M_Write, fixture, &fixture->synth),
        LOG_LEVEL_ERROR));
    CHECK(Result_Log(MdrvSynth_Play(fixture->synth, 0), LOG_LEVEL_ERROR));
    fixture->write_count = 0;
}

static void M_Pulses(FIXTURE *const fixture, const uint64_t count)
{
    const uint64_t end = MdrvSynth_Status(fixture->synth).pulses + count;
    while (MdrvSynth_Status(fixture->synth).pulses < end) {
        if (!Result_Log(
                MdrvSynth_Render(fixture->synth, nullptr, 1),
                LOG_LEVEL_ERROR)) {
            TEST_FAIL("render failed");
            break;
        }
    }
}

static const WRITE *M_Last(const FIXTURE *const fixture, const uint16_t reg)
{
    for (size_t i = fixture->write_count; i > 0; i--) {
        if (fixture->writes[i - 1].reg == reg) {
            return &fixture->writes[i - 1];
        }
    }
    TEST_FAIL("missing register %03x", reg);
    return &fixture->writes[0];
}

TEST(timer_status_phase_and_tempo_reload)
{
    const uint8_t code[] = { 0xe9, 120, 0x40, 255, 0xff };
    FIXTURE fixture;
    M_Init(&fixture, code, sizeof(code), 0x80);
    M_Start(&fixture);
    CHECK(Result_Ignore(MdrvSynth_Render(fixture.synth, nullptr, 443)));
    CHECK_EQ_INT(MdrvSynth_Status(fixture.synth).pulses, 0);
    CHECK(Result_Ignore(MdrvSynth_Render(fixture.synth, nullptr, 1)));
    CHECK_EQ_INT(MdrvSynth_Status(fixture.synth).pulses, 1);
    CHECK_EQ_INT(M_Last(&fixture, 0x26)->value, 220);
    CHECK_EQ_INT(M_Last(&fixture, 0x26)->time, 368640);
    CHECK(Result_Ignore(MdrvSynth_Render(fixture.synth, nullptr, 663)));
    CHECK_EQ_INT(MdrvSynth_Status(fixture.synth).pulses, 1);
    CHECK(Result_Ignore(MdrvSynth_Render(fixture.synth, nullptr, 1)));
    CHECK_EQ_INT(MdrvSynth_Status(fixture.synth).pulses, 2);
    CHECK_EQ_INT(M_Last(&fixture, 0x27)->time, 921600);
    M_Pulses(&fixture, 1);
    CHECK_EQ_INT(M_Last(&fixture, 0x27)->time, 1336320);
    Arena_Free(&fixture.arena);
}

TEST(program_fields_carrier_volume_and_gate)
{
    const uint8_t code[] = { 0xeb, 0, 0xec, 110, 0xea, 4, 0x40, 8, 0xff };
    FIXTURE fixture;
    M_Init(&fixture, code, sizeof(code), 0x80);
    M_Start(&fixture);
    M_Pulses(&fixture, 1);
    CHECK_EQ_INT(M_Last(&fixture, 0x30)->value, 1);
    CHECK_EQ_INT(M_Last(&fixture, 0x3c)->value, 4);
    CHECK_EQ_INT(M_Last(&fixture, 0xb0)->value, 7);
    CHECK_EQ_INT(M_Last(&fixture, 0x40)->value, 28);
    CHECK_EQ_INT(M_Last(&fixture, 0x4c)->value, 34);
    CHECK_EQ_INT(M_Last(&fixture, 0xa4)->value, 0x22);
    CHECK_EQ_INT(M_Last(&fixture, 0xa0)->value, 0x6a);
    CHECK_EQ_INT(M_Last(&fixture, 0x28)->value, 1);
    M_Pulses(&fixture, 3);
    CHECK_EQ_INT(M_Last(&fixture, 0x28)->value, 0);
    CHECK_EQ_INT(M_Last(&fixture, 0x28)->pulse, 4);
    Arena_Free(&fixture.arena);
}

TEST(ssg_envelope_uses_fine_volume_and_release)
{
    const uint8_t code[] = { 0xeb, 0x80, 0xec, 15, 0x40, 8, 0xff };
    FIXTURE fixture;
    M_Init(&fixture, code, sizeof(code), 0x41);
    M_Start(&fixture);
    M_Pulses(&fixture, 1);
    CHECK_EQ_INT(M_Last(&fixture, 8)->value, 12);
    CHECK_EQ_INT(M_Last(&fixture, 7)->value, 0xb8);
    M_Pulses(&fixture, 1);
    CHECK_EQ_INT(M_Last(&fixture, 8)->value, 15);
    M_Pulses(&fixture, 1);
    CHECK_EQ_INT(M_Last(&fixture, 8)->value, 14);
    M_Pulses(&fixture, 30);
    CHECK_EQ_INT(M_Last(&fixture, 8)->value, 0);
    Arena_Free(&fixture.arena);
}

TEST(portamento_has_signed_chip_domain_and_legacy_scale)
{
    const uint8_t code[] = { 0xeb, 0, 0xf2, 0x40, 8, 0xfd, 0xff, 0xff };
    FIXTURE fixture;
    M_Init(&fixture, code, sizeof(code), 0x80);
    fixture.songs[1].hardware = 1;
    M_Start(&fixture);
    M_Pulses(&fixture, 1);
    CHECK_EQ_INT(M_Last(&fixture, 0xa0)->value, 0x69);
    M_Pulses(&fixture, 3);
    CHECK_EQ_INT(M_Last(&fixture, 0xa0)->value, 0x66);
    MdrvSynth_Reset(fixture.synth);
    CHECK(Result_Ignore(MdrvSynth_Play(fixture.synth, 1)));
    M_Pulses(&fixture, 1);
    CHECK_EQ_INT(M_Last(&fixture, 0xa0)->value, 0x67);
    Arena_Free(&fixture.arena);
}

TEST(raw_register_bypasses_effect_owned_music)
{
    const uint8_t code[] = {
        0xeb, 0x80, 0xec, 15, 0x40, 1, 0xee, 8, 9, 0x42, 8, 0xff,
    };
    FIXTURE fixture;
    M_Init(&fixture, code, sizeof(code), 0x40);
    M_Start(&fixture);
    CHECK(Result_Ignore(MdrvSynth_EffectPlay(fixture.synth, 2)));
    M_Pulses(&fixture, 2);
    bool raw = false;
    for (size_t i = 0; i < fixture.write_count; i++) {
        const WRITE *const write = &fixture.writes[i];
        raw |= write->reg == 8 && write->value == 9 && write->pulse == 2;
    }
    CHECK(raw);
    CHECK_EQ_INT(MdrvSynth_Status(fixture.synth).effect_mask, 7);
    M_Pulses(&fixture, 8);
    CHECK(!MdrvSynth_Status(fixture.synth).effects_playing);
    CHECK_EQ_INT(MdrvSynth_Status(fixture.synth).effect_mask, 0);
    Arena_Free(&fixture.arena);
}

TEST(chunk_reset_replay_and_allocations_are_deterministic)
{
    const uint8_t code[] = {
        0xeb, 0,    0xec, 110,  0xed, 220,  6,    12,
        2,    0x40, 8,    0x42, 8,    0xf3, 0xfa, 0xff,
    };
    FIXTURE fixture;
    M_Init(&fixture, code, sizeof(code), 0x80);
    M_Start(&fixture);
    MDRV_SYNTH_FRAME *const first =
        Arena_Alloc(&fixture.arena, 12000 * sizeof(*first));
    MDRV_SYNTH_FRAME *const second =
        Arena_Alloc(&fixture.arena, 12000 * sizeof(*second));
    const size_t allocations = fixture.allocations;
    CHECK(Result_Ignore(MdrvSynth_EffectPlay(fixture.synth, 2)));
    CHECK(Result_Ignore(MdrvSynth_Render(fixture.synth, first, 12000)));
    MdrvSynth_Reset(fixture.synth);
    CHECK(Result_Ignore(MdrvSynth_Play(fixture.synth, 0)));
    CHECK(Result_Ignore(MdrvSynth_EffectPlay(fixture.synth, 2)));
    size_t position = 0;
    while (position < 12000) {
        size_t count = position % 137 + 1;
        if (count > 12000 - position) {
            count = 12000 - position;
        }
        CHECK(Result_Ignore(
            MdrvSynth_Render(fixture.synth, second + position, count)));
        position += count;
    }
    CHECK(memcmp(first, second, 12000 * sizeof(*first)) == 0);
    bool music = false, effect = false;
    for (size_t i = 0; i < 12000; i++) {
        music |= first[i].music[0] != 0 || first[i].music[1] != 0;
        effect |= first[i].effects[0] != 0 || first[i].effects[1] != 0;
    }
    CHECK(music);
    CHECK(effect);
    CHECK(Result_Ignore(MdrvSynth_Play(fixture.synth, 1)));
    MdrvSynth_Stop(fixture.synth);
    CHECK(Result_Ignore(MdrvSynth_EffectPlay(fixture.synth, 1)));
    MdrvSynth_Fade(fixture.synth, 2);
    CHECK_EQ_INT(fixture.allocations, allocations);
    Arena_Free(&fixture.arena);
}

TEST(fade_counts_zero_as_256_and_stop_cancels_effects)
{
    const uint8_t code[] = { 0xeb, 0,   0xec, 110,  0xea, 0,
                             0x40, 255, 0xf3, 0xfc, 0xff };
    FIXTURE fixture;
    M_Init(&fixture, code, sizeof(code), 0x80);
    M_Start(&fixture);
    MdrvSynth_Fade(fixture.synth, 0);
    M_Pulses(&fixture, 255);
    CHECK_EQ_INT(MdrvSynth_Status(fixture.synth).fade_attenuation, 0);
    M_Pulses(&fixture, 1);
    CHECK_EQ_INT(MdrvSynth_Status(fixture.synth).fade_attenuation, 1);
    CHECK(Result_Ignore(MdrvSynth_Play(fixture.synth, 0)));
    MdrvSynth_Fade(fixture.synth, 129);
    M_Pulses(&fixture, 127);
    CHECK(!MdrvSynth_Status(fixture.synth).fading);
    CHECK(MdrvSynth_Status(fixture.synth).music_playing);
    CHECK(Result_Ignore(MdrvSynth_EffectPlay(fixture.synth, 2)));
    MdrvSynth_Fade(fixture.synth, 1);
    M_Pulses(&fixture, 127);
    CHECK(!MdrvSynth_Status(fixture.synth).music_playing);
    CHECK(!MdrvSynth_Status(fixture.synth).effects_playing);
    CHECK_EQ_INT(M_Last(&fixture, 8)->value, 0);
    Arena_Free(&fixture.arena);
}

TEST(malformed_commands_latch_failure_and_reset_recovers)
{
    const uint8_t code[] = { 0xeb, 1, 0x40, 8, 0xff };
    FIXTURE fixture;
    M_Init(&fixture, code, sizeof(code), 0x80);
    M_Start(&fixture);
    MDRV_SYNTH_FRAME output[1024];
    CHECK(!Result_Ignore(MdrvSynth_Render(fixture.synth, output, 1024)));
    CHECK(MdrvSynth_Status(fixture.synth).failed);
    CHECK(Result_Ignore(MdrvSynth_EffectPlay(fixture.synth, 0)));
    CHECK_EQ_INT(output[1023].music[0], 0);
    CHECK_EQ_INT(output[1023].effects[1], 0);
    CHECK(!Result_Ignore(MdrvSynth_Render(fixture.synth, nullptr, 1)));
    MdrvSynth_Reset(fixture.synth);
    CHECK(!MdrvSynth_Status(fixture.synth).failed);
    CHECK(Result_Ignore(MdrvSynth_EffectPlay(fixture.synth, 1)));
    CHECK(Result_Ignore(MdrvSynth_Render(fixture.synth, output, 1024)));
    CHECK(!Result_Ignore(MdrvSynth_Play(fixture.synth, 2)));
    CHECK(!Result_Ignore(MdrvSynth_EffectPlay(fixture.synth, 3)));
    Arena_Free(&fixture.arena);
}

TEST(external_pcm_is_rejected)
{
    const uint8_t code[] = { 0xeb, 0xc0, 0x40, 8, 0xff };
    FIXTURE fixture;
    M_Init(&fixture, code, sizeof(code), 0x40);
    M_Start(&fixture);
    CHECK(!Result_Ignore(MdrvSynth_Render(fixture.synth, nullptr, 1024)));
    fixture.tracks[0][0].channel = 0x20;
    MDRV_SYNTH *other;
    CHECK(!Result_Ignore(MdrvSynth_Init(
        fixture.songs, 2, &fixture.effect_bank, &fixture.allocator, nullptr,
        nullptr, &other)));
    CHECK(other == nullptr);
    Arena_Free(&fixture.arena);
}

TEST(ssg_program_change_resets_stage_during_release)
{
    const uint8_t code[] = { 0xeb, 0x80, 0xec, 15, 0x40, 8,
                             0xeb, 0x40, 0x90, 8,  0xff };
    FIXTURE fixture;
    M_Init(&fixture, code, sizeof(code), 0x40);
    M_Start(&fixture);
    M_Pulses(&fixture, 8);
    CHECK(M_Last(&fixture, 8)->value != 0);
    M_Pulses(&fixture, 1);
    CHECK_EQ_INT(M_Last(&fixture, 8)->value, 0);
    CHECK_EQ_INT(M_Last(&fixture, 8)->pulse, 9);
    Arena_Free(&fixture.arena);
}

TEST(banked_fm_raw_writes_follow_context)
{
    const uint8_t code[] = {
        0xeb, 0, 0xee, 0x30, 9, 0xee, 8, 12, 0x40, 8, 0xff
    };
    FIXTURE fixture;
    M_Init(&fixture, code, sizeof(code), 0x85);
    fixture.bytes[0][200] = 0x90;
    fixture.bytes[0][201] = 255;
    fixture.bytes[0][202] = 0xff;
    for (size_t i = 0; i < 4; i++) {
        fixture.tracks[0][i] = (MDRV_TRACK) {
            .offset = i == 3 ? 32 : 200,
            .channel = 0x85,
        };
    }
    fixture.songs[0].track_count = 4;
    M_Start(&fixture);
    M_Pulses(&fixture, 1);
    CHECK_EQ_INT(M_Last(&fixture, 0x130)->value, 9);
    CHECK_EQ_INT(M_Last(&fixture, 8)->value, 12);
    CHECK_EQ_INT(M_Last(&fixture, 0x1a4)->value, 0x22);
    CHECK_EQ_INT(M_Last(&fixture, 0x28)->value, 0xf4);
    Arena_Free(&fixture.arena);
}

TEST(held_note_clocks_evolve_pitch_and_volume_lfos)
{
    const uint8_t code[] = {
        0xeb, 0,   0xec, 110, 0xed, 200,  6,  12,   2,
        0xe8, 255, 0x84, 2,   0,    0x40, 32, 0xff,
    };
    FIXTURE fixture;
    M_Init(&fixture, code, sizeof(code), 0x80);
    M_Start(&fixture);
    M_Pulses(&fixture, 1);
    CHECK_EQ_INT(M_Last(&fixture, 0xa0)->value, 0x6a);
    M_Pulses(&fixture, 2);
    CHECK_EQ_INT(M_Last(&fixture, 0x4c)->value, 36);
    M_Pulses(&fixture, 2);
    CHECK_EQ_INT(M_Last(&fixture, 0xa0)->value, 0x6d);
    Arena_Free(&fixture.arena);
}

TEST(portamento_wraps_signed_work_without_undefined_shifts)
{
    const uint8_t code[] = { 0xeb, 0, 0xf2, 0x00, 8, 0, 0x80, 0xff };
    FIXTURE fixture;
    M_Init(&fixture, code, sizeof(code), 0x80);
    M_Start(&fixture);
    M_Pulses(&fixture, 1);
    CHECK_EQ_INT(M_Last(&fixture, 0xa4)->value, 0xe2);
    CHECK_EQ_INT(M_Last(&fixture, 0xa0)->value, 0x6a);
    M_Pulses(&fixture, 1);
    CHECK_EQ_INT(M_Last(&fixture, 0xa4)->value, 0xc2);
    Arena_Free(&fixture.arena);
}

TEST(hardware_lfo_sync_keeps_pan_modulation_bits)
{
    const uint8_t code[] = {
        0xeb, 0, 0xfd, 0x0b, 1, 5, 2, 0xf1, 1, 0x40, 8, 0xff,
    };
    FIXTURE fixture;
    M_Init(&fixture, code, sizeof(code), 0x80);
    M_Start(&fixture);
    M_Pulses(&fixture, 1);
    CHECK_EQ_INT(M_Last(&fixture, 0xb4)->value, 0xa5);
    CHECK_EQ_INT(M_Last(&fixture, 0x22)->value, 0x0b);
    size_t sync_writes = 0;
    for (size_t i = 0; i < fixture.write_count; i++) {
        sync_writes += fixture.writes[i].reg == 0x22;
    }
    CHECK_EQ_INT(sync_writes, 3);
    Arena_Free(&fixture.arena);
}

TEST(instrument_ranges_are_checked_before_playback)
{
    const uint8_t code[] = { 0x40, 8, 0xff };
    FIXTURE fixture;
    M_Init(&fixture, code, sizeof(code), 0x80);
    fixture.songs[1].size = 799;
    MDRV_SYNTH *synth;
    CHECK(!Result_Ignore(MdrvSynth_Init(
        fixture.songs, 2, &fixture.effect_bank, &fixture.allocator, nullptr,
        nullptr, &synth)));
    CHECK(synth == nullptr);
    Arena_Free(&fixture.arena);
}

TEST(header_repeat_retains_hardware_without_keyoff)
{
    const uint8_t code[] = { 0xeb, 0, 0xec, 110, 0xea, 0, 0x40, 2, 0xff };
    FIXTURE fixture;
    M_Init(&fixture, code, sizeof(code), 0x80);
    fixture.songs[0].track_count = 1;
    fixture.songs[0].flags = 0x80;
    M_Start(&fixture);
    M_Pulses(&fixture, 1);
    CHECK_EQ_INT(M_Last(&fixture, 0x28)->value, 0xf0);
    M_Pulses(&fixture, 2);
    CHECK_EQ_INT(MdrvSynth_Status(fixture.synth).sync, 0xff);
    CHECK(MdrvSynth_Status(fixture.synth).music_playing);
    CHECK_EQ_INT(M_Last(&fixture, 0x28)->value, 0xf0);
    CHECK_EQ_INT(M_Last(&fixture, 0x28)->pulse, 1);
    M_Pulses(&fixture, 1);
    CHECK_EQ_INT(M_Last(&fixture, 0x28)->pulse, 4);
    Arena_Free(&fixture.arena);
}

TEST(restart_fade_in_preserves_controls_and_global_clock)
{
    const uint8_t code[] = { 0xeb, 0, 0xec, 110, 0x40, 255, 0xff };
    FIXTURE fixture;
    M_Init(&fixture, code, sizeof(code), 0x80);
    M_Start(&fixture);
    M_Pulses(&fixture, 4);
    const size_t allocations = fixture.allocations;
    const uint64_t time = MdrvSynth_Status(fixture.synth).time;
    CHECK(Result_Ignore(MdrvSynth_RestartFadeIn(fixture.synth)));
    CHECK_EQ_INT(MdrvSynth_Status(fixture.synth).time, time);
    CHECK_EQ_INT(MdrvSynth_Status(fixture.synth).pulses, 4);
    CHECK_EQ_INT(MdrvSynth_Status(fixture.synth).fade_attenuation, 127);
    fixture.write_count = 0;
    M_Pulses(&fixture, 1);
    size_t first = 0;
    while (first < fixture.write_count && fixture.writes[first].reg == 0x27) {
        first++;
    }
    CHECK(first + 1 < fixture.write_count);
    CHECK_EQ_INT(fixture.writes[first].reg, 0x4c);
    CHECK_EQ_INT(fixture.writes[first + 1].reg, 0x48);
    CHECK_EQ_INT(fixture.allocations, allocations);
    CHECK(Result_Ignore(MdrvSynth_Play(fixture.synth, 0)));
    CHECK(!MdrvSynth_Status(fixture.synth).fading);
    Arena_Free(&fixture.arena);
}

TEST(invalid_topology_and_switch_do_not_change_playback)
{
    const uint8_t code[] = { 0xeb, 0, 0x40, 255, 0xff };
    FIXTURE fixture;
    M_Init(&fixture, code, sizeof(code), 0x80);
    fixture.songs[1].track_count = 7;
    for (size_t i = 0; i < 7; i++) {
        fixture.tracks[1][i] = (MDRV_TRACK) { .channel = 0x80, .offset = 32 };
    }
    MDRV_SYNTH *invalid;
    CHECK(!Result_Ignore(MdrvSynth_Init(
        fixture.songs, 2, &fixture.effect_bank, &fixture.allocator, nullptr,
        nullptr, &invalid)));
    CHECK(invalid == nullptr);
    fixture.songs[1].track_count = 2;
    M_Start(&fixture);
    M_Pulses(&fixture, 1);
    const MDRV_SYNTH_STATUS before = MdrvSynth_Status(fixture.synth);
    const size_t writes = fixture.write_count;
    CHECK(!Result_Ignore(MdrvSynth_Play(fixture.synth, 99)));
    const MDRV_SYNTH_STATUS after = MdrvSynth_Status(fixture.synth);
    CHECK_EQ_INT(before.time, after.time);
    CHECK_EQ_INT(before.pulses, after.pulses);
    CHECK(before.music_playing && after.music_playing);
    CHECK_EQ_INT(fixture.write_count, writes);
    Arena_Free(&fixture.arena);
}

TEST(play_recovers_both_sequences_after_parser_failure)
{
    const uint8_t code[] = { 0xf3, 0xfe, 0xff };
    FIXTURE fixture;
    M_Init(&fixture, code, sizeof(code), 0x80);
    fixture.bytes[1][32] = 0x40;
    fixture.bytes[1][33] = 8;
    fixture.bytes[1][34] = 0xff;
    M_Start(&fixture);
    CHECK(!Result_Ignore(MdrvSynth_Render(fixture.synth, nullptr, 1024)));
    CHECK(MdrvSynth_Status(fixture.synth).failed);
    CHECK(Result_Ignore(MdrvSynth_Play(fixture.synth, 1)));
    CHECK(Result_Ignore(MdrvSynth_Render(fixture.synth, nullptr, 1024)));
    CHECK(!MdrvSynth_Status(fixture.synth).failed);
    Arena_Free(&fixture.arena);
}

TEST(disabled_timer_flags_do_not_clock_sequences)
{
    const uint8_t code[] = { 0xee, 0x27, 3, 0x40, 255, 0xff };
    FIXTURE fixture;
    M_Init(&fixture, code, sizeof(code), 0x80);
    M_Start(&fixture);
    M_Pulses(&fixture, 1);
    CHECK(Result_Ignore(MdrvSynth_Render(fixture.synth, nullptr, 5000)));
    CHECK_EQ_INT(MdrvSynth_Status(fixture.synth).pulses, 1);
    CHECK(!MdrvSynth_Status(fixture.synth).failed);
    Arena_Free(&fixture.arena);
}
