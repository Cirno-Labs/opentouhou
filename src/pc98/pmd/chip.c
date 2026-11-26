#include "pc98/pmd/chip.h"

#include <string.h>

static uint64_t M_PeriodB(const PMD_CHIP *const chip)
{
    static const uint16_t dividers[4] = { 48, 48, 144, 72 };
    return (uint64_t)(256 - chip->timer_b) * 16 * dividers[chip->prescaler] * 5;
}

void PmdChip_Init(
    PMD_CHIP *const chip, const ALLOCATOR *const allocator,
    const OPNA_TIMER_CALLBACK callback, void *const context)
{
    memset(chip, 0, sizeof(*chip));
    chip->next_b = UINT64_MAX;
    chip->prescaler = 2;
    chip->opna = Opna_Init(allocator, callback, context);
}

void PmdChip_Write(
    PMD_CHIP *const chip, const uint16_t reg, const uint8_t value)
{
    if (chip == nullptr) {
        return;
    }
    if (reg == 0x26) {
        chip->timer_b = value;
    } else if (reg == 0x27) {
        if ((value & 2) == 0) {
            chip->next_b = UINT64_MAX;
        } else if ((chip->timer_control & 2) == 0) {
            chip->next_b = Opna_Time(chip->opna) + M_PeriodB(chip);
        }
        chip->timer_control = value;
    } else if (reg == 0x2d) {
        chip->prescaler |= 2;
    } else if (reg == 0x2e) {
        chip->prescaler |= 1;
    } else if (reg == 0x2f) {
        chip->prescaler = 0;
    }
    if (reg < 16) {
        chip->registers[reg] = value;
    }
    Opna_Write(chip->opna, reg, value);
}

void PmdChip_ReloadTimerB(PMD_CHIP *const chip, const uint64_t time)
{
    chip->next_b = time + M_PeriodB(chip);
}

void PmdChip_PercussionWrite(
    PMD_CHIP *const chip, const uint8_t reg, const uint8_t value)
{
    if (reg == 6) {
        chip->last_noise = value;
    }
    PmdChip_Write(chip, reg, value);
}
