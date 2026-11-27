#pragma once

#include "tests/test.h"
#include "th02/hooks.h"

#include <string.h>

typedef struct {
    int16_t thresholds[16];
    size_t count;
} TEST_BOSS_HP;

static void TestBossHp_Apply(
    void *const arg, int16_t *const damage, const int16_t threshold)
{
    TEST_BOSS_HP *const hp = arg;
    CHECK_EQ_INT(*damage, 0);
    CHECK(hp->count < 16);
    hp->thresholds[hp->count++] = threshold;
    *damage = threshold - 1;
}

static inline void TestBossHp_Begin(TEST_BOSS_HP *const hp)
{
    memset(hp, 0, sizeof(*hp));
    g_Th02Hooks.on_boss = TestBossHp_Apply;
    g_Th02Hooks.arg = hp;
}

static inline void TestBossHp_End(void)
{
    memset(&g_Th02Hooks, 0, sizeof(g_Th02Hooks));
}
