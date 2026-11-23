#pragma once

#include "th01/player.h"

#include "core/vfs.h"

#define SHOOTOUT_LASERS_COUNT 10

typedef struct {
    uint8_t masks[8][8][2];
} SHOOTOUT_LASERS_ASSETS;

typedef struct {
    int32_t origin_left, origin_y, start_left, start_y;
    int32_t cursor_left, cursor_y, step_x, step_y;
    int16_t length, tail_speed, extend_speed, age, detach_age;
    uint8_t color, width_mask;
    bool alive, damaging;
} SHOOTOUT_LASER;

typedef struct {
    SHOOTOUT_LASER slots[SHOOTOUT_LASERS_COUNT];
} SHOOTOUT_LASERS;

// Loads all eight widths and pre-shifts from laser_s.bmp into the caller's
// grouped asset allocator; leaves out null on error.
RESULT ShootoutLasers_LoadAssets(
    VFS *vfs, const ALLOCATOR *allocator, SHOOTOUT_LASERS_ASSETS **out);

// Zero-initialize the pool once. Positions, cursors and steps use Q24.8.
// Does nothing for an alive slot, including retaining all history. Requires
// width 1..8. Divides speed_times_eight by eight toward zero, losing the
// fraction. Wraps target differences to signed 16 bits before finding angle.
// Retains cursor history; unused source target/velocity/id fields are omitted.
void ShootoutLasers_Spawn(
    SHOOTOUT_LASER *laser, int16_t origin_left, int16_t origin_y,
    int16_t target_left, int16_t target_y, int16_t speed_times_eight,
    uint8_t color, int16_t detach_age, int16_t width);

// Updates one slot or all ten in order. Allocates nothing and reads only
// supplied state and immutable masks. Extends until the detach age, then
// restores tail_speed samples and adopts the cursor actually reached.
// Signed negative sample counts wrap to unsigned 16 bits. Death returns
// before incrementing age or drawing. Live updates increment signed age.
// Draws and tests hits only when leaving a cached byte/row group, before
// checking the new cursor bounds; omits the final group. Cached hits use
// y > 376, player.left+4 <= x < player.left+24 and no upper Y limit.
// Invincibility or damaging=false suppresses hits; sliding does not.
// Put preserves the drawing page; reached restores select page 0.
// Boss scheduling and gameplay scene integration remain external.
void ShootoutLasers_Update(
    SHOOTOUT_LASER *laser, const SHOOTOUT_LASERS_ASSETS *assets);
void ShootoutLasers_UpdateAll(
    SHOOTOUT_LASERS *lasers, const SHOOTOUT_LASERS_ASSETS *assets);

// Restores with the broken low-16 raw Q24.8 start/origin coordinates, then
// clears alive only. count is 0..10; preserves slots after that prefix.
// Uses Video_LineErase's physical-copy extension for protection faults.
void ShootoutLasers_EraseAndReset(SHOOTOUT_LASERS *lasers, int32_t count);
