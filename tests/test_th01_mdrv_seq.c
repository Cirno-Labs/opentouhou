#include "th01/mdrv_seq.h"

#include "core/base/arena.h"
#include "test.h"

typedef struct {
    ARENA arena;
    uint8_t bytes[512];
    MDRV_TRACK tracks[3];
    MDRV_MUSIC music;
    MDRV_EFFECT entries[3];
    MDRV_EFFECTS effects;
    MDRV_SEQUENCE sequence;
    MDRV_EVENT events[512];
    size_t event_count;
} FIXTURE;

static void M_Event(void *const context, const MDRV_EVENT *const event)
{
    FIXTURE *const fixture = context;
    if (fixture->event_count < 512) {
        fixture->events[fixture->event_count++] = *event;
    }
}

static void M_Init(
    FIXTURE *const fixture, const uint8_t *const code, const size_t length,
    const uint8_t channel)
{
    memset(fixture, 0, sizeof(*fixture));
    Arena_Init(&fixture->arena, "sequencer test", 128 * 1024);
    memcpy(fixture->bytes + 32, code, length);
    fixture->tracks[0] = (MDRV_TRACK) { .offset = 32, .channel = channel };
    fixture->music = (MDRV_MUSIC) {
        .bytes = fixture->bytes,
        .size = sizeof(fixture->bytes),
        .tracks = fixture->tracks,
        .track_count = 1,
        .commands_offset = 32,
        .fm_offset = 480,
    };
    MdrvSeq_InitMusic(
        &fixture->sequence, &fixture->music, Arena_Allocator(&fixture->arena),
        M_Event, fixture);
    CHECK(Result_Ignore(MdrvSeq_Play(&fixture->sequence)));
}

static void M_Pulses(FIXTURE *const fixture, const uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) {
        if (!Result_Log(MdrvSeq_Pulse(&fixture->sequence), LOG_LEVEL_ERROR)) {
            TEST_FAIL("pulse failed");
            break;
        }
    }
}

static size_t M_Count(const FIXTURE *const fixture, const MDRV_EVENT_KIND kind)
{
    size_t count = 0;
    for (size_t i = 0; i < fixture->event_count; i++) {
        count += fixture->events[i].kind == kind;
    }
    return count;
}

static const MDRV_EVENT *M_Find(
    const FIXTURE *const fixture, const MDRV_EVENT_KIND kind,
    const size_t index)
{
    size_t count = 0;
    for (size_t i = 0; i < fixture->event_count; i++) {
        if (fixture->events[i].kind == kind && count++ == index) {
            return &fixture->events[i];
        }
    }
    TEST_FAIL("missing event %d index %zu", kind, index);
    return &fixture->events[0];
}

TEST(note_gate_and_rest_pulses)
{
    const uint8_t code[] = { 0xea, 4, 0x40, 8, 0x90, 3, 0x42, 2, 0xff };
    FIXTURE fixture;
    M_Init(&fixture, code, sizeof(code), 0x80);
    M_Pulses(&fixture, 14);
    CHECK_EQ_INT(M_Count(&fixture, MDRV_EVENT_NOTE), 2);
    CHECK_EQ_INT(M_Find(&fixture, MDRV_EVENT_NOTE, 0)->pulse, 0);
    CHECK_EQ_INT(M_Find(&fixture, MDRV_EVENT_KEY_OFF, 0)->pulse, 3);
    CHECK_EQ_INT(M_Find(&fixture, MDRV_EVENT_REST, 0)->pulse, 8);
    CHECK_EQ_INT(M_Find(&fixture, MDRV_EVENT_NOTE, 1)->pulse, 11);
    CHECK_EQ_INT(M_Find(&fixture, MDRV_EVENT_KEY_OFF, 1)->pulse, 11);
    CHECK_EQ_INT(M_Find(&fixture, MDRV_EVENT_TRACK_END, 0)->pulse, 13);
    CHECK(!fixture.sequence.playing);
    CHECK_EQ_INT(fixture.sequence.sync, 0xff);
    Arena_Free(&fixture.arena);
}

TEST(ties_merge_before_gate_and_slur_changes_pitch)
{
    const uint8_t code[] = {
        0xea, 4, 0x40, 4, 0x91, 0x40, 4, 0x42, 4, 0x91, 0x44, 4, 0x90, 2, 0xff,
    };
    FIXTURE fixture;
    M_Init(&fixture, code, sizeof(code), 0x80);
    M_Pulses(&fixture, 19);
    CHECK_EQ_INT(M_Count(&fixture, MDRV_EVENT_TIE), 2);
    CHECK_EQ_INT(M_Count(&fixture, MDRV_EVENT_NOTE), 3);
    CHECK_EQ_INT(M_Find(&fixture, MDRV_EVENT_NOTE, 0)->duration, 8);
    CHECK_EQ_INT(M_Find(&fixture, MDRV_EVENT_KEY_OFF, 0)->pulse, 3);
    CHECK_EQ_INT(M_Find(&fixture, MDRV_EVENT_NOTE, 1)->pulse, 8);
    CHECK(!M_Find(&fixture, MDRV_EVENT_NOTE, 1)->legato);
    CHECK_EQ_INT(M_Find(&fixture, MDRV_EVENT_NOTE, 2)->pulse, 12);
    CHECK(M_Find(&fixture, MDRV_EVENT_NOTE, 2)->legato);
    CHECK_EQ_INT(M_Find(&fixture, MDRV_EVENT_KEY_OFF, 1)->pulse, 13);
    Arena_Free(&fixture.arena);
}

TEST(q_zero_and_explicit_keyoff)
{
    const uint8_t code[] = { 0xea, 0, 0x40, 3, 0xe3, 0x90, 2, 0xff };
    FIXTURE fixture;
    M_Init(&fixture, code, sizeof(code), 0x40);
    M_Pulses(&fixture, 3);
    CHECK(fixture.sequence.tracks[0].key_on);
    CHECK_EQ_INT(M_Count(&fixture, MDRV_EVENT_KEY_OFF), 0);
    M_Pulses(&fixture, 1);
    CHECK_EQ_INT(M_Find(&fixture, MDRV_EVENT_KEY_OFF, 0)->pulse, 3);
    Arena_Free(&fixture.arena);
}

TEST(simple_repeats_and_last_iteration_exit)
{
    const uint8_t code[] = {
        0xe0, 3, 0x40, 2, 0xe1, 0x42, 2, 0xe2, 0xe4, 2, 0x44, 2, 0xe5, 0xff,
    };
    FIXTURE fixture;
    M_Init(&fixture, code, sizeof(code), 0x80);
    M_Pulses(&fixture, 16);
    CHECK_EQ_INT(M_Count(&fixture, MDRV_EVENT_NOTE), 7);
    CHECK_EQ_INT(M_Find(&fixture, MDRV_EVENT_NOTE, 4)->opcode, 0x40);
    CHECK_EQ_INT(M_Find(&fixture, MDRV_EVENT_NOTE, 5)->opcode, 0x44);
    CHECK(!fixture.sequence.playing);
    Arena_Free(&fixture.arena);
}

