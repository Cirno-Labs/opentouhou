#include "pc98/pmd/pitch.h"

#include "core/base/assert.h"

#include <string.h>

static const uint16_t m_Frequency[12] = {
    0x26a, 0x28f, 0x2b6, 0x2df, 0x30b, 0x339,
    0x36a, 0x39e, 0x3d5, 0x410, 0x44e, 0x48f,
};

static uint8_t M_RotateOctave(const uint8_t octave)
{
    return (uint8_t)(octave << 4 | octave >> 4);
}

static uint16_t M_Normalize(uint16_t frequency, uint16_t block)
{
    while (true) {
        if ((frequency & 0x8000) != 0 || frequency < 0x26a) {
            if (block < 0x800) {
                return (frequency & 0x8000) != 0 || frequency < 8 ? 8
                                                                  : frequency;
            }
            block -= 0x800;
            frequency = (uint16_t)(frequency + 0x26a);
        } else if (frequency >= 0x4d4) {
            block += 0x800;
            if (block == 0x4000) {
                return (uint16_t)(0x3800
                                  | (frequency < 0x800 ? frequency : 0x7ff));
            }
            frequency -= 0x26a;
        } else {
            return (uint16_t)(frequency | block);
        }
    }
}

void PmdPitch_Init(PMD_PITCH *const pitch)
{
    memset(pitch, 0, sizeof(*pitch));
}

uint8_t PmdPitch_Transpose(
    const uint8_t note, const int8_t transpose, const int8_t master)
{
    const uint8_t shift = (uint8_t)((uint8_t)transpose + (uint8_t)master);
    if (note == 0x0f || shift == 0) {
        return note;
    }
    uint8_t octave = note >> 4;
    uint8_t semitone = note & 0x0f;
    if (shift < 128) {
        semitone += shift;
        while (semitone >= 12) {
            octave++;
            semitone -= 12;
        }
    } else {
        unsigned sum = (unsigned)semitone + shift;
        semitone = (uint8_t)sum;
        while (sum < 256) {
            octave--;
            sum = (unsigned)semitone + 12;
            semitone = (uint8_t)sum;
        }
    }
    return (uint8_t)(M_RotateOctave(octave) | semitone);
}

RESULT PmdPitch_Note(
    PMD_PITCH *const pitch, const uint8_t note, const bool keep_rest_frequency)
{
    const uint8_t semitone = note & 0x0f;
    if (semitone == 0x0f) {
        pitch->note = 0xff;
        if (!keep_rest_frequency) {
            pitch->frequency = 0;
        }
        return OK;
    }
    FAIL_IF(semitone >= 12, "Invalid PMD FM semitone");
    pitch->note = note;
    pitch->frequency =
        (uint16_t)(m_Frequency[semitone] | (uint16_t)((note >> 4) & 7) << 11);
    return OK;
}

RESULT PmdPitch_StartGlide(
    PMD_PITCH *const pitch, const uint16_t target_frequency,
    const uint8_t duration)
{
    FAIL_IF(duration == 0, "Zero PMD portamento duration");
    const int block_difference = (int)((target_frequency >> 11) & 7)
        - (int)((pitch->frequency >> 11) & 7);
    const int difference = block_difference * 0x26a
        + (int)(target_frequency & 0x7ff) - (int)(pitch->frequency & 0x7ff);
    pitch->glide_step = (int16_t)(difference / duration);
    pitch->glide_remainder = (int16_t)(difference % duration);
    return OK;
}

void PmdPitch_GlideStep(PMD_PITCH *const pitch)
{
    pitch->glide = (uint16_t)(pitch->glide + (uint16_t)pitch->glide_step);
    if (pitch->glide_remainder > 0) {
        pitch->glide_remainder--;
        pitch->glide++;
    } else if (pitch->glide_remainder < 0) {
        pitch->glide_remainder++;
        pitch->glide--;
    }
}

uint16_t PmdPitch_Frequency(
    const PMD_PITCH *const pitch, const int16_t detune,
    const PMD_FM_MODULATION modulation[2])
{
    if (pitch->frequency == 0) {
        return 0;
    }
    uint16_t frequency = (uint16_t)((pitch->frequency & 0x7ff) + pitch->glide
                                    + (uint16_t)detune);
    if (modulation != nullptr) {
        for (size_t i = 0; i < 2; i++) {
            if (modulation[i].enabled) {
                frequency =
                    (uint16_t)(frequency + (uint16_t)modulation[i].amount);
            }
        }
    }
    return M_Normalize(frequency, pitch->frequency & 0x3800);
}

void PmdPitch_Render(
    const PMD_PITCH *const pitch, const uint8_t channel, const int16_t detune,
    const PMD_FM_MODULATION modulation[2], PMD_CHIP *const chip)
{
    ASSERT(channel < 6);
    if (pitch->frequency == 0) {
        return;
    }
    const uint16_t encoded = PmdPitch_Frequency(pitch, detune, modulation);
    const uint16_t bank = channel >= 3 ? 0x100 : 0;
    PmdChip_Write(
        chip, (uint16_t)(bank + 0xa4 + channel % 3), (uint8_t)(encoded >> 8));
    PmdChip_Write(
        chip, (uint16_t)(bank + 0xa0 + channel % 3), (uint8_t)encoded);
}
