#include "clock.h"

#include "iat.h"

#include <math.h>
#include <mmsystem.h>

#define M_COUNTS_PER_MS 1000

// Holds the clocks; the hooked imports take no context.
static struct {
    double frame_rate;
    const CLOCK_RANGE *pacing;
    size_t pacing_count;
    bool real_time;
    // Gives the real performance counter and the frame clock at the first
    // frame, and the counter's rate.
    LARGE_INTEGER real_start;
    LONG real_start_time;
    LARGE_INTEGER real_frequency;
    uint64_t frames;
    // Gives one frame in milliseconds, rounded up.
    LONG frame_ms;
    volatile LONG time;
    volatile LONG pacing_time;
    // Gives 1 more than the frame whose message queue ran empty, 0 before.
    uint64_t emptied;
    BOOL(WINAPI *peek)(LPMSG, HWND, UINT, UINT, UINT);
} m_Clock;

static DWORD M_Now(const uintptr_t caller)
{
    for (size_t i = 0; i < m_Clock.pacing_count; i++) {
        const CLOCK_RANGE *const range = &m_Clock.pacing[i];
        if (caller >= range->start && caller - range->start < range->size) {
            return (DWORD)InterlockedExchangeAdd(
                &m_Clock.pacing_time, m_Clock.frame_ms);
        }
    }
    return (DWORD)InterlockedCompareExchange(&m_Clock.time, 0, 0);
}

static DWORD WINAPI M_TimeGetTime(void)
{
    return M_Now((uintptr_t)__builtin_return_address(0));
}

static BOOL WINAPI M_QueryPerformanceCounter(LARGE_INTEGER *const count)
{
    count->QuadPart = (LONGLONG)M_Now((uintptr_t)__builtin_return_address(0))
        * M_COUNTS_PER_MS;
    return TRUE;
}

// Under Wine every peek is a round trip to wineserver, and a pacing loop
// peeks many times per frame.
static BOOL WINAPI M_PeekMessageA(
    const LPMSG msg, const HWND window, const UINT first, const UINT last,
    const UINT remove)
{
    if (m_Clock.emptied == m_Clock.frames + 1) {
        return FALSE;
    }
    const BOOL got = m_Clock.peek(msg, window, first, last, remove);
    if (!got) {
        m_Clock.emptied = m_Clock.frames + 1;
    }
    return got;
}

static BOOL WINAPI M_QueryPerformanceFrequency(LARGE_INTEGER *const frequency)
{
    frequency->QuadPart = 1000 * M_COUNTS_PER_MS;
    return TRUE;
}

bool Clock_Install(
    const HMODULE module, const double frame_rate,
    const CLOCK_RANGE *const pacing, const size_t count, const bool real_time)
{
    m_Clock.frame_rate = frame_rate;
    m_Clock.frame_ms = (LONG)ceil(1000.0 / frame_rate);
    m_Clock.pacing = pacing;
    m_Clock.pacing_count = count;
    m_Clock.real_time = real_time;
    if (real_time) {
        timeBeginPeriod(1);
        QueryPerformanceFrequency(&m_Clock.real_frequency);
    }
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
    if (Iat_Patch(
            module, "USER32.dll", "PeekMessageA", (uintptr_t)M_PeekMessageA,
            &original)) {
        m_Clock.peek = (BOOL(WINAPI *)(LPMSG, HWND, UINT, UINT, UINT))original;
    }
    return time || counter;
}

// Sleeps until the frame clock's time since the first frame has passed in
// real time.
static void M_WaitReal(const LONG time)
{
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    if (m_Clock.frames == 1) {
        m_Clock.real_start = now;
        m_Clock.real_start_time = time;
        return;
    }
    const LONGLONG deadline = m_Clock.real_start.QuadPart
        + (LONGLONG)(time - m_Clock.real_start_time)
            * m_Clock.real_frequency.QuadPart / 1000;
    for (;; QueryPerformanceCounter(&now)) {
        if (now.QuadPart >= deadline) {
            return;
        }
        Sleep((DWORD)((deadline - now.QuadPart) * 1000
                      / m_Clock.real_frequency.QuadPart));
    }
}

void Clock_Advance(void)
{
    m_Clock.frames++;
    const LONG time =
        (LONG)ceil((double)m_Clock.frames * 1000.0 / m_Clock.frame_rate);
    if (m_Clock.real_time) {
        M_WaitReal(time);
    }
    InterlockedExchange(&m_Clock.time, time);
    InterlockedExchange(&m_Clock.pacing_time, time);
}