TEST(nested_shared_counter_repeats)
{
    // F6 targets F7's counter; F7 branches from its last operand byte.
    const uint8_t code[] = {
        0xf6, 2,    13,   0,    0xf6, 3,    3, 0,    0x40, 1,    0xf7,
        0,    0xfb, 0xff, 0x42, 1,    0xf7, 0, 0xf1, 0xff, 0xff,
    };
    FIXTURE fixture;
    M_Init(&fixture, code, sizeof(code), 0x80);
    const uint8_t original[sizeof(code)] = {
        0xf6, 2,    13,   0,    0xf6, 3,    3, 0,    0x40, 1,    0xf7,
        0,    0xfb, 0xff, 0x42, 1,    0xf7, 0, 0xf1, 0xff, 0xff,
    };
    M_Pulses(&fixture, 10);
    CHECK_EQ_INT(M_Count(&fixture, MDRV_EVENT_NOTE), 8);
    CHECK(!fixture.sequence.playing);
    CHECK(memcmp(fixture.bytes + 32, original, sizeof(original)) == 0);
    CHECK_EQ_INT(fixture.sequence.work[32 + 11], 0);
    CHECK_EQ_INT(fixture.sequence.work[32 + 17], 0);
    Arena_Free(&fixture.arena);
}

TEST(counter_last_iteration_exit)
{
    const uint8_t code[] = {
        0xf6, 3, 8, 0, 0x40, 2, 0xf9, 3, 0, 0x42, 2, 0xf7, 0, 0xf6, 0xff, 0xff,
    };
    FIXTURE fixture;
    M_Init(&fixture, code, sizeof(code), 0x80);
    M_Pulses(&fixture, 12);
    CHECK_EQ_INT(M_Count(&fixture, MDRV_EVENT_NOTE), 5);
    CHECK(!fixture.sequence.playing);
    Arena_Free(&fixture.arena);
}

TEST(relative_song_loop_and_absolute_pattern)
{
    const uint8_t code[] = {
        0xfa, 42, 0, 0x42, 2, 0xf3, 0xf9, 0xff, 0xff, 0xff, 0x40, 2, 0xff,
    };
    FIXTURE fixture;
    M_Init(&fixture, code, sizeof(code), 0x80);
    M_Pulses(&fixture, 9);
    CHECK_EQ_INT(M_Count(&fixture, MDRV_EVENT_NOTE), 5);
    CHECK_EQ_INT(M_Count(&fixture, MDRV_EVENT_PATTERN), 5);
    CHECK_EQ_INT(fixture.sequence.loops, 2);
    CHECK_EQ_INT(fixture.sequence.sync, 0xfe);
    CHECK_EQ_INT(M_Find(&fixture, MDRV_EVENT_NOTE, 0)->offset, 42);
    CHECK_EQ_INT(M_Find(&fixture, MDRV_EVENT_NOTE, 1)->offset, 35);
    CHECK_EQ_INT(M_Find(&fixture, MDRV_EVENT_NOTE, 2)->offset, 42);
    Arena_Free(&fixture.arena);
}

TEST(control_events_preserve_encoded_parameters)
{
    const uint8_t code[] = {
        0xe6, 0xfe, 0xe7, 0xf4, 0xe8, 5,    6,    7,    8,    0xe9, 126,  0xea,
        8,    0xeb, 2,    0xec, 119,  0xed, 1,    2,    3,    4,    0xee, 0x22,
        9,    0xef, 255,  0xf0, 0x82, 0xf1, 1,    0xf4, 3,    0xf5, 2,    0xf8,
        42,   0xfb, 2,    3,    4,    5,    0xfc, 6,    7,    8,    9,    0xfd,
        1,    1,    3,    2,    0xf2, 0x40, 3,    0xff, 0xff, 0xff,
    };
    FIXTURE fixture;
    M_Init(&fixture, code, sizeof(code), 0x80);
    M_Pulses(&fixture, 1);
    const MDRV_CHANNEL_STATE *const state = &fixture.sequence.tracks[0];
    CHECK_EQ_INT(state->detune, -2);
    CHECK_EQ_INT(state->transpose, -12);
    CHECK_EQ_INT(state->program, 2);
    CHECK_EQ_INT(state->pan, 1);
    CHECK_EQ_INT(state->volume, 120);
    CHECK_EQ_INT(fixture.sequence.tempo, 126);
    CHECK_EQ_INT(fixture.sequence.sync, 42);
    CHECK_EQ_INT(M_Count(&fixture, MDRV_EVENT_PITCH_LFO), 2);
    CHECK_EQ_INT(M_Count(&fixture, MDRV_EVENT_VOLUME_LFO), 2);
    CHECK_EQ_INT(M_Find(&fixture, MDRV_EVENT_REGISTER, 0)->parameters[1], 9);
    const MDRV_EVENT *const slide = M_Find(&fixture, MDRV_EVENT_PORTAMENTO, 0);
    CHECK_EQ_INT(slide->duration, 3);
    CHECK_EQ_INT(slide->parameter_count, 4);
    CHECK_EQ_INT(slide->parameters[3], 255);
    M_Pulses(&fixture, 3);
    CHECK_EQ_INT(M_Find(&fixture, MDRV_EVENT_KEY_OFF, 0)->pulse, 2);
    Arena_Free(&fixture.arena);
}

TEST(rhythm_volume_and_pan_lengths)
{
    const uint8_t code[] = {
        0xec, 63,   1, 2, 3,    4,  5,    6, 0xec, 0x83,
        20,   0xf1, 3, 2, 0xeb, 11, 0x40, 2, 0xff,
    };
    FIXTURE fixture;
    M_Init(&fixture, code, sizeof(code), 0x10);
    M_Pulses(&fixture, 3);
    CHECK_EQ_INT(M_Find(&fixture, MDRV_EVENT_VOLUME, 0)->parameter_count, 7);
    CHECK_EQ_INT(M_Find(&fixture, MDRV_EVENT_VOLUME, 1)->parameter_count, 2);
    CHECK_EQ_INT(M_Find(&fixture, MDRV_EVENT_PAN, 0)->parameter_count, 2);
    CHECK_EQ_INT(fixture.sequence.tracks[0].volume, 63);
    CHECK(!fixture.sequence.playing);
    Arena_Free(&fixture.arena);
}

TEST(stop_restart_and_no_playback_allocations)
{
    const uint8_t code[] = { 0xe4, 0, 0xea, 0, 0x40, 4, 0xe5, 0xff };
    FIXTURE fixture;
    M_Init(&fixture, code, sizeof(code), 0x80);
    const size_t used = fixture.arena.used;
    M_Pulses(&fixture, 100);
    CHECK_EQ_INT(fixture.sequence.tracks[0].position, 38);
    MdrvSeq_Stop(&fixture.sequence);
    CHECK_EQ_INT(M_Find(&fixture, MDRV_EVENT_KEY_OFF, 0)->pulse, 100);
    CHECK_EQ_INT(M_Find(&fixture, MDRV_EVENT_STOP, 0)->pulse, 100);
    const size_t count = fixture.event_count;
    MdrvSeq_Stop(&fixture.sequence);
    M_Pulses(&fixture, 5);
    CHECK_EQ_INT(fixture.event_count, count);
    CHECK(Result_Ignore(MdrvSeq_Play(&fixture.sequence)));
    M_Pulses(&fixture, 1);
    CHECK_EQ_INT(fixture.sequence.pulse, 1);
    CHECK_EQ_INT(fixture.arena.used, used);
    CHECK_EQ_INT(fixture.bytes[33], 0);
    Arena_Free(&fixture.arena);
}

