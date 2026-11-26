#include "pc98/pmd/registers.h"

#include "core/base/assert.h"

#include <string.h>

static const uint8_t m_Carriers[8] = {
    0x80, 0x80, 0x80, 0x80, 0xa0, 0xe0, 0xe0, 0xf0,
};

static const uint8_t m_RegisterSlot[4] = { 3, 1, 2, 0 };

static uint16_t M_Register(const PMD_FM *const voice, const uint8_t reg)
{
    return (uint16_t)((voice->channel >= 3 ? 0x100 : 0) + reg
                      + voice->channel % 3);
}

static uint8_t M_KeyChannel(const PMD_FM *const voice)
{
    return (uint8_t)(voice->channel % 3 | (voice->channel >= 3 ? 4 : 0));
}

void PmdFm_Init(PMD_FM *const voice, const uint8_t channel)
{
    ASSERT(channel < 6);
    memset(voice, 0, sizeof(*voice));
    voice->channel = channel;
}

void PmdFm_Silence(PMD_FM *const voice, PMD_CHIP *const chip)
{
    for (uint8_t i = 0; i < 4; i++) {
        PmdChip_Write(chip, M_Register(voice, (uint8_t)(0x40 + i * 4)), 127);
        PmdChip_Write(chip, M_Register(voice, (uint8_t)(0x80 + i * 4)), 127);
    }
    PmdFm_KeyOff(voice, chip);
}

void PmdFm_Program(
    PMD_FM *const voice, const uint8_t parameters[PMD_INSTRUMENT_SIZE],
    PMD_CHIP *const chip)
{
    PmdFm_Silence(voice, chip);
    voice->algorithm_feedback = parameters[24];
    voice->carriers = m_Carriers[parameters[24] & 7];
    PmdChip_Write(chip, M_Register(voice, 0xb0), parameters[24]);
    for (uint8_t i = 0; i < 4; i++) {
        PmdChip_Write(
            chip, M_Register(voice, (uint8_t)(0x30 + i * 4)), parameters[i]);
    }
    for (uint8_t i = 0; i < 4; i++) {
        const uint8_t slot = m_RegisterSlot[i];
        if ((voice->carriers & (0x80 >> slot)) == 0) {
            PmdChip_Write(
                chip, M_Register(voice, (uint8_t)(0x40 + i * 4)),
                parameters[4 + i]);
        }
    }
    for (uint8_t i = 0; i < 16; i++) {
        PmdChip_Write(
            chip, M_Register(voice, (uint8_t)(0x50 + i * 4)),
            parameters[8 + i]);
    }
    memcpy(voice->levels, parameters + 4, sizeof(voice->levels));
}

int16_t PmdFm_Level(
    const PMD_FM *const voice, const uint8_t volume,
    const PMD_FM_MODULATION modulation[2], const uint8_t slot)
{
    ASSERT(slot < 4);
    const uint8_t bit = (uint8_t)(0x80 >> slot);
    bool active = (voice->carriers & bit) != 0;
    uint8_t level = active ? (uint8_t)~volume : 0x80;
    if (volume != 0 && modulation != nullptr) {
        for (size_t i = 0; i < 2; i++) {
            if (!modulation[i].enabled || (modulation[i].slots & bit) == 0) {
                continue;
            }
            active = true;
            const uint8_t amount = (uint8_t)modulation[i].amount;
            if (amount < 128) {
                level = level >= amount ? (uint8_t)(level - amount) : 0;
            } else {
                level = level < amount ? (uint8_t)(level - amount) : 255;
            }
        }
    }
    if (!active) {
        return -1;
    }
    unsigned total = (unsigned)level + voice->levels[m_RegisterSlot[slot]];
    if (total > 255) {
        total = 255;
    }
    return (int16_t)(total >= 128 ? total - 128 : 0);
}

void PmdFm_Volume(
    const PMD_FM *const voice, const uint8_t volume,
    const PMD_FM_MODULATION modulation[2], PMD_CHIP *const chip)
{
    for (uint8_t i = 0; i < 4; i++) {
        const int16_t level = PmdFm_Level(voice, volume, modulation, i);
        if (level >= 0) {
            PmdChip_Write(
                chip,
                M_Register(voice, (uint8_t)(0x40 + m_RegisterSlot[i] * 4)),
                (uint8_t)level);
        }
    }
}

void PmdFm_KeyOn(PMD_FM *const voice, PMD_CHIP *const chip)
{
    voice->key_slots = 0xf0;
    PmdChip_Write(
        chip, 0x28, (uint8_t)(M_KeyChannel(voice) | voice->key_slots));
}

void PmdFm_KeyOff(PMD_FM *const voice, PMD_CHIP *const chip)
{
    voice->key_slots = 0;
    PmdChip_Write(chip, 0x28, M_KeyChannel(voice));
}
