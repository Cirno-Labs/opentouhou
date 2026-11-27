#include "tests/test.h"

#include "pc98/pmd/pitch.h"

TEST(transpose_preserves_byte_octave_and_rest_quirks)
{
    CHECK_EQ_INT(PmdPitch_Transpose(0x40, 12, 0), 0x50);
    CHECK_EQ_INT(PmdPitch_Transpose(0x40, -1, 0), 0x3b);
    CHECK_EQ_INT(PmdPitch_Transpose(0x00, -1, 0), 0xff);
    CHECK_EQ_INT(PmdPitch_Transpose(0x7b, 12, 0), 0x8b);
    CHECK_EQ_INT(PmdPitch_Transpose(0x0f, 127, 1), 0x0f);
    CHECK_EQ_INT(PmdPitch_Transpose(0x1f, 1, 0), 0x24);
    CHECK_EQ_INT(PmdPitch_Transpose(0x00, -128, -128), 0x00);
}

TEST(notes_keep_glide_state_and_reject_unresolved_semitones)
{
    PMD_PITCH pitch;
    PmdPitch_Init(&pitch);
    pitch.glide = 123;
    CHECK(Result_Ignore(PmdPitch_Note(&pitch, 0x4a, false)));
    CHECK_EQ_INT(pitch.frequency, 0x244e);
    CHECK_EQ_INT(pitch.note, 0x4a);
    CHECK_EQ_INT(pitch.glide, 123);
    CHECK(Result_Ignore(PmdPitch_Note(&pitch, 0xca, false)));
    CHECK_EQ_INT(pitch.frequency, 0x244e);
    CHECK_EQ_INT(pitch.note, 0xca);
    const PMD_PITCH before = pitch;
    CHECK(!Result_Ignore(PmdPitch_Note(&pitch, 0x4c, false)));
    CHECK(memcmp(&pitch, &before, sizeof(pitch)) == 0);
    CHECK(Result_Ignore(PmdPitch_Note(&pitch, 0x4f, true)));
    CHECK_EQ_INT(pitch.frequency, 0x244e);
    CHECK_EQ_INT(pitch.note, 0xff);
    CHECK(Result_Ignore(PmdPitch_Note(&pitch, 0x0f, false)));
    CHECK_EQ_INT(pitch.frequency, 0);
}

TEST(glides_frontload_signed_remainder_and_wrap_the_offset)
{
    PMD_PITCH pitch;
    PmdPitch_Init(&pitch);
    CHECK(Result_Ignore(PmdPitch_Note(&pitch, 0x40, false)));
    CHECK(Result_Ignore(PmdPitch_StartGlide(&pitch, 0x2a6a, 4)));
    CHECK_EQ_INT(pitch.glide_step, 154);
    CHECK_EQ_INT(pitch.glide_remainder, 2);
    const uint16_t expected[] = { 155, 310, 464, 618 };
    for (size_t i = 0; i < ARRAY_SIZE(expected); i++) {
        PmdPitch_GlideStep(&pitch);
        CHECK_EQ_INT(pitch.glide, expected[i]);
    }
    CHECK_EQ_INT(pitch.glide_remainder, 0);
    pitch.glide = 0;
    CHECK(Result_Ignore(PmdPitch_StartGlide(&pitch, 0x1a6a, 4)));
    CHECK_EQ_INT(pitch.glide_step, -154);
    CHECK_EQ_INT(pitch.glide_remainder, -2);
    for (size_t i = 0; i < ARRAY_SIZE(expected); i++) {
        PmdPitch_GlideStep(&pitch);
        CHECK_EQ_INT(pitch.glide, (uint16_t)-expected[i]);
    }
    const PMD_PITCH before = pitch;
    CHECK(!Result_Ignore(PmdPitch_StartGlide(&pitch, 0x26a, 0)));
    CHECK(memcmp(&pitch, &before, sizeof(pitch)) == 0);
    pitch.glide = 0xffff;
    pitch.glide_step = 1;
    pitch.glide_remainder = 1;
    PmdPitch_GlideStep(&pitch);
    CHECK_EQ_INT(pitch.glide, 1);
}

TEST(frequency_preserves_word_wrap_and_clamps_without_changing_pitch)
{
    PMD_PITCH pitch;
    PmdPitch_Init(&pitch);
    CHECK_EQ_INT(PmdPitch_Frequency(&pitch, 0, nullptr), 0);
    CHECK(Result_Ignore(PmdPitch_Note(&pitch, 0x40, false)));
    CHECK_EQ_INT(PmdPitch_Frequency(&pitch, 0, nullptr), 0x226a);
    PMD_FM_MODULATION modulation[2] = {
        { .amount = 32767, .enabled = true },
        { .amount = 1, .enabled = true },
    };
    CHECK_EQ_INT(PmdPitch_Frequency(&pitch, 0, modulation), 8);
    CHECK(Result_Ignore(PmdPitch_Note(&pitch, 0x00, false)));
    CHECK_EQ_INT(PmdPitch_Frequency(&pitch, -0x26a, nullptr), 8);
    CHECK(Result_Ignore(PmdPitch_Note(&pitch, 0x70, false)));
    CHECK_EQ_INT(PmdPitch_Frequency(&pitch, 0x595, nullptr), 0x3fff);
    const PMD_PITCH before = pitch;
    PmdPitch_Render(&pitch, 0, 0, nullptr, nullptr);
    CHECK(memcmp(&pitch, &before, sizeof(pitch)) == 0);
}