TEST(shared_counters_are_private_to_each_player)
{
    const uint8_t code[] = {
        0xf6, 3, 3, 0, 0x40, 2, 0xf7, 0, 0xfb, 0xff, 0xff,
    };
    FIXTURE fixture;
    M_Init(&fixture, code, sizeof(code), 0x80);
    MDRV_SEQUENCE other;
    MdrvSeq_InitMusic(
        &other, &fixture.music, Arena_Allocator(&fixture.arena), nullptr,
        nullptr);
    CHECK(Result_Ignore(MdrvSeq_Play(&other)));
    M_Pulses(&fixture, 3);
    CHECK_EQ_INT(fixture.sequence.work[39], 2);
    CHECK(Result_Ignore(MdrvSeq_Pulse(&other)));
    CHECK_EQ_INT(other.work[39], 3);
    CHECK_EQ_INT(fixture.bytes[39], 0);
    CHECK(Result_Ignore(MdrvSeq_Play(&fixture.sequence)));
    CHECK_EQ_INT(fixture.sequence.work[39], 0);
    CHECK_EQ_INT(other.work[39], 3);
    Arena_Free(&fixture.arena);
}

TEST(header_repeat_and_disabled_track)
{
    const uint8_t code[] = { 0x40, 2, 0xff };
    FIXTURE fixture;
    M_Init(&fixture, code, sizeof(code), 0x80);
    fixture.music.flags = 0x80;
    M_Pulses(&fixture, 7);
    CHECK_EQ_INT(M_Count(&fixture, MDRV_EVENT_NOTE), 3);
    CHECK_EQ_INT(fixture.sequence.loops, 2);
    fixture.tracks[0].channel = 0;
    fixture.music.flags = 0;
    CHECK(Result_Ignore(MdrvSeq_Play(&fixture.sequence)));
    M_Pulses(&fixture, 1);
    CHECK(!fixture.sequence.playing);
    CHECK_EQ_INT(M_Count(&fixture, MDRV_EVENT_NOTE), 3);
    Arena_Free(&fixture.arena);
}

TEST(effect_groups_keep_reverse_driver_channels)
{
    FIXTURE fixture;
    memset(&fixture, 0, sizeof(fixture));
    Arena_Init(&fixture.arena, "effect sequencer", 128 * 1024);
    const uint8_t stream[] = { 0xeb, 0x80, 0xec, 12, 0x40, 2, 0xff };
    memcpy(fixture.bytes + 64, stream, sizeof(stream));
    fixture.effects = (MDRV_EFFECTS) {
        .bytes = fixture.bytes,
        .size = sizeof(fixture.bytes),
        .effects = fixture.entries,
        .effect_count = 3,
        .ssg_offset = 480,
    };
    for (uint16_t i = 0; i < 3; i++) {
        fixture.entries[i] = (MDRV_EFFECT) {
            .channel_count = i + 1,
            .base_channel = i == 0 ? 0x42 : 0x40,
            .stream_offsets = { 64, 64, 64 },
        };
    }
    MdrvSeq_InitEffects(
        &fixture.sequence, &fixture.effects, Arena_Allocator(&fixture.arena),
        M_Event, &fixture);
    const size_t used = fixture.arena.used;
    for (uint16_t number = 1; number <= 3; number++) {
        fixture.event_count = 0;
        CHECK(Result_Ignore(MdrvSeq_TriggerEffect(&fixture.sequence, number)));
        M_Pulses(&fixture, 1);
        CHECK_EQ_INT(M_Count(&fixture, MDRV_EVENT_NOTE), number);
        for (uint16_t i = 0; i < number; i++) {
            CHECK_EQ_INT(
                M_Find(&fixture, MDRV_EVENT_NOTE, i)->channel,
                0x43 - number + i);
            CHECK_EQ_INT(
                M_Find(&fixture, MDRV_EVENT_NOTE, i)->track, number - 1 - i);
            CHECK_EQ_INT(
                M_Find(&fixture, MDRV_EVENT_NOTE, i)->effect_number, number);
            CHECK_EQ_INT(
                M_Find(&fixture, MDRV_EVENT_NOTE, i)->source_channel,
                fixture.entries[number - 1].base_channel + number - 1 - i);
        }
        CHECK(Result_Ignore(MdrvSeq_TriggerEffect(&fixture.sequence, 0)));
        CHECK_EQ_INT(fixture.sequence.pulse, 1);
        M_Pulses(&fixture, 2);
        CHECK(!fixture.sequence.playing);
    }
    CHECK(!Result_Ignore(MdrvSeq_TriggerEffect(&fixture.sequence, 4)));
    CHECK_EQ_INT(fixture.arena.used, used);
    Arena_Free(&fixture.arena);
}

TEST(zero_duration_is_not_256_and_bad_streams_fail_bounded)
{
    const uint8_t zero[] = { 0x40, 0, 0x42, 2, 0xff };
    FIXTURE fixture;
    M_Init(&fixture, zero, sizeof(zero), 0x80);
    M_Pulses(&fixture, 1);
    CHECK_EQ_INT(M_Count(&fixture, MDRV_EVENT_NOTE), 2);
    CHECK_EQ_INT(M_Find(&fixture, MDRV_EVENT_NOTE, 1)->pulse, 0);
    const uint8_t invalid[][8] = {
        { 0xf3, 0xfe, 0xff },
        { 0xe4, 0, 0xe5 },
        { 0x40, 0, 0xf3, 0xfc, 0xff },
        { 0xfa, 32, 0 },
        { 0xf3, 0, 0x80 },
        { 0xf6, 2, 0, 0x80 },
        { 0xf9, 0, 0x80 },
        { 0xea, 9 },
        { 0x91 },
        { 0xfe, 0x9f },
        { 0x9f },
        { 0xe2 },
        { 0xe1 },
        { 0xe5 },
        { 0xf2, 0x9f, 2, 0, 0 },
        { 0xe0, 2, 0xe0, 2 },
        { 0xe4, 2, 0xe4, 2 },
    };
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        memcpy(fixture.bytes + 32, invalid[i], 8);
        CHECK(Result_Ignore(MdrvSeq_Play(&fixture.sequence)));
        CHECK(!Result_Ignore(MdrvSeq_Pulse(&fixture.sequence)));
        CHECK(fixture.sequence.failed);
        CHECK(!fixture.sequence.playing);
        CHECK(!Result_Ignore(MdrvSeq_Pulse(&fixture.sequence)));
    }
    fixture.sequence.commands_end = 33;
    CHECK(Result_Ignore(MdrvSeq_Play(&fixture.sequence)));
    CHECK(!Result_Ignore(MdrvSeq_Pulse(&fixture.sequence)));
    Arena_Free(&fixture.arena);
}

