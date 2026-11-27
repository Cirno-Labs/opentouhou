#include "th02/main.h"

#include "th02/program/private.h"
#include "th02/version.h"

static const char *const m_DataFiles[] = { "TOUHOU.DAT", "東方封魔.録" };

const GAME_DESC g_GameDesc = {
    .id = "th02",
    .title = "東方封魔録 ~ the Story of Eastern Wonderland",
    .version = GAME_VERSION,
    .logical_width = VIDEO_WIDTH,
    .logical_height = VIDEO_HEIGHT,
    .input_schema = &g_Th02InputSchema,
    .settings_schema = &g_Th02SettingsSchema,
    .settings = &g_Th02Settings,
    .data_files = m_DataFiles,
    .data_file_count = ARRAY_SIZE(m_DataFiles),
    .init = Program_Init,
    .tick = Program_Tick,
    .shutdown = Program_Shutdown,
};
