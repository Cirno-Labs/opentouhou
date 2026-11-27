#pragma once

#include "th02/stage/flow.h"

typedef enum {
    TEST_BOSS_BEGIN,
    TEST_BOSS_DRAW,
    TEST_BOSS_UPDATE,
    TEST_BOSS_END,
} TEST_BOSS_CALL;

typedef struct {
    bool (*call)(void *context, TEST_BOSS_CALL call, int16_t *value);
    void *context;
} TEST_BOSS;

void TestStageFlow_Boss(const TEST_BOSS *boss);
