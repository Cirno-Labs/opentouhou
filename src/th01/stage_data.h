#pragma once

#include "core/io/reader.h"

#define STAGE_DATA_STAGE_COUNT 5
#define STAGE_DATA_ROWS 10
#define STAGE_DATA_COLUMNS 20

typedef enum {
    STAGE_DATA_OBSTACLE_NONE = 0,
    STAGE_DATA_BUMPER = 1,
    STAGE_DATA_TURRET_SLOW_1_AIMED = 2,
    STAGE_DATA_TURRET_SLOW_1_RANDOM_NARROW_AIMED = 3,
    STAGE_DATA_TURRET_SLOW_2_SPREAD_WIDE_AIMED = 4,
    STAGE_DATA_TURRET_SLOW_3_SPREAD_WIDE_AIMED = 5,
    STAGE_DATA_TURRET_SLOW_4_SPREAD_WIDE_AIMED = 6,
    STAGE_DATA_TURRET_SLOW_5_SPREAD_WIDE_AIMED = 7,
    STAGE_DATA_TURRET_QUICK_1_AIMED = 8,
    STAGE_DATA_TURRET_QUICK_1_RANDOM_NARROW_AIMED = 9,
    STAGE_DATA_TURRET_QUICK_2_SPREAD_WIDE_AIMED = 10,
    STAGE_DATA_TURRET_QUICK_3_SPREAD_WIDE_AIMED = 11,
    STAGE_DATA_TURRET_QUICK_4_SPREAD_WIDE_AIMED = 12,
    STAGE_DATA_TURRET_QUICK_5_SPREAD_WIDE_AIMED = 13,
    STAGE_DATA_PORTAL = 17,
    STAGE_DATA_BAR_TOP = 18,
    STAGE_DATA_BAR_BOTTOM = 19,
    STAGE_DATA_BAR_LEFT = 20,
    STAGE_DATA_BAR_RIGHT = 21,
} STAGE_DATA_OBSTACLE;

typedef struct {
    bool card;
    uint8_t card_extra_hp;
    STAGE_DATA_OBSTACLE obstacle;
} STAGE_DATA_CELL;

typedef struct {
    STAGE_DATA_CELL cells[STAGE_DATA_ROWS][STAGE_DATA_COLUMNS];
} STAGE_DATA_STAGE;

typedef struct {
    const STAGE_DATA_STAGE *stages;
} STAGE_DATA;

// Loads all five STAGE?.DAT layouts before simulation. Reads and discards
// the 22-byte header, including its magic, ID and two eight-byte names.
// The released loader ignores these fields and derives ST{id}.GRP and
// ST{id}.MDT from the requested scene ID. This codec selects no files.
// Reads five 250-byte records without the original RAM's five-byte padding.
// Expands each card byte's low four bits from bit 3 at the left; ignores
// the upper nibble. Types 14 through 16 add one through three extra HP to
// an existing card at that cell. Ignores orphan HP markers and unknown or
// negative obstacle types. Cards and physical obstacles can share a cell.
// Preserves the fifth layout, although stage initialization skips boss
// slot 4. Does not instantiate objects, advance RNG or render anything.
// Reads from the current position and leaves trailing bytes unread. Keeps
// reader open. Returns immutable layouts in one allocation from allocator,
// intended for an arena that owns their lifetime. Leaves out empty and
// allocates nothing from allocator on read failure, including late I/O.
RESULT StageData_Load(
    READER *reader, const ALLOCATOR *allocator, STAGE_DATA *out);
