#include "pc98/pmd/ssg.h"

#include "core/base/arena.h"
#include "test.h"

typedef struct {
    MUSIC_SSG ssg;
    PMD_CONTROL control;
    ARENA arena;
    PMD_CHIP chip;
    uint8_t delta;
} FIXTURE;

static void M_Init(FIXTURE *const fixture)
{
    memset(fixture, 0, sizeof(*fixture));
    Arena_Init(&fixture->arena, "SSG test", 4 * 1024 * 1024);
    PmdChip_Init(
        &fixture->chip, Arena_Allocator(&fixture->arena), nullptr, nullptr);
    PmdChip_Write(&fixture->chip, 7, 0xbf);
    MusicSsg_Init(&fixture->ssg, &fixture->control, &fixture->chip);
    MusicSsg_Reset(&fixture->ssg);
}

static void M_Begin(FIXTURE *const fixture, const uint8_t remaining)
{
    MusicSsg_Begin(&fixture->ssg, 6, 0, remaining);
}

static void M_Advance(FIXTURE *const fixture)
{
    MusicSsg_Advance(&fixture->ssg, 6, 0, fixture->delta);
}

static void M_Note(
    FIXTURE *const fixture, const uint8_t note, const uint8_t duration,
    const bool slur, const bool next_tie)
{
    CHECK(Result_Ignore(MusicSsg_Note(
        &fixture->ssg, 6, 0, note, duration, slur, next_tie, fixture->delta)));
}

static void M_Close(FIXTURE *const fixture)
{
    uint8_t volumes[3];
    memcpy(volumes, fixture->chip.registers + 8, sizeof(volumes));
    MusicSsg_Stop(&fixture->ssg);
    CHECK_EQ_INT(fixture->chip.registers[7], 0xbf);
    CHECK(memcmp(volumes, fixture->chip.registers + 8, sizeof(volumes)) == 0);
    Arena_Free(&fixture->arena);
}

TEST(note_sets_volume_pitch_and_mixer_and_releases_without_mixer_change)
{
    FIXTURE fixture;
    M_Init(&fixture);
    M_Begin(&fixture, 1);
    M_Note(&fixture, 0x40, 4, false, false);
    M_Advance(&fixture);
    const uint8_t regs[] = { 8, 0, 1, 7 };
    const uint8_t values[] = { 8, 239, 0, 0xbe };
    for (size_t i = 0; i < ARRAY_SIZE(regs); i++) {
        CHECK_EQ_INT(fixture.chip.registers[regs[i]], values[i]);
    }
    uint8_t before[16];
    memcpy(before, fixture.chip.registers, sizeof(before));
    for (uint8_t remaining = 4; remaining > 1; remaining--) {
        M_Begin(&fixture, remaining);
        M_Advance(&fixture);
        CHECK(memcmp(before, fixture.chip.registers, sizeof(before)) == 0);
    }
    M_Begin(&fixture, 1);
    MusicSsg_End(&fixture.ssg, 6);
    M_Advance(&fixture);
    CHECK_EQ_INT(fixture.chip.registers[8], 0);
    CHECK_EQ_INT(fixture.chip.registers[7], 0xbe);
    M_Close(&fixture);
}

TEST(normal_attack_decay_and_gate_release_follow_byte_counters)
{
    FIXTURE fixture;
    M_Init(&fixture);
    M_Begin(&fixture, 1);
    const uint8_t volume[] = { 12 }, gate[] = { 2 },
                  envelope[] = { 2, 254, 2, 2 };
    MusicSsg_Command(&fixture.ssg, 6, 0, 0xfd, volume);
    MusicSsg_Command(&fixture.ssg, 6, 0, 0xfe, gate);
    MusicSsg_Command(&fixture.ssg, 6, 0, 0xf0, envelope);
    M_Note(&fixture, 0x40, 8, false, false);
    M_Advance(&fixture);
    const uint8_t volumes[] = { 12, 12, 10, 10, 9, 9, 9, 8, 8, 7 };
    CHECK_EQ_INT(fixture.chip.registers[8], volumes[0]);
    for (size_t i = 1; i < ARRAY_SIZE(volumes); i++) {
        M_Begin(&fixture, (uint8_t)(9 - i));
        if (i == 8) {
            MusicSsg_End(&fixture.ssg, 6);
        }
        M_Advance(&fixture);
        CHECK_EQ_INT(fixture.chip.registers[8], volumes[i]);
    }
    CHECK_EQ_INT(fixture.ssg.channels[0].envelope.mode, 2);
    M_Close(&fixture);
}

