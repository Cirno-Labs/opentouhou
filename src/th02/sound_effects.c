#include "th02/sound_effects.h"

#include "th02/audio_clock.h"

#include "core/base/assert.h"

static const struct {
    uint8_t priority[33];
    uint8_t hold[33];
} m_Rules = {
    .priority = { 0,  0,  32, 9, 2, 16, 5, 6, 48, 8, 9, 5, 16, 10, 17, 12, 32,
                  32, 16, 0,  0, 0, 0,  0, 0, 0,  0, 0, 0, 0,  0,  0,  0 },
    .hold = { 0,  0,  36, 16, 4, 16, 8, 10, 48, 80, 24, 17, 11, 4, 80, 16, 48,
              74, 32, 0,  0,  0, 0,  0, 0,  0,  0,  0,  0,  0,  0, 0,  0 },
};

static PMD *m_Pmd;
static uint8_t m_Playing = 255;
static uint8_t m_Frame;

void SoundEffects_Init(PMD *const pmd)
{
    m_Pmd = pmd;
    SoundEffects_Reset();
}

void SoundEffects_Reset(void)
{
    m_Playing = 255;
    m_Frame = 0;
}

void SoundEffects_Play(const SOUND_EFFECT id)
{
    if (m_Pmd == nullptr) {
        return;
    }
    ASSERT(id >= 0 && id < 33);
    if (m_Playing == 255) {
        m_Playing = (uint8_t)id;
    } else if (m_Rules.priority[m_Playing] <= m_Rules.priority[id]) {
        m_Playing = (uint8_t)id;
        m_Frame = 0;
    }
}

void SoundEffects_Update(void)
{
    if (m_Pmd == nullptr || m_Playing == 255) {
        return;
    }
    if (m_Frame == 0) {
        AudioClock_Effect(m_Pmd);
        Pmd_Effect(m_Pmd, m_Playing);
    }
    m_Frame++;
    if (m_Rules.hold[m_Playing] < m_Frame) {
        SoundEffects_Reset();
    }
}

void SoundEffects_Force(const SOUND_EFFECT id)
{
    SoundEffects_Reset();
    SoundEffects_Play(id);
    SoundEffects_Update();
}
