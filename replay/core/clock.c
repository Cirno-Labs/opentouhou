#include "clock.h"

#include "iat.h"

#include <mmsystem.h>

#define COUNTS_PER_SECOND 1000000
#define COUNTS_PER_MS (COUNTS_PER_SECOND / 1000)

typedef DWORD(WINAPI *TIMEGETTIME_FN)(void);

static TIMEGETTIME_FN m_RealTimeGetTime;
static DWORD m_Start;
static uint32_t m_Step;
static volatile LONG m_Time;

static DWORD M_Now(void)
{
    if (m_Step == 0) {
        return m_RealTimeGetTime() - m_Start;
    }
    return (DWORD)InterlockedExchangeAdd(&m_Time, (LONG)m_Step);
}

static DWORD WINAPI M_TimeGetTime(void)
{
    return M_Now();
}

static BOOL WINAPI M_QueryPerformanceCounter(LARGE_INTEGER *const count)
{
    count->QuadPart = (LONGLONG)M_Now() * COUNTS_PER_MS;
    return TRUE;
}

static BOOL WINAPI M_QueryPerformanceFrequency(LARGE_INTEGER *const frequency)
{
    frequency->QuadPart = COUNTS_PER_SECOND;
    return TRUE;
}

bool Clock_Install(const HMODULE module, const uint32_t step)
{
    // Read before the patch, which may be of this module's own import.
    m_RealTimeGetTime = timeGetTime;
    m_Start = m_RealTimeGetTime();
    m_Step = step;
    uintptr_t original;
    const bool time = Iat_Patch(
        module, "WINMM.dll", "timeGetTime", (uintptr_t)M_TimeGetTime,
        &original);
    const bool counter = Iat_Patch(
        module, "KERNEL32.dll", "QueryPerformanceCounter",
        (uintptr_t)M_QueryPerformanceCounter, &original);
    if (counter) {
        Iat_Patch(
            module, "KERNEL32.dll", "QueryPerformanceFrequency",
            (uintptr_t)M_QueryPerformanceFrequency, &original);
    }
    return time || counter;
}
