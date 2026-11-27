#include "replay/th06/harness.h"

#include "th06/buttons.h"

#include "core/base/utils.h"

static const BINDING m_Bindings[] = {
#define M_BINDING(name, mask, ...) { #name, mask },
#define M_STICK(...)
    TH06_ACTIONS(M_BINDING, M_STICK)
#undef M_BINDING
#undef M_STICK
};

#define M_COLUMN(name, kind) { #name, COLUMN_##kind },
static const COLUMN m_Columns[] = { TH06_CALC_FIELDS(M_COLUMN) };
static const COLUMN m_Choices[] = { TH06_CHOICE_FIELDS(M_COLUMN) };
#undef M_COLUMN

static_assert(
    ARRAY_SIZE(m_Columns) + ARRAY_SIZE(m_Choices) == TH06_COLUMN_COUNT);

const HARNESS_GAME g_Th06Harness = {
    .id = "th06",
    .version = "1.02h",
    .frame_rate = 60.0,
    .width = 640,
    .height = 480,
    .draw_frames = 60,
    .bindings = m_Bindings,
    .binding_count = ARRAY_SIZE(m_Bindings),
    .columns = m_Columns,
    .column_count = ARRAY_SIZE(m_Columns),
    .choices = m_Choices,
    .choice_count = ARRAY_SIZE(m_Choices),
    .columns_version = 2,
    .shows_fps = true,
};
