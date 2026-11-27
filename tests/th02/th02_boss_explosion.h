#pragma once

#include "th02/boss/explosion.h"

typedef struct {
    unsigned count;
    uint8_t size;
    bool valid;
} TEST_BOSS_DOTS;

// Observes explosion dot requests while retaining their actual drawing.
void TestBossExplosion_Observe(TEST_BOSS_DOTS *dots);
