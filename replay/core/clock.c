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
static bool m_UsesFrames;
static uint64_t m_Frames;
static DWORD m_FrameOrigin;
static volatile LONG m_PacingTime;
static const CLOCK_WAIT_RANGE *m_PacingRanges;
static size_t m_PacingCount;

static DWORD M_Now(const uintptr_t caller)
{
    if (m_UsesFrames) {
        for (size_t i = 0; i < m_PacingCount; i++) {
            const CLOCK_WAIT_RANGE *const range = &m_PacingRanges[i];
            if (caller >= range->start && caller - range->start < range->size) {
                return (DWORD)InterlockedExchangeAdd(&m_PacingTime, 1);
            }
        }
        return (DWORD)InterlockedCompareExchange(&m_Time, 0, 0);
    }
    if (m_Step == 0) {
        return m_RealTimeGetTime() - m_Start;
    }
    return (DWORD)InterlockedExchangeAdd(&m_Time, (LONG)m_Step);
}

static DWORD WINAPI M_TimeGetTime(void)
{
    return M_Now((uintptr_t)__builtin_return_address(0));
}

static BOOL WINAPI M_QueryPerformanceCounter(LARGE_INTEGER *const count)
{
    count->QuadPart =
        (LONGLONG)M_Now((uintptr_t)__builtin_return_address(0)) * COUNTS_PER_MS;
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

void Clock_UseFrames(void)
{
    m_FrameOrigin = M_Now(0);
    m_UsesFrames = true;
    m_Frames = 0;
    InterlockedExchange(&m_Time, (LONG)m_FrameOrigin);
    InterlockedExchange(&m_PacingTime, (LONG)m_FrameOrigin);
}

void Clock_AdvanceFrame(void)
{
    m_Frames++;
    const LONG time = (LONG)(m_FrameOrigin + (m_Frames * 1000 + 59) / 60);
    InterlockedExchange(&m_Time, time);
    InterlockedExchange(&m_PacingTime, time);
}

void Clock_SetPacingRanges(
    const CLOCK_WAIT_RANGE *const ranges, const size_t count)
{
    m_PacingRanges = ranges;
    m_PacingCount = count;
}
