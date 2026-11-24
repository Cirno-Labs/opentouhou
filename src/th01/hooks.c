#include "th01/hooks.h"

static TH01_HOOKS m_Hooks;

void Th01Hooks_Set(const TH01_HOOKS hooks)
{
    m_Hooks = hooks;
}

void Th01Hooks_CallOnStart(void)
{
    if (m_Hooks.on_start != nullptr) {
        m_Hooks.on_start(m_Hooks.arg);
    }
}

void Th01Hooks_CallOnVSync(void)
{
    if (m_Hooks.on_vsync != nullptr) {
        m_Hooks.on_vsync(m_Hooks.arg);
    }
}

void Th01Hooks_CallOnBoss(int16_t *const hp)
{
    if (m_Hooks.on_boss != nullptr) {
        m_Hooks.on_boss(m_Hooks.arg, hp);
    }
}
