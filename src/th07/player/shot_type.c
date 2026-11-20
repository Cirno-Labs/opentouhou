#include "th07/player/shot_type.h"

#include "shell/game.h"
#include "core/vfs.h"
#include "core/base/memory.h"

#define M_HEADER_SIZE 0x34
#define M_TABLE_SIZE 8
#define M_SHOT_SIZE 0x34
#define M_FULL_POWER 128
#define M_OPTION_COUNT 2
#define M_LASER_COUNT 3
#define M_TRAIL_LENGTH 16
#define M_FIRE_COUNT 6
#define M_MOVE_COUNT 6
#define M_DRAW_COUNT 2
#define M_HIT_COUNT 3
#define M_KIND_COUNT 6

static RESULT M_CountShots(
    READER *const reader, const uint32_t offset, int32_t *const count)
{
    FAIL_IF(
        offset >= Reader_Size(reader), "table at 0x%x is outside the file",
        offset);
    Reader_Seek(reader, offset);
    *count = 0;
    while (Reader_ReadI16(reader) >= 0 && !Reader_Failed(reader)) {
        Reader_Skip(reader, M_SHOT_SIZE - 2);
        (*count)++;
    }
    FAIL_IF(Reader_Failed(reader), "table at 0x%x has no closing shot", offset);
    FAIL_IF(*count == 0, "table at 0x%x has no shots", offset);
    return OK;
}

static RESULT M_ReadShot(READER *const reader, SHOT *const shot)
{
    shot->interval = Reader_ReadI16(reader);
    shot->phase = Reader_ReadI16(reader);
    shot->offset_x = Reader_ReadF32(reader);
    shot->offset_y = Reader_ReadF32(reader);
    shot->size_x = Reader_ReadF32(reader);
    shot->size_y = Reader_ReadF32(reader);
    shot->angle = Reader_ReadF32(reader);
    shot->speed = Reader_ReadF32(reader);
    shot->damage = Reader_ReadI16(reader);
    shot->option = Reader_ReadU8(reader);
    const uint8_t kind = Reader_ReadU8(reader);
    shot->script = Reader_ReadI16(reader);
    shot->sound = Reader_ReadI16(reader);
    const uint32_t fire = Reader_ReadU32(reader);
    const uint32_t move = Reader_ReadU32(reader);
    const uint32_t draw = Reader_ReadU32(reader);
    const uint32_t hit = Reader_ReadU32(reader);
    FAIL_IF(kind >= M_KIND_COUNT, "shot kind %u is unknown", kind);
    FAIL_IF(fire >= M_FIRE_COUNT, "fire behavior %u is unknown", fire);
    FAIL_IF(move >= M_MOVE_COUNT, "move behavior %u is unknown", move);
    FAIL_IF(draw >= M_DRAW_COUNT, "draw behavior %u is unknown", draw);
    // The original's hit entry 3 is the address 1, which crashes.
    FAIL_IF(hit >= M_HIT_COUNT, "hit behavior %u is unknown", hit);
    FAIL_IF(
        shot->option > M_OPTION_COUNT, "shot option %u is unknown",
        shot->option);
    // Kinds 1 and 2 act as plain shots, and fire 1 as fire 0.
    shot->kind = kind < SHOT_KIND_PIERCING ? SHOT_KIND_PLAIN : (SHOT_KIND)kind;
    shot->fire = fire == 1 ? SHOT_FIRE_PLAIN : (SHOT_FIRE)fire;
    shot->move = (SHOT_MOVE)move;
    shot->draw = (SHOT_DRAW)draw;
    shot->hit = (SHOT_HIT)hit;
    if (shot->fire == SHOT_FIRE_LASER || shot->fire == SHOT_FIRE_TRAIL_LASER) {
        FAIL_IF(
            shot->phase < 0 || shot->phase >= M_LASER_COUNT,
            "laser entry %d is unknown", shot->phase);
    } else {
        FAIL_IF(shot->interval == 0, "shot interval is 0");
    }
    FAIL_IF(
        shot->fire == SHOT_FIRE_TRAIL_LASER && shot->interval > M_TRAIL_LENGTH,
        "trail of %d is too long", shot->interval);
    // A laser move holds the option that a laser fire gives the bullet.
    FAIL_IF(
        shot->move == SHOT_MOVE_LASER
            && (shot->option == 0
                || (shot->fire != SHOT_FIRE_LASER
                    && shot->fire != SHOT_FIRE_TRAIL_LASER)),
        "laser move without a laser fire from an option");
    return OK;
}

