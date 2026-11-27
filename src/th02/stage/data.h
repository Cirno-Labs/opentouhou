#pragma once

#include "core/io/reader.h"

#define STAGE_DATA_MAP_LIMIT 320
#define STAGE_DATA_FORMATION_LIMIT 128
#define STAGE_DATA_SCRIPT_LIMIT 40
#define STAGE_DATA_SCRIPT_SIZE 64
#define STAGE_DATA_COLUMNS 48

typedef struct {
    int16_t reserved_00;
    int16_t spawn_top;
    int16_t bullet_offset_x;
    int16_t bullet_offset_y;
    int16_t pattern;
    int16_t image_count;
    int16_t frames_per_image;
    int16_t kill_pattern_offset;
    uint8_t item;
    int8_t reserved_11;
    int32_t score;
    int16_t hp;
    uint8_t player_collision_disabled;
    int8_t reserved_19;
    int16_t aimed_interval;
    uint8_t script;
} STAGE_FORMATION;

typedef struct {
    uint8_t size;
    uint8_t bytes[STAGE_DATA_SCRIPT_SIZE];
} STAGE_SCRIPT;

typedef struct {
    int16_t position;
    int16_t formations[STAGE_DATA_COLUMNS];
} STAGE_LINE;

typedef struct {
    uint16_t payload_size;
    uint16_t map_count;
    uint16_t formation_count;
    uint16_t script_count;
    uint16_t line_count;
    uint16_t declared_line_count;
    const uint8_t *map;
    const STAGE_FORMATION *formations;
    const STAGE_SCRIPT *scripts;
    const STAGE_LINE *lines;
} STAGE_DATA;

// Loads stage data, with declared and stored rows apart: stage 3 declares
// four rows more than it stores, and they stay absent.
RESULT StageData_Load(
    READER *reader, const ALLOCATOR *allocator, STAGE_DATA *out);

// Returns the aimed-shot interval for rank: doubled with signed-word wrap
// on Easy, halved toward negative infinity on Hard and Lunatic.
int16_t StageData_AimedInterval(const STAGE_FORMATION *formation, uint8_t rank);
