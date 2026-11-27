#include "tests/harness_game.h"

#include "core/base/utils.h"

static const BINDING m_Bindings[] = {
    { "shoot", 0x1 },
    { "bomb", 0x2 },
    { "up", 0x10 },
};

static const COLUMN m_Columns[] = {
    { "score", COLUMN_U32 },
    { "lives", COLUMN_I32 },
    { "x", COLUMN_F32 },
    { "bullets", COLUMN_HASH },
};

static const COLUMN m_Choices[] = {
    { "difficulty", COLUMN_U32 },
};

static const COLUMN m_Starts[] = {
    { "stage", COLUMN_U32 },
    { "lives", COLUMN_I32 },
};

const HARNESS_GAME g_TestGame = {
    .id = "test",
    .version = "1.0",
    .frame_rate = 60.0,
    .width = 4,
    .height = 2,
    .draw_frames = 1,
    .bindings = m_Bindings,
    .binding_count = ARRAY_SIZE(m_Bindings),
    .columns = m_Columns,
    .column_count = ARRAY_SIZE(m_Columns),
    .choices = m_Choices,
    .choice_count = ARRAY_SIZE(m_Choices),
    .columns_version = 3,
    .starts = m_Starts,
    .start_count = ARRAY_SIZE(m_Starts),
};
