#include "th06/time.h"

static bool m_Held;

void GameTime_Hold(void)
{
    m_Held = true;
}

void GameTime_Resume(void)
{
    m_Held = false;
}

float GameTime_Step(void)
{
    return m_Held ? 0.0f : 1.0f;
}
