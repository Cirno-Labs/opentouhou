#include "clock.h"

#include "iat.h"

typedef DWORD(WINAPI *TIMEGETTIME_FN)(void);

static TIMEGETTIME_FN m_RealTimeGetTime;
static DWORD m_Start;

static DWORD WINAPI M_TimeGetTime(void)
{
    return m_RealTimeGetTime() - m_Start;
}

bool Clock_Install(const HMODULE module)
{
    uintptr_t original;
    if (!Iat_Patch(
            module, "WINMM.dll", "timeGetTime", (uintptr_t)M_TimeGetTime,
            &original)) {
        return false;
    }
    m_RealTimeGetTime = (TIMEGETTIME_FN)original;
    m_Start = m_RealTimeGetTime();
    return true;
}