TEST(track_order_and_shared_counter_visibility)
{
    const uint8_t code[] = { 0x40, 2, 0xff };
    FIXTURE fixture;
    M_Init(&fixture, code, sizeof(code), 0x40);
    fixture.tracks[1] = (MDRV_TRACK) { .offset = 32, .channel = 0x10 };
    fixture.tracks[2] = (MDRV_TRACK) { .offset = 32, .channel = 0x80 };
    fixture.music.track_count = 3;
    MdrvSeq_Stop(&fixture.sequence);
    MdrvSeq_InitMusic(
        &fixture.sequence, &fixture.music, Arena_Allocator(&fixture.arena),
        M_Event, &fixture);
    CHECK(Result_Ignore(MdrvSeq_Play(&fixture.sequence)));
    M_Pulses(&fixture, 3);
    CHECK_EQ_INT(M_Find(&fixture, MDRV_EVENT_NOTE, 0)->channel, 0x80);
    CHECK_EQ_INT(M_Find(&fixture, MDRV_EVENT_NOTE, 1)->channel, 0x10);
    CHECK_EQ_INT(M_Find(&fixture, MDRV_EVENT_NOTE, 2)->channel, 0x40);
    const uint8_t first[] = { 0xf6, 3, 29, 0, 0x40, 2, 0xff };
    const uint8_t second[] = { 0xf7, 0, 0xfd, 0xff, 0xff };
    memcpy(fixture.bytes + 32, first, sizeof(first));
    memcpy(fixture.bytes + 64, second, sizeof(second));
    fixture.tracks[0].offset = 64;
    fixture.tracks[1].channel = 0;
    CHECK(Result_Ignore(MdrvSeq_Play(&fixture.sequence)));
    M_Pulses(&fixture, 1);
    CHECK_EQ_INT(fixture.sequence.work[65], 0);
    CHECK(!fixture.sequence.tracks[0].active);
    CHECK(fixture.sequence.tracks[2].active);
    CHECK_EQ_INT(fixture.bytes[65], 0);
    Arena_Free(&fixture.arena);
}

TEST(q8_duration_one_rest_slur_and_extended_dispatch)
{
    const uint8_t code[] = { 0xfe, 0x40, 1, 0x91, 0x90, 2, 0x42, 1, 0xff };
    FIXTURE fixture;
    M_Init(&fixture, code, sizeof(code), 0x80);
    M_Pulses(&fixture, 5);
    CHECK_EQ_INT(M_Count(&fixture, MDRV_EVENT_EXTENDED), 1);
    CHECK_EQ_INT(M_Find(&fixture, MDRV_EVENT_EXTENDED, 0)->parameter_count, 0);
    CHECK_EQ_INT(M_Find(&fixture, MDRV_EVENT_REST, 0)->pulse, 1);
    CHECK(M_Find(&fixture, MDRV_EVENT_REST, 0)->legato);
    CHECK_EQ_INT(M_Find(&fixture, MDRV_EVENT_NOTE, 1)->pulse, 3);
    CHECK_EQ_INT(M_Find(&fixture, MDRV_EVENT_KEY_OFF, 0)->pulse, 3);
    CHECK_EQ_INT(M_Find(&fixture, MDRV_EVENT_NOTE, 1)->duration, 1);
    Arena_Free(&fixture.arena);
}

TEST(global_control_writes_follow_hardware_track_order)
{
    const uint8_t code[] = {
        0xe9, 100, 0xf8, 10, 0xee, 0x22, 1, 0x40, 2, 0xff
    };
    FIXTURE fixture;
    M_Init(&fixture, code, sizeof(code), 0x40);
    const uint8_t fm[] = { 0xe9, 150, 0xf8, 20, 0xee, 0x22, 2, 0x40, 2, 0xff };
    const uint8_t rhythm[] = {
        0xe9, 200, 0xf8, 30, 0xee, 0x22, 3, 0x40, 2, 0xff
    };
    memcpy(fixture.bytes + 64, fm, sizeof(fm));
    memcpy(fixture.bytes + 96, rhythm, sizeof(rhythm));
    fixture.tracks[1] = (MDRV_TRACK) { .offset = 64, .channel = 0x80 };
    fixture.tracks[2] = (MDRV_TRACK) { .offset = 96, .channel = 0x10 };
    fixture.music.track_count = 3;
    MdrvSeq_Stop(&fixture.sequence);
    MdrvSeq_InitMusic(
        &fixture.sequence, &fixture.music, Arena_Allocator(&fixture.arena),
        M_Event, &fixture);
    CHECK(Result_Ignore(MdrvSeq_Play(&fixture.sequence)));
    fixture.event_count = 0;
    M_Pulses(&fixture, 1);
    CHECK_EQ_INT(M_Find(&fixture, MDRV_EVENT_TEMPO, 0)->parameters[0], 150);
    CHECK_EQ_INT(M_Find(&fixture, MDRV_EVENT_TEMPO, 1)->parameters[0], 200);
    CHECK_EQ_INT(M_Find(&fixture, MDRV_EVENT_TEMPO, 2)->parameters[0], 100);
    CHECK_EQ_INT(M_Find(&fixture, MDRV_EVENT_REGISTER, 0)->parameters[1], 2);
    CHECK_EQ_INT(M_Find(&fixture, MDRV_EVENT_REGISTER, 1)->parameters[1], 3);
    CHECK_EQ_INT(M_Find(&fixture, MDRV_EVENT_REGISTER, 2)->parameters[1], 1);
    CHECK_EQ_INT(fixture.sequence.tempo, 100);
    CHECK_EQ_INT(fixture.sequence.sync, 10);
    CHECK_EQ_INT(fixture.events[0].kind, MDRV_EVENT_TEMPO);
    CHECK_EQ_INT(fixture.events[1].kind, MDRV_EVENT_SYNC);
    CHECK_EQ_INT(fixture.events[2].kind, MDRV_EVENT_REGISTER);
    Arena_Free(&fixture.arena);
}

TEST(repeat_zero_wrap_and_header_preserves_counter_work)
{
    const uint8_t code[] = { 0xe0, 0, 0x40, 1, 0xe2, 0xff };
    FIXTURE fixture;
    M_Init(&fixture, code, sizeof(code), 0x80);
    M_Pulses(&fixture, 257);
    // The event capture is bounded; persistent state confirms all 256 clocks.
    CHECK(!fixture.sequence.playing);
    CHECK_EQ_INT(fixture.sequence.pulse, 257);
    CHECK_EQ_INT(fixture.sequence.tracks[0].repeat_one_count, 0);
    const uint8_t repeat[] = { 0x40, 1, 0xf7, 2, 0xfb, 0xff, 0xff };
    memcpy(fixture.bytes + 32, repeat, sizeof(repeat));
    fixture.music.flags = 0x80;
    fixture.event_count = 0;
    CHECK(Result_Ignore(MdrvSeq_Play(&fixture.sequence)));
    M_Pulses(&fixture, 5);
    CHECK_EQ_INT(fixture.sequence.loops, 1);
    CHECK_EQ_INT(fixture.sequence.work[35], 0);
    CHECK_EQ_INT(fixture.bytes[35], 2);
    CHECK_EQ_INT(M_Count(&fixture, MDRV_EVENT_NOTE), 4);
    Arena_Free(&fixture.arena);
}

static void M_InitPairEffect(
    FIXTURE *const effects, FIXTURE *const events, const uint8_t *const code,
    const size_t length, const uint8_t channels)
{
    memset(effects, 0, sizeof(*effects));
    Arena_Init(&effects->arena, "paired effects", 128 * 1024);
    memcpy(effects->bytes + 64, code, length);
    effects->entries[0] = (MDRV_EFFECT) { .channel_count = channels,
                                          .base_channel = 0x40,
                                          .stream_offsets = { 64, 64, 64 } };
    effects->entries[1] = (MDRV_EFFECT) { .channel_count = 1,
                                          .base_channel = 0x42,
                                          .stream_offsets = { 64 } };
    effects->effects = (MDRV_EFFECTS) { .bytes = effects->bytes,
                                        .size = sizeof(effects->bytes),
                                        .effects = effects->entries,
                                        .effect_count = 2,
                                        .ssg_offset = 480 };
    MdrvSeq_InitEffects(
        &effects->sequence, &effects->effects, Arena_Allocator(&effects->arena),
        M_Event, events);
    CHECK(Result_Ignore(MdrvSeq_TriggerEffect(&effects->sequence, 1)));
}

