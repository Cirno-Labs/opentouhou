#include "th02/audio_clock.h"

#include <string.h>

static struct {
    PMD *pmd;
    GAME_AUDIO *output;
    MUSIC_WAIT *wait;
    const TH02_CLOCK_SAMPLE *samples;
    size_t count;
    uint32_t ordinals[TH02_CLOCK_KIND_COUNT];
    uint32_t effect_ordinal;
    uint64_t origin;
    uint64_t rendered;
    bool initialized;
    bool active;
    RESULT error;
} m_AudioClock;

static bool M_Sample(const uint32_t key, uint64_t *const out)
{
    for (size_t i = 0; i < m_AudioClock.count; i++) {
        if (m_AudioClock.samples[i].key == key) {
            *out = m_AudioClock.samples[i].microseconds;
            return true;
        }
    }
    return false;
}

static void M_Render(uint64_t samples)
{
    while (samples != 0 && IS_OK(m_AudioClock.error)) {
        const uint32_t count =
            samples > UINT32_MAX ? UINT32_MAX : (uint32_t)samples;
        MusicWait_Advance(m_AudioClock.wait, count);
        m_AudioClock.error =
            GameAudio_Render(m_AudioClock.output, m_AudioClock.pmd, count);
        samples -= count;
    }
}

static void M_Advance(const uint64_t time)
{
    if (time < m_AudioClock.origin) {
        if (IS_OK(m_AudioClock.error)) {
            m_AudioClock.error = FAIL("Music clock precedes its origin");
        }
        return;
    }
    const uint64_t elapsed = time - m_AudioClock.origin;
    const uint64_t target = elapsed / 1000 * 48 + elapsed % 1000 * 48 / 1000;
    if (target > m_AudioClock.rendered) {
        M_Render(target - m_AudioClock.rendered);
        m_AudioClock.rendered = target;
    }
}

void AudioClock_Init(
    PMD *const pmd, GAME_AUDIO *const output, MUSIC_WAIT *const wait)
{
    Result_Ignore(m_AudioClock.error);
    memset(&m_AudioClock, 0, sizeof(m_AudioClock));
    m_AudioClock.pmd = pmd;
    m_AudioClock.output = output;
    m_AudioClock.wait = wait;
}

void AudioClock_Begin(
    const TH02_CLOCK_SAMPLE *const samples, const size_t count)
{
    m_AudioClock.samples = samples;
    m_AudioClock.count = count;
    memset(m_AudioClock.ordinals, 0, sizeof(m_AudioClock.ordinals));
    m_AudioClock.effect_ordinal = 0;
    uint64_t frame;
    m_AudioClock.active = m_AudioClock.pmd != nullptr
        && m_AudioClock.wait != nullptr && M_Sample(TH02_CLOCK_FRAME, &frame);
    if (m_AudioClock.active && !m_AudioClock.initialized) {
        if (!M_Sample(TH02_CLOCK_ORIGIN, &m_AudioClock.origin)) {
            if (IS_OK(m_AudioClock.error)) {
                m_AudioClock.error = FAIL("Music clock has no origin");
            }
            return;
        }
        m_AudioClock.initialized = true;
    }
    AudioClock_Advance(TH02_CLOCK_FRAME);
}

void AudioClock_Advance(const TH02_CLOCK_KIND kind)
{
    if (!m_AudioClock.active) {
        return;
    }
    uint64_t time;
    const uint32_t key =
        m_AudioClock.ordinals[kind]++ * TH02_CLOCK_KIND_COUNT + kind;
    if (M_Sample(key, &time) || M_Sample(TH02_CLOCK_FRAME, &time)) {
        M_Advance(time);
    }
}

void AudioClock_Effect(PMD *const pmd)
{
    if (!m_AudioClock.active || pmd != m_AudioClock.pmd) {
        return;
    }
    uint64_t time;
    const uint32_t ordinal = m_AudioClock.effect_ordinal++;
    if (M_Sample(TH02_CLOCK_EFFECT_ENTRY_KEY + ordinal, &time)
        || M_Sample(TH02_CLOCK_EFFECT_KEY + ordinal, &time)) {
        M_Advance(time);
    }
}

void AudioClock_End(void)
{
    if (m_AudioClock.active) {
        AudioClock_Advance(TH02_CLOCK_FRAME);
    } else if (m_AudioClock.pmd != nullptr && m_AudioClock.wait != nullptr) {
        M_Render(800);
    }
}

bool AudioClock_Failed(void)
{
    return !IS_OK(m_AudioClock.error);
}

RESULT AudioClock_TakeError(void)
{
    const RESULT error = m_AudioClock.error;
    memset(&m_AudioClock.error, 0, sizeof(m_AudioClock.error));
    return error;
}