static RESULT M_ReadTable(
    READER *const reader, const uint32_t offset, SHOT_TABLE *const table)
{
    MUST(M_CountShots(reader, offset, &table->count));
    table->shots = Mem_AllocArrZ((size_t)table->count, sizeof(SHOT));
    Reader_Seek(reader, offset);
    for (int32_t i = 0; i < table->count; i++) {
        MUST(M_ReadShot(reader, &table->shots[i]), "shot %d", i);
    }
    return OK;
}

static RESULT M_Read(READER *const reader, SHOT_TYPE *const out)
{
    Reader_Skip(reader, 2);
    out->table_count = Reader_ReadU16(reader);
    out->bombs = Reader_ReadF32(reader);
    out->bomb_window = Reader_ReadI32(reader);
    out->hitbox_size = Reader_ReadF32(reader);
    out->graze_size = Reader_ReadF32(reader);
    out->item_pull_speed = Reader_ReadF32(reader);
    out->item_collect_size = Reader_ReadF32(reader);
    out->death_cherry_share = Reader_ReadF32(reader);
    out->item_line = Reader_ReadF32(reader);
    out->speed = Reader_ReadF32(reader);
    out->speed_focused = Reader_ReadF32(reader);
    out->speed_diagonal = Reader_ReadF32(reader);
    out->speed_diagonal_focused = Reader_ReadF32(reader);
    MUST(Reader_Result(reader));
    FAIL_IF(
        (size_t)out->table_count * M_TABLE_SIZE > Reader_Remaining(reader),
        "%d tables do not fit", out->table_count);
    out->tables = Mem_AllocArrZ((size_t)out->table_count, sizeof(SHOT_TABLE));
    bool reaches_full_power = false;
    for (int32_t i = 0; i < out->table_count; i++) {
        Reader_Seek(reader, M_HEADER_SIZE + (size_t)i * M_TABLE_SIZE);
        const uint32_t offset = Reader_ReadU32(reader);
        SHOT_TABLE *const table = &out->tables[i];
        table->max_power = Reader_ReadI32(reader);
        MUST(M_ReadTable(reader, offset, table), "table %d", i);
        reaches_full_power |= table->max_power > M_FULL_POWER;
    }
    FAIL_IF(!reaches_full_power, "no table goes past full power");
    return OK;
}

RESULT ShotType_Read(READER *const reader, SHOT_TYPE *const out)
{
    *out = (SHOT_TYPE) {};
    const RESULT result = M_Read(reader, out);
    if (!IS_OK(result)) {
        ShotType_Release(out);
    }
    return result;
}

RESULT ShotType_Load(const char *const path, SHOT_TYPE *const out)
{
    *out = (SHOT_TYPE) {};
    READER reader;
    MUST(
        Vfs_Open(Shell_GetFiles(), path, VFS_IN_MEMORY, &reader),
        "the shot type cannot be read");
    const RESULT result = ShotType_Read(&reader, out);
    Reader_Close(&reader);
    return Result_Prefix(result, "%s", path);
}

void ShotType_Release(SHOT_TYPE *const shot_type)
{
    if (shot_type->tables != nullptr) {
        for (int32_t i = 0; i < shot_type->table_count; i++) {
            Mem_Free(shot_type->tables[i].shots);
        }
        Mem_Free(shot_type->tables);
    }
    *shot_type = (SHOT_TYPE) {};
}