static const MDRV_EVENT *M_SourceEvent(
    const FIXTURE *const fixture, const MDRV_EVENT_KIND kind,
    const uint16_t effect, const size_t index)
{
    size_t count = 0;
    for (size_t i = 0; i < fixture->event_count; i++) {
        const MDRV_EVENT *const event = &fixture->events[i];
        if (event->kind == kind && event->effect_number == effect
            && count++ == index) {
            return event;
        }
    }
    TEST_FAIL("missing source event");
    return &fixture->events[0];
}

TEST(paired_effect_ownership_and_same_pulse_event_order)
{
    const uint8_t music_code[] = { 0xeb, 0x80, 0x40, 1,    0xeb, 0x81,
                                   0x42, 1,    0xeb, 0x82, 0xee, 8,
                                   15,   0xf8, 42,   0x44, 1,    0xeb,
                                   0x83, 0x45, 2,    0xff };
    const uint8_t effect_code[] = { 0xeb, 0x81, 0x40, 2, 0xff };
    FIXTURE music, effects;
    M_Init(&music, music_code, sizeof(music_code), 0x42);
    M_InitPairEffect(&effects, &music, effect_code, sizeof(effect_code), 1);
    music.event_count = 0;
    for (uint32_t i = 0; i < 4; i++) {
        CHECK(Result_Ignore(
            MdrvSeq_PulsePair(&music.sequence, &effects.sequence)));
    }
    CHECK_EQ_INT(M_Count(&music, MDRV_EVENT_OUTPUT_MASK), 2);
    CHECK_EQ_INT(M_Find(&music, MDRV_EVENT_OUTPUT_MASK, 0)->parameters[0], 1);
    CHECK_EQ_INT(M_Find(&music, MDRV_EVENT_OUTPUT_MASK, 1)->parameters[0], 0);
    CHECK_EQ_INT(M_Find(&music, MDRV_EVENT_OUTPUT_MASK, 1)->pulse, 2);
    CHECK_EQ_INT(music.events[0].kind, MDRV_EVENT_OUTPUT_MASK);
    CHECK_EQ_INT(music.events[1].kind, MDRV_EVENT_PROGRAM);
    CHECK_EQ_INT(music.events[1].effect_number, 1);
    CHECK_EQ_INT(music.events[2].kind, MDRV_EVENT_NOTE);
    CHECK_EQ_INT(music.events[2].effect_number, 1);
    CHECK_EQ_INT(music.events[3].kind, MDRV_EVENT_PROGRAM);
    CHECK(music.events[3].suppressed);
    CHECK_EQ_INT(M_SourceEvent(&music, MDRV_EVENT_KEY_OFF, 1, 0)->pulse, 1);
    for (size_t i = 0; i < 2; i++) {
        CHECK(M_SourceEvent(&music, MDRV_EVENT_NOTE, 0, i)->suppressed);
        CHECK(M_SourceEvent(&music, MDRV_EVENT_KEY_OFF, 0, i)->suppressed);
    }
    CHECK(!M_SourceEvent(&music, MDRV_EVENT_NOTE, 0, 2)->suppressed);
    CHECK(!M_SourceEvent(&music, MDRV_EVENT_KEY_OFF, 0, 2)->suppressed);
    CHECK(!M_SourceEvent(&music, MDRV_EVENT_NOTE, 0, 3)->suppressed);
    CHECK(!M_Find(&music, MDRV_EVENT_REGISTER, 0)->suppressed);
    CHECK(!M_Find(&music, MDRV_EVENT_SYNC, 0)->suppressed);
    CHECK_EQ_INT(music.sequence.sync, 42);
    CHECK_EQ_INT(music.sequence.tracks[0].program, 0x83);
    CHECK_EQ_INT(music.sequence.tracks[0].remaining, 2);
    CHECK(!effects.sequence.playing);
    CHECK_EQ_INT(M_SourceEvent(&music, MDRV_EVENT_STOP, 1, 0)->pulse, 2);
    Arena_Free(&music.arena);
    Arena_Free(&effects.arena);
}

TEST(paired_retrigger_fewer_channels_and_stop_one_instance)
{
    const uint8_t music_code[] = { 0xea, 0, 0x40, 20, 0xff };
    const uint8_t effect_code[] = { 0xea, 0, 0xeb, 0x80, 0x42, 20, 0xff };
    FIXTURE music, effects;
    M_Init(&music, music_code, sizeof(music_code), 0x40);
    music.tracks[1] = (MDRV_TRACK) { .offset = 32, .channel = 0x41 };
    music.tracks[2] = (MDRV_TRACK) { .offset = 32, .channel = 0x42 };
    music.music.track_count = 3;
    MdrvSeq_Stop(&music.sequence);
    MdrvSeq_InitMusic(
        &music.sequence, &music.music, Arena_Allocator(&music.arena), M_Event,
        &music);
    CHECK(Result_Ignore(MdrvSeq_Play(&music.sequence)));
    M_InitPairEffect(&effects, &music, effect_code, sizeof(effect_code), 3);
    music.event_count = 0;
    const size_t music_used = music.arena.used,
                 effect_used = effects.arena.used;
    CHECK(Result_Ignore(MdrvSeq_PulsePair(&music.sequence, &effects.sequence)));
    CHECK_EQ_INT(M_Count(&music, MDRV_EVENT_OUTPUT_MASK), 3);
    CHECK(Result_Ignore(MdrvSeq_TriggerEffect(&effects.sequence, 2)));
    CHECK_EQ_INT(M_Count(&music, MDRV_EVENT_KEY_OFF), 3);
    for (size_t i = 0; i < 3; i++) {
        CHECK_EQ_INT(M_Find(&music, MDRV_EVENT_KEY_OFF, i)->channel, 0x40 + i);
    }
    CHECK(Result_Ignore(MdrvSeq_PulsePair(&music.sequence, &effects.sequence)));
    CHECK_EQ_INT(M_Count(&music, MDRV_EVENT_OUTPUT_MASK), 5);
    CHECK(!music.sequence.tracks[0].suppressed);
    CHECK(!music.sequence.tracks[1].suppressed);
    CHECK(music.sequence.tracks[2].suppressed);
    CHECK_EQ_INT(music.sequence.tracks[0].remaining, 19);
    MdrvSeq_Stop(&music.sequence);
    CHECK(M_SourceEvent(&music, MDRV_EVENT_KEY_OFF, 0, 2)->suppressed);
    const size_t before = music.event_count;
    CHECK(Result_Ignore(MdrvSeq_PulsePair(&music.sequence, &effects.sequence)));
    CHECK_EQ_INT(music.event_count, before);
    CHECK(effects.sequence.playing);
    CHECK(Result_Ignore(MdrvSeq_Play(&music.sequence)));
    CHECK(Result_Ignore(MdrvSeq_PulsePair(&music.sequence, &effects.sequence)));
    CHECK(music.sequence.tracks[2].suppressed);
    MdrvSeq_Stop(&effects.sequence);
    CHECK(Result_Ignore(MdrvSeq_PulsePair(&music.sequence, &effects.sequence)));
    CHECK(!music.sequence.tracks[2].suppressed);
    CHECK_EQ_INT(music.arena.used, music_used);
    CHECK_EQ_INT(effects.arena.used, effect_used);
    MdrvSeq_Stop(&music.sequence);
    const size_t stopped = music.event_count;
    CHECK(Result_Ignore(MdrvSeq_PulsePair(&music.sequence, &effects.sequence)));
    CHECK_EQ_INT(music.event_count, stopped);
    Arena_Free(&music.arena);
    Arena_Free(&effects.arena);
}

