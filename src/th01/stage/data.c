#include "th01/stage/data.h"

#include <string.h>

RESULT StageData_Load(
    READER *const reader, const ALLOCATOR *const allocator,
    STAGE_DATA *const out)
{
    *out = (STAGE_DATA) {};
    MUST(Reader_Result(reader));
    FAIL_IF(Reader_Remaining(reader) < 22 + 5 * 250, "Truncated stage data");
    uint8_t magic[5];
    uint8_t bgm_name[8];
    uint8_t grf_name[8];
    Reader_ReadBytes(reader, magic, sizeof(magic));
    Reader_ReadI8(reader);
    Reader_ReadBytes(reader, bgm_name, sizeof(bgm_name));
    Reader_ReadBytes(reader, grf_name, sizeof(grf_name));
    STAGE_DATA_STAGE stages[STAGE_DATA_STAGE_COUNT] = {};
    for (int32_t stage = 0; stage < STAGE_DATA_STAGE_COUNT; stage++) {
        uint8_t cards[STAGE_DATA_ROWS][STAGE_DATA_COLUMNS / 4];
        for (int32_t row = 0; row < STAGE_DATA_ROWS; row++) {
            Reader_ReadBytes(reader, cards[row], sizeof(cards[row]));
        }
        for (int32_t row = 0; row < STAGE_DATA_ROWS; row++) {
            for (int32_t column = 0; column < STAGE_DATA_COLUMNS; column++) {
                STAGE_DATA_CELL *const cell = &stages[stage].cells[row][column];
                cell->card =
                    (cards[row][column / 4] & (8 >> (column % 4))) != 0;
                const int8_t type = Reader_ReadI8(reader);
                if ((type >= 1 && type <= 13) || (type >= 17 && type <= 21)) {
                    cell->obstacle = (STAGE_DATA_OBSTACLE)type;
                } else if (cell->card && type >= 14 && type <= 16) {
                    cell->card_extra_hp = type - 13;
                }
            }
        }
    }
    MUST(Reader_Result(reader));
    STAGE_DATA_STAGE *const loaded = Allocator_Alloc(allocator, sizeof(stages));
    memcpy(loaded, stages, sizeof(stages));
    *out = (STAGE_DATA) { .stages = loaded };
    return OK;
}
