#include "pc98/pmd/rhythm.h"

#include <string.h>

static void M_RhythmVolume(PMD_RHYTHM *const rhythm)
{
    uint8_t value = rhythm->volume;
    const uint8_t fade = rhythm->control->fade.volume;
    if (fade != 0) {
        value = (uint8_t)((uint16_t)(256 - fade) * value >> 8);
    }
    PmdChip_Write(rhythm->chip, 0x11, value);
}

static void M_RhythmKey(PMD_RHYTHM *const rhythm, const uint8_t mask)
{
    if (mask == 0) {
        return;
    }
    if (rhythm->control->fade.volume != 0) {
        M_RhythmVolume(rhythm);
    }
    if ((mask & 0x80) == 0) {
        for (unsigned i = 0; i < 6; i++) {
            if ((mask & (1 << i)) != 0) {
                PmdChip_Write(rhythm->chip, 0x18 + i, rhythm->levels[i]);
            }
        }
    }
    PmdChip_Write(rhythm->chip, 0x10, mask);
}

static uint8_t M_ShiftRhythm(
    const uint8_t value, const uint8_t amount, const uint8_t max)
{
    const uint8_t sum = value + amount;
    return sum <= max ? sum : (sum & 0x80) != 0 ? 0 : max;
}

void PmdRhythm_Init(
    PMD_RHYTHM *const rhythm, PMD_CHIP *const chip, PMD_CONTROL *const control)
{
    memset(rhythm, 0, sizeof(*rhythm));
    rhythm->chip = chip;
    rhythm->control = control;
}

void PmdRhythm_Reset(PMD_RHYTHM *const rhythm)
{
    memset(rhythm->levels, 0xcf, sizeof(rhythm->levels));
    rhythm->volume = 48;
    rhythm->volume_down = 0;
    PmdChip_Write(rhythm->chip, 0x10, 0xff);
    M_RhythmVolume(rhythm);
}

RESULT PmdRhythm_Command(
    PMD_RHYTHM *const rhythm, const uint8_t opcode, const uint8_t *const p)
{
    unsigned voice;
    uint8_t value;
    switch (opcode) {
    case 0xeb:
        M_RhythmKey(rhythm, p[0]);
        return OK;
    case 0xea:
    case 0xe9:
        voice = p[0] >> 5;
        FAIL_IF(voice < 1 || voice > 6, "Invalid PMD rhythm voice");
        value = opcode == 0xea
            ? (rhythm->levels[voice - 1] & 0xc0) | (p[0] & 31)
            : (rhythm->levels[voice - 1] & 31) | ((p[0] & 3) << 6);
        rhythm->levels[voice - 1] = value;
        PmdChip_Write(rhythm->chip, 0x17 + voice, value);
        return OK;
    case 0xe5:
        voice = p[0];
        FAIL_IF(voice < 1 || voice > 6, "Invalid PMD rhythm voice");
        value = (rhythm->levels[voice - 1] & 0xe0)
            | M_ShiftRhythm(rhythm->levels[voice - 1] & 31, p[1], 31);
        rhythm->levels[voice - 1] = value;
        PmdChip_Write(rhythm->chip, 0x17 + voice, value);
        return OK;
    case 0xe8:
    case 0xe6:
        value = opcode == 0xe8 ? p[0] : M_ShiftRhythm(rhythm->volume, p[0], 63);
        if (rhythm->volume_down != 0) {
            value =
                (uint8_t)((uint16_t)(256 - rhythm->volume_down) * value >> 8);
        }
        rhythm->volume = value;
        M_RhythmVolume(rhythm);
        return OK;
    default:
        return FAIL("Invalid PMD rhythm command");
    }
}

void PmdRhythm_Play(PMD_RHYTHM *const rhythm, const uint16_t mask)
{
    static const uint8_t mapping[11][3] = {
        { 0x18, 0xdf, 0x01 }, { 0x19, 0xdf, 0x02 }, { 0x1c, 0x5f, 0x10 },
        { 0x1c, 0xdf, 0x10 }, { 0x1c, 0x9f, 0x10 }, { 0x1d, 0xd3, 0x20 },
        { 0x19, 0xdf, 0x02 }, { 0x1b, 0x9c, 0x88 }, { 0x1a, 0x9d, 0x04 },
        { 0x1a, 0xdf, 0x04 }, { 0x1a, 0x5e, 0x04 }
    };
    for (unsigned i = 0; i < 11; i++) {
        if ((mask & (1 << i)) != 0) {
            PmdChip_Write(rhythm->chip, mapping[i][0], mapping[i][1]);
            uint8_t key = mapping[i][2];
            if ((key & 0x80) != 0) {
                PmdChip_Write(rhythm->chip, 0x10, 0x84);
                key = 8;
            }
            PmdChip_Write(rhythm->chip, 0x10, key);
        }
    }
    if (rhythm->control->fade.volume != 0) {
        M_RhythmVolume(rhythm);
    }
}
