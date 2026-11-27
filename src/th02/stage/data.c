#include "th02/stage/data.h"

#include "th02/map.h"

#include <string.h>

static RESULT M_ReadFormations(
    READER *const reader, STAGE_FORMATION *const formations,
    const uint16_t count, STAGE_SCRIPT *const scripts,
    uint16_t *const script_count)
{
    for (size_t i = 0; i < count; i++) {
        STAGE_FORMATION *const formation = &formations[i];
        formation->reserved_00 = Reader_ReadI16(reader);
        formation->spawn_top = Reader_ReadI16(reader);
        formation->bullet_offset_x = Reader_ReadI16(reader);
        formation->bullet_offset_y = Reader_ReadI16(reader);
        formation->pattern = Reader_ReadI16(reader);
        formation->image_count = Reader_ReadI16(reader);
        formation->frames_per_image = Reader_ReadI16(reader);
        formation->kill_pattern_offset = Reader_ReadI16(reader);
        formation->item = Reader_ReadU8(reader);
        formation->reserved_11 = Reader_ReadI8(reader);
        formation->score = Reader_ReadI32(reader);
        formation->hp = Reader_ReadI16(reader);
        formation->player_collision_disabled = Reader_ReadU8(reader);
        formation->reserved_19 = Reader_ReadI8(reader);
        formation->aimed_interval = Reader_ReadI16(reader);
        uint8_t value = Reader_ReadU8(reader);
        MUST(Reader_Result(reader));
        if (value == 0xfe) {
            formation->script = Reader_ReadU8(reader);
            Reader_Skip(reader, 1);
            MUST(Reader_Result(reader));
            FAIL_IF(
                formation->script >= *script_count,
                "Formation script reference is not defined");
        } else {
            FAIL_IF(
                *script_count >= STAGE_DATA_SCRIPT_LIMIT,
                "Too many formation scripts");
            formation->script = (uint8_t)*script_count;
            STAGE_SCRIPT *const script = &scripts[(*script_count)++];
            for (;;) {
                FAIL_IF(
                    script->size >= STAGE_DATA_SCRIPT_SIZE,
                    "Formation script is too long");
                script->bytes[script->size++] = value;
                value = Reader_ReadU8(reader);
                MUST(Reader_Result(reader));
                if (value == 0xff) {
                    break;
                }
            }
        }
    }
    return OK;
}

RESULT StageData_Load(
    READER *const reader, const ALLOCATOR *const allocator,
    STAGE_DATA *const out)
{
    *out = (STAGE_DATA) {};
    const uint16_t size = (uint16_t)Reader_ReadU32(reader);
    MUST(Reader_Result(reader));
    READER payload = Reader_Slice(reader, size);
    MUST(Reader_Result(reader));
    STAGE_DATA data = {};
    data.payload_size = size;
    const int16_t map_count = Reader_ReadI16(&payload);
    MUST(Reader_Result(&payload));
    FAIL_IF(
        map_count < 0 || map_count > STAGE_DATA_MAP_LIMIT,
        "Invalid map section count");
    data.map_count = (uint16_t)map_count;
    uint8_t map[STAGE_DATA_MAP_LIMIT];
    for (size_t i = 0; i < data.map_count; i++) {
        map[i] = Reader_ReadU8(&payload);
        FAIL_IF(map[i] >= MAP_SECTION_COUNT, "Invalid map section index");
    }
    const int16_t formation_count = Reader_ReadI16(&payload);
    MUST(Reader_Result(&payload));
    FAIL_IF(
        formation_count < 0 || formation_count > STAGE_DATA_FORMATION_LIMIT,
        "Invalid formation count");
    data.formation_count = (uint16_t)formation_count;
    STAGE_FORMATION formations[STAGE_DATA_FORMATION_LIMIT];
    STAGE_SCRIPT scripts[STAGE_DATA_SCRIPT_LIMIT] = {};
    MUST(M_ReadFormations(
        &payload, formations, data.formation_count, scripts,
        &data.script_count));
    const int16_t declared_lines = Reader_ReadI16(&payload);
    MUST(Reader_Result(&payload));
    FAIL_IF(declared_lines < 0, "Invalid schedule row count");
    data.declared_line_count = (uint16_t)declared_lines;
    const size_t stored_lines = Reader_Remaining(&payload) / 50;
    FAIL_IF(
        stored_lines < data.declared_line_count
            && Reader_Remaining(&payload) % 50 != 0,
        "Incomplete schedule row");
    data.line_count = stored_lines < data.declared_line_count
        ? (uint16_t)stored_lines
        : data.declared_line_count;
    STAGE_LINE *const lines = Mem_AllocArr(data.line_count, sizeof(*lines));
    bool invalid_reference = false;
    for (size_t i = 0; i < data.line_count; i++) {
        lines[i].position = Reader_ReadI16(&payload);
        for (size_t column = 0; column < STAGE_DATA_COLUMNS; column++) {
            const uint8_t value = Reader_ReadU8(&payload);
            lines[i].formations[column] = value == 0xff ? -1 : value;
            invalid_reference |= value != 0xff && value >= data.formation_count;
        }
    }
    const RESULT result = Reader_Result(&payload);
    if (!IS_OK(result) || invalid_reference) {
        Mem_Free(lines);
        return !IS_OK(result) ? result : FAIL("Invalid schedule formation");
    }
    if (data.map_count != 0) {
        uint8_t *const loaded = Allocator_Alloc(allocator, data.map_count);
        memcpy(loaded, map, data.map_count);
        data.map = loaded;
    }
    if (data.formation_count != 0) {
        STAGE_FORMATION *const loaded =
            Allocator_Alloc(allocator, data.formation_count * sizeof(*loaded));
        memcpy(loaded, formations, data.formation_count * sizeof(*loaded));
        data.formations = loaded;
    }
    if (data.script_count != 0) {
        STAGE_SCRIPT *const loaded =
            Allocator_Alloc(allocator, data.script_count * sizeof(*loaded));
        memcpy(loaded, scripts, data.script_count * sizeof(*loaded));
        data.scripts = loaded;
    }
    if (data.line_count != 0) {
        STAGE_LINE *const loaded =
            Allocator_Alloc(allocator, data.line_count * sizeof(*loaded));
        memcpy(loaded, lines, data.line_count * sizeof(*loaded));
        data.lines = loaded;
    }
    Mem_Free(lines);
    *out = data;
    return OK;
}

int16_t StageData_AimedInterval(
    const STAGE_FORMATION *const formation, const uint8_t rank)
{
    const int32_t interval = formation->aimed_interval;
    if (rank == 0) {
        const uint16_t bits = (uint16_t)((uint16_t)interval << 1);
        return (int16_t)(bits <= INT16_MAX ? bits : (int32_t)bits - 65536);
    }
    if (rank == 2 || rank == 3) {
        return (int16_t)(interval / 2 - (interval < 0 && interval % 2 != 0));
    }
    return (int16_t)interval;
}
