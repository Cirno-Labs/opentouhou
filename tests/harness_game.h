#pragma once

// Gives the harness tests a small game with three bindings, one column of
// each kind and one choice.

#include "game.h"

#include "core/base/utils.h"

static const BINDING m_TestBindings[] = {
    { "shoot", 0x1 },
    { "bomb", 0x2 },
    { "up", 0x10 },
};

static const COLUMN m_TestColumns[] = {
    { "score", COLUMN_U32 },
    { "lives", COLUMN_I32 },
    { "x", COLUMN_F32 },
    { "bullets", COLUMN_HASH },
};

static const COLUMN m_TestChoices[] = {
    { "difficulty", COLUMN_U32 },
};

static const HARNESS_GAME m_TestGame = {
    .id = "test",
    .version = "1.0",
    .frame_rate = 60.0,
    .width = 4,
    .height = 2,
    .draw_frames = 1,
    .bindings = m_TestBindings,
    .binding_count = ARRAY_SIZE(m_TestBindings),
    .columns = m_TestColumns,
    .column_count = ARRAY_SIZE(m_TestColumns),
    .choices = m_TestChoices,
    .choice_count = ARRAY_SIZE(m_TestChoices),
    .columns_version = 3,
};