TEST(paired_failure_stops_both_and_restart_clears_latches)
{
    const uint8_t music_code[] = { 0xea, 0, 0x40, 20, 0xff };
    const uint8_t effect_code[] = { 0x9f };
    FIXTURE music, effects;
    M_Init(&music, music_code, sizeof(music_code), 0x80);
    M_InitPairEffect(&effects, &music, effect_code, sizeof(effect_code), 1);
    CHECK(
        !Result_Ignore(MdrvSeq_PulsePair(&music.sequence, &effects.sequence)));
    CHECK(!music.sequence.playing);
    CHECK(!effects.sequence.playing);
    CHECK(music.sequence.failed);
    CHECK(effects.sequence.failed);
    CHECK(
        !Result_Ignore(MdrvSeq_PulsePair(&music.sequence, &effects.sequence)));
    CHECK(Result_Ignore(MdrvSeq_Play(&music.sequence)));
    CHECK(!music.sequence.failed);
    CHECK(Result_Ignore(MdrvSeq_TriggerEffect(&effects.sequence, 1)));
    CHECK(!effects.sequence.failed);
    MdrvSeq_Stop(&effects.sequence);
    CHECK(Result_Ignore(MdrvSeq_PulsePair(&music.sequence, &effects.sequence)));
    CHECK(music.sequence.tracks[0].key_on);
    CHECK(!Result_Ignore(MdrvSeq_PulsePair(&music.sequence, &music.sequence)));
    CHECK(!Result_Ignore(MdrvSeq_Play(&effects.sequence)));
    CHECK(!Result_Ignore(MdrvSeq_TriggerEffect(&music.sequence, 1)));
    Arena_Free(&music.arena);
    Arena_Free(&effects.arena);
}

TEST(q_zero_ended_music_holds_until_all_tracks_stop)
{
    const uint8_t code[] = { 0xea, 0, 0x40, 1, 0xff };
    const uint8_t other[] = { 0xea, 0, 0x42, 3, 0xff };
    FIXTURE fixture;
    M_Init(&fixture, code, sizeof(code), 0x80);
    memcpy(fixture.bytes + 64, other, sizeof(other));
    fixture.tracks[1] = (MDRV_TRACK) { .offset = 64, .channel = 0x81 };
    fixture.music.track_count = 2;
    MdrvSeq_Stop(&fixture.sequence);
    MdrvSeq_InitMusic(
        &fixture.sequence, &fixture.music, Arena_Allocator(&fixture.arena),
        M_Event, &fixture);
    CHECK(Result_Ignore(MdrvSeq_Play(&fixture.sequence)));
    fixture.event_count = 0;
    M_Pulses(&fixture, 2);
    CHECK(!fixture.sequence.tracks[0].active);
    CHECK(fixture.sequence.tracks[0].key_on);
    CHECK_EQ_INT(M_Count(&fixture, MDRV_EVENT_KEY_OFF), 0);
    M_Pulses(&fixture, 2);
    CHECK_EQ_INT(M_Count(&fixture, MDRV_EVENT_KEY_OFF), 0);
    CHECK_EQ_INT(M_Find(&fixture, MDRV_EVENT_DRIVER_RESET, 0)->pulse, 3);
    CHECK_EQ_INT(
        M_Find(&fixture, MDRV_EVENT_DRIVER_RESET, 0)->parameters[0], 0);
    CHECK(!fixture.sequence.tracks[0].key_on);
    CHECK(!fixture.sequence.tracks[1].key_on);
    Arena_Free(&fixture.arena);
}

TEST(paired_global_controls_remain_shared_in_command_order)
{
    const uint8_t music_code[] = { 0xe9, 100,  0xf8, 10,   0x40, 1,   0xe9,
                                   110,  0xf8, 11,   0x42, 8,    0xff };
    const uint8_t effect_code[] = { 0xe9, 200,  0xf8, 20,   0x40, 1,   0xe9,
                                    210,  0xf8, 21,   0x42, 2,    0xff };
    FIXTURE music, effects;
    M_Init(&music, music_code, sizeof(music_code), 0x42);
    M_InitPairEffect(&effects, &music, effect_code, sizeof(effect_code), 1);
    CHECK(Result_Ignore(MdrvSeq_PulsePair(&music.sequence, &effects.sequence)));
    CHECK_EQ_INT(music.sequence.tempo, 100);
    CHECK_EQ_INT(effects.sequence.tempo, 100);
    CHECK_EQ_INT(music.sequence.sync, 10);
    CHECK_EQ_INT(effects.sequence.sync, 10);
    CHECK(Result_Ignore(MdrvSeq_PulsePair(&music.sequence, &effects.sequence)));
    CHECK_EQ_INT(music.sequence.tempo, 110);
    CHECK_EQ_INT(effects.sequence.tempo, 110);
    CHECK_EQ_INT(music.sequence.sync, 11);
    CHECK_EQ_INT(effects.sequence.sync, 11);
    for (int i = 0; i < 3; i++) {
        CHECK(Result_Ignore(
            MdrvSeq_PulsePair(&music.sequence, &effects.sequence)));
    }
    CHECK(!effects.sequence.playing);
    CHECK_EQ_INT(music.sequence.sync, 11);
    CHECK_EQ_INT(effects.sequence.sync, 11);
    MdrvSeq_Stop(&music.sequence);
    CHECK(Result_Ignore(MdrvSeq_PulsePair(&music.sequence, &effects.sequence)));
    CHECK_EQ_INT(music.sequence.sync, 0xff);
    CHECK_EQ_INT(effects.sequence.sync, 0xff);
    Arena_Free(&music.arena);
    Arena_Free(&effects.arena);
}

