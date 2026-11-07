#include "core/app/clock.h"

#include <stdio.h>
#include <stdlib.h>

#define NS_PER_SECOND 1000000000ULL
#define TICK_NS (NS_PER_SECOND / 60 + 1)

static int m_Failures;

static void M_Expect(const bool condition, const char *const what)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", what);
        m_Failures++;
    }
}

static void M_TestFirstCallStarts(void)
{
    CLOCK clock;
    Clock_Init(&clock, 60, 4);
    M_Expect(Clock_Advance(&clock, 5 * NS_PER_SECOND) == 0, "first call");
}

static void M_TestOneSecondIsSixtyTicks(void)
{
    CLOCK clock;
    Clock_Init(&clock, 60, 1000);
    Clock_Advance(&clock, 0);
    int total = 0;
    for (uint64_t ms = 1; ms <= 1000; ms++) {
        total += Clock_Advance(&clock, ms * 1000000ULL);
    }
    M_Expect(total == 60, "60 ticks in one second of 1 ms steps");
}

static void M_TestCatchUpIsCapped(void)
{
    CLOCK clock;
    Clock_Init(&clock, 60, 4);
    Clock_Advance(&clock, 0);
    M_Expect(Clock_Advance(&clock, NS_PER_SECOND) == 4, "cap");
    M_Expect(
        Clock_Advance(&clock, NS_PER_SECOND + TICK_NS) == 1, "dropped time");
}

static void M_TestBackwardsRestarts(void)
{
    CLOCK clock;
    Clock_Init(&clock, 60, 4);
    Clock_Advance(&clock, NS_PER_SECOND);
    M_Expect(Clock_Advance(&clock, 0) == 0, "time going backwards");
    M_Expect(Clock_Advance(&clock, TICK_NS) == 1, "tick after restart");
}

static void M_TestTimeUntilNextTick(void)
{
    CLOCK clock;
    Clock_Init(&clock, 60, 4);
    Clock_Advance(&clock, 0);
    M_Expect(Clock_NsUntilNextTick(&clock) == TICK_NS, "full tick");
    Clock_Advance(&clock, TICK_NS - 1000);
    M_Expect(Clock_NsUntilNextTick(&clock) == 1000, "partial tick");
}

int main(void)
{
    M_TestFirstCallStarts();
    M_TestOneSecondIsSixtyTicks();
    M_TestCatchUpIsCapped();
    M_TestBackwardsRestarts();
    M_TestTimeUntilNextTick();
    return m_Failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