TEST(extended_envelope_takes_initial_step_then_batches_timer_a_updates)
{
    FIXTURE fixture;
    M_Init(&fixture);
    M_Begin(&fixture, 1);
    const uint8_t volume[] = { 15 }, envelope[] = { 31, 16, 0, 0xaf, 0 },
                  timer[] = { 1 };
    MusicSsg_Command(&fixture.ssg, 6, 0, 0xfd, volume);
    MusicSsg_Command(&fixture.ssg, 6, 0, 0xcd, envelope);
    MusicSsg_Command(&fixture.ssg, 6, 0, 0xc9, timer);
    M_Note(&fixture, 0x40, 12, false, false);
    M_Advance(&fixture);
    CHECK_EQ_INT(fixture.ssg.channels[0].envelope.level, 15);
    CHECK_EQ_INT(fixture.ssg.channels[0].envelope.stage, 2);
    CHECK_EQ_INT(fixture.chip.registers[8], 15);
    uint8_t before[16];
    memcpy(before, fixture.chip.registers, sizeof(before));
    M_Begin(&fixture, 12);
    M_Advance(&fixture);
    CHECK(memcmp(before, fixture.chip.registers, sizeof(before)) == 0);
    fixture.delta = 2;
    M_Begin(&fixture, 11);
    M_Advance(&fixture);
    CHECK_EQ_INT(fixture.ssg.channels[0].envelope.level, 14);
    CHECK_EQ_INT(fixture.chip.registers[8], 14);
    M_Close(&fixture);
}

TEST(envelope_mode_switches_retain_shared_rate_and_counter_bytes)
{
    FIXTURE fixture;
    M_Init(&fixture);
    M_Begin(&fixture, 1);
    const uint8_t normal[] = { 3, 254, 4, 5 },
                  extended[] = { 31, 16, 0, 0xaf, 1 },
                  final[] = { 0, 253, 0, 2 };
    MusicSsg_Command(&fixture.ssg, 6, 0, 0xf0, normal);
    M_Note(&fixture, 0x40, 1, false, false);
    M_Advance(&fixture);
    M_Begin(&fixture, 1);
    MusicSsg_Command(&fixture.ssg, 6, 0, 0xcd, extended);
    M_Note(&fixture, 0x0f, 1, false, false);
    M_Advance(&fixture);
    CHECK_EQ_INT(fixture.ssg.channels[0].envelope.mode, 255);
    CHECK_EQ_INT(fixture.ssg.channels[0].envelope.stage, 4);
    CHECK_EQ_INT(fixture.ssg.channels[0].envelope.counters[3], 5);
    M_Begin(&fixture, 1);
    MusicSsg_Command(&fixture.ssg, 6, 0, 0xf0, final);
    M_Note(&fixture, 0x41, 2, false, false);
    M_Advance(&fixture);
    CHECK_EQ_INT(fixture.ssg.channels[0].envelope.mode, 1);
    CHECK_EQ_INT(fixture.chip.registers[8], 5);
    M_Close(&fixture);
}

TEST(tied_previous_note_keeps_envelope_and_rest_enters_release)
{
    FIXTURE fixture;
    M_Init(&fixture);
    M_Begin(&fixture, 1);
    const uint8_t volume[] = { 15 };
    const uint8_t envelope[] = { 0, 0, 1, 1 };
    MusicSsg_Command(&fixture.ssg, 6, 0, 0xfd, volume);
    MusicSsg_Command(&fixture.ssg, 6, 0, 0xf0, envelope);
    M_Note(&fixture, 0x40, 2, false, true);
    M_Advance(&fixture);
    M_Begin(&fixture, 2);
    M_Advance(&fixture);
    M_Begin(&fixture, 1);
    MusicSsg_Command(&fixture.ssg, 6, 0, 0xfb, nullptr);
    M_Note(&fixture, 0x4c, 2, true, false);
    M_Advance(&fixture);
    CHECK_EQ_INT(fixture.ssg.channels[0].pitch.note, 0x40);
    CHECK_EQ_INT(fixture.chip.registers[8], 14);
    CHECK_EQ_INT(fixture.ssg.channels[0].note.gate, 0);
    M_Begin(&fixture, 2);
    M_Advance(&fixture);
    M_Begin(&fixture, 1);
    M_Note(&fixture, 0x0f, 2, false, false);
    M_Advance(&fixture);
    CHECK_EQ_INT(fixture.ssg.channels[0].pitch.note, 255);
    CHECK_EQ_INT(fixture.ssg.channels[0].envelope.mode, 2);
    CHECK_EQ_INT(fixture.chip.registers[8], 13);
    M_Begin(&fixture, 2);
    M_Advance(&fixture);
    CHECK_EQ_INT(fixture.chip.registers[8], 12);
    M_Close(&fixture);
}