TEST(paired_header_repeat_cancels_effects_and_preserves_music_counters)
{
    const uint8_t music_code[] = { 0xeb, 0x81, 0xec, 12, 0xe6, 2,
                                   0xe7, 3,    0xf1, 1,  0xea, 4,
                                   0xf7, 2,    1,    0,  0xff };
    const uint8_t effect_code[] = { 0xeb, 0x82, 0x42, 10, 0xff };
    FIXTURE music, effects;
    M_Init(&music, music_code, sizeof(music_code), 0x42);
    music.music.flags = 0x80;
    M_InitPairEffect(&effects, &music, effect_code, sizeof(effect_code), 1);
    music.event_count = 0;
    CHECK(Result_Ignore(MdrvSeq_PulsePair(&music.sequence, &effects.sequence)));
    CHECK_EQ_INT(music.sequence.loops, 1);
    CHECK_EQ_INT(music.sequence.sync, 0xff);
    CHECK_EQ_INT(effects.sequence.sync, 0xff);
    CHECK_EQ_INT(music.sequence.work[45], 1);
    CHECK_EQ_INT(music.bytes[45], 2);
    CHECK_EQ_INT(music.sequence.tracks[0].volume, 12);
    CHECK_EQ_INT(music.sequence.tracks[0].program, 0x81);
    CHECK_EQ_INT(music.sequence.tracks[0].gate, 8);
    CHECK_EQ_INT(music.sequence.tracks[0].pan, 3);
    CHECK_EQ_INT(music.sequence.tracks[0].detune, 0);
    CHECK_EQ_INT(music.sequence.tracks[0].transpose, 0);
    CHECK(!music.sequence.tracks[0].suppressed);
    CHECK(!effects.sequence.playing);
    CHECK(!effects.sequence.tracks[0].active);
    CHECK_EQ_INT(effects.sequence.tracks[0].volume, 12);
    CHECK_EQ_INT(effects.sequence.tracks[0].program, 0x81);
    CHECK_EQ_INT(M_Count(&music, MDRV_EVENT_KEY_OFF), 0);
    CHECK_EQ_INT(M_Count(&music, MDRV_EVENT_DRIVER_RESET), 1);
    CHECK_EQ_INT(M_Find(&music, MDRV_EVENT_DRIVER_RESET, 0)->parameters[0], 1);
    CHECK_EQ_INT(M_Count(&music, MDRV_EVENT_OUTPUT_MASK), 1);
    CHECK(Result_Ignore(MdrvSeq_PulsePair(&music.sequence, &effects.sequence)));
    CHECK_EQ_INT(music.sequence.loops, 2);
    CHECK_EQ_INT(music.sequence.work[45], 0);
    CHECK_EQ_INT(M_Count(&music, MDRV_EVENT_OUTPUT_MASK), 1);
    CHECK_EQ_INT(M_Count(&music, MDRV_EVENT_NOTE), 1);
    CHECK_EQ_INT(M_Count(&music, MDRV_EVENT_DRIVER_RESET), 2);
    CHECK_EQ_INT(M_Count(&music, MDRV_EVENT_STOP), 1);
    CHECK_EQ_INT(effects.sequence.pulse, 1);
    Arena_Free(&music.arena);
    Arena_Free(&effects.arena);
}

TEST(paired_already_stopped_music_permits_effect_only_controls)
{
    const uint8_t music_code[] = { 0x40, 1, 0xff };
    const uint8_t effect_code[] = { 0xe9, 200, 0xf8, 20, 0x42, 2, 0xff };
    FIXTURE music, effects;
    M_Init(&music, music_code, sizeof(music_code), 0x42);
    MdrvSeq_Stop(&music.sequence);
    M_InitPairEffect(&effects, &music, effect_code, sizeof(effect_code), 1);
    music.event_count = 0;
    for (int i = 0; i < 4; i++) {
        CHECK(Result_Ignore(
            MdrvSeq_PulsePair(&music.sequence, &effects.sequence)));
    }
    CHECK(!music.sequence.playing);
    CHECK(!effects.sequence.playing);
    CHECK_EQ_INT(music.sequence.tempo, 200);
    CHECK_EQ_INT(music.sequence.sync, 20);
    CHECK_EQ_INT(effects.sequence.sync, 20);
    CHECK_EQ_INT(M_Count(&music, MDRV_EVENT_DRIVER_RESET), 0);
    CHECK_EQ_INT(M_Count(&music, MDRV_EVENT_NOTE), 1);
    CHECK_EQ_INT(M_SourceEvent(&music, MDRV_EVENT_NOTE, 1, 0)->pulse, 0);
    Arena_Free(&music.arena);
    Arena_Free(&effects.arena);
}

TEST(paired_simultaneous_effect_ends_release_last_active_channel_first)
{
    const uint8_t music_code[] = { 0x40, 1, 0x42, 1, 0x44, 3, 0xff };
    const uint8_t effect_code[] = { 0x47, 1, 0xff };
    FIXTURE music, effects;
    M_Init(&music, music_code, sizeof(music_code), 0x40);
    music.tracks[1] = (MDRV_TRACK) { .offset = 32, .channel = 0x41 };
    music.tracks[2] = (MDRV_TRACK) { .offset = 32, .channel = 0x42 };
    music.music.track_count = 3;
    MdrvSeq_Stop(&music.sequence);
    MdrvSeq_InitMusic(
        &music.sequence, &music.music, Arena_Allocator(&music.arena), M_Event,
        &music);
    CHECK(Result_Ignore(MdrvSeq_Play(&music.sequence)));
    M_InitPairEffect(&effects, &music, effect_code, sizeof(effect_code), 2);
    music.event_count = 0;
    CHECK(Result_Ignore(MdrvSeq_PulsePair(&music.sequence, &effects.sequence)));
    CHECK(Result_Ignore(MdrvSeq_PulsePair(&music.sequence, &effects.sequence)));
    CHECK(!effects.sequence.playing);
    CHECK(music.sequence.tracks[1].suppressed);
    CHECK(!music.sequence.tracks[2].suppressed);
    CHECK(M_SourceEvent(&music, MDRV_EVENT_NOTE, 0, 4)->suppressed);
    CHECK(!M_SourceEvent(&music, MDRV_EVENT_NOTE, 0, 5)->suppressed);
    CHECK_EQ_INT(M_Find(&music, MDRV_EVENT_OUTPUT_MASK, 2)->channel, 0x42);
    CHECK_EQ_INT(M_Find(&music, MDRV_EVENT_OUTPUT_MASK, 2)->pulse, 1);
    CHECK(Result_Ignore(MdrvSeq_PulsePair(&music.sequence, &effects.sequence)));
    CHECK(!music.sequence.tracks[1].suppressed);
    CHECK_EQ_INT(M_Find(&music, MDRV_EVENT_OUTPUT_MASK, 3)->channel, 0x41);
    CHECK_EQ_INT(M_Find(&music, MDRV_EVENT_OUTPUT_MASK, 3)->pulse, 2);
    Arena_Free(&music.arena);
    Arena_Free(&effects.arena);
}

TEST(header_repeat_retains_volume_program_but_fresh_play_clears)
{
    const uint8_t code[] = { 0xeb, 2,    0xec, 119,  0xf1, 1, 0xea, 4,
                             0xe6, 0xfe, 0xe7, 0xfc, 0x40, 1, 0xff };
    FIXTURE fixture;
    M_Init(&fixture, code, sizeof(code), 0x80);
    fixture.music.flags = 0x80;
    M_Pulses(&fixture, 2);
    CHECK_EQ_INT(fixture.sequence.loops, 1);
    CHECK_EQ_INT(fixture.sequence.tracks[0].volume, 119);
    CHECK_EQ_INT(fixture.sequence.tracks[0].program, 2);
    CHECK_EQ_INT(fixture.sequence.tracks[0].gate, 8);
    CHECK_EQ_INT(fixture.sequence.tracks[0].pan, 3);
    CHECK_EQ_INT(fixture.sequence.tracks[0].detune, 0);
    CHECK_EQ_INT(fixture.sequence.tracks[0].transpose, 0);
    CHECK(Result_Ignore(MdrvSeq_Play(&fixture.sequence)));
    CHECK_EQ_INT(fixture.sequence.tracks[0].volume, 0);
    CHECK_EQ_INT(fixture.sequence.tracks[0].program, 0);
    Arena_Free(&fixture.arena);
}

