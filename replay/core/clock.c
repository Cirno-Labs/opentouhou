#include "clock.h"

#include "iat.h"

typedef DWORD(WINAPI *TIMEGETTIME_FN)(void);

static TIMEGETTIME_FN m_RealTimeGetTime;
static DWORD m_Start;
static uint32_t m_Step;
static volatile LONG m_Time;

static DWORD WINAPI M_TimeGetTime(void)
{
    return m_RealTimeGetTime() - m_Start;
}

static DWORD WINAPI M_SteppingTimeGetTime(void)
{
    return (DWORD)InterlockedExchangeAdd(&m_Time, (LONG)m_Step);
}

bool Clock_Install(const HMODULE module, const uint32_t step)
{
    uintptr_t original;
    const uintptr_t hook =
        step == 0 ? (uintptr_t)M_TimeGetTime : (uintptr_t)M_SteppingTimeGetTime;
    if (!Iat_Patch(module, "WINMM.dll", "timeGetTime", hook, &original)) {
        return false;
    }
    m_RealTimeGetTime = (TIMEGETTIME_FN)original;
    m_Start = m_RealTimeGetTime();
    m_Step = step;
    return true;
}