TEST(noise_and_mixer_are_shared_and_a_masked_part_keeps_global_noise_commands)
{
    for (uint8_t masked = 0; masked < 2; masked++) {
        FIXTURE fixture;
        M_Init(&fixture);
        const uint8_t pattern[] = { 0x38 };
        const uint8_t noise[] = { 5 };
        MusicSsg_Begin(&fixture.ssg, 6, masked, 1);
        MusicSsg_Command(&fixture.ssg, 6, masked, 0xed, pattern);
        MusicSsg_Command(&fixture.ssg, 6, masked, 0xee, noise);
        CHECK(Result_Ignore(
            MusicSsg_Note(&fixture.ssg, 6, masked, 0x40, 2, false, false, 0)));
        MusicSsg_Advance(&fixture.ssg, 6, masked, 0);
        MusicSsg_Begin(&fixture.ssg, 7, 0, 1);
        CHECK(Result_Ignore(
            MusicSsg_Note(&fixture.ssg, 7, 0, 0x41, 2, false, false, 0)));
        MusicSsg_Advance(&fixture.ssg, 7, 0, 0);
        CHECK_EQ_INT(fixture.chip.registers[6], 5);
        CHECK_EQ_INT(fixture.chip.registers[7], masked != 0 ? 0xbd : 0xb5);
        M_Close(&fixture);
    }
}

TEST(ssg_portamento_uses_period_differences_and_distributes_the_remainder)
{
    FIXTURE fixture;
    M_Init(&fixture);
    M_Begin(&fixture, 1);
    CHECK(Result_Ignore(
        MusicSsg_Glide(&fixture.ssg, 6, 0, 0x40, 0x41, 4, false, false, 0)));
    M_Advance(&fixture);
    const uint8_t periods[] = { 239, 235, 231, 228 };
    CHECK_EQ_INT(fixture.chip.registers[0], periods[0]);
    CHECK_EQ_INT(fixture.chip.registers[1], 0);
    for (size_t i = 1; i < ARRAY_SIZE(periods); i++) {
        M_Begin(&fixture, (uint8_t)(5 - i));
        M_Advance(&fixture);
        CHECK_EQ_INT(fixture.chip.registers[0], periods[i]);
        CHECK_EQ_INT(fixture.chip.registers[1], 0);
    }
    M_Close(&fixture);
}

TEST(detune_clamps_and_extended_negative_rounding_follow_word_arithmetic)
{
    static const struct {
        uint8_t detune[2];
        uint8_t note;
        bool extended;
        uint16_t period;
    } cases[] = {
        { { 0x88, 0x13 }, 0x00, false, 0 },
        { { 0x78, 0xec }, 0x00, false, 4095 },
        { { 255, 255 }, 0x40, true, 241 },
    };
    for (size_t i = 0; i < ARRAY_SIZE(cases); i++) {
        FIXTURE fixture;
        M_Init(&fixture);
        M_Begin(&fixture, 1);
        if (cases[i].extended) {
            const uint8_t mode[] = { 1 };
            MusicSsg_Command(&fixture.ssg, 6, 0, 0xcc, mode);
        }
        MusicSsg_Command(&fixture.ssg, 6, 0, 0xfa, cases[i].detune);
        M_Note(&fixture, cases[i].note, 2, false, false);
        M_Advance(&fixture);
        const uint16_t period =
            (uint16_t)(fixture.chip.registers[0]
                       | (uint16_t)fixture.chip.registers[1] << 8);
        CHECK_EQ_INT(period, cases[i].period);
        M_Close(&fixture);
    }
}

TEST(stop_and_restart_retain_dac_levels_and_reset_software_controls)
{
    FIXTURE fixture;
    M_Init(&fixture);
    M_Begin(&fixture, 1);
    const uint8_t volume[] = { 12 };
    MusicSsg_Command(&fixture.ssg, 6, 0, 0xfd, volume);
    M_Note(&fixture, 0x40, 1, false, true);
    M_Advance(&fixture);
    M_Begin(&fixture, 1);
    MusicSsg_Command(&fixture.ssg, 6, 0, 0xfb, nullptr);
    MusicSsg_End(&fixture.ssg, 6);
    M_Advance(&fixture);
    CHECK(fixture.control.tied);
    CHECK_EQ_INT(fixture.chip.registers[8], 12);
    MusicSsg_Stop(&fixture.ssg);
    CHECK_EQ_INT(fixture.chip.registers[7], 0xbf);
    CHECK_EQ_INT(fixture.chip.registers[8], 12);
    MusicSsg_Reset(&fixture.ssg);
    CHECK(!fixture.control.tied);
    CHECK_EQ_INT(fixture.ssg.channels[0].volume, 8);
    CHECK_EQ_INT(fixture.chip.registers[8], 12);
    CHECK_EQ_INT(fixture.chip.registers[7], 0xbf);
    M_Close(&fixture);
}
