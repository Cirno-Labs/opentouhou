#include "harness.h"
#include "state.h"

#include "core/base/utils.h"

#define M_BINDING(name, mask) { #name, mask },
static const BINDING m_Bindings[] = { TH02_BUTTONS(M_BINDING) };
#undef M_BINDING

#define M_COLUMN(name, kind) { #name, COLUMN_##kind },
static const COLUMN m_Columns[] = { TH02_CALC_FIELDS(M_COLUMN) };
static const COLUMN m_Choices[] = { TH02_CHOICE_FIELDS(M_COLUMN) };
static const COLUMN m_Starts[] = { TH02_START_FIELDS(M_COLUMN) };
#undef M_COLUMN

const HARNESS_GAME g_Th02Harness = {
    .id = "th02",
    .version = "jp-original",
    .frame_rate = 60.0,
    .width = 640,
    .height = 400,
    .draw_frames = 1,
    .bindings = m_Bindings,
    .binding_count = ARRAY_SIZE(m_Bindings),
    .columns = m_Columns,
    .column_count = ARRAY_SIZE(m_Columns),
    .choices = m_Choices,
    .choice_count = ARRAY_SIZE(m_Choices),
    .columns_version = 5,
    .starts = m_Starts,
    .start_count = ARRAY_SIZE(m_Starts),
};

uint32_t Th02_HashGraphics(
    const uint8_t *const pages[2], const size_t loaded_sources)
{
    uint32_t hash = STATE_HASH_INIT;
    for (size_t page = 0; page < 2; page++) {
        uint32_t bytes = STATE_HASH_INIT;
        for (size_t y = 0; y < 400; y++) {
            const uint8_t *const row = pages[page] + y * 640;
            bytes = State_HashBytes(bytes, row, 576);
            for (size_t column = 0; column < 4; column++) {
                if (column * 25 + y / 16 < loaded_sources) {
                    bytes = State_HashBytes(bytes, row + 576 + column * 16, 16);
                }
            }
        }
        hash = State_Hash(hash, bytes);
    }
    return hash;
}