TEST(paired_final_fm_ff_aborts_later_effect_clock)
{
    const uint8_t music_code[] = { 0xff };
    const uint8_t effect_code[] = { 0xe9, 200,  0xf8, 20,  0xeb,
                                    0x82, 0x42, 10,   0xff };
    FIXTURE music, effects;
    M_Init(&music, music_code, sizeof(music_code), 0x80);
    M_InitPairEffect(&effects, &music, effect_code, sizeof(effect_code), 1);
    music.event_count = 0;
    const size_t music_used = music.arena.used,
                 effect_used = effects.arena.used;
    CHECK(Result_Ignore(MdrvSeq_PulsePair(&music.sequence, &effects.sequence)));
    CHECK_EQ_INT(music.event_count, 4);
    CHECK_EQ_INT(music.events[0].kind, MDRV_EVENT_TRACK_END);
    CHECK_EQ_INT(music.events[1].kind, MDRV_EVENT_DRIVER_RESET);
    CHECK_EQ_INT(music.events[1].parameters[0], 0);
    CHECK_EQ_INT(music.events[2].kind, MDRV_EVENT_STOP);
    CHECK_EQ_INT(music.events[2].effect_number, 1);
    CHECK_EQ_INT(music.events[3].kind, MDRV_EVENT_STOP);
    CHECK_EQ_INT(music.events[3].effect_number, 0);
    CHECK(!music.sequence.playing);
    CHECK(!effects.sequence.playing);
    CHECK_EQ_INT(music.sequence.tempo, 0);
    CHECK_EQ_INT(music.sequence.sync, 0xff);
    CHECK_EQ_INT(M_Count(&music, MDRV_EVENT_NOTE), 0);
    CHECK_EQ_INT(M_Count(&music, MDRV_EVENT_PROGRAM), 0);
    CHECK_EQ_INT(M_Count(&music, MDRV_EVENT_SYNC), 0);
    CHECK_EQ_INT(music.sequence.pulse, 1);
    CHECK_EQ_INT(effects.sequence.pulse, 1);
    CHECK(Result_Ignore(MdrvSeq_PulsePair(&music.sequence, &effects.sequence)));
    CHECK_EQ_INT(music.event_count, 4);
    CHECK_EQ_INT(music.sequence.pulse, 1);
    CHECK_EQ_INT(effects.sequence.pulse, 1);
    CHECK_EQ_INT(music.arena.used, music_used);
    CHECK_EQ_INT(effects.arena.used, effect_used);
    CHECK(Result_Ignore(MdrvSeq_TriggerEffect(&effects.sequence, 1)));
    CHECK(Result_Ignore(MdrvSeq_PulsePair(&music.sequence, &effects.sequence)));
    CHECK_EQ_INT(M_Count(&music, MDRV_EVENT_NOTE), 1);
    CHECK_EQ_INT(music.sequence.sync, 20);
    Arena_Free(&music.arena);
    Arena_Free(&effects.arena);
}

TEST(paired_final_ssg_ff_preserves_earlier_events_and_skips_later_effect)
{
    const uint8_t music_code[] = { 0xff };
    const uint8_t effect_code[] = { 0xeb, 0x80, 0x47, 10, 0xff };
    FIXTURE music, effects;
    M_Init(&music, music_code, sizeof(music_code), 0x41);
    M_InitPairEffect(&effects, &music, effect_code, sizeof(effect_code), 3);
    music.event_count = 0;
    CHECK(Result_Ignore(MdrvSeq_PulsePair(&music.sequence, &effects.sequence)));
    CHECK_EQ_INT(M_Count(&music, MDRV_EVENT_NOTE), 2);
    CHECK_EQ_INT(M_SourceEvent(&music, MDRV_EVENT_NOTE, 1, 0)->channel, 0x40);
    CHECK_EQ_INT(M_SourceEvent(&music, MDRV_EVENT_NOTE, 1, 1)->channel, 0x41);
    CHECK_EQ_INT(M_Count(&music, MDRV_EVENT_KEY_OFF), 0);
    CHECK_EQ_INT(M_Count(&music, MDRV_EVENT_DRIVER_RESET), 1);
    size_t reset = 0;
    for (size_t i = 0; i < music.event_count; i++) {
        if (music.events[i].kind == MDRV_EVENT_DRIVER_RESET) {
            reset = i;
        }
    }
    CHECK_EQ_INT(music.events[reset - 1].kind, MDRV_EVENT_TRACK_END);
    CHECK_EQ_INT(music.events[reset + 1].kind, MDRV_EVENT_STOP);
    CHECK_EQ_INT(music.events[reset + 2].kind, MDRV_EVENT_STOP);
    CHECK(!effects.sequence.tracks[0].active);
    CHECK(!effects.sequence.tracks[1].active);
    CHECK(!effects.sequence.tracks[2].active);
    CHECK(!music.sequence.tracks[0].suppressed);
    const size_t count = music.event_count;
    CHECK(Result_Ignore(MdrvSeq_PulsePair(&music.sequence, &effects.sequence)));
    CHECK_EQ_INT(music.event_count, count);
    Arena_Free(&music.arena);
    Arena_Free(&effects.arena);
}

TEST(paired_header_repeat_on_fm_cancels_effect_before_it_advances)
{
    const uint8_t music_code[] = { 0xeb, 7, 0xec, 119, 0xf8, 42, 0xff };
    const uint8_t effect_code[] = { 0x47, 10, 0xff };
    FIXTURE music, effects;
    M_Init(&music, music_code, sizeof(music_code), 0x80);
    music.music.flags = 0x80;
    M_InitPairEffect(&effects, &music, effect_code, sizeof(effect_code), 1);
    music.event_count = 0;
    CHECK(Result_Ignore(MdrvSeq_PulsePair(&music.sequence, &effects.sequence)));
    CHECK_EQ_INT(M_Count(&music, MDRV_EVENT_NOTE), 0);
    CHECK_EQ_INT(M_Count(&music, MDRV_EVENT_KEY_OFF), 0);
    CHECK_EQ_INT(M_Count(&music, MDRV_EVENT_LOOP), 1);
    CHECK_EQ_INT(M_Find(&music, MDRV_EVENT_DRIVER_RESET, 0)->parameters[0], 1);
    CHECK_EQ_INT(music.sequence.tracks[0].position, 32);
    CHECK_EQ_INT(music.sequence.tracks[0].volume, 119);
    CHECK_EQ_INT(music.sequence.tracks[0].program, 7);
    CHECK_EQ_INT(music.sequence.sync, 0xff);
    CHECK(music.sequence.playing);
    CHECK(!effects.sequence.playing);
    CHECK(Result_Ignore(MdrvSeq_PulsePair(&music.sequence, &effects.sequence)));
    CHECK_EQ_INT(music.sequence.loops, 2);
    CHECK_EQ_INT(M_Count(&music, MDRV_EVENT_STOP), 1);
    Arena_Free(&music.arena);
    Arena_Free(&effects.arena);
}

TEST(paired_f3_branch_does_not_reset_active_effects)
{
    const uint8_t music_code[] = { 0x40, 1, 0xf3, 0xfc, 0xff };
    const uint8_t effect_code[] = { 0x47, 10, 0xff };
    FIXTURE music, effects;
    M_Init(&music, music_code, sizeof(music_code), 0x80);
    M_InitPairEffect(&effects, &music, effect_code, sizeof(effect_code), 1);
    music.event_count = 0;
    for (int i = 0; i < 4; i++) {
        CHECK(Result_Ignore(
            MdrvSeq_PulsePair(&music.sequence, &effects.sequence)));
    }
    CHECK_EQ_INT(M_Count(&music, MDRV_EVENT_DRIVER_RESET), 0);
    CHECK_EQ_INT(music.sequence.loops, 3);
    CHECK_EQ_INT(effects.sequence.tracks[0].remaining, 7);
    CHECK(effects.sequence.playing);
    CHECK_EQ_INT(M_Count(&music, MDRV_EVENT_NOTE), 5);
    Arena_Free(&music.arena);
    Arena_Free(&effects.arena);
}
